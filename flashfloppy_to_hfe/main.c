#include <endian.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BC_BUF_SIZE_BYTES (2 * 1024 * 1024)

uint32_t ff_v341(uint16_t *ff_samples, size_t ff_sample_count, uint32_t *bc_buf, uint32_t bc_bufmask)
{
    /* FlashFloppy v3.41 */
    uint16_t cell = 72;
    uint16_t window = cell + (cell >> 1);

    uint16_t prev = 0;
    uint32_t bc_dat = ~0;
    uint32_t bc_prod = 0;

    for (int ii = 0; ii < ff_sample_count; ++ii)
    {
        uint16_t next = ff_samples[ii];
        uint16_t curr = next - prev;
        prev = next;
        while (curr > window)
        {
            curr -= cell;
            bc_dat <<= 1;
            bc_prod++;
            if (!(bc_prod & 31))
                bc_buf[((bc_prod - 1) / 32) & bc_bufmask] = htobe32(bc_dat);
        }
        bc_dat = (bc_dat << 1) | 1;
        bc_prod++;

        if (!(bc_prod & 31))
            bc_buf[((bc_prod - 1) / 32) & bc_bufmask] = htobe32(bc_dat);
    }

    bc_buf[(bc_prod / 32) & bc_bufmask] = htobe32(bc_dat << (-bc_prod & 31));

    return bc_prod;
}

uint32_t ff_master(uint16_t *ff_samples, size_t ff_sample_count, uint32_t *bc_buf, uint32_t bc_bufmask)
{
    /* FlashFloppy master */
    int cell = 72;

    uint16_t prev = 0;
    uint32_t bc_prod = 0;
    uint32_t bc_dat = ~0;

    for (int ii = 0; ii < ff_sample_count; ++ii)
    {
        uint16_t next = ff_samples[ii];
        int curr = (uint16_t)(next - prev) - (cell >> 1);

        if (curr < 0)
        {
            /* Runt flux, much shorter than bitcell clock. Merge it forward. */
            continue;
        }
        prev = next;

        while ((curr -= cell) > 0)
        {
            bc_dat <<= 1;
            bc_prod++;
            if (!(bc_prod & 31))
                bc_buf[((bc_prod - 1) / 32) & bc_bufmask] = htobe32(bc_dat);
        }
        bc_dat = (bc_dat << 1) | 1;
        bc_prod++;
        if (!(bc_prod & 31))
            bc_buf[((bc_prod - 1) / 32) & bc_bufmask] = htobe32(bc_dat);
    }

    bc_buf[(bc_prod / 32) & bc_bufmask] = htobe32(bc_dat << (-bc_prod & 31));
    return bc_prod;
}

uint32_t ff_v341_crude_pll(uint16_t *ff_samples, size_t ff_sample_count, uint32_t *bc_buf, uint32_t bc_bufmask)
{
    /* FlashFloppy v3.41 with a very crude PLL */
    uint16_t cell = 72;
    uint16_t window = cell + (cell >> 1);

    uint16_t prev = 0;
    uint32_t bc_dat = ~0;
    uint32_t bc_prod = 0;

    for (int ii = 0; ii < ff_sample_count; ++ii)
    {
        uint16_t next = ff_samples[ii];
        uint16_t curr = next - prev;

        // Only attempt to adjust write_bc_ticks if we have a measurement
        // between two actual pulses.  When a write starts, prev is set to zero
        // and does not represent an actual pulse.
        if (prev != 0)
        {
            uint16_t bc_in_curr = (curr + (cell / 2)) / cell; // Round to nearest integer # of bitcells
            int16_t cur_total_phase_error = curr - bc_in_curr * cell;

            uint16_t phase_error_threshold_per_bc = cell / 8;
            uint16_t total_phase_error_threshold = phase_error_threshold_per_bc * bc_in_curr;

            if (cur_total_phase_error > total_phase_error_threshold)
            {
                // Write pulses are slower than expected.  Increase
                // write_bc_ticks slightly to compensate.
                cell += 1;
                window = cell + (cell >> 1);
                printf("Increased write_bc_ticks to %hu\n", cell);
            }
            else if (cur_total_phase_error < -total_phase_error_threshold)
            {
                // Write pulses are faster than expected.  Decrease
                // write_bc_ticks slightly to compensate.
                cell -= 1;
                window = cell + (cell >> 1);
                printf("Decreased write_bc_ticks to %hu\n", cell);
            }
        }

        prev = next;
        while (curr > window)
        {
            curr -= cell;
            bc_dat <<= 1;
            bc_prod++;
            if (!(bc_prod & 31))
                bc_buf[((bc_prod - 1) / 32) & bc_bufmask] = htobe32(bc_dat);
        }
        bc_dat = (bc_dat << 1) | 1;
        bc_prod++;

        if (!(bc_prod & 31))
            bc_buf[((bc_prod - 1) / 32) & bc_bufmask] = htobe32(bc_dat);
    }

    bc_buf[(bc_prod / 32) & bc_bufmask] = htobe32(bc_dat << (-bc_prod & 31));
    return bc_prod;
}

