#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/pci.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/types.h>
#include <linux/io.h>
#include <linux/pci_ids.h>
#include <linux/dma-mapping.h>
#include <linux/dma-map-ops.h>
#include <asm/host_ops.h>

#include <linux/dma-mapping.h>
#include <linux/scatterlist.h>
#include <linux/mm.h>

#include "fuzz.h"

#define WARN_DMA_CTRL
#undef WARN_DMA_CTRL

static int fuzz_dma = 0;
void __attribute__((weak)) __asan_poison_memory_region(unsigned long a, unsigned long l);
void __attribute__((weak)) __asan_unpoison_memory_region(unsigned long a, unsigned long l);

#define DMA_TO_CPU(x) ((uint64_t)x + 0x700000000000ULL)
#define CPU_TO_DMA(x) ((uint64_t)x - 0x700000000000ULL)
// called from asan
void __attribute__((no_sanitize("address"))) maybe_inj_dma(unsigned long addr, unsigned long size, int is_write) {
  uint64_t new_val = 0xdeadbeefcafebabe;
  uint64_t old_val = 0x0;
  switch (size) {
     case 8:
        old_val = *(uint64_t *)addr;
        break;
     case 4:
        old_val = *(uint32_t *)addr;
        break;
     case 2:
        old_val = *(uint16_t *)addr;
        break;
     case 1:
        old_val = *(uint8_t *)addr;
        break;
     default:
        break;
  }
  if(size <= 8) {
     lkl_ops->fuzz_ops->is_ptr_leak((void*)old_val);
  } else {
     lkl_ops->fuzz_ops->scan_for_ptr_leaks((void*)addr, size);
  }

  if(fuzz_dma && !is_write) {
     lkl_ops->fuzz_ops->get_n((uint8_t*)addr, size);
  }

  if(lkl_ops->fuzz_ops->do_trace(FUZZ_TRACE_DMAINJ)) {
     if(!is_write) {
        switch (size) {
           case 8:
              new_val = *(uint64_t *)addr;
              break;
           case 4:
              new_val = *(uint32_t *)addr;
              break;
           case 2:
              new_val = *(uint16_t *)addr;
              break;
           case 1:
              new_val = *(uint8_t *)addr;
              break;
           default:
              break;
        }
     }
     pr_err("%s: %6s @%lx(+%ld) -> old %18llx, new %18llx\n", __FUNCTION__, is_write ? "write" : "read", addr, size, old_val, new_val);
  }
}
EXPORT_SYMBOL(maybe_inj_dma);

dma_addr_t fuzz_add_s_dma(void *vptr, size_t size, enum dma_data_direction dir) {
   if(vptr != 0 && dir != PCI_DMA_FROMDEVICE && dir != DMA_FROM_DEVICE) {
      lkl_ops->fuzz_ops->scan_for_ptr_leaks(vptr, size);
   }
   if(vptr == 0 || dir == DMA_TO_DEVICE || dir == PCI_DMA_TODEVICE || !fuzz_dma) {
      return (dma_addr_t)CPU_TO_DMA(vptr);
   }
   if(lkl_ops->fuzz_ops->do_trace(FUZZ_TRACE_DMA)) {
      pr_info("%s: cpu: %llx, dev: %llx, size: %ld\n", __FUNCTION__, (uint64_t)vptr, (uint64_t)CPU_TO_DMA(vptr), size);
   }
   lkl_ops->fuzz_ops->store_dma_handle((uint64_t)CPU_TO_DMA(vptr), size);
   return (dma_addr_t)CPU_TO_DMA(vptr);
}

