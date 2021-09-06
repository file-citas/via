#include <stdbool.h>
#include <fcntl.h>
#include <dlfcn.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/mman.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <linux/ethtool.h>
#include <net/if.h>
#include <pthread.h>
#include <libconfig.h>
#include <lkl.h>
#include <lkl_host.h>
#include "util.h"
#include "fuzz_fw.h"

mod_fuzz_func_fp mod_fuzz_func;
mod_init_func_fp mod_init_func;
//static size_t hwm_limit = 10000000; // should match -max_len option
static size_t hwm_limit = 100000000; // should match -max_len option
uint64_t fuzz_dev_handle = 0;
//int fuzz_irq = 0;
int loglevel;
int devtype;
int nqueues;
void *this_module = NULL;
void *module_handle;
char moddeps_path[MAXDEPS][512];
char module_path[512];
char interface[512];
char *interface_ptr = NULL;
int interface_idx = 0;
size_t moddep_count = 0;
size_t fws_count = 0;
size_t barsizes_count = 0;
size_t barflags_count = 0;
uint8_t *io_init = NULL;
size_t io_init_size = 0;
int VID = 0;
int DID = 0;
int SVID = 0;
int SDID = 0;
int barsizes[LKL_MAX_FUZZ_MMIO];
int barflags[LKL_MAX_FUZZ_MMIO];
int revision = 0;
int pci_class = 0;
char plt_name[256];
int fuzz_dma = 0;
int drain_irqs = 0;
int extra_io = 0;
//int delay_work;
//int delay_task;
int minimize_delay = 0;
int minimize_wq_delay= 0;
int minimize_timeout = 0;
int minimize_timebefore= 0;
int minimize_timeafter = 0;
int apply_hacks = 0;
int apply_patch = 0;
int apply_patch_2 = 0;
int target_irq = 0;
int fast_irq = 0;
long ns_wait_min = 1;
long ns_wait_max = 1000;
int mtu = 0;
int do_update_hwm = 0;
int do_use_bst = 0;
int do_use_done_cb = 0;
long long n_request_irqs;
unsigned long long features_set_mask = 0;
unsigned long long features_unset_mask = 0;
size_t vio_nofuzz_count = 0;
int vio_nofuzz[LKL_MAX_VIO_NOFUZZ];
int aga_eval = 0;
static int virtio_irqs=0;
static int pci_irqs=1;
// debug options
struct fuzz_debug_options dopts;

