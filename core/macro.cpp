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
static jmp_buf long_jmp_context;
#define EXCEPTION_MAGIC_NUMBER  128
#define THROW_EXCEPTION()  longjmp(long_jmp_context, EXCEPTION_MAGIC_NUMBER)

typedef CC_ARRAY<TOKEN_ARRAY *>   MACRO_ARGS;

/* The input token stream structure for macro expansion */
class TOKEN_STREAM {
protected:
	TOKEN_ARRAY&  input;
	size_t        pos;

public:
	TOKEN_STREAM(TOKEN_ARRAY& tokens) : input(tokens) { pos = 0; }
	inline TOKEN & current(void) { return input[pos]; }
	inline bool lookahead(ssize_t offset, sym_t id) {
		const size_t i = pos + offset; return i < input.size() ? input[i].id == id : false; 
	}
	inline bool have(size_t n) { return pos + n <= input.size(); }
	inline void move(size_t n) { pos += n; }
	inline bool eol() { return pos >= input.size(); }
};


/******  Part I: MACRO EXPANSION ********/

/* Find the specific macro in the table. Expand it if it is not expanded.
 *
 */
static MACRO_INFO * __find(TCC_CONTEXT *tc, sym_t mid)
{
	MACRO_INFO *mi = macro_table_lookup(tc->mtab, mid);
	if(mi != NULL && mi != TCC_MACRO_UNDEF && ! mi->handled) {
		ConditionalParser *p = (ConditionalParser *) mi->priv;
		TOKEN_ARRAY tokens;
		p->split(SSID_SHARP_DEFINE, mi->line, tokens);
		free(mi->line);
		handle_define(tc, tokens);
		return macro_table_lookup(tc->mtab, mid);
	}
	return mi;
}


static void expand_object_like_macro(TCC_CONTEXT *tc, TOKEN_STREAM& tstr, TOKEN_ARRAY& result)
{
	MACRO_INFO *minfo;

	const TOKEN ctok = tstr.current();
	minfo = __find(tc, ctok.id);
	if(minfo == TCC_MACRO_UNDEF) {
		debug_console << DML_ERROR << "Undefined macro " << id_to_symbol(tc->symtab, ctok.id) << TCC_DEBUG_CONSOLE::endl ;
//		THROW_EXCEPTION();
		result.push_back(g_unevaluable_token);
	} else if(minfo == NULL) {
		if(!preprocess_mode)
			result.push_back(g_unevaluable_token);
		else {
			TOKEN false_token = { SSID_SYMBOL_X, TA_UINT, 0 } ;
			result.push_back(false_token);
		}
	} else {
		if( minfo->params != NULL && minfo->params->nr > 0 ) {
			debug_console << DML_ERROR << id_to_symbol(tc->symtab, ctok.id) << " should take " << 
				minfo->params->nr << " arguments!" << TCC_DEBUG_CONSOLE::endl;
			THROW_EXCEPTION();
		}
		if(minfo->def != NULL && minfo->def->nr != 0)
			result.insert(result.size(), minfo->def->buf, minfo->def->nr);
	}
	tstr.move(1);
}

static bool can_be_expanded(const TOKEN_ARRAY& tokens)
{
	size_t i;
	for(i = 0; i < tokens.size(); i++) {
		const TOKEN& ctok = tokens[i];
		if( ctok.attr == TA_IDENTIFIER && ctok.id != SSID_SYMBOL_X )
			return true;
	}
	return false;
}

static void expand_defined(TCC_CONTEXT *tc, TOKEN_STREAM& tstr, TOKEN_ARRAY& result)
{
	sym_t id = SSID_INVALID;
	size_t step;
	TOKEN *tbuf;

	tbuf = &(tstr.current());
	if( tstr.have(2) && tbuf[1].attr == TA_IDENTIFIER ) {
		id = tbuf[1].id;
		step = 2;
	} else if( tstr.have(3) && 
		tbuf[1].id == SSID_LEFT_PARENTHESIS && 
		tbuf[2].attr == TA_IDENTIFIER &&
		tbuf[3].id == SSID_RIGHT_PARENTHESIS) {
		id = tbuf[2].id;
		step = 4;
	}
	if(id == SSID_INVALID) {
		debug_console << DML_ERROR << "Throw expection at " << __func__ << ":" << (size_t)__LINE__ << TCC_DEBUG_CONSOLE::endl;
		THROW_EXCEPTION();
	}

	TOKEN token;
	check_symbol_defined(tc, id, false, &token);
	result.push_back(token);
	tstr.move(step);
}

