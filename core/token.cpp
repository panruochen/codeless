#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/types.h>

#include "tcc.h"
#include "c++-if.h"


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
		runtime_console << "Bad format:" << s << '\n';
		assert(0);
	}
	return fmt;
}

#define ishexdigit(c)  (isdigit(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'))

static TOKEN new_token(TCC_CONTEXT *tc, const CC_STRING& name, short attr = TA_OPERATOR)
{
	TOKEN token;

	token.id   = symtab_insert(tc->symtab, name.c_str());
	token.attr = attr;
#if ! __OPTIMIZE__
	token.symbol = id_to_symbol(tc->symtab, token.id);
	token.base = 0;
#endif
	return token;
}

void new_number(TCC_CONTEXT *tc, TOKEN &token, const CC_STRING& name, int base, const char *format)
{
	token.id     = symtab_insert(tc->symtab, name.c_str());
	token.attr   = TA_UINT;
	sscanf(name.c_str(), format, &token.u32_val);
#if ! __OPTIMIZE__
	token.symbol = strdup(name.c_str());
	token.base   = base;
#endif
}

#define GET_OPERATOR()                       do{ \
    if(cword.size() > 0)                         \
        token = new_token(tc, cword);            \
    if(cword.size() == 2)                        \
        p++;                                     \
    goto done;                                   \
} while(0)

#define GET_NUMBER(base,format)              do{ \
	new_number(tc, token, cword, base, format);  \
    goto done;                                   \
} while(0)

bool take_include_token(TCC_CONTEXT *tc, const char **line, ERR_STRING *errs)
{
	const char *p;
	char c;

	*errs = NULL;
	p = *line;
	SKIP_WHITE_SPACES(p);

	if( (c = *p++) == '<' ) {
		SKIP_WHITE_SPACES(p);
		while( (c = *p++) != '>' ) {
			if( c == '\0' ) {
				*errs = "missing terminating '<' character";
				return false;
			}
		}
		*line = p;
		return true;
	}
	return false;
}


/**  
 * Get one token from the input line and then move the read pointer forward.
 **/
bool read_token(TCC_CONTEXT *tc, const char **line, TOKEN *tokp, ERR_STRING *errs, int flags)
{
	static CC_STRING last_error;
	enum {
		SM_STATE_INITIAL,
		SM_STATE_IDENTIFIER,
		SM_STATE_IDENTIFIER2,
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

		SM_STATE_STRING,
		SM_STATE_STR_ESC,
		SM_STATE_STR_HEX,
	} state;
	bool retval = false;
	CC_STRING cword;
	TOKEN& token = *tokp;
	char c;
	char quation;
	const char *p;
	int base;
	CC_STRING format;
	char char_val;
	bool have_sharp = false;

	/* Initialize */
	state = SM_STATE_INITIAL;
	char_val = -1;
	p = *line;
	SKIP_WHITE_SPACES(p);
	if(*p == '\n' || *p == '\0')
		return false;

	*errs = NULL;
	for(; ; p++) {

		c = *p;
		switch(state) {
		case SM_STATE_INITIAL:
			if( isalpha(c) || c == '_' || c == '#') {
				cword += c;
				have_sharp = (c == '#');
				if( ! (flags & TT_FLAG_GET_SHARP) )
					state = SM_STATE_IDENTIFIER;
				else
					state = SM_STATE_IDENTIFIER2;
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
				state = SM_STATE_SUBTRACTION;
				cword = c;
			} else if(c == '"' || c == '\'') {
				state = SM_STATE_STRING;
				cword   = c;
				quation = c;
			} else if(c == '*' || c == '/' || c == '%' || c == '(' || c == ')' || c == '?' || c == ':'
				|| c == ',' ) {
				cword = c;
                token = new_token(tc, cword);
				p++;
				goto done;
			} else {
				cword = c;
                token = new_token(tc, cword);
				p++;
				goto done;
			}
			break;

		case SM_STATE_IDENTIFIER:
			if( identifier_char(c) )
				cword += c;
			else {
				token = new_token(tc, cword, TA_IDENTIFIER);
				goto done;
			}
			break;

		case SM_STATE_IDENTIFIER2:
			if( isspace(c) )
				;
			else if( identifier_char(c) || c == '#' ) {
				if(c == '#')
					have_sharp = true;
				cword += c;
			} else {
				token = new_token(tc, cword, have_sharp ? TA_MACRO_STR: TA_IDENTIFIER);
				goto done;
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
				*errs = last_error.c_str();
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
				*errs = "Invalid token 0x";
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
				new_number(tc, token, cword, base, get_format(format).c_str());
				p++;
				goto done;
			} else
				GET_NUMBER(base, get_format(format).c_str());
			break;

		case SM_STATE_SINGLE_VBAR:
			if(c == '|')
				cword = "||";
			else
				cword = '|';
			GET_OPERATOR();
			break;

		case SM_STATE_SINGLE_AND:
			if(c == '&')
				cword = "&&";
			else
				cword = '&';
			GET_OPERATOR();
			break;

		case SM_STATE_SINGLE_NOT: /* STATE(!) */
			if(c == '=')
				cword = "!=";
			else
				cword = '!';
			GET_OPERATOR();
			break;

		case SM_STATE_SINGLE_EQUAL: /* STATE(=) */
			if(c == '=')
				cword = "==";
			else
				cword = "=";
			GET_OPERATOR();
			break;

		case SM_STATE_ANGLE_BRACKET: /* STATE(<>) */
			if(c == '=' )
				cword += c;
			else if(cword[0] == c)
				cword += c;
			GET_OPERATOR();
			break;

		case SM_STATE_ADDITION:
			if(c == '+') {
				*errs = "Invalid ++ operator";
				goto error;
			} else {
				GET_OPERATOR();
			}
			break;

		case SM_STATE_SUBTRACTION:
			if(c == '-') {
				*errs = "Invalid -- operator";
				goto error;
			} else {
				GET_OPERATOR();
			}
			break;

		case SM_STATE_STRING :
			cword += c;
			switch( c ) {
			case '"':
				if(quation == '"') {
					token = new_token(tc, cword, TA_STRING);
					p++;
					goto done;
				}
				break;
			case '\'':
				if(quation == '\'') {
					token = new_token(tc, cword, TA_CHAR);
					token.i32_val = char_val;
					p++;
					goto done;
				}
				break;
			case '\\':
				state = SM_STATE_STR_ESC;
				break;
			default:
				char_val = c;
				break;
			}
			break;

		case SM_STATE_STR_ESC :
			cword += c;
			switch( c ) {
			case 'x':
			case 'X':
				state = SM_STATE_STR_HEX;
				break;
			default:
				state = SM_STATE_STRING;
				break;
			}
			break;

		case SM_STATE_STR_HEX:
			cword += c;
			if( ! ishexdigit(c) )
				state = SM_STATE_STRING;
			break;

		}
		if(c == '\0') {
			if(state == SM_STATE_STRING || state == SM_STATE_STR_ESC || state == SM_STATE_STR_HEX) {
				*errs  = "Unterminated string";
				goto error;
			} 
			break;
		}
	}

done:
	*line = p;
	retval = true;

error:
	return retval;
}

