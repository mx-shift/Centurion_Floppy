#ifndef ALGORITHM_H_
#define ALGORITHM_H_

#include <stdint.h>
#include <stdlib.h>

struct algorithm
{
    const char *name;
    uint32_t (*func)(uint16_t write_bc_ticks, uint16_t *ff_samples, size_t ff_sample_count, uint32_t *bc_buf, uint32_t bc_buf_mask);
};

#endif