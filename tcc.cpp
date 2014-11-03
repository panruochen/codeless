#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>

#define  __OPM_CONST_DATA
#include "tcc.h"
#include "utils.h"

static int precedence_matrix_setup(TCC_CONTEXT *tc, size_t oprnum, const char **oprset, const char **matrix)
{
	size_t i;
	int rc = 0;

	if( tc->syMap.Create() != 0)
		goto fail;
	if( tc->opMap.Create() != 0 )
		goto fail;
	tc->syMap.Construct(oprnum, oprset);
	tc->opMap.Construct();
	for(i = 0; i < oprnum * oprnum; i++) {
		char opr1[16], opr2[16];
		int result, id1, id2;
		char prec;
		sscanf(matrix[i],"%s %s %c", opr1, opr2, &prec);
		switch(prec) {
		case '=': result = OP_EQUAL; break;
		case '<': result = OP_LOWER; break;
		case '>': result = OP_HIGHER; break;
		case 'X': result = OP_ERROR; break;
		default:
			goto fail;
		}
		id1 = tc->syMap.TrSym(opr1);
		id2 = tc->syMap.TrSym(opr2);
		tc->opMap.Put(id1, id2, result);
	}
	for(i = 0; i < COUNT_OF(reserved_symbols); i++)
		tc->syMap.Put(reserved_symbols[i]);

	assert(tc->syMap.Put("defined") == SSID_DEFINED);

fail:
	if(rc < 0) {
		tc->opMap.Destroy();
		tc->syMap.Destroy();
	}
	return rc;
}


int tcc_init(TCC_CONTEXT *tc)
{
	if( precedence_matrix_setup(tc, COUNT_OF(g1_oprset), g1_oprset, g1_oprmx) < 0) {
		fatal(140, "Cannot create precedence matrix\n");
		return  -1;
	}
	if(tc->maMap.Create() != 0)
		return -ENOMEM;
	return 0;
}


