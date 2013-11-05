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


BASIC_CONSOLE& BASIC_CONSOLE::operator << (const char *m)
{
	if( ! rtm_silent )
		fprintf(stream, "%s", m);
	return *this;
}

BASIC_CONSOLE& BASIC_CONSOLE::operator << (const char m)
{
	if( ! rtm_silent )
		fprintf(stream, "%c", m);
	return *this;
}

BASIC_CONSOLE& BASIC_CONSOLE::operator << (const ssize_t m)
{
	if( ! rtm_silent )
		fprintf(stream, "%d", m);
	return *this;
}

BASIC_CONSOLE& BASIC_CONSOLE::operator << (const size_t m)
{
	if( ! rtm_silent )
		fprintf(stream, "%u", m);
	return *this;
}

BASIC_CONSOLE& BASIC_CONSOLE::operator << (const CC_STRING& m)
{
	if( ! rtm_silent )
		fprintf(stream, "%s", m.c_str());
	return *this;
}

/*********************************************************************/
const MESSAGE_LEVEL TCC_DEBUG_CONSOLE::default_gate_level = (MESSAGE_LEVEL) DML_RUNTIME;

#define CAN_PRINT()   ((!rtm_silent) && can_print())

TCC_DEBUG_CONSOLE& TCC_DEBUG_CONSOLE::operator << (const char *m)
{
	if(CAN_PRINT())
		fprintf(stream, "%s", m);
	return *this;
}

TCC_DEBUG_CONSOLE& TCC_DEBUG_CONSOLE::operator << (const char m)
{
	if(CAN_PRINT())
		fprintf(stream, "%c", m);
	return *this;
}

TCC_DEBUG_CONSOLE& TCC_DEBUG_CONSOLE::operator << (const ssize_t m)
{
	if(CAN_PRINT())
		fprintf(stream, "%d", m);
	return *this;
}

TCC_DEBUG_CONSOLE& TCC_DEBUG_CONSOLE::operator << (const size_t m)
{
	if(CAN_PRINT())
		fprintf(stream, "%u", m);
	return *this;
}

TCC_DEBUG_CONSOLE& TCC_DEBUG_CONSOLE::operator << (const CC_STRING& m)
{
	if(CAN_PRINT())
		fprintf(stream, "%s", m.c_str());
	return *this;
}

TCC_DEBUG_CONSOLE TCC_DEBUG_CONSOLE::operator << (MESSAGE_LEVEL level)
{
	current_level = level;
	return *this;
}


TCC_DEBUG_CONSOLE& TCC_DEBUG_CONSOLE::operator << (const TOKEN_ARRAY& tokens)
{
	if( !CAN_PRINT() )
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
	if( !CAN_PRINT() )
		return *this;
	size_t i;
	for(i = 0; i < tokens.size(); i++) 
		fprintf(stream, "  %s", id_to_symbol(tc->symtab,tokens[i]));
	return *this;
}

TCC_DEBUG_CONSOLE& TCC_DEBUG_CONSOLE::operator << (const TOKEN *token)
{
	if( !CAN_PRINT() )
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
	if( !CAN_PRINT() )
		return *this;
	sym_t id;
	MACRO_INFO *minfo;

	while( (id = macro_table_list((CC_HANDLE)macro_table, &minfo)) != SSID_INVALID ) {
		size_t j;

		if( minfo->line != NULL ) {
			fprintf(stream, "#define %s", minfo->line);
			continue;
		}

		if(minfo == TCC_MACRO_UNDEF) {
			fprintf(stream, "#undf  %s  ", id_to_symbol(tc->symtab, id));
			continue;
		} else 
			fprintf(stream, "#define %s  ", id_to_symbol(tc->symtab, id));

		fprintf(stream, "\n");
	}
	fprintf(stream, "\n");
	return *this;
}

TCC_DEBUG_CONSOLE& TCC_DEBUG_CONSOLE::precedence_matrix_dump(void)
{
	if( !CAN_PRINT() )
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
		fprintf(stream, "\n");
	}
	return *this;
}