void fuzz_remove_s_dma(dma_addr_t handle, size_t size, enum dma_data_direction dir) {
   int len;
   if(handle == 0 || dir == DMA_TO_DEVICE || dir == PCI_DMA_TODEVICE || !fuzz_dma) {
      return;
   }
   if(lkl_ops->fuzz_ops->do_trace(FUZZ_TRACE_DMA)) {
      pr_info("%s: cpu: %llx, dev: %llx, size: %ld\n", __FUNCTION__, (uint64_t)DMA_TO_CPU(handle), (uint64_t)handle, size);
   }
   len = lkl_ops->fuzz_ops->get_dma_handle(handle);
   if(len) {
      if(lkl_ops->fuzz_ops->do_trace(FUZZ_TRACE_DMACTRL)) {
         if(len > 1 && len > size) { // Note(feli): len == 1 indicates no tracking
            pr_warn("Warning: potential dma len contolled unmap %ld/%ld\n", len, size);
            size = len;
         }
      }
      lkl_ops->fuzz_ops->get_n((uint8_t*)DMA_TO_CPU(handle), size);
      // might still be synced, dma mappings are reset on each fuzzing iteration anyway
      //lkl_ops->fuzz_ops->remove_dma_handle(handle);
   } else if(lkl_ops->fuzz_ops->do_trace(FUZZ_TRACE_DMACTRL)) {
      pr_warn("Warning: potential dma addr contolled unmap %llx\n", handle);
   }
}

void fuzz_sync_s_dma(dma_addr_t handle, size_t size, enum dma_data_direction dir) {
  int len;
  if(handle == 0 || dir == DMA_TO_DEVICE || dir == PCI_DMA_TODEVICE || !fuzz_dma) {
     return;
  }
  if(lkl_ops->fuzz_ops->do_trace(FUZZ_TRACE_DMA)) {
     pr_info("%s: cpu: %llx, dev: %llx, size: %ld\n", __FUNCTION__, (uint64_t)DMA_TO_CPU(handle), (uint64_t)handle, size);
  }
  len = lkl_ops->fuzz_ops->get_dma_handle(handle);
  if(len) {
     if(lkl_ops->fuzz_ops->do_trace(FUZZ_TRACE_DMACTRL)) {
        if(len > 1 && len > size) { // Note(feli): len == 1 indicates no tracking
           pr_warn("Warning: potential dma len contolled sync %ld/%ld\n", len, size);
           size = len;
        }
     }
     lkl_ops->fuzz_ops->get_n((uint8_t*)DMA_TO_CPU(handle), size);
  } else if(lkl_ops->fuzz_ops->do_trace(FUZZ_TRACE_DMACTRL)) {
     pr_warn("Warning: potential dma addr contolled sync %llx\n", handle);
  }
}

dma_addr_t fuzz_add_c_dma_alloc(void *vptr, size_t size) {
  if(lkl_ops->fuzz_ops->do_trace(FUZZ_TRACE_DMA)) {
     pr_info("%s: cpu: %llx, dev: %llx, size: %ld\n", __FUNCTION__, (uint64_t)vptr, (uint64_t)CPU_TO_DMA(vptr), size);
  }
  __asan_poison_memory_region((unsigned long)vptr, size);
  //if(fuzz_dma) __asan_poison_memory_region((unsigned long)vptr, size);
  return (dma_addr_t)CPU_TO_DMA(vptr);
}

void fuzz_remove_c_dma_alloc(void *vptr, size_t size, dma_addr_t handle) {
  if(lkl_ops->fuzz_ops->do_trace(FUZZ_TRACE_DMA)) {
     pr_info("%s: cpu: %llx, dev: %llx, size: %ld\n", __FUNCTION__, (uint64_t)vptr, (uint64_t)handle, size);
  }
  __asan_unpoison_memory_region((unsigned long)vptr, size);
  //if(fuzz_dma) __asan_unpoison_memory_region((unsigned long)vptr, size);
  lkl_ops->fuzz_ops->scan_for_ptr_leaks(vptr, size);
}


void set_fuzz_dma(int f) {
  pr_info("%s %d\n", __FUNCTION__, f);
  fuzz_dma = f;
}

