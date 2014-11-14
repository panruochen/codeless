#ifndef __PXCC_INL
#define __PXCC_INL

inline CException::CException(void)
{
}

inline CException::CException(const char *msg)
{
	this->msg = msg;
}

inline CException::CException(const CC_STRING& msg)
{
	this->msg = msg;
}


inline const char *CException::GetError()
{
	return msg.c_str();
}
	
inline CException& CException::operator = (const char *msg)
{
	this->msg = msg;
	return *this;
}
	
inline CException& CException::operator = (const CC_STRING& msg)
{
	this->msg = msg;
	return *this;
}

inline void CException::format(const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	msg.format(fmt, ap);
	va_end(ap);
}


inline CFile *Cycpp::current_file()
{  return include_levels.top().srcfile; }


inline void Cycpp::include_level_push(CFile *srcfile, FILE *of, size_t if_level)
{
	Cycpp::INCLUDE_LEVEL lvl(srcfile,of,if_level);
	include_levels.push(lvl);
}


inline Cycpp::INCLUDE_LEVEL Cycpp::include_level_pop()
{
	Cycpp::INCLUDE_LEVEL lvl;
	include_levels.pop(lvl);
	return lvl;
}

#endif

