#include <lkl_host.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <limits.h>
#include <sys/time.h>
#include <time.h>
#include "../iomem.h"
#include "bst.h"
#include "ht.h"

#define FUZZ_TEST_PRNG
#undef  FUZZ_TEST_PRNG

#define PANIC_PTRLEAK
#undef  PANIC_PTRLEAK

#define WARN_PTRLEAK
//#undef  WARN_PTRLEAK

#ifdef FUZZ_TEST_PRNG
extern void sanitizer_prng_buf_test(uint8_t* a, unsigned long b);
#endif


void * __attribute__((weak)) __asan_unsanitized_memcpy(void *dst, const void *src, unsigned long size) { return memcpy(dst, src, size); }
void * __attribute__((weak)) __asan_unsanitized_memset(void* s, int c, unsigned long n) { return memset(s, c, n); }

typedef unsigned long int  u4;
typedef struct ranctx { u4 a; u4 b; u4 c; u4 d; u4 lrv; u4 lrv_j;} ranctx;

#define MAX_IRQS 512
#define MAX_SIRQD 16
typedef struct lkl_fuzz_buf {
  uint8_t *data;
  unsigned long size;
  unsigned int idx;
  unsigned int active;
  ranctx x;
  ranctx x_mod;
#ifdef FUZZ_TEST_PRNG
  unsigned long prng_buf_idx;
  uint8_t prng_buf[2048];
#endif
} lkl_fuzz_buf;

typedef struct lkl_fuzzer {
  lkl_fuzz_buf buf;
  struct lkl_mutex *mu;
  enum LKL_FUZZ_DEV_TYPE dev_type;
  void* dev_handle;
  // custom device reset callback
  lkl_fp_dev_reset dev_reset;
  // irq lines requested by module
  uint32_t irqs[MAX_IRQS];
  int irq_in_use[MAX_IRQS];
  int irq_requested[MAX_IRQS];
  // devnodes requested by module
  struct lkl_fuzz_devnode devnode[LKL_FUZZ_MAX_DEVT];
  size_t n_devnode;
  // callback on empty IO-stream
  lkl_fp_done_callback done_cb;
  int done_cb_called;
  // delay concurrernt work
  unsigned long work_delay;
  unsigned long task_delay;
  unsigned long sirq_delay;
  int sirq_delays[MAX_SIRQD];
  int sirq_nos[MAX_SIRQD];
  size_t n_sirq_delays;
  // minimize delays
  int minimize_delays;
  int minimize_wq_delays;
  int minimize_timeouts;
  int minimize_timebefore;
  int minimize_timeafter;
  // apply different levels of patches
  int apply_hacks;
  int apply_patch;
  int apply_patch_2;
  // keep track of dma mappings
  int use_bst;
  struct node *dma_bst_root;
  // notfiers for queued works/waits
  pthread_mutex_t wait_started_mutex;
  pthread_cond_t wait_started_cond;
  pthread_mutex_t conc_ended_mutex;
  pthread_cond_t conc_ended_cond;
  int stop_work_pending;
  int targeted_irqs;
  int fast_irqs;
  struct fuzz_debug_options dopts;
} lkl_fuzzer;

static lkl_fuzzer fb;


// Note(feli): We want some randomness in generated IO stream to avoid deadlocks
// at the same time drivers usually expect simple values (0, 1, 0xff, ...)
// we use a prng in order to avoid storing the values,
// libfuzzer has been extended to generate the same values and store them in the
// corpus files (needs testing though)
#define rot(x,k) (((x)<<(k))|((x)>>(32-(k))))
static u4 ranval_mod( ranctx *x ) {
  u4 e;
  e = x->a - rot(x->b, 27);
  x->a = x->b ^ rot(x->c, 17);
  x->b = x->c + x->d;
  x->c = x->d + e;
  x->d = e + x->a;
  return x->d;
}

// 0-256
const uint8_t prob_zero = 128;
const uint8_t prob_one = prob_zero + 64;
const uint8_t prob_ff = prob_one + 32;
static u4 ranval( ranctx *x ) {
  u4 e;
  uint8_t rv_mod = (ranval_mod(&fb.buf.x_mod)) % 256;
  if(rv_mod < prob_zero) {
    return 0;
  } else if(rv_mod < prob_one) {
    return 1;
  } else if(rv_mod < prob_ff) {
    return -1;
  }
  //if(rv_mod % 7 == 0) {
  //  return 0;
  //}
  //if(rv_mod % 13 == 0) {
  //  return 1;
  //}
  //if(rv_mod % 17 == 0) {
  //  return 0xffffffff;
  //}
  e = x->a - rot(x->b, 27);
  x->a = x->b ^ rot(x->c, 17);
  x->b = x->c + x->d;
  x->c = x->d + e;
  x->d = e + x->a;
  return x->d;
}

