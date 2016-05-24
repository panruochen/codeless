#include <stdarg.h>
#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#include "cc_string.h"

void CC_STRING::Format(const char *fmt, va_list ap)
{
	char __buf0[1];
	size_t n;
	va_list aq;

	va_copy(aq, ap);
    n = vsnprintf(__buf0, sizeof(__buf0), fmt, ap);

	if(n >= sizeof(__buf0)) {
		resize(n);
		reserve(n + 1);
		vsnprintf(&std::string::operator[](0), n+1, fmt, aq);
	} else
		std::string::clear();
	va_end(aq);
}


void CC_STRING::Format(const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	Format(fmt, ap);
	va_end(ap);
}

