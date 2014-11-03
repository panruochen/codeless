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
MSG_LEVEL log_gate_level;
void fatal(int exit_status, const char *fmt, ...);

void log(MSG_LEVEL level, const char *format, ...)
{
    char *buf, __buf0[1024];
    va_list ap;
    size_t n;
    int rc;

    if((int)level > (int)log_gate_level)
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



