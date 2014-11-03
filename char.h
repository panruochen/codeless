#ifndef __CHAR_H
#define __CHAR_H

#include <ctype.h>
static inline bool is_id_char(char c)
{   return isalpha(c) || isdigit(c) || c == '_'; }

#define skip_blanks(ptr)  while( isblank(*ptr) ) ptr++
#define iseol(c)          ((c) == '\n' || (c) == '\0')

#endif