uint32_t ff_master_greaseweazle_default_pll(uint16_t *ff_samples, size_t ff_sample_count, uint32_t *bc_buf, uint32_t bc_bufmask)
{
    /* FlashFloppy master w/ Greaseweazle's Default PLL */
    int cell_nominal = 72;
    int cell_min = cell_nominal - (cell_nominal * 10 / 100);
    int cell_max = cell_nominal + (cell_nominal * 10 / 100);

    int cell = cell_nominal;
    uint16_t prev = 0;
    uint32_t bc_prod = 0;
    uint32_t bc_dat = ~0;

    for (int ii = 0; ii < ff_sample_count; ++ii)
    {
        uint16_t next = ff_samples[ii];
        int curr = (uint16_t)(next - prev);

        if (curr < (cell / 2))
        {
            /* Runt flux, much shorter than bitcell clock. Merge it forward. */
            printf("Runt flux\n");
            continue;
        }
        prev = next;

        uint8_t zeros = 0;
        while ((curr -= cell) > (cell / 2))
        {
            zeros += 1;
            bc_dat <<= 1;
            bc_prod++;
            if (!(bc_prod & 31))
                bc_buf[((bc_prod - 1) / 32) & bc_bufmask] = htobe32(bc_dat);
        }
        bc_dat = (bc_dat << 1) | 1;
        bc_prod++;
        if (!(bc_prod & 31))
            bc_buf[((bc_prod - 1) / 32) & bc_bufmask] = htobe32(bc_dat);

        printf("Encountered %hhu zeros with phase offset %d ticks\n", zeros, curr);

        // PLL: Adjust clock frequency according to phase mismatch.
        // curr is now the accumulated phase offset since last pulse
        if (zeros <= 3)
        {
            // In sync: adjust clock by a fraction of the phase mismatch.
            int adj_amount = curr * 5 / 100;

            cell += adj_amount;
            printf("In Sync: adjusting clock by %d ticks to %d\n", adj_amount, cell);
        }
        else
        {
            // Out of sync: adjust clock towards centre.
            int adj_amount = (cell_nominal - cell) * 5 / 100;

            cell += adj_amount;
            printf("Out of Sync: adjusting clock by %d ticks to %d\n", adj_amount, cell);
        }

        // Clamp the clock's adjustment range.
        if (cell > cell_max)
        {
            cell = cell_max;
            printf("Clock clamped to max %d\n", cell);
        }
        else if (cell < cell_min)
        {
            cell = cell_min;
            printf("Clock clamped to min %d\n", cell);
        }
    }

    bc_buf[(bc_prod / 32) & bc_bufmask] = htobe32(bc_dat << (-bc_prod & 31));
    return bc_prod;
}

