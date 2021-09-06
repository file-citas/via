#ifndef ARCH_FUZZ_H
#define ARCH_FUZZ_H

#include <uapi/asm/fuzz.h>
#include <linux/init.h>
#include <linux/kernel.h>

long fuzz_configure_pci(struct fuzz_pci_dev_config __user *uconf);
long fuzz_configure_platform(struct fuzz_platform_dev_config __user *uconf);
long fuzz_configure_acpi(struct fuzz_acpi_dev_config __user *uconf);

#endif
