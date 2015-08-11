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

#define offset_of(type, member) ((unsigned long)(&((type*)0)->member))
#define container_of(ptr, type, member)  ((type*)((unsigned long)(ptr) - offset_of(type,member)))

#include "file.h"

#define INV_LN           0xFFFFEEFF

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

/* Tri-State values */
enum TRI_STATE {
	TSV_0 = 0,
	TSV_1 = 1,
	TSV_X = 2,
};

class CYcpp {
protected:
	static const char *preprocessors[];
	size_t       num_preprocessors;
	CP_CONTEXT   *rtctx;

	/*-------------------------------------------------------------*/
	class CConditionalChain {
	private:
		enum {
			FG_TRUE_ON_IF    = 1,
			FG_TRUE_ON_ELIF  = 2,
			FG_HAS_X         = 4,

			SG_ON_NONE       = 0,
			SG_ON_IF         = 1,
			SG_ON_ELIF       = 2,
			SG_ON_ELSE       = 3,
		};
		TRI_STATE  value;
		int flags;
		int stage;

	public:
		/*-- For saving parsing results --*/
		const char *filename;
		size_t line;
#if 0
		struct TSubBlock {
			uint32_t  begin;
			bool      value;
		};
		CC_ARRAY<TSubBlock> branches;
		uint32_t end;
#endif
		inline CConditionalChain();
		inline void enter_if_branch(TRI_STATE value);
		inline void enter_elif_branch(TRI_STATE value);
		inline void enter_else_branch();
		inline bool keep_endif();
		inline TRI_STATE eval_condition();
	};
	inline CConditionalChain *upper_chain() { return conditionals.size() == 0 ? NULL : conditionals.top(); }
	CC_STACK<CConditionalChain*> conditionals;
	TRI_STATE eval_current_condition();
	TRI_STATE eval_upper_condition(bool on_if);

	/*-------------------------------------------------------------*/
	class  TIncludedFile {
		class TListEntry {
		public:
			void Init();
			void AddTail(TListEntry *entry);
			bool IsEmpty();
			TListEntry *last();
		private:
			TListEntry *next;
			TListEntry *prev;
			void AddAfter(TListEntry *pos, TListEntry *entry);
			friend class TIncludedFile;
		};

		class TCondChain;

		class TCond {
		private:
			TListEntry link;
			enum COND_TYPE {
				CT_ROOT = 0,
				CT_IF   = 1,
				CT_ELIF = 2,
				CT_ELSE = 3,
			};
			const COND_TYPE type;
			const size_t begin;
			size_t       end;
			TListEntry   sub_chains; /* subordinate chains */

		public:
			TCond(TCondChain *cc, COND_TYPE ctype, size_t ln);
			void append(TCondChain *cc);
			TCondChain *superior();

			friend class TCondChain;
			friend class TIncludedFile;
		};

		class TCondChain {
		private:
			TListEntry  link;
			TListEntry  chain; /* The conditional chain */
			size_t      begin;
			size_t      end; /* The end line number */
			TCond      *true_cond;
			TCond      *superior;

		public:
			TCondChain();
			void mark_end(size_t);
			void add_if(TCond *, bool);
			void add_elif(TCond*, bool);
			void add_else(TCond*, bool);
			void add_endif(size_t);

			friend class TCond;
			friend class TIncludedFile;
		};

		typedef void (*ON_CONDITIONAL_CALLBACK)(void *, TCond *, bool);
		typedef void (*ON_CONDITIONAL_CHAIN_CALLBACK)(void *, TCondChain *);

		class CWalkThrough {
		public:
			enum WT_METHOD {
				MED_BF, /* breadth-first */
				MED_DF, /* depth-first   */
			};
		private:
			enum WT_METHOD method;
			void *context;
			ON_CONDITIONAL_CALLBACK       on_conditional_callback;
			ON_CONDITIONAL_CHAIN_CALLBACK on_conditional_chain_callback;
			void enter_conditional(TCond *c, bool);
			void enter_conditional_chain(TCondChain *cc);
		public:
			CWalkThrough(TCond *rc, WT_METHOD method, ON_CONDITIONAL_CHAIN_CALLBACK callback1, ON_CONDITIONAL_CALLBACK callback2, void *context);
		};

		size_t   nh; /* The hierarchy number of including */

		TCond *virtual_root;
		TCond *cursor;
		bool under_false();

		static void drop_conditional(void *, TCond *c, bool);
		static void drop_conditional_chain(void *, TCondChain *cc);

		static void delete_conditional(void *, TCond *, bool);
		static void delete_conditional_chain(void *, TCondChain *);

		static void save_conditional_to_json(void *, TCond *, bool);
		static void save_conditional_chain_to_json(void *, TCondChain *);

		void save_conditional_to_json(TCond *, bool);
		void save_conditional_chain_to_json(TCondChain *);

	public:
		CFile   *ifile; /* Source File */
		FILE    *ofile; /* Dest File   */
		size_t   np;    /* The number of preporcessors */
		CC_STRING json_text;

		TIncludedFile(CFile *s, FILE *of, size_t np, size_t nh);
		~TIncludedFile();
		void add_if(size_t line_nr, bool value);    // #if
		void add_elif(size_t line_nr, bool value);  // #elif
		void add_else(size_t line_nr, bool value);  // #else
		void add_endif(size_t line_nr);             // #endif
		void get_json_text();
	};
	CC_STACK<TIncludedFile*> included_files;

	inline TIncludedFile& GetCurrentFile();
	inline const CC_STRING& GetCurrentFileName();
	inline const size_t GetCurrentLineNumber();

	inline void PushIncludedFile(CFile *srcfile, FILE *of, size_t np, size_t nh);
	inline TIncludedFile *PopIncludedFile();

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

	CC_STRING   json_file;


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

	inline void mark_comment_start();

public:
	CC_STRING errmsg;
	inline CYcpp();
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