static void raninit( ranctx *x, u4 seed ) {
  u4 i;
  fb.buf.x.lrv = 0;
  fb.buf.x.lrv_j = sizeof(u4);
  x->a = 0xf1ea5eed, x->b = x->c = x->d = seed;
  for (i=0; i<20; ++i) {
    (void)ranval_mod(x);
  }
}

// Note(feli): we keep the overlap from the last val
// this makes special values less likely since hey have to
// align. not sure if this is a good thing
static void __attribute__((no_sanitize("address"))) memset_ranval(uint8_t *dst, unsigned long n) {
  unsigned long i, j, off=0;
  u4 rv = fb.buf.x.lrv;
  for(j=fb.buf.x.lrv_j; j<sizeof(u4) && off<n; j++) {
#ifdef FUZZ_TEST_PRNG
    lkl_printf("Using remaining at off:%d rv[%d] %x (%lx)\n", off, j, (rv & (0xff << (j*8))) >> (j*8), rv);
#endif
    dst[off] = (rv & (0xffUL << (j*8))) >> (j*8);
#ifdef FUZZ_TEST_PRNG
    if(fb.prng_buf_idx<2048) {
      fb.prng_buf[fb.prng_buf_idx++] = dst[off];
    }
#endif
    off++;
  }
  fb.buf.x.lrv_j += off;
  for(i=off; i<n; i+=sizeof(u4)) {
    rv = ranval(&fb.buf.x);
    fb.buf.x.lrv = rv;
    fb.buf.x.lrv_j = 0;
    for(j=0; j<sizeof(u4); j++) {
      if(i+j < n) {
        fb.buf.x.lrv_j = j+1;
        dst[i+j] = (rv & (0xffUL << (j*8))) >> (j*8);
#ifdef FUZZ_TEST_PRNG
        if(fb.prng_buf_idx<2048) {
          fb.prng_buf[fb.prng_buf_idx++] = dst[i+j];
        }
#endif
      }
    }
  }
}

static int fuzz_do_trace(enum LKL_FUZZ_TRACE_TYPE type) {
   switch(type) {
      case LKL_FUZZ_TRACE_MOD:
         return fb.dopts.mod;
      case LKL_FUZZ_TRACE_IO:
         return fb.dopts.io;
      case LKL_FUZZ_TRACE_IRQ:
         return fb.dopts.irq;
      case LKL_FUZZ_TRACE_MSIIRQ:
         return fb.dopts.msiirq;
      case LKL_FUZZ_TRACE_DMA:
         return fb.dopts.dma;
      case LKL_FUZZ_TRACE_DMAINJ:
         return fb.dopts.dmainj;
      case LKL_FUZZ_TRACE_CONF:
         return fb.dopts.conf;
      case LKL_FUZZ_TRACE_WAITERS:
         return fb.dopts.waiters;
      case LKL_FUZZ_TRACE_BST:
         return fb.dopts.bst;
      case LKL_FUZZ_TRACE_DONECB:
         return fb.dopts.donecb;
      case LKL_FUZZ_TRACE_DEVNODES:
         return fb.dopts.devnodes;
      case LKL_FUZZ_TRACE_PTRLEAKS:
         return fb.dopts.ptrleaks;
      case LKL_FUZZ_TRACE_DMACTRL:
         return fb.dopts.dmactrl;
      default:
         lkl_printf("Warning: unknown trace option: %d\n", type);
         break;
   }
   return 0;
}

static void fuzz_set_dev_data(void *handle, lkl_fp_dev_reset dev_reset, enum LKL_FUZZ_DEV_TYPE type, unsigned int irq) {
  int i;
  char buf[64];
  fb.dev_handle = handle;
  fb.dev_reset = dev_reset;
  fb.dev_type = type;
  fb.irqs[0] = irq;
  fb.irq_in_use[0] = 1;
  fb.irq_requested[0] = 0;
  for(i=1; i<MAX_IRQS; i++) {
    snprintf(buf, 64, "fuzz_pci_%d", i);
    fb.irqs[i] = lkl_get_free_irq(buf);
    fb.irq_in_use[i] = 0;
  }
}

static void fuzz_notify_irq_free(unsigned int irq) {
  int i, irq_idx=-1;
  if(fuzz_do_trace(LKL_FUZZ_TRACE_IRQ)) {
     lkl_printf("%s %d\n", __FUNCTION__, irq);
  }
  for(i=0; i<MAX_IRQS; i++) {
    if(!fb.irq_in_use[i]) {
      continue;
    }
    if(fb.irqs[i] == irq) {
      irq_idx = i;
      break;
    }
  }
  if(irq_idx < 0) {
    return;
  }
  if(fuzz_do_trace(LKL_FUZZ_TRACE_IRQ)) {
     lkl_printf("%s free requested irq[%d] %d\n", __FUNCTION__, irq_idx, fb.irqs[irq_idx]);
  }
  fb.irq_requested[irq_idx] = 0;
}

