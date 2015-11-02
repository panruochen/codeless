#ifndef __EXCEPTION_INL
#define __EXCPETION_INL

inline Exception::Exception(void)
{
}

inline Exception::Exception(const char *msg)
{
	this->msg = msg;
}

inline Exception::Exception(const CC_STRING& msg)
{
	this->msg = msg;
}

inline const char *Exception::GetError()
{
	return msg.c_str();
}

inline Exception& Exception::operator = (const char *msg)
{
	this->msg = msg;
	return *this;
}

inline Exception& Exception::operator = (const CC_STRING& msg)
{
	this->msg = msg;
	return *this;
}

inline void Exception::format(const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	msg.format(fmt, ap);
	va_end(ap);
}

inline void Exception::AddPrefix(const CC_STRING& prefix)
{
	msg = prefix + msg;
}

#endif

