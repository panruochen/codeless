#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <ctype.h>
#include <unistd.h>

#include <sys/types.h>
#include <assert.h>

#include "tcc.h"
#include "precedence-matrix.h"
#include "c++-if.h"

#include <setjmp.h>

struct EH_CONTEXT {
	TCC_CONTEXT      *tc;
	jmp_buf          long_jmp_context;
	const char       *errs;
};

enum ME_STATE {
	ME_STAT_OKAY,
	ME_STAT_LEFT_PARENTHESIS,
	ME_STAT_ARGUMENT,
	ME_STAT_SEPERATOR,
} ;


#define EXCEPTION_MAGIC_NUMBER  128
#define LONG_RETURN(ehc)  longjmp((ehc)->long_jmp_context, EXCEPTION_MAGIC_NUMBER)
#define IS_FLM(mi)     ((mi)->arg_nr >= 0)
#define IS_MACRO(mi)   ((mi) != NULL && (mi) != TCC_MACRO_UNDEF)

static CC_STRING last_err_msg;

static void handle_define(EH_CONTEXT *ehc, sym_t mid, MACRO_INFO *minfo);

/* Find the specific macro in the table. Expand it if it is not expanded.
 *
 */
static MACRO_INFO * find_macro(EH_CONTEXT *ehc, sym_t mid)
{
	MACRO_INFO *mi = macro_table_lookup(ehc->tc->mtab, mid);
	if( IS_MACRO(mi) && mi->id == SSID_INVALID ) {
		const char *err;
		handle_define(ehc, mid, mi);
		free(mi->line); mi->line = NULL;
		return macro_table_lookup(ehc->tc->mtab, mid);
	}
	return mi;
}

void join(CC_STRING& s, const char *start, const char *end)
{
	while(start < end)
		s += *start++;
}

static void join(XCHAR **xc, const char *start, const char *end)
{
	XCHAR *p;
	p = *xc;
	while(start < end)
		*p++ = *start++;
	*xc = p;
}

static void dump(const XCHAR *xc)
{
	while( *xc != 0) {
		if( *xc & XF_MACRO_PARAM )
			printf("\\%u", (uint8_t)*xc );
		else
			printf("%c", *xc);
		xc++;
	}
	printf("\n");
}

static bool get_token(EH_CONTEXT *ehc, const char **line, TOKEN *token)
{
	bool retval;
	retval = read_token(ehc->tc, line, token, &ehc->errs, 0);
	if( ehc->errs != NULL )
		LONG_RETURN(ehc);
	return retval;
}

static void eat_spaces(CC_STRING& s, const char **line)
{
	const char *p = *line;
	while(1) {
		char c = *p;
		if(c == ' ' || c == '\t') {
			s += c;
			p++;
		} else
			break;
	}
	*line = p;
}

static CC_STRING generic_macros_expand(EH_CONTEXT *ehc, const char **line);
static CC_STRING object_like_macro_expand(EH_CONTEXT *ehc, MACRO_INFO *mi);

static CC_STRING function_like_macro_expand(EH_CONTEXT *ehc, MACRO_INFO *mi, const char **line)
{
	CC_STRING outs;
	CC_ARRAY<CC_STRING> margs;
	CC_STRING ma;
	const char *p = *line;
	int level;

	SKIP_WHITE_SPACES(p);
	if( IS_EOL(*p) ) {
		ehc->errs = "[1] macro expects arguments";
		LONG_RETURN(ehc);
	}
	if( *p != '(' ) {
		ehc->errs = "macro expects '('";
		LONG_RETURN(ehc);
	}

	p++;
	SKIP_WHITE_SPACES(p);
	level = 1;
	while( 1 ) {
		if( IS_EOL(*p) )
			break;

		if( *p == ','  ) {
			if( level == 1 ) {
				margs.push_back(ma);
				ma.clear();
			} else
				ma += ',';
		} else if(*p == '(') {
			level ++;
			ma += '(';
		} else if(*p == ')') {
			level --;
			if(level == 0) {
				p++;
				margs.push_back(ma);
				ma.clear();
				break;
			} else
				ma += ')';
		} else
			ma += *p;
		p++;
	}
	*line = p;

done:
	if( 1 ) {
		XCHAR *xc;
		CC_STRING s;
		int n;

		n = mi->arg_nr >= 0 ? mi->arg_nr : 0;
		if(n != margs.size()) {
			char buf[32];
			last_err_msg  = "Macro \"" ;
			last_err_msg += id_to_symbol(ehc->tc->symtab, mi->id);
			last_err_msg += "\" requires ";
			snprintf(buf, sizeof(buf), "%u ", n);
			last_err_msg += buf;
			last_err_msg += "arguments, but ";
			snprintf(buf, sizeof(buf), "%u ", margs.size());
			last_err_msg += buf;
			last_err_msg += "given";
			ehc->errs = last_err_msg.c_str();
			LONG_RETURN(ehc);
		}

		for(xc = mi->define; *xc != 0; xc++) {
			if( (*xc) & XF_MACRO_PARAM ) {
				if(*xc & XF_MACRO_PARAM2)
					s += margs[(uint8_t)*xc];
				else if(*xc & XF_MACRO_PARAM1)
					;
				else {
					const char *p = margs[(uint8_t)*xc].c_str();
					s += generic_macros_expand(ehc, &p);
				}
			} else
				s += (char) (*xc) ;
		}
		outs += s;
	}
	return outs;
}