static void fuzz_notify_irq_request(unsigned int irq) {
  int i, irq_idx=-1;
  if(fuzz_do_trace(LKL_FUZZ_TRACE_IRQ)) {
     lkl_printf("%s %d\n", __FUNCTION__, irq);
  }
  for(i=0; i<MAX_IRQS; i++) {
    if(!fb.irq_in_use[i]) {
      continue;
    }
    if(fb.irqs[i] == irq) {
      irq_idx = i;
      break;
    }
  }
  if(irq_idx < 0) {
    return;
  }
  if(fuzz_do_trace(LKL_FUZZ_TRACE_IRQ)) {
     lkl_printf("%s set requested irq[%d] %d\n", __FUNCTION__, irq_idx, fb.irqs[irq_idx]);
  }
  if(fb.fast_irqs==1) {
     lkl_trigger_irq(irq);
  }
  fb.irq_requested[irq_idx] = 1;
}

static int fuzz_free_irq(unsigned int irq) {
  int i;
  for(i=1; i<MAX_IRQS; i++) {
    if(!fb.irq_in_use[i]) {
      continue;
    }
    if(fb.irqs[i] == irq) {
      if(fuzz_do_trace(LKL_FUZZ_TRACE_IRQ)) {
         lkl_printf("%s set free irq[%d]=%d\n", __FUNCTION__, i, fb.irqs[i]);
      }
      fb.irq_in_use[i] = 0;
      return 0;
    }
  }
  return -1;
}

static int fuzz_get_free_irq(void) {
  int i;
  for(i=1; i<MAX_IRQS; i++) {
    if(!fb.irq_in_use[i]) {
      fb.irq_in_use[i] = 1;
      if(fuzz_do_trace(LKL_FUZZ_TRACE_IRQ)) {
         lkl_printf("%s pick free irq[%d]=%d\n", __FUNCTION__, i, fb.irqs[i]);
      }
      return fb.irqs[i];
    }
  }
  return -1;
}

static void fuzz_reset_irqs(void) {
  int i;
  for(i=1; i<MAX_IRQS; i++) {
    fb.irq_in_use[i] = 0;
    fb.irq_requested[i] = 0;
  }
  fb.irq_requested[0] = 0;
}

static int fuzz_is_active(void) {
  return __atomic_load_n(&fb.buf.active, __ATOMIC_SEQ_CST);
}

static void fuzz_notify_devnode_add(const char* name, unsigned long devt, int type) {
  if(!fuzz_is_active()) {
     return;
  }
  lkl_host_ops.mutex_lock(fb.mu);
  if(fb.n_devnode >= LKL_FUZZ_MAX_DEVT) {
      lkl_printf("WARNING: too many device nodes\n");
      lkl_host_ops.mutex_unlock(fb.mu);
      return;
  }
  if(fuzz_do_trace(LKL_FUZZ_TRACE_DEVNODES)) {
     lkl_printf("Add devnode %d, devt:%ld, type:%d\n", fb.n_devnode, devt, type);
  }
  fb.devnode[fb.n_devnode].devt = devt;
  fb.devnode[fb.n_devnode].type = type;
  fb.devnode[fb.n_devnode].active = 1;
  fb.n_devnode++;
  lkl_host_ops.mutex_unlock(fb.mu);
}

static void fuzz_notify_devnode_remove(const char* name, unsigned long devt, int type) {
  // nothing to see here
  //lkl_host_ops.mutex_lock(fb.mu);
  //lkl_host_ops.mutex_unlock(fb.mu);
}

int lkl_fuzz_get_requested_irqs(int *rirqs)
{
  int i;
  int n_req=0;
  for(i=0; i<MAX_IRQS; i++) {
    if(fb.irq_requested[i]) {
      if(fuzz_do_trace(LKL_FUZZ_TRACE_IRQ)) {
         lkl_printf("%s rirq[%d] = %d\n", __FUNCTION__, n_req, fb.irqs[i]);
      }
      rirqs[n_req++] = fb.irqs[i];
    }
  }
  return n_req;
}

int lkl_fuzz_get_devnodes(struct lkl_fuzz_devnode **dn)
{
   *dn = fb.devnode;
   return fb.n_devnode;
}

int lkl_fuzz_get_idx(void)
{
  return __atomic_load_n(&fb.buf.idx, __ATOMIC_SEQ_CST);
}

static void __memcpy(void *dst, const void *src, unsigned long size) {
  __asan_unsanitized_memcpy(dst, src, size);
}

static long fuzz_get_rem(void) {
  long rem;
  long idx;
  idx = __atomic_load_n(&fb.buf.idx, __ATOMIC_SEQ_CST);
  rem = fb.buf.size - idx;
  return rem;
}

