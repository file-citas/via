#ifndef UTIL_H
#define UTIL_H
#define MAXDEPS 16

#include <lkl.h>
#include <lkl_host.h>

#define DRAIN_ATTEMPTS 100000
#define DRAIN_DELAY_NS 100
typedef int (*mod_fuzz_func_fp)(const uint8_t *, size_t);
extern mod_fuzz_func_fp mod_fuzz_func;
typedef int (*mod_init_func_fp)(void);
extern mod_init_func_fp mod_init_func;
extern uint64_t fuzz_dev_handle;
extern uint8_t *io_init;
extern size_t io_init_size;
extern void *this_module;
extern void *module_handle;
extern char moddeps[MAXDEPS][512];
extern char module_name[512];
extern char interface[512];
extern char *interface_ptr;
extern int interface_idx;
extern size_t moddep_count;
extern int VID;
extern int DID;
extern int SVID;
extern int SDID;
extern int barsize;
extern int revision;
extern int pci_class;
extern int nqueues;
extern int mtu;
extern long long n_request_irqs;
extern int devtype;
extern int count_syscalls;
extern int aga_eval;
extern int loglevel;

#define lkl_dbg_printf(fmt, ...) \
            do { if (loglevel != 0) lkl_printf( fmt, ##__VA_ARGS__); } while (0)

int eth_auto_init(void);
typedef int(*fp_irq_cb)(void *arg);
void start_irqthread_cb(fp_irq_cb cb);
void start_irqthread(void);
void stop_irqthread(void);
void request_irqs(size_t n);
void cancel_irqs(void);
int do_config(char *fuzz_target);
uint64_t do_lkl_init(void);
void update_hwm(void);
//int crash(const char *target, const uint8_t *data, size_t size);
void crash(void);

////////////////////////////////////////////////////////////////////////////////
// IRQS ////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
void trigger_one_irq(void);
void trigger_irqs(void);
void request_irqs(size_t n);
void cancel_irqs(void);
void start_irqthread_default(void);
void start_irqthread_cb(fp_irq_cb cb);

////////////////////////////////////////////////////////////////////////////////
// DONE CALLBACK ///////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
void start_thread_done_default(void);

////////////////////////////////////////////////////////////////////////////////
// FUZZ ITERATION //////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
void start_fuzz(const uint8_t *data, size_t size);
void end_fuzz(void);
int init_module();
int uninit_module();

#define MINORBITS 20
#define MINORMASK ((1U << MINORBITS) - 1)
#define MAJOR(dev)   ((unsigned int) ((dev) >> MINORBITS))
#define MINOR(dev)   ((unsigned int) ((dev) & MINORMASK))
static inline uint32_t new_encode_dev(unsigned int major, unsigned int minor)
{
  return (minor & 0xff) | (major << 8) | ((minor & ~0xff) << 12);
}

static int inline ifindex_to_name(int sock, struct lkl_ifreq *ifr, int ifindex)
{
   ifr->lkl_ifr_ifindex = ifindex;
   return lkl_sys_ioctl(sock, LKL_SIOCGIFNAME, (long)ifr);
}

#endif
