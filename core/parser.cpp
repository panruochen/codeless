#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <ctype.h>
#include <unistd.h>

#include <sys/types.h>

#include "tcc.h"
#include "precedence-matrix.h"
#include "c++-if.h"

#define  PUSH(s,v)  (s).push_back(v)
#define  POP(s,v)   (s).pop_back(v)

enum IF_VALUE {
	IV_NS      = -1,
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


static inline IF_VALUE logic_not(IF_VALUE val)
{
	return (val == IV_UNKNOWN) ? IV_UNKNOWN :
		(val == IV_FALSE ? IV_TRUE : IV_FALSE);
}

static bool calculate(TOKEN& opnd1, sym_t opr, TOKEN& opnd2, TOKEN& result)
{
#define CMP(x,opr,val) \
	(((x).attr == TA_UINT || (x).attr == TA_INT)&& (x).u32_val opr (val))

	result.id = SSID_SYMBOL_X;
	if(opnd1.attr == TA_IDENTIFIER || opnd2.attr == TA_IDENTIFIER) {
		result.attr = TA_IDENTIFIER;
		result.u32_val = 0;
		if(opr == SSID_LOGIC_OR && ( CMP(opnd1,!=,0) || CMP(opnd2,!=,0))) {
			result.attr  = TA_UINT;
			result.u32_val = 1;
			return true;
		}
		else if(opr == SSID_LOGIC_AND && (CMP(opnd1,==,0) || CMP(opnd2,==,0))) {
			result.attr  = TA_UINT;
			result.u32_val = 0;
			return true;
		}

		if( !rtm_preprocess )
			return false;
		else {
			opnd1.attr = TA_UINT;
			opnd2.attr = TA_UINT;
			opnd1.u32_val = 0;
			opnd2.u32_val = 0;
		}
	}
#undef CMP

	result.attr = TA_UINT;
	switch(opr) {
	case SSID_LOGIC_AND:
		result.b_val = opnd1.b_val && opnd2.b_val;
		break;
	case SSID_LOGIC_OR:
		result.b_val = opnd1.b_val || opnd2.b_val;
		break;
	case SSID_LESS:
		if(opnd1.attr == TA_UINT && opnd2.attr == TA_UINT)
			result.b_val = opnd1.u32_val < opnd2.u32_val;
		else
			result.b_val = opnd1.i32_val < opnd2.i32_val;
		break;
	case SSID_LESS_EQUAL:
		if(opnd1.attr == TA_UINT && opnd2.attr == TA_UINT)
			result.b_val = opnd1.u32_val <= opnd2.u32_val;
		else
			result.b_val = opnd1.i32_val <= opnd2.i32_val;
		break;
	case SSID_GREATER:
		if(opnd1.attr == TA_UINT && opnd2.attr == TA_UINT)
			result.b_val = opnd1.u32_val > opnd2.u32_val;
		else
			result.b_val = opnd1.i32_val > opnd2.i32_val;
		break;
	case SSID_GREATER_EQUAL:
		if(opnd1.attr == TA_UINT && opnd2.attr == TA_UINT)
			result.b_val = opnd1.u32_val >= opnd2.u32_val;
		else
			result.b_val = opnd1.i32_val >= opnd2.i32_val;
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
		result.u32_val = opnd1.u32_val / opnd2.u32_val;
		break;
	case SSID_PERCENT:
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
		return false;
	}
	return true;
}

static bool deduce(TCC_CONTEXT *tc, sym_t opr, TOKEN_ARRAY& opnd_stack,
	CC_ARRAY<sym_t>& opr_stack, const char **errs)
{
	TOKEN a, b;
	TOKEN result = { SSID_SYMBOL_X, TA_UINT };
	static CC_STRING error_string;

	if( opr == SSID_COLON ) {
		TOKEN c;
		sym_t tmp_opr = SSID_INVALID;
		if(opnd_stack.size() < 3) {
			*errs = "Not enough operands for '? :'";
			return false;
		}
		POP(opr_stack,tmp_opr);
		if(tmp_opr != SSID_QUESTION) {
			*errs = "Invalid operator -- ':'";
			return false;
		}
		POP(opnd_stack, c);
		POP(opnd_stack, b);
		POP(opnd_stack, a);
		
		debug_console << DML_DEBUG << &a << " ? " <<
			&b << " : " << &c << '\n';
		result = a.u32_val ? b : c;
		goto done;
	} else if( opr != SSID_LOGIC_NOT && opr != SSID_BITWISE_NOT ) {
		if( ! POP(opnd_stack, b) || ! POP(opnd_stack, a)) 
			goto error_no_operands;
		debug_console << DML_DEBUG << &a << "  " <<
			id_to_symbol(tc->symtab, opr) <<
			"  " << &b << '\n';
	} else {
		if( ! POP(opnd_stack, a) ) 
			goto error_no_operands;
		debug_console << DML_DEBUG << "! " << &a << '\n';
	}
	if( ! calculate(a, opr, b, result) ) {
		error_string  = "Caculation not implented for operator '";
		error_string += id_to_symbol(tc->symtab, opr);
		error_string += '\'';
		*errs = error_string.c_str();
		return false;
	}
done:
	PUSH(opnd_stack, result);
	return true;

error_no_operands:
	error_string  = "Not enough operands for opeator '";
	error_string += id_to_symbol(tc->symtab, opr);
	error_string += '\'';
	*errs = error_string.c_str();
	return false;
}

static bool is_opr(sym_t sym)
{
	return sym < SSID_SYMBOL_X;
}


int check_symbol_defined(TCC_CONTEXT *tc, const char *line, bool reverse, TOKEN *result, const char **errs)
{
	short attr = TA_IDENTIFIER;
	int retval = IV_UNKNOWN;
	TOKEN token;
	MACRO_INFO *minfo;

	if( ! read_token(tc, &line, &token, errs, 0) || *errs != NULL )
		goto error;
	minfo = macro_table_lookup(tc->mtab, token.id);
	if( ! rtm_preprocess ) {
		if(minfo == NULL) {
			retval = IV_UNKNOWN;
			attr   = TA_IDENTIFIER;
		} else if(minfo == TCC_MACRO_UNDEF) {
			retval = reverse ? IV_TRUE : IV_FALSE;
			attr   = TA_UINT;
		} else {
			retval = reverse ? IV_FALSE : IV_TRUE;
			attr   = TA_UINT;
		}
	} else {
		attr    = TA_UINT;
		retval  = (minfo == NULL) ? IV_FALSE : IV_TRUE;
		retval ^= reverse;
	}

error:
	if(result != NULL) {
		result->attr    = attr;
		result->u32_val = retval;
		result->id      = SSID_SYMBOL_X;
#if HAVE_NAME_IN_TOKEN
		result->name = id_to_symbol(tc->symtab, SSID_SYMBOL_X);
#endif
	}
	if(rtm_preprocess)
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

static int expression_evaluate(TCC_CONTEXT *tc, const char *line, const char **errs)
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
	CC_ARRAY<sym_t> opr_stack;
	TOKEN_ARRAY     opnd_stack;
	sym_t last_opr = SSID_SHARP;
	char sign;
	const char *symbol;
	MESSAGE_LEVEL dml = DML_DEBUG;
	CC_STRING expansion;

	SKIP_WHITE_SPACES(saved_line);

	dflag = contain(dx_traced_lines, line);

	if(dflag)
		runtime_console << "RAW:  " << line << '\n';
	expansion = macro_expand(tc, line, errs);
	if(*errs != NULL) {
		if(dflag) {
			runtime_console << "EXP:  " << line << '\n';
			runtime_console << "Cannot expand due to " << *errs << '\n';
		}
		return IV_UNKNOWN;
	}
	line = expansion.c_str();
	if(dflag) {
		runtime_console << "EXP:  " << line << '\n';
		dml = DML_RUNTIME;
	}

	opr_stack.push_back(SSID_SHARP);
	debug_console << dml << "PUSH OPR: #" << '\n' ;
	sign = '\0';
	state = STAT_OPR1;
	while(1) {
		sym_t  opr;
		TOKEN  token;
		const char *last_pos = line;

		if( ! read_token(tc, &line, &token, errs, 0) )
			break;
		if( *errs != NULL )
			goto error;
		if( token.attr == TA_CHAR )
			token.attr = TA_INT;

		switch(state) {
		case STAT_INIT:
			if(is_opr(token.id))
				state = STAT_OPR1;
			else if(token.id == SSID_DEFINED)
				state = STAT_DEFINED;
			else if(token.attr == TA_IDENTIFIER)
				state = STAT_OPND;
			break;
		case STAT_OPND:
			if(is_opr(token.id))
				state = STAT_OPR1;
			else {
				*errs = "Adjacent operands" ;
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
				state= STAT_INIT;
			break;
		case STAT_DEFINED:
			if(token.id == SSID_LEFT_PARENTHESIS)
				state = STAT_DEFINED1;
			else if(token.attr == TA_IDENTIFIER) {
				check_symbol_defined(tc, last_pos, false, &token, errs);
				if(*errs != NULL)
					goto error;
				state = STAT_INIT;
			}
			break;
		case STAT_DEFINED1:
			if(token.attr == TA_IDENTIFIER) {
				state = STAT_DEFINED2;
				symbol = last_pos;
			} else
				goto error;
			break;
		case STAT_DEFINED2:
			if(token.id == SSID_RIGHT_PARENTHESIS) {
				check_symbol_defined(tc, symbol, false, &token, errs);
				if(*errs != NULL)
					goto error;
				state = STAT_INIT;
				opnd_stack.push_back(token);
				goto next;
			} else
				goto error;
			break;
		}
		if(state == STAT_DEFINED || state == STAT_DEFINED1 || state == STAT_DEFINED2)
			goto next;

		if( token.id == TA_UINT || token.id == TA_INT )
			debug_console << dml <<  "Current: " << (size_t)token.u32_val << '\n' ;
		else
			debug_console << dml <<  "Current: " << id_to_symbol(tc->symtab,token.id) << '\n' ;
		debug_console << dml <<  "OPR  Stack: " << opr_stack << '\n' ;
		debug_console << dml <<  "OPND Stack: " << opnd_stack << '\n' << '\n' ;
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

			opr0 = opr_stack.back();
			result = oprmx_compare(tc->matrix, opr0, opr);
			switch(result) {
			case OP_EQUAL:
				opr_stack.pop_back(opr0);
				break;
			
			case OP_HIGHER:
				POP(opr_stack, opr0);
				if( ! deduce(tc, opr0, opnd_stack, opr_stack, errs) )
					goto error;
				goto again;
				break;
			
			case OP_LOWER:
				PUSH(opr_stack, opr);
				break;
			
			case OP_ERROR:
				debug_console << DML_ERROR << "Cannot compare precedence for `" <<
					id_to_symbol(tc->symtab,opr0) << "' and `" <<
					id_to_symbol(tc->symtab,opr) << '\'' << '\n' ;
				goto error;
			}
		} else {

			if( sign == '-' && token.attr == TA_UINT ) {
				token.attr = TA_INT;
				token.i32_val = - token.i32_val;
			}
			opnd_stack.push_back(token);
		}

	next:
		sign = 0;
		last_opr = opr;
	}

	debug_console << dml <<  "OPR  Stack: " << opr_stack << '\n' << '\n' ;
	debug_console << dml <<  "OPND Stack: " << opnd_stack << '\n' << '\n' ;
	do {
		sym_t opr0;
		int result;

		if( !POP(opr_stack, opr0) )
			goto error;
		result = oprmx_compare(tc->matrix, opr0, SSID_SHARP);
		switch(result) {
		case OP_EQUAL:
			break;
		case OP_HIGHER:
			if( ! deduce(tc, opr0, opnd_stack, opr_stack, errs) )
				goto error;
			break;
		default:
			debug_console << DML_ERROR << __func__ << ':' << (size_t)__LINE__ << ": bad expression" << '\n';
			*errs = "[1] Bad expression";
			goto error;
		}
	} while( opr_stack.size() != 0 );

	if( opnd_stack.size() != 1 ) {
		*errs = "[2] Bad expression";
		debug_console << DML_ERROR << __func__ << ':' << (size_t)__LINE__ << ": Opnd stack size: " << opnd_stack.size() << '\n';
		goto error;
	}

	if( opnd_stack.back().attr == TA_IDENTIFIER ) {
		return rtm_preprocess ? IV_FALSE : IV_UNKNOWN;
	}
	debug_console << dml << "Numberic Value: " << (ssize_t)opnd_stack.back().i32_val << '\n';
	return !!opnd_stack.back().i32_val;

error:
//	assert(*errs != NULL);
	if(*errs == NULL)
		*errs = "Unknown errors";
	printf("LINE:    %s\n", saved_line);
	debug_console << DML_ERROR << *errs << '\n' ;
	return IV_UNKNOWN;
}

CC_STRING ConditionalParser::handle_elif(int mode)
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
		SKIP_WHITE_SPACES(p);
		p ++; /* # */
		SKIP_WHITE_SPACES(p);
		p += 4; /* elif */
		trailing = p;
		output = "#if";
	}
	output += trailing;
	return output;
}