static long __attribute__((no_sanitize("address"))) fuzz_get_n(void *dst, unsigned long n) {
  int idx;
  long rem;
  int is_done = 0;
  lkl_host_ops.mutex_lock(fb.mu);
  idx = __atomic_load_n(&fb.buf.idx, __ATOMIC_SEQ_CST);
  if(n >= INT_MAX) {
     lkl_printf("ERROR: invalid memsize %lx!!!\n", n);
     lkl_host_ops.mutex_unlock(fb.mu);
     lkl_host_ops.panic();
  }
  if(idx >= INT_MAX - (int)n) {
     lkl_printf("ERROR: fuzz_buf overflow!!!\n");
     lkl_host_ops.mutex_unlock(fb.mu);
     lkl_host_ops.panic();
  }
  if(fuzz_do_trace(LKL_FUZZ_TRACE_IO)) {
     lkl_printf("%s: fuzz %llx(+%lx)\n", __FUNCTION__, (uint64_t)dst, n);
  }
  rem = fb.buf.size - idx;
  if(rem <= 0) {
    is_done = 1;
    memset_ranval(dst, n);
  } else if(rem > 0 && rem < (long)n) {
    is_done = 1;
    __memcpy(dst, &fb.buf.data[idx], rem);
    memset_ranval(dst+rem, n-rem);
  } else if(rem > 0 && rem >= (long)n) {
    __memcpy(dst, &fb.buf.data[idx], n);
  }
  __atomic_add_fetch(&fb.buf.idx, n, __ATOMIC_SEQ_CST);
  lkl_host_ops.mutex_unlock(fb.mu);

  if(fuzz_do_trace(LKL_FUZZ_TRACE_IO)) {
     uint64_t val = 0;
     switch (n) {
        case 8:
           val = *(uint64_t *)dst;
           break;
        case 4:
           val = *(uint32_t *)dst;
           break;
        case 2:
           val = *(uint16_t *)dst;
           break;
        case 1:
           val = *(uint8_t *)dst;
           break;
        default:
           break;
     }
     lkl_printf("%s: idx %d, rem %d, size %d, val %llx\n", __FUNCTION__, idx, rem, n, val);
  }
  if(fb.done_cb && is_done > 0) {
    if(fuzz_do_trace(LKL_FUZZ_TRACE_DONECB)) {
       lkl_printf("%s calling done\n", __FUNCTION__);
    }
    fb.done_cb();
    fb.done_cb_called = 1;
  }
  return rem;
}

// Note(feli): I think we want to use the kernel function here,
// since it actually changes the task state
extern void usleep_range(unsigned long min, unsigned long max);
static void fuzz_delay_work(void) {
  if(fb.work_delay > 0) {
    lkl_printf("%s delaying by %ld\n", __FUNCTION__, fb.work_delay);
    usleep_range(fb.work_delay, fb.work_delay+10);
    lkl_printf("%s delaying by %ld returned\n", __FUNCTION__, fb.work_delay);
  }
}

static void fuzz_delay_task(void) {
  if(fb.task_delay > 0) {
    lkl_printf("%s delaying by %ld\n", __FUNCTION__, fb.task_delay);
    usleep_range(fb.task_delay, fb.task_delay+10);
    lkl_printf("%s delaying by %ld returned\n", __FUNCTION__, fb.task_delay);
  }
}

static void fuzz_delay_sirq(int no) {
   size_t i;
   if(fb.n_sirq_delays > 0) {
      for(i=0; i<fb.n_sirq_delays; i++) {
         if(no == fb.sirq_nos[i]) {
            lkl_printf("%s delaying sirq %d by %ld\n", __FUNCTION__, fb.sirq_nos[i], fb.sirq_delays[i]);
            usleep_range(fb.sirq_delays[i], fb.sirq_delays[i]+10);
            lkl_printf("%s delaying sirq %d by %ld returned\n", __FUNCTION__, fb.sirq_nos[i], fb.sirq_delays[i]);
            break;
         }
      }
   }
}


static int fuzz_minimize_delays(void) {
  return fb.minimize_delays;
}
static int fuzz_minimize_wq_delays(void) {
  return fb.minimize_wq_delays;
}
static int fuzz_minimize_timeouts(void) {
  return fb.minimize_timeouts;
}
static int fuzz_minimize_timebefore(void) {
  return fb.minimize_timebefore;
}
static int fuzz_minimize_timeafter(void) {
  return fb.minimize_timeafter;
}

static int fuzz_apply_hacks(void) {
  return fb.apply_hacks;
}
static int fuzz_apply_patch(void) {
  return fb.apply_patch;
}
static int fuzz_apply_patch_2(void) {
  return fb.apply_patch_2;
}

static void fuzz_store_dma_handle(unsigned long long handle, unsigned long size) {
   if(!fb.use_bst) {
      return;
   }
   if(fuzz_do_trace(LKL_FUZZ_TRACE_BST)) {
      printf("Store DMA %llx+%ld\n", handle, size);
   }
   lkl_host_ops.mutex_lock(fb.mu);
   fb.dma_bst_root = insert(fb.dma_bst_root, handle, size);
   lkl_host_ops.mutex_unlock(fb.mu);
}