uint32_t ff_master_greaseweazle_fallback_pll(uint16_t *ff_samples, size_t ff_sample_count, uint32_t *bc_buf, uint32_t bc_bufmask)
{
    /* FlashFloppy master w/ Greaseweazle's Default PLL */
    int cell_nominal = 72;
    int cell_min = cell_nominal - (cell_nominal * 10 / 100);
    int cell_max = cell_nominal + (cell_nominal * 10 / 100);

    int cell = cell_nominal;
    uint16_t prev = 0;
    uint32_t bc_prod = 0;
    uint32_t bc_dat = ~0;

    for (int ii = 0; ii < ff_sample_count; ++ii)
    {
        uint16_t next = ff_samples[ii];
        int curr = (uint16_t)(next - prev);

        if (curr < (cell / 2))
        {
            /* Runt flux, much shorter than bitcell clock. Merge it forward. */
            printf("Runt flux\n");
            continue;
        }
        prev = next;

        uint8_t zeros = 0;
        while ((curr -= cell) > (cell / 2))
        {
            zeros += 1;
            bc_dat <<= 1;
            bc_prod++;
            if (!(bc_prod & 31))
                bc_buf[((bc_prod - 1) / 32) & bc_bufmask] = htobe32(bc_dat);
        }
        bc_dat = (bc_dat << 1) | 1;
        bc_prod++;
        if (!(bc_prod & 31))
            bc_buf[((bc_prod - 1) / 32) & bc_bufmask] = htobe32(bc_dat);

        printf("Encountered %hhu zeros with phase offset %d ticks\n", zeros, curr);

        // PLL: Adjust clock frequency according to phase mismatch.
        // curr is now the accumulated phase offset since last pulse
        if (zeros <= 3)
        {
            // In sync: adjust clock by a fraction of the phase mismatch.
            int adj_amount = curr * 1 / 100;

            cell += adj_amount;
            printf("In Sync: adjusting clock by %d ticks to %d\n", adj_amount, cell);
        }
        else
        {
            // Out of sync: adjust clock towards centre.
            int adj_amount = (cell_nominal - cell) * 1 / 100;

            cell += adj_amount;
            printf("Out of Sync: adjusting clock by %d ticks to %d\n", adj_amount, cell);
        }

        // Clamp the clock's adjustment range.
        if (cell > cell_max)
        {
            cell = cell_max;
            printf("Clock clamped to max %d\n", cell);
        }
        else if (cell < cell_min)
        {
            cell = cell_min;
            printf("Clock clamped to min %d\n", cell);
        }
    }

    bc_buf[(bc_prod / 32) & bc_bufmask] = htobe32(bc_dat << (-bc_prod & 31));
    return bc_prod;
}

