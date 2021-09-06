#ifndef _UAPIFUZZ_H
#define _UAPIFUZZ_H

#include <linux/types.h>

typedef int(*fp_done_callback)(void);

#define FUZZ_MAX_DEVT 16
struct fuzz_devnode {
   unsigned long devt;
   int type;
   int active;
};

struct fuzz_pci_config {
  uint16_t vendor_id;
  uint16_t device_id;
  uint16_t command;
  uint16_t status;
  uint8_t  revision_id;
  uint8_t  class_prog;
  uint16_t class_device;
  uint8_t  cache_line_size;
  uint8_t  latency_timer;
  uint8_t  header_type;
  uint8_t  bist;
  uint32_t bar_0;
  uint32_t bar_1;
  uint32_t bar_2;
  uint32_t bar_3;
  uint32_t bar_4;
  uint32_t bar_5;
  uint32_t cardbus_cis;
  uint16_t sub_vendor_id;
  uint16_t sub_id;
  uint32_t rom_address;
  uint8_t  cap_list;
  uint8_t  reserved_0[3];
  uint32_t reserved_1;
  uint8_t  int_line;
  uint8_t  int_pin;
  uint8_t  min_gnt;
  uint8_t  max_lat;
} __attribute__ ((packed));

struct fuzz_mmio_resource {
  uint32_t flags;
  uint64_t start;
  uint64_t end;
  uint32_t remapped;
  char name[64];
};

#define MAX_FUZZ_MMIO 6
struct fuzz_pci_dev_config {
    uint32_t size; /* userspace sets p->size = sizeof(struct xyzzy_params) */
    int fuzz_dma;
    struct fuzz_pci_config conf;
    unsigned int n_mmio;
    struct fuzz_mmio_resource mmio_regions[MAX_FUZZ_MMIO];
};

struct fuzz_platform_dev_config {
    uint32_t size; /* userspace sets p->size = sizeof(struct xyzzy_params) */
    int fuzz_dma;
    char name[256];
    int irq;
    unsigned int n_mmio;
    struct fuzz_mmio_resource mmio_regions[MAX_FUZZ_MMIO];
};

struct fuzz_acpi_dev_config {
    uint32_t size; /* userspace sets p->size = sizeof(struct xyzzy_params) */
    int fuzz_dma;
    char name[256];
    int irq;
    unsigned int n_mmio;
    struct fuzz_mmio_resource mmio_regions[MAX_FUZZ_MMIO];
};

#define MAX_VIO_NOFUZZ 32
struct fuzz_virtio_dev_config {
    int vendor_id;
    int device_id;
    int nqueues;
    int num_max;
    int fuzz_dma;
    int drain_irqs;
    int extra_io;
    int nofuzz[MAX_VIO_NOFUZZ];
    unsigned int n_nofuzz;
    unsigned long long features_set_mask;
    unsigned long long features_unset_mask;
};

#endif
