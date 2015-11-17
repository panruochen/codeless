#ifndef __MSG_FMT_H
#define __MSG_FMT_H

#ifdef __cplusplus
extern "C" {
#endif

#include <sys/types.h>
#include <inttypes.h>
#include <unistd.h>

typedef struct _Md5Sum {
	uint8_t data[16];
} Md5Sum;

typedef struct _DsMsg_S {
#define DMF_MD5  0x20
	uint32_t len; /* The length of the message including this header */
	char     tag[2]; /* "MSG" */
	uint8_t  flags;
	uint8_t  type;
	pid_t    pid;
	Md5Sum   md5_sum;
	char data[0];
} DsMsg;

enum {
	MSGT_CL = 0,
	MSGT_CV,
	MSGT_DEP,
	MSGT_MAX,
};

#ifdef __cplusplus
}
#endif

#endif
