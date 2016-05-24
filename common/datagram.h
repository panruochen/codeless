#ifndef __DATAGRAM_H
#define __DATAGRAM_H

#ifdef __cplusplus
extern "C" {
#endif

#include <sys/types.h>
#include <inttypes.h>
#include <unistd.h>

typedef struct {
	uint8_t data[16];
} Md5Sum;

typedef struct {
#define DMF_MD5  0x20
	uint32_t len; /* The length of the message including this header */
	char     tag[2]; /* "MSG" */
	uint8_t  flags;
	uint8_t  type;
	pid_t    pid;
	Md5Sum   md5_sum;
	char data[0];
} Datagram;

/* virtual channles for message transfer */
enum {
	VCH_CL = 0,
	VCH_CV,
	VCH_DEP,
	VCH_MAX,
};

#ifdef __cplusplus
}
#endif

#endif
