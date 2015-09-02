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

inline CYcpp::CIncludedFile& CYcpp::GetCurrentFile()
{  return *included_files.top(); }

inline const CC_STRING& CYcpp::GetCurrentFileName()
{  return included_files.top()->ifile->name; }

inline const size_t CYcpp::GetCurrentLineNumber()
{  return included_files.top()->ifile->line; }

inline void CYcpp::PushIncludedFile(CFile *srcfile, FILE *of, size_t np, bool in_compiler_dir, size_t nh)
{
	included_files.push(new CYcpp::CIncludedFile(srcfile, of, np, in_compiler_dir, nh));
}

inline CYcpp::CIncludedFile *CYcpp::PopIncludedFile()
{
	CYcpp::CIncludedFile *ifile = NULL;
	included_files.pop(ifile);
	return ifile;
}

inline bool CYcpp::has_dep_file(void)
{
	return rtc && rtc->of_dep.size() != 0;
}

inline CYcpp::CConditionalChain::CConditionalChain()
{
	value = TSV_X;
	stage = SG_ON_NONE;
	flags = 0;
}

inline void CYcpp::CConditionalChain::enter_if(TRI_STATE value)
{
	this->value = value;
	switch(value) {
	case TSV_1:
		flags |= FG_TRUE_ON_IF;
		break;
	case TSV_X:
		flags |= FG_HAS_X;
		break;
	default:
		break;
	}
	stage = SG_ON_IF;
}

inline void CYcpp::CConditionalChain::enter_elif(TRI_STATE value)
{
	this->value = value;
	switch(value) {
	case TSV_1:
		flags |= FG_TRUE_ON_ELIF;
		break;
	case TSV_X:
		flags |= FG_HAS_X;
		break;
	default:
		break;
	}
	stage = SG_ON_ELIF;
}

inline TRI_STATE CYcpp::CConditionalChain::enter_else()
{
	if( flags & (FG_TRUE_ON_IF|FG_TRUE_ON_ELIF) )
		value = TSV_0;
	else if(value != TSV_X)
		value = (TRI_STATE) ! value;
	else
		value = TSV_X;
	stage = SG_ON_ELSE;
	return value;
}

inline bool CYcpp::CConditionalChain::keep_endif()
{
	return flags & FG_HAS_X;
}

inline bool CYcpp::CConditionalChain::has_true()
{
	return flags & (FG_TRUE_ON_IF|FG_TRUE_ON_ELIF);
}

inline TRI_STATE CYcpp::CConditionalChain::eval_condition()
{
	return value;
}

inline TRI_STATE CYcpp::eval_current_conditional()
{
	return conditionals.size() == 0 ? TSV_1 : conditionals.top()->eval_condition();
}

inline CYcpp::CConditionalChain *CYcpp::upper_chain()
{
	return conditionals.size() == 0 ? NULL : conditionals.top();
}

inline void CYcpp::CIncludedFile::CListEntry::Init()
{
	prev = this;
	next = this;
}

inline void CYcpp::CIncludedFile::CListEntry::AddAfter(CListEntry *x, CListEntry *y)
{
	CYcpp::CIncludedFile::CListEntry *z = x->next;

	x->next = y;
	y->prev = x;
	y->next = z;
	z->prev = y;
}

inline bool CYcpp::CIncludedFile::CListEntry::IsEmpty()
{
	return this == prev;
}

inline CYcpp::CIncludedFile::CListEntry *CYcpp::CIncludedFile::CListEntry::last()
{
	return prev;
}

inline void CYcpp::CIncludedFile::CListEntry::AddTail(CListEntry *entry)
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

inline void CYcpp::CIncludedFile::CCond::append(CCondChain *cc)
{
	sub_chains.AddTail(&cc->link);
	cc->superior = this;
}

