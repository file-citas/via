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
#include <linux/acpi.h>
#include "fuzz.h"
#include "fuzz_dma.h"

int acpi_lapic;
int acpi_ioapic;
int acpi_noirq;
int acpi_strict;
int acpi_disabled;
int acpi_pci_disabled;
int acpi_skip_timer_override;
int acpi_use_timer_override;
int acpi_fix_pin2_polarity;
int acpi_disable_cmcff;
uint64_t acpi_fuzz_root_pointer;

int raw_pci_read(unsigned int domain, unsigned int bus, unsigned int devfn,
						int reg, int len, u32 *val)
{
  return 0;
}
int raw_pci_write(unsigned int domain, unsigned int bus, unsigned int devfn,
						int reg, int len, u32 val)
{
  return 0;
}
int acpi_register_gsi(struct device *dev, u32 gsi, int trigger, int polarity)
{
  return 0;
}
struct pci_bus *pci_acpi_scan_root(struct acpi_pci_root *root)
{
  return NULL;
}
int acpi_gsi_to_irq(u32 gsi, unsigned int *irqp)
{
  return 0;
}
void acpi_unregister_gsi(u32 gsi)
{
}
void __init __iomem *__acpi_map_table(unsigned long phys, unsigned long size)
{
  void *vaddr;
  vaddr = lkl_ops->mem_alloc(size);
  fuzz_add_c_dma_alloc(vaddr, size);
  return vaddr;
}

void __init __acpi_unmap_table(void __iomem *map, unsigned long size)
{
  fuzz_remove_c_dma_alloc(map, size, (dma_addr_t)map);
  lkl_ops->mem_free(map);
}

enum acpi_irq_model_id acpi_irq_model = 0;

static int __init acpi_parse_sbf(struct acpi_table_header *table) {
  return 0;
}

void __init acpi_boot_table_init(void)
{
	if (acpi_disabled)
		return;

	if (acpi_table_init()) {
		disable_acpi();
		return;
	}
	acpi_table_parse(ACPI_SIG_BOOT, acpi_parse_sbf);
}

int __init early_acpi_boot_init(void)
{
	if (acpi_disabled)
		return 1;
	return 0;
}

int __init acpi_boot_init(void)
{
	if (acpi_disabled)
		return 1;
	acpi_table_parse(ACPI_SIG_BOOT, acpi_parse_sbf);
	return 0;
}


int __acpi_acquire_global_lock(unsigned int *lock)
{
	unsigned int old, new, val;
	do {
		old = *lock;
		new = (((old & ~0x3) + 2) + ((old >> 1) & 0x1));
		val = cmpxchg(lock, old, new);
	} while (unlikely (val != old));
	return ((new & 0x3) < 3) ? -1 : 0;
}

int __acpi_release_global_lock(unsigned int *lock)
{
	unsigned int old, new, val;
	do {
		old = *lock;
		new = old & ~0x3;
		val = cmpxchg(lock, old, new);
	} while (unlikely (val != old));
	return old & 0x1;
}


int __init acpi_init(void);
int __init pnpacpi_init(void);
void fuzz_load_acpi(void) {
  acpi_disabled = 0;
  acpi_permanent_mmap = 0;
  acpi_boot_table_init();
  early_acpi_boot_init();
  acpi_boot_init();
  acpi_early_init();
  acpi_subsystem_init();
  arch_post_acpi_subsys_init();
  acpi_init();
  pnpacpi_init();
}
EXPORT_SYMBOL(fuzz_load_acpi);

long fuzz_configure_acpi(struct fuzz_acpi_dev_config __user *uconf)
{
  acpi_fuzz_root_pointer = 0xdeadbeef;
  set_fuzz_dma(1);
  return 0;
}

