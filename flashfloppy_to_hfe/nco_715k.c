#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>

#include "nco_715k.h"

uint32_t nco_715k(uint16_t write_bc_ticks, uint16_t *ff_samples, size_t ff_sample_count, uint32_t *bc_buf, uint32_t bc_bufmask)
{
    // dma_wr struct
    uint32_t phase_step;
    int32_t phase_integral;
    uint32_t prev_bc_left;
    uint32_t curr_bc_left;

    // zeta = (loop dampening coefficient)
    // f_n = (loop natural frequency)
    // f_s = (NCO base frequency)
    // k_p = (phase detector gain)
    // k_nco = (NCO feedback scaler)

    // w_n = f_n * 2 * pi
    // Ts = 1 / f_s
    // k_l = 2 * zeta * w_n * Ts / (k_p * k_nco)
    // k_i = w_n^2 * Ts^2 / (k_p * k_nco)

    // Constants
    // zeta = 1
    // f_s = 72,000,000 Hz
    // k_p = 1.0
    // k_nco = 1.0

    // f_n = 715,000 Hz
    // k_l = 0.125   = 1/8
    // k_i = 0.003893  = 1/256

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
        // Scale the NCO frequency to the expected data frequency
        uint32_t bc_step = phase_step * (uint32_t)write_bc_ticks;

        // Shifted up 16-bits so samples wraparound at the same time as the
        // phase accumulator
        uint32_t next_edge = ff_samples[ii] << 16;

        // If this is the first pulse since WGATE was asserted,
        // treat it as perfectly in phase.
        if (prev_bc_left == 0 && curr_bc_left == 0) {
            curr_bc_left = next_edge - (bc_step / 2);
            prev_bc_left = curr_bc_left - bc_step;
        }

        //printf("Next edge: Next edge: %10u (%8x) ", next_edge, next_edge);

        uint32_t distance_from_prev_bc_left = next_edge - prev_bc_left;

        // If the next edge would fall within the previous
        if (distance_from_prev_bc_left < (curr_bc_left - prev_bc_left))
        {
            printf("Runt\n");
            continue;
        }

        // Advance to the current bitcell
        uint32_t distance_from_curr_bc_left = next_edge - curr_bc_left;

        //printf("Curr bc: %10u (%8x) Dist: %10d (%8.3f) ", curr_bc_left, curr_bc_left,
        //       distance_from_curr_bc_left, (double)distance_from_curr_bc_left / 65536.0);


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

        //printf("Zeros: %3d Edge bc: [%10u,%10u) ([%8x,%8x)) Dist: %10d (%8.3f) ",
        //       zeros,
        //       curr_bc_left, curr_bc_left + bc_step,
        //       curr_bc_left, curr_bc_left + bc_step,
        //       distance_from_curr_bc_left, (double)distance_from_curr_bc_left / 65536.0);

        // Record a one for this bitcell
        bc_dat = (bc_dat << 1) | 1;
        bc_prod++;
        if (!(bc_prod & 31))
            bc_buf[((bc_prod - 1) / 32) & bc_bufmask] = htobe32(bc_dat);

        // Figure out the phase error before we start mucking with state
        int32_t phase_error = ((int32_t)distance_from_curr_bc_left - ((int32_t)bc_step / 2)) / (int32_t)write_bc_ticks;

        //printf("Phase Error: %8d ", phase_error);

        // Adjust bitcells for next iteration
        prev_bc_left = curr_bc_left;
        curr_bc_left += bc_step;

        // Accumulate error into integral, saturing as necessary
        if (phase_integral > 0 && phase_error > INT32_MAX - phase_integral) {
            phase_integral = INT32_MAX;
        } else if (phase_integral < 0 && phase_error < INT32_MIN - phase_integral) {
            phase_integral = INT32_MIN;
        } else {
            phase_integral += phase_error;
        }

        //printf("P: %10d I: %10d ", phase_error/8, phase_integral/256);

        phase_step = (uint32_t)((int32_t)(1 << 16) + phase_integral/256 + phase_error/8);

        //printf("Phase step: %10u\n", phase_step);
    }

    bc_buf[(bc_prod / 32) & bc_bufmask] = htobe32(bc_dat << (-bc_prod & 31));
    return bc_prod;
}

struct algorithm algorithm_nco_715k = {
    .name = "nco_715k",
    .func = nco_715k,
};