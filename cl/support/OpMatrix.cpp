#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <errno.h>

#include <map>
#include "InternalTables.h"

typedef unsigned int ykey_t;
typedef std::map<ykey_t,int>  II_MAP;

#define MAKE_KEY(op1, op2)  (((op1) << 16) | (op2))
#define GET_CC_MAP_CONTAINER(_c, _h)  II_MAP &_c = (* (II_MAP *) _h)

int OPMatrix::Create()
{
	II_MAP  *opm;
	opm = new II_MAP;
	if( opm == NULL )
		return -ENOMEM;
	handle = (CC_HANDLE) opm;
    return 0;
}

void OPMatrix::Construct()
{
	GET_CC_MAP_CONTAINER(container, handle);
	container.clear();
}

int OPMatrix::Put(sym_t op1, sym_t op2, int precedence)
{
	GET_CC_MAP_CONTAINER(container, handle);
	II_MAP::iterator pos;
	ykey_t key = MAKE_KEY(op1, op2);

	if( (pos = container.find(key)) == container.end() ) {
		if( ! (container.insert(II_MAP::value_type(key,precedence))).second )
			return -EINVAL;
		return 0;
	}
	return -EEXIST;
}

void OPMatrix::Destroy()
{
	delete ((II_MAP *) handle);
}

int OPMatrix::Compare(sym_t op1, sym_t op2)
{
	GET_CC_MAP_CONTAINER(container, handle);
	const ykey_t key = MAKE_KEY(op1, op2);
	II_MAP::iterator pos;

	if( (pos = container.find(key)) == container.end() ) 
		return -EINVAL;
	return pos->second;
}

/*-------------------------------------------*/
