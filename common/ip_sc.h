#ifndef __MP_ATOMIC_COUNTER_H
#define __MP_ATOMIC_COUNTER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <inttypes.h>
#include <pthread.h>

#define MAX_CNTS  8
typedef struct {
    pthread_mutex_t lock;
	struct {
		uint32_t writes;
		uint64_t bytes;
		uint64_t usecs;
	} records[MAX_CNTS];
} InterProcessSharedCounter;

InterProcessSharedCounter *ipsc_create(const char *name);
InterProcessSharedCounter *ipsc_open(const char *name);
void ipsc_acc_bytes(InterProcessSharedCounter *amap, int index, uint32_t val);
void ipsc_acc_usecs(InterProcessSharedCounter *amap, int index, uint32_t val);
void ipsc_acc_writes(InterProcessSharedCounter *amap,int index, uint32_t val);
void ipsc_close(InterProcessSharedCounter *amap);

#include <sys/time.h>

static inline uint64_t calc_time_elapsed(const struct timeval *tv0)
{
	struct timeval tv1;
	uint64_t diff_usec;

	gettimeofday(&tv1, NULL);
	diff_usec = (tv1.tv_sec - tv0->tv_sec) * 1000000ULL + (tv1.tv_usec - tv0->tv_usec);
	return diff_usec;
}

#ifdef __cplusplus
}
#endif

#endif

