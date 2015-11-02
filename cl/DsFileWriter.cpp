#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <time.h>
#include <errno.h>
#include <assert.h>
#include "msgfmt.h"
#include "md5.h"
#include "FileWriter.h"

#define CLI_PATH    "/var/tmp/"      /* +5 for pid = 14 chars */

#if 0
#define ALOG(fmt,args...)  printf("[C] " fmt,##args)
#else
#define ALOG(fmt,args...)  do{}while(0)
#endif

const char *DsFileWriter::svr_addr;
const char *DsFileWriter::cli_addr;
int DsFileWriter::svr_fd = -1;
unsigned int DsFileWriter::ref_count;


/*
 * Create a client endpoint and connect to a server.
 * Returns fd if all OK, <0 on error.
 */
int DsFileWriter::Connect(const char *saddr)
{
	int fd = -1, len, saved_errno;
	struct sockaddr_un saddr_un;

	if(svr_fd > 0) {
		if( strcmp(saddr, svr_addr) != 0 )
			return -EPERM;
		ref_count++;
		return svr_fd;
	}

	svr_addr = strdup(saddr);

	/* create a UNIX domain stream socket */
	if ((fd = socket(AF_UNIX, SOCK_STREAM, 0)) < 0)
		goto errout;

	/* fill socket address structure with our address */
	memset(&saddr_un, 0, sizeof(saddr_un));
	saddr_un.sun_family = AF_UNIX;
	sprintf(saddr_un.sun_path, "%s%06u", CLI_PATH, getpid());
	len = offsetof(struct sockaddr_un, sun_path) + strlen(saddr_un.sun_path);
	cli_addr = strdup(saddr_un.sun_path);

	unlink(saddr_un.sun_path); /* in case it already exists */
	if (bind(fd, (struct sockaddr *)&saddr_un, len) < 0)
		goto errout;

	/* fill socket address structure with server's address */
	memset(&saddr_un, 0, sizeof(saddr_un));
	saddr_un.sun_family = AF_UNIX;
	strcpy(saddr_un.sun_path, svr_addr);
	len = offsetof(struct sockaddr_un, sun_path) + strlen(svr_addr);
	if (connect(fd, (struct sockaddr *)&saddr_un, len) < 0)
		goto errout;

	svr_fd = fd;
	ref_count++;
	return(fd);

errout:
	saved_errno = errno;
	if(fd > 0)
		close(fd);
	errno = saved_errno;
	svr_fd = -1;
	return (-1);
}


#define WRITE2(fd,buf,n) do { ret = write(fd,buf,n); if(ret!=(n)) return -1; } while(0)

ssize_t DsFileWriter::Write(const void *buf, size_t count)
{
	int ret;
	const size_t chunksize = 2048;
	DsMsg msghdr;
	const int retval = count + sizeof(msghdr);
	char *ptr;
	MD5_CTX md5_context;

	MD5_Init(&md5_context);
	MD5_Update(&md5_context, buf, count);
	MD5_Final(msghdr.md5, &md5_context);

	if(count == 0)
		return 0;
	msghdr.len   = count + sizeof(msghdr);
	msghdr.type  = mtype;
	msghdr.pid   = getpid();
	ptr = (char *) &msghdr;
	if(rand() & 1) {
		WRITE2(svr_fd, ptr, 1);
		WRITE2(svr_fd, ptr+1, 1);
	} else {
		WRITE2(svr_fd, ptr, 2);
	}

	WRITE2(svr_fd, ptr+2, sizeof(msghdr) - 2);

	while(count > 0) {
		const int nb = count >= chunksize ? chunksize : count;

		ret = write(svr_fd, buf, nb);
		if(ret != nb) {
			ALOG("(%06u) SendMsg failed, write(%d) = %d, %d/%d bytes sent\n", getpid(), nb, ret, retval - count, retval);
			return -1;
		}
		buf = nb + (char*)buf;
		count -= nb;
	}
	return retval;
}

DsFileWriter::DsFileWriter(int mtype_)
{
	mtype = mtype_;
}

DsFileWriter::~DsFileWriter()
{
	if(ref_count == 0)
		exit(EINVAL);
	ref_count--;
	if(ref_count == 0) {
		if(svr_fd > 0)
			close(svr_fd);
		unlink(cli_addr);
		if(cli_addr)
			free((void*)cli_addr);
		if(svr_addr)
			free((void*)svr_addr);
	}
}