static void fuzz_remove_dma_handle(unsigned long long handle) {
   if(!fb.use_bst) {
      return;
   }
   if(fuzz_do_trace(LKL_FUZZ_TRACE_BST)) {
      printf("Remove DMA %llx\n", handle);
   }
   lkl_host_ops.mutex_lock(fb.mu);
   fb.dma_bst_root = deleteNode(fb.dma_bst_root, handle);
   lkl_host_ops.mutex_unlock(fb.mu);
}

static int fuzz_get_dma_handle(unsigned long long handle) {
   struct node *n;
   int ret;

   if(!fb.use_bst) {
      return 1;
   }
   lkl_host_ops.mutex_lock(fb.mu);
   n = search(fb.dma_bst_root, handle);
   ret = n ? n->len : 0;
   lkl_host_ops.mutex_unlock(fb.mu);

   if(fuzz_do_trace(LKL_FUZZ_TRACE_BST)) {
      printf("Get DMA %llx -> %d\n", handle, ret);
   }
   return ret;
}

static void fuzz_trigger_wait_cond(void) {
  if(fuzz_do_trace(LKL_FUZZ_TRACE_WAITERS)) {
     lkl_printf("%s\n", __FUNCTION__);
  }
  pthread_mutex_lock(&fb.wait_started_mutex);
  if(fb.targeted_irqs != 0) {
     pthread_cond_signal(&fb.wait_started_cond);
  }
  pthread_mutex_unlock(&fb.wait_started_mutex);
}

int lkl_fuzz_has_waiters(void) {
   if(fb.targeted_irqs == 0) {
      return 0;
   }
   return ht_items();
}

static void fuzz_trigger_conc_cond(void) {
  pthread_mutex_lock(&fb.conc_ended_mutex);
  pthread_cond_signal(&fb.conc_ended_cond);
  pthread_mutex_unlock(&fb.conc_ended_mutex);
}

static int fuzz_add_waiter(void *handle) {
   int ret;
   if(fb.targeted_irqs == 0) {
      return 0;
   }
   ret = ht_insert(handle);
   if(fuzz_do_trace(LKL_FUZZ_TRACE_WAITERS)) {
      lkl_printf("Add waiter %llx, exst:%d\n", (uint64_t)handle, ret);
   }
   fuzz_trigger_wait_cond();
   return 0;
}

static int fuzz_del_waiter(void *handle) {
   void *ret;
   if(fb.targeted_irqs == 0) {
      return 0;
   }
   ret = ht_delete(handle);
   if(fuzz_do_trace(LKL_FUZZ_TRACE_WAITERS)) {
      lkl_printf("Del waiter %llx, exst:%p\n", (uint64_t)handle, ret);
   }
   return 0;
}

extern unsigned long memory_start, memory_end;
extern void __asan_print_allocation_context_buf(int, char*, long);
extern void __asan_print_allocation_context(int);
extern int __asan_get_allocation_context(void);
int sprint_symbol(char *buffer, unsigned long address);
#define KSYM_NAME_LEN 128
static void fuzz_is_ptr_leak(unsigned long long addr) {
  char namebuf[KSYM_NAME_LEN];
  if(!fuzz_do_trace(LKL_FUZZ_TRACE_PTRLEAKS)) {
     return;
  }
  //if(addr > 0x7f0000000001 && addr <= 0x7fffffffffff) {
  if(addr >= memory_start && addr <= memory_end) {
     if(sprint_symbol(namebuf, addr) > 2  && !(namebuf[0] == '0' && namebuf[1] == 'x')) {
        //__asan_print_allocation_context(__asan_get_allocation_context());
        //lkl_printf("WARNING potential pointer leak: %llx, sym %s\n", addr, namebuf);
#ifdef WARN_PTRLEAK
        fprintf(stderr, "WARNING potential pointer leak: %llx, sym %s\n", addr, namebuf);
#endif
#ifdef PANIC_PTRLEAK
        lkl_host_ops.panic();
#endif
     } else {
        //__asan_print_allocation_context(__asan_get_allocation_context());
        //lkl_printf("WARNING potential pointer leak: %llx\n", addr);
#ifdef WARN_PTRLEAK
        fprintf(stderr, "WARNING potential pointer leak: %llx\n", addr);
#endif
#ifdef PANIC_PTRLEAK
        lkl_host_ops.panic();
#endif
     }
  }
}

