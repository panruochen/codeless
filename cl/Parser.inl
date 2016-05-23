#ifndef __PARSER_INL
#define __PARSER_INL
#include <assert.h>

inline Parser::IncludedFile& Parser::GetCurrentFile()
{  return *included_files.top(); }

inline const CC_STRING& Parser::GetCurrentFileName()
{  return included_files.top()->ifile->name; }

inline const size_t Parser::GetCurrentLineNumber()
{  return included_files.top()->ifile->line; }

inline void Parser::PushIncludedFile(File *srcfile, FILE *of, size_t np, bool in_compiler_dir, size_t nh)
{
	included_files.push(new Parser::IncludedFile(srcfile, of, np, in_compiler_dir, nh));
}

inline Parser::IncludedFile *Parser::PopIncludedFile()
{
	Parser::IncludedFile *ifile = NULL;
	included_files.pop(ifile);
	return ifile;
}

inline bool Parser::has_dep_file(void)
{
	return rtc && !rtc->of_array[MSGT_DEP].isnull();
}

inline const char *Parser::GetError()
{
	return errmsg.c_str();
}

inline Parser::CConditionalChain::CConditionalChain()
{
	value = TSV_X;
	stage = SG_ON_NONE;
	flags = 0;
}

inline void Parser::CConditionalChain::enter_if(TRI_STATE value)
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

inline void Parser::CConditionalChain::enter_elif(TRI_STATE value)
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

inline TRI_STATE Parser::CConditionalChain::enter_else()
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

inline bool Parser::CConditionalChain::keep_endif()
{
	return flags & FG_HAS_X;
}

inline bool Parser::CConditionalChain::has_true()
{
	return flags & (FG_TRUE_ON_IF|FG_TRUE_ON_ELIF);
}

inline TRI_STATE Parser::CConditionalChain::eval_condition()
{
	return value;
}

inline TRI_STATE Parser::eval_current_conditional()
{
	return conditionals.size() == 0 ? TSV_1 : conditionals.top()->eval_condition();
}

inline Parser::CConditionalChain *Parser::upper_chain()
{
	return conditionals.size() == 0 ? NULL : conditionals.top();
}

inline void Parser::IncludedFile::CListEntry::Init()
{
	prev = this;
	next = this;
}

inline void Parser::IncludedFile::CListEntry::AddAfter(CListEntry *x, CListEntry *y)
{
	Parser::IncludedFile::CListEntry *z = x->next;

	x->next = y;
	y->prev = x;
	y->next = z;
	z->prev = y;
}

inline bool Parser::IncludedFile::CListEntry::IsEmpty()
{
	return this == prev;
}

inline Parser::IncludedFile::CListEntry *Parser::IncludedFile::CListEntry::last()
{
	return prev;
}

inline void Parser::IncludedFile::CListEntry::AddTail(CListEntry *entry)
{
	AddAfter(prev, entry);
}

inline void Parser::mark_comment_start()
{
	pline.comment_start = pline.from.size() - 2;
}

inline Parser::Parser()
{
	intab = NULL;
}

inline void Parser::IncludedFile::Cond::append(CondChain *cc)
{
	sub_chains.AddTail(&cc->link);
	cc->superior = this;
}

inline Parser::IncludedFile::Cond::Cond(CondChain *head_, Cond::COND_TYPE type_, bool value_,
	uint32_t line_, int offset, const char *filename_) :
	type(type_), begin(line_), boff(offset)
{
#if SANITY_CHECK
	memcpy(tag, TAG, sizeof(TAG));
#endif
	value = value_;
	sub_chains.Init();
	end  = INV_LN;
	eoff = 0;
	head = head_;
	filename = filename_;
}

inline void Parser::IncludedFile::Cond::sanity_check()
{
#if SANITY_CHECK
	assert(memcmp(tag, TAG, sizeof(TAG)) == 0);
#endif
}

inline Parser::IncludedFile::CondChain::CondChain()
{
#if SANITY_CHECK
	memcpy(tag, TAG, sizeof(TAG));
#endif
	chain.Init();
	superior  = NULL;
	begin = 0;
	end   = 0;
}

inline void Parser::IncludedFile::CondChain::sanity_check()
{
#if SANITY_CHECK
	assert(memcmp(tag, TAG, sizeof(TAG)) == 0);
#endif
}

inline void Parser::IncludedFile::CondChain::mark_end(linenum_t line_nr)
{
	Cond *c = container_of(chain.last(), Cond, link);
	c->sanity_check();
	c->end = line_nr;
}

inline void Parser::IncludedFile::CondChain::add_if(Cond *c)
{
	begin = c->begin;
	chain.AddTail(&c->link);
}

inline void Parser::IncludedFile::CondChain::add_elif(Cond *c)
{
	mark_end(c->begin - 1);
	chain.AddTail(&c->link);
}

inline void Parser::IncludedFile::CondChain::add_else(Cond *c)
{
	mark_end(c->begin - 1);
	chain.AddTail(&c->link);
}

inline void Parser::IncludedFile::CondChain::add_endif(linenum_t line_nr)
{
	mark_end(line_nr);
	end  = line_nr;
}

inline void Parser::IncludedFile::add_if(bool value)
{
	if(false_level > 0 || !cursor->value) {
		false_level++;
		return;
	}
	CondChain *cc = new CondChain();
	Cond *c = new Cond(cc, Cond::CT_IF, value, ifile->line, ifile->offset, ifile->name.c_str());
	cursor->append(cc);
	cc->add_if(c);
	assert(cursor == c->head->superior);
	cursor = c;
}

inline void Parser::IncludedFile::add_elif(bool value)
{
	if(false_level)
		return;
	CondChain *cc = cursor->head;
	Cond *c = new Cond(cc, Cond::CT_ELIF, value, ifile->line, ifile->offset, ifile->name.c_str());
	cc->add_elif(c);
	cursor = c;
}

inline void Parser::IncludedFile::add_else(bool value)
{
//	GDB_TRAP2(strstr(ifile->name.c_str(), "/asm/elf.h"), value);
	if(false_level)
		return;
	CondChain *cc = cursor->head;
	Cond *c = new Cond(cc, Cond::CT_ELSE, value, ifile->line, ifile->offset, ifile->name.c_str());
	cc->add_else(c);
	cursor = c;
}

inline void Parser::IncludedFile::add_endif()
{
	if(false_level > 0) {
		false_level--;
		return;
	}
	CondChain *cc = cursor->head;
	cursor->eoff = ifile->offset;
	cc->add_endif(ifile->line);
#if SANITY_CHECK
	assert(!cc->chain.IsEmpty());
#endif
	cursor = cc->superior;
}

inline Parser::IncludedFile::IncludedFile(File *ifile_, FILE *ofile_, size_t np_, bool in_compiler_dir_, size_t nh_)
{
	ifile = ifile_;
	ofile = ofile_;
	np    = np_;
	nh    = nh_;
	in_compiler_dir = in_compiler_dir_;
	virtual_root = new Cond(NULL, Cond::CT_ROOT, true, 0, 0, NULL);
	virtual_root->end = 0;
	cursor = virtual_root;
	false_level = 0;
}

#endif