static CC_STRING object_like_macro_expand(EH_CONTEXT *ehc, MACRO_INFO *mi)
{
	XCHAR *xc;
	CC_STRING outs;

	for(xc = mi->define; *xc != 0; xc++)
		outs += (char) (*xc);
	return outs;
}

static inline bool in_defined_context(const sym_t id[2])
{
	return (id[1] == SSID_DEFINED || (id[0] == SSID_DEFINED && id[1] == SSID_LEFT_PARENTHESIS));
}

static CC_STRING generic_macros_expand(EH_CONTEXT *ehc, const char **line)
{
	TOKEN token;
	CC_STRING outs;
	sym_t last_ids[2] = { SSID_INVALID, SSID_INVALID };

	while(1) {
		const char *last_pos;

		eat_spaces(outs, line);
		last_pos = *line;
		if( ! get_token(ehc, line, &token) )
			break;
		if( token.attr == TA_IDENTIFIER ) {
			MACRO_INFO *mi;
			CC_STRING tmp;
			mi = find_macro(ehc, token.id);
			if( IS_MACRO(mi) && ! in_defined_context(last_ids) ) {
				const char *p1;
				if( IS_FLM(mi) ) {
					p1 = *line;
					SKIP_WHITE_SPACES(p1);
					if( *p1 == '(' )
						tmp = function_like_macro_expand(ehc, mi, line);
					else
						join(outs, last_pos, *line);
				} else
					tmp = object_like_macro_expand(ehc, mi);
					//*line = last_pos;
				p1 = tmp.c_str();
				tmp = generic_macros_expand(ehc, &p1);
				outs += tmp;
			} else
				join(outs, last_pos, *line);
		} else
			join(outs, last_pos, *line);
		last_ids[0] = last_ids[1];
		last_ids[1] = token.id;
	}
	return outs;

error:
	outs.clear();
	return outs;
}


CC_STRING macro_expand(TCC_CONTEXT *tc, const char *line, const char **errs)
{
	int status;
	CC_STRING expansion; 
	EH_CONTEXT ehc;

	ehc.tc = tc;
	status = setjmp(ehc.long_jmp_context);
	if(status ==  0) {
		expansion = generic_macros_expand(&ehc, &line);
		*errs = NULL;
	} else {
		*errs = ehc.errs;
	}
	return expansion;
}

/****** Part II: MACRO RECOGNITION ********/

static int find(sym_t id, const CC_ARRAY<sym_t>& list)
{
	ssize_t i;
	for(i = 0; i < list.size(); i++)
		if( list[i] == id )
			return i;
	return -1;
}

static void dump(const char *a, const char *b)
{
	while(a < b && *a != 0) {
		putchar(*a);
		a++;
	}
	putchar('\n');
}