int do_config(char *fuzz_target) {
   config_t cfg, *cf;
   const config_setting_t *moddep_set;
   const config_setting_t *fws_set;
   const config_setting_t *barsizes_set;
   const config_setting_t *barflags_set;
   const config_setting_t *vio_nofuzz_set;
   void *harness_handle;
   const char *loglevel_str;
   const char *updatehwm_str;
   const char *usebst_str;
   const char *usedone_cb_str;
   const char *tmp_harness_name;
   const char *tmp_module_name;
   const char *tmp_plt_name;
   const char *tmp_interface;
   const char *tmp_env;
   const char *moddeps_tmp[MAXDEPS];
   const char *tmp_fw_name;
   long long features_set_mask_low;
   long long features_set_mask_high;
   long long features_unset_mask_low;
   long long features_unset_mask_high;
   char *harness_dir;
   char *config_dir;
   char *module_dir;
   char *fw_dir;
   char harness_path[256];
   char config_path[256];
   char fw_path[256];
   size_t i;
   int res = 0;

   ///////////////////////////////////////////////////////////////////////////
   // ENV DIRECTORIES ////////////////////////////////////////////////////////
   ///////////////////////////////////////////////////////////////////////////
   harness_dir = getenv("VIA_HARNESS");
   if(!harness_dir) {
      fprintf(stderr, "need VIA_HARNESS\n");
      res = -1;
      goto out;
   }

   config_dir = getenv("VIA_CONFIGS");
   if(!config_dir) {
      fprintf(stderr, "need VIA_CONFIGS\n");
      res = -1;
      goto out;
   }

   module_dir = getenv("VIA_MODULES");
   if(!module_dir) {
      fprintf(stderr, "need VIA_MODULES\n");
      res = -1;
      goto out;
   }

   fw_dir = getenv("VIA_FW");
   if(!fw_dir) {
      fprintf(stderr, "need VIA_FW\n");
      res = -1;
      goto out;
   }

   snprintf(config_path, sizeof(config_path), "%s/%s.config", config_dir, fuzz_target);

   cf = &cfg;
   config_init(cf);

   if (!config_read_file(cf, config_path)) {
      fprintf(stderr, "Error parsing config %s %s:%d - %s\n",
            config_path,
            config_error_file(cf),
            config_error_line(cf),
            config_error_text(cf));
      res = -1;
      goto out;
   }

   ///////////////////////////////////////////////////////////////////////////
   // GLOBAL /////////////////////////////////////////////////////////////////
   ///////////////////////////////////////////////////////////////////////////
   // required: harness
   tmp_harness_name = getenv("TARGET_HARNESS");
   if (tmp_harness_name || config_lookup_string(cf, "harness", &tmp_harness_name)) {
      snprintf(harness_path, sizeof(harness_path), "%s/%s", harness_dir, tmp_harness_name);
      fprintf(stderr, "harness: %s\n", harness_path);
      harness_handle = dlopen(harness_path, RTLD_GLOBAL | RTLD_NOW);
      if (!harness_handle) {
         fprintf(stderr, "Error: %s\n", dlerror());
         res = -1;
         goto out;
      }
      mod_fuzz_func = dlsym(harness_handle, "mod_fuzz");
      if(!mod_fuzz_func) {
         fprintf(stderr, "Error resolving mod_fuzz function in %s: %s\n", harness_path, dlerror());
         res = -1;
         goto out;
      }
      // can be zero
      mod_init_func = dlsym(harness_handle, "mod_init");
   } else {
      fprintf(stderr, "Error: No fuzzing harness lib specified (set 'harness')\n");
      res = -1;
      goto out;
   }

   // required: target module
   if (config_lookup_string(cf, "module", &tmp_module_name)) {
   } else {
      fprintf(stderr, "Failed to get tmp_module_name\n");
      res = -1;
      goto out;
   }
   snprintf(module_path, sizeof(module_path), "%s/%s", module_dir, tmp_module_name);
   fprintf(stderr, "module: %s\n", module_path);

   // optional: target module dependencies
   moddep_set = config_lookup(cf, "moddeps");
   if(moddep_set) {
      moddep_count = config_setting_length(moddep_set);
      if(moddep_count >= MAXDEPS-1) {
         fprintf(stderr, "Error too many deps %ld (max %d)\n", moddep_count, MAXDEPS);
         res = -1;
         goto out;
      }
      fprintf(stderr, "#deps: %ld\n", moddep_count);
      for (i=0; i < moddep_count; i++) {
         moddeps_tmp[i] = config_setting_get_string_elem(moddep_set, i);
         snprintf(moddeps_path[i], sizeof(moddeps[i]), "%s/%s", module_dir, moddeps_tmp[i]);
         fprintf(stderr, "\t#%ld. %s\n", i, moddeps_path[i]);
      }
   }

   // optional: firmware dependencies
   fws_set = config_lookup(cf, "fws");
   if(fws_set) {
      fws_count = config_setting_length(fws_set);
      if(fws_count >= MAX_FW-1) {
         fprintf(stderr, "Error too many firmwares %ld (max %d)\n", fws_count, MAX_FW);
         res = -1;
         goto out;
      }
      fprintf(stderr, "#fws: %ld\n", fws_count);
      for (i=0; i < fws_count; i++) {
         tmp_fw_name = config_setting_get_string_elem(fws_set, i);
         snprintf(fw_path, sizeof(fw_path), "%s/%s", fw_dir, tmp_fw_name);
         res = fuzz_add_fw(fw_path);
         if(res != 0) {
            fprintf(stderr, "\t#%ld. %s\n", i, fw_path);
            goto out;
         }
      }
   }

   // optional: memory resources (mmio and pio)
   // resource sizes
   barsizes_set = config_lookup(cf, "barsizes");
   if(barsizes_set) {
      barsizes_count = config_setting_length(barsizes_set);
      if(barsizes_count > LKL_MAX_FUZZ_MMIO) {
         fprintf(stderr, "Error too many bars %ld (max %d)\n", barsizes_count, LKL_MAX_FUZZ_MMIO);
         res = -1;
         goto out;
      }
      fprintf(stderr, "#barsizes: %ld\n", barsizes_count);
      for (i=0; i < barsizes_count; i++) {
         barsizes[i] = config_setting_get_int_elem(barsizes_set, i);
      }
   }
   // resource flags
   barflags_set = config_lookup(cf, "barflags");
   if(barflags_set) {
      barflags_count = config_setting_length(barflags_set);
      if(barflags_count > LKL_MAX_FUZZ_MMIO) {
         fprintf(stderr, "Error too many bars %ld (max %d)\n", barflags_count, LKL_MAX_FUZZ_MMIO);
         res = -1;
         goto out;
      }
      fprintf(stderr, "#barflags: %ld\n", barflags_count);
      for (i=0; i < barflags_count; i++) {
         barflags[i] = config_setting_get_int_elem(barflags_set, i);
      }
   }
   if(barflags_count != barsizes_count) {
      fprintf(stderr, "#barsizes != #barflags: %ld/%ld\n", barsizes_count, barflags_count);
      res = -1;
      goto out;
   }
   // add default
   if(barsizes_count == 0) {
      barsizes_count = 1;
      barflags_count = 1;
      barsizes[0] = 0x1fffff;
      barflags[0] = 0x40200;
   }

   // required: device type (PCI, VIRTIO, PLATFORM)
   if (config_lookup_int(cf, "devtype", &devtype)) {
      if(devtype == LKL_FDEV_TYPE_PCI) {
         fprintf(stderr, "devtype: %x (PCI)\n", devtype);
      } else if(devtype == LKL_FDEV_TYPE_PLATFORM) {
         fprintf(stderr, "devtype: %x (PLATFORM)\n", devtype);
      } else if(devtype == LKL_FDEV_TYPE_VIRTIO) {
         fprintf(stderr, "devtype: %x (VIRTIO)\n", devtype);
      } else if(devtype == LKL_FDEV_TYPE_ACPI) {
         fprintf(stderr, "devtype: %x (ACPI)\n", devtype);
      } else {
         fprintf(stderr, "Error: unknown device type %d\n", devtype);
         return -1;
      }
   } else {
      fprintf(stderr, "Failed to get devtype\n");
      return -1;
   }

   // optional: kernel loglevel (overwritten by env)
   loglevel_str = getenv("LOGLEVEL");
   if(loglevel_str && strlen(loglevel_str)==1){
      loglevel = atoi(loglevel_str);
   } else {
      if(config_lookup_int(cf, "loglevel", &loglevel)) {
      } else {
         loglevel=0;
      }
   }
   fprintf(stderr, "loglevel: %d\n", loglevel);

   // EXECUTION CONTROL PARAMETERS ///////////////////////////////////////////
   // optional: use targeted irq injection (overwritten via env)
   tmp_env = getenv("TARGET_IRQS");
   if(tmp_env && strlen(tmp_env)==1){
      target_irq = atoi(tmp_env);
   } else {
      if (config_lookup_int(cf, "target_irq", &target_irq)) {
      } else {
         target_irq = 0;
      }
   }
   fprintf(stderr, "target_irq: %x\n", target_irq);

   // optional: trigger one irq immediatly after request (overwritten via env)
   tmp_env = getenv("FAST_IRQS");
   if(tmp_env && strlen(tmp_env)==1){
      fast_irq = atoi(tmp_env);
   } else {
      if (config_lookup_int(cf, "fast_irq", &fast_irq)) {
      } else {
         fast_irq = 0;
      }
   }
   fprintf(stderr, "fast_irq: %x\n", fast_irq);


   if (config_lookup_int(cf, "ns_wait_min", (int*)&ns_wait_min)) {
   } else {
      ns_wait_min = 1;
   }
   fprintf(stderr, "ns_wait_min: %ld\n", ns_wait_min);

   if (config_lookup_int(cf, "ns_wait_max", (int*)&ns_wait_max)) {
   } else {
      ns_wait_max = 1000;
   }
   fprintf(stderr, "ns_wait_max: %ld\n", ns_wait_max);

   // optional: update high water mark to adjust io-stream size
   // according to lenght of input consumed by module
   updatehwm_str = getenv("UPDATEHWM");
   if(updatehwm_str && strlen(updatehwm_str)==1){
      do_update_hwm = atoi(updatehwm_str);
   } else {
      if (config_lookup_int(cf, "do_update_hwm", &do_update_hwm)) {
      } else {
         do_update_hwm = 1;
      }
   }
   fprintf(stderr, "do_update_hwm: %x\n", do_update_hwm);

   // optional: call done callback after input is used up
   usedone_cb_str = getenv("USEDONECB");
   if(usedone_cb_str && strlen(usedone_cb_str)==1){
      do_use_done_cb = atoi(usedone_cb_str);
   } else {
      if (config_lookup_int(cf, "use_done_cb", &do_use_done_cb)) {
      } else {
         do_use_done_cb = 0;
      }
   }
   fprintf(stderr, "use_done_cb: %x\n", do_use_done_cb);

   // optional: keep track of dma mappings for drivers that
   // pass device controlled dma addresses to dma_unmap_*, etc.
   usebst_str = getenv("USEBST");
   if(usebst_str && strlen(usebst_str)==1){
      do_use_bst = atoi(usebst_str);
   } else {
      if (config_lookup_int(cf, "use_bst", &do_use_bst)) {
      } else {
         do_use_bst = 0;
      }
   }
   fprintf(stderr, "use_bst: %x\n", do_use_bst);

   // optional: request specific number of irqs per fuzzing interation
   if (config_lookup_int64(cf, "n_request_irqs", &n_request_irqs)) {
   } else {
      n_request_irqs = -1;
   }
   fprintf(stderr, "n_request_irqs: %llx\n", n_request_irqs);

   // optional: fuzz dma data (should be disabled for virtio simulation mode)
   if (config_lookup_int(cf, "fuzz_dma", &fuzz_dma)) {
   } else {
      fuzz_dma = 0;
   }
   fprintf(stderr, "fuzz_dma: %x\n", fuzz_dma);

   // SPECIAL AGAMOTTO EVAL OPTIONS //////////////////////////////////////////
   tmp_env = getenv("EVAL_AGAMOTTO"); // emulate agamotto userspace behaviour
   if(tmp_env && strlen(tmp_env)==1) {
      aga_eval = atoi(tmp_env);
   } else {
      aga_eval = 0;
   }

   // DEBUG OPTIONS //////////////////////////////////////////////////////////
   tmp_env = getenv("FUZZ_TRACE_MOD"); // module (un)init
   if(tmp_env && strlen(tmp_env)==1) {
      dopts.mod = atoi(tmp_env);
   } else {
      dopts.mod = 0;
   }
   tmp_env = getenv("FUZZ_TRACE_IO"); // io-stream
   if(tmp_env && strlen(tmp_env)==1) {
      dopts.io = atoi(tmp_env);
   } else {
      dopts.io = 0;
   }
   tmp_env = getenv("FUZZ_TRACE_IRQ"); // requested irq lines
   if(tmp_env && strlen(tmp_env)==1) {
      dopts.irq = atoi(tmp_env);
   } else {
      dopts.irq = 0;
   }
   tmp_env = getenv("FUZZ_TRACE_MSIIRQ"); // requested msi irq lines
   if(tmp_env && strlen(tmp_env)==1) {
      dopts.msiirq = atoi(tmp_env);
   } else {
      dopts.msiirq = 0;
   }
   tmp_env = getenv("FUZZ_TRACE_DMA"); // requested dma allocations
   if(tmp_env && strlen(tmp_env)==1) {
      dopts.dma = atoi(tmp_env);
   } else {
      dopts.dma = 0;
   }
   tmp_env = getenv("FUZZ_TRACE_DMAINJ"); // coherent dma injection
   if(tmp_env && strlen(tmp_env)==1) {
      dopts.dmainj = atoi(tmp_env);
   } else {
      dopts.dmainj = 0;
   }
   tmp_env = getenv("FUZZ_TRACE_CONF"); // pci device config
   if(tmp_env && strlen(tmp_env)==1) {
      dopts.conf = atoi(tmp_env);
   } else {
      dopts.conf = 0;
   }
   tmp_env = getenv("FUZZ_TRACE_WAITERS"); // waiter tracking
   if(tmp_env && strlen(tmp_env)==1) {
      dopts.waiters = atoi(tmp_env);
   } else {
      dopts.waiters = 0;
   }
   tmp_env = getenv("FUZZ_TRACE_BST"); // dma map verification
   if(tmp_env && strlen(tmp_env)==1) {
      dopts.bst = atoi(tmp_env);
   } else {
      dopts.bst = 0;
   }
   tmp_env = getenv("FUZZ_TRACE_DONECB"); // callback on empty io-stream
   if(tmp_env && strlen(tmp_env)==1) {
      dopts.donecb = atoi(tmp_env);
   } else {
      dopts.donecb = 0;
   }
   tmp_env = getenv("FUZZ_TRACE_DEVNODES"); // requested device nodes
   if(tmp_env && strlen(tmp_env)==1) {
      dopts.devnodes = atoi(tmp_env);
   } else {
      dopts.devnodes = 0;
   }
   tmp_env = getenv("FUZZ_TRACE_PTRLEAKS"); // requested device nodes
   if(tmp_env && strlen(tmp_env)==1) {
      dopts.ptrleaks = atoi(tmp_env);
   } else {
      dopts.ptrleaks = 0;
   }
   tmp_env = getenv("FUZZ_TRACE_DMACTRL"); // requested device nodes
   if(tmp_env && strlen(tmp_env)==1) {
      dopts.dmactrl = atoi(tmp_env);
   } else {
      dopts.dmactrl = 0;
   }

   // DELAY OPTIMIZATIONS ////////////////////////////////////////////////////
   // optional: minimize delays (overwritten via env)
   tmp_env = getenv("MIN_ALL_DELAY");
   if(tmp_env && strlen(tmp_env)==1){
      minimize_delay = minimize_wq_delay = minimize_timeout = minimize_timebefore = minimize_timeafter = atoi(tmp_env);
   } else {
      if (config_lookup_int(cf, "minimize_delay", &minimize_delay)) {
      } else {
         minimize_delay = 0;
      }
      if (config_lookup_int(cf, "minimize_wq_delay", &minimize_wq_delay)) {
      } else {
         minimize_wq_delay = 0;
      }

      if (config_lookup_int(cf, "minimize_timeout", &minimize_timeout)) {
      } else {
         minimize_timeout = 0;
      }

      if (config_lookup_int(cf, "minimize_timebefore", &minimize_timebefore)) {
      } else {
         minimize_timebefore = 0;
      }

      if (config_lookup_int(cf, "minimize_timeafter", &minimize_timeafter)) {
      } else {
         minimize_timeafter = 0;
      }
   }
   fprintf(stderr, "minimize_delay: %x\n", minimize_delay);
   fprintf(stderr, "minimize_wq_delay: %x\n", minimize_wq_delay);
   fprintf(stderr, "minimize_timeout: %x\n", minimize_timeout);
   fprintf(stderr, "minimize_timebefore: %x\n", minimize_timebefore);
   fprintf(stderr, "minimize_timeafter: %x\n", minimize_timeafter);

   // CODE MODIFICATION //////////////////////////////////////////////////////
   // optional: apply patches to assertions, deadlocks and unbounded allocations
   tmp_env = getenv("PATCH");
   if(tmp_env && strlen(tmp_env)==1){
      apply_patch = atoi(tmp_env);
   } else {
      if (config_lookup_int(cf, "apply_patch", &apply_patch)) {
      } else {
         apply_patch = 0;
      }
   }
   fprintf(stderr, "apply_patch: %x\n", apply_patch);

   // optional: apply patches for remaining bugs (if available)
   tmp_env = getenv("PATCH2");
   if(tmp_env && strlen(tmp_env)==1){
      apply_patch_2 = atoi(tmp_env);
   } else {
      if (config_lookup_int(cf, "apply_patch_2", &apply_patch_2)) {
      } else {
         apply_patch_2 = 0;
      }
   }
   fprintf(stderr, "apply_patch_2: %x\n", apply_patch_2);

   // optional: apply fuzzing optimizations
   tmp_env = getenv("HACKS");
   if(tmp_env && strlen(tmp_env)==1){
      apply_hacks = atoi(tmp_env);
   } else {
      if (config_lookup_int(cf, "apply_hacks", &apply_hacks)) {
      } else {
         apply_hacks = 0;
      }
   }
   fprintf(stderr, "apply_hacks: %x\n", apply_hacks);


   ///////////////////////////////////////////////////////////////////////////
   // NETWORK SPECIFIC ///////////////////////////////////////////////////////
   ///////////////////////////////////////////////////////////////////////////
   // optional: network interface
   if (config_lookup_string(cf, "interface", &tmp_interface)) {
      strncpy(interface, tmp_interface, 512);
      interface_ptr = interface;
      fprintf(stderr, "interface: %s\n", tmp_interface);
   } else {
      interface_ptr = NULL;
   }

   // optional: network interface index
   if (config_lookup_int(cf, "ifindex", &interface_idx)) {
      fprintf(stderr, "ifindex: %x\n", interface_idx);
   } else {
      interface_idx = 0;
   }

   // optional: transfer unit size fo network drivers
   if (config_lookup_int(cf, "mtu", &mtu)) {
      fprintf(stderr, "mtu: %x\n", mtu);
   } else {
      mtu = -1;
   }

   ///////////////////////////////////////////////////////////////////////////
   // DEVICE SPECIFIC ////////////////////////////////////////////////////////
   ///////////////////////////////////////////////////////////////////////////
   if(devtype == LKL_FDEV_TYPE_PCI) {
      // required: pci vendor id
      if (config_lookup_int(cf, "vid", &VID)) {
         fprintf(stderr, "vendor id: %x\n", VID);
      } else {
         fprintf(stderr, "Failed to get vid\n");
         res = -1;
         goto out;
      }

      // required: pci device id
      if (config_lookup_int(cf, "did", &DID)) {
         fprintf(stderr, "device id: %x\n", DID);
      } else {
         fprintf(stderr, "Failed to get did\n");
         res = -1;
         goto out;
      }

      // optional: pci class
      if (config_lookup_int(cf, "pci_class", &pci_class)) {
         fprintf(stderr, "pci_class: %x\n", pci_class);
      } else {
         pci_class = 0x02;
      }

      // optional: pci revision
      if (config_lookup_int(cf, "revision", &revision)) {
         fprintf(stderr, "revision: %x\n", revision);
      } else {
         revision = 0x0;
      }

      // optional: pci sub vendor id
      if (config_lookup_int(cf, "svid", &SVID)) {
         fprintf(stderr, "sub vendor id: %x\n", SVID);
      } else {
         SVID = 0;
      }

      // optional: pci sub device id
      if (config_lookup_int(cf, "sdid", &SDID)) {
         fprintf(stderr, "sub device id: %x\n", SDID);
      } else {
         SDID = 0;
      }

   } else if(devtype == LKL_FDEV_TYPE_VIRTIO) {
      // required: pci vendor id
      if (config_lookup_int(cf, "vid", &VID)) {
         fprintf(stderr, "vendor id: %x\n", VID);
      } else {
         fprintf(stderr, "Failed to get vid\n");
         res = -1;
         goto out;
      }

      // required: pci device id
      if (config_lookup_int(cf, "did", &DID)) {
         fprintf(stderr, "device id: %x\n", DID);
      } else {
         fprintf(stderr, "Failed to get did\n");
         res = -1;
         goto out;
      }

      // optional: do not fuzz these offsets read from mmio
      vio_nofuzz_set = config_lookup(cf, "vio_nofuzz");
      if(vio_nofuzz_set) {
         vio_nofuzz_count = config_setting_length(vio_nofuzz_set);
         if(vio_nofuzz_count >= LKL_MAX_VIO_NOFUZZ-1) {
            fprintf(stderr, "Error too many vio nofuzz offsets %ld (max %d)\n", vio_nofuzz_count, LKL_MAX_VIO_NOFUZZ);
            res = -1;
            goto out;
         }
         fprintf(stderr, "#vio_nofuzz: %ld\n", vio_nofuzz_count);
         for (i=0; i < vio_nofuzz_count; i++) {
            vio_nofuzz[i] = config_setting_get_int_elem(vio_nofuzz_set, i);
            fprintf(stderr, "\t#%ld: %d\n", i, vio_nofuzz[i]);
         }
      }

      // optional: mask features that have to be enabled (only for virtio)
      if (config_lookup_int64(cf, "features_set_mask_low", (long long*)&features_set_mask_low)) {
         features_set_mask = ((unsigned long long)features_set_mask_low)& 0xffffffff;
         fprintf(stderr, "features_set_mask: %llx\n", features_set_mask);
      } else {
         features_set_mask = 0;
      }
      if (config_lookup_int64(cf, "features_set_mask_high", (long long*)&features_set_mask_high)) {
         features_set_mask |= (unsigned long long)(features_set_mask_high<<32UL);
         fprintf(stderr, "features_set_mask: %llx\n", features_set_mask);
      }

      // optional: mask features that have to be disabled (only for virtio)
      if (config_lookup_int64(cf, "features_unset_mask_low", (long long*)&features_unset_mask_low)) {
         features_unset_mask = ((unsigned long long)features_unset_mask_low)& 0xffffffff;
         fprintf(stderr, "features_unset_mask: %llx\n", features_unset_mask);
      } else {
         features_unset_mask = 0;
      }
      if (config_lookup_int64(cf, "features_unset_mask_high", (long long*)&features_unset_mask_high)) {
         features_unset_mask |= (unsigned long long)(features_unset_mask_high<<32UL);
         fprintf(stderr, "features_unset_mask: %llx\n", features_unset_mask);
      }

      // optional: drain irqs after io-stream end (only for virtio simulation)
      if (config_lookup_int(cf, "drain_irqs", &drain_irqs)) {
      } else {
         drain_irqs = 0;
      }
      fprintf(stderr, "drain_irqs: %x\n", drain_irqs);

      // optional: extend io after io-stream end (only for virtio simulation)
      if (config_lookup_int(cf, "extra_io", &extra_io)) {
      } else {
         extra_io = 0;
      }
      fprintf(stderr, "extra_io: %x\n", extra_io);

      // optional: number of virtio queues
      if (config_lookup_int(cf, "nqueues", &nqueues)) {
         fprintf(stderr, "nqueues: %x\n", nqueues);
      } else {
         fprintf(stderr, "Warning: no nqueues specified for virtio device\n");
         nqueues = 0;
      }

      // optional: use virtio irq trigger (send to queue)
      if (config_lookup_int(cf, "virtio_irqs", &virtio_irqs)) {
         fprintf(stderr, "virtio_irqs: %x\n", virtio_irqs);
      } else {
         fprintf(stderr, "Warning: no virtio_irqs specified for virtio device\n");
         virtio_irqs = 0;
      }

      // optional: use pci irq trigger (normal irqs)
      if (config_lookup_int(cf, "pci_irqs", &pci_irqs)) {
         fprintf(stderr, "pci_irqs: %x\n", pci_irqs);
      } else {
         fprintf(stderr, "Warning: no pci_irqs specified for pci device\n");
         pci_irqs = 1;
      }

   } else if(devtype == LKL_FDEV_TYPE_PLATFORM) {
      // required: platform device name
      if (config_lookup_string(cf, "plt_name", &tmp_plt_name)) {
         fprintf(stderr, "platform dev name: %s\n", tmp_plt_name);
         strncpy(plt_name, tmp_plt_name, 256);
      }

   }

out:
   config_destroy(&cfg);
   return res;
}