uint32_t fdc9216(uint16_t *ff_samples, size_t ff_sample_count, uint32_t *bc_buf, uint32_t bc_bufmask)
{
    // A PLL that actually adjusts phase gradually

    uint16_t write_bc_ticks = 72;

    // Things that happen when write-enable is asserted.
    uint32_t write_pll_period = (uint32_t)write_bc_ticks << 16; // write_bc_ticks
    uint32_t write_pll_period_adjust = write_pll_period / 800;
    uint32_t write_pll_period_max = write_pll_period * 11 / 10;
    uint32_t write_pll_period_min = write_pll_period * 9 / 10;
    uint8_t write_pll_phase_incs = 0;
    uint8_t write_pll_phase_decs = 0;
    uint32_t write_prev_bc_left_edge = 0 - write_pll_period;
    int pll_phase_offset = 0;

    uint32_t bc_prod = 0;
    uint32_t bc_dat = ~0;

    // Things that happen on a write DMA buffer full

    for (int ii = 0; ii < ff_sample_count; ++ii)
    {
        uint32_t next_edge = ff_samples[ii] << 16;

        // By computing distance, wraparound is accounted for naturally.
        uint32_t distance_from_prev_bc_left_edge = next_edge - write_prev_bc_left_edge;
        printf("Period: %10u (%8.3f) Phase: %6d (%8.3f) Next edge: %10u (%8x) ",
               write_pll_period, (double)write_pll_period / 65536.0,
               pll_phase_offset, (double)pll_phase_offset / 65536.0, next_edge, next_edge);

        // If the next edge would fall in the last bitcell, ignore it.
        if (distance_from_prev_bc_left_edge < write_pll_period)
        {
            printf("Runt\n");
            continue;
        }

        // Advance to the current bitcell
        uint32_t curr_bc_left_edge = write_prev_bc_left_edge + write_pll_period;
        uint32_t distance_from_curr_bc_left_edge = next_edge - curr_bc_left_edge;
        printf("Curr bc: %10u (%8x) Dist: %10d (%8.3f) ", curr_bc_left_edge, curr_bc_left_edge,
               distance_from_curr_bc_left_edge, (double)distance_from_curr_bc_left_edge / 65536.0);

        // Record zeros for each bitcell that passed before this pulse
        int zeros = 0;
        while (distance_from_curr_bc_left_edge > write_pll_period)
        {
            bc_dat <<= 1;
            bc_prod++;

            if (!(bc_prod & 31))
                bc_buf[((bc_prod - 1) / 32) & bc_bufmask] = htobe32(bc_dat);

            zeros++;
            distance_from_curr_bc_left_edge -= write_pll_period;
            curr_bc_left_edge += write_pll_period;
        }

        printf("Zeros: %3d Edge bc: [%10u,%10u) ([%8x,%8x)) Edge pos: %1u ",
               zeros,
               curr_bc_left_edge, curr_bc_left_edge + write_pll_period,
               curr_bc_left_edge, curr_bc_left_edge + write_pll_period,
               distance_from_curr_bc_left_edge / (write_pll_period / 16));

        // Record a one for this bitcell
        bc_dat = (bc_dat << 1) | 1;
        bc_prod++;
        if (!(bc_prod & 31))
            bc_buf[((bc_prod - 1) / 32) & bc_bufmask] = htobe32(bc_dat);

        // If the edge lands more than 1/8th of a pll period from the center,
        // adjust the pll phase by 1/8 so the pulse moves toward the center
        uint32_t pll_phase_adjust = write_pll_period / 8;
        uint32_t pll_phase_early_threshold = write_pll_period * 3 / 8;
        uint32_t pll_phase_late_threshold = write_pll_period * 5 / 8;

        if (distance_from_curr_bc_left_edge < pll_phase_early_threshold)
        {
            // Pulse was early, decrease pll phase
            pll_phase_offset -= pll_phase_adjust;
            curr_bc_left_edge -= pll_phase_adjust;
            write_pll_phase_decs++;
            printf("Early. phase- %8.3f ", (double)pll_phase_offset / 65536.0);
        }
        else if (distance_from_curr_bc_left_edge > pll_phase_late_threshold)
        {
            // Pulse was late, increase pll phase
            pll_phase_offset += pll_phase_adjust;
            curr_bc_left_edge += pll_phase_adjust;
            write_pll_phase_incs++;
            printf("Late.  phase+ %8.3f ", (double)pll_phase_offset / 65536.0);
        }

        if (write_pll_phase_incs + write_pll_phase_decs >= 5)
        {
            int history_trend = (int)write_pll_phase_incs - (int)write_pll_phase_decs;

            if (history_trend > 2)
            {
                // Recent phase adjustments were mostly increases, frequency must be fast
                write_pll_period += write_pll_period_adjust;
                if (write_pll_period > write_pll_period_max)
                {
                    write_pll_period = write_pll_period_max;
                }
                printf("period+ %8.3f", (double)write_pll_period / 65536.0);
            }
            else if (history_trend < -2)
            {
                // Recent phase adjustments were mostly decreases, frequency must be slow
                write_pll_period -= write_pll_period_adjust;
                if (write_pll_period < write_pll_period_min)
                {
                    write_pll_period = write_pll_period_min;
                }
                printf("period- %8.3f", (double)write_pll_period / 65536.0);
            }

            write_pll_phase_incs = 0;
            write_pll_phase_decs = 0;
        }

        write_prev_bc_left_edge = curr_bc_left_edge;

        printf("\n");
    }

    bc_buf[(bc_prod / 32) & bc_bufmask] = htobe32(bc_dat << (-bc_prod & 31));
    return bc_prod;
};

