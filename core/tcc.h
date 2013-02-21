#ifndef  __TCC_H__
#define  __TCC_H__

#include "cc_typedef.h"
#include <inttypes.h>

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

#define COUNT_OF(a)  (sizeof(a) / sizeof(a[0]))

CC_HANDLE  symtab_create();
int  symtab_init(CC_HANDLE, int, const char **);
int  symtab_insert(CC_HANDLE, const char *);
void symtab_destroy(CC_HANDLE _sym_tab);

       int   symbol_to_id(CC_HANDLE, const char *);
const char * id_to_symbol(CC_HANDLE, int);

typedef struct {
	sym_t  id;
	short  attr;
	union {
		int32_t  i32_val;
		uint32_t u32_val;
		int      b_val;
		int      flag_flm; /* Function-like-macro flag */
	};
#if HAVE_NAME_IN_TOKEN
	int  base;
	const char *name;
#endif
} TOKEN;

typedef struct {
	size_t nr;
	TOKEN  buf[0];
} TOKEN_SEQ;

typedef struct {
	size_t nr;
	sym_t  buf[0];
} PARA_INFO;

typedef struct {
	int        handled;
	char       *line;
	void       *priv;
	PARA_INFO  *params; /* The paramater list */
	TOKEN_SEQ  *def;    /* The macro definition */
} MACRO_INFO;


#define TCC_MACRO_UNDEF     ((MACRO_INFO *) 1) /* undefined macro */

CC_HANDLE    macro_table_create();
      int    macro_table_insert(CC_HANDLE __h, sym_t id, MACRO_INFO *minfo);
      void   macro_table_delete(CC_HANDLE __h, sym_t id);
MACRO_INFO * macro_table_lookup(CC_HANDLE __h, sym_t id);
     void    macro_table_cleanup(CC_HANDLE __h);
     sym_t   macro_table_list(CC_HANDLE __h, MACRO_INFO **minfo);

#ifdef __cplusplus
	}
#endif

#endif

