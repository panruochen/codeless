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

inline CYcpp::TIncludedFile& CYcpp::GetCurrentFile()
{  return included_files.top(); }

inline const CC_STRING& CYcpp::GetCurrentFileName()
{  return included_files.top().srcfile->name; }

inline const size_t CYcpp::GetCurrentLineNumber()
{  return included_files.top().srcfile->line; }

inline void CYcpp::PushIncludedFile(CFile *srcfile, FILE *of, size_t np, size_t nh)
{
	CYcpp::TIncludedFile ifile(srcfile, of, np, nh);
	included_files.push(ifile);
}


inline CYcpp::TIncludedFile CYcpp::PopIncludedFile()
{
	CYcpp::TIncludedFile ifile;
	included_files.pop(ifile);
	return ifile;
}

inline bool CYcpp::has_dep_file(void)
{
	return rtctx && rtctx->save_depfile.size() != 0;
}

inline CYcpp::CConditionalChain::CConditionalChain()
{
	value = TSV_X;
	stage = SG_ON_NONE;
	flags = 0;
}

inline void CYcpp::CConditionalChain::enter_if_branch(TRI_STATE v)
{
	value = v;
	switch(value) {
	case TSV_1:
		flags |= FG_TRUE_ON_IF;
		break;
	case TSV_X:
		flags |= FG_TRUE_ON_IF;
		break;
	default:
		break;
	}
	stage = SG_ON_IF;
}

inline void CYcpp::CConditionalChain::enter_elif_branch(TRI_STATE v)
{
	value = v;
	switch(value) {
	case TSV_1:
		flags |= FG_TRUE_ON_ELIF;
		break;
	case TSV_X:
		flags |= FG_TRUE_ON_IF;
		break;
	default:
		break;
	}
	stage = SG_ON_ELIF;
}

inline void CYcpp::CConditionalChain::enter_else_branch()
{
	if( flags & (FG_TRUE_ON_IF|FG_TRUE_ON_ELIF) )
		value = TSV_0;
	else if(value != TSV_X)
		value = (TRI_STATE) ! value;
	else
		value = TSV_X;
	stage = SG_ON_ELSE;
}

inline bool CYcpp::CConditionalChain::keep_endif()
{
	return flags & FG_HAS_X;
}


inline TRI_STATE CYcpp::CConditionalChain::eval_condition()
{
	return value;
}

inline TRI_STATE CYcpp::eval_current_condition()
{
	return conditionals.size() == 0 ? TSV_1 : conditionals.top()->eval_condition();
}

inline CYcpp::TIncludedFile::TIncludedFile(CFile *s, FILE *of, size_t np, size_t nh)
{
	srcfile = s;
	outfp   = of;
	this->np = np;
	this->nh = nh;
}

inline CYcpp::TIncludedFile::TIncludedFile()
{
	srcfile = NULL;
	outfp   = NULL;
	np = 0;
	nh = 0;
}

inline void CYcpp::mark_comment_start()
{
	comment_start = raw_line.size() - 2;
}

inline CYcpp::CYcpp()
{
	tc = NULL;
}

#endif

