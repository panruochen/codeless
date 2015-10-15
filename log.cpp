/*
 *  A runtime log library.
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <stdarg.h>
#include <errno.h>
#include "log.h"

const char *log_file = "/var/exfat.log";
static int log_fd = 2;
static LOG_VERB verb_gate = LOGV_RUNTIME;  /* A gate for the log messages. Only message with more significant
											  verbosity than the gate value would be output. */
void fatal(int exit_status, const char *fmt, ...);

void set_log_verbosity(LOG_VERB verb)
{
	int tmp = (int) LOGV_RUNTIME - (int) verb;

	if(tmp < 0)
		tmp = 0;
	else if(tmp > (int)LOGV_RUNTIME)
		tmp = (int) LOGV_RUNTIME;
	verb_gate = (LOG_VERB) tmp;
}

void log(LOG_VERB verb, const char *format, ...)
{
    char *buf, lbuf[1024];
    va_list ap;
    size_t len;

    if((unsigned long)verb < (unsigned long)verb_gate)
        return;
    if(log_fd == -1) {
        log_fd = open(log_file, O_CREAT|O_WRONLY|O_TRUNC, 0664);
        if(log_fd == -1)
            fatal(133, "Cannot open \"%s\": %s\n", log_file, strerror(errno));
    }

    buf = lbuf;
    va_start(ap, format);
    len = vsnprintf(buf, sizeof(lbuf), format, ap);
    va_end(ap);

	/* buffer is not large enough, malloc and do it again */
    if(len >= sizeof(lbuf)) {
		const size_t buf_sz = len + 1;
        buf = (char*)malloc(buf_sz);
		va_start(ap, format);
		len = vsnprintf(buf, buf_sz, format, ap);
		va_end(ap);
    }

    write(log_fd, buf, len);
    if(buf != lbuf)
        free(buf);
}

