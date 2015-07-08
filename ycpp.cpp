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

enum IF_VALUE {
	IV_FALSE   = 0,
	IV_UNKNOWN = 2,
	IV_TRUE    = 1,
};

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


static inline IF_VALUE operator ! (IF_VALUE val)
{
	return (val == IV_UNKNOWN) ? IV_UNKNOWN :
		(val == IV_FALSE ? IV_TRUE : IV_FALSE);
}

//
// Get the include file name without surrouding angle brackets or double quoation marks
//
static CC_STRING GetIncludeFileName(const CC_STRING& line, char& style)
{
	const char *p1, *p2;
	CC_STRING filename;

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

error:
	return filename;
}

static char *GetRealPath(const CC_STRING& name, char *buf)
{
	static char rp_buf[PATH_MAX];
	char *p = buf ? buf : rp_buf;
	if( realpath(name.c_str(), p) == NULL )
		return NULL;
	return p;
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
	if( !gv_strict_mode ) {
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

		log(DML_DEBUG, "%s ? %s : %s\n", TOKEN_NAME(a), TOKEN_NAME(b), TOKEN_NAME(c));
		result = a.u32_val ? b : c;
		goto done;
	} else if( opr != SSID_LOGIC_NOT && opr != SSID_BITWISE_NOT ) {
		if( ! opnd_stack.pop(b) || ! opnd_stack.pop(a))
			goto error_no_operands;
		log(DML_DEBUG, "%s %s %s\n", TOKEN_NAME(a), TR(tc,opr), TOKEN_NAME(b));
	} else {
		if( ! opnd_stack.pop(a) )
			goto error_no_operands;
		log(DML_DEBUG, "! %s\n", TOKEN_NAME(a));
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
	int retval = IV_UNKNOWN;
	CToken token;
	CMacro *minfo;

	if( ! ReadToken(tc, &line, &token, gex, false) || gex->GetError() != NULL )
		goto error;
	minfo = tc->maMap.Lookup(token.id);
	if( gv_strict_mode ) {
		if(minfo == NULL) {
			retval = IV_UNKNOWN;
			attr   = CToken::TA_IDENT;
		} else if(minfo == CMacro::NotDef) {
			retval = reverse ? IV_TRUE : IV_FALSE;
			attr   = CToken::TA_UINT;
		} else {
			retval = reverse ? IV_FALSE : IV_TRUE;
			attr   = CToken::TA_UINT;
		}
	} else {
		attr    = CToken::TA_UINT;
		retval  = (minfo == NULL) ? IV_FALSE : IV_TRUE;
		retval ^= reverse;
	}

error:
	if(result != NULL) {
		result->attr    = attr;
		result->u32_val = retval;
		result->id      = SSID_SYMBOL_X;
		result->name    = TR(tc, SSID_SYMBOL_X);
	}
	if( !gv_strict_mode )
		assert(retval != IV_UNKNOWN);
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
	MSG_LEVEL dml = DML_DEBUG;
	CC_STRING expansion;
	CException ex2;

	skip_blanks(saved_line);
	dflag = contain(dx_traced_lines, line);
	if(dflag)
		log(DML_RUNTIME, "RAW: %s\n", line);

	expansion = ExpandLine(tc, false, line, gex);
	if(gex->GetError() != NULL) {
		if(dflag) {
			log(DML_RUNTIME, "*Error* %s\n");
		}
		return IV_UNKNOWN;
	}

	line = expansion.c_str();
	if(dflag) {
		log(DML_RUNTIME, "EXP: %s\n", line);
		dml = DML_RUNTIME;
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
				log(DML_ERROR, "*ERROR* %s\n", gex->GetError());
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
			log(DML_ERROR, "%s:%u: Bad expression\n", __func__,  __LINE__);
			*gex = "[1] Bad expression";
			goto error;
		}
	} while( opr_stack.size() != 0 );

	if( opnd_stack.size() != 1 ) {
		log(DML_ERROR, "%s:%u: Bad expression\n", __func__,  __LINE__);
		*gex = "[2] Bad expression";
		goto error;
	}

	if( opnd_stack.top().attr == CToken::TA_IDENT ) {
		if( !gv_strict_mode ) {
			*gex = ex2;
			return IV_FALSE;
		}
		return IV_UNKNOWN;
	}
	log(dml, "Numberic Value: %u\n", opnd_stack.top().i32_val);;
	*gex = "";
	return !!opnd_stack.top().i32_val;

error:
	log(DML_ERROR, "*Error* %s\n", gex->GetError());
	fprintf(stderr, "*** %s\n", expansion.c_str());
	return !gv_strict_mode ? IV_FALSE : IV_UNKNOWN;
}

