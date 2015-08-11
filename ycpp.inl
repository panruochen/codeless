#ifndef __YCPP_INL
#define __YCPP_INL
#include <assert.h>

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
{  return *included_files.top(); }

inline const CC_STRING& CYcpp::GetCurrentFileName()
{  return included_files.top()->ifile->name; }

inline const size_t CYcpp::GetCurrentLineNumber()
{  return included_files.top()->ifile->line; }

inline void CYcpp::PushIncludedFile(CFile *srcfile, FILE *of, size_t np, size_t nh)
{
	included_files.push(new CYcpp::TIncludedFile(srcfile, of, np, nh));
}


inline CYcpp::TIncludedFile *CYcpp::PopIncludedFile()
{
	CYcpp::TIncludedFile *ifile = NULL;
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


inline void CYcpp::TIncludedFile::TListEntry::Init()
{
	prev = this;
	next = this;
}

inline void CYcpp::TIncludedFile::TListEntry::AddAfter(TListEntry *x, TListEntry *y)
{
	CYcpp::TIncludedFile::TListEntry *z = x->next;

	x->next = y;
	y->prev = x;
	y->next = z;
	z->prev = y;
}

inline bool CYcpp::TIncludedFile::TListEntry::IsEmpty()
{
	return this == prev;
}

inline CYcpp::TIncludedFile::TListEntry *CYcpp::TIncludedFile::TListEntry::last()
{
	return prev;
}

inline void CYcpp::TIncludedFile::TListEntry::AddTail(TListEntry *entry)
{
	AddAfter(prev, entry);
}

inline void CYcpp::mark_comment_start()
{
	comment_start = raw_line.size() - 2;
}

inline CYcpp::CYcpp()
{
	tc = NULL;
}

inline void CYcpp::TIncludedFile::TCond::append(TCondChain *cc)
{
	sub_chains.AddTail(&cc->link);
	cc->superior = this;
}

inline CYcpp::TIncludedFile::TCond::TCond(TCondChain *superior, TCond::COND_TYPE _type, size_t ln) :
	type(_type), begin(ln)
{
	sub_chains.Init();
	end = INV_LN;
}

/*
 * +------------------------------------- +
 * |                                      |
 * +-> *----*     *----*         *----*   |
 *     |    |     |    |         |    |   |
 *     |    |     |    |         |    |   |
 *     |    | --> |    | ... --> |    | --+
 *     *----*     *----*         *----*
 */
inline CYcpp::TIncludedFile::TCondChain *CYcpp::TIncludedFile::TCond::superior()
{
	TListEntry *head = link.next;
	/* The head exactly points to the _chain_ member within the struct `TCondChain' */
	return container_of(head, TCondChain, chain);
}

inline CYcpp::TIncludedFile::TCondChain::TCondChain()
{
	chain.Init();
	true_cond = NULL;
	superior  = NULL;
}

inline void CYcpp::TIncludedFile::TCondChain::mark_end(size_t line_nr)
{
	TCond *c = (TCond*)chain.last();
	c->end = line_nr;
}

inline void CYcpp::TIncludedFile::TCondChain::add_if(TCond *c, bool cv)
{
	begin = c->begin;
	chain.AddTail(&c->link);
	if(cv)
		true_cond = c;
}

inline void CYcpp::TIncludedFile::TCondChain::add_elif(TCond *c, bool cv)
{
	mark_end(c->begin);
	chain.AddTail(&c->link);
	if(cv)
		true_cond = c;
}

inline void CYcpp::TIncludedFile::TCondChain::add_else(TCond *c, bool cv)
{
	mark_end(c->begin);
	chain.AddTail(&c->link);
	if(cv)
		true_cond = c;
}

inline void CYcpp::TIncludedFile::TCondChain::add_endif(size_t line_nr)
{
	mark_end(line_nr);
	end = line_nr;
}

inline void CYcpp::TIncludedFile::add_if(size_t line_nr, bool value)
{
	if(under_false())
		value = false;
	TCondChain *cc = new TCondChain();
	TCond *c = new TCond(cc, TCond::CT_IF, line_nr);
	cursor->append(cc);
	cc->add_if(c, value);
	cursor = c;
}

inline void CYcpp::TIncludedFile::add_elif(size_t line_nr, bool value)
{
	if(under_false())
		value = false;
	TCondChain *cc = cursor->superior();
	TCond *c = new TCond(cc, TCond::CT_ELIF, line_nr);
	cc->add_elif(c, value);
	cursor = c;
}

inline void CYcpp::TIncludedFile::add_else(size_t line_nr, bool value)
{
	if(under_false())
		value = false;
	TCondChain *cc = cursor->superior();
	TCond *c = new TCond(cc, TCond::CT_ELSE, line_nr);
	cc->add_else(c, value);
	cursor = c;
}

inline void CYcpp::TIncludedFile::add_endif(size_t line_nr)
{
#if 0
	if( strstr(ifile->name.c_str(), "config_defaults.h") && ifile->line == 27 )
	{
		volatile int loop = 1;
		do {} while (loop);
	}
#endif

	cursor->superior()->add_endif(line_nr);

	TCondChain *cc = cursor->superior();
	assert(!cc->chain.IsEmpty());
	cursor = (TCond*)cc->superior;
}

inline CYcpp::TIncludedFile::TIncludedFile(CFile *_ifile, FILE *_ofile, size_t _np, size_t _nh)
{
	ifile = _ifile;
	ofile = _ofile;
	np    = _np;
	nh    = _nh;
	virtual_root = new TCond(NULL, TCond::CT_ROOT, 0);
	virtual_root->end = 0;
	cursor = virtual_root;
}

inline bool CYcpp::TIncludedFile::under_false()
{
	return !(cursor == virtual_root || cursor->superior()->true_cond);
}

#endif


