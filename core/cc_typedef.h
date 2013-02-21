#ifndef __CC_TYPEDEF_H__
#define __CC_TYPEDEF_H__

#	ifdef  __cplusplus
extern "C" {
#	endif

struct _CC_HANDLE;
typedef  struct _CC_HANDLE    *CC_HANDLE;

#include <errno.h>

typedef  unsigned short sym_t;

#	ifdef  __cplusplus
}
#	endif

#	ifndef  __cplusplus
typedef enum { false, true }  bool;
#	endif

#endif /* __CC_TYPEDEF_H__ */