bool ConditionalParser::current_state_pop()
{
	if( POP(conditionals, current) ) {
		return true;
	}
	return false;
}

bool ConditionalParser::under_false_conditional()
{
	return conditionals.size() > 1 && conditionals.back().curr_value == IV_FALSE;
}

static CC_STRING get_include_file_name(const char *line, char& style)
{
	char *p1, *p2, saved;
	CC_STRING filename;

	p1 = (char *)line;
	SKIP_WHITE_SPACES(p1);

	if( *p1 == '"')
		style = '"';
	else if( *p1 == '<' )
		style = '>';
	else
		goto error;
	p1++;
	SKIP_WHITE_SPACES(p1);
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
	saved = * ++ p2;
	*p2 = '\0';
	filename = p1;
	*p2 = saved;

error:
	return filename;
}

const char * ConditionalParser::run(sym_t preprocessor, const char *line, bool *anything_changed)
{
	const char *retval = NULL, *errs;
	IF_VALUE result = IV_FALSE;
	CC_STRING new_line;
	const char *output = raw_line.c_str();

	* anything_changed = false;
	if(preprocessor == SSID_SHARP_DEFINE) {
		if(current.curr_value == IV_TRUE) {
			const char *p;
			CC_STRING word;
			sym_t mid;

			p = line;
			SKIP_WHITE_SPACES(p);
			if( isalpha(*p) || *p == '_' ) {
				word = *p++;
				while( isalpha(*p) || *p == '_' || isdigit(*p) )
					word += *p++;
				mid = symtab_insert(tc->symtab, word.c_str());
				MACRO_INFO *mi = (MACRO_INFO*) malloc(sizeof(*mi));
				mi->id     = SSID_INVALID;
				mi->line   = strdup(line);
				mi->define = NULL;
				macro_table_insert(tc->mtab, mid, mi);
				assert(macro_table_lookup(tc->mtab, mid) != NULL);
				if( dx_traced_macros.size() > 0 && find(dx_traced_macros, word)  )
					runtime_console << current_file->name << ':' << current_file->line << ": " <<
						raw_line << '\n';
			}
		}
		if(current.curr_value == IV_FALSE)
			goto done;
		else
			goto print_and_exit;
	} 
	switch(preprocessor) {
		case SSID_SHARP_IF:
			if( current.curr_value != IV_FALSE ) {
				result = (IF_VALUE) expression_evaluate(tc, line, &errs);
			}
			goto handle_if_branch;
		case SSID_SHARP_IFNDEF:
			if( current.curr_value != IV_FALSE ) {
				result = (IF_VALUE) check_symbol_defined(tc, line, true, NULL, &errs);
			}
			goto handle_if_branch;
		case SSID_SHARP_IFDEF:
			if( current.curr_value != IV_FALSE ) {
				result = (IF_VALUE) check_symbol_defined(tc, line, false, NULL, &errs);
			}
handle_if_branch:
			if( rtm_preprocess && result == IV_UNKNOWN) {
				char buf[32];
			prepare_to_exit:
				last_error += errs;
				retval = last_error.c_str();
				goto done;
			}

			PUSH(conditionals, current);
			if( current.curr_value != IV_FALSE ) {
				current.if_value   =
				current.curr_value = result;
				current.elif_value = IV_NS;
				if(current.if_value != IV_UNKNOWN) output = NULL;
			} else
				goto done;
			break;
		case SSID_SHARP_ELIF:
			if( under_false_conditional() )
				goto done;
			result = (IF_VALUE) expression_evaluate(tc, line, &errs);
			if(rtm_preprocess && result == IV_UNKNOWN)
				goto prepare_to_exit;
			
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
					new_line = handle_elif(1);
					output = new_line.c_str();
				} else
					output = NULL;
				current.if_value = current.curr_value = result;
			} else if( current.if_value != IV_TRUE && current.elif_value != IV_TRUE ) {
				if(result == IV_TRUE) {
					new_line = handle_elif(2);
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
				current.curr_value = (int8_t) logic_not((IF_VALUE)current.curr_value);
			if( current.if_value != IV_UNKNOWN || current.elif_value == IV_TRUE )
				output = NULL;
			break;
		case SSID_SHARP_ENDIF:
			if( under_false_conditional() || current.if_value != IV_UNKNOWN)
				output = NULL;
			if( conditionals.size() == 0 ) {
				return "Unmatched #endif";
			}
			POP(conditionals, current);
			break;

		default:
			if(current.curr_value == IV_FALSE)
				goto done;
			else if(current.curr_value == IV_TRUE) {
				if(preprocessor == SSID_SHARP_DEFINE)
					;
				else if(preprocessor == SSID_SHARP_UNDEF)
					handle_undef(tc, line);
				else if( rtm_preprocess && (preprocessor == SSID_SHARP_INCLUDE || preprocessor == SSID_SHARP_INCLUDE_NEXT) ) {
					CC_STRING tmp;
					const char *new_pos;
					const char *path;
					char style;
					bool in_sys_dir;
					
					new_pos = line;
					if( take_include_token(tc, &new_pos, &errs) ) {
						join(tmp, line, new_pos);
						path = tmp.c_str();
						tmp = get_include_file_name(path, style);
					} else {
						tmp = macro_expand(tc, line, &errs);
						if(errs != NULL) {
							last_error = errs;
							retval = last_error.c_str();
							goto done;
						}
						path = tmp.c_str();
						tmp = get_include_file_name(path, style);
					}

					if(tmp.size() == 0) {
						last_error = "Error: ";
						last_error = raw_line;
						retval = last_error.c_str();
						goto done;
					}
					path = check_file(tmp.c_str(), current_file->name.c_str(), style == '"', preprocessor == SSID_SHARP_INCLUDE_NEXT, &in_sys_dir);
					if(path != NULL) {
						FILE *fp = fopen(path, "rb");
						if(fp != NULL) {
							REAL_FILE *file = new REAL_FILE;
							file->set_fp(fp, path);
							if(dep_fp != NULL && ! in_sys_dir) {
								fprintf(dep_fp, "#include  ");
								if( path[0] != '/' ) {
									char cwd[260];
									getcwd(cwd, sizeof(cwd));
									fprintf(dep_fp, "%s/", cwd);
								}
								fprintf(dep_fp, "%s\n", path);
							}
							PUSH(include_files, current_file);
							current_file = file;
						}
					} else {
						last_error  = "No such include file: ";
						last_error += (style == '>' ? '<' : '"') ;
						last_error += tmp;
						last_error += (char) style;
						retval = last_error.c_str();
						goto done;
					}
				}
			}
	}

print_and_exit:
	if( outdev != NULL && output != NULL && have_output ) {
		fprintf(outdev, "%s", output);
	} else
		*anything_changed = true;
done:
	comment_start = -1;
	return retval;
}

