#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <ctype.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <utime.h>

#include <fcntl.h>
#include <semaphore.h>

#include "Parser.h"
#include "GlobalVars.h"
#include "Utilities.h"
#include "misc.h"

static bool IsOperator(sym_t sym)
{
	return sym < SSID_SYMBOL_X;
}

static bool CheckPrevOperator(sym_t id)
{
	switch(id) {
	case  SSID_LOGIC_OR:
	case  SSID_LOGIC_AND:
	case  SSID_LOGIC_NOT:
	case  SSID_BITWISE_OR:
	case  SSID_BITWISE_XOR:
	case  SSID_BITWISE_AND:
	case  SSID_EQUAL:
	case  SSID_NOT_EQUAL:
	case  SSID_LESS:
	case  SSID_GREATER:
	case  SSID_LESS_EQUAL:
	case  SSID_GREATER_EQUAL:
	case  SSID_LEFT_SHIFT:
	case  SSID_RIGHT_SHIFT:
	case  SSID_ADDITION:
	case  SSID_SUBTRACTION:
	case  SSID_MULTIPLICATION:
	case  SSID_DIVISION:
	case  SSID_PERCENT:
	case  SSID_SHARP:
	case  SSID_LEFT_PARENTHESIS:
		return true;
	}
	return false;
}


static inline TRI_STATE operator ! (TRI_STATE val)
{
	return (val == TSV_X) ? TSV_X :
		(val == TSV_0 ? TSV_1 : TSV_0);
}

//
// Get the include file name without surrouding angle brackets or double quoation marks
//
static CC_STRING GetIncludeFileName(const CC_STRING& line, bool& quoted)
{
	const char *p1, *p2;
	CC_STRING filename;
	char style = '\0';

	p1 = line.c_str();
	skip_blanks(p1);

	if( *p1 == '"')
		style = '"';
	else if( *p1 == '<' )
		style = '>';
	else
		goto error;
	p1++;
	skip_blanks(p1);
	p2 = p1 + strlen(p1) - 1;
	while( *p2 == '\t' || *p2 == ' ' || *p2 == '\r' || *p2 == '\n') {
		if(--p2 < p1)
			goto error;
	}
	if( *p2 != style )
		goto error;
	p2--;
	while( *p2 == '\t' || *p2 == ' ' ) {
		if(--p2 < p1)
			goto error;
	}
	filename = CC_STRING(p1, p2+1);
	quoted = (style == '"');

error:
	return filename;
}

static bool GetRealPath(const CC_STRING& name, CC_STRING& opath)
{
/*  Use my own function rather than realpath since realpath will expand symblic links */
	opath = fol_realpath(name);
	return ! opath.isnull() ;
}

static inline bool operator >= (const SynToken& a, const SynToken& b)
{
	if(a.attr == SynToken::TA_UINT && b.attr == SynToken::TA_UINT)
		return a.u32_val >= b.u32_val;
	if( (a.attr == SynToken::TA_UINT && b.attr == SynToken::TA_INT) ||
		(a.attr == SynToken::TA_INT && b.attr == SynToken::TA_UINT) ||
		(a.attr == SynToken::TA_INT && b.attr == SynToken::TA_INT))
		return a.i32_val >= b.i32_val;
	assert(0);
}

static inline bool operator > (const SynToken& a, const SynToken& b)
{
	if(a.attr == SynToken::TA_UINT && b.attr == SynToken::TA_UINT)
		return a.u32_val > b.u32_val;
	if( (a.attr == SynToken::TA_UINT && b.attr == SynToken::TA_INT) ||
		(a.attr == SynToken::TA_INT && b.attr == SynToken::TA_UINT) ||
		(a.attr == SynToken::TA_INT && b.attr == SynToken::TA_INT))
		return a.i32_val > b.i32_val;
	assert(0);
}


static inline bool operator == (const SynToken& a, uint32_t val)
{
	return a.u32_val == val;
}


static inline bool operator != (const SynToken& a, uint32_t val)
{
	return a.u32_val != val;
}

const char *Parser::TransToken(SynToken *tokp)
{
	if(tokp->attr == SynToken::TA_UINT ) {
		tokp->name.Format("%u", tokp->u32_val);
		return tokp->name.c_str();
	} else if(tokp->attr == SynToken::TA_INT ) {
		tokp->name.Format("%d", tokp->u32_val);
		return tokp->name.c_str();
	}
	return TR(intab, tokp->id);
}

