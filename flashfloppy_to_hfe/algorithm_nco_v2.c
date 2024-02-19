#include "algorithm_nco_v2.h"

#include <stdio.h>
#include <string.h>

static int parse_param_integer(const char *value, int *dst)
{
    if (value == NULL)
        return -1;
    if (*value == '\0')
        return -1;

    char *endptr = NULL;
    *dst = strtol(value, &endptr, 10);
    if (*endptr != '\0')
        return -1;

    // Only allow positive integers and zero
    if (*dst < 0)
        return -1;

    return 0;
}

#define PLL_PERIOD_NOMINAL      (1 << 16)
#define PLL_PERIOD_MIN          (PLL_PERIOD_NOMINAL - PLL_PERIOD_NOMINAL/8)
#define PLL_PERIOD_MAX          (PLL_PERIOD_NOMINAL + PLL_PERIOD_NOMINAL/8)

static uint32_t nco_v2(
    uint16_t write_bc_ticks,
    uint16_t *ff_samples,
    size_t ff_sample_count,
    uint32_t *bc_buf,
    uint32_t bc_bufmask,
    struct kv_pair *params,
    struct data_logger *logger)
{
    int p_mul = -1;
    int p_div = -1;
    int i_mul = -1;
    int i_div = -1;

    for (
        struct kv_pair *param = params;
        param != NULL && param->key != NULL;
        ++param)
    {
        if (strcmp(param->key, "p_mul") == 0)
        {
            if (parse_param_integer(param->value, &p_mul) < 0)
            {
                fprintf(stderr, "ERROR: parameter %s must be a positive integer\n", param->key);
                return 0;
            }
        }
        else if (strcmp(param->key, "p_div") == 0)
        {
            if (parse_param_integer(param->value, &p_div) < 0)
            {
                fprintf(stderr, "ERROR: parameter %s must be a positive integer\n", param->key);
                return 0;
            }
        }
        else if (strcmp(param->key, "i_mul") == 0)
        {
            if (parse_param_integer(param->value, &i_mul) < 0)
            {
                fprintf(stderr, "ERROR: parameter %s must be a positive integer\n", param->key);
                return 0;
            }
        }
        else if (strcmp(param->key, "i_div") == 0)
        {
            if (parse_param_integer(param->value, &i_div) < 0)
            {
                fprintf(stderr, "ERROR: parameter %s must be a positive integer\n", param->key);
                return 0;
            }
        }
        else
        {
            fprintf(stderr, "ERROR: unknown parameter %s\n", param->key);
        }
    }

    if (p_mul == -1 || p_div == -1 || i_mul == -1 || i_div == -1)
    {
        fprintf(stderr, "ERROR: required parameters not set\n");
        return 0;
    }

    uint64_t timestamp = 0ULL;
    data_logger_set_timestamp_freq(logger, 72000000);

    // dma_wr struct
    uint32_t pll_period;  // PLL clock ticks per sample clock ticks.
                          // Effectively 16.16 fixed-point.
    int32_t pll_integral;
    uint16_t prev_sample;
    uint32_t prev_pll_bc_left;
    uint32_t prev_pll_edge;

    // Things that happen when write-enable is asserted.
    pll_period = PLL_PERIOD_NOMINAL;  // Initially sssume recovered clock is perfectly
                                      // aligned with sample clock.
    pll_integral = 0;
    prev_sample = 0;
    prev_pll_bc_left = 0;
    prev_pll_edge = 0;

    // Things that happen on each DMA
    uint32_t bc_prod = 0;
    uint32_t bc_dat = ~0;
    #define BC_BEFORE_FIRST_PULSE 10

    for (int ii = 0; ii < ff_sample_count; ++ii)
    {
        uint32_t pll_write_bc_ticks = write_bc_ticks * pll_period;

        // If this is the first pulse since WGATE was asserted,
        // treat it as perfectly in phase and insert a few leading zeros.
        if ( prev_sample == 0 && prev_pll_bc_left == 0) {
            prev_sample = ff_samples[ii] - (BC_BEFORE_FIRST_PULSE * write_bc_ticks);
            prev_pll_bc_left = 0;
            prev_pll_edge = pll_write_bc_ticks/2;
        }

        uint16_t duration_between_samples = ff_samples[ii] - prev_sample;

        timestamp += duration_between_samples;

        // If this edge would fall within the previous bitcell, it is a runt.
        // Since the pll timestamps will wrap on overflow, the timestamps can't
        // be directly compared.  Instead, compare the distance from the
        // previous bitcell's left edge as the subtraction will account any
        // wrapping.
        uint32_t curr_pll_edge = prev_pll_edge + (uint32_t)duration_between_samples * PLL_PERIOD_NOMINAL;
        uint32_t curr_pll_bc_left = prev_pll_bc_left + pll_write_bc_ticks;
        if ((curr_pll_edge - prev_pll_bc_left) < (curr_pll_bc_left - prev_pll_bc_left)) {
            printf("Runt\n");
            continue;
        }

        // Record zeros for each bitcell that passed before this pulse.
        int zeros = 0;
        while (curr_pll_edge - curr_pll_bc_left > pll_write_bc_ticks)
        {
            bc_dat <<= 1;
            bc_prod++;

            if (!(bc_prod & 31))
                bc_buf[((bc_prod - 1) / 32) & bc_bufmask] = htobe32(bc_dat);

            zeros++;
            curr_pll_bc_left += pll_write_bc_ticks;
        }

        // Record a one for this bitcell
        bc_dat = (bc_dat << 1) | 1;
        bc_prod++;
        if (!(bc_prod & 31))
            bc_buf[((bc_prod - 1) / 32) & bc_bufmask] = htobe32(bc_dat);

        // Figure out the phase error before we start mucking with state
        int32_t phase_error = (int32_t)(curr_pll_edge - (curr_pll_bc_left + pll_write_bc_ticks/2));

        data_logger_event(logger, timestamp, (double)phase_error/65536.0);

        // Accumulate error into integral, saturing as necessary
        if (pll_integral > 0 && phase_error > INT32_MAX - pll_integral)
        {
            pll_integral = INT32_MAX;
        }
        else if (pll_integral < 0 && phase_error < INT32_MIN - pll_integral)
        {
            pll_integral = INT32_MIN;
        }
        else
        {
            pll_integral += phase_error;
        }

        pll_period = (uint32_t)((int32_t)PLL_PERIOD_NOMINAL + (phase_error * p_mul / p_div) + (pll_integral * i_mul / i_div));
        if (pll_period < PLL_PERIOD_MIN) {
            pll_period = PLL_PERIOD_MIN;
        } else if (pll_period > PLL_PERIOD_MAX) {
            pll_period = PLL_PERIOD_MAX;
        }

        // Advance to the next bitcell
        prev_pll_bc_left = curr_pll_bc_left;
        prev_pll_edge = curr_pll_edge;
        prev_sample = ff_samples[ii];
    }

    bc_buf[(bc_prod / 32) & bc_bufmask] = htobe32(bc_dat << (-bc_prod & 31));
    return bc_prod;
};

static struct parameter nco_v2_params[] = {
    {.name = "p_mul", .required = 1, .description = "f"},
    {.name = "p_div", .required = 1, .description = ""},
    {.name = "i_mul", .required = 1, .description = ""},
    {.name = "i_div", .required = 1, .description = ""},
    {.name = NULL, .description = NULL}};

struct algorithm algorithm_nco_v2 = {
    .name = "nco_v2",
    .func = nco_v2,
    .params = nco_v2_params,
};