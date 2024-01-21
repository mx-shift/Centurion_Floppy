#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>

#include "ff_master_greaseweazle_fallback_pll.h"

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