void crash(void) {
   int *ptr = NULL;
   *ptr = 1;
}

void sanitizer_high_watermark(uint32_t m);
void update_hwm(void) {
   if(do_update_hwm) {
      long hwm;
      hwm = lkl_fuzz_get_idx();
      if(hwm < hwm_limit && hwm > 0) {
         sanitizer_high_watermark(hwm);
      } else if(hwm > 0) {
         sanitizer_high_watermark(hwm_limit-1);
         fprintf(stderr, "Warning: HWM exceeds limit %ld/%ld\n", hwm, hwm_limit);
      }
   }
}

////////////////////////////////////////////////////////////////////////////////
// IRQS ////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

static void send_to_queue_virtio(void) {
   for(int qidx=0; qidx<nqueues; qidx++) {
      //lkl_dbg_printf("Send to Queue %d\n", qidx);
      lkl_virtio_send_to_queue(fuzz_dev_handle, qidx);
   }
}

static void trigger_irqs_virtio(void) {
   //lkl_dbg_printf("Trigger IRQ Virtio\n");
   lkl_virtio_trigger_irq();
}

static size_t irqs_triggered = 0;
static void trigger_irqs_pci(void) {
   long n_irq;
   int irqs[512];
   struct timespec sleepValue = {0};
   sleepValue.tv_nsec = 100;
   n_irq = lkl_fuzz_get_requested_irqs(irqs);
   for(long i=0; i<n_irq; i++) {
      lkl_dbg_printf("Trigger IRQ %d\n", irqs[i]);
      lkl_sys_fuzz_trigger_irq(irqs[i]);
      irqs_triggered++;
   }
   sched_yield();
   //nanosleep(&sleepValue, NULL);
}

