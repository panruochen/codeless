#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <ctype.h>
#include <unistd.h>

#include <sys/types.h>
#include <assert.h>

#include "ycpp.h"

class CMaExpander {
public:
	CMaExpander(TCC_CONTEXT *tc, const char *line, bool for_include);
	CC_STRING   TryExpand();

private:
	const char *  pos;
	TCC_CONTEXT*  tc;
	CC_STRING     inStr;
	bool          for_include;
	sym_t         last_ids[2];
	CException    gex;

	CC_STRING     Expand_FLM(CMacro *ma);
	CC_STRING     Expand_OLM(CMacro *ma);
	CMacro*       GetMacro(sym_t mid);
	inline bool   in_defined_context();
	
	bool          ReadToken(CToken *token);
	void          IgnoreSpaces();
	bool          IsValidToken(const CC_STRING& s);
	void          Trim(CC_STRING &s);
};

inline bool CMaExpander::in_defined_context()
{
	return (last_ids[1] == SSID_DEFINED || 
		(last_ids[0] == SSID_DEFINED && last_ids[1] == SSID_LEFT_PARENTHESIS));
}



#define IS_FLM(ma)        ((ma)->nr_args != CMacro::OL_M)
#define IS_MACRO(ma)      ((ma) != NULL && (ma) != CMacro::NotDef)

static void handle_define(TCC_CONTEXT *tc, sym_t mid, CMacro *ma);

/* Find the specific macro in the table. Expand it if it is not expanded.
 *
 */
CMacro * CMaExpander::GetMacro(sym_t mid)
{
	CMacro *ma = tc->maMap.Lookup(mid);
	if( IS_MACRO(ma) && ma->id == SSID_INVALID ) {
		handle_define(tc, mid, ma);
		free(ma->line);
		ma->line = NULL;
		return tc->maMap.Lookup(mid);
	}
	return ma;
}

static inline void join(XCHAR **xc, const char *start, const char *end)
{
	XCHAR *p;
	p = *xc;
	while(start < end)
		*p++ = *start++;
	*xc = p;
}

static void __NO_USE__ dump(const XCHAR *xc)
{
	CC_STRING s;
	char buf[32];
	while( *xc != 0) {
		if( IS_MA_PAR0(*xc) )
			snprintf(buf, sizeof(buf), "\\%u", (uint8_t)*xc );
		else if( IS_MA_PAR1(*xc) )
			snprintf(buf, sizeof(buf), "#\\%u", (uint8_t)*xc );
		else if( IS_MA_PAR2(*xc) )
			snprintf(buf, sizeof(buf), "##\\%u", (uint8_t)*xc );
		else
			snprintf(buf, sizeof(buf), "%c", *xc);
		s += buf;
		xc++;
	}
	if( ! s.empty() )
		printf("%s\n", s.c_str());
}

static bool get_token(TCC_CONTEXT *tc, const char **line, CToken *token, bool for_include)
{
	bool retval;
	CException *ex = new CException;

	retval = ::ReadToken(tc, line, token, ex, for_include);
	if( ex->GetError() != NULL )
		throw ex;
	delete ex;
	return retval;
}


bool CMaExpander::ReadToken(CToken *token)
{
	bool retval;

	retval = ::ReadToken(tc, &pos, token, &gex, for_include);
	if( gex.GetError() != NULL )
		throw &gex;
	return retval;
}


void CMaExpander::IgnoreSpaces()
{
	skip_blanks(pos);
}

bool CMaExpander::IsValidToken(const CC_STRING& s)
{
	const char *p = s.c_str();
	if(p == NULL)
		return false;
	if( ! isalpha(*p) && *p != '_' )
		return false;
	for(p++; *p != '\0'; p++ ) {
		if( isblank(*p) )
			break;
		if( ! is_id_char(*p) )
			return false;
	}
	return true;
}