static void __attribute__((no_sanitize("address"))) fuzz_scan_for_ptr_leaks(void *ptr, unsigned long size) {
   unsigned long i;
   uint64_t addr;
   char namebuf[KSYM_NAME_LEN];
   if(!fuzz_do_trace(LKL_FUZZ_TRACE_PTRLEAKS)) {
      return;
   }
   if(size < sizeof(addr)) {
      return;
   }
   for(i=0; i<size - sizeof(addr); i++) {
      //lkl_printf("%lx/%lx\n", size, i);
      addr = *(uint64_t*)(ptr+i);
      //if(addr > 0x7f0000000001 && addr <= 0x7fffffffffff) {
      if(addr >= memory_start && addr <= memory_end) {
         if(sprint_symbol(namebuf, addr) > 2  && !(namebuf[0] == '0' && namebuf[1] == 'x')) {
            //__asan_print_allocation_context(__asan_get_allocation_context());
            //lkl_printf("WARNING potential pointer leak @ %llx + %lx: %llx, sym %s\n", (uint64_t)ptr, i, addr, namebuf);
#ifdef WARN_PTRLEAK
            fprintf(stderr, "WARNING potential pointer leak @ %lx + %lx: %lx, sym %s\n", (uint64_t)ptr, i, addr, namebuf);
#endif
#ifdef PANIC_PTRLEAK
            lkl_host_ops.panic();
#endif
         } else {
            //__asan_print_allocation_context(__asan_get_allocation_context());
            //lkl_printf("WARNING potential pointer leak @ %llx + %lx: %llx\n", (uint64_t)ptr, i, addr);
#ifdef WARN_PTRLEAK
            fprintf(stderr, "WARNING potential pointer leak @ %lx + %lx: %lx\n", (uint64_t)ptr, i, addr);
#endif
#ifdef PANIC_PTRLEAK
            lkl_host_ops.panic();
#endif
         }
      }
      //fuzz_is_ptr_leak(addr);
   }
}

static int fuzz_resource_read(void *data, int offset, void *res, int size) {
   if(fuzz_is_active()) {
      fuzz_get_n((uint8_t*)res, size);
   }
   return 0;
}

static int fuzz_resource_write(void *data, int offset, void *res, int size)
{
   uint64_t val = 0x0;
   switch (size) {
      case 8:
         val = *(uint64_t *)res;
         break;
      case 4:
         val = *(uint32_t *)res;
         break;
      case 2:
         val = *(uint16_t *)res;
         break;
      case 1:
         val = *(uint8_t *)res;
         break;
      default:
         break;
   }
   fuzz_is_ptr_leak(val);
   return 0;
}

static const struct lkl_iomem_ops fuzz_resource_ops = {
	.read = fuzz_resource_read,
	.write = fuzz_resource_write,
};

static void *fuzz_resource_alloc(unsigned long resource_size, int resource_index)
{
	void *mmio_addr;
   mmio_addr = lkl_host_ops.mem_alloc(resource_size);
	return register_iomem(mmio_addr, resource_size, &fuzz_resource_ops);
}

static struct lkl_fuzz_ops fops = {
	.set_dev_data = fuzz_set_dev_data,
	.resource_alloc = fuzz_resource_alloc,
	.notify_irq_free = fuzz_notify_irq_free,
	.notify_irq_request = fuzz_notify_irq_request,
	.notify_devnode_add = fuzz_notify_devnode_add,
	.notify_devnode_remove = fuzz_notify_devnode_remove,
	.free_irq = fuzz_free_irq,
	.get_free_irq = fuzz_get_free_irq,
	.is_active = fuzz_is_active,
	.get_n = fuzz_get_n,
	.get_rem = fuzz_get_rem,
	.delay_work = fuzz_delay_work,
	.delay_task = fuzz_delay_task,
	.delay_sirq = fuzz_delay_sirq,
	.minimize_delays = fuzz_minimize_delays,
	.minimize_wq_delays = fuzz_minimize_wq_delays,
	.minimize_timeouts = fuzz_minimize_timeouts,
	.minimize_timebefore = fuzz_minimize_timebefore,
	.minimize_timeafter = fuzz_minimize_timeafter,
	.apply_hacks = fuzz_apply_hacks,
	.apply_patch = fuzz_apply_patch,
	.apply_patch_2 = fuzz_apply_patch_2,
   .store_dma_handle = fuzz_store_dma_handle,
   .remove_dma_handle = fuzz_remove_dma_handle,
   .get_dma_handle = fuzz_get_dma_handle,
	.nosan_memset = __asan_unsanitized_memset,
	.nosan_memcpy = __asan_unsanitized_memcpy,
   .add_waiter = fuzz_add_waiter,
   .del_waiter = fuzz_del_waiter,
   .trigger_wait_cond = fuzz_trigger_wait_cond,
   .trigger_conc_cond = fuzz_trigger_conc_cond,
   .scan_for_ptr_leaks = fuzz_scan_for_ptr_leaks,
   .is_ptr_leak = fuzz_is_ptr_leak,
   .do_trace = fuzz_do_trace,
};

void lkl_fuzz_set_debug_options(struct fuzz_debug_options *dopts) {
   __asan_unsanitized_memcpy(&fb.dopts, dopts, sizeof(*dopts));
}

void lkl_fuzz_set_stop_work_pending(int set)
{
  fb.stop_work_pending = set;
  if(set) {
     fuzz_trigger_conc_cond();
  }
}

void lkl_fuzz_set_targeted_irqs(int set)
{
  fb.targeted_irqs = set;
}

int lkl_fuzz_get_fast_irqs(void) {
   return fb.fast_irqs;
}

