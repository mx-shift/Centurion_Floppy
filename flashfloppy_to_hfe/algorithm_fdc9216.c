#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>

#include "algorithm_fdc9216.h"

uint32_t fdc9216(uint16_t write_bc_ticks, uint16_t *ff_samples, size_t ff_sample_count, uint32_t *bc_buf, uint32_t bc_bufmask, struct kv_pair *params)
{
    // A PLL that actually adjusts phase gradually

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
}

struct algorithm algorithm_fdc9216 = {
    .name = "fdc9216",
    .func = fdc9216,
    .params = NULL,
};