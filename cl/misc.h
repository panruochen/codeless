#ifndef __MISC_H
#define __MISC_H

#include "InternalTables.h"

CC_STRING ExpandLine(InternalTables *pstate, bool for_include, const char *line, CC_STRING& errmsg);
void handle_undef(InternalTables *pstate, const char *line);

/*
 *   1 - successful
 *   0 - no any more tokens
 *  -1 - error occurs
 */
int ReadToken(InternalTables *pstate, ReadTokenOp *rop, bool for_include);

#endif

