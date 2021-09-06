#ifndef _ASM_LKL_ACPI_H
#define _ASM_LKL_ACPI_H
#ifdef CONFIG_ACPI

extern uint64_t acpi_fuzz_root_pointer;

static inline void arch_fix_phys_package_id(int num, u32 slot)
{
}
extern int acpi_lapic;
extern int acpi_ioapic;
extern int acpi_noirq;
extern int acpi_strict;
extern int acpi_disabled;
extern int acpi_pci_disabled;
extern int acpi_skip_timer_override;
extern int acpi_use_timer_override;
extern int acpi_fix_pin2_polarity;
extern int acpi_disable_cmcff;

extern u8 acpi_sci_flags;
extern u32 acpi_sci_override_gsi;
void acpi_pic_sci_set_trigger(unsigned int, u16);

struct device;

extern int (*__acpi_register_gsi)(struct device *dev, u32 gsi,
				  int trigger, int polarity);
extern void (*__acpi_unregister_gsi)(u32 gsi);

static inline void disable_acpi(void)
{
	acpi_disabled = 1;
	acpi_pci_disabled = 1;
	acpi_noirq = 1;
}

extern int acpi_gsi_to_irq(u32 gsi, unsigned int *irq);

static inline void acpi_noirq_set(void) { acpi_noirq = 1; }
static inline void acpi_disable_pci(void)
{
	acpi_pci_disabled = 1;
}

/* Low-level suspend routine. */
extern int (*acpi_suspend_lowlevel)(void);

/* Physical address to resume after wakeup */
unsigned long acpi_get_wakeup_address(void);

/*
 * Check if the CPU can handle C2 and deeper
 */
static inline unsigned int acpi_processor_cstate_check(unsigned int max_cstate)
{
	return max_cstate;
}

static inline bool arch_has_acpi_pdc(void)
{
	return true;
}

static inline void arch_acpi_set_pdc_bits(u32 *buf)
{
}

static inline bool acpi_has_cpu_in_madt(void)
{
	return !!acpi_lapic;
}

#define ACPI_HAVE_ARCH_SET_ROOT_POINTER
static inline void acpi_arch_set_root_pointer(u64 addr)
{
}

#define ACPI_HAVE_ARCH_GET_ROOT_POINTER
static inline u64 acpi_arch_get_root_pointer(void)
{
	return (u64)acpi_fuzz_root_pointer;
}

void acpi_generic_reduced_hw_init(void);

void x86_default_set_root_pointer(u64 addr);
u64 x86_default_get_root_pointer(void);

#else /* !CONFIG_ACPI */

#define acpi_lapic 0
#define acpi_ioapic 0
#define acpi_disable_cmcff 0
static inline void acpi_noirq_set(void) { }
static inline void acpi_disable_pci(void) { }
static inline void disable_acpi(void) { }

static inline void acpi_generic_reduced_hw_init(void) { }

static inline void x86_default_set_root_pointer(u64 addr) { }

static inline u64 x86_default_get_root_pointer(void)
{
	return 0;
}

#endif /* !CONFIG_ACPI */

#endif /* _ASM_LKL_ACPI_H */

