#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <ctype.h>
#include <unistd.h>

#include <sys/types.h>
#include <assert.h>

#include "base.h"
#include "misc.h"
#include "InternalTables.h"
#include "cc_string.h"
#include "cc_array.h"
#include "GlobalVars.h"

class Exception {

public:
	CC_STRING  msg;
    inline Exception(void) {
	}

    inline Exception(const char *msg) {
		this->msg = msg;
	}

    inline Exception(const CC_STRING& msg) {
		this->msg = msg;
	}

	inline Exception& operator = (const char *msg) {
		this->msg = msg;
		return *this;
	}
	inline Exception& operator = (const CC_STRING& msg) {
		this->msg = msg;
		return *this;
	}
};

static Exception excep;

class ExpandMacro {
public:
	ExpandMacro(InternalTables *intab, const char *line, bool for_include);
	CC_STRING   TryExpand();

private:
	ReadReq req;
	InternalTables *intab;
	CC_STRING     inStr;
	bool for_include;
	sym_t last_ids[2];

	CC_STRING Expand_FLM(SynMacro *ma);
	CC_STRING Expand_OLM(SynMacro *ma);
	SynMacro* GetMacro(sym_t mid);
	inline bool in_defined_context();

	void IgnoreSpaces();
	bool IsValidToken(const CC_STRING& s);
	void Trim(CC_STRING &s);
	void process_define(sym_t mid, SynMacro *ma);
	bool ReadToken(ReadReq *req, bool for_include);
};

inline bool ExpandMacro::in_defined_context()
{
	return (last_ids[1] == SSID_DEFINED ||
		(last_ids[0] == SSID_DEFINED && last_ids[1] == SSID_LEFT_PARENTHESIS));
}

#define IS_FLM(ma)        ((ma)->nr_args != SynMacro::OL_M)
#define IS_MACRO(ma)      ((ma) != NULL && (ma) != SynMacro::NotDef)


/* Find the specific macro in the table. Expand it if it is not expanded.
 *
 */
