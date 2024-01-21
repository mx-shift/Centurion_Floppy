#ifndef FF_MASTER_GREASEWEAZLE_FALLBACK_PLL_H_
#define FF_MASTER_GREASEWEAZLE_FALLBACK_PLL_H_

#include <stdlib.h>
#include <stdint.h>

uint32_t ff_master_greaseweazle_fallback_pll(uint16_t *ff_samples, size_t ff_sample_count, uint32_t *bc_buf, uint32_t bc_bufmask);

#endif