void ConditionalParser::initialize(TCC_CONTEXT *tc, const char **keywords, size_t num_keywords, GENERIC_FILE *infile,
	FILE *outdev, FILE *dep_fp)
{
	this->keywords     = keywords;
	this->num_keywords = num_keywords;
	this->tc           = tc;
	this->outdev       = outdev;
	this->dep_fp       = dep_fp;

	current.if_value   = IV_TRUE;
	current.elif_value = IV_NS;
	current.curr_value = IV_TRUE;

	assert(include_files.size() == 0);
	current_file = infile;

	cword.clear();
	raw_line.clear();
	line.clear();
	have_output = true;
	comment_start = -1;
}

static inline int is_arithmetic_operator(const TOKEN& token)
{
	/* This is a tricky hack! */
	return (token.id > SSID_COMMA && token.id < SSID_I);
}

static inline int get_sign(const TOKEN& token)
{
	if(token.id == SSID_ADDITION)
		return 1;
	else if(token.id == SSID_SUBTRACTION)
		return -1;
	return 0;
}

sym_t ConditionalParser::preprocessed_line(const char *line, const char **pos)
{
	const char *p;
	CC_STRING word;

	p = line;
	SKIP_WHITE_SPACES(p);
	if( *p++ != '#' ) {
		*pos = p;
		return SSID_INVALID;
	}
	word = '#';

	SKIP_WHITE_SPACES(p);
	while( isalpha(*p) || *p == '_' )
		word += *p++;

	*pos = p;
	for(size_t i = 0; i < num_keywords; i++)
		if( word == keywords[i] )
			return symtab_insert(tc->symtab, keywords[i]);
	return SSID_INVALID;
}