uint32_t nco_715k(uint16_t *ff_samples, size_t ff_sample_count, uint32_t *bc_buf, uint32_t bc_bufmask)
{
    // image struct
    uint16_t write_bc_ticks = 72;

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

        phase_integral += phase_error / 256;
        int32_t phase_proportional = phase_error / 8;

        //printf("P: %10d I: %10d ", phase_proportional, phase_integral);

        phase_step = (uint32_t)((int32_t)(1 << 16) + phase_integral + phase_proportional);

        //printf("Phase step: %10u\n", phase_step);
    }

    bc_buf[(bc_prod / 32) & bc_bufmask] = htobe32(bc_dat << (-bc_prod & 31));
    return bc_prod;
};

uint32_t nco_358k(uint16_t *ff_samples, size_t ff_sample_count, uint32_t *bc_buf, uint32_t bc_bufmask)
{
    // image struct
    uint16_t write_bc_ticks = 72;

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

    // f_n = 358,000 Hz
    // k_l = 0.0625  = 1/16
    // k_i = 0.000976 = 1/1024

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

        phase_integral += phase_error / 1024;
        int32_t phase_proportional = phase_error / 16;

        //printf("P: %10d I: %10d ", phase_proportional, phase_integral);

        phase_step = (uint32_t)((int32_t)(1 << 16) + phase_integral + phase_proportional);

        //printf("Phase step: %10u\n", phase_step);
    }

    bc_buf[(bc_prod / 32) & bc_bufmask] = htobe32(bc_dat << (-bc_prod & 31));
    return bc_prod;
};

struct algorithm
{
    const char *name;
    uint32_t (*func)(uint16_t *ff_samples, size_t ff_sample_count, uint32_t *bc_buf, uint32_t bc_buf_mask);
};

static struct algorithm ALGS[] = {
    {"ff_v341", &ff_v341},
    {"ff_master", &ff_master},
    {"ff_v341_crude_pll", &ff_v341_crude_pll},
    {"ff_master_greaseweazle_default_pll", ff_master_greaseweazle_default_pll},
    {"ff_master_greaseweazle_fallback_pll", ff_master_greaseweazle_fallback_pll},
    {"fdc9216", fdc9216},
    {"nco_715k", nco_715k},
    {"nco_358k", nco_358k},
    {NULL, NULL},
};

void usage(const char *const progname)
{
    fprintf(stderr, "Usage: %s <ff_samples> <hfe_out> <algorithm>\n", progname);
    fprintf(stderr, "\n");
    fprintf(stderr, "Algorithms:\n");

    for (struct algorithm *alg = ALGS; alg->name != NULL; alg += 1)
    {
        fprintf(stderr, "\t* %s\n", alg->name);
    }

    exit(1);
}

