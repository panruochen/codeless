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

static inline IF_VALUE logic_not(IF_VALUE val)
{
	return (val == IV_UNKNOWN) ? IV_UNKNOWN : 
		(val == IV_FALSE ? IV_TRUE : IV_FALSE);
}

static bool calculate(const TOKEN& opnd1, const sym_t opr, const TOKEN& opnd2, TOKEN& result)
{
#define CMP(x,opr,val) \
	(((x).attr == TA_UINT || (x).attr == TA_INT)&& (x).u32_val opr (val))

	result.id = SSID_SYMBOL_X;
	if(opnd1.attr == TA_IDENTIFIER || opnd2.attr == TA_IDENTIFIER) {
		result.attr = TA_IDENTIFIER;
		result.u32_val = 0;
		if(opr == SSID_LOGIC_OR && ( CMP(opnd1,!=,0) || CMP(opnd2,!=,0))) {
			result.attr  = TA_UINT;
			result.u32_val = true;
		}
		if(opr == SSID_LOGIC_AND && (CMP(opnd1,==,0) || CMP(opnd2,==,0))) {
			result.attr  = TA_UINT;
			result.u32_val = false;
		}
		return true;
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
		if(opnd1.attr == TA_INT && opnd2.attr == TA_INT)
			result.b_val = opnd1.i32_val < opnd2.i32_val;
		else
			result.b_val = opnd1.u32_val < opnd2.u32_val;
		break;
	case SSID_LESS_EQUAL:
		if(opnd1.attr == TA_INT && opnd2.attr == TA_INT)
			result.b_val = opnd1.i32_val <= opnd2.i32_val;
		else
			result.b_val = opnd1.u32_val <= opnd2.u32_val;
		break;
	case SSID_GREATER:
		if(opnd1.attr == TA_INT && opnd2.attr == TA_INT)
			result.b_val = opnd1.i32_val > opnd2.i32_val;
		else
			result.b_val = opnd1.u32_val > opnd2.u32_val;
		break;
	case SSID_GREATER_EQUAL:
		if(opnd1.attr == TA_INT && opnd2.attr == TA_INT)
			result.b_val = opnd1.i32_val >= opnd2.i32_val;
		else
			result.b_val = opnd1.u32_val >= opnd2.u32_val;
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
	default:
		return false;
	}
	return true;
}

static bool deduce(TCC_CONTEXT *tc, sym_t opr, TOKEN_ARRAY& opnd_stack, CC_ARRAY<sym_t>& opr_stack)
{
	TOKEN a, b;
	TOKEN result = { SSID_SYMBOL_X, TA_UINT };

	if( opr == SSID_COLON ) {
		TOKEN c;
		sym_t tmp_opr = SSID_INVALID;
		if(opnd_stack.size() < 3)
			return false;
		POP(opr_stack,tmp_opr);
		if(tmp_opr != SSID_QUESTION)
			return false;
		POP(opnd_stack, c);
		POP(opnd_stack, b);
		POP(opnd_stack, a);
		
		result = a.u32_val ? b : c;
		goto done;
	} else if( opr != SSID_LOGIC_NOT && opr != SSID_BITWISE_NOT ) {
		if( ! POP(opnd_stack, b) )
			return false;
		if( ! POP(opnd_stack, a) )
			return false;

		debug_console << DML_DEBUG << &a << TCC_DEBUG_CONSOLE::endl;
		debug_console << DML_DEBUG << id_to_symbol(tc->symtab, opr);
		debug_console << DML_DEBUG << &b << TCC_DEBUG_CONSOLE::endl;
	} else {
		if( ! POP(opnd_stack, a) )
			return false;
		debug_console << DML_DEBUG << "! ";
		debug_console << DML_DEBUG << &a << TCC_DEBUG_CONSOLE::endl;
	}
	if( ! calculate(a, opr, b, result) )
		return false;
done:
	PUSH(opnd_stack, result);
	return true;
}

static bool is_opr(sym_t sym)
{
	return sym < SSID_SYMBOL_X;
}