void trigger_irqs(void) {
   if(devtype == LKL_FDEV_TYPE_VIRTIO) {
      //trigger_irqs_pci();
      //trigger_irqs_virtio();
      if(virtio_irqs==1 && pci_irqs==0) {
         send_to_queue_virtio();
      } else if(virtio_irqs==0 && pci_irqs==1) {
         trigger_irqs_virtio();
      } else {
         send_to_queue_virtio();
         trigger_irqs_virtio();
      }
   } else {
      trigger_irqs_pci();
   }
}

void trigger_one_irq(void) {
   if(irqs_triggered == 0) {
      trigger_irqs_pci();
   //} else {
   //   lkl_dbg_printf("IRQs already triggered (%d)\n", irqs_triggered);
   }
}

static int random_irq_cb(void *arg) {
   if(lkl_fuzz_wait_for_wait(ns_wait_min, ns_wait_max)) {
      trigger_irqs();
      return 1;
   }
   return 0;
}

static int default_irq_cb(void *arg) {
   if(lkl_fuzz_has_waiters() || lkl_fuzz_wait_for_wait(ns_wait_min, ns_wait_max)) {
      trigger_irqs();
      return 1;
   }
   return 0;
}

static pthread_t irq_tid;
static bool do_stop_irqthread = false;
static pthread_mutex_t need_irq_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t need_irq_cond = PTHREAD_COND_INITIALIZER;
static int need_irq = 0;

