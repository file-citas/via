#include <stdio.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <libgen.h>
#include "fuzz_fw.h"

static char fwnames[MAX_FW][MAX_FW_NAME];
static uint8_t *fwdata[MAX_FW];
static size_t fwsize[MAX_FW];
static size_t nfw = 0;

int fuzz_add_fw(const char *fwpath) {
  uint8_t *data = NULL;
  size_t size = 0;
  struct stat st;
  const char *fwname = basename((char*)fwpath);
  printf("%s #%ld (%s) %s\n", __FUNCTION__, nfw, fwname, fwpath);
  int fd = open(fwpath, O_RDONLY);
  if(fd < 0) {
    fprintf(stderr, "open failed %s\n", fwpath);
    return -1;
  }
  FILE *fp = fopen(fwpath, "rb");
  if(!fp) {
    fprintf(stderr, "fopen failed %s\n", fwpath);
    return -1;
  }
  if ((fstat(fd, &st) != 0) || (!S_ISREG(st.st_mode))) {
    fprintf(stderr, "fstat failed\n");
    return -1;
  }
  if (fseeko(fp, 0 , SEEK_END) != 0) {
    fprintf(stderr, "seek failed\n");
    return -1;
  }
  size = ftello(fp);
  data = malloc(size);
  fseeko(fp, 0, SEEK_SET);
  fread(data, 1, size, fp);
  fclose(fp);

  strncpy(fwnames[nfw], fwname, MAX_FW_NAME);
  fwdata[nfw] = data;
  fwsize[nfw] = size;
  nfw++;
  return 0;
}

int fuzz_read_fw(const char *fwname, void **buffer, size_t *size)
{
  size_t i;
  const char *fwname_base = basename((char*)fwname);
  for(i=0; i<nfw; i++) {
    if(strcmp(fwname_base, fwnames[i]) == 0) {
      printf("Found fw %s\n", fwname_base);
      *buffer = fwdata[i];
      *size = fwsize[i];
      return 0;
    }
  }
  fprintf(stderr, "WARNING could not find fw %s (%s)\n", fwname, fwname_base);
  abort();
  return -1;
}
