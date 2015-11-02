#ifndef __PARSED_STATE_H
#define __PARSED_STATE_H

#include "cc_string.h"
#include <inttypes.h>

typedef struct _CC_HANDLE *CC_HANDLE;

typedef uint32_t sym_t;

enum OP_PRIO {
	_LT = -1,
	_EQ = 0,
	_GT = 1,
	_XX = 2,
};

#ifndef COUNT_OF
#define COUNT_OF(a)  (sizeof(a) / sizeof(a[0]))
#endif

/* Operator Precedence Matrix */
class OPMatrix {
public:
  int  Create();
  void Construct();
  void Destroy();
  int  Put(sym_t op1, sym_t op2, int precedence);
  int  Compare(sym_t op1, sym_t op2);
private:
  CC_HANDLE handle;
};

/* Symbol Lookup Table */
class SymLut {
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

struct SynToken {
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
struct SynMacro {
    static SynMacro *const NotDef;
	enum { OL_M = 0xFFFF };
	sym_t       id;
	uint16_t    nr_args;
	bool        va_args;
	char       *line;
	XCHAR      *parsed;
};

/* Macro Lookup Table */
class MacLut {
public:
  int    Create();
  int    Put(sym_t id, SynMacro *minfo);
  void   Remove(sym_t id);
  SynMacro* Lookup(sym_t id);
  void   Destroy();

private:
  CC_HANDLE handle;
};

struct ParsedState {
	SymLut    syLut;   /* The Symbol Translattion Table */
	OPMatrix  opMat;   /* The Operator Precedence Matrix */
	MacLut    maLut;   /* The Macro Definition Table */
};

int paserd_state_init(ParsedState *pstate);

#include "precedence-matrix.h"

#endif

