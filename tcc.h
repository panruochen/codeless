#ifndef  __TCC_H__
#define  __TCC_H__

#include "cc_string.h"
#include <inttypes.h>

/* A Tiny C/C++ Preprocessor */

struct _CC_HANDLE;
typedef struct _CC_HANDLE *CC_HANDLE;


typedef unsigned long sym_t;

/* Operator Precedence Maxtrix */
class COPMatrix {
public:
  int  Create();
  void Construct();
  void Destroy();
  int  Put(sym_t op1, sym_t op2, int precedence);
  int  Compare(sym_t op1, sym_t op2);
private:
  CC_HANDLE handle;
};

/*-----------------------*/
#define  OP_EQUAL   0
#define  OP_LOWER   1
#define  OP_HIGHER  2
#define  OP_ERROR   3

#define COUNT_OF(a)  (sizeof(a) / sizeof(a[0]))

/* Symbol Table */
class CSymMap {
public:
  int    Create();
  int    Construct(int, const char **);
  void   Destroy();
  sym_t  Put(const CC_STRING&);
  sym_t  TrSym(const CC_STRING&);
  const CC_STRING&  TrId(sym_t);

protected:
    CC_HANDLE handle;
};

/* Lexical Token */
struct CToken {
    enum {
        TA_OPR,
        TA_UINT,
        TA_INT,
        TA_IDENT,
        TA_STR,
        TA_CHAR,
    };
	sym_t  id;
	short  attr;
	union {
		int32_t     i32_val;
		uint32_t    u32_val;
		int         b_val;
	};
    CC_STRING       name;
};

typedef uint16_t XCHAR;

#define XF_MA_PAR0     0x4000  // macro parameter
#define XF_MA_PAR1     0x8000  // macro parameter: #x
#define XF_MA_PAR2     0xC000  // macro parameter: ##x

#define __IS_MA_PAR(xc,n)  (((xc) & 0xc000) == XF_MA_PAR##n)
#define IS_MA_PAR0(xc)     __IS_MA_PAR(xc,0)  
#define IS_MA_PAR1(xc)     __IS_MA_PAR(xc,1)  
#define IS_MA_PAR2(xc)     __IS_MA_PAR(xc,2)

/* Macro */
struct CMacro {
    static CMacro *const NotDef;
	enum { OL_M = 0xFFFF };
	sym_t       id;
	uint16_t    nr_args;
	bool        va_args;
	char       *line;
	XCHAR      *parsed;
};

/* Macro Class */
class CMacMap {
public:
  int    Create();
  int    Put(sym_t id, CMacro *minfo);
  void   Remove(sym_t id);
CMacro*  Lookup(sym_t id);
  void   Destroy();

private:
  CC_HANDLE handle;
};

/*
 * The TCC context structure
 **/
typedef struct TCC_CONTEXT {
	CSymMap    syMap;   /* The Symbol Translattion Table */
	COPMatrix  opMap;   /* The Operator Precedence Matrix */
	CMacMap    maMap;   /* The Macro Definition Table */
}  TCC_CONTEXT;


int tcc_init(TCC_CONTEXT *tc);

#include "precedence-matrix.h"

#endif

