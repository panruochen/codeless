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
    char *buf, __buf0[1024];
    va_list ap;
    size_t n;
    int rc;

    if((unsigned long)verb < (unsigned long)verb_gate)
        return;
    if(log_fd == -1) {
        log_fd = open(log_file, O_CREAT|O_WRONLY|O_TRUNC, 0664);
        if(log_fd == -1)
            fatal(133, "Cannot open \"%s\": %s\n", log_file, strerror(errno));
    }

    buf = __buf0;
    va_start(ap, format);
    n = vsnprintf(__buf0, sizeof(__buf0), format, ap);
    va_end(ap);
    if(n >= sizeof(__buf0)) {
        buf = (char*)malloc(n + 4);
        n = vsnprintf(buf, n+4, format, ap);
    }

    rc = write(log_fd, buf, n);
    if(buf != __buf0)
        free(buf);
}



