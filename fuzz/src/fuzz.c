#include "ch9.h"
#include <dlfcn.h>
#include <lkl_host.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>

// test with lkl and libfuzzer

typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;

struct list_head {
  void *next, *prev;
};
struct usb_host_endpoint {
  struct usb_endpoint_descriptor desc;
  struct usb_ss_ep_comp_descriptor ss_ep_comp;
  struct usb_ssp_isoc_ep_comp_descriptor ssp_isoc_ep_comp;
  struct list_head urb_list;
  void *hcpriv;
  struct ep_device *ep_dev; /* For sysfs info */

  unsigned char *extra; /* Extra descriptors */
  int extralen;
  int enabled;
  int streams;
};

struct usb2_lpm_parameters {
  unsigned int besl;
  int timeout;
};

struct usb3_lpm_parameters {
  unsigned int mel;
  unsigned int pel;
  unsigned int sel;
  int timeout;
};

struct usb_device {
  int devnum;
  char devpath[16];
  u32 route;
  int state;
  int speed;
  unsigned int rx_lanes;
  unsigned int tx_lanes;

  void *tt;
  int ttport;

  unsigned int toggle[2];

  void *parent;
  void *bus;
  struct usb_host_endpoint ep0;

  char reserved[512];
  //   struct device dev;
  //
  //   struct usb_device_descriptor descriptor;
  //   void *bos;
  //   void *config;
  //
  //   void *actconfig;
  //   void *ep_in[16];
  //   void *ep_out[16];
  //
  //   char **rawdescriptors;
  //
  //   unsigned short bus_mA;
  //   u8 portnum;
  //   u8 level;
  //   u8 devaddr;
  //
  //   unsigned can_submit:1;
  //   unsigned persist_enabled:1;
  //   unsigned have_langid:1;
  //   unsigned authorized:1;
  //   unsigned authenticated:1;
  //   unsigned wusb:1;
  //   unsigned lpm_capable:1;
  //   unsigned usb2_hw_lpm_capable:1;
  //   unsigned usb2_hw_lpm_besl_capable:1;
  //   unsigned usb2_hw_lpm_enabled:1;
  //   unsigned usb2_hw_lpm_allowed:1;
  //   unsigned usb3_lpm_u1_enabled:1;
  //   unsigned usb3_lpm_u2_enabled:1;
  //   int string_langid;
  //
  //   /* static strings from the device */
  //   char *product;
  //   char *manufacturer;
  //   char *serial;
  //
  //   struct list_head filelist;
  //
  //   int maxchild;
  //
  //   u32 quirks;
  //   int urbnum;
  //
  //   unsigned long active_duration;
  //
  //#ifdef CONFIG_PM
  //   unsigned long connect_time;
  //
  //   unsigned do_remote_wakeup:1;
  //   unsigned reset_resume:1;
  //   unsigned port_is_suspended:1;
  //#endif
  //   void *wusb_dev;
  //   int slot_id;
  //   int removable;
  //   struct usb2_lpm_parameters l1_params;
  //   struct usb3_lpm_parameters u1_params;
  //   struct usb3_lpm_parameters u2_params;
  //   unsigned lpm_disable_count;
  //
  //   u16 hub_delay;
};

struct usb_interface {
  /* array of alternate settings for this interface,
   * stored in no particular order */
  void *altsetting;

  void *cur_altsetting;    /* the currently
                            * active alternate setting */
  unsigned num_altsetting; /* number of alternate settings */

  /* If there is an interface association descriptor then it will list
   * the associated interfaces */
  void *intf_assoc;

  int minor;                        /* minor number this interface is
                                     * bound to */
  unsigned int condition;           /* state of binding */
  unsigned sysfs_files_created : 1; /* the sysfs attributes exist */
  unsigned ep_devs_created : 1;     /* endpoint "devices" exist */
  unsigned unregistering : 1;       /* unregistration is in progress */
  unsigned needs_remote_wakeup : 1; /* driver requires remote wakeup */
  unsigned needs_altsetting0 : 1;   /* switch to altsetting 0 is pending */
  unsigned needs_binding : 1;       /* needs delayed unbind/rebind */
  unsigned resetting_device : 1;    /* true: bandwidth alloc after reset */
  unsigned authorized : 1;          /* used for interface authorization */

  unsigned long reseved[512];
  // struct device dev;      /* interface specific device info */
  // void *usb_dev;
  // struct work_struct reset_ws;  /* for resets in atomic context */
};

struct usb_device_id {
  /* which fields to match against? */
  unsigned short match_flags;

  /* Used for product specific matches; range is inclusive */
  unsigned short idVendor;
  unsigned short idProduct;
  unsigned short bcdDevice_lo;
  unsigned short bcdDevice_hi;

  /* Used for device class matches */
  unsigned char bDeviceClass;
  unsigned char bDeviceSubClass;
  unsigned char bDeviceProtocol;

  /* Used for interface class matches */
  unsigned char bInterfaceClass;
  unsigned char bInterfaceSubClass;
  unsigned char bInterfaceProtocol;

  /* Used for vendor-specific interface matches */
  unsigned char bInterfaceNumber;

  /* not matched against */
  unsigned long driver_info;
};

typedef int (*fp_rtl8192_usb_probe)(void *intf, void *id);
typedef unsigned int (*fp_x)(void *dev, const uint8_t *pstats);
static fp_x x = NULL;
static fp_rtl8192_usb_probe rtl8192_usb_probe = NULL;



static bool DoInitialization() {
  printf("Start_kernel\n");
  lkl_start_kernel(&lkl_host_ops, "mem=16M loglevel=8");
  printf("===============DONE starting kernel============\n");
  /* void *handle = dlopen("./targets/rtl8192u.so", RTLD_LAZY | RTLD_GLOBAL | RTLD_DEEPBIND); */
  void *handle = dlopen("./nokaslr.so", RTLD_LAZY);
  if (!handle) {
    printf("error dlopen: %s\n", dlerror());
    exit(1);
  }
  rtl8192_usb_probe = dlsym(handle, "rtl8192_usb_probe");
  if (!rtl8192_usb_probe) {
    printf("error dlsym: %s\n", dlerror());
    exit(1);
  }
  printf("=====Driver has been initialized!\n");
  return true;
}
static bool init = false;

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  if (!init) {
    if (!DoInitialization()) {
      return 0;
    }
    init = true;
  }
  if (!rtl8192_usb_probe)
    printf("x not init\n");
  else {
    struct usb_device_id id;
    struct usb_interface intf;
    rtl8192_usb_probe(&intf, &id);
  }
  return 0;
}

#ifndef FUZZ
int main(){
  DoInitialization();
}
#endif