int main(int argc, const char *const argv[])
{
    if (argc < 4)
    {
        usage(argv[0]);
    }

    const char *const ff_sample_path = argv[1];
    const char *const hfe_path = argv[2];
    const char *const algorithm = argv[3];

    // Open sample input file
    FILE *ff_sample_fd = fopen(ff_sample_path, "rb");
    if (ff_sample_fd == NULL)
    {
        fprintf(stderr, "ERROR: Unable to open ff samples file: %s\n", ff_sample_path);
        return 1;
    }

    // Figure out how big the sample file is.
    if (fseek(ff_sample_fd, 0, SEEK_END) < 0)
    {
        fprintf(stderr, "ERROR: failed to seek to end of ff sample file: %s\n", strerror(errno));
        return 1;
    }
    long ff_sample_size = ftell(ff_sample_fd);
    if (ff_sample_size < 0)
    {
        fprintf(stderr, "ERROR: unable to read current ff sample file position: %s\n", strerror(errno));
        return 1;
    }
    rewind(ff_sample_fd);

    // Allocate memory for samples
    size_t ff_sample_count = ff_sample_size / sizeof(uint16_t);
    uint16_t *ff_samples = calloc(ff_sample_count, sizeof(uint16_t));
    if (ff_samples == NULL)
    {
        fprintf(stderr, "ERROR: failed to allocate memory for %lu samples\n", ff_sample_count);
        return 1;
    }

    // Read samples into buffer
    {
        int res = fread(ff_samples, sizeof(uint16_t), ff_sample_count, ff_sample_fd);
        if (res < 0)
        {
            fprintf(stderr, "ERROR: error reading samples from file: %s\n", strerror(errno));
            return 1;
        }
        else if (res != ff_sample_count)
        {
            fprintf(stderr, "ERROR: only read %d samples of %lu expected\n", res, ff_sample_count);
            return 1;
        }
        fclose(ff_sample_fd);
    }

    /* Process the flux timings into the raw bitcell buffer. */

    printf("Starting to process flux to bitcells\n");

    uint32_t *bc_buf = malloc(BC_BUF_SIZE_BYTES);
    uint32_t bc_bufmask = (BC_BUF_SIZE_BYTES / 4) - 1;

    struct algorithm *alg = ALGS;
    while (alg->name != NULL)
    {
        if (strcmp(algorithm, alg->name) == 0)
        {
            break;
        }
        alg += 1;
    }

    if (alg->name == NULL)
    {
        fprintf(stderr, "Unknown algorithm: %s\n", algorithm);
        return 1;
    }

    printf("Running %s\n", alg->name);
    uint32_t bc_prod = alg->func(ff_samples, ff_sample_count, bc_buf, bc_bufmask);

    if (bc_prod / 4 >= BC_BUF_SIZE_BYTES)
    {
        fprintf(stderr, "ERROR: decoded more bitcells than buffer space\n");
        return 1;
    }

    printf("Decoded %u bitcells\n", bc_prod);
    /* Round up to next byte and word in case last byte is partial */
    size_t bc_bytes = (bc_prod + 7) / 8;
    size_t bc_words = bc_bytes / 4;

    /* Write HFE */
    FILE *hfe_fd = fopen(hfe_path, "w");
    if (hfe_fd == NULL)
    {
        fprintf(stderr, "ERROR: unable to open output HFE file: %s\n", strerror(errno));
        return 1;
    }

    const uint8_t header[] = {
        'H',
        'X',
        'C',
        'P',
        'I',
        'C',
        'F',
        'E',
        /* Revision */ 0x0,
        /* Number of tracks */ 0x1,
        /* Number of sides */ 0x1,
        /* Track encoding */ 0xFF /* Unknown */,
        /* Bitrate (kpbs) */ 0xf4,
        0x01, /* 500 */
        /* RPM */ 0x00,
        0x00,
        /* Interface mode */ 0x07 /* GENERIC_SHUGGART_DD_FLOPPYMODE */,
        /* Reserved */ 0,
        /* Track list offset */ 0x01,
        0x00,
    };
    fwrite(header, sizeof(header), 1, hfe_fd);

    // Track list
    size_t track_data_length_bytes = bc_bytes * 2;
    const uint8_t track_list[] = {
        /* Track data offset */ 0x02,
        0x00,
        /* Track data length */ (track_data_length_bytes & 0xFF),
        (track_data_length_bytes >> 8) & 0xFF,
    };
    fseek(hfe_fd, 0x200, SEEK_SET);
    fwrite(track_list, sizeof(track_list), 1, hfe_fd);

    // Track data
    for (int ii = 0; ii < bc_words; ++ii)
    {
        uint32_t bits_left_to_right = be32toh(bc_buf[ii]);
        uint8_t bits_out[4];

        for (int jj = 31; jj >= 0; --jj, bits_left_to_right >>= 1)
        {
            int bit = bits_left_to_right & 1;

            int byte_idx = jj / 8;
            bits_out[byte_idx] = bits_out[byte_idx] << 1 | bit;
        }

        long byte_number = ii * 4;
        long block_number = byte_number / 256;
        long offset = 0x400 + (block_number * 512) + (byte_number % 256);
        //printf("Writing word %d (data: 0x%x, 0x%x 0x%x 0x%x 0x%x) to offset 0x%lx\n", ii, be32toh(bc_buf[ii]), bits_out[0], bits_out[1], bits_out[2], bits_out[3], offset);
        fseek(hfe_fd, offset, SEEK_SET);
        fwrite(bits_out, 4, 1, hfe_fd);
    }

    fclose(hfe_fd);

    return 0;
}
