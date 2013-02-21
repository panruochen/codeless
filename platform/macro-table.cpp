#include <string>
#include <map>
#include <string.h>
#include <stdlib.h>

#include "tcc.h"

typedef std::map<sym_t,MACRO_INFO*>  MAP;

struct MACRO_TABLE {
	MAP             map;
	bool            first;
	MAP::iterator   curpos;
};

#define DECLARE(_m,_h)  MAP& _m = ((MACRO_TABLE *)(_h))->map

CC_HANDLE macro_table_create()
{
	MACRO_TABLE  *mtab;
		
	mtab = new MACRO_TABLE;
	if( mtab == NULL )
		return NULL;
	mtab->first = true;
	return (CC_HANDLE) mtab;
}

int macro_table_insert(CC_HANDLE __h, sym_t id, MACRO_INFO *minfo)
{
	DECLARE(map, __h);
	MAP::iterator pos;

	pos = map.find(id);
	if(pos == map.end()) {
		std::pair<MAP::iterator, bool>  result;
		result = map.insert( MAP::value_type(id, minfo) );
		if( ! result.second )
			return (-1);
		return id;
	} else {
		MACRO_INFO *old = pos->second;
		if(old != TCC_MACRO_UNDEF)
			free(old);
		pos->second = minfo;
		return id;
	}
	return -1;
}

void macro_table_delete(CC_HANDLE __h, sym_t id)
{
	DECLARE(map, __h);
	MAP::iterator pos;

	pos = map.find(id);
	if(pos != map.end()) 
		map.erase(pos);
}

/******************************************
 * Return Value:
 * NULL            -- The macro name is NOT explicitly defined
 * TCC_MACRO_UNDEF -- The macro name is explicitly undefined
 * Others          -- A pointer to a MACRO_INFO structure
 */
MACRO_INFO *macro_table_lookup(CC_HANDLE __h, sym_t id)
{
	DECLARE(map, __h);
	MAP::iterator pos;

	pos = map.find(id);
	if(pos == map.end())
		return NULL;
	return pos->second;
}


void macro_table_cleanup(CC_HANDLE __h)
{
	MACRO_TABLE *mt = (MACRO_TABLE *)__h;
	DECLARE(map, __h);
	
	map.clear();
	delete mt;
}

sym_t macro_table_list(CC_HANDLE __h, MACRO_INFO **minfo)
{
	MACRO_TABLE *mt = (MACRO_TABLE *)__h;
	DECLARE(map, __h);

	if(mt->first) {
		mt->curpos = map.begin();
		mt->first  = false;
	} else {
		mt->curpos++;
	}
	return mt->curpos != map.end() ? (*minfo = mt->curpos->second, mt->curpos->first) : (mt->first = true, (sym_t)-1);
}

