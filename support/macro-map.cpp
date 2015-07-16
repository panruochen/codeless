#include <string>
#include <map>
#include <string.h>
#include <stdlib.h>
#include <errno.h>

#include "tcc.h"

typedef std::map<sym_t,CMacro*>  MAP;

struct MACRO_TABLE {
	MAP             map;
	bool            first;
	MAP::iterator   curpos;
};

#define DECLARE(_m,_h)  MAP& _m = ((MACRO_TABLE *)(_h))->map

int CMacMap::Create()
{
	MACRO_TABLE  *mtab;
		
	mtab = new MACRO_TABLE;
	if( mtab == NULL )
		return -ENOMEM;
	mtab->first = true;
	handle = (CC_HANDLE) mtab;
    return 0;
}

int CMacMap::Put(sym_t id, CMacro *minfo)
{
	DECLARE(map, handle);
	MAP::iterator pos;

	pos = map.find(id);
	if(pos == map.end()) {
		std::pair<MAP::iterator, bool>  result;
		result = map.insert( MAP::value_type(id, minfo) );
		if( ! result.second )
			return (-1);
		return id;
	} else {
		CMacro *old = pos->second;
		if(old != CMacro::NotDef)
			free(old);
		pos->second = minfo;
		return id;
	}
	return -1;
}

void CMacMap::Remove(sym_t id)
{
	DECLARE(map, handle);
	MAP::iterator pos;

	pos = map.find(id);
	if(pos != map.end()) 
		map.erase(pos);
}

/******************************************
 * Return Value:
 * NULL            -- The macro name is NOT explicitly defined
 * TCC_MACRO_UNDEF -- The macro name is explicitly undefined
 * Others          -- A pointer to a CMacro structure
 */
CMacro *CMacMap::Lookup(sym_t id)
{
	DECLARE(map, handle);
	MAP::iterator pos;

	pos = map.find(id);
	if(pos == map.end())
		return NULL;
	return pos->second;
}


void CMacMap::Destroy()
{
	MACRO_TABLE *mt = (MACRO_TABLE *)handle;
	DECLARE(map, handle);

	map.clear();
	delete mt;
}

/***
sym_t MacMap_list(CC_HANDLE __h, CMacro **minfo)
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
***/
