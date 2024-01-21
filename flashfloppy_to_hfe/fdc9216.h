#ifndef FDC9216_H_
#define FDC9216_H_

#include <stdlib.h>
#include <stdint.h>

uint32_t fdc9216(uint16_t *ff_samples, size_t ff_sample_count, uint32_t *bc_buf, uint32_t bc_bufmask);

#endif
