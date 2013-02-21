#include <string>
#include <map>

#include "tcc.h"

typedef std::map<std::string, int>  MAP1;
typedef std::map<int, std::string>  MAP2;

struct SYM_TABLE {
	MAP1 map1; // The Symbol-To-Id map
	MAP2 map2; // The Id-To-Symbol map
	int cur_id;
};

CC_HANDLE symtab_create()
{
	SYM_TABLE  *symtab;
		
	symtab = new SYM_TABLE;
	if( symtab == NULL )
		return NULL;
	return (CC_HANDLE) symtab;
}

int symtab_init(CC_HANDLE _sym_tab, int min_id, const char *reserved_symbols[])
{
	SYM_TABLE&  symtab = *(SYM_TABLE *) _sym_tab;
	symtab.map1.clear();
	symtab.map2.clear();
	symtab.cur_id = min_id;

	int i;
	for(i=0; i<min_id; i++) {
		std::pair<MAP1::iterator, bool>  result1;
		std::pair<MAP2::iterator, bool>  result2;

		result1 = symtab.map1.insert(MAP1::value_type(reserved_symbols[i],i));
		if( ! result1.second )
			return -1;
		result2 = symtab.map2.insert(MAP2::value_type(i,reserved_symbols[i]));
		if( ! result2.second )
			return -1;
	}
	return 0;
}

int symtab_insert(CC_HANDLE _sym_tab, const char *symbol)
{
	SYM_TABLE&  symtab = *(SYM_TABLE *) _sym_tab;
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

void symtab_destroy(CC_HANDLE _sym_tab)
{
	SYM_TABLE *symtab = (SYM_TABLE *) _sym_tab;
	
	symtab->map1.clear();
	delete symtab;
}

int symbol_to_id(CC_HANDLE _sym_tab, const char *symbol)
{
	SYM_TABLE&  symtab = *(SYM_TABLE *) _sym_tab;
	MAP1&    map1 =  symtab.map1;
	MAP1::iterator pos;
	
	pos = map1.find(symbol);
	if(pos == map1.end())
		return -1;
	return pos->second;
}

const char *id_to_symbol(CC_HANDLE _sym_tab, int id)
{
	SYM_TABLE&  symtab = *(SYM_TABLE *) _sym_tab;
	MAP2::iterator pos;
	
	pos = symtab.map2.find(id);
	return pos->second.c_str();
}