void CMaExpander::Trim(CC_STRING& s)
{
	const char *p1, *p2, *end = s.c_str() + s.size();

	for(p1 = s.c_str() ; p1 != '\0' && isblank(*p1); p1++ ) ;
	for(p2 = end - 1 ; p2 >= s.c_str() && isblank(*p2); p2-- ) ;
	p2++;
	if(p2 < end && isblank(*p2) )
		p2++;
	CC_STRING ns;
	while(p1 < p2)
		ns += *p1++;
	s = ns;
}

CC_STRING CMaExpander::Expand_FLM(CMacro *ma)
{
	CC_STRING outs;
	CC_ARRAY<CC_STRING> margs;
	CC_STRING carg;
	const char *p = pos;
	int level;

	skip_blanks(p);
	if( iseol(*p) ) {
		gex.format("Macro \"%s\" expects arguments", TR(tc,ma->id));
		throw &gex;
	}
	if( *p != '(' ) {
		gex = "Macro expects '('";
		throw &gex;
	}

	p++;
	skip_blanks(p);
	level = 1;
	while( 1 ) {
		if( iseol(*p) )
			break;

		if( *p == ','  ) {
			if( level == 1 && ( ! ma->va_args || (margs.size() + 1 < ma->nr_args) ) ) {
				Trim(carg);
				margs.push_back(carg);
				carg.clear();
			} else {
				carg += ',';
				skip_blanks(p);
			}
		} else if(*p == '(') {
			level ++;
			carg += '(';
		} else if(*p == ')') {
			level --;
			if(level == 0) {
				p++;
				Trim(carg);
				margs.push_back(carg);
				carg.clear();
				break;
			} else
				carg += ')';
		} else {
			carg += *p;
		}
		p++;
	}
	pos = p;

	XCHAR *xc;
	CC_STRING s;
	size_t n;

	n = ma->nr_args;
	assert(n != CMacro::OL_M);
	if(n != margs.size()) {
		if( ma->va_args && margs.size() + 1 == n ) {
			margs.push_back(CC_STRING(""));
		} else {
			gex.format("Macro \"%s\" requires %u arguments, but %u given",
				TR(tc,ma->id), n, margs.size() );
			throw &gex;
		}
	}

	for(xc = ma->parsed; *xc != 0; xc++) {
		if(IS_MA_PAR2(*xc) || IS_MA_PAR0(*xc)) {
			const CC_STRING& carg = margs[(uint8_t)*xc];
			CMaExpander expander2(tc, carg.c_str(), for_include);
			CC_STRING tmp;
			tmp = expander2.TryExpand();
			s += tmp;
		} else if(IS_MA_PAR1(*xc)) {
			const CC_STRING& carg = margs[(uint8_t)*xc];
			s += '"';
			s += carg;
			s += '"';
		} else {
			s += (char) *xc;
		}
	}
	outs += s;

	return outs;
}


CC_STRING CMaExpander::Expand_OLM(CMacro *ma)
{
	XCHAR *xc;
	CC_STRING outs;

	for(xc = ma->parsed; *xc != 0; xc++)
		outs += (char) (*xc);
	return outs;
}


CC_STRING CMaExpander::TryExpand()
{
	CToken token;
	CC_STRING outs;
	static int debug_level;
	CC_STRING saved_inStr = inStr;

	debug_level++;


	while(1) {
		const char *const last_pos  = pos;

		IgnoreSpaces();
		if( ! get_token(tc, &pos, &token, for_include) )
			break;
		if( token.attr == CToken::TA_IDENT ) {
			CMacro *ma;
			CC_STRING tmp;
			ma = GetMacro(token.id);
			if( IS_MACRO(ma) && ! in_defined_context() ) {
				if( IS_FLM(ma) ) {
					skip_blanks(pos);
					if( *pos == '(' ) {
						tmp = Expand_FLM(ma);
					}
					else
						goto do_cat;
				} else {
					tmp = Expand_OLM(ma);
				}

				const ssize_t offset = last_pos - inStr.c_str();
				CC_STRING newStr;
				
				newStr.strcat(inStr.c_str(), last_pos);
				newStr += tmp;
				newStr += pos;
				
				inStr = newStr;
				pos   = inStr.c_str() + offset;
			} else
				goto do_cat;
		} else {
		do_cat:
			outs.strcat(last_pos, pos);
		}
		last_ids[0] = last_ids[1];
		last_ids[1] = token.id;
	}

//	printf("Leave [%u] %s\n", debug_level, outs.c_str());
	--debug_level;
//	fprintf(stderr, "*** %u: %s => %s\n", debug_level, saved_inStr.c_str(), outs.c_str());

	return outs;
}


