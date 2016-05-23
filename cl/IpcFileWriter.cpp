#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <time.h>
#include <sys/time.h>
#include <errno.h>
#include <assert.h>
#include "msgfmt.h"
#include "md5.h"
#include "FileWriter.h"
#include "ip_sc.h"
#include "GlobalVars.h"

#if 0
#include <sys/time.h>
//#define ALOG(fmt,args...)  printf("[C] " fmt,##args)
#define ALOG(fmt,args...)  do { struct timeval tv; gettimeofday(&tv,NULL); fprintf(stderr, "[%08lu.%06lu] [C] " fmt, tv.tv_sec, tv.tv_usec, ##args); } while(0)
#else
#define ALOG(fmt,args...)  do{}while(0)
#endif

const char *IpcFileWriter::svr_addr;
const char *IpcFileWriter::cli_addr;
int IpcFileWriter::svr_fd = -1;
unsigned int IpcFileWriter::ref_count;


/*
 * Create a client endpoint and connect to a server.
 * Returns fd if all OK, <0 on error.
 */
int IpcFileWriter::Connect(const char *rtdir, const char *saddr)
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

	/* Create a UNIX domain stream socket */
	if ((fd = socket(AF_UNIX, SOCK_STREAM, 0)) < 0)
		goto errout;

	/* Fill socket address structure with our address */
	memset(&saddr_un, 0, sizeof(saddr_un));
	saddr_un.sun_family = AF_UNIX;
//	fprintf(stderr, "%s\n", rtdir);
	sprintf(saddr_un.sun_path, "%s/%06u", rtdir, getpid());
	len = offsetof(struct sockaddr_un, sun_path) + strlen(saddr_un.sun_path);
	cli_addr = strdup(saddr_un.sun_path);

	unlink(saddr_un.sun_path); /* in case it already exists */
	if (bind(fd, (struct sockaddr *)&saddr_un, len) < 0)
		goto errout;

	/* Fill socket address structure with server's address */
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


#define WRITE_CHECKED(fd,buf,n) do { ret = write(fd,buf,n); if(ret!=(n)){retval = -1; goto write_fail;} } while(0)

ssize_t IpcFileWriter::Write(const void *buf, size_t count)
{
	int ret;
	const size_t chunksize = 0x10000;
	DsMsg msghdr;
	ssize_t retval = count;
	uint64_t elapsed;

	struct timeval tv0;
	gettimeofday(&tv0, NULL);

	if(count == 0)
		return 0;
	msghdr.len   = count + sizeof(msghdr);
	msghdr.type  = id;
	msghdr.pid   = getpid();
#if HAVE_MD5
	MD5_CTX md5_context;
	MD5_Init(&md5_context);
	MD5_Update(&md5_context, buf, count);
	MD5_Final(msghdr.md5_sum.data, &md5_context);
	msghdr.flags = DMF_MD5;
#endif

	WRITE_CHECKED(svr_fd, &msghdr, sizeof(msghdr));

	while(count > 0) {
		const int nb = count >= chunksize ? chunksize : count;

		WRITE_CHECKED(svr_fd, buf, nb);
		buf = nb + (char*)buf;
		count -= nb;
	}
	ALOG("(%06u) %u bytes sent\n", getpid(), retval);

write_fail:
	elapsed   = calc_time_elapsed(&tv0);

	if(gvar_sm) {
		ipsc_acc_bytes(gvar_sm, id, msghdr.len);
		ipsc_acc_usecs(gvar_sm, id, elapsed);
		ipsc_acc_writes(gvar_sm,id, 1);
	}

	if(retval < 0) {
		fprintf(stderr, "Write fail\n");
		exit(EXIT_FAILURE);
	}

	return retval;
}

IpcFileWriter::IpcFileWriter(int mtype_)
{
	id = mtype_;
}

IpcFileWriter::~IpcFileWriter()
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

