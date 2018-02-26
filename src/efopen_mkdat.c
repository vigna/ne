/* tool for compiling files into a binary lump suitable for inclusion */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "efopen.h"

/* qsort wrapper for strcmp */
int qstrcmp(const void* a, const void* b) {
   return strcmp(*((char**) a), *((char**) b));
}

/* entry point */
int main(int argc, char **argv) {

   /* show usage */
   if(argc < 2) {
      fprintf(stderr, "Usage: %s INFILES... > OUTFILE\n", argv[0]);
      return 1;
   }

   /* sort args */
   size_t count = argc - 1;
   qsort(&argv[1], count, sizeof(void *), &qstrcmp);
   
   /* table of contents */
   toc_t toc[count];
   
   /* concat the files */
   size_t pos = 0;
   int i;
   char buf[1024];
   for(i = 0; i < count; i++) {
      toc[i].name = pos;
      pos += fwrite(argv[i + 1], 1, strlen(argv[i + 1]) + 1, stdout);
      FILE *fh = fopen(argv[i + 1], "r");
      fseek(fh, 0, SEEK_END);
      size_t s = ftell(fh);
      toc[i].data = pos;
      fseek(fh, 0, SEEK_SET);
      pos += fwrite(&s, 1, sizeof(s), stdout);
      while((s = fread(buf, 1, sizeof(buf), fh)) > 0)
	pos += fwrite(buf, 1, s, stdout);
      fclose(fh);
   }

   /* output the table of contents */
   fwrite(toc, sizeof(*toc), count, stdout);
   
   /* output entry count */
   fwrite(&count, sizeof(count), 1, stdout);
   
   /* finished */
   return 0;
}
