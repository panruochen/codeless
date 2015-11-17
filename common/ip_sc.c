#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "ip_sc.h"

InterProcessSharedCounter * ipsc_create(const char *shared_name)
{
	int fd = -1;
    InterProcessSharedCounter *sc = NULL;
    pthread_mutexattr_t psharedm;

    fd = open(shared_name, O_RDWR | O_CREAT , 0666);
    if (fd < 0)
        return (NULL);
	if(ftruncate(fd, sizeof(*sc)))
		goto fail_and_return;
    (void) pthread_mutexattr_init(&psharedm);
    (void) pthread_mutexattr_setpshared(&psharedm, PTHREAD_PROCESS_SHARED);
    sc = (InterProcessSharedCounter *) mmap(NULL, sizeof(*sc), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    close (fd);
    (void) pthread_mutex_init(&sc->lock, &psharedm);
    memset(sc->records, 0, sizeof(sc->records));
    return (sc);

fail_and_return:
	if(sc)
		free(sc);
	if(fd > 0)
		close(fd);
	return NULL;
}

InterProcessSharedCounter * ipsc_open(const char *shared_name)
{
    int fd;
    InterProcessSharedCounter *sc;

    fd = open(shared_name, O_RDWR, 0666);
    if (fd < 0)
        return (NULL);
    sc = (InterProcessSharedCounter *) mmap(NULL, sizeof(*sc),
            PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    close(fd);
    return (sc);
}

void ipsc_acc_bytes(InterProcessSharedCounter *sc, int index, uint32_t val)
{
    pthread_mutex_lock(&sc->lock);
    sc->records[index].bytes += val;
    pthread_mutex_unlock(&sc->lock);
}

void ipsc_acc_writes(InterProcessSharedCounter *sc, int index, uint32_t val)
{
    pthread_mutex_lock(&sc->lock);
    sc->records[index].writes += val;
    pthread_mutex_unlock(&sc->lock);
}

void ipsc_acc_usecs(InterProcessSharedCounter *sc, int index, uint32_t val)
{
    pthread_mutex_lock(&sc->lock);
    sc->records[index].usecs += val;
    pthread_mutex_unlock(&sc->lock);
}


void ipsc_close(InterProcessSharedCounter *sc)
{
    munmap((void *) sc, sizeof(InterProcessSharedCounter));
}