bool Parser::DoCalculate(SynToken& opnd1, sym_t opr, SynToken& opnd2, SynToken& result)
{
	if( gvar_preprocess_mode ) {
		if(opnd1.attr == SynToken::TA_IDENT) {
			opnd1.attr    = SynToken::TA_UINT;
			opnd1.u32_val = 0;
		}
		if((opr != SSID_LOGIC_NOT && opr != SSID_BITWISE_NOT) && opnd2.attr == SynToken::TA_IDENT  ) {
			opnd2.attr    = SynToken::TA_UINT;
			opnd2.u32_val = 0;
		}
	} else {
		if(opnd1.attr == SynToken::TA_IDENT || opnd2.attr == SynToken::TA_IDENT) {
			result.attr = SynToken::TA_IDENT;
			return true;
		}
	}

	result.attr = SynToken::TA_UINT;
	switch(opr) {
	case SSID_LOGIC_AND:
		result.b_val = opnd1.b_val && opnd2.b_val;
		break;
	case SSID_LOGIC_OR:
		result.b_val = opnd1.b_val || opnd2.b_val;
		break;
	case SSID_LESS:
		result.b_val = opnd2 > opnd1;
		break;
	case SSID_LESS_EQUAL:
		result.b_val = opnd2 >= opnd1;
		break;
	case SSID_GREATER:
		result.b_val = opnd1 > opnd2;
		break;
	case SSID_GREATER_EQUAL:
		result.b_val = opnd1 >= opnd2;
		break;
	case SSID_EQUAL:
		result.b_val = opnd1.u32_val == opnd2.u32_val;
		break;
	case SSID_NOT_EQUAL:
		result.b_val = opnd1.u32_val != opnd2.u32_val;
		break;
	case SSID_LOGIC_NOT:
		result.b_val = ! opnd1.u32_val;
		break;
	case SSID_ADDITION:
		result.u32_val = opnd1.u32_val + opnd2.u32_val;
		break;
	case SSID_SUBTRACTION:
		result.u32_val = opnd1.u32_val - opnd2.u32_val;
		break;
	case SSID_MULTIPLICATION:
		result.u32_val = opnd1.u32_val * opnd2.u32_val;
		break;
	case SSID_DIVISION:
		if(opnd2.u32_val == 0) {
			errmsg = "divided by zero";
			goto error;
		}
		result.u32_val = opnd1.u32_val / opnd2.u32_val;
		break;
	case SSID_PERCENT:
		if(opnd2.u32_val == 0) {
			errmsg = "divided by zero";
			goto error;
		}
		result.u32_val = opnd1.u32_val % opnd2.u32_val;
		break;

	case SSID_BITWISE_AND:
		result.u32_val = opnd1.u32_val & opnd2.u32_val;
		break;
	case SSID_BITWISE_OR:
		result.u32_val = opnd1.u32_val | opnd2.u32_val;
		break;
	case SSID_BITWISE_XOR:
		result.u32_val = opnd1.u32_val ^ opnd2.u32_val;
		break;
	case SSID_BITWISE_NOT:
		result.u32_val = ~ opnd1.u32_val;
		break;

	case SSID_LEFT_SHIFT:
		result.u32_val = opnd1.u32_val << opnd2.u32_val;
		break;
	case SSID_RIGHT_SHIFT:
		result.u32_val = opnd1.u32_val >> opnd2.u32_val;
		break;
	default:
		errmsg.Format("Invalid operator %s", TR(intab,opr));
		goto error;
	}
	return true;

error:
	result.attr = SynToken::TA_IDENT;
	result.id   = SSID_SYMBOL_X;
	return false;
}

bool Parser::CalculateOnStackTop(sym_t opr, CC_STACK<SynToken>& opnd_stack, CC_STACK<sym_t>& opr_stack)
{
	SynToken a, b;
	SynToken result = { SSID_SYMBOL_X, SynToken::TA_UINT };

	if( opr == SSID_COLON ) {
		SynToken c;
		sym_t tmp_opr = SSID_INVALID;
		if(opnd_stack.size() < 3) {
			errmsg = "Not enough operands for `? :'";
			return false;
		}
		opr_stack.pop(tmp_opr);
		if(tmp_opr != SSID_QUESTION) {
			errmsg = "Invalid operator `:'";
			return false;
		}
		opnd_stack.pop(c);
		opnd_stack.pop(b);
		opnd_stack.pop(a);

		log(LOGV_DEBUG, "%s ? %s : %s\n", TransToken(&a), TransToken(&b), TransToken(&c));
		result = a.u32_val ? b : c;
		goto done;
	} else if( opr != SSID_LOGIC_NOT && opr != SSID_BITWISE_NOT ) {
		if( ! opnd_stack.pop(b) || ! opnd_stack.pop(a))
			goto error_no_operands;
		log(LOGV_DEBUG, "%s %s %s\n", TransToken(&a), TR(intab,opr), TransToken(&b));
	} else {
		if( ! opnd_stack.pop(a) )
			goto error_no_operands;
		log(LOGV_DEBUG, "! %s\n", TransToken(&a));
	}
	if( ! DoCalculate(a, opr, b, result) ) {
		return false;
	}
done:
	opnd_stack.push(result);
	return true;

error_no_operands:
	errmsg.Format("Not enough operands for opeator '%s'", TR(intab,opr));
	return false;
}

int Parser::CheckSymbolDefined(const char *line, bool reverse, SynToken *result)
{
	short attr = SynToken::TA_IDENT;
	int retval = TSV_X;
	SynMacro *minfo;
	const char *name = TR(intab, SSID_SYMBOL_X);
	ReadReq req(line);

	if( ReadToken(intab, &req, false) < 0 ) {
		errmsg = req.error;
		goto error;
	}
	minfo = intab->maLut.Lookup(req.token.id);
	if( ! gvar_preprocess_mode ) {
		if(minfo == NULL) {
			retval = TSV_X;
			attr   = SynToken::TA_IDENT;
		} else if(minfo == SynMacro::NotDef) {
			retval = reverse ? TSV_1 : TSV_0;
			attr   = SynToken::TA_UINT;
		} else {
			retval = reverse ? TSV_0 : TSV_1;
			attr   = SynToken::TA_UINT;
		}
	} else {
		attr    = SynToken::TA_UINT;
		retval  = (minfo == NULL) ? TSV_0 : TSV_1;
		retval ^= reverse;
	}
	name = retval ? "1" : "0";

error:
	if(result != NULL) {
		result->attr    = attr;
		result->u32_val = retval;
		result->id      = SSID_SYMBOL_X;
		result->name    = name;
	}
	if( gvar_preprocess_mode )
		assert(retval != TSV_X);
	return retval;
}

