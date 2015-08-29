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

#define DBG_TRAP2(cond1, cond2)                   \
do {                                              \
	if( (cond1) && (cond2) )   {                  \
		volatile int loop = 1;                    \
		do {} while (loop);                       \
	}                                             \
} while(0)                                        \

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

	/* Additional output files */
	CC_STRING            of_dep; /* dependencies */
	CC_STRING            of_cl;  /* command line */
	CC_STRING            of_by;  /* bypass */
	CC_STRING            of_con; /* conditional evaluation */

	CC_STRING            baksuffix;
	CC_STRING            cc;
	CC_STRING            cc_path;
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

		inline CConditionalChain();
		inline void enter_if_branch(TRI_STATE value);
		inline void enter_elif_branch(TRI_STATE value);
		inline TRI_STATE enter_else_branch();
		inline bool keep_endif();
		inline TRI_STATE eval_condition();
	};
	CConditionalChain *upper_chain();
	CC_STACK<CConditionalChain*> conditionals;
	TRI_STATE eval_current_condition();
	TRI_STATE eval_upper_condition(bool on_if);

	/*-------------------------------------------------------------*/
	class CIncludedFile {
		class CListEntry {
		public:
			void Init();
			void AddTail(CListEntry *entry);
			bool IsEmpty();
			CListEntry *last();
		private:
			CListEntry *next;
			CListEntry *prev;
			void AddAfter(CListEntry *pos, CListEntry *entry);
			friend class CIncludedFile;
		};

		class CCondChain;

		class CCond {
		private:
			CListEntry  link;
			CCondChain *head;
			enum COND_TYPE {
				CT_ROOT = 0,
				CT_IF   = 1,
				CT_ELIF = 2,
				CT_ELSE = 3,
			};
#if SANITY_CHECK
			char tag[4];
			static const char TAG[4];
#endif
			const COND_TYPE type;
			const uint32_t begin;
			uint32_t end;
			bool value;
			CListEntry sub_chains; /* subordinate chains */

		public:
			CCond(CCondChain *cc, COND_TYPE ctype, bool value, uint32_t ln);
			void append(CCondChain *cc);
			void sanity_check();

			friend class CCondChain;
			friend class CIncludedFile;
		};

		class CCondChain {
		private:
#if SANITY_CHECK
			char tag[4];
			static const char TAG[4];
#endif
			CListEntry  link;
			CListEntry  chain; /* The conditional chain */
			size_t      begin;
			size_t      end; /* The end line number */
			CCond      *superior;

		public:
			CCondChain();
			void mark_end(size_t);
			void add_if(CCond *, bool);
			void add_elif(CCond*, bool);
			void add_else(CCond*, bool);
			void add_endif(size_t);
			void sanity_check();

			friend class CCond;
			friend class CIncludedFile;
		};

		typedef void (*ON_CONDITIONAL_CALLBACK)(void *, CCond *);
		typedef void (*ON_CONDITIONAL_CHAIN_CALLBACK)(void *, CCondChain *);

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
			void enter_conditional(CCond *c);
			void enter_conditional_chain(CCondChain *cc);
		public:
			CWalkThrough(CCond *rc, WT_METHOD method, ON_CONDITIONAL_CHAIN_CALLBACK callback1, ON_CONDITIONAL_CALLBACK callback2, void *context);
		};

		size_t   nh; /* The hierarchy number of including */

		CCond *virtual_root;
		CCond *cursor;
		uint32_t false_level;

		static void delete_conditional(void *, CCond *);
		static void delete_conditional_chain(void *, CCondChain *);

		static void save_conditional_to_json(void *, CCond *);
		static void save_conditional_chain_to_json(void *, CCondChain *);

		void save_conditional_to_json(CCond *);
		void save_conditional_chain_to_json(CCondChain *);

	public:
		CFile   *ifile; /* Source File */
		FILE    *ofile; /* Dest File   */
		size_t   np;    /* The number of preporcessors */
		bool     in_compiler_dir; /* The included file is inside the compiler root directory. */
		CC_STRING json_text;

		CIncludedFile(CFile *s, FILE *of, size_t np, bool in_compiler_dir, size_t nh);
		~CIncludedFile();
		void add_if(bool value);    // #if
		void add_elif(bool value);  // #elif
		void add_else(bool value);  // #else
		void add_endif();           // #endif
		void get_json_text();
	};
	CC_STACK<CIncludedFile*> included_files;

	inline CIncludedFile& GetCurrentFile();
	inline const CC_STRING& GetCurrentFileName();
	inline const size_t GetCurrentLineNumber();

	inline void PushIncludedFile(CFile *srcfile, FILE *of, size_t np, bool in_compiler_dir, size_t nh);
	inline CIncludedFile *PopIncludedFile();

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
	CFile* GetIncludedFile(sym_t preprocessor, const char *line, FILE** outf, bool& in_compiler_dir);

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


