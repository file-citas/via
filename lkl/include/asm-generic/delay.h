/* SPDX-License-Identifier: GPL-2.0 */
#ifndef __ASM_GENERIC_DELAY_H
#define __ASM_GENERIC_DELAY_H

/* Undefined functions to get compile-time errors */
extern void __bad_udelay(void);
extern void __bad_ndelay(void);

#if defined(MODULE) || defined(FUZZ_REMOVE_DELAY)
extern void __udelay_fuzz(unsigned long usecs);
extern void __ndelay_fuzz(unsigned long nsecs);
extern void __const_udelay_fuzz(unsigned long xloops);
extern void __delay_fuzz(unsigned long loops);
#define __udelay __udelay_fuzz
#define __ndelay __ndelay_fuzz
#define __const_udelay __const_udelay_fuzz
#define __delay __delay_fuzz
#else
extern void __udelay(unsigned long usecs);
extern void __ndelay(unsigned long nsecs);
extern void __const_udelay(unsigned long xloops);
extern void __delay(unsigned long loops);
#endif

/*
 * The weird n/20000 thing suppresses a "comparison is always false due to
 * limited range of data type" warning with non-const 8-bit arguments.
 */

/* 0x10c7 is 2**32 / 1000000 (rounded up) */
#define udelay(n)							\
	({								\
		if (__builtin_constant_p(n)) {				\
			if ((n) / 20000 >= 1)				\
				 __bad_udelay();			\
			else						\
				__const_udelay((n) * 0x10c7ul);		\
		} else {						\
			__udelay(n);					\
		}							\
	})

/* 0x5 is 2**32 / 1000000000 (rounded up) */
#define ndelay(n)							\
	({								\
		if (__builtin_constant_p(n)) {				\
			if ((n) / 20000 >= 1)				\
				__bad_ndelay();				\
			else						\
				__const_udelay((n) * 5ul);		\
		} else {						\
			__ndelay(n);					\
		}							\
	})

#endif /* __ASM_GENERIC_DELAY_H */