CC_STRING Cycpp::do_elif(int mode)
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

bool Cycpp::current_state_pop()
{
	if( conditionals.pop(current) )
		return true;
	return false;
}

bool Cycpp::under_false_conditional()
{
	return conditionals.size() > 1 && conditionals.top().curr_value == IV_FALSE;
}


void Cycpp::AddDependency(const char *prefix, const CC_STRING& filename)
{
	char *rp_name;
	rp_name = GetRealPath(filename, NULL);
	if( rp_name != NULL ) {
		dependencies += prefix;
		dependencies += rp_name;
		dependencies += "\n";
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

void Cycpp::do_define(const char *line)
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
			log(DML_RUNTIME, "%s:%u:  %s\n", current_file()->name.c_str(), current_file()->line, raw_line.c_str());
	}
}


CFile *Cycpp::GetIncludedFile(sym_t preprocessor, const char *line, FILE **outf)
{
	bool in_sys_dir;
	CC_STRING ifpath, itoken, iline;
	char style = 0;
	uint8_t filetype;
	CFile *retval = NULL;

	ifpath = ExpandLine(tc, true, line, &gex);

	if(gex.GetError() != NULL)
		goto done;

	itoken = GetIncludeFileName(ifpath, style);
	if( itoken.isnull() ) {
		gex.format("Invalid include preprocessor: %s", raw_line.c_str());
		goto done;
	}
	ifpath = get_include_file_path(itoken, current_file()->name,
		style == '"', preprocessor == SSID_SHARP_INCLUDE_NEXT, &in_sys_dir);
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
			CC_STRING dir = fsl_dirname(ifpath);
			int retval __NO_USE__ = -1;

			for(unsigned int i = 0; ; i++) {
				if(i == 0)
					suffix = ".c_include";
				else
					suffix.format(".c_include%u", i);
				cifile = itoken + suffix;
				cipath = ifpath + suffix;
				if( ! fsl_exist(cipath) )
					break;
			}
			bakfile = ifpath + baksuffix;
			iline = CC_STRING("#include ") + (style == '"' ? '"' : '<') + cifile +
				(style == '"' ? '"' : '>') + '\n';
			*outf = NULL;
			semname = MakeSemaName(current_file()->name);
			sem = sem_open(semname.c_str(), O_CREAT, 0666, 1);
			sem_wait(sem);

			if( ! fsl_exist(bakfile) )
				retval = fsl_copy(ifpath, cipath);
			else
				retval = fsl_copy(bakfile, cipath);
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

	if(depfile.c_str() != NULL && ! in_sys_dir)
		AddDependency("  ", ifpath.c_str());
done:
	return retval;
}


bool Cycpp::do_include(sym_t preprocessor, const char *line, const char **output)
{
	FILE *outf;
	CFile *file;

	file = GetIncludedFile(preprocessor, line, &outf);
	if( file == NULL )
		return false;
	if(file->Open()) {
		include_level_push(file, NULL, conditionals.size());
	} else {
		gex.format("Cannot open `%s'", file->name.c_str());
		return false;
	}

	*output = new_line.c_str();
	return true;
}

