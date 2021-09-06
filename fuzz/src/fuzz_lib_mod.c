#define _GNU_SOURCE
#include <time.h>
#include <dlfcn.h>
#include <elf.h>
#include <unistd.h>
#include <errno.h>
#include <sys/personality.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <link.h>
#include <lkl.h>
#include <lkl_host.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#ifdef DFS
#include <sanitizer/dfsan_interface.h>
#endif
#include "fuzz_interface.h"
#include "util.h"

void __attribute__((weak)) __sanitizer_high_watermark(uint32_t mark);
void __attribute__((weak)) __sanitizer_prng_buf_test(uint8_t* a, size_t b);
void sanitizer_prng_buf_test(uint8_t* a, size_t b) {
#ifdef FUZZ
  __sanitizer_prng_buf_test(a, b);
#endif
}

void sanitizer_high_watermark(uint32_t m) {
#ifdef FUZZ
  __sanitizer_high_watermark(m);
#endif
}

int DoInit(void) {
   char* fuzz_target = getenv("FUZZ_TARGET");
   if(do_config(fuzz_target)!=0) {
      fprintf(stderr, "Failed to read config %s\n", fuzz_target);
      return -EXIT_FAILURE;
   }
   if(do_lkl_init() < 0) {
      fprintf(stderr, "Failed to init lkl\n");
      return -EXIT_FAILURE;
   }

   if(mod_init_func != NULL) {
      if(mod_init_func() != 0) {
         fprintf(stderr, "harness mod_init failed\n");
         return -EXIT_FAILURE;
      }
   }

   if(aga_eval != 0 && interface_ptr != NULL) {
      if(eth_auto_init()!=0) {
         return -EXIT_FAILURE;
      }
   }

  lkl_fuzz_print_config();
  return 0;
}

static bool initialized = false;
#ifndef FUZZ
int main(int argc, char **argv){
#else
int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
#endif
  int ret;
#ifndef FUZZ
  uint8_t *data = NULL;
  size_t size = 0;
  struct stat st;
  if(argc == 2) {
    int fd = open(argv[1], O_RDONLY);
    FILE *fp = fopen(argv[1], "rb");
    if ((fstat(fd, &st) != 0) || (!S_ISREG(st.st_mode))) {
      fprintf(stderr, "fstat failed %s\n", argv[1]);
      return 1;
    }
    if (fseeko(fp, 0 , SEEK_END) != 0) {
      fprintf(stderr, "seek failed\n");
      return 1;
    }
    size = ftello(fp);
    data = malloc(size);
    fseeko(fp, 0, SEEK_SET);
    fread(data, 1, size, fp);
    fclose(fp);
  } else {
    fprintf(stderr, "Usage: prog io io.dma\n");
    return 1;
  }

#endif
  if(!initialized) {
     ret = DoInit();
     initialized = true;
     if(ret != 0)
        return ret;
  }
  mod_fuzz_func(data, size);
  update_hwm();

#ifndef FUZZ
  free(data);
#endif
  return 0;
}

