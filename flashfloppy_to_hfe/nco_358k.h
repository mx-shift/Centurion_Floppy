#ifndef NCO_358K_H_
#define NCO_358K_H_

#include <stdlib.h>
#include <stdint.h>

uint32_t nco_358k(uint16_t write_bc_ticks, uint16_t *ff_samples, size_t ff_sample_count, uint32_t *bc_buf, uint32_t bc_bufmask);

#endif