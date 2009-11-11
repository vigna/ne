/*
 *	Copyright
 *		(C) 2004 Joseph H. Allen
 *
 *	This file is part of JOE (Joe's Own Editor)
 */
 
#include <string.h>
#include <errno.h>
#include <math.h>

#define joe_gettext(s) my_gettext((unsigned char *)(s))

/* Strings needing translation are marked with this macro */
#define _(s) (s)

/* Global Defines */

/* Prefix to make string constants unsigned */
#define USTR (unsigned char *)
/* Doubly-linked list node */
#define LINK(type) struct { type *next; type *prev; }

#ifdef HAVE_SNPRINTF

#define joe_snprintf_0(buf,len,fmt) snprintf((char *)(buf),(len),(char *)(fmt))
#define joe_snprintf_1(buf,len,fmt,a) snprintf((char *)(buf),(len),(char *)(fmt),(a))
#define joe_snprintf_2(buf,len,fmt,a,b) snprintf((char *)(buf),(len),(char *)(fmt),(a),(b))
#define joe_snprintf_3(buf,len,fmt,a,b,c) snprintf((char *)(buf),(len),(char *)(fmt),(a),(b),(c))
#define joe_snprintf_4(buf,len,fmt,a,b,c,d) snprintf((char *)(buf),(len),(char *)(fmt),(a),(b),(c),(d))
#define joe_snprintf_5(buf,len,fmt,a,b,c,d,e) snprintf((char *)(buf),(len),(char *)(fmt),(a),(b),(c),(d),(e))
#define joe_snprintf_6(buf,len,fmt,a,b,c,d,e,f) snprintf((char *)(buf),(len),(char *)(fmt),(a),(b),(c),(d),(e),(f))
#define joe_snprintf_7(buf,len,fmt,a,b,c,d,e,f,g) snprintf((char *)(buf),(len),(char *)(fmt),(a),(b),(c),(d),(e),(f),(g))
#define joe_snprintf_8(buf,len,fmt,a,b,c,d,e,f,g,h) snprintf((char *)(buf),(len),(char *)(fmt),(a),(b),(c),(d),(e),(f),(g),(h))
#define joe_snprintf_9(buf,len,fmt,a,b,c,d,e,f,g,h,i) snprintf((char *)(buf),(len),(char *)(fmt),(a),(b),(c),(d),(e),(f),(g),(h),(i))
#define joe_snprintf_10(buf,len,fmt,a,b,c,d,e,f,g,h,i,j) snprintf((char *)(buf),(len),(char *)(fmt),(a),(b),(c),(d),(e),(f),(g),(h),(i),(j))

#define i_printf_0(fmt) (snprintf((char *)(i_msg),sizeof(i_msg),(char *)(fmt)), internal_msg(i_msg))
#define i_printf_1(fmt,a) (snprintf((char *)(i_msg),sizeof(i_msg),(char *)(fmt),(a)), internal_msg(i_msg))
/*#define i_printf_2(fmt,a,b) (snprintf((char *)(i_msg),sizeof(i_msg),(char *)(fmt),(a),(b)), internal_msg(i_msg))*/
#define i_printf_2(fmt,a,b) (fprintf(stderr,(char *)(fmt),(a),(b)))
#define i_printf_3(fmt,a,b,c) (snprintf((char *)(i_msg),sizeof(i_msg),(char *)(fmt),(a),(b),(c)), internal_msg(i_msg))
#define i_printf_4(fmt,a,b,c,d) (snprintf((char *)(i_msg),sizeof(i_msg),(char *)(fmt),(a),(b),(c),(d)), internal_msg(i_msg))

#else

#define joe_snprintf_0(buf,len,fmt) sprintf((char *)(buf),(char *)(fmt))
#define joe_snprintf_1(buf,len,fmt,a) sprintf((char *)(buf),(char *)(fmt),(a))
#define joe_snprintf_2(buf,len,fmt,a,b) sprintf((char *)(buf),(char *)(fmt),(a),(b))
#define joe_snprintf_3(buf,len,fmt,a,b,c) sprintf((char *)(buf),(char *)(fmt),(a),(b),(c))
#define joe_snprintf_4(buf,len,fmt,a,b,c,d) sprintf((char *)(buf),(char *)(fmt),(a),(b),(c),(d))
#define joe_snprintf_5(buf,len,fmt,a,b,c,d,e) sprintf((char *)(buf),(char *)(fmt),(a),(b),(c),(d),(e))
#define joe_snprintf_6(buf,len,fmt,a,b,c,d,e,f) sprintf((char *)(buf),(char *)(fmt),(a),(b),(c),(d),(e),(f))
#define joe_snprintf_7(buf,len,fmt,a,b,c,d,e,f,g) sprintf((char *)(buf),(char *)(fmt),(a),(b),(c),(d),(e),(f),(g))
#define joe_snprintf_8(buf,len,fmt,a,b,c,d,e,f,g,h) sprintf((char *)(buf),(char *)(fmt),(a),(b),(c),(d),(e),(f),(g),(h))
#define joe_snprintf_9(buf,len,fmt,a,b,c,d,e,f,g,h,i) sprintf((char *)(buf),(char *)(fmt),(a),(b),(c),(d),(e),(f),(g),(h),(i))
#define joe_snprintf_10(buf,len,fmt,a,b,c,d,e,f,g,h,i,j) sprintf((char *)(buf),(char *)(fmt),(a),(b),(c),(d),(e),(f),(g),(h),(i),(j))

#define i_printf_0(fmt) (sprintf((char *)(i_msg),(char *)(fmt)), internal_msg(i_msg))
#define i_printf_1(fmt,a) (sprintf((char *)(i_msg),(char *)(fmt),(a)), internal_msg(i_msg))
/*#define i_printf_2(fmt,a,b) (sprintf((char *)(i_msg),(char *)(fmt),(a),(b)), internal_msg(i_msg))*/
#define i_printf_2(fmt,a,b) (fprintf(stderr,(char *)(fmt),(a),(b)))
#define i_printf_3(fmt,a,b,c) (sprintf((char *)(i_msg),(char *)(fmt),(a),(b),(c)), internal_msg(i_msg))
#define i_printf_4(fmt,a,b,c,d) (sprintf((char *)(i_msg),(char *)(fmt),(a),(b),(c),(d)), internal_msg(i_msg))

#endif

#define stdsiz			8192

typedef struct header H;
typedef struct buffer B;
typedef struct point P;
typedef struct options OPTIONS;
typedef struct macro MACRO;
typedef struct cmd CMD;
typedef struct entry HENTRY;
typedef struct hash HASH;
typedef struct bw BW;
typedef struct scrn SCRN;
typedef struct cap CAP;
typedef struct vpage VPAGE;
typedef struct vfile VFILE;
typedef struct highlight_state HIGHLIGHT_STATE;

/* Structure which are passed by value */

struct highlight_state {
	struct high_frame *stack;   /* Pointer to the current frame in the call stack */
	int state;                  /* Current state in the current subroutine */
	unsigned char saved_s[24];  /* Buffer for saved delimiters */
};


