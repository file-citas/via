#ifndef KASAN_ASAN_H
#define KASAN_ASAN_H

#include <linux/stddef.h>

#ifdef CONFIG_KASAN

// Note(feli): we get these dynamically from asan
#define KASAN_SHADOW_START ASAN_SHADOW_START
#ifndef ASAN_SHADOW_SCALE
#define ASAN_SHADOW_SCALE 3
#endif
#define KASAN_SHADOW_SCALE_SHIFT ASAN_SHADOW_SCALE
#define KASAN_SHADOW_OFFSET ASAN_SHADOW_OFFSET
extern unsigned long __asan_MemToShadow(unsigned long ptr);
extern bool __asan_AddressIsPoisoned(unsigned long a);
extern bool __asan_AddrIsInShadow(unsigned long a);
extern int __asan_get_allocation_context(void);
extern void __asan_print_allocation_context(int);
extern void __asan_print_allocation_context_err(int);
extern void __asan_print_allocation_context_buf(int, char*, long);
extern void __sanitizer_print_stack_trace(void);
#else

#define KASAN_SHADOW_START 0
#define KASAN_SHADOW_SCALE_SHIFT 0
#define KASAN_SHADOW_OFFSET 0
static inline unsigned long __asan_MemToShadow(unsigned long ptr) { return 0; }
static inline bool __asan_AddressIsPoisoned(unsigned long a) { return false; }
static inline bool __asan_AddrIsInShadow(unsigned long a) { return false; }
static inline int __asan_get_allocation_context(void) {}
static inline void __asan_print_allocation_context(int) {}
extern void __asan_print_allocation_context_err(int) {}
extern void __asan_print_allocation_context_buf(int, char*, long) {}
static inline void __sanitizer_print_stack_trace(void) {}

#endif
#endif
