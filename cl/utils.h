#ifndef __UTILS_H
#define __UTILS_H

#include "cc_string.h"
#include "cc_array.h"
#include "base.h"

bool     find(const CC_ARRAY<CC_STRING>& haystack, const CC_STRING& needle);
void     join(CC_ARRAY<CC_STRING>& x, const CC_ARRAY<CC_STRING>& y);
void     ujoin(CC_ARRAY<CC_STRING>& uarray, const char *elem);
ssize_t  getline(FILE *fp, CC_STRING& line);
void     fatal(int exit_status, const char *fmt, ...);

SOURCE_TYPE check_source_type(const CC_STRING& filename);

#if     ! defined(O_BINARY)
#define O_BINARY 0
#endif

bool       fol_exist(const CC_STRING& path);
int        fol_mkdir(const CC_STRING *path);
CC_STRING  fol_realpath(const CC_STRING& src);
CC_STRING  fol_dirname(const CC_STRING& path);


static inline int stat(const CC_STRING& filename, struct stat *buf)
{
    return ::stat(filename.c_str(), buf);
}

static inline FILE* fopen(const CC_STRING& filename, const char *mode)
{
    return ::fopen(filename.c_str(), mode);
}

static inline int rename(const CC_STRING& oldpath, const CC_STRING& newpath)
{
    return ::rename(oldpath.c_str(), newpath.c_str());
}

#endif

