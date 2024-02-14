#include <stdlib.h>
#include <stdint.h>

#include "algorithm_flashfloppy_v341.h"

static uint32_t flashfloppy_v341(
    uint16_t write_bc_ticks,
    uint16_t *ff_samples,
    size_t ff_sample_count,
    uint32_t *bc_buf,
    uint32_t bc_bufmask,
    struct kv_pair *params,
    struct data_logger *logger)
{
    /* FlashFloppy v3.41 */
    uint16_t cell = write_bc_ticks;
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

struct algorithm algorithm_flashfloppy_v341 = {
    .name = "flashfloppy_v341",
    .func = flashfloppy_v341,
    .params = NULL,
};