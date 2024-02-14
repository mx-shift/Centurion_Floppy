#ifndef ALGORITHM_H_
#define ALGORITHM_H_

#include <stdint.h>
#include <stdlib.h>

#include "data_logger.h"
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
    uint32_t (*func)(
        uint16_t write_bc_ticks,
        uint16_t *ff_samples,
        size_t ff_sample_count,
        uint32_t *bc_buf,
        uint32_t bc_buf_mask,
        struct kv_pair *params,
        struct data_logger *logger);
    const struct parameter *params;
};

#endif