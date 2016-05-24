#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>

#define  __OPM_CONST_DATA
#include "InternalTables.h"
#include "Utilities.h"

static int precedence_matrix_setup(InternalTables *intab, size_t oprnum, const char **oprset, const char **matrix)
{
	size_t i;
	int rc = 0;

	if( intab->syLut.Create() != 0)
		goto fail;
	if( intab->opMat.Create() != 0 )
		goto fail;
	intab->syLut.Construct(oprnum, oprset);
	intab->opMat.Construct();
	for(i = 0; i < oprnum * oprnum; i++) {
		char opr1[16], opr2[16];
		int result, id1, id2;
		char prec;
		sscanf(matrix[i],"%s %s %c", opr1, opr2, &prec);
		switch(prec) {
		case '=': result = _EQ; break;
		case '<': result = _LT; break;
		case '>': result = _GT; break;
		case 'X': result = _XX; break;
		default:
			goto fail;
		}
		id1 = intab->syLut.TrSym(opr1);
		id2 = intab->syLut.TrSym(opr2);
		intab->opMat.Put(id1, id2, result);
	}
	for(i = 0; i < COUNT_OF(reserved_symbols); i++)
		intab->syLut.Put(reserved_symbols[i]);

	assert(intab->syLut.Put("defined") == SSID_DEFINED);

fail:
	if(rc < 0) {
		intab->opMat.Destroy();
		intab->syLut.Destroy();
	}
	return rc;
}

int paserd_state_init(InternalTables *intab)
{
	if( precedence_matrix_setup(intab, COUNT_OF(g1_oprset), g1_oprset, g1_oprmx) < 0) {
		fatal(140, "Cannot create precedence matrix\n");
		return  -1;
	}
	if(intab->maLut.Create() != 0)
		return -ENOMEM;
	return 0;
}