static void* irqthread_cb(void *arg)
{
   fp_irq_cb cb = (fp_irq_cb)arg;
   if(cb==NULL) {
      fprintf(stderr, "%s error no callback\n", __FUNCTION__);
   }
   while(!do_stop_irqthread) {
      pthread_mutex_lock(&need_irq_mutex);
      while(need_irq==0 && !do_stop_irqthread) {
         pthread_cond_wait(&need_irq_cond, &need_irq_mutex);
      }
      pthread_mutex_unlock(&need_irq_mutex);
      if(need_irq!=0) {
         need_irq -= cb(NULL);
      }
   }
   return NULL;
}

void start_irqthread_default(void) {
   lkl_dbg_printf("%s\n", __FUNCTION__);
   if(n_request_irqs==0) return;
   if(target_irq!=0) {
      lkl_printf("%s using targeted irq injection\n", __FUNCTION__);
      pthread_create(&irq_tid, NULL, &irqthread_cb, default_irq_cb);
   } else {
      lkl_printf("%s using random irq injection\n", __FUNCTION__);
      pthread_create(&irq_tid, NULL, &irqthread_cb, random_irq_cb);
   }
}
void start_irqthread_cb(fp_irq_cb cb) {
   lkl_printf("%s\n", __FUNCTION__);
   pthread_create(&irq_tid, NULL, &irqthread_cb, cb);
}