int Parser::Compute(const char *line)
{
	static const char opp[] = { '<', '=', '>', 'X'};
	enum {
		STAT_INIT,
		STAT_OPR1,
		STAT_OPR2,
		STAT_OPND,
		STAT_DEFINED,  /* defined    */
		STAT_DEFINED1, /* defined(   */
		STAT_DEFINED2, /* defined(X  */
	} state;
	CC_STACK<sym_t>  opr_stack;
	CC_STACK<SynToken>  opnd_stack;
	sym_t last_opr = SSID_SHARP;
	SynToken last_token;
	char sign;
	const char *symbol = NULL;
	LOG_VERB dml = LOGV_DEBUG;
	CC_STRING expansion;

	if(!gvar_preprocess_mode) {
		/*
		 *  Keep `#if 0' blocks which are usually user comments.
		 */
		char *copied_line = strdup(line);
		const char *first = strtok(copied_line, " \t");
		int ignore = 0;
		/* Simple code and work properly in most cases */
		if(first && first[0] == '0')
			ignore = 1;
		free(copied_line);
		if(ignore)
			return TSV_X;
	}

	expansion = ExpandLine(intab, false, line, errmsg);
	if( !errmsg.isnull() ) {
		return TSV_X;
	}

	ReadReq req(expansion.c_str());
	SynToken& token = req.token;
	opr_stack.push(SSID_SHARP);
	log(dml, "PUSH OPR: #\n");
	sign = 0;
	state = STAT_OPR1;
	while(1) {
		sym_t  opr;
		const char *last_pos = req.cp;
		int ret;

		ret = ReadToken(intab, &req, false);
		if( ret == 0 )
			break;
		else if( ret < 0 )
			goto error;

		if( token.attr == SynToken::TA_CHAR )
			token.attr = SynToken::TA_INT;
		if( sign != 0 ) {
			if(token.attr != SynToken::TA_UINT && token.id != SSID_DEFINED) {
				errmsg.Format("%s following %c", TR(intab,token.id), sign);
				goto error;
			}
			if(sign == '-') {
				token.attr = SynToken::TA_INT;
				token.i32_val = -token.i32_val;
			}
			sign = 0;
		} else if( (token.id == SSID_ADDITION || token.id == SSID_SUBTRACTION) &&  CheckPrevOperator(last_opr)) {
			sign = token.id == SSID_ADDITION ? '+' : '-';
			goto next;
		}

		switch(state) {
		case STAT_INIT:
			if(IsOperator(token.id))
				state = STAT_OPR1;
			else if(token.id == SSID_DEFINED)
				state = STAT_DEFINED;
			else if(token.attr == SynToken::TA_IDENT)
				state = STAT_OPND;
			break;
		case STAT_OPND:
			if(IsOperator(token.id))
				state = STAT_OPR1;
			else {
				errmsg = "Adjacent operands";
				goto error;
			}
			break;
		case STAT_OPR1:
			if(IsOperator(token.id))
				state = STAT_OPR2;
			else if(token.id == SSID_DEFINED)
				state = STAT_DEFINED;
			else
				state= STAT_INIT;
			break;
		case STAT_OPR2:
			if(IsOperator(token.id)) {
			} else if(token.id == SSID_DEFINED)
				state = STAT_DEFINED;
			else
				state = STAT_INIT;
			break;
		case STAT_DEFINED:
			if(token.id == SSID_LEFT_PARENTHESIS)
				state = STAT_DEFINED1;
			else if(token.attr == SynToken::TA_IDENT) {
				CheckSymbolDefined(last_pos, false, &token);
				if(!errmsg.isnull())
					goto error;
				state = STAT_INIT;
			}
			break;
		case STAT_DEFINED1:
			if(token.attr == SynToken::TA_IDENT) {
				state = STAT_DEFINED2;
				symbol = last_pos;
			} else {
				errmsg = "Syntax error: ";
				goto error;
			}
			break;
		case STAT_DEFINED2:
			if(token.id == SSID_RIGHT_PARENTHESIS) {
				CheckSymbolDefined(symbol, false, &token);
				if(!errmsg.isnull())
					goto error;
				state = STAT_INIT;
				opnd_stack.push(token);
				opr = token.id;
				goto next;
			} else {
				errmsg = "Unmatched (";
				goto error;
			}
			break;
		}
		if(state == STAT_DEFINED || state == STAT_DEFINED1 || state == STAT_DEFINED2)
			goto next;

		if( token.id == SynToken::TA_UINT || token.id == SynToken::TA_INT )
			log(dml, "Current: %u\n", token.u32_val);
		else
			log(dml, "Current: %s\n", TR(intab,token.id));
//		log(dml, "OPR  Stack: \n", opr_stack);
//		log(dml, "OPND Stack: \n", opnd_stack);
		opr = token.id;
		if( IsOperator(opr) ) {
			sym_t opr0;
			int result;
again:
			opr0 = opr_stack.top();
			result = intab->opMat.Compare(opr0, opr);
			log(dml, "Compare: %s %c %s\n", TR(intab,opr0),opp[1+result],TR(intab,opr));
			switch(result) {
			case _EQ:
				opr_stack.pop(opr0);
				break;
			case _GT:
				opr_stack.pop(opr0);
				if( ! CalculateOnStackTop(opr0, opnd_stack, opr_stack) ) {
					goto error;
				}
				goto again;
			case _LT:
				opr_stack.push(opr);
				break;
			case _XX:
				errmsg.Format("Cannot compare \"%s\" and \"%s\"", TR(intab,opr0), TR(intab,opr));
				log(LOGV_ERROR, "*ERROR* %s\n", errmsg.c_str());
				goto error;
			}
		} else {
			opnd_stack.push(token);
		}

	next:
		last_opr = opr;
	}

	do {
		sym_t opr0;
		int result;

		if( ! opr_stack.pop(opr0) )
			goto error;
		result = intab->opMat.Compare(opr0, SSID_SHARP);
		log(dml, "Compare: %s %c #\n", TR(intab,opr0),opp[result]);
		switch(result) {
		case _EQ:
			break;
		case _GT:
			if( ! CalculateOnStackTop(opr0, opnd_stack, opr_stack) ) {
				goto error;
			}
			break;
		default:
			log(LOGV_ERROR, "%s:%u: Bad expression\n", __func__,  __LINE__);
			errmsg = "[1] Bad expression";
			goto error;
		}
	} while( opr_stack.size() != 0 );

	if( opnd_stack.size() != 1 ) {
		log(LOGV_ERROR, "%s:%u: Bad expression\n", __func__,  __LINE__);
		errmsg = "[2] Bad expression";
		goto error;
	}

	if( opnd_stack.top().attr == SynToken::TA_IDENT ) {
		if( gvar_preprocess_mode ) {
			return TSV_0;
		}
		return TSV_X;
	}
	log(dml, "Numberic Value: %u\n", opnd_stack.top().i32_val);;
	return !!opnd_stack.top().i32_val;

error:
	log(LOGV_ERROR, "*Error* %s\n", errmsg.c_str());
	return gvar_preprocess_mode ? TSV_0 : TSV_X;
}