CMaExpander::CMaExpander(TCC_CONTEXT *tc, const char *line, bool for_include)
{
	this->tc          = tc;
	this->inStr       = line;
	this->pos         = inStr.c_str();
	this->for_include = for_include;
	last_ids[0] = SSID_INVALID;
	last_ids[1] = SSID_INVALID;
}

CC_STRING ExpandLine(TCC_CONTEXT *tc, bool for_include, const char *line, CException *gex)
{
	CMaExpander maExpander(tc, line, for_include );
	CC_STRING expansion; 

	try {
		expansion = maExpander.TryExpand();
	} catch(CException *e) {
		*gex = *e;
	}
	return expansion;
}

/****** Part II: MACRO RECOGNITION ********/
static CException gEx;

static int find(sym_t id, const CC_ARRAY<sym_t>& list)
{
	size_t i;
	for(i = 0; i < list.size(); i++)
		if( list[i] == id )
			return (int) i;
	return -1;
}

static void __NO_USE__ dump(const char *a, const char *b)
{
	while(a < b && *a != 0) {
		putchar(*a);
		a++;
	}
	putchar('\n');
}


static void handle_define(TCC_CONTEXT *tc, sym_t mid, CMacro *ma)
{
	enum {
		STA_OKAY,
		STA_LEFT_PARENTHESIS,
		STA_PARAMETER,
		STA_SEPERATOR,
		STA_TRIDOT,
	} state;

	const char *line = ma->line;
	CToken token;

	if( ! get_token(tc, &line, &token, false) )
		return;

	if(token.attr == CToken::TA_IDENT) {
		if(*line == '(') {
			CC_ARRAY<sym_t>  para_list;
			XCHAR *xc;
			const char *last_pos;
			sym_t last_para = SSID_INVALID;
			bool has_va_args = false;
		
			state = STA_LEFT_PARENTHESIS;
			line++;
			while( get_token(tc, &line, &token, false) ) {
				switch(state) {
				case STA_LEFT_PARENTHESIS:	
					if( token.id == SSID_RIGHT_PARENTHESIS) {
						state = STA_OKAY;
						goto okay;
					} else if(token.attr == CToken::TA_IDENT) {
						para_list.push_back(token.id);
						state = STA_SEPERATOR;
					} else if(token.id == SSID_TRIDOT) {
						state = STA_TRIDOT;
						continue;
					} else {
						gEx.format("\"%s\" may not appear in macro parameter list", TR(tc,token.id));
						goto error;
					}
					break;
		
				case STA_SEPERATOR:
					if(token.id == SSID_RIGHT_PARENTHESIS) {
						state = STA_OKAY;
						goto okay;
					} else if(token.id == SSID_COMMA)
						state = STA_PARAMETER;
					else if(token.id == SSID_TRIDOT) {
						state = STA_TRIDOT;
					} else {
						gEx = "macro parameters must be comma-separated";
						goto error;
					}
					break;
		
				case STA_PARAMETER:
					if(token.id == SSID_TRIDOT) {
						state = STA_TRIDOT;
						continue;
					} else if(token.attr == CToken::TA_IDENT) {
						para_list.push_back(token.id);
						state = STA_SEPERATOR;
					} else {
						gEx = "parameter name missing";
						goto error;
					}
					break;

				case STA_TRIDOT:
					if(token.id != SSID_RIGHT_PARENTHESIS) {
						gEx = "missing ')' in macro parameter list";
						throw &gEx;
					}
					has_va_args = true;
					if(last_para == SSID_COMMA || last_para == SSID_LEFT_PARENTHESIS) {
						para_list.push_back(SSID_VA_ARGS);
						last_para = SSID_VA_ARGS;
					}
					goto okay;

				default:
					assert(0);
				}
				last_para = token.id;
			}

			if(state != STA_OKAY) {
				gEx = "missing ')' in macro parameter list";
				throw &gEx;
			}
		
		okay:
			xc = (XCHAR*) malloc(sizeof(XCHAR) * strlen(line) + 4);
			ma->id         = mid;
			ma->parsed     = xc;
			ma->nr_args    = para_list.size();
			ma->va_args    = has_va_args;

			skip_blanks(line);
			last_pos = line;
			sym_t prev_sid = SSID_INVALID;

			while(  ReadToken(tc, &line, &token, &gEx, false) ) {
				if(gEx.GetError() != NULL)
					throw (&gEx);
				
				if(token.id == SSID_DUAL_SHARP) {
					if(xc == ma->parsed) {
						gEx = "'##' cannot appear at either end of a macro expansion";
						goto error;
					} else if( IS_MA_PAR0(*(xc-1)) )
						*(xc-1) |= XF_MA_PAR2;
				}
				if(prev_sid == SSID_DUAL_SHARP) {
					XCHAR *yc = xc - 3;
					while( yc >= ma->parsed && (*yc == '\t' || *yc == ' ') )
						yc--;
					xc = yc + 1;
					assert(*xc == '#' || *xc == ' ' || *xc == '\t');
				}

				if( token.attr == CToken::TA_IDENT  ) {
					int magic = 0;
					int16_t para_ord;
					static const XCHAR xflags[3] = { XF_MA_PAR0, XF_MA_PAR1, XF_MA_PAR2 };

					if( has_va_args && (token.id == last_para || token.id == SSID_VA_ARGS) )
						para_ord = ma->nr_args - 1;
					else
						para_ord = find(token.id, para_list);
					if(prev_sid == SSID_SHARP) {
						if(para_ord < 0) {
							gEx = "'#' is not followed by a macro parameter";
							goto error;
						}
						magic = 1;
						xc--;
					} else if(prev_sid == SSID_DUAL_SHARP) {
						magic = 2;
					}
					if(para_ord < 0)
						goto do_cat;
					*xc++ = para_ord | xflags[magic];
				} else {
				do_cat:
					if(prev_sid == SSID_DUAL_SHARP) {
						skip_blanks(last_pos);
					}
					join(&xc, last_pos, line);
				}
				last_pos = line;
				prev_sid = token.id;
			} /*end while*/
			*xc = 0;
		} else {
			XCHAR *xc;

			xc = (XCHAR*) malloc(sizeof(XCHAR) * strlen(line) + 4);
			ma->id      = mid;
			ma->parsed  = xc;
			ma->nr_args = CMacro::OL_M;

			skip_blanks(line);
			while(1) {
				char c = *line;
				if( iseol(c) ) {
					*xc = '\0';
					break;
				}
				*xc = c;
				xc++, line++;
			}
		}
	}
//	dump(ma->parsed);
	return;

error:
	throw &gEx;
}


void handle_undef(TCC_CONTEXT *tc, const char *line)
{
	CToken token;

	ReadToken(tc, &line, &token, &gEx, false);
	if( ! gv_preprocess_mode )
		tc->maMap.Put(token.id, CMacro::NotDef);
	else {
		CMacro *ma = tc->maMap.Lookup(token.id);
		if( IS_MACRO(ma) )
			free(ma->parsed);
		tc->maMap.Remove(token.id);
	}
}

CMacro *const CMacro::NotDef = (CMacro *const) 1;
