#ifndef  __PXCC_H

#include <errno.h>
#include <string.h>

#include "precedence-matrix.h"
#include "cc_string.h"
#include "cc_array.h"
#include "log.h"
#include "tcc.h"
#include "fol.h"

#if __linux
#include <linux/limits.h>
#elif   !defined(PATH_MAX)
#define PATH_MAX    4096
#endif

#define __NO_USE__   __attribute__((__unused__))

#include "file.h"


class CP_CONTEXT {
private:
	int    get_options(int argc, char *argv[], const char *short_options, const struct option *long_options);
    int    via_file_read(const char *filename, char ***pargv);
	int    last_optind;
	int    levels;
	int    do_quote;
	void   Reset();

public:
	CC_ARRAY<CC_STRING>  source_files;
	CC_ARRAY<CC_STRING>  search_dirs;
	CC_ARRAY<CC_STRING>  compiler_search_dirs;
	CMemFile             predef_macros;
	CC_STRING            outfile;
	CC_STRING            save_depfile; /* saved dependency file */
	CC_STRING            save_clfile;  /* saved command-line file*/
	CC_STRING            save_byfile;  /* saved bypass file */
	CC_STRING            baksuffix;
	CC_STRING            cc;
	CC_STRING            cc_path;
	CC_STRING            save_dep_file;
	CC_STRING            cc_args;
	CC_STRING            my_args;
	bool                 nostdinc;
	CC_ARRAY<CC_STRING>  imacro_files;
	CC_ARRAY<CC_STRING>  isystem_dirs;
	CC_ARRAY<CC_STRING>  include_files;
	CC_ARRAY<CC_STRING>  bypass_list;

	int                  argc;
	char               **argv;

	int    get_options(int argc, char *argv[]);
	void   save_cc_args();
	void   save_my_args();

	bool check_if_bypass(const CC_STRING& filename);
	CC_STRING get_include_file_path(const CC_STRING& included_file, const CC_STRING& current_file,
		bool quote_include, bool include_next, bool *in_sys_dir);
};

class CException {
protected:
	CC_STRING  msg;

public:
    inline CException(void);
    inline CException(const char *msg);
    inline CException(const CC_STRING& msg);
	inline const char *GetError();
	inline CException& operator = (const char *msg);
	inline CException& operator = (const CC_STRING& msg);
    inline void format(const char *fmt, ...);
};


class CYcpp {
protected:
	static const char *preprocessors[];
	size_t       num_preprocessors;
	CP_CONTEXT   *rtctx;

	/*-------------------------------------------------------------*/
	struct CONDITIONAL {
		int8_t  if_value;
		int8_t  elif_value;
		int8_t  curr_value;
	};
	CONDITIONAL            current;
	CC_STACK<CONDITIONAL>  conditionals;
	bool  current_state_pop();
	bool  under_false_conditional();

	/*-------------------------------------------------------------*/
	struct  INCLUDE_LEVEL {
		CFile   *srcfile;
		FILE    *outfp;
		size_t   np;
		size_t   if_level;

		INCLUDE_LEVEL(CFile *s, FILE *of, size_t np, size_t ilevels)
		{ srcfile = s; outfp = of; this->np = np; if_level=ilevels;}
		INCLUDE_LEVEL()
		{ srcfile = NULL; outfp = NULL; np = 0; if_level = 0; }
	};
	CC_STACK<INCLUDE_LEVEL> include_levels;

	inline CFile *current_file();
	inline void include_level_push(CFile *srcfile, FILE *of, size_t np, size_t ilvl);
	inline INCLUDE_LEVEL include_level_pop();

	/*-------------------------------------------------------------*/
	TCC_CONTEXT   *tc;

	CC_STRING      baksuffix;
	CC_STRING      deptext;
	CException     gex;

	/* line-process based variables */
	ssize_t comment_start;
	CC_STRING    line;     /* The comment-stripped line */
	CC_STRING    raw_line; /* The original input line */
	CC_STRING    new_line; /* The processed output line */

	CC_STRING  do_elif(int mode);
	void       do_define(const char *line);
	bool       do_include(sym_t preprocess, const char *line, const char **output);

	void   Reset(TCC_CONTEXT *tc, size_t num_preprocessors, CP_CONTEXT *ctx);
	sym_t  GetPreprocessor(const char *line, const char **pos);
	int    ReadLine();
	bool   SM_Run();
	void   AddDependency(const char *prefix, const CC_STRING& filename);
	CFile* GetIncludedFile(sym_t preprocessor, const char *line, FILE** outf);

	bool GetCmdLineIncludeFiles(const CC_ARRAY<CC_STRING>& ifiles, size_t np);
	bool RunEngine(size_t n);

	inline bool has_dep_file();
	bool check_file_processed(const CC_STRING& filename);

	inline void mark_comment_start()
	{ comment_start = raw_line.size() - 2; }

public:
	CC_STRING errmsg;
	CYcpp() {
		tc          = NULL;
	}
	bool DoFile(TCC_CONTEXT *tc, size_t num_preprocessors, CFile  *infile, CP_CONTEXT *cp_ctx);
};


CC_STRING  ExpandLine(TCC_CONTEXT *tc, bool for_include, const char *line, CException *gex);

bool ReadToken(TCC_CONTEXT *tc, const char **line, CToken *token, CException *gex, bool for_include);

void handle_undef(TCC_CONTEXT *tc, const char *line);

enum SOURCE_TYPE {
	SOURCE_TYPE_ERR = 0,
	SOURCE_TYPE_C   = 1,
	SOURCE_TYPE_CPP = 2,
};
SOURCE_TYPE check_source_type(const CC_STRING& filename);

#define IS_STDOUT(s)     (s == "/dev/stdout")
#define TR(tc,opr)       tc->syMap.TrId(opr).c_str()
#define TOKEN_NAME(a)    (a).name.c_str()

#define CB_BEGIN     {
#define CB_END       }

#include "ycpp.inl"
#include "char.h"
#include "globalvar.h"

#endif


