#include "ne.h"
  
static FILE *fp_trace;
	 
void __attribute__ ((constructor)) trace_begin (void) {
	fp_trace = fopen("trace.out", "w");
	setvbuf(fp_trace, NULL, _IONBF, 0);
}
	 
void __attribute__ ((destructor)) trace_end (void) {
	fclose(fp_trace);
}
	 
void __cyg_profile_func_enter (void *func,  void *caller) {
	if(func == add_tail || func == rem || func == detect_encoding || func == next_pos || func == utf8char || func == output_width || func == get_char_width || func == calc_pos || func == strnlen_ne) return;
	fprintf(fp_trace, "e %p %p %lu\n", func, caller, time(NULL));
}
	 
void __cyg_profile_func_exit (void *func, void *caller) {
	if(func == add_tail || func == rem || func == detect_encoding || func == next_pos || func == utf8char || func == output_width || func == get_char_width || func == calc_pos || func == strnlen_ne) return;
	fprintf(fp_trace, "x %p %p %lu\n", func, caller, time(NULL));
}