SynMacro * ExpandMacro::GetMacro(sym_t mid)
{
	SynMacro *ma = intab->maLut.Lookup(mid);
	if( IS_MACRO(ma) && ma->id == SSID_INVALID ) {
		process_define(mid, ma);
		free(ma->line);
		ma->line = NULL;
		return intab->maLut.Lookup(mid);
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

bool ExpandMacro::ReadToken(ReadReq *req, bool for_include)
{
	int retval;

	retval = ::ReadToken(intab, req, for_include);
	if( retval < 0 ) {
		excep = req->error;
		throw &excep;
	}
	return retval;
}

void ExpandMacro::IgnoreSpaces()
{
	skip_blanks(req.cp);
}

bool ExpandMacro::IsValidToken(const CC_STRING& s)
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

void ExpandMacro::Trim(CC_STRING& s)
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

CC_STRING ExpandMacro::Expand_FLM(SynMacro *ma)
{
	CC_STRING outs;
	CC_ARRAY<CC_STRING> margs;
	CC_STRING carg;
	const char *p = req.cp;
	int level;

	skip_blanks(p);
	if( iseol(*p) ) {
		CC_STRING a;
		a.Format("Macro \"%s\" expects arguments", TR(intab,ma->id));
		excep = a;
		throw &excep;
	}
	if( *p != '(' ) {
		excep = "Macro expects '('";
		throw &excep;
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
	req.cp = p;

	XCHAR *xc;
	CC_STRING s;
	size_t n;

	n = ma->nr_args;
	assert(n != SynMacro::OL_M);
	if(n != margs.size()) {
		if( ma->va_args && margs.size() + 1 == n ) {
			margs.push_back(CC_STRING(""));
		} else {
			CC_STRING a;
			a.Format("Macro \"%s\" requires %u arguments, but %u given",
				TR(intab,ma->id), n, margs.size() );
			excep = a;
			throw &excep;
		}
	}

	for(xc = ma->parsed; *xc != 0; xc++) {
		if(IS_MA_PAR2(*xc) || IS_MA_PAR0(*xc)) {
			const CC_STRING& carg = margs[(uint8_t)*xc];
			ExpandMacro expander2(intab, carg.c_str(), for_include);
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


CC_STRING ExpandMacro::Expand_OLM(SynMacro *ma)
{
	XCHAR *xc;
	CC_STRING outs;

	for(xc = ma->parsed; *xc != 0; xc++)
		outs += (char) (*xc);
	return outs;
}


CC_STRING ExpandMacro::TryExpand()
{
	CC_STRING outs;
	static int debug_level;
	CC_STRING saved_inStr = inStr;

	debug_level++;


	while(1) {
		const char *const last_pos  = req.cp;

		IgnoreSpaces();
		if( ! ReadToken(&req, for_include) )
			break;
		if( req.token.attr == SynToken::TA_IDENT ) {
			SynMacro *ma;
			CC_STRING tmp;
			ma = GetMacro(req.token.id);
			if( IS_MACRO(ma) && ! in_defined_context() ) {
				if( IS_FLM(ma) ) {
					skip_blanks(req.cp);
					if( *req.cp == '(' ) {
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
				newStr += req.cp;

				inStr = newStr;
				req.cp   = inStr.c_str() + offset;
			} else
				goto do_cat;
		} else {
		do_cat:
			outs.strcat(last_pos, req.cp);
		}
		last_ids[0] = last_ids[1];
		last_ids[1] = req.token.id;
	}

//	printf("Leave [%u] %s\n", debug_level, outs.c_str());
	--debug_level;
//	fprintf(stderr, "*** %u: %s => %s\n", debug_level, saved_inStr.c_str(), outs.c_str());

	return outs;
}


ExpandMacro::ExpandMacro(InternalTables *intab, const char *line, bool for_include)
{
	this->intab      = intab;
	this->inStr       = line;
	this->req.line    =
	this->req.cp      = inStr.c_str();
	this->for_include = for_include;
	last_ids[0] = SSID_INVALID;
	last_ids[1] = SSID_INVALID;
}

CC_STRING ExpandLine(InternalTables *intab, bool for_include, const char *line, CC_STRING& errmsg)
{
	ExpandMacro maExpander(intab, line, for_include );
	CC_STRING expansion;

	try {
		expansion = maExpander.TryExpand();
	} catch(Exception *ex) {
		errmsg = ex->msg;
	}
	return expansion;
}

/****** Part II: MACRO RECOGNITION ********/

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


void ExpandMacro::process_define(sym_t mid, SynMacro *ma)
{
	enum {
		STA_OKAY,
		STA_LEFT_PARENTHESIS,
		STA_PARAMETER,
		STA_SEPERATOR,
		STA_TRIDOT,
	} state;

	ReadReq req(ma->line);
	SynToken& token = req.token;

	if( ! ReadToken(&req, false) )
		return;

	if(token.attr == SynToken::TA_IDENT) {
		if(*req.cp == '(') {
			CC_ARRAY<sym_t>  para_list;
			XCHAR *xc;
			const char *last_pos;
			sym_t last_para = SSID_INVALID;
			bool has_va_args = false;

			state = STA_LEFT_PARENTHESIS;
			req.cp++;
			while( ReadToken(&req, false) ) {
				switch(state) {
				case STA_LEFT_PARENTHESIS:
					if( token.id == SSID_RIGHT_PARENTHESIS) {
						state = STA_OKAY;
						goto okay;
					} else if(token.attr == SynToken::TA_IDENT) {
						para_list.push_back(token.id);
						state = STA_SEPERATOR;
					} else if(token.id == SSID_TRIDOT) {
						state = STA_TRIDOT;
						continue;
					} else {
						CC_STRING a;
						a.Format("\"%s\" may not appear in macro parameter list", TR(intab,token.id));
						excep = a;
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
						excep = "macro parameters must be comma-separated";
						goto error;
					}
					break;

				case STA_PARAMETER:
					if(token.id == SSID_TRIDOT) {
						state = STA_TRIDOT;
						continue;
					} else if(token.attr == SynToken::TA_IDENT) {
						para_list.push_back(token.id);
						state = STA_SEPERATOR;
					} else {
						excep = "parameter name missing";
						goto error;
					}
					break;

				case STA_TRIDOT:
					if(token.id != SSID_RIGHT_PARENTHESIS) {
						excep = "missing ')' in macro parameter list";
						goto error;
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
				excep = "missing ')' in macro parameter list";
				goto error;
			}

		okay:
			xc = (XCHAR*) malloc(sizeof(XCHAR) * strlen(req.cp) + 4);
			ma->id         = mid;
			ma->parsed     = xc;
			ma->nr_args    = para_list.size();
			ma->va_args    = has_va_args;

			skip_blanks(req.cp);
			last_pos = req.cp;
			sym_t prev_sid = SSID_INVALID;

			while(ReadToken(&req, false)) {
				if(token.id == SSID_DUAL_SHARP) {
					if(xc == ma->parsed) {
						excep = "'##' cannot appear at either end of a macro expansion";
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

				if( token.attr == SynToken::TA_IDENT  ) {
					int magic = 0;
					int16_t para_ord;
					static const XCHAR xflags[3] = { XF_MA_PAR0, XF_MA_PAR1, XF_MA_PAR2 };

					if( has_va_args && (token.id == last_para || token.id == SSID_VA_ARGS) )
						para_ord = ma->nr_args - 1;
					else
						para_ord = find(token.id, para_list);
					if(prev_sid == SSID_SHARP) {
						if(para_ord < 0) {
							excep = "'#' is not followed by a macro parameter";
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
					join(&xc, last_pos, req.cp);
				}
				last_pos = req.cp;
				prev_sid = token.id;
			} /*end while*/
			*xc = 0;
		} else {
			XCHAR *xc;

			xc = (XCHAR*) malloc(sizeof(XCHAR) * strlen(req.cp) + 4);
			ma->id      = mid;
			ma->parsed  = xc;
			ma->nr_args = SynMacro::OL_M;

			skip_blanks(req.cp);
			while(1) {
				char c = *req.cp;
				if( iseol(c) ) {
					*xc = '\0';
					break;
				}
				*xc = c;
				xc++, req.cp++;
			}
		}
	}
//	dump(ma->parsed);
	return;

error:
	throw &excep;
}


void handle_undef(InternalTables *intab, const char *line)
{
	ReadReq req(line);

	ReadToken(intab, &req, false);
	if( ! gv_preprocess_mode )
		intab->maLut.Put(req.token.id, SynMacro::NotDef);
	else {
		SynMacro *ma = intab->maLut.Lookup(req.token.id);
		if( IS_MACRO(ma) )
			free(ma->parsed);
		intab->maLut.Remove(req.token.id);
	}
}

SynMacro *const SynMacro::NotDef = (SynMacro *const) 1;
