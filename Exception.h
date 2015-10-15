#ifndef __EXCEPTION_H
#define __EXCEPTION_H

class Exception {
protected:
	CC_STRING  msg;

public:
    inline Exception(void);
    inline Exception(const char *msg);
    inline Exception(const CC_STRING& msg);
	inline const char *GetError();
	inline Exception& operator = (const char *msg);
	inline Exception& operator = (const CC_STRING& msg);
    inline void format(const char *fmt, ...);
	inline void AddPrefix(const CC_STRING&);
};

#include "Exception.inl"

#endif
