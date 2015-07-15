/* efopen is like fopen, but using an embedded set of files */
#include <stdio.h>

typedef struct __attribute__((__packed__)) {
   size_t name;
   size_t data;
} toc_t;

extern FILE *efopen(const char *path, const char *mode);
