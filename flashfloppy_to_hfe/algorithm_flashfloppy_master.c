#include <stdlib.h>
#include <stdint.h>

#include "algorithm_flashfloppy_master.h"

static uint32_t flashfloppy_master(
    uint16_t write_bc_ticks,
    uint16_t *ff_samples,
    size_t ff_sample_count,
    uint32_t *bc_buf,
    uint32_t bc_bufmask,
    struct kv_pair *params,
    struct data_logger *logger)
{
    uint64_t timestamp = 0ULL;

    /* FlashFloppy master */
    int cell = write_bc_ticks;

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
        timestamp += (uint16_t)(next - prev);
        prev = next;

        while ((curr -= cell) > 0)
        {
            bc_dat <<= 1;
            bc_prod++;
            if (!(bc_prod & 31))
                bc_buf[((bc_prod - 1) / 32) & bc_bufmask] = htobe32(bc_dat);
        }

        data_logger_event(logger, timestamp, curr + (cell >> 1));

        bc_dat = (bc_dat << 1) | 1;
        bc_prod++;
        if (!(bc_prod & 31))
            bc_buf[((bc_prod - 1) / 32) & bc_bufmask] = htobe32(bc_dat);
    }

    bc_buf[(bc_prod / 32) & bc_bufmask] = htobe32(bc_dat << (-bc_prod & 31));
    return bc_prod;
}

struct algorithm algorithm_flashfloppy_master = {
    .name = "flashfloppy_master",
    .func = flashfloppy_master,
    .params = NULL,
};