/*----------------------------------------------------------------------------------*/

#if SANITY_CHECK
const char Parser::IncludedFile::Cond::TAG[4] = "C\x0\x1";
const char Parser::IncludedFile::CondChain::TAG[4] = "CC\x0";
#endif

void Parser::IncludedFile::WalkThrough::enter_conditional(Cond *c)
{
	CListEntry *i, *j;

	rh++;
	if(method == MED_BF && on_conditional_callback)
		on_conditional_callback(context, c, rh);
	for(i = c->sub_chains.next; i != &c->sub_chains; i = j) {
		j = i->next;
		enter_conditional_chain(container_of(i, CondChain, link));
	}
	if(method == MED_DF && on_conditional_callback)
		on_conditional_callback(context, c, rh);
	rh--;
}


void Parser::IncludedFile::WalkThrough::dump_and_exit(CondChain *cc, const char *msg)
{
	CListEntry *i;
	for(i = cc->chain.next; i != &cc->chain; i = i->next) {
		Cond *c = container_of(i, Cond, link);
		log(LOGV_RUNTIME, "%s:%u: %u\n", c->filename, c->begin, c->value);
	}
	log(LOGV_RUNTIME, "%s\n", msg);
	exit(1);
}

void Parser::IncludedFile::WalkThrough::enter_conditional_chain(CondChain *cc)
{
	CListEntry *i, *j;
#if SANITY_CHECK
	unsigned sum = 0;
	unsigned has_else = 0;
#endif

	if(method == MED_BF && on_conditional_chain_callback)
		on_conditional_chain_callback(context, cc);
	for(i = cc->chain.next; i != &cc->chain; i = j) {
		j = i->next;
		Cond *c = container_of(i, Cond, link);
#if SANITY_CHECK
		sum += c->value;
		has_else += (c->type == Cond::CT_ELSE);
#endif
		c->sanity_check();
		enter_conditional(c);
	}
#if SANITY_CHECK
	char emsg[256] = {0x1} ;
	if(sum > 1)
		snprintf(emsg, sizeof(emsg), "Has %u true conditionals", sum);
	if(has_else && sum != 1)
		snprintf(emsg, sizeof(emsg), "A chain with else has %u true conditionals", sum);
	if(emsg[0] != '\x1')
		dump_and_exit(cc, emsg);
#endif
	if(method == MED_DF && on_conditional_chain_callback)
		on_conditional_chain_callback(context, cc);
}

Parser::IncludedFile::WalkThrough::WalkThrough(Cond *rc, WT_METHOD method_,
	ON_CONDITIONAL_CHAIN_CALLBACK callback1, ON_CONDITIONAL_CALLBACK callback2, void *context_ )
{
	method    = method_;
	context   = context_;
	on_conditional_chain_callback = callback1;
	on_conditional_callback = callback2;
	rh = -1; /* Make sure the virtual root at level 0 */
	enter_conditional(rc);
}

void Parser::IncludedFile::delete_conditional(void *context, Cond *c, int rh)
{
#if SANITY_CHECK
	if(c->end == INV_LN) {
		log(LOGV_RUNTIME, "Sanity check failed on %s:%u\n", dv_current_file, c->begin);
		exit(1);
	}
#endif
	(void)context;
	(void)rh;
	delete c;
}

void Parser::IncludedFile::delete_conditional_chain(void *context, CondChain *cc)
{
	(void)context;
	delete cc;
}

void Parser::IncludedFile::save_conditional(Cond *c, int rh)
{
	CC_STRING tmp;
	static const char *directives[] = {NULL, "if", "elif", "else"};

	assert(c->type <= Cond::CT_ELSE);
	if(directives[c->type]) {
		tmp.Format("  %-4u %s %s ", rh, directives[c->type], (c->value ? "true" : "false"));
		cr_text += tmp;

		if(c->boff)
			tmp.Format("%u,%d ", c->begin, c->boff);
		else
			tmp.Format("%u ", c->begin);
		cr_text += tmp;

		if(c->eoff)
			tmp.Format("%u,%d\n", c->end, c->eoff);
		else
			tmp.Format("%u\n", c->end);
		cr_text += tmp;
	}
}

void Parser::IncludedFile::save_conditional(void *context, Cond *c, int rh)
{
	((IncludedFile*)context)->save_conditional(c, rh);
}

void Parser::IncludedFile::produce_cr_text()
{
	if(!virtual_root->sub_chains.IsEmpty()) {
		CC_STRING apath;
		GetRealPath(ifile->name, apath);
		cr_text = apath;
		cr_text += "\n";
		WalkThrough(virtual_root, WalkThrough::MED_BF, NULL, save_conditional, this);
	}
}

Parser::IncludedFile::~IncludedFile()
{
	WalkThrough(virtual_root, WalkThrough::MED_DF, delete_conditional_chain, delete_conditional, NULL);
	delete ifile;
}

const char *Parser::preprocessors[] = {
	"#define", "#undef",
	"#if", "#ifdef", "#ifndef", "#elif", "#else", "#endif",
	"#include", "#include_next"
};

