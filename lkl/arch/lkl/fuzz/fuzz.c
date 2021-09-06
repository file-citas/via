#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/syscalls.h>
#include <linux/pci.h>
#include <linux/printk.h>
#include <linux/interrupt.h>
#include <linux/types.h>
#include <linux/io.h>
#include <linux/dma-mapping.h>
#include <linux/scatterlist.h>
#include <linux/mm.h>
#include <uapi/asm/fuzz.h>
#include <asm/host_ops.h>
#include "fuzz.h"

static void kasan_test(void) {
   int i;
   int *ptr;
   ptr = kmalloc(sizeof(int)*16, GFP_KERNEL);
   for(i=0; i<17; i++) {
      pr_err("%s ptr[%d] = %d\n", __FUNCTION__, i, ptr[i]);
   }
}

SYSCALL_DEFINE2(fuzz_configure_dev, unsigned int, type, void __user *, uconf)
{
   switch(type) {
      case FDEV_TYPE_PCI:
         return fuzz_configure_pci((struct fuzz_pci_dev_config __user*)uconf);
      case FDEV_TYPE_PLATFORM:
         return fuzz_configure_platform((struct fuzz_platform_dev_config __user*)uconf);
      case FDEV_TYPE_ACPI:
         return fuzz_configure_acpi((struct fuzz_acpi_dev_config __user*)uconf);
      default:
         pr_err("Error: unknown device type %d running kasan test instead\n", type);
         kasan_test();
   }
   return -EINVAL;
}

//void run_irq(int irq);
SYSCALL_DEFINE1(fuzz_trigger_irq, int, irq)
{
	//run_irq(irq);
   //return 0;
   return lkl_trigger_irq(irq);
}