static void handle_define(EH_CONTEXT *ehc, sym_t mid, MACRO_INFO *minfo)
{
	const char *line = minfo->line;
	TOKEN token;

	if( ! get_token(ehc, &line, &token) )
		return;
	
	if(token.attr == TA_IDENTIFIER) {
		if(*line == '(') {
			CC_ARRAY<sym_t>  para_list;
			enum ME_STATE state;
			XCHAR *xc;
			int32_t para_id;
			const char *last_pos;
		
			state = ME_STAT_LEFT_PARENTHESIS;
			line++;
			while( get_token(ehc, &line, &token) ) {
				switch(state) {
				case ME_STAT_LEFT_PARENTHESIS:	
					if( token.id == SSID_RIGHT_PARENTHESIS) {
						state = ME_STAT_OKAY;
						goto okay;
					} else if(token.attr == TA_IDENTIFIER) {
						para_list.push_back(token.id);
						state = ME_STAT_SEPERATOR;
					} else
						goto error;
					break;
		
				case ME_STAT_SEPERATOR:
					if(token.id == SSID_RIGHT_PARENTHESIS) {
						state = ME_STAT_OKAY;
						goto okay;
					} else if(token.id == SSID_COMMA)
						state = ME_STAT_ARGUMENT;
					else
						goto error;
					break;
		
				case ME_STAT_ARGUMENT:
					if(token.attr != TA_IDENTIFIER)
						goto error;
					para_list.push_back(token.id);
					state = ME_STAT_SEPERATOR;
					break;

				case ME_STAT_OKAY:
					assert(0);
				}
			}

			if(state != ME_STAT_OKAY) {
		error:
				ehc->errs = "Invalid macro definition";
				LONG_RETURN(ehc);
			}
		
		okay:
			xc = (XCHAR*) malloc(sizeof(XCHAR) * strlen(line) + 4);
			minfo->id         = mid;
			minfo->define     = xc;
			minfo->arg_nr     = para_list.size();

			SKIP_WHITE_SPACES(line);
			last_pos = line;

			while(  read_token(ehc->tc, &line, &token, &ehc->errs, TT_FLAG_GET_SHARP) ) {
				if(ehc->errs != NULL)
					LONG_RETURN(ehc);
				
				if( token.attr == TA_IDENTIFIER && (para_id = find(token.id, para_list)) >= 0 ) {
					*xc ++ = (para_id | XF_MACRO_PARAM);
				} else if(token.attr == TA_MACRO_STR) {
					const char *p1 = last_pos, *p0;
					TOKEN x;
					CC_STRING prefix;

					SKIP_WHITE_SPACES(p1);
					while( p1 < line ) {
						prefix.clear();
						if( *p1 == '#' ) {
							p1++;
							prefix += '#';
							if( *p1 == '#' ) {
								p1 ++;
								prefix += '#';
							}
						}
						SKIP_WHITE_SPACES(p1);
						p0 = p1;
						if( ! read_token(ehc->tc, &p1, &token, &ehc->errs, 0) )
							break;
						if( prefix.size() > 0 && (para_id = find(token.id, para_list)) >= 0 ) {
							if(prefix.size() == 1) 
								*xc++ = para_id | XF_MACRO_PARAM | XF_MACRO_PARAM1;
							else
								*xc++ = para_id | XF_MACRO_PARAM | XF_MACRO_PARAM2;
						} else {
							join(&xc, p0, p1);
						}
						SKIP_WHITE_SPACES(p1);
					}
				} else {
					join(&xc, last_pos, line);
				}
				last_pos = line;
//				dump(minfo->define);
			}
			*xc = 0;
		} else {
			XCHAR *xc;

			xc = (XCHAR*) malloc(sizeof(XCHAR) * strlen(line) + 4);
			minfo->id      = mid;
			minfo->define  = xc;
			minfo->arg_nr  = -1;

			SKIP_WHITE_SPACES(line);
			while(1) {
				char c = *line;
				if( IS_EOL(c) ) {
					*xc = '\0';
					break;
				}
				*xc = c;
				xc++, line++;
			}
		}
	}
}

void handle_undef(TCC_CONTEXT *tc, const char *line)
{
	TOKEN token;
	ERR_STRING errs;
	read_token(tc, &line, &token, &errs, 0);
	if( ! rtm_preprocess )
		macro_table_insert(tc->mtab, token.id, TCC_MACRO_UNDEF);
	else {
		MACRO_INFO *mi = macro_table_lookup(tc->mtab, token.id);
		if( IS_MACRO(mi) )
			free(mi->define);
		macro_table_delete(tc->mtab, token.id);
	}
}