static void *lkl_fuzz_dma_alloc(struct device *dev, size_t size,
    dma_addr_t *dma_handle, gfp_t gfp,
    unsigned long attrs)
{
  void *vaddr;
  struct page *p = alloc_pages(gfp, get_order(size));
  if(p==NULL) {
    pr_err("%s: failed to alloc %ld/%ld\n", __FUNCTION__, size, get_order(size));
    return NULL;
  }
  vaddr = page_to_virt(p);
  lkl_ops->fuzz_ops->nosan_memset(vaddr, 0, size);
  *dma_handle = fuzz_add_c_dma_alloc(vaddr, size);
  return vaddr;
}

static void lkl_fuzz_dma_free(struct device *dev, size_t size, void *cpu_addr,
    dma_addr_t dma_addr, unsigned long attrs)
{
  if(cpu_addr==0) {
    pr_err("%s: error invalid cpu_addr NULL\n", __FUNCTION__);
    BUG();
  }
  fuzz_remove_c_dma_alloc(cpu_addr, size, dma_addr);
  __free_pages(virt_to_page(cpu_addr), get_order(size));
}

static dma_addr_t lkl_fuzz_dma_map_page(struct device *dev, struct page *page,
    unsigned long offset, size_t size,
    enum dma_data_direction dir,
    unsigned long attrs)
{
  return fuzz_add_s_dma(page_to_virt(page) + offset, size, dir);
}

static void lkl_fuzz_dma_unmap_page(struct device *dev, dma_addr_t dma_addr,
    size_t size, enum dma_data_direction dir,
    unsigned long attrs)
{
  fuzz_remove_s_dma(dma_addr, size, dir);
}

static int lkl_fuzz_dma_map_sg(struct device *dev, struct scatterlist *sgl,
    int nents, enum dma_data_direction dir,
    unsigned long attrs)
{
  int i;
  struct scatterlist *sg;

  for_each_sg(sgl, sg, nents, i) {
    void *va;

    WARN_ON(!sg_page(sg));
    va = sg_virt(sg);
    sg_dma_address(sg) = (dma_addr_t)lkl_fuzz_dma_map_page(
        dev, sg_page(sg), sg->offset, sg->length, dir, attrs);
    sg_dma_len(sg) = sg->length;
  }
  return nents;
}

static void lkl_fuzz_dma_unmap_sg(struct device *dev, struct scatterlist *sgl,
    int nents, enum dma_data_direction dir,
    unsigned long attrs)
{
  int i;
  struct scatterlist *sg;

  for_each_sg(sgl, sg, nents, i)
    lkl_fuzz_dma_unmap_page(dev, sg_dma_address(sg), sg_dma_len(sg), dir,
          attrs);
}

static void lkl_fuzz_sync_single_for_cpu(struct device *dev, dma_addr_t dma_handle,
    size_t size, enum dma_data_direction dir) {
  fuzz_sync_s_dma(dma_handle, size, dir);
}

static void lkl_fuzz_sync_single_for_device(struct device *dev,
		dma_addr_t dma_handle, size_t size, enum dma_data_direction dir) {
   if(dir != PCI_DMA_FROMDEVICE && dir != DMA_FROM_DEVICE) {
      lkl_ops->fuzz_ops->scan_for_ptr_leaks((void*)DMA_TO_CPU(dma_handle), size);
   }
   return;
}

static int lkl_fuzz_dma_supported(struct device *dev, u64 mask)
{
  return 1;
}

const struct dma_map_ops lkl_fuzz_dma_ops = {
  .alloc = lkl_fuzz_dma_alloc,
  .free = lkl_fuzz_dma_free,
  .map_sg = lkl_fuzz_dma_map_sg,
  .unmap_sg = lkl_fuzz_dma_unmap_sg,
  .map_page = lkl_fuzz_dma_map_page,
  .unmap_page = lkl_fuzz_dma_unmap_page,
  .dma_supported = lkl_fuzz_dma_supported,
  .sync_single_for_cpu = lkl_fuzz_sync_single_for_cpu,
  .sync_single_for_device = lkl_fuzz_sync_single_for_device,
};
EXPORT_SYMBOL(lkl_fuzz_dma_ops);