TRI_STATE Parser::superior_conditional_value(bool on_if)
{
	const size_t n = on_if ? 0 : 1;
	CConditionalChain **top;
	assert(conditionals.size() >= n);
	return conditionals.size() == n ? TSV_1 : (top = &(conditionals.top()) - n, (*top)->eval_condition());
}

CC_STRING Parser::do_elif(int mode)
{
	CC_STRING output;
	CC_STRING trailing;
	const char *p;

	if( pline.comment_start > 0 )
		trailing = pline.from.c_str() + pline.comment_start ;
	else {
		p = pline.from.c_str();
		while( *p != '\r' && *p != '\n' && *p != '\0') p++;
		trailing = p;
	}

	if(mode == 2)
		output = "#else";
	else if(mode == 1){
		p = pline.from.c_str();
		skip_blanks(p);
		p ++; /* # */
		skip_blanks(p);
		p += 4; /* elif */
		trailing = p;
		output = "#if";
	}
	output += trailing;
	return output;
}


void Parser::AddDependency(const char *prefix, const CC_STRING& filename)
{
	CC_STRING rp;

	if( GetRealPath(filename, rp) ) {
		deptext += prefix;
		deptext += rp;
		deptext += "\n";
	}
}

void Parser::SaveCondValInfo(const CC_STRING& s)
{
	if(writers[VCH_CV] && !s.isnull()) {
		ssize_t ret;
		ret = writers[VCH_CV]->Write(s.c_str(), s.size());
		if(ret < 0)
			exit(-EPIPE);
	}
}

void Parser::SaveDepInfo(const CC_STRING& s)
{
	if(writers[VCH_DEP] && !s.isnull()) {
		ssize_t ret;
		ret = writers[VCH_DEP]->Write(s.c_str(), s.size());
		if(ret < 0)
			exit(-EPIPE);
	}
}

static CC_STRING MakeSemaName(const CC_STRING& filename)
{
	CC_STRING sname;
	const char *p;
	char c;

	for(p = filename.c_str(); (c = *p) != 0; p++) {
		if(c == '/' || c == '\\')
			sname += '-';
		else if(c == '.')
			c += '@';
		else
			sname += c;
	}
	return sname;
}


bool Parser::do_define(const char *line)
{
	const char *p;
	CC_STRING word;
	sym_t mid;

	p = line;
	skip_blanks(p);
	if(*p == '\r' || *p == '\n' || *p == '\0') {
		errmsg = "no macro name given in #define directive";
		return false;
	}
	if( isalpha(*p) || *p == '_' ) {
		word = *p++;
		while( isalpha(*p) || *p == '_' || isdigit(*p) )
			word += *p++;
		mid = intab->syLut.Put(word);
		SynMacro *ma  = (SynMacro*) malloc(sizeof(*ma));
		ma->id      = SSID_INVALID;
		ma->line    = strdup(line);
		ma->parsed  = NULL;
		ma->va_args = false;
		intab->maLut.Put(mid, ma);
		assert(intab->maLut.Lookup(mid) != NULL);
	}
	return true;
}

File *Parser::GetIncludedFile(sym_t preprocessor, const char *line, FILE **outf, bool& in_compiler_dir)
{
	bool quoted = true;
	CC_STRING ifpath, itoken, iline;
	File *retval = NULL;

	iline = ExpandLine(intab, true, line, errmsg);
	if(!errmsg.isnull())
		goto done;

	itoken = GetIncludeFileName(iline, quoted);
	iline.clear();
	if( itoken.isnull() ) {
		errmsg.Format("Invalid include preprocessor: %s", pline.from.c_str());
		goto done;
	}

	if(!rtc->get_include_file_path(itoken, GetCurrentFileName(), quoted, preprocessor == SSID_SHARP_INCLUDE_NEXT, ifpath, &in_compiler_dir)) {
		errmsg.Format("Cannot find include file \"%s\"", itoken.c_str());
		goto done;
	}

	RealFile *file;
	pline.to = pline.from;
	file = new RealFile;
	file->SetFileName(ifpath);
	retval = file;
	*outf = NULL;

	if(has_dep_file() && ! in_compiler_dir)
		AddDependency("  ", ifpath.c_str());
done:
	return retval;
}


bool Parser::do_include(sym_t preprocessor, const char *line, const char **output)
{
	FILE *outf;
	File *file;
	bool in_compiler_dir;

	file = GetIncludedFile(preprocessor, line, &outf, in_compiler_dir);
	if( file == NULL )
		return false;
	if(file->Open()) {
		PushIncludedFile(file, NULL, COUNT_OF(Parser::preprocessors), in_compiler_dir, conditionals.size());
	} else {
		errmsg.Format("Cannot open `%s'", file->name.c_str());
		return false;
	}

	*output = pline.to.c_str();
	return true;
}

