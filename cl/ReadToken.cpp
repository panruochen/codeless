#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/types.h>

#include "InternalTables.h"
#include "cc_string.h"
#include "cc_array.h"
#include "base.h"

static CC_STRING get_format(const CC_STRING& s, CC_STRING& error)
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
		error.Format("Bad format: %s\n", s.c_str());
	}
	return fmt;
}

static SynToken new_token(InternalTables *intab, const CC_STRING& name, short attr = SynToken::TA_OPR)
{
	SynToken token;

	token.id   = intab->syLut.Put(name);
	token.attr = attr;
    token.name = name;
	return token;
}

static void new_number(InternalTables *intab, SynToken &token, const CC_STRING& name, int base, const CC_STRING& format)
{
	token.id     = intab->syLut.Put(name);
	token.attr   = SynToken::TA_UINT;
	sscanf(name.c_str(), format.c_str(), &token.u32_val);
    token.name = name;
}

#define GET_OPERATOR()                       do{ \
    if(cword.size() > 0)                         \
        token = new_token(intab, cword);            \
    if(cword.size() == 2)                        \
        p++;                                     \
    goto done;                                   \
} while(0)

#define GET_NUMBER(base,format)              do{ \
	new_number(intab, token, cword, base, format);  \
    goto done;                                   \
} while(0)

#define GET_FORMAT(fmt)                  do { \
	format = get_format(fmt,req->error);      \
	if(!req->error.isnull())                  \
	    goto error;                           \
} while(0)


/**
 * Get one token from the input line and then move the read pointer forward.
 **/
int ReadToken(InternalTables *intab, ReadReq *req, bool for_include)
{
	CC_STRING e_msg;
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
		SM_STATE_LESS,
		SM_STATE_MORE,
		SM_STATE_ADDITION,
		SM_STATE_SUBTRACTION,

		SM_STATE_STRING,
		SM_STATE_STR_ESC,
		SM_STATE_STR_HEX,

		SM_STATE_SHARP,
		SM_STATE_DOT,
		SM_STATE_DOT2,
	} state;
	int retval = -1;
	CC_STRING cword;
	SynToken& token = req->token;
	char c;
	char quation = 0;
	const char *p;
	int base = 0;
	CC_STRING format;
	char char_val;

	/* Initialize */
	state = SM_STATE_INITIAL;
	char_val = -1;
	p = req->cp;
	skip_blanks(p);
	if(*p == '\n' || *p == '\0')
		return false;

	for(; ; p++) {

		c = *p;
		switch(state) {
		case SM_STATE_INITIAL:
			switch (c)
			{
			case '0':
				cword += c;
				state = SM_STATE_NUM0;
				break;
			case ' ':
			case '\t':
			case '\n':
				break;
			case '|':
				state = SM_STATE_SINGLE_VBAR ;
				break;
			case '&':
				state = SM_STATE_SINGLE_AND;
				break;
			case '!':
				state = SM_STATE_SINGLE_NOT;
				break;
			case '=':
				state = SM_STATE_SINGLE_EQUAL;
				break;
			case '<':
				state = SM_STATE_LESS;
				cword = c;
				break;
			case '>':
				state = SM_STATE_MORE;
				cword = c;
				break;
			case '+':
				state = SM_STATE_ADDITION;
				cword = c;
				break;
			case '-':
				state = SM_STATE_SUBTRACTION;
				cword = c;
				break;
			case '#':
				state = SM_STATE_SHARP;
				cword = c;
				break;
			case '"':
			case '\'':
			got_string:
				state = SM_STATE_STRING;
				cword   = c;
				quation = c;
				break;
			case '.':
				state = SM_STATE_DOT;
				cword = c;
				break;
			case '*':
			case '/':
			case '%':
			case '(':
			case ')':
			case '?':
			case ':':
			case ',':
				cword = c;
				token = new_token(intab, cword);
				p++;
				goto done;
			default:
				if( c >= '1' && c <= '9' ) {
					cword += c;
					state = SM_STATE_DEC_NUM;
				} else if( isalpha(c) || c == '_' ) {
					cword += c;
					state = SM_STATE_IDENTIFIER;
				} else {
					cword = c;
		               token = new_token(intab, cword);
					p++;
					goto done;
				}
				break;
			}
			break;

		case SM_STATE_IDENTIFIER:
			if( is_id_char(c) )
				cword += c;
			else if(c == '"' || c == '\'') {
				goto got_string;
			} else {
				token = new_token(intab, cword, SynToken::TA_IDENT);
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
				e_msg  = "Invalid number: ";
				e_msg += c;
				req->error = e_msg;
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
			if( isxdigit(c) ) {
				cword += c;
				state = SM_STATE_HEX_NUM;
			} else {
				req->error = "Invalid token 0x";
				goto error;
			}
			break;

		case SM_STATE_HEX_NUM:
			if( isxdigit(c) )
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
			} else {
				GET_FORMAT(format);
				GET_NUMBER(base, format);
			}
			break;
		case SM_STATE_NUM_POSTFIX2:
			if(c == 'L' || c == 'l') {
				format = 'l' + format ;
				p++;
			}
			GET_FORMAT(format);
			GET_NUMBER(base, format);
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

		case SM_STATE_LESS:
		case SM_STATE_MORE: /* STATE(<>) */
			if( ! for_include ) {
				if(c == '=' )
					cword += c;
				else if(cword[0] == c)
					cword += c;
				GET_OPERATOR();
			} else {
				cword += c;
				if(c == '>') {
					p++;
					goto done;
				}
			}
			break;

		case SM_STATE_ADDITION:
			if(c == '+')
				cword = "++";
			GET_OPERATOR();
			break;

		case SM_STATE_SUBTRACTION:
			if(c == '-')
				cword = "--";
			GET_OPERATOR();
			break;

		case SM_STATE_SHARP:
			if(c == '#')
				cword += c;
			GET_OPERATOR();
			break;

		case SM_STATE_STRING :
			cword += c;
			switch( quation ) {
			case '"':
				if(c == '"') {
					token = new_token(intab, cword, SynToken::TA_STR);
					p++;
					goto done;
				} else if(c == '\\')
					state = SM_STATE_STR_ESC;
				break;
			case '\'':
				if(c == '\'') {
					token = new_token(intab, cword, SynToken::TA_CHAR);
					token.i32_val = char_val;
					p++;
					goto done;
				} else if(c == '\\')
					state = SM_STATE_STR_ESC;
				else
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
				char_val = c;
				state = SM_STATE_STRING;
				break;
			}
			break;

		case SM_STATE_STR_HEX:
			cword += c;
			if( ! isxdigit(c) )
				state = SM_STATE_STRING;
			break;

		case SM_STATE_DOT:
			if(c == '.') {
				cword += c;
				state = SM_STATE_DOT2;
			} else
				GET_OPERATOR();
			break;

		case SM_STATE_DOT2:
			cword += c;
			token = new_token(intab, cword);
			p++;
			goto done;
		}
		if(c == '\0') {
			if(state == SM_STATE_STRING || state == SM_STATE_STR_ESC || state == SM_STATE_STR_HEX) {
				req->error = "Unterminated string";
				goto error;
			}
			break;
		}
	}

done:
	req->cp = p;
	retval = true;

error:
	return retval;
}

