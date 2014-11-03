#ifndef __LOG_H
#define __LOG_H

typedef int MSG_LEVEL;

#define   DML_RUNTIME ((MSG_LEVEL) 0)
#define   DML_ERROR   ((MSG_LEVEL) 1)
#define   DML_WARN    ((MSG_LEVEL) 2)
#define   DML_DEBUG   ((MSG_LEVEL) 3)
#define   DML_INFO    ((MSG_LEVEL) 4)

void log(MSG_LEVEL level, const char *fmt, ...);

#endif


