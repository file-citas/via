/*
 * Just a stub driver ...
 *
 */
#include <linux/module.h>
#include <linux/kernel.h>

extern void fuzz_load_acpi(void);
int init_module(void)
{
   fuzz_load_acpi();
   return 0;
}

void cleanup_module(void)
{
}
MODULE_LICENSE("GPL v2");
