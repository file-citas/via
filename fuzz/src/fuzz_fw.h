#ifndef FUZZ_FW_H
#define FUZZ_FW_H

#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>

#define MAX_FW 32
#define MAX_FW_NAME 128
extern int fuzz_add_fw(const char *fwpath);
extern int fuzz_read_fw(const char *fwname, void **buffer, size_t *size);

#endif