int check_symbol_defined(TCC_CONTEXT *tc, sym_t id, bool reverse, TOKEN *token)
{
	short attr;
	int retval;
	MACRO_INFO *minfo = macro_table_lookup(tc->mtab, id);

	if( ! preprocess_mode ) {
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
	if(token != NULL) {
		token->attr    = attr;
		token->u32_val = retval;
		token->id      = SSID_SYMBOL_X;
#if HAVE_NAME_IN_TOKEN
		token->name = id_to_symbol(tc->symtab, SSID_SYMBOL_X);
#endif
	}
	if(preprocess_mode)
		assert(retval != IV_UNKNOWN);
	return retval;
}

static int expression_evaluate(TCC_CONTEXT *tc, TOKEN_ARRAY& tokens)
{
	size_t i;
	CC_ARRAY<sym_t> opr_stack;
	TOKEN_ARRAY     opnd_stack;
	bool last_is_opr;

	debug_console << DML_DEBUG <<  "Original Expression: " << tokens << TCC_DEBUG_CONSOLE::endl << TCC_DEBUG_CONSOLE::endl ; 
	do_macro_expansion(tc, tokens);	
	debug_console << DML_DEBUG << "Expanded Expression: " << tokens << TCC_DEBUG_CONSOLE::endl << TCC_DEBUG_CONSOLE::endl ; 
 
	opr_stack.push_back(SSID_SHARP);
	last_is_opr = true;
	for(i = 0; i < tokens.size(); i++) {
		sym_t opr = tokens[i].id;
		if( is_opr(opr) ) {
			sym_t opr0 = opr_stack.back();
			int result;

			result = oprmx_compare(tc->matrix, opr0, opr);
			switch(result) {
			case OP_EQUAL:
				opr_stack.pop_back(opr0);
				break;
			case OP_HIGHER:
				POP(opr_stack, opr0);
				if( ! deduce(tc, opr0, opnd_stack, opr_stack) )
					return false;
				i --;
				break;
			case OP_LOWER:
				PUSH(opr_stack, opr);
				break;
			case OP_ERROR:
				debug_console << DML_ERROR << "Cannot compare precedence for `" << id_to_symbol(tc->symtab,opr0) << "' and `" <<
					id_to_symbol(tc->symtab,opr) << '\'' << TCC_DEBUG_CONSOLE::endl ;
				return IV_UNKNOWN;
			}
			last_is_opr = true;
		} else {
			if( ! last_is_opr ) {
				debug_console << DML_ERROR << "Adjacent operands" << TCC_DEBUG_CONSOLE::endl ;
				return IV_UNKNOWN;
			}
			opnd_stack.push_back(tokens[i]);
			last_is_opr = false;
		}
	}

	debug_console << DML_DEBUG <<  "OPR  Stack: " << opr_stack << TCC_DEBUG_CONSOLE::endl << TCC_DEBUG_CONSOLE::endl ; 
	debug_console << DML_DEBUG <<  "OPND Stack: " << opnd_stack << TCC_DEBUG_CONSOLE::endl << TCC_DEBUG_CONSOLE::endl ; 
	do {
		sym_t opr0;
		int result;

		if( !POP(opr_stack, opr0) )
			return IV_UNKNOWN;
		result = oprmx_compare(tc->matrix, opr0, SSID_SHARP);
		switch(result) {
		case OP_EQUAL:
			break;
		case OP_HIGHER:
			if( ! deduce(tc, opr0, opnd_stack, opr_stack) )
				return IV_UNKNOWN;
			break;
		default:
			debug_console << DML_ERROR << __func__ << ':' << (size_t)__LINE__ << ": bad expression" << TCC_DEBUG_CONSOLE::endl;
			return IV_UNKNOWN;
		}
	} while( opr_stack.size() != 0 );

	if( opnd_stack.size() != 1 ) {
		debug_console << DML_ERROR << __func__ << ':' << (size_t)__LINE__ << ": Opnd stack size: " << opnd_stack.size() << TCC_DEBUG_CONSOLE::endl;
		return IV_UNKNOWN;
	}
	if( opnd_stack.back().attr == TA_IDENTIFIER )
		return IV_UNKNOWN;
	return !!opnd_stack.back().i32_val;		
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

const char * ConditionalParser::run(sym_t preprocessor, const char *line, TOKEN_ARRAY& tokens)
{
	const char *retval = NULL;
	IF_VALUE result = IV_FALSE;
	CC_STRING new_line;
	const char *output = raw_line.c_str();

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
				mi->handled = false;
				mi->line = strdup(line);
				mi->priv = (void *)this;
				macro_table_insert(tc->mtab, mid, mi);
				assert(macro_table_lookup(tc->mtab, mid) != NULL);
			}
		}
		if(current.curr_value == IV_FALSE)
			goto done;
		else
			goto print_and_exit;
	} else if(preprocessor != SSID_INVALID && preprocessor != SSID_SHARP_INCLUDE && preprocessor != SSID_SHARP_INCLUDE_NEXT ) {
		retval = split(preprocessor, line, tokens);
/*		fprintf(stderr, "LINE: %s\n", line);
		for(size_t i = 0; i < tokens.size(); i++) {
			fprintf(stderr, "[%u]:  %s\n", i, id_to_symbol(tc->symtab,tokens[i].id));
		}
		fprintf(stderr, "\n\n");*/
		if(retval != NULL) {
			runtime_console << __FILE__ ":" << (size_t)__LINE__ << BASIC_CONSOLE::endl;
			runtime_console << current_file->name << ':' << current_file->line << ": " << retval << BASIC_CONSOLE::endl ;
			return retval;
		}
	}