void lkl_fuzz_set_fast_irqs(int set)
{
  fb.fast_irqs = set;
}

void lkl_fuzz_set_use_bst(int set)
{
  fb.use_bst = set;
}

void lkl_fuzz_set_done_callback(lkl_fp_done_callback cb)
{
  fb.done_cb = cb;
}

void lkl_fuzz_set_delay_work(unsigned long delay)
{
  fb.work_delay = delay;
}

void lkl_fuzz_set_delay_task(unsigned long delay)
{
  fb.task_delay = delay;
}

void lkl_fuzz_set_apply_hacks(int set)
{
  fb.apply_hacks = set;
}

void lkl_fuzz_set_apply_patch(int set)
{
  fb.apply_patch = set;
}

void lkl_fuzz_set_apply_patch_2(int set)
{
  fb.apply_patch_2 = set;
}

void lkl_fuzz_set_delay_sirq(unsigned long delay, int no)
{
  if(fb.n_sirq_delays >= MAX_SIRQD) {
    lkl_printf("ERROR: max configurable sirq delays reached\n");
    return;
  }
  fb.sirq_delays[fb.n_sirq_delays] = delay;
  fb.sirq_nos[fb.n_sirq_delays] = no;
  fb.n_sirq_delays++;
}

void lkl_fuzz_set_minimize_delays(int set)
{
  fb.minimize_delays = set;
}

void lkl_fuzz_set_minimize_wq_delays(int set)
{
  fb.minimize_wq_delays = set;
}

void lkl_fuzz_set_minimize_timeouts(int set)
{
  fb.minimize_timeouts = set;
}

void lkl_fuzz_set_minimize_timebefore(int set)
{
  fb.minimize_timebefore = set;
}

void lkl_fuzz_set_minimize_timeafter(int set)
{
  fb.minimize_timeafter = set;
}

long lkl_fuzz_get_n(void *dst, unsigned long n) {
  return fuzz_get_n(dst, n);
}

int __attribute__((weak)) get_waits_pending(void) { return 1; }
int lkl_fuzz_wait_for_wait(unsigned long ns_wait_min, unsigned long ns_wait_max) {
  struct timespec sleepValue = {0};
  unsigned long r;
  unsigned long ns_wait = ns_wait_max - ns_wait_min;

  // for random irqs
  if(fb.targeted_irqs == 0) {
     pthread_mutex_lock(&fb.wait_started_mutex);
     r = ns_wait_min + ((rand() % ns_wait));
     sleepValue.tv_nsec = r;
     if(fuzz_do_trace(LKL_FUZZ_TRACE_WAITERS)) {
        lkl_printf("%s start wait random ns: %ld (%ld-%ld)\n", __FUNCTION__, sleepValue.tv_nsec, ns_wait_min, ns_wait_max);
     }
     nanosleep(&sleepValue, NULL);
     pthread_mutex_unlock(&fb.wait_started_mutex);
     return 1;
  }
  pthread_mutex_lock(&fb.wait_started_mutex);
  // WARNING: do not do syscalls here, that will lead to deadlocks
  // because trigger_* is also called from syscall paths and
  // the cpu semaphore will block
  while(ht_items()==0) {
    if(fuzz_do_trace(LKL_FUZZ_TRACE_WAITERS)) {
       lkl_printf("%s start wait #waiters: %d\n", __FUNCTION__, ht_items());
    }
    //pthread_cond_wait_timedwait(&fb.wait_started_cond, &fb.wait_started_mutex, &timeToWait);
    pthread_cond_wait(&fb.wait_started_cond, &fb.wait_started_mutex);
    if(fuzz_do_trace(LKL_FUZZ_TRACE_WAITERS)) {
       lkl_printf("%s end wait #waiters: %d\n", __FUNCTION__, ht_items());
    }
  }
  pthread_mutex_unlock(&fb.wait_started_mutex);
  return 1;
}

// Note(feli): turned out to be not useful for current setup
// might be reactivated when analyzing single irq
int get_works_pending(void);
int lkl_fuzz_wait_for_conc(unsigned long ns_wait) {
  return 0;
#if 0
  struct timespec timeToWait;
  struct timeval now;
  int ret = 0;
  gettimeofday(&now,NULL);
  timeToWait.tv_sec = now.tv_sec;
  timeToWait.tv_nsec = now.tv_usec * 1000UL + ns_wait;
  return 0;
  if(fb.targeted_irqs) {
    return ret;
  }
  if(fb.stop_work_pending) {
    return ret;
  }
  pthread_mutex_lock(&fb.conc_ended_mutex);
  while(!fb.targeted_irqs && !fb.stop_work_pending && get_works_pending()) {
    //lkl_printf("%s:%d %d\n", __FUNCTION__, __LINE__, get_works_pending());
    ret = pthread_cond_timedwait(&fb.conc_ended_cond, &fb.conc_ended_mutex, &timeToWait);
  }
  pthread_mutex_unlock(&fb.conc_ended_mutex);
  return ret;
#endif
}

