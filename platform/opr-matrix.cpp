#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>

#include <map>
#include "tcc.h"

typedef unsigned int ykey_t;
typedef std::map<ykey_t,int>  II_MAP;

#define MAKE_KEY(op1, op2)  (((op1) << 16) | (op2))
#define GET_CC_MAP_CONTAINER(_c, _h)  II_MAP &_c = (* (II_MAP *) _h)

CC_HANDLE oprmx_create()
{
	II_MAP  *handle;
	handle = new II_MAP;
	if( handle == NULL )
		return NULL;
	return (CC_HANDLE) handle;
}

void oprmx_init(CC_HANDLE handle)
{
	GET_CC_MAP_CONTAINER(container, handle);
	container.clear();
}

int oprmx_add(CC_HANDLE handle, sym_t op1, sym_t op2, int precedence)
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

void oprmx_destroy(CC_HANDLE handle)
{
	delete ((II_MAP *) handle);
}

int oprmx_compare(CC_HANDLE handle, sym_t op1, sym_t op2)
{
	GET_CC_MAP_CONTAINER(container, handle);
	const ykey_t key = MAKE_KEY(op1, op2);
	II_MAP::iterator pos;

	if( (pos = container.find(key)) == container.end() ) 
		return -EINVAL;
	return pos->second;
}

/*-------------------------------------------*/