static void __expand_function_like_macro(TCC_CONTEXT *tc, sym_t mid, MACRO_ARGS& margv, TOKEN_ARRAY& expansion)
{
	size_t i;
	MACRO_INFO *minfo;
	CC_MAP<sym_t,TOKEN_ARRAY*> map;
	size_t argc;

	minfo = __find(tc, mid);
//	if(minfo == TCC_MACRO_UNDEF) {
	if(minfo == TCC_MACRO_UNDEF || minfo == NULL) {
		expansion.push_back(g_unevaluable_token);
		return;
	}

	argc = (minfo->params ? minfo->params->nr : 0);
	if( margv.size() != argc ) {
		debug_console << DML_ERROR << argc << " arguments required, " << margv.size() << "passed!" << TCC_DEBUG_CONSOLE::endl;
		THROW_EXCEPTION();
	}
	for(i = 0; i < argc; i++)
		map.insert(minfo->params->buf[i], margv[i]);

	for(i = 0 ; i < minfo->def->nr; i++) {
		const TOKEN& token = minfo->def->buf[i] ;
		TOKEN_ARRAY *tmp;
		if( map.find(token.id, tmp) ) {
			expansion.insert(expansion.size(), *tmp);
		} else {
			expansion.push_back(token);
		}
	}
}

static void expand_any(TCC_CONTEXT *tc, TOKEN_STREAM& str, TOKEN_ARRAY& result);

static void cleanup(MACRO_ARGS& argv)
{
	size_t i;
	for(i = 0; i < argv.size(); i++) {
		delete argv[i];
	}
	argv.clear();
}


static void expand_function_like_macro(TCC_CONTEXT *tc, TOKEN_STREAM& tstr, TOKEN_ARRAY& result)
{
	sym_t mid;
	MACRO_ARGS margv;
	TOKEN_ARRAY *this_arg;
	int parenthesis_level = 0;

	mid = tstr.current().id;
	tstr.move(2);
	if( tstr.lookahead(0, SSID_RIGHT_PARENTHESIS) ) {
success:
		__expand_function_like_macro(tc, mid, margv, result);
		tstr.move(1);
		return;
	}

	this_arg = NULL;
	while( ! tstr.eol() ) {
		const TOKEN& ctok = tstr.current();

		switch( ctok.id ) {
		case SSID_COMMA:
			margv.push_back(this_arg);
			this_arg = NULL;
			break;
		case SSID_RIGHT_PARENTHESIS: 
			if( parenthesis_level == 0 ) {
				if( this_arg != NULL ) {
					margv.push_back(this_arg);
					this_arg = NULL;
				}
				goto success;
			} else {
				(*this_arg).push_back(ctok);
				--parenthesis_level;
			}
			break;
		default: 
			if(this_arg == NULL)
				this_arg = new TOKEN_ARRAY;
			if(ctok.attr == TA_IDENTIFIER ) {
				expand_any(tc, tstr, *this_arg);
				continue;
			} else {
				if(ctok.id == SSID_LEFT_PARENTHESIS)
					++parenthesis_level;
				(*this_arg).push_back(ctok);
			}
			break;
		}
		tstr.move(1);
	}
	cleanup(margv);
	debug_console << DML_ERROR << "Throw expection at " << __func__ << ":" << (size_t) __LINE__ << TCC_DEBUG_CONSOLE::endl;
	THROW_EXCEPTION();
	return;
}


static void run_macro_expansion(TCC_CONTEXT *tc, TOKEN_STREAM& tstr, TOKEN_ARRAY& result);
static void expand_any(TCC_CONTEXT *tc, TOKEN_STREAM& tstr, TOKEN_ARRAY& result)
{
	if( tstr.lookahead(0, SSID_DEFINED) )
		expand_defined(tc, tstr, result);
	else {
		MACRO_INFO *mi = __find(tc, tstr.current().id);
		if( mi && mi != TCC_MACRO_UNDEF && mi->params != NULL && tstr.lookahead(1, SSID_LEFT_PARENTHESIS) )
			expand_function_like_macro(tc, tstr, result);
		else
			expand_object_like_macro(tc, tstr, result);
	}

	if( can_be_expanded(result) ) {
		TOKEN_STREAM tstr2(result);
		TOKEN_ARRAY  result2;
		run_macro_expansion(tc, tstr2, result2);
		result.clear();
		result.insert(0, result2);
	}
}


static void run_macro_expansion(TCC_CONTEXT *tc, TOKEN_STREAM& tstr, TOKEN_ARRAY& result)
{
	while( ! tstr.eol() ) {
		if(tstr.current().attr == TA_IDENTIFIER) {
			TOKEN_ARRAY tmp;
			expand_any(tc, tstr, tmp);
			result.insert(result.size(), tmp);
		} else {
			result.push_back(tstr.current());
			tstr.move(1);
		}
	}
}

