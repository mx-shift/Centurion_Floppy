#ifndef ALGORITHM_H_
#define ALGORITHM_H_

#include <stdint.h>
#include <stdlib.h>

#include "kv_pair.h"

struct parameter
{
    const char *name;
    uint8_t required;
    const char *description;
};

struct algorithm
{
    const char *name;
    uint32_t (*func)(uint16_t write_bc_ticks, uint16_t *ff_samples, size_t ff_sample_count, uint32_t *bc_buf, uint32_t bc_buf_mask, struct kv_pair *params);
    const struct parameter *params;
};

#endif