#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/syscalls.h>
#include <linux/pci.h>
#include <linux/msi.h>
#include <linux/printk.h>
#include <linux/interrupt.h>
#include <linux/types.h>
#include <linux/io.h>
#include <asm/dma-mapping.h>
#include <linux/scatterlist.h>
#include <linux/mm.h>
#include <uapi/asm/fuzz.h>
#include <asm/host_ops.h>
#include "fuzz.h"
#include "fuzz_dma.h"

#define MAX_CAPS 512
typedef struct generic_cap {
  uint8_t cap_id;
  uint8_t next;
  uint16_t ctrl;
  uint8_t buf[128];
} __attribute__ ((packed)) generic_cap;
#define CAPPOS_TO_OFFSET(cp) ((cp * sizeof(struct generic_cap) + sizeof(struct fuzz_pci_config)))

typedef struct lkl_fuzz_pci_dev {
  struct fuzz_pci_config conf;
  generic_cap caps[MAX_CAPS];
} __attribute__ ((packed)) lkl_fuzz_pci_dev;


static int lkl_fuzz_pci_generic_read(struct pci_bus *bus, unsigned int devfn,
    int where, int size, u32 *val)
{
  lkl_fuzz_pci_dev *fuzz_dev = (lkl_fuzz_pci_dev*)bus->sysdata;
  if(devfn == 0) {
    if(lkl_ops->fuzz_ops->is_active()) {
      lkl_ops->fuzz_ops->get_n((uint8_t*)val, size);
      return PCIBIOS_SUCCESSFUL;
    }
    if(where+size<=sizeof(fuzz_dev->conf)+sizeof(fuzz_dev->caps)) {
      lkl_ops->fuzz_ops->nosan_memcpy(val, ((void*)&fuzz_dev->conf) + where, size);
      if(lkl_ops->fuzz_ops->do_trace(FUZZ_TRACE_CONF)) {
         pr_info("%s: at %x, val %x\n", __FUNCTION__, where, *val);
      }
      return PCIBIOS_SUCCESSFUL;
    }
  }
  return PCIBIOS_FUNC_NOT_SUPPORTED;
}

static int lkl_fuzz_pci_generic_write(struct pci_bus *bus, unsigned int devfn,
    int where, int size, u32 val)
{
  lkl_ops->fuzz_ops->is_ptr_leak(val);
  if (devfn == 0) {
     if(lkl_ops->fuzz_ops->do_trace(FUZZ_TRACE_CONF)) {
        pr_info("%s: at %x, val %x\n", __FUNCTION__, where, val);
     }
     return PCIBIOS_SUCCESSFUL;
  }
  return PCIBIOS_FUNC_NOT_SUPPORTED;
}

static void *fuzz_resource_alloc(struct lkl_fuzz_pci_dev *dev,
    unsigned long resource_size,
    int resource_index,
    uint32_t flags)
{
  return lkl_ops->fuzz_ops->resource_alloc(resource_size, resource_index);
}


static int lkl_pci_override_resource(struct pci_dev *dev, void *data)
{
  int i;
  struct resource *r;
  resource_size_t start, size;
  void *remapped_start = NULL;
  lkl_fuzz_pci_dev *fuzz_dev = (lkl_fuzz_pci_dev*)dev->sysdata;

  if (dev->devfn != 0)
    return 0;

  for (i = 0; i < PCI_NUM_RESOURCES; i++) {
    r = &dev->resource[i];

    if (!r->parent && r->start && r->flags) {
      dev_info(&dev->dev, "claiming resource %s/%d\n",
	  pci_name(dev), i);
      if (pci_claim_resource(dev, i)) {
	dev_err(&dev->dev,
	    "Could not claim resource %s/%d!",
	    pci_name(dev), i);
      }

      size = pci_resource_len(dev, i);

      if (pci_resource_flags(dev, i) & IORESOURCE_MEM) {
	remapped_start =
	  fuzz_resource_alloc(
	      fuzz_dev, size, i, pci_resource_flags(dev, i));
      }

      if (remapped_start) {
	/* override values */
	start = (resource_size_t)remapped_start;
	pci_resource_start(dev, i) = start;
	pci_resource_end(dev, i) = start + size - 1;
      } else {
	/*
	 * A host library or the application could
	 * not handle the resource. Disable it
	 * not to be touched by drivers.
	 */
	pci_resource_flags(dev, i) |=
	  IORESOURCE_DISABLED;
      }
    }
  }

  dev->irq = lkl_get_free_irq("fuzz_pci_0");
  pr_info("Got IRQ %d\n", dev->irq);
  return 0;
}

static void fuzz_msi_teardown_irq(struct msi_controller *chip,
    unsigned int irq)
{
  struct msi_desc *msi;
  lkl_fuzz_pci_dev *fuzz_dev;
  msi = irq_get_msi_desc(irq);
  if(msi == NULL) {
    pr_err("%s: Error failed to get msi desc for irq %d\n", __FUNCTION__, irq);
    return;
  }
  fuzz_dev = (lkl_fuzz_pci_dev*)msi_desc_to_pci_sysdata(msi);
  if(lkl_ops->fuzz_ops->free_irq(irq) != 0) {
    pr_err("%s: Error failed to free irq %d\n", __FUNCTION__, irq);
    return;
  }
}

static int fuzz_msi_setup_irq(struct msi_controller *chip,
    struct pci_dev *pdev,
    struct msi_desc *desc)
{
  int irq;
  irq = lkl_ops->fuzz_ops->get_free_irq();
  if(irq<=0) {
    pr_err("%s: Error could not get irq\n", __FUNCTION__);
    return -EINVAL;
  }
  irq_set_msi_desc(irq, desc);
  if(lkl_ops->fuzz_ops->do_trace(FUZZ_TRACE_MSIIRQ)) {
     pr_err("%s: set msi irq %d\n", __FUNCTION__, irq);
  }
  return 0;
}


