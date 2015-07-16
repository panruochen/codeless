#ifndef __FS_LIBRARY_H
#define __FS_LIBRARY_H

#include "cc_string.h"

#if     ! defined(O_BINARY)
#define O_BINARY 0
#endif

FILE*      fsl_afopen(const CC_STRING& path);
int        fsl_fdappend(int fd, const void *buf, off_t n);
void       fsl_append(const CC_STRING& path, const void *buf, size_t n);
void       fsl_write(const CC_STRING& path, const void *buf, size_t n);
int        fsl_copy(const CC_STRING& src, const CC_STRING& dst);
bool       fsl_exist(const CC_STRING& path);
int        fsl_mkdir(const CC_STRING *path);
int        fsl_copy_with_parent(const CC_STRING& src, const CC_STRING& dst);
CC_STRING  fsl_realpath(const CC_STRING& src);
CC_STRING  fsl_dirname(const CC_STRING& path);


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

