#ifndef FUZZ_INTERFACE_H
#define FUZZ_INTERFACE_H

#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdbool.h>
#include <fcntl.h>
#include <dlfcn.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/mman.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <linux/ethtool.h>
#include <net/if.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <libconfig.h>
#include <lkl.h>
#include <lkl_host.h>


void sanitizer_prng_buf_test(uint8_t* a, size_t b);
void sanitizer_high_watermark(uint32_t m);
void fuzz_set_irq(int irq);
void fuzz_set_pci_ids(uint32_t vid, uint32_t pid, uint32_t svid, uint32_t spid, char revision, uint16_t pci_class);
void set_fuzz_buf(const uint8_t *ptr, size_t  size);
void unset_fuzz_buf(void);
size_t get_fuzz_idx(void);
void *fuzz_get_dma_access_list();
void fuzz_set_dma_access_list(void*);
void fuzz_add_bar(uint32_t size, uint32_t flags);
extern size_t fuzz_dma_access_list_size;

#endif