void stop_irqthread(void) {
   do_stop_irqthread = true;
   pthread_cond_signal(&need_irq_cond);
}

void request_irqs(size_t n) {
   if(n==0) {
      return;
   }
   pthread_mutex_lock(&need_irq_mutex);
   need_irq = n;
   pthread_cond_signal(&need_irq_cond);
   pthread_mutex_unlock(&need_irq_mutex);
}

void cancel_irqs() {
   need_irq = 0;
}

////////////////////////////////////////////////////////////////////////////////
// DONE CALLBACK ///////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
static pthread_mutex_t unload_module_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t unload_module_cond = PTHREAD_COND_INITIALIZER;
static pthread_t done_tid;
static int do_unload_module = 0; // start module unload
static int mod_loaded = 0;    // module was loaded
static int done_cb(void) {
   //lkl_dbg_printf("Done cb called\n");
   pthread_mutex_lock(&unload_module_mutex);
   do_unload_module = 1; // start module unload
   pthread_cond_signal(&unload_module_cond); // signal unload thread
   pthread_mutex_unlock(&unload_module_mutex);
   return 0;
}

static void* thread_done_cb(void *arg)
{
   int err;
   while(true) {
      pthread_mutex_lock(&unload_module_mutex);
      while(!do_unload_module) {
         pthread_cond_wait(&unload_module_cond, &unload_module_mutex);
      }
      pthread_mutex_unlock(&unload_module_mutex);
      if(__atomic_exchange_n(&mod_loaded, 0, __ATOMIC_SEQ_CST)) {
         do {
            lkl_dbg_printf("Trying to stop module (cb)\n");
            err = lkl_sys_uninit_loaded_module(this_module);
            //if(err!=0 && target_irq==1) {
            if(err!=0) {
               trigger_irqs();
            }
         } while(err);
         lkl_dbg_printf("Stopped module (cb)\n");
      }
      //lkl_dbg_printf("Module already stopped (cb)\n");
      do_unload_module = 0;
   }
   return NULL;
}

