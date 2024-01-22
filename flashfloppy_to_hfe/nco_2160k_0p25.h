#ifndef NCO_2160_0P25_H_
#define NCO_2160_0P25_H_

#include <stdint.h>
#include <stdlib.h>

uint32_t nco_2160k_0p25(uint16_t write_bc_ticks, uint16_t *ff_samples, size_t ff_sample_count, uint32_t *bc_buf, uint32_t bc_bufmask);

#endif