bool Parser::SM_Run()
{
	FILE *out_fp = included_files.top()->ofile;
	const char *pos;
	TRI_STATE result = TSV_0;
	const char *output = pline.from.c_str();
	sym_t preprocessor;
	CC_STRING expanded_line;

	do {
		pos = pline.parsed.c_str();
		preprocessor = GetPreprocessor(pos, &pos);
		if(preprocessor != pline.pp_id || (pline.content != -1 && pline.content + pline.parsed.c_str() != pos) ) {
			fprintf(stderr, "%s:%zu\n", dv_current_file, dv_current_line);
			fprintf(stderr, "  (%d) %s\n", pline.content, pline.parsed.c_str());
			fprintf(stderr, "  %d vs %d\n", preprocessor, pline.pp_id);
			exit(1);
		}
	} while(0);

	pos = pline.parsed.c_str() + pline.content;

	dv_current_file = GetCurrentFileName().c_str();
	dv_current_line = GetCurrentLineNumber();

//	GDB_TRAP2(strstr(dv_current_file,"makeint.h"), (dv_current_line==30));
//	GDB_TRAP2(strstr(dv_current_file,"/usr/include/features.h"), dv_current_line==131);
	switch(pline.pp_id) {
	case SSID_SHARP_IF:
		if( superior_conditional_value(true) != TSV_0 )
			result = (TRI_STATE) Compute(pos);
		goto handle_if_branch;

	case SSID_SHARP_IFNDEF:
		if( superior_conditional_value(true) != TSV_0 )
			result = (TRI_STATE) CheckSymbolDefined(pos, true, NULL);
		goto handle_if_branch;

	case SSID_SHARP_IFDEF:
		if( superior_conditional_value(true) != TSV_0 )
			result = (TRI_STATE) CheckSymbolDefined(pos, false, NULL);

handle_if_branch:
		if( result == TSV_X ) {
			if( gvar_preprocess_mode ) {
				if(errmsg.isnull())
					errmsg = "Error on processing conditional";
				goto done;
			}
		}

		conditionals.push(new CConditionalChain());

		upper_chain()->enter_if(result);
		upper_chain()->filename = dv_current_file;
		upper_chain()->line = dv_current_line;
		if(eval_current_conditional() != TSV_X)
			output = NULL;
		included_files.top()->add_if(result);
		break;

	case SSID_SHARP_ELIF:
		if( superior_conditional_value(false) == TSV_0 ) {
			included_files.top()->add_elif(false);
			goto done;
		}
		if( upper_chain()->has_true() ) {
			upper_chain()->enter_elif(TSV_0);
			included_files.top()->add_elif(false);
			goto done;
		}
		result = (TRI_STATE) Compute(pos);
		if( gvar_preprocess_mode && result == TSV_X)
			goto done;

		/*
		 *  Transformation on the condition of:
		 *
		 *  1) #if   0
		 *     #elif X --> #if   X
		 *
		 *  2) #elif X
		 *     #elif 1 --> #else
		 */
		if( eval_current_conditional() == TSV_0 ) {
			if(result == TSV_X) {
				pline.to = do_elif(1);
				output = pline.to.c_str();
			} else
				output = NULL;
		} else {
			if(result == TSV_1) {
				pline.to = do_elif(2);
				output = pline.to.c_str();
			} else if(result == TSV_0)
				output = NULL;
		}
		upper_chain()->enter_elif(result);
		included_files.top()->add_elif(result);
		break;

	case SSID_SHARP_ELSE:
		if( superior_conditional_value(false) == TSV_0 ) {
			included_files.top()->add_else(false);
			goto done;
		}
		result = upper_chain()->enter_else();
		included_files.top()->add_else(result);
		if( eval_current_conditional() != TSV_X )
			output = NULL;
		break;

	case SSID_SHARP_ENDIF:
		included_files.top()->add_endif();
		CConditionalChain *c;
		if( ! upper_chain()->keep_endif() )
			output = NULL;
		if( conditionals.size() == 0 ) {
			errmsg = "Unmatched #endif";
			goto done;
		}
		conditionals.pop(c);
		delete c;
		break;

	default:
		if(eval_current_conditional() == TSV_0)
			goto done;
		if(eval_current_conditional() == TSV_X)
			goto print_and_exit;

		switch(pline.pp_id) {
		  case SSID_SHARP_DEFINE:
			if(!do_define(pos))
				goto error_out;
			break;
		  case SSID_SHARP_UNDEF:
			handle_undef(intab, pos);
			break;
		  case SSID_SHARP_INCLUDE:
		  case SSID_SHARP_INCLUDE_NEXT:
			if( gvar_preprocess_mode )
				do_include(pline.pp_id, pos, &output);
			break;
		  default:
			if(gvar_expand_macros) {
				pos = pline.parsed.c_str();
				expanded_line = ExpandLine(intab, false, pos, errmsg);
				if(!errmsg.isnull())
					goto error_out;
				if(included_files.size() == 1) {
					expanded_line += '\n';
					output = expanded_line.c_str();
				}
			}
		}
	}

print_and_exit:
	if( out_fp != NULL && output != NULL )
		fprintf(out_fp, "%s", output);
done:
	pline.comment_start = -1;
	if(gvar_preprocess_mode && GetError()) {
		IncludedFile *tmp;
		CC_STRING pmsg, ts;
		while(included_files.size() > 0) {
			included_files.pop(tmp);
			ts.Format("  %s:%u\n", tmp->ifile->name.c_str(), tmp->ifile->line);
			pmsg += ts;
		}
		if( !pmsg.isnull() ) {
			errmsg = pmsg + errmsg;
		}
	}
	return gvar_preprocess_mode ? (GetError() == NULL) : true;

error_out:
	return false;
}


void Parser::Reset(InternalTables *intab, size_t num_preprocessors, ParserContext *ctx)
{
	this->num_preprocessors = num_preprocessors;
	this->intab                = intab;
	this->rtc             = ctx;

	assert(included_files.size() == 0);

	pline.from.clear();
	pline.parsed.clear();
	pline.comment_start = -1;
	deptext.clear();
	errmsg = "";
	errmsg.clear();
}

static inline int is_arithmetic_operator(const SynToken& token)
{
	/* This is a tricky hack! */
	return (token.id > SSID_COMMA && token.id < SSID_I);
}

static inline int get_sign(const SynToken& token)
{
	if(token.id == SSID_ADDITION)
		return 1;
	else if(token.id == SSID_SUBTRACTION)
		return -1;
	return 0;
}

sym_t Parser::GetPreprocessor(const char *line, const char **pos)
{
	const char *p;
	CC_STRING word;

	p = line;
	skip_blanks(p);
	if( *p++ != '#' ) {
		*pos = p;
		return SSID_INVALID;
	}
	word = '#';

	skip_blanks(p);
	while( isalpha(*p) || *p == '_' )
		word += *p++;

	*pos = p;
	for(size_t i = 0; i < num_preprocessors; i++)
		if( word == preprocessors[i] )
			return intab->syLut.Put(preprocessors[i]);
	return SSID_INVALID;
}