struct msi_controller fuzz_msi_controller = {
  .setup_irq = fuzz_msi_setup_irq,
  .teardown_irq = fuzz_msi_teardown_irq,
};
EXPORT_SYMBOL(fuzz_msi_controller);


static struct pci_ops lkl_pci_root_ops = {
  .read = lkl_fuzz_pci_generic_read,
  .write = lkl_fuzz_pci_generic_write,
};

static int find_next_empty_cap(lkl_fuzz_pci_dev *fuzz_dev) {
  int cap_pos;
  generic_cap *cap;
  for(cap_pos = 0; cap_pos < MAX_CAPS; cap_pos++) {
      cap = (generic_cap*)&fuzz_dev->caps[cap_pos];
      if(cap->cap_id==0) {
	return cap_pos;
      }
  }
  return -1;
}

static int add_cap(lkl_fuzz_pci_dev *fuzz_dev, uint8_t cap_id) {
  int cap_pos;
  cap_pos = find_next_empty_cap(fuzz_dev);
  if(cap_pos < 0) {
    pr_err("Error out of cap space\n");
    return -1;
  }
  pr_err("%s: pos %d, offset %x, cap %x\n", __FUNCTION__, cap_pos, CAPPOS_TO_OFFSET(cap_pos), cap_id);
  fuzz_dev->caps[cap_pos].cap_id = cap_id;
  fuzz_dev->caps[cap_pos].next = 0xff;
  if(cap_pos>0) {
    fuzz_dev->caps[cap_pos-1].next = CAPPOS_TO_OFFSET(cap_pos);
  }
  return 0;
}

struct pci_dev *do_fuzz_configure_pci(struct fuzz_pci_dev_config *conf) {
  int i;
  struct pci_bus *bus;
  struct pci_dev *dev;
  struct resource *rom_res;
  lkl_fuzz_pci_dev *fuzz_dev = kzalloc(sizeof(lkl_fuzz_pci_dev), GFP_KERNEL);
  memcpy(&fuzz_dev->conf, &conf->conf, sizeof(fuzz_dev->conf));
  fuzz_dev->conf.header_type = PCI_HEADER_TYPE_NORMAL;
  // enable capabilities
  // capability space starts after config
  fuzz_dev->conf.cap_list = sizeof(struct fuzz_pci_config);
  fuzz_dev->conf.status |= PCI_STATUS_CAP_LIST;
  lkl_ops->fuzz_ops->nosan_memset((void*)&fuzz_dev->caps, 0, sizeof(fuzz_dev->caps));
  if(add_cap(fuzz_dev, PCI_CAP_ID_MSI) != 0) {
    pr_err("Error failed to add PCI_CAP_ID_MSI\n");
    return NULL;
  }
  if(add_cap(fuzz_dev, PCI_CAP_ID_MSIX) != 0) {
    pr_err("Error failed to add PCI_CAP_ID_MSIX\n");
    return NULL;
  }

  bus = pci_scan_bus(0, &lkl_pci_root_ops, (void *)fuzz_dev);
  if (!bus) {
    pr_err("pci_scan_bus failed\n");
    return NULL;
  }
  dev = pci_scan_single_device(bus, 0); // Add & populate device
  if(dev==NULL) {
    pr_err("pci_scan_sigle_device failed\n");
    return NULL;
  }
  if(dev->sysdata!=fuzz_dev) {
    pr_err("pci_scan_sigle_device failed\n");
    return NULL;
  }
  dev->dev.init_name = "FUZZDEV";
  dev->dev.dma_ops = &lkl_fuzz_dma_ops;
  if(conf->n_mmio > MAX_FUZZ_MMIO) {
    pr_err("error n_mmio %d > %d\n", conf->n_mmio, MAX_FUZZ_MMIO);
    return NULL;
  }
  for(i=0; i<conf->n_mmio; i++) {
    dev->resource[i].flags = conf->mmio_regions[i].flags;
    dev->resource[i].start = conf->mmio_regions[i].start;
    dev->resource[i].end   = conf->mmio_regions[i].end;
    dev->resource[i].parent = NULL; //mmio_parent;
  }

  // Note(feli) some devs need this
  rom_res = &dev->resource[PCI_ROM_RESOURCE];
  //dev->rom_base_reg = 0xdeadbeef;
  rom_res->flags = IORESOURCE_MEM | IORESOURCE_PREFETCH |
    IORESOURCE_READONLY | IORESOURCE_SIZEALIGN;

  pci_walk_bus(bus, lkl_pci_override_resource, NULL);
  pci_bus_add_devices(bus);
  return dev;
}

static void fuzz_pci_reset_dev(void *dev_handle) {
  struct pci_dev *dev;
  dev = (struct pci_dev*) dev_handle;
  pci_disable_msi(dev);
}

long fuzz_configure_pci(struct fuzz_pci_dev_config __user *uconf)
{
  struct pci_dev *dev;
  struct fuzz_pci_dev_config conf;
  int ret = copy_from_user(&conf, uconf, sizeof(struct fuzz_pci_dev_config));
  if(ret != 0) {
    pr_err("%s: copy_from_user failed\n", __FUNCTION__);
    return -EINVAL;
  }
  dev = do_fuzz_configure_pci(&conf);
  lkl_ops->fuzz_ops->set_dev_data((void*)dev, fuzz_pci_reset_dev, FDEV_TYPE_PCI, dev->irq);
  set_fuzz_dma(conf.fuzz_dma);
  return (long)dev;
}
