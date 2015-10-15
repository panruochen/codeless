#include <string>
#include <map>
#include <string.h>
#include <errno.h>
#include "cc_string.h"

#include "ParsedState.h"

struct str_less {
    bool operator() (const CC_STRING& s1, const CC_STRING& s2)
    {
        return strcmp(s1.c_str(), s2.c_str()) < 0;
    }
};

typedef std::map<CC_STRING, sym_t, str_less>  MAP1;
typedef std::map<sym_t, CC_STRING>  MAP2;

struct SYM_TABLE {
	MAP1 map1; // The Symbol-To-Id map
	MAP2 map2; // The Id-To-Symbol map
	int cur_id;
};

int SymLut::Create()
{
	SYM_TABLE  *symtab;
		
	symtab = new SYM_TABLE;
	if( symtab == NULL )
		return -ENOMEM;
	handle = (CC_HANDLE) symtab;
    return 0;
}

int SymLut::Construct(int min_id, const char *reserved_symbols[])
{
	SYM_TABLE&  symtab = *(SYM_TABLE *) handle;
	symtab.map1.clear();
	symtab.map2.clear();
	symtab.cur_id = min_id;

	int i;
	for(i=0; i<min_id; i++) {
		std::pair<MAP1::iterator, bool>  result1;
		std::pair<MAP2::iterator, bool>  result2;
        CC_STRING symbol(reserved_symbols[i]);

		result1 = symtab.map1.insert(MAP1::value_type(symbol,i));
		if( ! result1.second )
			return -1;
		result2 = symtab.map2.insert(MAP2::value_type(i,symbol));
		if( ! result2.second )
			return -1;
	}
	return 0;
}

sym_t SymLut::Put(const CC_STRING& symbol)
{
	SYM_TABLE&  symtab = *(SYM_TABLE *) handle;
	MAP1& map1 = symtab.map1;
	MAP2& map2 = symtab.map2;
	MAP1::iterator pos;
	
	pos = map1.find(symbol);
	if(pos == map1.end()) {
		std::pair<MAP1::iterator, bool>  result1;
		std::pair<MAP2::iterator, bool>  result2;
		result1 = map1.insert( MAP1::value_type(symbol, symtab.cur_id) );
		if( ! result1.second )
			return (-1);
		result2 = map2.insert( MAP2::value_type(symtab.cur_id, symbol) );
		if( ! result2.second )
			return (-1);
		return symtab.cur_id++;
	} else {
		return pos->second;
	}
}

void SymLut::Destroy()
{
	SYM_TABLE *symtab = (SYM_TABLE *) handle;

	symtab->map1.clear();
    symtab->map2.clear();
	delete symtab;
}

sym_t SymLut::TrSym(const CC_STRING& symbol)
{
	SYM_TABLE&  symtab = *(SYM_TABLE *) handle;
	MAP1&    map1 =  symtab.map1;
	MAP1::iterator pos;
	
	pos = map1.find(symbol);
	if(pos == map1.end())
		return -1;
	return pos->second;
}

const CC_STRING& SymLut::TrId(const sym_t id)
{
	SYM_TABLE&  symtab = *(SYM_TABLE *) handle;
	MAP2::iterator pos;

	pos = symtab.map2.find(id);
	return pos->second;
}


