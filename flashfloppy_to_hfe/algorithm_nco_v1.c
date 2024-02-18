#include "algorithm_nco_v1.h"

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

static uint32_t nco_v1(
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
                fprintf(stderr, "nco parameter %s must be a positive integer\n", param->key);
                return 0;
            }
        }
        else if (strcmp(param->key, "p_div") == 0)
        {
            if (parse_param_integer(param->value, &p_div) < 0)
            {
                fprintf(stderr, "nco parameter %s must be a positive integer\n", param->key);
                return 0;
            }
        }
        else if (strcmp(param->key, "i_mul") == 0)
        {
            if (parse_param_integer(param->value, &i_mul) < 0)
            {
                fprintf(stderr, "nco parameter %s must be a positive integer\n", param->key);
                return 0;
            }
        }
        else if (strcmp(param->key, "i_div") == 0)
        {
            if (parse_param_integer(param->value, &i_div) < 0)
            {
                fprintf(stderr, "nco parameter %s must be a positive integer\n", param->key);
                return 0;
            }
        }
        else
        {
            fprintf(stderr, "nco: unknown parameter %s\n", param->key);
        }
    }

    if (p_mul == -1 || p_div == -1 || i_mul == -1 || i_div == -1)
    {
        fprintf(stderr, "nco_v1: required parameters not set\n");
        return 0;
    }

    uint64_t timestamp = 0ULL;
    data_logger_set_timestamp_freq(logger, 72000000);

    // dma_wr struct
    uint32_t phase_step;
    int32_t phase_integral;
    uint32_t prev_bc_left;
    uint32_t curr_bc_left;

    // Things that happen when write-enable is asserted.
    phase_step = 1 << 16;
    phase_integral = 0;
    prev_bc_left = 0;
    curr_bc_left = 0;

    // Things that happen on each DMA
    uint32_t bc_prod = 0;
    uint32_t bc_dat = ~0;

    for (int ii = 0; ii < ff_sample_count; ++ii)
    {
        if (ii > 0) {
            timestamp += (uint16_t)(ff_samples[ii] - ff_samples[ii-1]);
        }

        // Scale the NCO frequency to the expected data frequency
        uint32_t bc_step = phase_step * (uint32_t)write_bc_ticks;

        // Shifted up 16-bits so samples wraparound at the same time as the
        // phase accumulator
        uint32_t next_edge = ff_samples[ii] << 16;

        // If this is the first pulse since WGATE was asserted,
        // treat it as perfectly in phase.
        if (prev_bc_left == 0 && curr_bc_left == 0)
        {
            curr_bc_left = next_edge - (bc_step / 2);
            prev_bc_left = curr_bc_left - bc_step;
        }

        // printf("Next edge: Next edge: %10u (%8x) ", next_edge, next_edge);

        uint32_t distance_from_prev_bc_left = next_edge - prev_bc_left;

        // If the next edge would fall within the previous
        if (distance_from_prev_bc_left < (curr_bc_left - prev_bc_left))
        {
            printf("Runt\n");
            continue;
        }

        // Advance to the current bitcell
        uint32_t distance_from_curr_bc_left = next_edge - curr_bc_left;

        // printf("Curr bc: %10u (%8x) Dist: %10d (%8.3f) ", curr_bc_left, curr_bc_left,
        //        distance_from_curr_bc_left, (double)distance_from_curr_bc_left / 65536.0);

        // Record zeros for each bitcell that passed before this pulse
        int zeros = 0;
        while (distance_from_curr_bc_left > bc_step)
        {
            bc_dat <<= 1;
            bc_prod++;

            if (!(bc_prod & 31))
                bc_buf[((bc_prod - 1) / 32) & bc_bufmask] = htobe32(bc_dat);

            zeros++;
            distance_from_curr_bc_left -= bc_step;
            curr_bc_left += bc_step;
        }

        // printf("Zeros: %3d Edge bc: [%10u,%10u) ([%8x,%8x)) Dist: %10d (%8.3f) ",
        //        zeros,
        //        curr_bc_left, curr_bc_left + bc_step,
        //        curr_bc_left, curr_bc_left + bc_step,
        //        distance_from_curr_bc_left, (double)distance_from_curr_bc_left / 65536.0);

        // Record a one for this bitcell
        bc_dat = (bc_dat << 1) | 1;
        bc_prod++;
        if (!(bc_prod & 31))
            bc_buf[((bc_prod - 1) / 32) & bc_bufmask] = htobe32(bc_dat);

        // Figure out the phase error before we start mucking with state
        int32_t phase_error = ((int32_t)distance_from_curr_bc_left - ((int32_t)bc_step / 2)) / (int32_t)write_bc_ticks;

        data_logger_event(logger, timestamp, phase_error);

        // printf("Phase Error: %8d ", phase_error);

        // Adjust bitcells for next iteration
        prev_bc_left = curr_bc_left;
        curr_bc_left += bc_step;

        // Accumulate error into integral, saturing as necessary
        if (phase_integral > 0 && phase_error > INT32_MAX - phase_integral)
        {
            phase_integral = INT32_MAX;
        }
        else if (phase_integral < 0 && phase_error < INT32_MIN - phase_integral)
        {
            phase_integral = INT32_MIN;
        }
        else
        {
            phase_integral += phase_error;
        }

        // printf("P: %10d I: %10d ", phase_error/16, phase_integral/1024);

        phase_step = (uint32_t)((int32_t)(1 << 16) + (phase_error * p_mul / p_div) + (phase_integral * i_mul / i_div));

        // printf("Phase step: %10u\n", phase_step);
    }

    bc_buf[(bc_prod / 32) & bc_bufmask] = htobe32(bc_dat << (-bc_prod & 31));
    return bc_prod;
}

static struct parameter nco_v1_params[] = {
    {.name = "p_mul", .required = 1, .description = "f"},
    {.name = "p_div", .required = 1, .description = ""},
    {.name = "i_mul", .required = 1, .description = ""},
    {.name = "i_div", .required = 1, .description = ""},
    {.name = NULL, .description = NULL}};

struct algorithm algorithm_nco_v1 = {
    .name = "nco_v1",
    .func = nco_v1,
    .params = nco_v1_params,
};