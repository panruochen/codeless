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
{  return current_level.srcfile; }


inline void Cycpp::include_level_push(CFile *srcfile, FILE *outfile, size_t if_level)
{
	Cycpp::INCLUDE_LEVEL lvl(srcfile,outfile,if_level);
	current_level = lvl;
	include_levels.push(lvl);
}


inline Cycpp::INCLUDE_LEVEL Cycpp::include_level_pop()
{
	include_levels.pop(current_level);
	return current_level;
}

#endif

