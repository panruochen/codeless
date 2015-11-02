#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>

#define  __OPM_CONST_DATA
#include "ParsedState.h"
#include "utils.h"

static int precedence_matrix_setup(ParsedState *pstate, size_t oprnum, const char **oprset, const char **matrix)
{
	size_t i;
	int rc = 0;

	if( pstate->syLut.Create() != 0)
		goto fail;
	if( pstate->opMat.Create() != 0 )
		goto fail;
	pstate->syLut.Construct(oprnum, oprset);
	pstate->opMat.Construct();
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
		id1 = pstate->syLut.TrSym(opr1);
		id2 = pstate->syLut.TrSym(opr2);
		pstate->opMat.Put(id1, id2, result);
	}
	for(i = 0; i < COUNT_OF(reserved_symbols); i++)
		pstate->syLut.Put(reserved_symbols[i]);

	assert(pstate->syLut.Put("defined") == SSID_DEFINED);

fail:
	if(rc < 0) {
		pstate->opMat.Destroy();
		pstate->syLut.Destroy();
	}
	return rc;
}

int paserd_state_init(ParsedState *pstate)
{
	if( precedence_matrix_setup(pstate, COUNT_OF(g1_oprset), g1_oprset, g1_oprmx) < 0) {
		fatal(140, "Cannot create precedence matrix\n");
		return  -1;
	}
	if(pstate->maLut.Create() != 0)
		return -ENOMEM;
	return 0;
}


