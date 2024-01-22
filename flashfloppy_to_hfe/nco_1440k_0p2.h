#ifndef NCO_1440K_0P2_H_
#define NCO_1440K_0P2_H_

#include <stdint.h>
#include <stdlib.h>

uint32_t nco_1440k_0p2(uint16_t write_bc_ticks, uint16_t *ff_samples, size_t ff_sample_count, uint32_t *bc_buf, uint32_t bc_bufmask);

#endif