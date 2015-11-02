#ifndef __BASE_H
#define __BASE_H

#if __linux
#include <linux/limits.h>
#include <inttypes.h>
#elif   !defined(PATH_MAX)
#define PATH_MAX    4096
#endif

#define __NO_USE__   __attribute__((__unused__))

#define offset_of(type, member) ((unsigned long)(&((type*)0)->member))
#define container_of(ptr, type, member)  ((type*)((unsigned long)(ptr) - offset_of(type,member)))

#define GDB_TRAP2(cond1, cond2)                   \
do {                                              \
	if( (cond1) && (cond2) )   {                  \
		volatile int loop = 1;                    \
		do {} while (loop);                       \
	}                                             \
} while(0)                                        \

#ifndef COUNT_OF
#define COUNT_OF(a)  (sizeof(a) / sizeof(a[0]))
#endif

#define TR(pstate,opr)       pstate->syLut.TrId(opr).c_str()
#define TOKEN_NAME(a)    (a).name.c_str()

#define CB_BEGIN     {
#define CB_END       }

enum SOURCE_TYPE {
	SOURCE_TYPE_ERR = 0,
	SOURCE_TYPE_C   = 1,
	SOURCE_TYPE_CPP = 2,
	SOURCE_TYPE_S   = 3,
};

#include <ctype.h>
static inline bool is_id_char(char c)
{ return isalpha(c) || isdigit(c) || c == '_'; }

#define skip_blanks(ptr)  while( isblank(*ptr) ) ptr++
#define iseol(c)          ((c) == '\n' || (c) == '\0')

#endif
