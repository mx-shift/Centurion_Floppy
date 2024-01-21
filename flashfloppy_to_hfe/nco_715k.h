#ifndef NCO_715K_H_
#define NCO_715K_H_

#include <stdint.h>
#include <stdlib.h>

uint32_t nco_715k(uint16_t *ff_samples, size_t ff_sample_count, uint32_t *bc_buf, uint32_t bc_bufmask);

#endif