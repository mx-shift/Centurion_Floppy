#include "algorithm_bitcell_width_pi_v2.h"

#include <stdio.h>
#include <string.h>

#if 0
#define DEBUG(...) printf(__VA_ARGS__)
#else
#define DEBUG(...)
#endif

// bitcell_width_pi_v2 applies a PI control loop adjusting bitcell width based
// on the distance of a WDATA# edge from the center of the bitcell.  Because
// bitcell width is being constantly adjusted, a 1-sample history is kept to
// determine the next bitcell's start time.
//
// Cleanup of v1 to use correct terminology and description of behavior.

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

#define BC_WIDTH_FRACTIONAL_BITS    16

static uint32_t bitcell_width_pi_v2(
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
                fprintf(stderr, "bitcell_width_pi parameter %s must be a positive integer\n", param->key);
                return 0;
            }
        }
        else if (strcmp(param->key, "p_div") == 0)
        {
            if (parse_param_integer(param->value, &p_div) < 0)
            {
                fprintf(stderr, "bitcell_width_pi parameter %s must be a positive integer\n", param->key);
                return 0;
            }
        }
        else if (strcmp(param->key, "i_mul") == 0)
        {
            if (parse_param_integer(param->value, &i_mul) < 0)
            {
                fprintf(stderr, "bitcell_width_pi parameter %s must be a positive integer\n", param->key);
                return 0;
            }
        }
        else if (strcmp(param->key, "i_div") == 0)
        {
            if (parse_param_integer(param->value, &i_div) < 0)
            {
                fprintf(stderr, "bitcell_width_pi parameter %s must be a positive integer\n", param->key);
                return 0;
            }
        }
        else
        {
            fprintf(stderr, "bitcell_width_pi: unknown parameter %s\n", param->key);
        }
    }

    if (p_mul == -1 || p_div == -1 || i_mul == -1 || i_div == -1)
    {
        fprintf(stderr, "bitcell_width_pi_v2: required parameters not set\n");
        return 0;
    }

    uint64_t timestamp = 0ULL;
    data_logger_set_timestamp_freq(logger, 72000000);

    // dma_wr struct
    uint32_t bc_width;  // in (2**BC_WIDTH_FRACTIONAL_BITS)ths of a sample clock
    int32_t bc_width_error_integral;
    uint32_t prev_bc_left;
    uint32_t curr_bc_left;

    // Things that happen when write-enable is asserted.
    bc_width_error_integral = 0;
    prev_bc_left = 0;
    curr_bc_left = 0;

    // Things that happen on each DMA
    uint32_t bc_prod = 0;
    uint32_t bc_dat = ~0;

    for (int ii = 0; ii < ff_sample_count; ++ii)
    {
        uint32_t curr_edge = ff_samples[ii] << BC_WIDTH_FRACTIONAL_BITS;

        DEBUG("\n");

        if (ii > 0) {
            DEBUG("timestamp: %lu\tprev_sample=%u\tcurr_sample=%u\tdist=%u\n",
                timestamp, ff_samples[ii-1], ff_samples[ii], (uint16_t)(ff_samples[ii] - ff_samples[ii-1]));
            timestamp += (uint16_t)(ff_samples[ii] - ff_samples[ii-1]);
        }

        DEBUG("timestamp: %lu\tbc_width=%u\tprev_bc_left=%u\tcurr_bc_left=%u\tcurr_edge=%u\n",
            timestamp, bc_width, prev_bc_left, curr_bc_left, curr_edge);

        // If this is the first pulse since WGATE was asserted, treat it as
        // perfectly aligned with the center of the current bitcell.
        if (prev_bc_left == 0 && curr_bc_left == 0)
        {
            bc_width = (uint32_t)write_bc_ticks << BC_WIDTH_FRACTIONAL_BITS;
            curr_bc_left = curr_edge - (bc_width / 2);
            prev_bc_left = curr_bc_left - bc_width;

            DEBUG("timestamp: %lu\tFIRST EDGE AFTER WGATE\tbc_width=%u\tprev_bc_left=%u\tcurr_bc_left=%u\tcurr_edge=%u\n",
                timestamp, bc_width, prev_bc_left, curr_bc_left, curr_edge);
        }

        // If the next edge would fall within the previous, consider it a runt
        // and ignore it.
        uint32_t distance_from_prev_bc_left = curr_edge - prev_bc_left;
        if (distance_from_prev_bc_left < (curr_bc_left - prev_bc_left))
        {
            DEBUG("timestamp: %lu\tRUNT\tdistance_from_prev_bc_left=%u\n",
                timestamp, distance_from_prev_bc_left);
            continue;
        }

        // Record zeros for each bitcell that passed before this pulse
        uint32_t distance_from_curr_bc_left = curr_edge - curr_bc_left;
        int zeros = 0;
        while (distance_from_curr_bc_left > bc_width)
        {
            bc_dat <<= 1;
            bc_prod++;

            if (!(bc_prod & 31))
                bc_buf[((bc_prod - 1) / 32) & bc_bufmask] = htobe32(bc_dat);

            zeros++;
            distance_from_curr_bc_left -= bc_width;
            curr_bc_left += bc_width;
        }

        DEBUG("timestamp: %lu\tzeros=%d\tcurr_bc_left=%u\tcurr_edge=%u\tdistance_from_curr_bc_left=%u\n",
            timestamp, zeros, curr_bc_left, curr_edge, distance_from_curr_bc_left);

        // Record a one for this bitcell
        bc_dat = (bc_dat << 1) | 1;
        bc_prod++;
        if (!(bc_prod & 31))
            bc_buf[((bc_prod - 1) / 32) & bc_bufmask] = htobe32(bc_dat);

        // Calculate distance of the edge from the bitcell's center
        uint32_t curr_bc_center = curr_bc_left + bc_width/2;
        int32_t distance_from_curr_bc_center = curr_edge - curr_bc_center;

        data_logger_event(logger, timestamp, (double)distance_from_curr_bc_center/(double)(1 << BC_WIDTH_FRACTIONAL_BITS));

        // Accumulate error into integral, saturating as necessary
        if ((bc_width_error_integral > 0)
            && (distance_from_curr_bc_center > (INT32_MAX - bc_width_error_integral)))
        {
            bc_width_error_integral = INT32_MAX;
        }
        else if ((bc_width_error_integral < 0)
                    && (distance_from_curr_bc_center < (INT32_MIN - bc_width_error_integral)))
        {
            bc_width_error_integral = INT32_MIN;
        }
        else
        {
            bc_width_error_integral += distance_from_curr_bc_center;
        }

        int32_t p_term = distance_from_curr_bc_center * (int32_t)p_mul / (int32_t)p_div;
        int32_t i_term = bc_width_error_integral * (int32_t)i_mul / (int32_t)i_div;

        DEBUG("timestamp: %lu\tdistance_from_curr_bc_center=%d\tbc_width_error_integral=%d\tp_term=%ld\ti_term=%ld\n",
            timestamp, distance_from_curr_bc_center, bc_width_error_integral, p_term, i_term);

        prev_bc_left = curr_bc_left;
        curr_bc_left += bc_width;
        bc_width =
            (uint32_t)(write_bc_ticks << BC_WIDTH_FRACTIONAL_BITS)
            + p_term 
            + i_term;
    }

    bc_buf[(bc_prod / 32) & bc_bufmask] = htobe32(bc_dat << (-bc_prod & 31));
    return bc_prod;
}

static struct parameter bitcell_width_pi_v2_params[] = {
    {.name = "p_mul", .required = 1, .description = "f"},
    {.name = "p_div", .required = 1, .description = ""},
    {.name = "i_mul", .required = 1, .description = ""},
    {.name = "i_div", .required = 1, .description = ""},
    {.name = NULL, .description = NULL}};

struct algorithm algorithm_bitcell_width_pi_v2 = {
    .name = "bitcell_width_pi_v2",
    .func = bitcell_width_pi_v2,
    .params = bitcell_width_pi_v2_params,
};