/*  Parse the input file and update the global symbol table and macro table,
 *  output the stripped file contents to devices if OUTFILE is not nil.
 *
 *  Returns a pointer to the error messages on failure, or nil on success.
 */
const char *ConditionalParser::do_parse(TCC_CONTEXT *tc, const char **keywords, size_t num_keywords,
	GENERIC_FILE *infile, const char *outfile, FILE *depf, bool *anything_changed)
{
	int last_levels_num = 0;
	FILE *outs = stdout;
	int state = 0;
	int prev;
	static char message[512];
	const char *retval = NULL;
	bool loop ;

	if(anything_changed)
		*anything_changed = false;
	if( ! infile->open() ) {
		snprintf(message, sizeof(message), "Cannot open %s", infile->name.c_str());
		return message;
	}
	if( outfile == NULL )
		outs = NULL;
	else if(  strcmp(outfile, "/dev/stdout") != 0 ) {
		outs = fopen(outfile, "wb");
		if( outs == NULL ) {
			snprintf(message, sizeof(message), "Cannot write to %s", outfile);
			infile->close();
			return message;
		}
	}

	initialize(tc, keywords, num_keywords, infile, outs, depf);
	loop = true;
	while( loop ) {
		bool flag_popup = false;
		int c;

		prev = state;
		c = current_file->read();
		if(c == EOF) {
			if(include_files.size() == 0)
				loop = false;
			else
				flag_popup = true;
			c = '\n';
			goto handle_last_line;
		}

		raw_line += c;
		if( c == '\r' ) /* Ignore carriage characters */
			continue;
		switch(state) {
		case 0:
			if(c == '/')
				state = 1;
			else if(c == '\\')
				state = 5;
			break;

		case 1:
			if(c == '/') {
				state = 2;
				mark_comment_start();
			} else if(c == '*') {
				state = 3;
				mark_comment_start();
			} else
				state = 0;
			break;

		case 2: /* line comment */
			if(c == '\n')
				state = 0;
			break;

		case 3: /* block comments */
			if(c == '*')
				state = 4;
			break;

		case 4: /* asterisk */
			if(c == '/')
				state = 0;
			else if(c != '*')
				state = 3;
			break;

		case 5: /* folding the line */
			if(c == '\t' || c == ' ')
				;
			else
				state = 0;
			break;
		}

		if(c == '\n') {
			current_file->line++;
		}

handle_last_line:
		if(state == 0 ) {
			if( prev == 1)
				line.push_back('/');
			if(prev != 4 && prev != 5) {
				bool cx;
				line.push_back(c);
				if(c == '\n') {
					const char *pos;
					sym_t id = preprocessed_line(line.c_str(), &pos);
					/*runtime_console << current_file->name << ':' <<  current_file->line 
						<< ":   " << raw_line << '\n' ;*/
					retval = run(id, pos, &cx);
					if( retval != NULL) {
						runtime_console << __FILE__ ":" << (size_t)__LINE__ << '\n';
						runtime_console << current_file->name << ':' <<  current_file->line << ":   " << raw_line <<
							retval << '\n' ;

						debug_console << DML_ERROR << __FILE__ ":" << (size_t)__LINE__ << '\n';
						debug_console << DML_ERROR << current_file->name << ':' << 
							current_file->line << ":   " << raw_line <<			retval << '\n' ;
						goto done;
					}
					raw_line.clear();
					line.clear();
					if(anything_changed)
						*anything_changed |= cx;
				}
			}
		}

		if(flag_popup) {
			current_file->close();
			POP(include_files, current_file);
		}

		if(include_files.size() - last_levels_num == 1)
			enable_output();
		else if( include_files.size() == 0 )
			enable_output();
		else
			disable_output();
		last_levels_num = include_files.size();
	}

done:
	while(include_files.size() > 1) {
		current_file->close();
		delete current_file;
		POP(include_files, current_file);
	}
	current_file->close();

	if(outs != NULL && outs != stdout)
		fclose(outs);
	if(depf != NULL)
		fclose(depf);
	return retval;
}



