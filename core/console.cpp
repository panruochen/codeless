#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/types.h>
#include <stdarg.h>

#include "tcc.h"
#include "precedence-matrix.h"
#include "c++-if.h"

const char BASIC_CONSOLE::endl = 10;

BASIC_CONSOLE& BASIC_CONSOLE::operator << (const char *m)
{
	fprintf(stream, "%s", m);
	return *this;
}

BASIC_CONSOLE& BASIC_CONSOLE::operator << (const char m)
{
	fprintf(stream, "%c", m);
	return *this;
}

BASIC_CONSOLE& BASIC_CONSOLE::operator << (const ssize_t m)
{
	fprintf(stream, "%d", m);
	return *this;
}

BASIC_CONSOLE& BASIC_CONSOLE::operator << (const size_t m)
{
	fprintf(stream, "%u", m);
	return *this;
}

BASIC_CONSOLE& BASIC_CONSOLE::operator << (const CC_STRING& m)
{
	fprintf(stream, "%s", m.c_str());
	return *this;
}

/*********************************************************************/
const MESSAGE_LEVEL TCC_DEBUG_CONSOLE::default_gate_level = (MESSAGE_LEVEL) DML_RUNTIME;

TCC_DEBUG_CONSOLE& TCC_DEBUG_CONSOLE::operator << (const char *m)
{
	if(can_print())
		fprintf(stream, "%s", m);
	return *this;
}

TCC_DEBUG_CONSOLE& TCC_DEBUG_CONSOLE::operator << (const char m)
{
	if(can_print())
		fprintf(stream, "%c", m);
	return *this;
}

TCC_DEBUG_CONSOLE& TCC_DEBUG_CONSOLE::operator << (const ssize_t m)
{
	if(can_print())
		fprintf(stream, "%d", m);
	return *this;
}

TCC_DEBUG_CONSOLE& TCC_DEBUG_CONSOLE::operator << (const size_t m)
{
	if(can_print())
		fprintf(stream, "%u", m);
	return *this;
}

TCC_DEBUG_CONSOLE& TCC_DEBUG_CONSOLE::operator << (const CC_STRING& m)
{
	if(can_print())
		fprintf(stream, "%s", m.c_str());
	return *this;
}

TCC_DEBUG_CONSOLE TCC_DEBUG_CONSOLE::operator << (MESSAGE_LEVEL level)
{
	current_level = level;
	return *this;
}

TCC_DEBUG_CONSOLE& TCC_DEBUG_CONSOLE::operator << (TOKEN_SEQ *tseq)
{
	if( !can_print() )
		return *this;
	size_t i;
	for(i = 0; i < tseq->nr; i++)
		fprintf(stream, " %s", id_to_symbol(tc->symtab,tseq->buf[i].id));
	return *this;
}

TCC_DEBUG_CONSOLE& TCC_DEBUG_CONSOLE::operator << (const TOKEN_ARRAY& tokens)
{
	if( !can_print() )
		return *this;
	size_t i;
	for(i = 0; i < tokens.size(); i++) {
		if(tokens[i].attr == TA_OPERATOR || tokens[i].attr == TA_IDENTIFIER)
			fprintf(stream, " %s", id_to_symbol(tc->symtab,tokens[i].id));
		else
			fprintf(stream, " %d", tokens[i].i32_val);
	}
	return *this;
}

TCC_DEBUG_CONSOLE& TCC_DEBUG_CONSOLE::operator << (const CC_ARRAY<sym_t>& tokens)
{
	if( !can_print() )
		return *this;
	size_t i;
	for(i = 0; i < tokens.size(); i++) 
		fprintf(stream, "  %s", id_to_symbol(tc->symtab,tokens[i]));
	return *this;
}

TCC_DEBUG_CONSOLE& TCC_DEBUG_CONSOLE::operator << (const TOKEN *token)
{
	if( !can_print() )
		return *this;
	switch(token->attr) {
	case TA_UINT:
		fprintf(stream, "%u", token->u32_val);
		break;
	case TA_INT:
		fprintf(stream, "%d", token->u32_val);
		break;
	case TA_OPERATOR:
	case TA_IDENTIFIER:
		fprintf(stream, "%s", id_to_symbol(tc->symtab, token->id));
		break;
	}
	return *this;
}

TCC_DEBUG_CONSOLE& TCC_DEBUG_CONSOLE::operator << (const TCC_MACRO_TABLE macro_table)
{
	if( !can_print() )
		return *this;
	sym_t id;
	MACRO_INFO *minfo;

	while( (id = macro_table_list((CC_HANDLE)macro_table, &minfo)) != SSID_INVALID ) {
		size_t j;

		if( ! minfo->handled ) {
			fprintf(stream, "#define %s", minfo->line);
			continue;
		}

		if(minfo == TCC_MACRO_UNDEF) {
			fprintf(stream, "#undf  %s  ", id_to_symbol(tc->symtab, id));
			continue;
		} else 
			fprintf(stream, "#define %s  ", id_to_symbol(tc->symtab, id));

		if(minfo->params != NULL) {
			fprintf(stream, "(");
			for(j = 0; j + 1 < minfo->params->nr; j++)  {
				fprintf(stream, "%s,", id_to_symbol(tc->symtab, minfo->params->buf[j]));
			}
			if( j < minfo->params->nr )
				fprintf(stream, "%s", id_to_symbol(tc->symtab, minfo->params->buf[j]));
			fprintf(stream, ")  ");
		}
		if(minfo->def != NULL) {
			for(j = 0; j < minfo->def->nr; j++) 
				operator << (minfo->def->buf + j);
		}
		fprintf(stream, "%c", endl);
	}
	fprintf(stream, "%c", endl);
	return *this;
}

TCC_DEBUG_CONSOLE& TCC_DEBUG_CONSOLE::precedence_matrix_dump(void)
{
	if( !can_print() )
		return *this;
	size_t i, j;

	fprintf(stream, "   ");
	for(i = 0; i < COUNT_OF(g1_oprset); i++) 
		fprintf(stream, "%2s ", g1_oprset[i]);
	fprintf(stream, "\n\n");
	for(i = 0; i < COUNT_OF(g1_oprset); i++) {
		fprintf(stream, "%2s ", g1_oprset[i]);
		for(j = 0; j < COUNT_OF(g1_oprset); j++) {
			int prio = oprmx_compare(tc->matrix, 
					symbol_to_id(tc->symtab, g1_oprset[i]), symbol_to_id(tc->symtab, g1_oprset[j]));
			const char *sym[] = { "=", "<", ">", "X" };
			const char *result = sym[prio];
			fprintf(stream, "%2s ", result);
		}
		fprintf(stream, "%c", endl);
	}
	return *this;
}


