#ifndef FUZZ_DMA_H
#define FUZZ_DMA_H

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/dma-mapping.h>

dma_addr_t fuzz_add_s_dma(void *vptr, size_t size, enum dma_data_direction dir);
void fuzz_remove_s_dma(dma_addr_t handle, size_t size, enum dma_data_direction dir);
void fuzz_sync_s_dma(dma_addr_t handle, size_t size, enum dma_data_direction dir);
void fuzz_remove_c_dma_alloc(void *vptr, size_t size, dma_addr_t handle);
dma_addr_t fuzz_add_c_dma_alloc(void *vptr, size_t size);
void set_fuzz_dma(int f);

#endif

