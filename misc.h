#ifndef __MISC_H
#define __MISC_H

#include "ParsedState.h"
#include "Exception.h"

CC_STRING ExpandLine(ParsedState *pstate, bool for_include, const char *line, Exception *excep);
void handle_undef(ParsedState *pstate, const char *line);
bool ReadToken(ParsedState *pstate, const char **line, SynToken *tokp, Exception *excep, bool for_include);

#endif