int lkl_fuzz_set_buf(void *buf_ptr, unsigned long buf_len)
{
  int res = 0;
  lkl_host_ops.mutex_lock(fb.mu);
  if(fuzz_is_active() != 0) {
    lkl_printf("error last buffer still active\n");
    res = -1;
    goto out;
  }
  __atomic_store_n(&fb.buf.idx, 0, __ATOMIC_SEQ_CST);
  fb.done_cb_called = 0;
  fb.buf.size = buf_len;
  fb.buf.data = buf_ptr;
  fb.stop_work_pending = 0;
  fb.n_devnode = 0;
#ifdef FUZZ_TEST_PRNG
  fb.prng_buf_idx = 0;
#endif
  raninit(&fb.buf.x, 0x42);
  raninit(&fb.buf.x_mod, 0x23);
  fuzz_reset_irqs();
  __atomic_store_n(&fb.buf.active, 1, __ATOMIC_SEQ_CST);
out:
  lkl_host_ops.mutex_unlock(fb.mu);
  return res;
}

void lkl_fuzz_unset_buf(void)
{
#ifdef FUZZ_TEST_PRNG
  lkl_printf("TESTING PRNG...\n");
  sanitizer_prng_buf_test((uint8_t*)&fb.prng_buf, fb.prng_buf_idx);
#endif
  lkl_host_ops.mutex_lock(fb.mu);
  __atomic_store_n(&fb.buf.active, 0, __ATOMIC_SEQ_CST);
  if(fb.use_bst) {
     clearTree(fb.dma_bst_root);
     fb.dma_bst_root = NULL;
  }
  lkl_host_ops.mutex_unlock(fb.mu);
  //fuzz_trigger_wait_cond();
  //fuzz_trigger_conc_cond();
  if(fb.dev_reset != 0) {
    fb.dev_reset(fb.dev_handle);
  }
  if(ht_items()) {
     lkl_printf("Warning still %d waiters left\n", ht_items());
  }
}

void lkl_fuzz_print_config(void) {
   lkl_printf("DONE CB:        %llx\n", (uint64_t)fb.done_cb);
   lkl_printf("WORK DELAY:     %d\n", fb.work_delay);
   lkl_printf("TASK DELAY:     %d\n", fb.task_delay);
   lkl_printf("MIN DELAY:      %d\n", fb.minimize_delays);
   lkl_printf("MIN WQ DELAY:   %d\n", fb.minimize_wq_delays);
   lkl_printf("MIN TIMEOUTS:   %d\n", fb.minimize_timeouts);
   lkl_printf("MIN TIMEAFTER:  %d\n", fb.minimize_timeafter);
   lkl_printf("MIN TIMEBEFORE: %d\n", fb.minimize_timebefore);
   lkl_printf("APPLY HACKS:    %d\n", fb.apply_hacks);
   lkl_printf("APPLY PATCH:    %d\n", fb.apply_patch);
   lkl_printf("APPLY PATCH2:   %d\n", fb.apply_patch_2);
   lkl_printf("USE BST:        %d\n", fb.use_bst);
   lkl_printf("TARGET IRQ:     %d\n", fb.targeted_irqs);
   lkl_printf("FAST IRQ:       %d\n", fb.fast_irqs);
}

int lkl_fuzz_init_fuzzer(void) {
	fb.buf.data = 0;
	fb.buf.size = 0;
	__atomic_store_n(&fb.buf.idx, 0, __ATOMIC_SEQ_CST);
	fb.mu = lkl_host_ops.mutex_alloc(0);
	fb.dev_handle = NULL;
	fb.dev_reset = NULL;
	fb.done_cb = NULL;
	fb.done_cb_called = 0;
	fb.work_delay = 0;
	fb.task_delay = 0;
	fb.n_sirq_delays = 0;
	fb.minimize_delays = 0;
	fb.minimize_wq_delays = 0;
	fb.minimize_timeouts = 0;
	fb.minimize_timebefore = 0;
	fb.minimize_timeafter = 0;
	fb.apply_hacks = 0;
	fb.apply_patch = 0;
	fb.apply_patch_2 = 0;
   fb.stop_work_pending = 0;
   fb.targeted_irqs = 0;
   fb.fast_irqs = 0;
   fb.n_devnode = 0;
   lkl_host_ops.mutex_lock(fb.mu);
   fb.dma_bst_root = NULL;
   fb.use_bst = 0;
   lkl_host_ops.mutex_unlock(fb.mu);
	fuzz_reset_irqs();
   pthread_mutex_init(&fb.wait_started_mutex, NULL);
   pthread_cond_init(&fb.wait_started_cond, NULL);
   pthread_mutex_init(&fb.conc_ended_mutex, NULL);
   pthread_cond_init(&fb.conc_ended_cond, NULL);
	lkl_host_ops.fuzz_ops = &fops;
	lkl_printf("fuzzer initialized\n");
	return 0;
}
