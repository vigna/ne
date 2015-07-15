/* efopen is like fopen, but using an embedded set of files */
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "efopen.h"

extern char _binary_efopen_dat_start;
extern char _binary_efopen_dat_end;

static toc_t *tocv = NULL;
static size_t tocc;

/* comparator for toc_t */
static int toccmp(const void* k, const void* a) {
   return strcmp((char*)k, &_binary_efopen_dat_start + ((toc_t*)a)->name);
}

/* lazy init */
static void init(void) {
   tocc = *(size_t*)(&_binary_efopen_dat_end - sizeof(tocc));
   tocv = (toc_t*)(&_binary_efopen_dat_end - sizeof(tocc) - tocc * sizeof(toc_t));
}

extern FILE *efopen(const char *path, const char *mode) {

   /* lazy init */
   if(tocv == NULL) init();

   /* find matching entry in table of contents */
   toc_t *entry = bsearch(path, tocv, tocc, sizeof(toc_t), &toccmp);
   if(entry == NULL) {
      errno = ENOENT;
      return NULL;
   }

   /* only allow read-only access */
   if(strcmp("r", mode) != 0) {
      errno = EPERM;
      return NULL;
   }

   /* create filehandle to the relevant embedded data */
   size_t *size = (size_t*)(&_binary_efopen_dat_start + entry->data);
   void *data = size + 1;
   return fmemopen(data, *size, "r");

}