//
// Read and unfold one semantic line from the source file, stripping off any coments.
//
int Parser::ReadLine()
{
#define RETRY() do { \
	state = STAT_0;  \
	goto retry;      \
} while(0)

#define ENTER_COMMENT() do { prev_state = state; state = STAT_SLASH; } while(0)
#define EXIT_COMMENT()  do { state = prev_state; prev_state = STAT_INVALID; } while(0)

	CC_STRING& line = pline.parsed;

	pline.from.clear();
	pline.parsed.clear();
	pline.to.clear();
	pline.pp_id = SSID_INVALID;
	pline.content = -1;

	enum {
		STAT_INVALID = -1,
		STAT_INIT = 0,

		STAT_SPACE1,
		STAT_SHARP,
		STAT_SPACE2,
		STAT_WORD,

		STAT_0,
		STAT_SLASH,
		STAT_LC,
		STAT_BC,
		STAT_ASTERISK,
		STAT_FOLD,

		STAT_SQ,
		STAT_DQ,
		STAT_SQ_ESC,
		STAT_DQ_ESC,
	} state, prev_state = STAT_INVALID;
	int c;
	int foldcnt = 0;
	File *file = GetCurrentFile().ifile;
	CC_STRING directive;
	bool eof = false;

	state = STAT_INIT;
	while(!eof) {
		c = file->ReadChar();
		if(c == EOF) {
			if(line.isnull())
				return 0;
			c = '\n';
			eof = true;
		} else
			pline.from += c;
		if( c == '\r' ) /* Ignore carriage characters */
			continue;
		if(c == '\n')
			file->line++;

retry:
		switch(state) {
		case STAT_INIT:
			switch(c) {
			case ' ':
			case '\t':
				state = STAT_SPACE1;
				line = c;
				continue;
			case '#':
				state = STAT_SHARP;
				line = c;
				continue;
			}
			state = STAT_0;

		case STAT_0:
			switch(c) {
			case '/':
				ENTER_COMMENT();
				break;
			case '\\':
				state = STAT_FOLD;
				break;
			case '"':
				state = STAT_DQ;
				break;
			case '\'':
				state = STAT_SQ;
				break;
			default:
				if(rtc && c == rtc->as_lc_char && pline.pp_id == SSID_INVALID)
					state = STAT_LC;
				break;
			}
			break;

		case STAT_SLASH:
			switch(c) {
			case '/':
				state = STAT_LC;
				mark_comment_start();
				line.remove_last();
				break;
			case '*':
				state = STAT_BC;
				mark_comment_start();
				line.remove_last();
				break;
			case '"':
				state = STAT_DQ;
				break;
			case '\'':
				state = STAT_SQ;
				break;
			default:
				EXIT_COMMENT();
			}
			break;

		case STAT_LC: /* line comment */
			if(c == '\n')
				state = STAT_0;
			break;

		case STAT_BC: /* block comments */
			if(c == '*')
				state = STAT_ASTERISK;
			if(c == '\n')
				foldcnt++;
			break;

		case STAT_ASTERISK: /* asterisk */
			if(c == '/') {
				EXIT_COMMENT();
				continue;
			}
			else if(c != '*')
				state = STAT_BC;
			break;

		case STAT_FOLD: /* folding the line */
			if(c == '\t' || c == ' ')
				;
			else {
				if(c == '\n')
					foldcnt++;
				state = STAT_0;
				continue;
			}
			break;

		case STAT_DQ:
			switch(c) {
			case '\\':
				state = STAT_DQ_ESC;
				break;
			case '"':
				state = STAT_0;
				break;
			}
			break;

		case STAT_SQ:
			switch(c) {
			case '\\':
				state = STAT_SQ_ESC;
				break;
			case '\'':
				state = STAT_0;
				break;
			}
			break;

		case STAT_DQ_ESC:
			state = STAT_DQ;
			break;

		case STAT_SQ_ESC:
			state = STAT_SQ;
			break;

		case STAT_SPACE1:
			switch(c) {
			case '#':
				state = STAT_SHARP; break;
			case ' ':
			case '\t':
				break;
			case '/':
				ENTER_COMMENT();
				break;
			default:
				RETRY();
			}
			break;

		case STAT_SHARP:
			if(isblank(c)) {
				state = STAT_SPACE2;
				break;
			}
		/* fall through */
		case STAT_SPACE2:
			if(isalpha(c) || isdigit(c) || c == '_') {
				state = STAT_WORD;
				directive = '#';
				directive += c;
			} else if(c == '/')
				ENTER_COMMENT();
			else if(!isblank(c))
				RETRY();
			break;

		case STAT_WORD:
			if(isalpha(c) || isdigit(c) || c == '_')
				directive += c;
			else {
				for(size_t i = 0; i < num_preprocessors; i++)
					if(preprocessors[i] == directive) {
						pline.pp_id = intab->syLut.Put(preprocessors[i]);
						pline.content = line.size();
						state = STAT_0;
						break;
					}
				RETRY();
			}
			break;

		case STAT_INVALID:
			errmsg = "Invalid state";
			return -1;
		}

		if(state != STAT_LC && state != STAT_BC && state != STAT_FOLD && state != STAT_ASTERISK)
			line += c;
		if ((state == STAT_INIT || state == STAT_0) && c == '\n')
			break;
	}
	if(state != STAT_INIT && state != STAT_0) {
		errmsg = "Syntax error";
		return -1;
	}
	file->offset = -foldcnt;
	return 1;
}

