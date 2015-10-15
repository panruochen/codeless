#include <string>
#include <map>
#include <string.h>
#include <stdlib.h>
#include <errno.h>

#include "ParsedState.h"

typedef std::map<sym_t,SynMacro*>  MAP;

struct MACRO_TABLE {
	MAP             map;
	bool            first;
	MAP::iterator   curpos;
};

#define DECLARE(_m,_h)  MAP& _m = ((MACRO_TABLE *)(_h))->map

int MacLut::Create()
{
	MACRO_TABLE  *mtab;
		
	mtab = new MACRO_TABLE;
	if( mtab == NULL )
		return -ENOMEM;
	mtab->first = true;
	handle = (CC_HANDLE) mtab;
    return 0;
}

int MacLut::Put(sym_t id, SynMacro *minfo)
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
		SynMacro *old = pos->second;
		if(old != SynMacro::NotDef)
			free(old);
		pos->second = minfo;
		return id;
	}
	return -1;
}

void MacLut::Remove(sym_t id)
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
 * Others          -- A pointer to a SynMacro structure
 */
SynMacro *MacLut::Lookup(sym_t id)
{
	DECLARE(map, handle);
	MAP::iterator pos;

	pos = map.find(id);
	if(pos == map.end())
		return NULL;
	return pos->second;
}


void MacLut::Destroy()
{
	MACRO_TABLE *mt = (MACRO_TABLE *)handle;
	DECLARE(map, handle);

	map.clear();
	delete mt;
}

/***
sym_t MacMap_list(CC_HANDLE __h, SynMacro **minfo)
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