inline CYcpp::CIncludedFile::CCond::CCond(CCondChain *head_, CCond::COND_TYPE type_, bool value_, uint32_t line_, const char *filename_) :
	type(type_), begin(line_)
{
#if SANITY_CHECK
	memcpy(tag, TAG, sizeof(TAG));
#endif
	value = value_;
	sub_chains.Init();
	end = INV_LN;
	head = head_;
	filename = filename_;
}

inline void CYcpp::CIncludedFile::CCond::sanity_check()
{
#if SANITY_CHECK
	assert(memcmp(tag, TAG, sizeof(TAG)) == 0);
#endif
}

inline CYcpp::CIncludedFile::CCondChain::CCondChain()
{
#if SANITY_CHECK
	memcpy(tag, TAG, sizeof(TAG));
#endif
	chain.Init();
	superior  = NULL;
}

inline void CYcpp::CIncludedFile::CCondChain::sanity_check()
{
#if SANITY_CHECK
	assert(memcmp(tag, TAG, sizeof(TAG)) == 0);
#endif
}

inline void CYcpp::CIncludedFile::CCondChain::mark_end(linenum_t line_nr)
{
	CCond *c = container_of(chain.last(), CCond, link);
	c->sanity_check();
	c->end = line_nr;
}

inline void CYcpp::CIncludedFile::CCondChain::add_if(CCond *c)
{
	begin = c->begin;
	chain.AddTail(&c->link);
}

inline void CYcpp::CIncludedFile::CCondChain::add_elif(CCond *c)
{
	mark_end(c->begin - 1);
	chain.AddTail(&c->link);
}

inline void CYcpp::CIncludedFile::CCondChain::add_else(CCond *c)
{
	mark_end(c->begin - 1);
	chain.AddTail(&c->link);
}

inline void CYcpp::CIncludedFile::CCondChain::add_endif(linenum_t line_nr)
{
	mark_end(line_nr);
	end = line_nr;
}

inline void CYcpp::CIncludedFile::add_if(bool value)
{
	if(false_level > 0 || !cursor->value) {
		false_level++;
		return;
	}
	CCondChain *cc = new CCondChain();
	CCond *c = new CCond(cc, CCond::CT_IF, value, ifile->line, ifile->name.c_str());
	cursor->append(cc);
	cc->add_if(c);
	assert(cursor == c->head->superior);
	cursor = c;
}

inline void CYcpp::CIncludedFile::add_elif(bool value)
{
	if(false_level)
		return;
	CCondChain *cc = cursor->head;
	CCond *c = new CCond(cc, CCond::CT_ELIF, value, ifile->line, ifile->name.c_str());
	cc->add_elif(c);
	cursor = c;
}

inline void CYcpp::CIncludedFile::add_else(bool value)
{
//	GDB_TRAP2(strstr(ifile->name.c_str(), "/asm/elf.h"), value);
	if(false_level)
		return;
	CCondChain *cc = cursor->head;
	CCond *c = new CCond(cc, CCond::CT_ELSE, value, ifile->line, ifile->name.c_str());
	cc->add_else(c);
	cursor = c;
}

inline void CYcpp::CIncludedFile::add_endif()
{
	if(false_level > 0) {
		false_level--;
		return;
	}
	CCondChain *cc = cursor->head;
	cc->add_endif(ifile->line);
#if SANITY_CHECK
	assert(!cc->chain.IsEmpty());
#endif
	cursor = cc->superior;
}

inline CYcpp::CIncludedFile::CIncludedFile(CFile *ifile_, FILE *ofile_, size_t np_, bool in_compiler_dir_, size_t nh_)
{
	ifile = ifile_;
	ofile = ofile_;
	np    = np_;
	nh    = nh_;
	in_compiler_dir = in_compiler_dir_;
	virtual_root = new CCond(NULL, CCond::CT_ROOT, true, 0, NULL);
	virtual_root->end = 0;
	cursor = virtual_root;
	false_level = 0;
}

#endif