#define WARN_1() \
	debug_console << current_file->name << ':' << current_file->line << ": " << raw_line.c_str() << '\n'
	switch(preprocessor) {
		case SSID_SHARP_IF:
			if( current.curr_value != IV_FALSE ) {
				result = (IF_VALUE) expression_evaluate(tc, tokens);
			}
			goto handle_if_branch;
		case SSID_SHARP_IFNDEF:
			if( current.curr_value != IV_FALSE ) {
				result = (IF_VALUE) check_symbol_defined(tc, tokens[0].id, true, NULL);
			}
			goto handle_if_branch;
		case SSID_SHARP_IFDEF:
			if( current.curr_value != IV_FALSE ) {
				result = (IF_VALUE) check_symbol_defined(tc, tokens[0].id, false, NULL);
			}
handle_if_branch:
			if( preprocess_mode && result == IV_UNKNOWN) {
				char buf[32];
				last_error = current_file->name;
				last_error += ':';
				sprintf(buf, "%u", current_file->line);
				last_error += buf;
				last_error += ':';
				last_error += " Cannot evaluate (A): ";
				last_error += raw_line;
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
			result = (IF_VALUE) expression_evaluate(tc, tokens);
			if(preprocess_mode && result == IV_UNKNOWN) {
				char buf[32];
				last_error = current_file->name;
				last_error += ':';
				sprintf(buf, "%u", current_file->line);
				last_error += buf;
				last_error += ':';
				last_error += " Cannot evaluate (B): ";
				last_error += raw_line;
				retval = last_error.c_str();

				goto done;
			}
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
					/*handle_define(tc, tokens)*/;
				else if(preprocessor == SSID_SHARP_UNDEF)
					handle_undef(tc, tokens[0]);
				else if( preprocess_mode && (preprocessor == SSID_SHARP_INCLUDE || preprocessor == SSID_SHARP_INCLUDE_NEXT) ) {
					CC_STRING tmp;
					const char *path;
					char style;
					bool in_sys_dir;

					tmp = get_include_file_name(line, style);
					if(tmp.size() == 0) {
						last_error = "Error: ";
						last_error = raw_line;
						retval = last_error.c_str();
						goto done;
					}
					path = check_file(tmp.c_str(), style == '"', preprocessor == SSID_SHARP_INCLUDE_NEXT, &in_sys_dir);
					if(path != NULL) {
						FILE *fp = fopen(path, "rb");
						if(fp != NULL) {
							REAL_FILE *file = new REAL_FILE;
							file->set_fp(fp, path);
							/* Usually, we ignore those header files in the `system' search directories */
							if(dep_fp != NULL && ! in_sys_dir) {
//								fprintf(dep_fp, "//-%s:%d: %s\n", current_file->name.c_str(), current_file->line, raw_line.c_str());
								fprintf(dep_fp, "#include  ", path);
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
	}
done:
	comment_start = -1;
	return retval;
}


#define IS_EOL(c)      ((c) == '\n')
#define ishexdigit(c)  (isdigit(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'))

static TOKEN new_token(TCC_CONTEXT *tc, const CC_STRING& name, short attr = TA_OPERATOR)
{
	TOKEN token;

	token.flag_flm = 0;
	token.id   = symtab_insert(tc->symtab, name.c_str());
	token.attr = attr;
#if HAVE_NAME_IN_TOKEN
	token.name = id_to_symbol(tc->symtab, token.id);
	token.base = 0;
#endif
	return token;
}

static TOKEN new_token(TCC_CONTEXT *tc, const CC_STRING& name, int base, const char *format)
{
	TOKEN token;

	token.id   = symtab_insert(tc->symtab, name.c_str());
	token.attr = TA_UINT;
#if HAVE_NAME_IN_TOKEN
	token.name = strdup(name.c_str());
	token.base = base;
#endif
	sscanf(name.c_str(), format, &token.u32_val);
	return token;
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

static void get_number(TCC_CONTEXT *tc, TOKEN_ARRAY& tokens, CC_STRING& cword, int base, const char *format)
{
	if(cword.size() > 0) {
		TOKEN token;
		size_t n;
		int sign;

		token = new_token(tc, cword, base, format);
		n = tokens.size();
		if((n == 1 && (sign = get_sign(tokens[0])) != 0) ||
			(n > 1 && is_arithmetic_operator(tokens[n-2]) && (sign = get_sign(tokens[n-1])) != 0)) {
			if(sign < 0) {
				token.u32_val = -token.u32_val;
				token.attr = TA_INT;
			}
			tokens[n-1] = token;
		} else 
			tokens.push_back(token);
		cword.clear();
	}
}


static void get_token(TCC_CONTEXT *tc, TOKEN_ARRAY& tokens, CC_STRING& cword, short attr)
{
	if(cword.size() > 0) {
		TOKEN token;
		token = new_token(tc, cword, attr);
		tokens.push_back(token);
		cword.clear();
	}
}


sym_t ConditionalParser::preprocessed_line(const char *line, const char **pos)
{
	const char *p;
	CC_STRING word;

	p = line;
	SKIP_WHITE_SPACES(p);
	if( *p++ != '#' )
		return SSID_INVALID;
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

static bool identifier_char(char c)			
{
	return isalpha(c) || isdigit(c) || c == '_';
}

static CC_STRING get_format(const CC_STRING& s)
{
	CC_STRING fmt;

	fmt = '%';
	
	switch(s.size()) {
	case 3:
		fmt += s[2];
	case 2:
		fmt += s[1];
	case 1:
		fmt += s[0];
		break;
	default:
		runtime_console << "Bad format:" << s << BASIC_CONSOLE::endl;
		assert(0);
	}
	return fmt;
}

#define GET_TOKEN(attr)              do{ \
	get_token(tc,tokens,cword,attr);     \
	/*cword.clear();*/                   \
	state = SM_STATE_INITIAL;            \
	p--;                                 \
} while(0)


#define GET_NUMBER(base,format)              do{ \
	get_number(tc,tokens,cword,base,format);     \
	/*cword.clear();*/                           \
	state = SM_STATE_INITIAL;                    \
	p--;                                         \
} while(0)


/*  Split the input line into tokens if this line contains proprocessing contents.
 *
 *  Returns a pointer to the error messages on failure, or nil on success.
 */
const char * ConditionalParser::split(sym_t tid, const char *line, TOKEN_ARRAY& tokens)
{
	enum {
		SM_STATE_INITIAL,
		SM_STATE_IDENTIFIER,
		SM_STATE_NUM0,
		SM_STATE_0X,
		SM_STATE_DEC_NUM,
		SM_STATE_OCT_NUM,
		SM_STATE_HEX_NUM,
		SM_STATE_NUM_POSTFIX1,
		SM_STATE_NUM_POSTFIX2,
		SM_STATE_SINGLE_VBAR,
		SM_STATE_SINGLE_AND,
		SM_STATE_SINGLE_NOT,
		SM_STATE_SINGLE_EQUAL,
		SM_STATE_ANGLE_BRACKET,
		SM_STATE_ADDITION,
		SM_STATE_SUBTRACTION,
	} state;
	const char *retval = NULL;
	char c;
	const char *p;
	int base;
	CC_STRING format;

	/* Initialize */
	state = SM_STATE_INITIAL;
	cword.clear();	
	p = line;
	SKIP_WHITE_SPACES(p);

	for(; (c = *p) != 0; p++) {

		switch(state) {
		case SM_STATE_INITIAL:
			if( isalpha(c) || c == '_' ) {
				cword += c;
				state = SM_STATE_IDENTIFIER;
			} else if( c == '0' ) {
				cword += c;
				state = SM_STATE_NUM0;
			} else if( c >= '1' && c <= '9' ) { 
				cword += c;
				state = SM_STATE_DEC_NUM;
			} else if(isspace(c) || c == '\n') {
			} else if(c == '|') {
				state = SM_STATE_SINGLE_VBAR ;
			} else if(c == '&') {
				state = SM_STATE_SINGLE_AND;
			} else if(c == '!') {
				state = SM_STATE_SINGLE_NOT;
			} else if(c == '=') { 
				state = SM_STATE_SINGLE_EQUAL;
			} else if (c == '<' || c == '>') {
				state = SM_STATE_ANGLE_BRACKET;
				cword = c;
			} else if(c == '+' ) {
				state = SM_STATE_ADDITION;
				cword = c;
			} else if(c == '-' ) {
				state = SM_STATE_ADDITION;
				cword = c;
			} 
			else if(c == '*' || c == '/' || c == '%' || c == '(' || c == ')' || c == '?' || c == ':'
				|| c == ',' ) {
				char name[2];
				name[0] = c;
				name[1] = '\0';
				tokens.push_back(new_token(tc, name));
				state = SM_STATE_INITIAL;
			} else {
				last_error  = "Invalid operator: ";
				last_error += c;
				retval = last_error.c_str();
				goto error;
			}
			break;

		case SM_STATE_IDENTIFIER:
			if( identifier_char(c) ) 
				cword += c;
			else {
				GET_TOKEN(TA_IDENTIFIER);
				if(c == '(' && tokens.size() == 1 && tid == SSID_SHARP_DEFINE ) {
					/* Macro with parameters */;
					tokens[0].attr = TA_IDENTIFIER;
					tokens[0].flag_flm = 1;
				}
			}
			break;

		case SM_STATE_NUM0:
			if(c == 'X' || c == 'x') {
				cword += c;
				state = SM_STATE_0X;
			} else if(c >= '0' && c <= '7') {
				cword += c;
				state = SM_STATE_OCT_NUM;
			} else if(c == '8' || c == '9') {
				last_error  = "Invalid number: ";
				last_error += c;
				retval = last_error.c_str();
				goto error;
			} else
				GET_NUMBER(10,"%u");
			break;
		
		case SM_STATE_OCT_NUM:
			if(c >= '0' && c <= '7')
				cword += c;
			else
				GET_NUMBER(8,"%o");
			break;

		case SM_STATE_0X:
			if( ishexdigit(c) ) {
				cword += c;
				state = SM_STATE_HEX_NUM;
			} else {
				retval = "Invalid token 0x";
				goto error;
			}
			break;

		case SM_STATE_HEX_NUM:
			if( ishexdigit(c) )
				cword += c;
			else if(c == 'u' || c == 'U') {
				base = 16;
				format = 'x';
				state = SM_STATE_NUM_POSTFIX1;
			} else if(c == 'L' || c == 'L') {
				base = 16;
				format = 'x';
				state = SM_STATE_NUM_POSTFIX2;
			} else {
				GET_NUMBER(16,"%x");
			}
			break;

		case SM_STATE_DEC_NUM:
			if( isdigit(c) )
				cword += c;
			else if(c == 'u' || c == 'U') {
				state = SM_STATE_NUM_POSTFIX1;
				base = 10;
				format = 'u';
			} else if(c == 'L' || c == 'l') {
				state = SM_STATE_NUM_POSTFIX2;
				base = 10;
				format = 'd';
			} else
				GET_NUMBER(10,"%u");
			break;

		case SM_STATE_NUM_POSTFIX1:
			if(c == 'L' || c == 'l') {
				format = 'l' + format ;
				state = SM_STATE_NUM_POSTFIX2;
			} else
				GET_NUMBER(base, get_format(format).c_str());
			break;
		case SM_STATE_NUM_POSTFIX2:
			if(c == 'L' || c == 'l') {
				format = 'l' + format ;
				get_number(tc, tokens, cword, base, get_format(format).c_str());
				state = SM_STATE_INITIAL;
			} else
				GET_NUMBER(base, get_format(format).c_str());
			break;
		
		case SM_STATE_SINGLE_VBAR:
			if(c == '|') {
				tokens.push_back(new_token(tc,"||"));
				state = SM_STATE_INITIAL;
			} else {
				cword = '|';
				GET_TOKEN(TA_OPERATOR);
			}
			break;

		case SM_STATE_SINGLE_AND:
			if(c == '&') {
				tokens.push_back(new_token(tc, "&&"));
				state = SM_STATE_INITIAL;
			} else {
				cword = '&';
				GET_TOKEN(TA_OPERATOR);
			}
			break;

		case SM_STATE_SINGLE_NOT: /* STATE(!) */
			if(c == '=') {
				tokens.push_back(new_token(tc, "!="));
				state = SM_STATE_INITIAL;
			} else {
				cword = '!';
				GET_TOKEN(TA_OPERATOR);
			}
			break;

		case SM_STATE_SINGLE_EQUAL: /* STATE(=) */
			if(c == '=') {
				tokens.push_back(new_token(tc,"=="));
				state = SM_STATE_INITIAL;
			} else {
				GET_TOKEN(TA_OPERATOR);
			}
			break;

		case SM_STATE_ANGLE_BRACKET: /* STATE(<>) */
			if(c == '=') {
				cword += c;
				tokens.push_back(new_token(tc, cword));
				cword.clear();
				state = SM_STATE_INITIAL;
			} else {
				GET_TOKEN(TA_OPERATOR);
			}
			break;

		case SM_STATE_ADDITION:
			if(c == '+') {
				retval = "Invalid ++ operator";
				goto error;
			} else {
				GET_TOKEN(TA_OPERATOR);
			}
			break;

		case SM_STATE_SUBTRACTION:
			if(c == '-') {
				retval = "Invalid -- operator";
				goto error;
			} else {
				GET_TOKEN(TA_OPERATOR);
			}
			break;
		}
	}

#if 0
	size_t i;
	for(i = 0; i < tokens.size(); i++)
		fprintf(stderr, "**  %s\n", tokens[i].name);
#endif

error:
	return retval;
}


/*  Parse the input file and update the global symbol table and macro table,
 *  output the stripped file contents to devices if OUTFILE is not nil.
 *
 *  Returns a pointer to the error messages on failure, or nil on success.
 */
const char *ConditionalParser::do_parse(TCC_CONTEXT *tc, const char **keywords, size_t num_keywords,
	GENERIC_FILE *infile, const char *outfile, FILE *depf)
{
	int last_levels_num = 0;
	FILE *outs = stdout;
	int state = 0;
	int prev;
	static char message[512];
	const char *retval = NULL;
	bool loop ;

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
				line.push_back(c);
				if(c == '\n') {
					TOKEN_ARRAY tokens;
					const char *pos;
					sym_t id = preprocessed_line(line.c_str(), &pos);
					retval = run(id, pos, tokens);
					if( retval != NULL) {
						runtime_console << __FILE__ ":" << (size_t)__LINE__ << BASIC_CONSOLE::endl;
						runtime_console << current_file->name << ':' <<  current_file->line << ": " << retval << BASIC_CONSOLE::endl ;
						goto done;
					}
					raw_line.clear();
					line.clear();
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


const TOKEN g_unevaluable_token = { 
	SSID_SYMBOL_X, TA_IDENTIFIER, {0},
#if HAVE_NAME_IN_TOKEN
	0,
	"*X*"
#endif
};

