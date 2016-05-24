#ifndef __LOG_H
#define __LOG_H

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
	LOGV_INFO = 1,
	LOGV_DEBUG,
	LOGV_WARN,
	LOGV_ERROR,
	LOGV_RUNTIME,
}   LOG_VERB;

/* The number is high, the verbosity is more significant. */
//#define   LOGV_INFO    ((int) 1)
//#define   LOGV_DEBUG   ((int) 2)
//#define   LOGV_WARN    ((int) 3)
//#define   LOGV_ERROR   ((int) 4)
//#define   LOGV_MAX     ((int) 5)
//#define   LOGV_RUNTIME LOGV_MAX                 /* Runtime messages are always output */
void log(LOG_VERB verb, const char *fmt, ...);
void set_log_verbosity(LOG_VERB verb);

#ifdef __cplusplus
}
#endif

#endif

