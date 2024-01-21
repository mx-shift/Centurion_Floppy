#ifndef FF_MASTER_H_
#define FF_MASTER_H_

#include <stdlib.h>
#include <stdint.h>

uint32_t ff_master(uint16_t write_bc_ticks, uint16_t *ff_samples, size_t ff_sample_count, uint32_t *bc_buf, uint32_t bc_bufmask);

#endif