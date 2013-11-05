#ifndef  __TCC_H__
#define  __TCC_H__

#include "cc_typedef.h"
#include <inttypes.h>
#include <ctype.h>

#ifdef __cplusplus
  extern "C" {
#endif

typedef struct TCC_CONTEXT {
	CC_HANDLE  symtab;
	CC_HANDLE  matrix;
	CC_HANDLE  mtab;
}  TCC_CONTEXT;

int build_precedence_matrix(TCC_CONTEXT *g, int oprnum, const char **oprset, const char **matrix);

CC_HANDLE  oprmx_create();
void  oprmx_init(CC_HANDLE container);
int   oprmx_add(CC_HANDLE container, sym_t op1, sym_t op2, int precedence);
void  oprmx_destroy(CC_HANDLE container);
int   oprmx_compare(CC_HANDLE container, sym_t op1, sym_t op2);

/*-----------------------*/
#define  OP_EQUAL   0
#define  OP_LOWER   1
#define  OP_HIGHER  2
#define  OP_ERROR   3

/* Token Attributes */
#define  TA_OPERATOR      0
#define  TA_UINT          1
#define  TA_INT           2
#define  TA_IDENTIFIER    3
#define  TA_STRING        4 // "XXX"
#define  TA_CHAR          5 // 'Y'
#define  TA_MACRO_STR     6 // XX##YY#xx

#define COUNT_OF(a)  (sizeof(a) / sizeof(a[0]))

CC_HANDLE  symtab_create();
      int  symtab_init(CC_HANDLE, int, const char **);
      int  symtab_insert(CC_HANDLE, const char *);
     void  symtab_destroy(CC_HANDLE _sym_tab);

       int   symbol_to_id(CC_HANDLE, const char *);
const char * id_to_symbol(CC_HANDLE, int);

typedef struct {
	sym_t  id;
	short  attr;
	union {
		int32_t     i32_val;
		uint32_t    u32_val;
		int         b_val;
		const char *sym;
	};
#if !  __OPTIMIZE__
	int  base;
	const char *symbol;
#endif
} TOKEN;

typedef uint16_t XCHAR;

#define XF_MACRO_PARAM      0x8000 // Macro Parameter
#define XF_MACRO_PARAM1     0x4000 // Macro Parameter: #x
#define XF_MACRO_PARAM2     0x2000 // Macro Parameter: ##x

typedef struct {
	sym_t       id;
	int16_t     arg_nr;
	char       *line;
	XCHAR      *define;
} MACRO_INFO;

#define TCC_MACRO_UNDEF     ((MACRO_INFO *) 1) /* undefined macro */

CC_HANDLE    macro_table_create();
      int    macro_table_insert(CC_HANDLE h, sym_t id, MACRO_INFO *minfo);
      void   macro_table_delete(CC_HANDLE h, sym_t id);
MACRO_INFO * macro_table_lookup(CC_HANDLE h, sym_t id);
     void    macro_table_cleanup(CC_HANDLE h);
     sym_t   macro_table_list(CC_HANDLE h, MACRO_INFO **minfo);

typedef const char * ERR_STRING;

#define TT_FLAG_GET_SHARP       0x001
#define TT_FLAG_RET_SYMBOL      0x002
#define TT_FLAG_NO_SYMID        0x004
bool read_token(TCC_CONTEXT *tc, const char **line, TOKEN *token, ERR_STRING *errs, int flags);
bool take_include_token(TCC_CONTEXT *tc, const char **line, ERR_STRING *errs);


static inline bool identifier_char(char c)
{   return isalpha(c) || isdigit(c) || c == '_'; }

#include <ctype.h>
#define SKIP_WHITE_SPACES(ptr)    while( isspace(*ptr) ) ptr++
#define IS_EOL(c)        ((c) == '\n' || (c) == '\0')


#ifdef __cplusplus
	}
#endif

#endif

