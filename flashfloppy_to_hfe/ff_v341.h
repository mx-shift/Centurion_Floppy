#ifndef FF_V341_H_
#define FF_V341_H_

#include <stdlib.h>
#include <stdint.h>

uint32_t ff_v341(uint16_t *ff_samples, size_t ff_sample_count, uint32_t *bc_buf, uint32_t bc_bufmask);

#endif