void start_thread_done_default(void) {
   if(do_use_done_cb) {
      lkl_fuzz_set_done_callback(done_cb);
      pthread_create(&done_tid, NULL, &thread_done_cb, NULL);
   }
}

static void wait_for_module_unloaded(void) {
   while(do_unload_module) {
      struct timespec sleepValue = {0};
      sleepValue.tv_nsec = 100;
      nanosleep(&sleepValue, NULL);
   }
}

static char eth_auto_data[0x4000];
static int eth_auto_stream_sock;
static struct lkl_ifreq eth_auto_ifr;
int eth_auto_init(void) {
  strcpy(eth_auto_ifr.lkl_ifr_name, interface_ptr);
  eth_auto_ifr.lkl_ifr_mtu = 1024;
  eth_auto_ifr.ifr_hwaddr.sa_data[0] = 0xDE;
  eth_auto_ifr.ifr_hwaddr.sa_data[1] = 0xAD;
  eth_auto_ifr.ifr_hwaddr.sa_data[2] = 0xBE;
  eth_auto_ifr.ifr_hwaddr.sa_data[3] = 0xEF;
  eth_auto_ifr.ifr_hwaddr.sa_data[4] = 0xCA;
  eth_auto_ifr.ifr_hwaddr.sa_data[5] = 0xFE;
  eth_auto_ifr.ifr_hwaddr.sa_family = 1;
  eth_auto_stream_sock = lkl_sys_socket(AF_INET, SOCK_STREAM, 0);
  if (eth_auto_stream_sock < 0) {
     fprintf(stderr, "Failed to create STREAM socket: %d\n", eth_auto_stream_sock);
     return -1;
  }
  return 0;
}
static void eth_auto(void) {
    struct ethtool_cmd  *cmd = (struct ethtool_cmd  *)eth_auto_data;
    struct ethtool_link_settings *link_ksettings = (struct ethtool_link_settings*)eth_auto_data;
    cmd->cmd = ETHTOOL_GSET;
    eth_auto_ifr.ifr_data = (void *)cmd;
    link_ksettings->link_mode_masks_nwords = 3;
    lkl_sys_ioctl(eth_auto_stream_sock, LKL_SIOCETHCUSTOM, (long)&eth_auto_ifr);
}

int init_module() {
   int err;
   err = lkl_sys_init_loaded_module(this_module);
   if(err==0) {
      __atomic_store_n (&mod_loaded, 1, __ATOMIC_SEQ_CST);
      lkl_dbg_printf("Module initialized\n");
      if(interface_ptr!=NULL && aga_eval != 0 && fuzz_last_probe_status==0) {
         eth_auto();
      }
   } else {
      lkl_dbg_printf("Module init failed %d\n", err);
   }

   return err;
}

int uninit_module() {
   int err = 0;
   if(__atomic_exchange_n(&mod_loaded, 0, __ATOMIC_SEQ_CST)) {
      do {
         lkl_dbg_printf("Trying to stop module\n");
         err = lkl_sys_uninit_loaded_module(this_module);
         //if(err!=0 && target_irq==1) {
         if(err!=0) {
            trigger_irqs();
         }
      } while(err!=0);
      lkl_dbg_printf("Stopped module\n");
   }
   wait_for_module_unloaded();
   return err;
}

////////////////////////////////////////////////////////////////////////////////
// FUZZ ITERATION //////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
void start_fuzz(const uint8_t *data, size_t size) {
   request_irqs(n_request_irqs);
   lkl_fuzz_set_buf((uint8_t*)data, size);
}

void end_fuzz(void) {
   cancel_irqs();
   irqs_triggered = 0;
   lkl_fuzz_unset_buf();
}

