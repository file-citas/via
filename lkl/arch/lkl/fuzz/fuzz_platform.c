#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/syscalls.h>
#include <linux/pci.h>
#include <linux/msi.h>
#include <linux/printk.h>
#include <linux/interrupt.h>
#include <linux/types.h>
#include <linux/io.h>
#include <linux/dma-mapping.h>
#include <linux/dma-map-ops.h>
#include <linux/scatterlist.h>
#include <linux/mm.h>
#include <uapi/asm/fuzz.h>
#include <asm/host_ops.h>
#include <linux/platform_device.h>
#include "fuzz.h"
#include "fuzz_dma.h"

long fuzz_configure_platform(struct fuzz_platform_dev_config __user *uconf)
{
  int ret, i;
  size_t rsize;
  int irq;
  struct platform_device *pdev;
  struct fuzz_platform_dev_config conf;
  struct resource *res;
  size_t n_res;

  ret = copy_from_user(&conf, uconf, sizeof(struct fuzz_platform_dev_config));

  pdev = platform_device_alloc(conf.name, PLATFORM_DEVID_AUTO);
  if (!pdev) {
    pr_err("%s: Unable to device alloc for %s\n", __func__, conf.name);
    return -ENOMEM;
  }

  // one extra for irq
  n_res = conf.n_mmio + 1;
  res = kmalloc(sizeof(*res) * n_res, GFP_KERNEL);
  if(!res) {
    pr_err("%s: Unable to resource alloc for %s\n", __func__, conf.name);
    return -ENOMEM;
  }

  // config irq
  if(conf.irq > 0) {
    irq = conf.irq;
  } else {
    irq = lkl_get_free_irq("fuzz_plt_0");
  }
  res[0].start = irq;
  res[0].end = irq;
  res[0].flags = IORESOURCE_IRQ;

  for(i=0; i<conf.n_mmio; i++) {
    rsize = conf.mmio_regions[i].end - conf.mmio_regions[i].start;
    if(conf.mmio_regions[i].remapped) {
      pr_err("%s: iomem already remapped\n", __FUNCTION__);
      res[i+1].start = (resource_size_t)conf.mmio_regions[i].start;
    } else {
      pr_err("%s: remapping iomem\n", __FUNCTION__);
      res[i+1].start = (resource_size_t)lkl_ops->fuzz_ops->resource_alloc(rsize, i+1);
    }
    res[i+1].end = res[i+1].start + rsize - 1;
    res[i+1].flags = conf.mmio_regions[i].flags;
  }

  ret = platform_device_add_resources(pdev, res, n_res);
  if (ret) {
    pr_err("%s: Unable to resource add for %s\n", __func__, conf.name);
    goto exit_device_put;
  }

  ret = platform_device_add(pdev);

  if (ret < 0) {
    pr_err("%s: Unable to add %s\n", __func__, conf.name);
    goto exit_release_pdev;
  }

  pdev->dev.dma_ops = &lkl_fuzz_dma_ops;
  set_fuzz_dma(conf.fuzz_dma);

  lkl_ops->fuzz_ops->set_dev_data((void*)pdev, NULL, FDEV_TYPE_PCI, irq);
  return pdev->id;

exit_release_pdev:
  platform_device_del(pdev);
exit_device_put:
  platform_device_put(pdev);

  return ret;
}

