/*
 *  A multi-process safe file operation library.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/file.h>

#include <stdarg.h>
#include <assert.h>
#include <errno.h>
#include <libgen.h>

#include "Fol.h"
#include "utils.h"

int fol_fdappend(int fd, const void *buf, off_t n)
{
	struct flock flock;
	off_t offset;

	offset = lseek(fd, 0, SEEK_END);
	flock.l_type   = F_WRLCK;
	flock.l_whence = SEEK_SET;
	flock.l_start  = offset;
	flock.l_len    = n;
	flock.l_pid    = 0;
	if( fcntl(fd, F_SETLKW, &flock) == 0 ) {
		write(fd, buf, n);
		flock.l_type = F_UNLCK;
		fcntl(fd, F_SETLKW, &flock);
		return 0;
	} else {
		fatal(130, "File system error %s\n", strerror(errno));
	}
	return -1;
}

void fol_append(const CC_STRING& filename, const void *buf, size_t n)
{
	int fd;
	fd = open(filename.c_str(), O_CREAT|O_BINARY|O_WRONLY|O_APPEND, 0664);
	fol_fdappend(fd, buf, n);
	close(fd);
}

void fol_write(const CC_STRING& filename, const void *buf, size_t n)
{
	int fd;

	fd = open(filename.c_str(), O_CREAT|O_BINARY|O_WRONLY|O_TRUNC, 0664);
	flock(fd, LOCK_EX);
	(void) write(fd, buf, n);
	flock(fd, LOCK_UN);
	close(fd);
}

FILE *fol_afopen(const CC_STRING& filename)
{
	struct stat stbuf;
	const char *mode ;

	if(filename.isnull())
		return NULL;
	if(stat(filename, &stbuf) == 0)
		mode = "ab+";
	else
		mode = "wb";
	return fopen(filename, mode);
}

bool fol_exist(const CC_STRING& filename)
{
	struct stat stb;
	return stat(filename, &stb) == 0;
}


CC_STRING fol_dirname(const CC_STRING& path)
{
	char *tmp;
	CC_STRING dir;

	tmp = strdup(path.c_str());
	dir = dirname(tmp);
	free(tmp);
	return dir;
}

#include "realpath.h"
CC_STRING fol_realpath(const CC_STRING& src)
{
    char dest[4096];

	if( ! y_realpath(dest, sizeof(dest), src.c_str()) )
		return CC_STRING("");
	return CC_STRING(dest);
}

#if 0
int fol_mkdir(const CC_STRING& dirname)
{
    CC_STRING newdir;
	char *p, *dir;
	struct stat stb;
	int retval = 0;

	if( dirname[0] != '/' ) {
        newdir = fol_realpath(dirname);
		if( newdir.isnull() ) {
			retval = -EINVAL;
			goto fail;
		}
	} else
		newdir = dirname.c_str();

	dir = (char*)newdir.c_str();
	if(newdir[0] == '/')
		p = (char*)newdir.c_str() + 1;
	else
		p = (char*)newdir.c_str();
	while(1) {
		char c = *p;
		if(c == 0 || c == '/') {
			const char saved_char = *p;
			*p = '\0';
			if(stat(dir, &stb) != 0) {
				if ( mkdir(dir, 0775) != 0 ) {
					retval = -errno;
					goto fail;
				}
			} else if( ! S_ISDIR(stb.st_mode) ) {
				retval = -EEXIST;
				goto fail;
			}
			if(c == '\0')
				break;
			*p = saved_char;
		}
		p ++;
	}

fail:
	return retval;
}

int fol_copy_with_parent(const CC_STRING& src, const CC_STRING& dst)
{
	CC_STRING ddir;
	const char *pos;
	int retval;

	pos = strrchr(dst.c_str(), '/');
	if(pos != NULL) {
		ddir.strcat(dst.c_str(), pos);
		retval = fol_mkdir(ddir);
		if( retval != 0)
			return retval;
	}
	return fol_copy(src, dst);
}

int fol_copy(const CC_STRING& src, const CC_STRING& dst)
{
	int fs, fd, last_errno = 0;
	static char buf[512];
	const int chunksize = sizeof(buf);

	fs = open(src.c_str(), O_RDONLY, 0666);
	if(fs < 0) {
		last_errno = errno;
		goto error;
	}
	fd = open(dst.c_str(), O_WRONLY|O_CREAT|O_TRUNC, 0666);
	if(fd < 0) {
		last_errno = errno;
		goto error;
	}
	if(fs > 0 && fd > 0) {
		int n1, n2;
		do {
			n1 = read(fs, buf, chunksize);
			n2 = write(fd, buf, n1);
			if(n1 != n2) {
				last_errno = errno;
				break;
			}
		} while(n2 == chunksize);
		close(fs);
		close(fd);
	}
error:
	return - last_errno;
}

#endif