////////////////////////////////////////////////////////////////////////////////
// LKL /////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
uint64_t do_lkl_init(void) {
   int err, i=0;
   long handle;
   struct lkl_fuzz_pci_dev_config pci_conf;
   struct lkl_fuzz_platform_dev_config platform_conf;
   struct lkl_fuzz_acpi_dev_config acpi_conf;
   struct lkl_fuzz_virtio_dev_config virtio_conf;
   char k_cmdline[256];

   // configure fuzzer engine
   lkl_fuzz_init_fuzzer();
   lkl_fuzz_set_use_bst(do_use_bst);
   lkl_fuzz_set_minimize_delays(minimize_delay);
   lkl_fuzz_set_minimize_wq_delays(minimize_wq_delay);
   lkl_fuzz_set_minimize_timeouts(minimize_timeout);
   lkl_fuzz_set_minimize_timebefore(minimize_timebefore);
   lkl_fuzz_set_minimize_timeafter(minimize_timeafter);
   lkl_fuzz_set_apply_hacks(apply_hacks);
   lkl_fuzz_set_apply_patch(apply_patch);
   lkl_fuzz_set_apply_patch_2(apply_patch_2);
   lkl_fuzz_set_targeted_irqs(target_irq);
   lkl_fuzz_set_fast_irqs(fast_irq);
   lkl_fuzz_set_debug_options(&dopts);

   // start lkl
   fprintf(stderr, "Init lkl ...\n");
   snprintf(k_cmdline, sizeof(k_cmdline), "mem=4096M noirqdebug loglevel=%d maxcpus=1", loglevel);
   lkl_start_kernel(&lkl_host_ops, k_cmdline);

   // configure device
   if(devtype == LKL_FDEV_TYPE_PCI) {
      uint64_t mmio_start = 0x10000; // arbitrary value
      memset(&pci_conf, 0, sizeof(pci_conf));
      pci_conf.conf.vendor_id = VID;
      pci_conf.conf.device_id = DID;
      pci_conf.conf.sub_vendor_id = SVID;
      pci_conf.conf.sub_id = SDID;
      pci_conf.conf.revision_id = revision;
      pci_conf.conf.class_device = pci_class;
      pci_conf.fuzz_dma = fuzz_dma;
      pci_conf.n_mmio = barsizes_count;
      for(i=0; i<pci_conf.n_mmio; i++) {
         platform_conf.mmio_regions[i].remapped = 0;
         pci_conf.mmio_regions[i].flags = barflags[i];
         pci_conf.mmio_regions[i].start = mmio_start;
         pci_conf.mmio_regions[i].end = mmio_start + barsizes[i];
         mmio_start = pci_conf.mmio_regions[i].end;
      }
      handle = lkl_sys_fuzz_configure_dev(LKL_FDEV_TYPE_PCI, &pci_conf);
   } else if (devtype == LKL_FDEV_TYPE_PLATFORM) {
      uint64_t mmio_start = 0x10000; // arbitrary value
      memset(&platform_conf, 0, sizeof(platform_conf));
      strncpy(platform_conf.name, plt_name, 256);
      platform_conf.fuzz_dma = fuzz_dma;
      platform_conf.n_mmio = barsizes_count;
      for(i=0; i<platform_conf.n_mmio; i++) {
         platform_conf.mmio_regions[i].remapped = 0;
         platform_conf.mmio_regions[i].flags = barflags[i];
         platform_conf.mmio_regions[i].start = mmio_start;
         platform_conf.mmio_regions[i].end = mmio_start + barsizes[i];
         mmio_start = platform_conf.mmio_regions[i].end;
      }
      handle = lkl_sys_fuzz_configure_dev(LKL_FDEV_TYPE_PLATFORM, &platform_conf);
   } else if (devtype == LKL_FDEV_TYPE_ACPI) {
      uint64_t mmio_start = 0x10000; // arbitrary value
      memset(&acpi_conf, 0, sizeof(acpi_conf));
      strncpy(acpi_conf.name, plt_name, 256);
      acpi_conf.n_mmio = barsizes_count;
      for(i=0; i<acpi_conf.n_mmio; i++) {
         acpi_conf.mmio_regions[i].remapped = 0;
         acpi_conf.mmio_regions[i].flags = barflags[i];
         acpi_conf.mmio_regions[i].start = mmio_start;
         acpi_conf.mmio_regions[i].end = mmio_start + barsizes[i];
         mmio_start = acpi_conf.mmio_regions[i].end;
      }
      handle = lkl_sys_fuzz_configure_dev(LKL_FDEV_TYPE_ACPI, &acpi_conf);
   } else if (devtype == LKL_FDEV_TYPE_VIRTIO) {
      memset(&virtio_conf, 0, sizeof(virtio_conf));
      virtio_conf.vendor_id = VID;
      virtio_conf.device_id = DID;
      virtio_conf.nqueues = nqueues;
      virtio_conf.num_max = 128;
      virtio_conf.fuzz_dma = fuzz_dma;
      virtio_conf.drain_irqs = drain_irqs;
      virtio_conf.extra_io = extra_io;
      virtio_conf.features_set_mask = features_set_mask;
      virtio_conf.features_unset_mask = features_unset_mask;
      virtio_conf.n_nofuzz = vio_nofuzz_count;
      for(i=0; i<virtio_conf.n_nofuzz; i++) {
         virtio_conf.nofuzz[i] = vio_nofuzz[i];
      }
      handle = lkl_add_virtio_fuzz_dev(&virtio_conf);
   } else {
      fprintf(stderr, "Error: unknown device type %d\n", devtype);
      return -1;
   }

   for(i=0; i<moddep_count; i++) {
      fprintf(stderr, "Loading mod dependency %s\n", moddeps_path[i]);
      void* dep_module_handle = dlopen(moddeps_path[i], RTLD_GLOBAL | RTLD_NOW);
      if (!dep_module_handle) {
         fprintf(stderr, "Error loading module dependency %s: %s\n", moddeps_path[i], dlerror());
         return -1;
      }
      void *this_module_dep = dlsym(dep_module_handle, "__this_module");
      if(!this_module_dep) {
         fprintf(stderr, "Error resolving __this_module for %s: %s\n", moddeps_path[i], dlerror());
         return -1;
      }
      err = lkl_sys_init_loaded_module(this_module_dep);
      if(err!=0) {
         fprintf(stderr, "Error initializing module dependency %s\n", moddeps_path[i]);
      }
   }

   fprintf(stderr, "open module library %s\n", module_path);
   void *module_handle = dlopen(module_path, RTLD_GLOBAL | RTLD_NOW);
   if (!module_handle) {
      fprintf(stderr, "Error loading module dependency %s: %s\n", module_path, dlerror());
      return -1;
   }
   this_module = dlsym(module_handle, "__this_module");
   if(!this_module) {
      fprintf(stderr, "Error resolving __this_module for %s: %s\n", module_path, dlerror());
      return -1;
   }
   return handle;
}


