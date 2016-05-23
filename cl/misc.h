#ifndef __MISC_H
#define __MISC_H

#include "InternalTables.h"

CC_STRING ExpandLine(InternalTables *intab, bool for_include, const char *line, CC_STRING& errmsg);
void handle_undef(InternalTables *intab, const char *line);

/*
 *   1 - successful
 *   0 - no any more tokens
 *  -1 - error occurs
 */
int ReadToken(InternalTables *intab, ReadReq *req, bool for_include);

#endif