bool Cycpp::SM_Run()
{
	FILE *out_fp = include_levels.top().outfp;
	const char *pos;
	IF_VALUE result = IV_FALSE;
	const char *output = raw_line.c_str();
	sym_t preprocessor;
	CC_STRING expanded_line;

	pos = line.c_str();
	preprocessor = GetPreprocessor(pos, &pos);

	dv_current_file = current_file()->name.c_str();
	dv_current_line = current_file()->line;
//	fprintf(stderr, "ON %s:%u\n", dv_current_file, dv_current_line);

#if 0
	if( include_levels.size() == 1 && 0 != strcmp(dv_current_file, "<command line>") && dv_current_line > 1)
	{
		volatile int loop = 1;
		do {} while (loop);
	}
#endif

	switch(preprocessor) {
	case SSID_SHARP_IF:
		if( current.curr_value != IV_FALSE )
			result = (IF_VALUE) expression_evaluate(tc, pos, &gex);
		goto handle_if_branch;
	case SSID_SHARP_IFNDEF:
		if( current.curr_value != IV_FALSE )
			result = (IF_VALUE) symbol_definition_check(tc, pos, true, NULL, &gex);
		goto handle_if_branch;
	case SSID_SHARP_IFDEF:
		if( current.curr_value != IV_FALSE )
			result = (IF_VALUE) symbol_definition_check(tc, pos, false, NULL, &gex);
handle_if_branch:

		if( !gv_strict_mode && result == IV_UNKNOWN) {
			goto done;
		}

		conditionals.push(current);
		if( current.curr_value != IV_FALSE ) {
			current.if_value   =
			current.curr_value = result;
			current.elif_value = IV_UNKNOWN;
			if(current.if_value != IV_UNKNOWN) output = NULL;
		} else
			goto done;
		break;
	case SSID_SHARP_ELIF:
		if( under_false_conditional() )
			goto done;
		result = (IF_VALUE) expression_evaluate(tc, pos, &gex);
		if( !gv_strict_mode && result == IV_UNKNOWN)
			goto done;

		/*
		 *  Transformation on the condition of:
		 *  1) #if 0
		 *    #elif X --> #if   X
		 *  2) #elif X
		 *    #elif 1 --> #else
		 *    #elif X --> #elif X
		 */
		if( current.if_value == IV_FALSE ) {
			if(result == IV_UNKNOWN) {
				new_line = do_elif(1);
				output = new_line.c_str();
			} else
				output = NULL;
			current.if_value = current.curr_value = result;
		} else if( current.if_value != IV_TRUE && current.elif_value != IV_TRUE ) {
			if(result == IV_TRUE) {
				new_line = do_elif(2);
				output = new_line.c_str();
			} else if(result == IV_FALSE)
				output = NULL;
			current.elif_value = current.curr_value = result;
		} else {
			current.curr_value = IV_FALSE;
			output = NULL;
		}
		break;
	case SSID_SHARP_ELSE:
		if( under_false_conditional() )
			goto done;
		if( current.if_value == IV_TRUE || current.elif_value == IV_TRUE )
			current.curr_value = IV_FALSE;
		else
			current.curr_value = (int8_t) ! ((IF_VALUE)current.curr_value);
		if( current.if_value != IV_UNKNOWN || current.elif_value == IV_TRUE )
			output = NULL;
		break;
	case SSID_SHARP_ENDIF:
		if( under_false_conditional() || current.if_value != IV_UNKNOWN)
			output = NULL;
		if( conditionals.size() == 0 ) {
			gex = "Unmatched #endif";
			goto done;
		}
		conditionals.pop(current);
		break;

	default:
		if(current.curr_value == IV_FALSE)
			goto done;
		if(current.curr_value == IV_UNKNOWN)
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
			if( !gv_strict_mode )
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
	return !gv_strict_mode ? (gex.GetError() == NULL) : true;
}


void Cycpp::Reset(TCC_CONTEXT *tc, const char **preprocessors, size_t num_preprocessors, CFile *infile, CP_CONTEXT *ctx)
{
	this->preprocessors     = preprocessors;
	this->num_preprocessors = num_preprocessors;
	this->tc                = tc;
	if(ctx != NULL) {
		this->depfile       = ctx->depfile;
	} else {
		this->depfile.clear();
	}

	current.if_value   = IV_TRUE;
	current.elif_value = IV_UNKNOWN;
	current.curr_value = IV_TRUE;

	assert(include_levels.size() == 0);

	CC_STRING tmpath;

	raw_line.clear();
	line.clear();
	dependencies.clear();
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

sym_t Cycpp::GetPreprocessor(const char *line, const char **pos)
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
int Cycpp::ReadLine()
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
	CFile *file = current_file();

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
	if( 0 && include_levels.size() == 1) {
	fprintf(stderr, "*** %s", raw_line.c_str());
	fprintf(stderr, "+++ %s", line.c_str());
	}
	return 1;
}


/*  Parse the input file and update the global symbol table and macro table,
 *  output the stripped file contents to devices if OUTFILE is not nil.
 *
 *  Returns a pointer to the error messages on failure, or nil on success.
 */
bool Cycpp::DoFile(TCC_CONTEXT *tc, const char **preprocessors, size_t num_preprocessors, CFile *infile,
	CP_CONTEXT *ctx)
{
	FILE *out_fp;
	bool retval = false;
	char tmp_outfile[64] = { 0 };
	CC_STRING bakfile;
	struct stat    stb;
	struct utimbuf utb;

	if( stat(infile->name, &stb) == 0 ) {
		utb.actime  = stb.st_atime;
		utb.modtime = stb.st_mtime;
	}
	if( ! infile->Open() ) {
		log(DML_RUNTIME, "Cannot open \"%s\" for reading\n");
		return false;
	}

	out_fp = NULL;
	baksuffix.clear();

	if(ctx != NULL ) {
		if( ctx->baksuffix.c_str() != NULL && ctx->baksuffix[0] != '\0' ) {
			bakfile   = infile->name + ctx->baksuffix;
			baksuffix = ctx->baksuffix;
		}
		if( ctx->outfile.isnull() )
			out_fp = NULL;
		else if( IS_STDOUT(ctx->outfile) )
			out_fp = stdout;
		else {
			int fd;
			strcpy(tmp_outfile, "#pxcc-XXXXXX");
			fd = mkstemp(tmp_outfile);
			if( fd < 0 ) {
				log(DML_RUNTIME, "Cannot open \"%s\" for writing\n", ctx->outfile.c_str());
				infile->Close();
				return false;
			}
			out_fp = fdopen(fd, "wb");
		}
	}

	Reset(tc, preprocessors, num_preprocessors, infile, ctx);

	if(depfile.c_str() != NULL) {
		size_t j;
		CC_STRING ipath;
		bool in_sys_dir;

		AddDependency("", infile->name);
		for(j = 0; j < ctx->imacro_files.size(); j++) {
			ipath = get_include_file_path(ctx->imacro_files[j], current_file()->name, true, false, &in_sys_dir);
			if(!in_sys_dir)
				AddDependency("  ", ipath);
		}
		for(j = 0; j < ctx->include_files.size(); j++) {
			ipath = get_include_file_path(ctx->include_files[j], current_file()->name, true, false, &in_sys_dir);
			if(!in_sys_dir)
				AddDependency("  ", ipath);
		}
	}

	include_level_push(infile, out_fp, conditionals.size());
	INCLUDE_LEVEL ilevel;
	while( 1 ) {
		int rc ;
		rc = ReadLine();
		if( rc > 0 ) {
			if( ! SM_Run() )
				goto error;
		} else if (rc == 0) {
			ilevel = include_level_pop();
			/****
			if(lvl.if_level != conditionals.size()) {
				gex.format("Levels mismatched on %s, %zu %zu\n", lvl.srcfile->name.c_str(), lvl.if_level, conditionals.size());
				goto error;
			}
			****/
			if(infile != ilevel.srcfile )
				delete ilevel.srcfile;
			if(include_levels.size() == 0)
				break;
		} else
			goto error;
		raw_line.clear();
		line.clear();
		new_line.clear();
	}
	if(depfile.c_str() != NULL && dependencies.c_str() != NULL ) {
		fsl_mp_append(depfile, dependencies.c_str(), dependencies.size());
	}
	if( conditionals.size() != 0 )
		gex = "Unmatched #if";
	else
		retval = true;

error:
	if(!retval) {
		if(include_levels.size() > 0)
			errmsg.format("%s:%u:  %s\n%s\n", current_file()->name.c_str(), current_file()->line, raw_line.c_str(), gex.GetError());
		else
			errmsg = gex.GetError();
		while(include_levels.size() > 0) {
			ilevel = include_level_pop();
			if(infile != ilevel.srcfile)
				delete ilevel.srcfile;
		}
	}

	if(out_fp != NULL && out_fp != stdout)
		fclose(out_fp);

	 if( retval && ctx != NULL ) {
		CC_STRING semname;
		sem_t *sem;

		semname = MakeSemaName(infile->name);
		sem = sem_open(semname.c_str(), O_CREAT, 0666, 1);
		sem_wait(sem);

		if( bakfile.c_str() != NULL )
			rename(infile->name, bakfile);
		rename(tmp_outfile, ctx->outfile);
		utime(ctx->outfile.c_str(), &utb);

		sem_post(sem);
		sem_unlink(semname.c_str());
	} else
		unlink(tmp_outfile);

	return retval;
}


