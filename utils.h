#ifndef __UTILS_H
#define __UTILS_H

#include "cc_string.h"
#include "cc_array.h"

bool     find(const CC_ARRAY<CC_STRING>& haystack, const CC_STRING& needle);
void     join(CC_ARRAY<CC_STRING>& x, const CC_ARRAY<CC_STRING>& y);
void     ujoin(CC_ARRAY<CC_STRING>& uarray, const char *elem);
ssize_t  getline(FILE *fp, CC_STRING& line);
void     fatal(int exit_status, const char *fmt, ...);

#endif