void do_macro_expansion(TCC_CONTEXT *tc, TOKEN_ARRAY& tokens)
{
	int retval;

	retval = setjmp(long_jmp_context);
	if(retval ==  0) {
		TOKEN_ARRAY  source;
		source.insert(0, tokens);
		tokens.clear();

		TOKEN_STREAM tstr(source);
		run_macro_expansion(tc, tstr, tokens);
	} else {
		tokens.erase(0, tokens.size());
		tokens.push_back(g_unevaluable_token);
	}
}

/****** Part II: MACRO RECOGNITION ********/
static void new_macro(TCC_CONTEXT *tc, sym_t macro, const TOKEN *tok_buf, size_t tok_num,
	const sym_t *params, size_t params_num)
{
	MACRO_INFO *minfo;
	char *ptr;
	size_t size = sizeof(MACRO_INFO);
	if(params_num != 0)
		size += sizeof(sym_t) * params_num + sizeof(PARA_INFO);
	if(tok_num != 0)
		size += tok_num * sizeof(TOKEN) + sizeof(TOKEN_SEQ);

	ptr = (char *) malloc(size);
	minfo = (MACRO_INFO *) ptr;
	ptr = (char *) &minfo[1];
	if(params_num != 0) {
		minfo->params = (PARA_INFO *) ptr;
		ptr = (char *) &minfo->params->buf[params_num];
		minfo->params->nr = params_num;
		memcpy(minfo->params->buf, params, sizeof(sym_t) * params_num);
	} else
		minfo->params = NULL;
	if(tok_num != 0) {
		minfo->def = (TOKEN_SEQ*) ptr;
		ptr = (char *) &minfo->def->buf[tok_num];
		minfo->def->nr = tok_num;
		memcpy(minfo->def->buf, tok_buf, minfo->def->nr * sizeof(TOKEN));
	} else
		minfo->def = NULL;
	minfo->handled = true;
	minfo->line = NULL;
	minfo->priv = NULL;
	macro_table_insert(tc->mtab, macro, minfo);

//	printf("%d, %d, %d vs %d\n", tok_num, params_num, size, ptr - (char *)minfo);
	assert(size == (size_t)(ptr - (char *)minfo));
}


static int get_function_like_macro(TCC_CONTEXT *tc, TOKEN_ARRAY& tokens)
{
	CC_ARRAY<sym_t>  para_list;
	size_t i;
	TOKEN *tbuf;
	enum ME_STATE {
		ME_STAT_INITIAL,
		ME_STAT_ARGUMENT,
		ME_STAT_SEPERATOR,
	} state;
	
	tokens[0].attr = TA_IDENTIFIER;
	tbuf = tokens.get_buffer() + 1;
	if(tbuf->id != SSID_LEFT_PARENTHESIS)
		return -1;
	if( tokens.size() >= 3 && tbuf[0].id == SSID_RIGHT_PARENTHESIS ) {
		i = 3;
		goto okay;
	}

	state = ME_STAT_ARGUMENT;
	for(i = 2; i < tokens.size(); i++) {
		tbuf = tokens.get_buffer() + i;
		if(state == ME_STAT_SEPERATOR ) {
			if(tbuf->id == SSID_COMMA)
				state = ME_STAT_ARGUMENT;
			else if(tbuf->id == SSID_RIGHT_PARENTHESIS) {
				state = ME_STAT_INITIAL;
				i ++;
				break;
			}
		} else if(state == ME_STAT_ARGUMENT) {
			if(tbuf->attr != TA_IDENTIFIER)
				goto error;
			para_list.push_back(tbuf->id);
			state = ME_STAT_SEPERATOR;
		}
	}
	
	if(state == ME_STAT_INITIAL) {
okay:
		sym_t *p = para_list.size() == 0 ? NULL : &para_list.front();
		new_macro(tc, tokens[0].id, 
			i + tokens.get_buffer(), tokens.size() - i, p, para_list.size() );
		return 0;
	}
error:
	return -1;
}

void handle_define(TCC_CONTEXT *tc, TOKEN_ARRAY& tokens)
{
	if(tokens.size() >= 1) {
		if(tokens[0].attr == TA_IDENTIFIER && tokens[0].flag_flm) {
			get_function_like_macro(tc, tokens);
		} else
			new_macro(tc, tokens[0].id, 
				1 + tokens.get_buffer(), tokens.size() - 1, NULL, 0);
	} else if(tokens.size() == 0)
		new_macro(tc, tokens[0].id, NULL, 0, NULL, 0);
}

void handle_undef(TCC_CONTEXT *tc, TOKEN& token)
{
	if( ! preprocess_mode )
		macro_table_insert(tc->mtab, token.id, TCC_MACRO_UNDEF);
	else
		macro_table_delete(tc->mtab, token.id);
}