bool Parser::GetCmdLineIncludeFiles(const CC_ARRAY<CC_STRING>& ifiles, size_t np)
{
	if( ifiles.size() == 0)
		return true;

	CC_STRING path;
	bool in_compiler_dir;
	for(size_t i = 0; i < ifiles.size(); i++) {
		if(!rtc->get_include_file_path(ifiles[i], CC_STRING(""), true, false, path, &in_compiler_dir)) {
			errmsg.Format("Can not find include file \"%s\"", ifiles[i].c_str());
			return false;
		}
		RealFile *file;
		file = new RealFile;
		file->SetFileName(path);

		if( ! file->Open() ) {
			errmsg.Format("Can not open include file \"%s\"", ifiles[i].c_str());
			return false;
		}
		PushIncludedFile(file, NULL, np, in_compiler_dir, conditionals.size());
		if( ! RunEngine(1) )
			return false;

		if(has_dep_file() && ! in_compiler_dir)
			AddDependency("  ",  path.c_str());
	}
	return true;
}

bool Parser::RunEngine(size_t cond)
{
	if(included_files.size() == cond)
		return true;

	while(1) {
		int rc ;
		rc = ReadLine();
		if(rc > 0) {
			if(!SM_Run())
				return false;
		} else if (rc == 0) {
			IncludedFile *ifile = PopIncludedFile();
			if(!ifile->in_compiler_dir && rtc && !rtc->of_array[VCH_CV].isnull()) {
				ifile->produce_cr_text();
				SaveCondValInfo(ifile->cr_text);
			}
			if(included_files.size() > 0)
				delete ifile;
			if(included_files.size() == cond)
				break;
			num_preprocessors = ifile->np;
		} else
			return false;

	}
	return true;
}

/*  Parse the input file and update the global symbol table and macro table,
 *  output the stripped file contents to devices if OUTFILE is not nil.
 *
 *  Returns a pointer to the error messages on failure, or nil on success.
 */
bool Parser::DoFile(InternalTables *intab, size_t num_preprocessors, File *infile, ParserContext *ctx)
{
	bool ignored;
	FILE *out_fp;
	bool retval = false;
	CC_STRING bak_fname, out_fname;
	struct stat    stb;
	struct utimbuf utb;

	if(ctx) {
		ignored = ctx->check_ignore(infile->name);
		if( ignored && ! has_dep_file() )
			return true;
	} else
		ignored = false;

	if( stat(infile->name, &stb) == 0 ) {
		utb.actime  = stb.st_atime;
		utb.modtime = stb.st_mtime;
	}
	if( ! infile->Open() ) {
		errmsg.Format("Cannot open \"%s\" for reading\n");
		return false;
	}

	out_fp = NULL;

	if(ctx != NULL && ctx->outfile != ParserContext::OF_NULL ) {
		if( ctx->outfile == ParserContext::OF_STDOUT )
			out_fp = stdout;
		else {
#if SANITY_CHECK
			assert( ! ctx->baksuffix.isnull() );
#endif
			if( ctx->baksuffix[0] != ParserContext::MAGIC_CHAR )
				bak_fname = infile->name + ctx->baksuffix;
			else
				out_fname = infile->name;

			int fd;
			char tmp_outfile[32];
			strcpy(tmp_outfile, "@cl@-XXXXXX");
			fd = mkstemp(tmp_outfile);
			if( fd < 0 ) {
				errmsg.Format("Cannot open \"%s\" for writing\n", tmp_outfile);
				infile->Close();
				return false;
			}
			out_fp = fdopen(fd, "wb");
			out_fname = tmp_outfile;
		}
	}

	if(ctx == NULL)
		memset(writers, 0, sizeof(writers));
	else {
		writers[VCH_CL] = NULL;
		if(!ctx->of_array[VCH_DEP].isnull())
			writers[VCH_DEP] = gvar_file_writers[VCH_DEP];
		if(!ctx->of_array[VCH_CV].isnull())
			writers[VCH_CV] = gvar_file_writers[VCH_CV];
	}

	if( num_preprocessors >= COUNT_OF(Parser::preprocessors) )
		num_preprocessors  = COUNT_OF(Parser::preprocessors);
	Reset(intab, num_preprocessors, ctx);

	if(has_dep_file())
		AddDependency("", infile->name);

	PushIncludedFile(infile, out_fp, COUNT_OF(Parser::preprocessors), false, conditionals.size());

	if(ctx != NULL) {
		GetCmdLineIncludeFiles(ctx->imacro_files, 2);
		GetCmdLineIncludeFiles(ctx->include_files, COUNT_OF(preprocessors));
	}

	if( ! RunEngine(0) )
		goto error;

	SaveDepInfo(deptext);
	if( conditionals.size() != 0 )
		errmsg = "Unmatched #if";
	else
		retval = true;

error:
	if(!retval) {
		if(included_files.size() > 0) {
			CC_STRING tmp;
			tmp.Format("%s:%u:  %s\n%s\n", GetCurrentFileName().c_str(), GetCurrentLineNumber(), pline.from.c_str(), GetError());
			errmsg = tmp;
		}
		#if 0
		IncludedFile *ilevel;
		while(included_files.size() > 0) {
			ilevel = PopIncludedFile();
			if(infile != ilevel->ifile)
				delete ilevel;
		}
		#endif
	}

	if(out_fp != NULL && out_fp != stdout)
		fclose(out_fp);

	if( retval && ctx != NULL && ! ignored ) {
		CC_STRING semname;
		sem_t *sem;

		semname = MakeSemaName(infile->name);
		sem = sem_open(semname.c_str(), O_CREAT, 0666, 1);
		sem_wait(sem);

		if( ! bak_fname.isnull() )
			rename(infile->name, bak_fname);
		if( ! out_fname.isnull() ) {
			rename(out_fname, infile->name);
			utime(infile->name.c_str(), &utb);
		}

		sem_post(sem);
		sem_unlink(semname.c_str());
	} else if( ! out_fname.isnull() )
		unlink(out_fname.c_str());

	return retval;
}


