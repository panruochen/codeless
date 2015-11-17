#ifndef __DEF_CONFIG_H
#define __DEF_CONFIG_H

#define DEF_RT_DIR   "/var/tmp"
#define DEF_SVR_ADDR "cl-server"

static inline const char *getopt_default(const char *opt, const char *defval)
{   return  (!opt || !*opt) ? defval : opt; }

#endif

