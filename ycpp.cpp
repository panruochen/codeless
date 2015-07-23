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

#include "ycpp.h"
#include "utils.h"



static bool check_prev_operator(sym_t id)
{
	switch(id) {
		case  SSID_LOGIC_OR:
		case  SSID_LOGIC_AND:
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

static bool GetRealPath(const CC_STRING& name, CC_STRING& rp)
{
/*  Use my own function rather than realpath since realpath will expand symblic links */
//	if( realpath(name.c_str(), p) == NULL )
//		return NULL;
	rp = fol_realpath(name);
	return ! rp.isnull() ;
}

static inline bool operator >= (const CToken& a, const CToken& b)
{
	if(a.attr == CToken::TA_UINT && b.attr == CToken::TA_UINT)
		return a.u32_val >= b.u32_val;
	if( (a.attr == CToken::TA_UINT && b.attr == CToken::TA_INT) ||
		(a.attr == CToken::TA_INT && b.attr == CToken::TA_UINT) ||
		(a.attr == CToken::TA_INT && b.attr == CToken::TA_INT))
		return a.i32_val >= b.i32_val;
	assert(0);
}

static inline bool operator > (const CToken& a, const CToken& b)
{
	if(a.attr == CToken::TA_UINT && b.attr == CToken::TA_UINT)
		return a.u32_val > b.u32_val;
	if( (a.attr == CToken::TA_UINT && b.attr == CToken::TA_INT) ||
		(a.attr == CToken::TA_INT && b.attr == CToken::TA_UINT) ||
		(a.attr == CToken::TA_INT && b.attr == CToken::TA_INT))
		return a.i32_val > b.i32_val;
	assert(0);
}


static inline bool operator == (const CToken& a, uint32_t val)
{
	return a.u32_val == val;
}


static inline bool operator != (const CToken& a, uint32_t val)
{
	return a.u32_val != val;
}


static bool DoCalculate(CToken& opnd1, sym_t opr, CToken& opnd2, CToken& result, TCC_CONTEXT *tc, CException *ex)
{
	if( gv_preprocess_mode ) {
		if(opnd1.attr == CToken::TA_IDENT) {
			opnd1.attr    = CToken::TA_UINT;
			opnd1.u32_val = 0;
		}
		if((opr != SSID_LOGIC_NOT && opr != SSID_BITWISE_NOT) && opnd2.attr == CToken::TA_IDENT  ) {
			opnd2.attr    = CToken::TA_UINT;
			opnd2.u32_val = 0;
		}
	}

	result.attr = CToken::TA_UINT;
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
			*ex = "divided by zero";
			goto error;
		}
		result.u32_val = opnd1.u32_val / opnd2.u32_val;
		break;
	case SSID_PERCENT:
		if(opnd2.u32_val == 0) {
			*ex = "divided by zero";
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
		ex->format("Invalid operator %s", TR(tc,opr));
		goto error;
	}
	return true;

error:
	result.attr = CToken::TA_IDENT;
	result.id   = SSID_SYMBOL_X;
	return false;
}

static bool DoCalculationOnStackTop(TCC_CONTEXT *tc, sym_t opr, CC_STACK<CToken>& opnd_stack,
	CC_STACK<sym_t>& opr_stack, CException *gex)
{
	CToken a, b;
	CToken result = { SSID_SYMBOL_X, CToken::TA_UINT };

	if( opr == SSID_COLON ) {
		CToken c;
		sym_t tmp_opr = SSID_INVALID;
		if(opnd_stack.size() < 3) {
			*gex = "Not enough operands for `? :'";
			return false;
		}
		opr_stack.pop(tmp_opr);
		if(tmp_opr != SSID_QUESTION) {
			*gex = "Invalid operator `:'";
			return false;
		}
		opnd_stack.pop(c);
		opnd_stack.pop(b);
		opnd_stack.pop(a);

		log(LOGV_DEBUG, "%s ? %s : %s\n", TOKEN_NAME(a), TOKEN_NAME(b), TOKEN_NAME(c));
		result = a.u32_val ? b : c;
		goto done;
	} else if( opr != SSID_LOGIC_NOT && opr != SSID_BITWISE_NOT ) {
		if( ! opnd_stack.pop(b) || ! opnd_stack.pop(a))
			goto error_no_operands;
		log(LOGV_DEBUG, "%s %s %s\n", TOKEN_NAME(a), TR(tc,opr), TOKEN_NAME(b));
	} else {
		if( ! opnd_stack.pop(a) )
			goto error_no_operands;
		log(LOGV_DEBUG, "! %s\n", TOKEN_NAME(a));
	}
	if( ! DoCalculate(a, opr, b, result, tc, gex) ) {
		return false;
	}
done:
	opnd_stack.push(result);
	return true;

error_no_operands:
	gex->format("Not enough operands for opeator '%s'", TR(tc,opr));
	return false;
}

static bool is_opr(sym_t sym)
{
	return sym < SSID_SYMBOL_X;
}


static int symbol_definition_check(TCC_CONTEXT *tc, const char *line, bool reverse, CToken *result, CException *gex)
{
	short attr = CToken::TA_IDENT;
	int retval = TSV_X;
	CToken token;
	CMacro *minfo;

	if( ! ReadToken(tc, &line, &token, gex, false) || gex->GetError() != NULL )
		goto error;
	minfo = tc->maMap.Lookup(token.id);
	if( ! gv_preprocess_mode ) {
		if(minfo == NULL) {
			retval = TSV_X;
			attr   = CToken::TA_IDENT;
		} else if(minfo == CMacro::NotDef) {
			retval = reverse ? TSV_1 : TSV_0;
			attr   = CToken::TA_UINT;
		} else {
			retval = reverse ? TSV_0 : TSV_1;
			attr   = CToken::TA_UINT;
		}
	} else {
		attr    = CToken::TA_UINT;
		retval  = (minfo == NULL) ? TSV_0 : TSV_1;
		retval ^= reverse;
	}

error:
	if(result != NULL) {
		result->attr    = attr;
		result->u32_val = retval;
		result->id      = SSID_SYMBOL_X;
		result->name    = TR(tc, SSID_SYMBOL_X);
	}
	if( gv_preprocess_mode )
		assert(retval != TSV_X);
	return retval;
}

static bool contain(const CC_ARRAY<CC_STRING>& hints, const CC_STRING& line)
{
	size_t i;
	for(i = 0; i < hints.size(); i++)
		if( strstr(line.c_str(), hints[i].c_str()) != NULL )
			return true;
	return false;
}

static int expression_evaluate(TCC_CONTEXT *tc, const char *line, CException *gex)
{
	const char *saved_line = line;
	bool dflag = false;
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
	CC_STACK<CToken>  opnd_stack;
	sym_t last_opr = SSID_SHARP;
	char sign;
	const char *symbol = NULL;
	LOG_VERB dml = LOGV_DEBUG;
	CC_STRING expansion;
	CException ex2;

	skip_blanks(saved_line);
	dflag = contain(dx_traced_lines, line);
	if(dflag)
		log(LOGV_RUNTIME, "RAW: %s\n", line);

	expansion = ExpandLine(tc, false, line, gex);
	if(gex->GetError() != NULL) {
		if(dflag) {
			log(LOGV_RUNTIME, "*Error* %s\n");
		}
		return TSV_X;
	}

	line = expansion.c_str();
	if(dflag) {
		log(LOGV_RUNTIME, "EXP: %s\n", line);
		dml = LOGV_RUNTIME;
	}

	opr_stack.push(SSID_SHARP);
	log(dml, "PUSH OPR: #\n");
	sign = '\0';
	state = STAT_OPR1;
	while(1) {
		sym_t  opr;
		CToken  token;
		const char *last_pos = line;

		if( ! ReadToken(tc, &line, &token, gex, false) )
			break;
		if( gex->GetError() != NULL )
			goto error;
		if( token.attr == CToken::TA_CHAR )
			token.attr = CToken::TA_INT;

		switch(state) {
		case STAT_INIT:
			if(is_opr(token.id))
				state = STAT_OPR1;
			else if(token.id == SSID_DEFINED)
				state = STAT_DEFINED;
			else if(token.attr == CToken::TA_IDENT)
				state = STAT_OPND;
			break;
		case STAT_OPND:
			if(is_opr(token.id))
				state = STAT_OPR1;
			else {
				*gex = "Adjacent operands";
				goto error;
			}
			break;
		case STAT_OPR1:
			if(is_opr(token.id))
				state = STAT_OPR2;
			else if(token.id == SSID_DEFINED)
				state = STAT_DEFINED;
			else
				state= STAT_INIT;
			break;
		case STAT_OPR2:
			if(is_opr(token.id)) {
			} else if(token.id == SSID_DEFINED)
				state = STAT_DEFINED;
			else
				state = STAT_INIT;
			break;
		case STAT_DEFINED:
			if(token.id == SSID_LEFT_PARENTHESIS)
				state = STAT_DEFINED1;
			else if(token.attr == CToken::TA_IDENT) {
				symbol_definition_check(tc, last_pos, false, &token, gex);
				if(gex->GetError() != NULL)
					goto error;
				state = STAT_INIT;
			}
			break;
		case STAT_DEFINED1:
			if(token.attr == CToken::TA_IDENT) {
				state = STAT_DEFINED2;
				symbol = last_pos;
			} else {
				*gex = "Syntax error: ";
				goto error;
			}
			break;
		case STAT_DEFINED2:
			if(token.id == SSID_RIGHT_PARENTHESIS) {
				symbol_definition_check(tc, symbol, false, &token, gex);
				if(gex->GetError() != NULL)
					goto error;
				state = STAT_INIT;
				opnd_stack.push(token);
				goto next;
			} else {
				*gex = "Unmatched (";
				goto error;
			}
			break;
		}
		if(state == STAT_DEFINED || state == STAT_DEFINED1 || state == STAT_DEFINED2)
			goto next;

		if( token.id == CToken::TA_UINT || token.id == CToken::TA_INT )
			log(dml, "Current: %u\n", token.u32_val);
		else
			log(dml, "Current: %s\n", TR(tc,token.id));
//		log(dml, "OPR  Stack: \n", opr_stack);
//		log(dml, "OPND Stack: \n", opnd_stack);
		opr = token.id;
		if( is_opr(opr) ) {
			sym_t opr0;
			int result;
again:
			if(state == STAT_OPR2 && check_prev_operator(last_opr) &&
				(opr == SSID_ADDITION || opr == SSID_SUBTRACTION)) {
				if(opr == SSID_SUBTRACTION)
					sign = '-';
				continue;
			}

			opr0 = opr_stack.top();
			result = tc->opMap.Compare(opr0, opr);
			switch(result) {
			case OP_EQUAL:
				opr_stack.pop(opr0);
				break;
			case OP_HIGHER:
				opr_stack.pop(opr0);
				if( ! DoCalculationOnStackTop(tc, opr0, opnd_stack, opr_stack, &ex2) ) {
					*gex = ex2;
					goto error;
				}
				goto again;
				break;
			case OP_LOWER:
				opr_stack.push(opr);
				break;
			case OP_ERROR:
				gex->format("Cannot compare \"%s\" and \"%s\"", TR(tc,opr0), TR(tc,opr));
				log(LOGV_ERROR, "*ERROR* %s\n", gex->GetError());
				goto error;
			}
		} else {

			if( sign == '-' && token.attr == CToken::TA_UINT ) {
				token.attr = CToken::TA_INT;
				token.i32_val = - token.i32_val;
			}
			opnd_stack.push(token);
		}

	next:
		sign = 0;
		last_opr = opr;
	}

	do {
		sym_t opr0;
		int result;

		if( ! opr_stack.pop(opr0) )
			goto error;
		result = tc->opMap.Compare(opr0, SSID_SHARP);
		switch(result) {
		case OP_EQUAL:
			break;
		case OP_HIGHER:
			if( ! DoCalculationOnStackTop(tc, opr0, opnd_stack, opr_stack, &ex2) ) {
				*gex = ex2;
				goto error;
			}
			break;
		default:
			log(LOGV_ERROR, "%s:%u: Bad expression\n", __func__,  __LINE__);
			*gex = "[1] Bad expression";
			goto error;
		}
	} while( opr_stack.size() != 0 );

	if( opnd_stack.size() != 1 ) {
		log(LOGV_ERROR, "%s:%u: Bad expression\n", __func__,  __LINE__);
		*gex = "[2] Bad expression";
		goto error;
	}

	if( opnd_stack.top().attr == CToken::TA_IDENT ) {
		if( gv_preprocess_mode ) {
			*gex = ex2;
			return TSV_0;
		}
		return TSV_X;
	}
	log(dml, "Numberic Value: %u\n", opnd_stack.top().i32_val);;
	*gex = "";
	return !!opnd_stack.top().i32_val;

error:
	log(LOGV_ERROR, "*Error* %s\n", gex->GetError());
	return gv_preprocess_mode ? TSV_0 : TSV_X;
}

const char *CYcpp::preprocessors[] = {
	"#define", "#undef",
	"#if", "#ifdef", "#ifndef", "#elif", "#else", "#endif",
	"#include", "#include_next"
};

TRI_STATE CYcpp::eval_upper_condition(bool on_if)
{
	const size_t n = on_if ? 0 : 1;
	CConditionalChain **top;
	assert(conditionals.size() >= n);
	return conditionals.size() == n ? TSV_1 : (top = &(conditionals.top()) - n, (*top)->eval_condition());
}

CC_STRING CYcpp::do_elif(int mode)
{
	CC_STRING output;
	CC_STRING trailing;
	const char *p;

	if( comment_start > 0 )
		trailing = raw_line.c_str() + comment_start ;
	else {
		p = raw_line.c_str();
		while( *p != '\r' && *p != '\n' && *p != '\0') p++;
		trailing = p;
	}

	if(mode == 2)
		output = "#else";
	else if(mode == 1){
		p = raw_line.c_str();
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


void CYcpp::AddDependency(const char *prefix, const CC_STRING& filename)
{
	CC_STRING rp;

	if( GetRealPath(filename, rp) ) {
		deptext += prefix;
		deptext += rp;
		deptext += "\n";
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

/******************************************************************************************/

void CYcpp::do_define(const char *line)
{
	const char *p;
	CC_STRING word;
	sym_t mid;

	p = line;
	skip_blanks(p);
	if( isalpha(*p) || *p == '_' ) {
		word = *p++;
		while( isalpha(*p) || *p == '_' || isdigit(*p) )
			word += *p++;
		mid = tc->syMap.Put(word);
		CMacro *ma  = (CMacro*) malloc(sizeof(*ma));
		ma->id      = SSID_INVALID;
		ma->line    = strdup(line);
		ma->parsed  = NULL;
		ma->va_args = false;
		tc->maMap.Put(mid, ma);
		assert(tc->maMap.Lookup(mid) != NULL);
		if( dx_traced_macros.size() > 0 && find(dx_traced_macros, word))
			log(LOGV_RUNTIME, "%s:%u:  %s\n", GetCurrentFileName().c_str(), GetCurrentLineNumber(), raw_line.c_str());
	}
}


CFile *CYcpp::GetIncludedFile(sym_t preprocessor, const char *line, FILE **outf)
{
	bool in_sys_dir, quoted;
	CC_STRING ifpath, itoken, iline;
	uint8_t filetype;
	CFile *retval = NULL;

	iline = ExpandLine(tc, true, line, &gex);
	if(gex.GetError() != NULL)
		goto done;

	itoken = GetIncludeFileName(iline, quoted);
	iline.clear();
	if( itoken.isnull() ) {
		gex.format("Invalid include preprocessor: %s", raw_line.c_str());
		goto done;
	}
	ifpath = rtctx->get_include_file_path(itoken, GetCurrentFileName(),
		quoted, preprocessor == SSID_SHARP_INCLUDE_NEXT, &in_sys_dir);
	if(ifpath.isnull()) {
		gex.format("Cannot find include file \"%s\"", itoken.c_str());
		goto done;
	}

	filetype = check_source_type(ifpath);
	if( ! in_sys_dir ) {
		if(filetype == SOURCE_TYPE_C || filetype == SOURCE_TYPE_CPP) {
			CC_STRING cifile, cipath, bakfile, suffix;
			CC_STRING semname;
			sem_t *sem;
			CC_STRING dir = fol_dirname(ifpath);
			int retval __NO_USE__ = -1;

			for(unsigned int i = 0; ; i++) {
				if(i == 0)
					suffix = ".c_include";
				else
					suffix.format(".c_include%u", i);
				cifile = itoken + suffix;
				cipath = ifpath + suffix;
				if( ! fol_exist(cipath) )
					break;
			}
			bakfile = ifpath + baksuffix;
			iline = CC_STRING("#include ") + (quoted ? '"' : '<') + cifile + (quoted ? '"' : '>') + '\n';
			*outf = NULL;
			semname = MakeSemaName(GetCurrentFileName());
			sem = sem_open(semname.c_str(), O_CREAT, 0666, 1);
			sem_wait(sem);

			if( ! fol_exist(bakfile) )
				retval = fol_copy(ifpath, cipath);
			else
				retval = fol_copy(bakfile, cipath);
			sem_post(sem);
			sem_unlink(semname.c_str());
			if(retval < 0) {
				gex.format("Cannot create %s", cipath.c_str());
				goto done;
			}
			ifpath += suffix;
		}
	}


	CRealFile *file;
	new_line = (iline.isnull() ?  raw_line : iline);
	file = new CRealFile;
	file->SetFileName(ifpath);
	retval = file;
	*outf = NULL;

	if(has_dep_file() && ! in_sys_dir)
		AddDependency("  ", ifpath.c_str());
done:
	return retval;
}


bool CYcpp::do_include(sym_t preprocessor, const char *line, const char **output)
{
	FILE *outf;
	CFile *file;

	file = GetIncludedFile(preprocessor, line, &outf);
	if( file == NULL )
		return false;
	if(file->Open()) {
		PushIncludedFile(file, NULL, COUNT_OF(CYcpp::preprocessors), conditionals.size());
	} else {
		gex.format("Cannot open `%s'", file->name.c_str());
		return false;
	}

	*output = new_line.c_str();
	return true;
}

bool CYcpp::SM_Run()
{
	FILE *out_fp = included_files.top().outfp;
	const char *pos;
	TRI_STATE result = TSV_0;
	const char *output = raw_line.c_str();
	sym_t preprocessor;
	CC_STRING expanded_line;

	pos = line.c_str();
	preprocessor = GetPreprocessor(pos, &pos);

	dv_current_file = GetCurrentFileName().c_str();
	dv_current_line = GetCurrentLineNumber();
//	fprintf(stderr, "ON %s:%u\n", dv_current_file, dv_current_line);

#if 0
//	if( included_files.size() == 1 && 0 != strcmp(dv_current_file, "<command line>") && dv_current_line > 1)

	CFile *sfile = (&included_files.top() + 1 - included_files.size())->srcfile;
	if( strstr(sfile->name.c_str(), "emif-common.c") &&
		strstr(dv_current_file, "include/config.h") && dv_current_line == 5)
	{
//		fprintf(stderr, "++++ %s:%u\n", dv_current_file, dv_current_line);
		volatile int loop = 1;
		do {} while (loop);
	}
#endif

	switch(preprocessor) {
	case SSID_SHARP_IF:
		if( eval_upper_condition(true) != TSV_0 )
			result = (TRI_STATE) expression_evaluate(tc, pos, &gex);
		goto handle_if_branch;

	case SSID_SHARP_IFNDEF:
		if( eval_upper_condition(true) != TSV_0 )
			result = (TRI_STATE) symbol_definition_check(tc, pos, true, NULL, &gex);
		goto handle_if_branch;

	case SSID_SHARP_IFDEF:
		if( eval_upper_condition(true) != TSV_0 )
			result = (TRI_STATE) symbol_definition_check(tc, pos, false, NULL, &gex);

handle_if_branch:
		if( result == TSV_X ) {
			if( gv_preprocess_mode ) {
				gex = "Error on processing conditional";
				goto done;
			}
		}

		conditionals.push(new CConditionalChain());

		upmost_chain()->enter_if_branch(result);
		upmost_chain()->filename = dv_current_file;
		upmost_chain()->line = dv_current_line;
		if(eval_current_condition() != TSV_X)
			output = NULL;
		break;

	case SSID_SHARP_ELIF:
		if( eval_upper_condition(false) == TSV_0 )
			goto done;
		if(  eval_current_condition() == TSV_1 ) {
			upmost_chain()->enter_elif_branch(TSV_0);
			goto done;
		}
		result = (TRI_STATE) expression_evaluate(tc, pos, &gex);
		if( gv_preprocess_mode && result == TSV_X)
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
		if( eval_current_condition() == TSV_0 ) {
			if(result == TSV_X) {
				new_line = do_elif(1);
				output = new_line.c_str();
			} else
				output = NULL;
		} else {
			if(result == TSV_1) {
				new_line = do_elif(2);
				output = new_line.c_str();
			} else if(result == TSV_0)
				output = NULL;
		}
		upmost_chain()->enter_elif_branch(result);
		goto save_current_block;
		break;

	case SSID_SHARP_ELSE:
		if( eval_upper_condition(false) == TSV_0 )
			goto done;
		upmost_chain()->enter_else_branch();
		if( eval_current_condition() != TSV_X )
			output = NULL;
save_current_block:
		break;

	case SSID_SHARP_ENDIF:
		CB_BEGIN
		CConditionalChain *c;
		if( ! upmost_chain()->keep_endif() )
			output = NULL;
		if( conditionals.size() == 0 ) {
			gex = "Unmatched #endif";
			goto done;
		}
		conditionals.pop(c);
		delete c;
		CB_END
		break;

	default:
		if(eval_current_condition() == TSV_0)
			goto done;
		if(eval_current_condition() == TSV_X)
			goto print_and_exit;

		switch(preprocessor) {
		  case SSID_SHARP_DEFINE:
			do_define(pos);
			break;
		  case SSID_SHARP_UNDEF:
			handle_undef(tc, pos);
			break;
		  case SSID_SHARP_INCLUDE:
		  case SSID_SHARP_INCLUDE_NEXT:
			if( gv_preprocess_mode )
				do_include(preprocessor, pos, &output);
			break;
		  default:
			if(rtm_expand_macros) {
				pos = line.c_str();
				expanded_line = ExpandLine(tc, false, pos, &gex);
				if(gex.GetError() != NULL)
					return false;
				expanded_line += '\n';
				output = expanded_line.c_str();
			}
		}
	}

print_and_exit:
	if( out_fp != NULL && output != NULL )
		fprintf(out_fp, "%s", output);
done:

	comment_start = -1;
	if(gv_preprocess_mode && gex.GetError()) {
		TIncludedFile tmp;
		while(included_files.size() > 0) {
			included_files.pop(tmp);
			fprintf(stderr, "**** %s\n", tmp.srcfile->name.c_str());
		}
		exit(2);
	}
	return gv_preprocess_mode ? (gex.GetError() == NULL) : true;
}


void CYcpp::Reset(TCC_CONTEXT *tc, size_t num_preprocessors, CP_CONTEXT *ctx)
{
	this->num_preprocessors = num_preprocessors;
	this->tc                = tc;
	this->rtctx             = ctx;

	assert(included_files.size() == 0);

	raw_line.clear();
	line.clear();
	deptext.clear();
	comment_start = -1;
	gex = "";
	errmsg.clear();
}

static inline int is_arithmetic_operator(const CToken& token)
{
	/* This is a tricky hack! */
	return (token.id > SSID_COMMA && token.id < SSID_I);
}

static inline int get_sign(const CToken& token)
{
	if(token.id == SSID_ADDITION)
		return 1;
	else if(token.id == SSID_SUBTRACTION)
		return -1;
	return 0;
}

sym_t CYcpp::GetPreprocessor(const char *line, const char **pos)
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
			return tc->syMap.Put(preprocessors[i]);
	return SSID_INVALID;
}


//
// Read and unfold one semantic line from the source file, stripping off any coments.
//
int CYcpp::ReadLine()
{
	enum {
		STAT_INIT,
		STAT_SLASH,
		STAT_LC,
		STAT_BC,
		STAT_ASTERISK,
		STAT_FOLD,

		STAT_SQ,
		STAT_DQ,
		STAT_SQ_ESC,
		STAT_DQ_ESC,
	} state;
	int c;
	CFile *file = GetCurrentFile().srcfile;

#if 0
	if( strcmp(file->name.c_str(), "/usr/include/x86_64-linux-gnu/bits/types.h") == 0 && file->line == 120 )
	{
		volatile int loop = 1;
		do {} while (loop);
	}
#endif

	state = STAT_INIT;
	while(1) {
		c = file->ReadChar();
		if(c == EOF) {
			if(line.isnull())
				return 0;
			c = '\n';
			if(state != STAT_INIT) {
				gex = "Syntax error found";
				return -1;
			}
			goto handle_last_line;
		}

		raw_line += c;
		if( c == '\r' ) /* Ignore carriage characters */
			continue;
		if(c == '\n')
			file->line++;

		switch(state) {
		case STAT_INIT:
			switch(c) {
			case '/':
				state = STAT_SLASH;
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
				state = STAT_INIT;
			}
			break;

		case STAT_LC: /* line comment */
			if(c == '\n')
				state = STAT_INIT;
			break;

		case STAT_BC: /* block comments */
			if(c == '*')
				state = STAT_ASTERISK;
			break;

		case STAT_ASTERISK: /* asterisk */
			if(c == '/') {
				state = STAT_INIT;
				continue;
			}
			else if(c != '*')
				state = STAT_BC;
			break;

		case STAT_FOLD: /* folding the line */
			if(c == '\t' || c == ' ')
				;
			else {
				state = STAT_INIT;
				continue;
			}
			break;

		case STAT_DQ:
			switch(c) {
			case '\\':
				state = STAT_DQ_ESC;
				break;
			case '"':
				state = STAT_INIT;
				break;
			}
			break;

		case STAT_SQ:
			switch(c) {
			case '\\':
				state = STAT_SQ_ESC;
				break;
			case '\'':
				state = STAT_INIT;
				break;
			}
			break;

		case STAT_DQ_ESC:
			state = STAT_DQ;
			break;

		case STAT_SQ_ESC:
			state = STAT_SQ;
			break;
		}

		if(state != STAT_LC && state != STAT_BC && state != STAT_FOLD && state != STAT_ASTERISK)
			line += c;

		if(state == STAT_INIT) {
handle_last_line:
			if(c == '\n')
				break;
		}
	}
	return 1;
}

bool CYcpp::check_file_processed(const CC_STRING& filename)
{
	CC_STRING bakfile;

	bakfile = filename + baksuffix;
	if( ! fol_exist(bakfile) )
		return false;
	return true;
}

bool CYcpp::GetCmdLineIncludeFiles(const CC_ARRAY<CC_STRING>& ifiles, size_t np)
{
	if( ifiles.size() == 0)
		return true;

	CC_STRING path;
	bool in_sys_dir;
	for(size_t i = 0; i < ifiles.size(); i++) {
		path = rtctx->get_include_file_path(ifiles[i], CC_STRING(""), true, false, &in_sys_dir);
		if( path.isnull() ) {
			gex.format("Can not find include file \"%s\"", ifiles[i].c_str());
			return false;
		}
		CRealFile *file;
		file = new CRealFile;
		file->SetFileName(path);

		if( ! file->Open() ) {
			gex.format("Can not open include file \"%s\"", ifiles[i].c_str());
			return false;
		}
		PushIncludedFile(file, NULL, np, conditionals.size());
		if( ! RunEngine(1) )
			return false;

		if(has_dep_file() && ! in_sys_dir)
			AddDependency("  ",  path.c_str());
	}
	return true;
}

bool CYcpp::RunEngine(size_t cond)
{
	if(included_files.size() == cond)
		return true;

	TIncludedFile ilevel;
	while( 1 ) {
		int rc ;
		rc = ReadLine();
		if( rc > 0 ) {
			if( ! SM_Run() )
				return false;
		} else if (rc == 0) {
			ilevel = PopIncludedFile();
			/****
			if(lvl.if_level != conditionals.size()) {
				gex.format("Levels mismatched on %s, %zu %zu\n", lvl.srcfile->name.c_str(), lvl.if_level, conditionals.size());
				goto error;
			}
			****/
///			if(infile != ilevel.srcfile )
			if(included_files.size() > 0)
				delete ilevel.srcfile;
			if(included_files.size() == cond)
				break;
			num_preprocessors = ilevel.np;
		} else
			return false;
		raw_line.clear();
		line.clear();
		new_line.clear();
	}
	return true;
}

/*  Parse the input file and update the global symbol table and macro table,
 *  output the stripped file contents to devices if OUTFILE is not nil.
 *
 *  Returns a pointer to the error messages on failure, or nil on success.
 */
bool CYcpp::DoFile(TCC_CONTEXT *tc, size_t num_preprocessors, CFile *infile, CP_CONTEXT *ctx)
{
	bool bypass;
	FILE *out_fp;
	bool retval = false;
	char tmp_outfile[64] = { 0 };
	CC_STRING bakfile;
	struct stat    stb;
	struct utimbuf utb;

	if( check_file_processed(infile->name) && ! ctx->save_byfile.isnull() ) {
		CC_STRING tmp = infile->name + '\n';
		fol_append(ctx->save_byfile, tmp.c_str(), tmp.size());
	}

	if(ctx) {
		bypass = ctx->check_if_bypass(infile->name);
		if( bypass && ! has_dep_file() )
			return true;
	} else
		bypass = false;

	/**
	if(ctx)
		fprintf(stdout, "** %s\n", infile->name.c_str());
	if( strstr(infile->name.c_str(), "boot-common.c") ) {
		fprintf(stderr, "bypass_list has %u elements\n", ctx->bypass_list.size());
		fprintf(stderr, "cmp '%s' against '%s'\n", infile->name.c_str(), ctx->bypass_list[0].c_str());
		fprintf(stderr, "bypass %u\n", bypass);
		exit(2);
	}
	**/

	if( stat(infile->name, &stb) == 0 ) {
		utb.actime  = stb.st_atime;
		utb.modtime = stb.st_mtime;
	}
	if( ! infile->Open() ) {
		log(LOGV_RUNTIME, "Cannot open \"%s\" for reading\n");
		return false;
	}

	out_fp = NULL;
	baksuffix.clear();

	if(ctx != NULL ) {
		if( ! ctx->baksuffix.isnull() && ctx->baksuffix[0] != '\0' ) {
			bakfile   = infile->name + ctx->baksuffix;
			baksuffix = ctx->baksuffix;
		}
		if( ctx->outfile.isnull() || bypass )
			out_fp = NULL;
		else if( IS_STDOUT(ctx->outfile) )
			out_fp = stdout;
		else {
			int fd;
			strcpy(tmp_outfile, "#yccp-XXXXXX");
			fd = mkstemp(tmp_outfile);
			if( fd < 0 ) {
				log(LOGV_RUNTIME, "Cannot open \"%s\" for writing\n", ctx->outfile.c_str());
				infile->Close();
				return false;
			}
			out_fp = fdopen(fd, "wb");
		}
	}

	if( num_preprocessors >= COUNT_OF(CYcpp::preprocessors) )
		num_preprocessors  = COUNT_OF(CYcpp::preprocessors);
	Reset(tc, num_preprocessors, ctx);

	if(has_dep_file())
		AddDependency("", infile->name);

	PushIncludedFile(infile, out_fp, COUNT_OF(CYcpp::preprocessors), conditionals.size());

	if(ctx != NULL) {
		GetCmdLineIncludeFiles(ctx->imacro_files, 2);
		GetCmdLineIncludeFiles(ctx->include_files, COUNT_OF(preprocessors));
	}

	if( ! RunEngine(0) )
		goto error;

	if(has_dep_file() && ! deptext.isnull() )
		fol_append(ctx->save_depfile, deptext.c_str(), deptext.size());
	if( conditionals.size() != 0 )
		gex = "Unmatched #if";
	else
		retval = true;

error:
	if(!retval) {
		if(included_files.size() > 0)
			errmsg.format("%s:%u:  %s\n%s\n", GetCurrentFileName().c_str(), GetCurrentLineNumber(), raw_line.c_str(), gex.GetError());
		else
			errmsg = gex.GetError();
		TIncludedFile ilevel;
		while(included_files.size() > 0) {
			ilevel = PopIncludedFile();
			if(infile != ilevel.srcfile)
				delete ilevel.srcfile;
		}
	}

	if(out_fp != NULL && out_fp != stdout)
		fclose(out_fp);

	if( retval && ctx != NULL && ! bypass ) {
		CC_STRING semname;
		sem_t *sem;

		semname = MakeSemaName(infile->name);
		sem = sem_open(semname.c_str(), O_CREAT, 0666, 1);
		sem_wait(sem);

		if( ! bakfile.isnull() )
			rename(infile->name, bakfile);
		rename(tmp_outfile, ctx->outfile);
		utime(ctx->outfile.c_str(), &utb);

		sem_post(sem);
		sem_unlink(semname.c_str());
	} else
		unlink(tmp_outfile);

	return retval;
}


