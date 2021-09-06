#ifndef _ASM_LKL_IO_H
#define _ASM_LKL_IO_H

#include <asm/bug.h>
#include <asm/host_ops.h>

#define memcpy_fromio memcpy_fromio
static inline void memcpy_fromio(void *buffer,
				 const volatile void __iomem *addr,
				 size_t size)
{
   lkl_ops->fuzz_ops->get_n(buffer, size);
}

#define __raw_readb __raw_readb
static inline u8 __raw_readb(const volatile void __iomem *addr)
{
	int ret;
	u8 value;

	ret = lkl_ops->iomem_access(addr, &value, sizeof(value), 0);
	WARN(ret, "error reading iomem %p", addr);

	return value;
}

#define __raw_readw __raw_readw
static inline u16 __raw_readw(const volatile void __iomem *addr)
{
	int ret;
	u16 value;

	ret = lkl_ops->iomem_access(addr, &value, sizeof(value), 0);
	WARN(ret, "error reading iomem %p", addr);

	return value;
}

#define __raw_readl __raw_readl
static inline u32 __raw_readl(const volatile void __iomem *addr)
{
	int ret;
	u32 value;

	ret = lkl_ops->iomem_access(addr, &value, sizeof(value), 0);
	WARN(ret, "error reading iomem %p", addr);

	return value;
}

#ifdef CONFIG_64BIT
#define __raw_readq __raw_readq
static inline u64 __raw_readq(const volatile void __iomem *addr)
{
	int ret;
	u64 value;

	ret = lkl_ops->iomem_access(addr, &value, sizeof(value), 0);
	WARN(ret, "error reading iomem %p", addr);

	return value;
}
#endif /* CONFIG_64BIT */

#define __raw_writeb __raw_writeb
static inline void __raw_writeb(u8 value, volatile void __iomem *addr)
{
	int ret;

	ret = lkl_ops->iomem_access(addr, &value, sizeof(value), 1);
	WARN(ret, "error writing iomem %p", addr);
}

#define __raw_writew __raw_writew
static inline void __raw_writew(u16 value, volatile void __iomem *addr)
{
	int ret;

	ret = lkl_ops->iomem_access(addr, &value, sizeof(value), 1);
	WARN(ret, "error writing iomem %p", addr);
}

#define __raw_writel __raw_writel
static inline void __raw_writel(u32 value, volatile void __iomem *addr)
{
	int ret;

	ret = lkl_ops->iomem_access(addr, &value, sizeof(value), 1);
	WARN(ret, "error writing iomem %p", addr);
}

#ifdef CONFIG_64BIT
#define __raw_writeq __raw_writeq
static inline void __raw_writeq(u64 value, volatile void __iomem *addr)
{
	int ret;

	ret = lkl_ops->iomem_access(addr, &value, sizeof(value), 1);
	WARN(ret, "error writing iomem %p", addr);
}
#endif /* CONFIG_64BIT */

#define ioremap ioremap
static inline void __iomem *ioremap(phys_addr_t offset, size_t size)
{
	return (void __iomem *)lkl_ops->ioremap(offset, size);
}

#define ioremap_cache ioremap_cache
static inline void __iomem *ioremap_cache(phys_addr_t offset, size_t size) {
	return (void __iomem *)lkl_ops->ioremap(offset, size);
}

#define ioport_map ioport_map
static inline void __iomem *ioport_map(unsigned long port, unsigned int nr)
{
	return (void __iomem *)lkl_ops->ioremap(port, nr);
}

#define _inb _inb
static inline u8 _inb(unsigned long addr)
{
	u8 val;

	val = lkl_ops->iomem_access((void*)addr, &val, sizeof(val), 0);
	return val;
}

#define _inw _inw
static inline u16 _inw(unsigned long addr)
{
	u16 val;

	val = lkl_ops->iomem_access((void*)addr, &val, sizeof(val), 0);
	return val;
}

#define _inl _inl
static inline u32 _inl(unsigned long addr)
{
	u32 val;

	val = lkl_ops->iomem_access((void*)addr, &val, sizeof(val), 1);
	return val;
}

#define _outb _outb
static inline void _outb(u8 value, unsigned long addr)
{
   lkl_ops->iomem_access((void*)addr, &value, sizeof(value), 1);
}

#define _outw _outw
static inline void _outw(u16 value, unsigned long addr)
{
   lkl_ops->iomem_access((void*)addr, &value, sizeof(value), 1);
}

#define _outl _outl
static inline void _outl(u32 value, unsigned long addr)
{
   lkl_ops->iomem_access((void*)addr, &value, sizeof(value), 1);
}

#include <asm-generic/io.h>

#endif /* _ASM_LKL_IO_H */

