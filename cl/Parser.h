#ifndef  __PXCC_H

#include <errno.h>
#include <string.h>

#include "precedence-matrix.h"
#include "cc_string.h"
#include "cc_array.h"
#include "Log.h"
#include "InternalTables.h"

#include "base.h"
#include "File.h"
#include "ParserContext.h"
#include "FileWriter.h"

#define INV_LN           0xFFFFEEFF

/* Tri-State values */
enum TRI_STATE {
	TSV_0 = 0,
	TSV_1 = 1,
	TSV_X = 2,
};

class Parser {
protected:
	static const char *preprocessors[];
	size_t       num_preprocessors;
	ParserContext   *rtc;
	FileWriter      *writers[VCH_MAX];

	/*-------------------------------------------------------------
	 *  class for conditional chain
	 *-----------------------------------------------------------*/
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
		const char *filename;
		size_t line;

		inline CConditionalChain();
		inline void enter_if(TRI_STATE value);
		inline void enter_elif(TRI_STATE value);
		inline TRI_STATE enter_else();
		inline bool keep_endif();
		inline bool has_true();
		inline TRI_STATE eval_condition();
	};
	CConditionalChain *upper_chain();
	CC_STACK<CConditionalChain*> conditionals;
	TRI_STATE eval_current_conditional();
	TRI_STATE superior_conditional_value(bool on_if);

	/*-------------------------------------------------------------
	 *  class for included file
	 *-----------------------------------------------------------*/
	class IncludedFile {
		typedef uint32_t linenum_t;

		/*-------------------------------------------------------------
		 *  class for doubly-linked list entry
		 *-----------------------------------------------------------*/
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
			friend class IncludedFile;
		};

		class CondChain;

		/*-------------------------------------------------------------
		 *  class for file-based conditional chain
		 *-----------------------------------------------------------*/
		class Cond {
		private:
			CListEntry  link;
			CondChain *head;
			enum COND_TYPE {
				CT_ROOT = 0,
				CT_IF   = 1,
				CT_ELIF = 2,
				CT_ELSE = 3,
			};
#if SANITY_CHECK
			char tag[4];
			static const char TAG[4];
			const char *filename;
#endif
			const COND_TYPE type;

/*
 *  A folded syntactic line could come from multiple physical lines as follows
 *
 *     Line 1 \  <-- boff
 *     Line 2 \
 *     ...
 *     Line n    <-- line_num
 */
			const linenum_t begin;
			const int boff;
			linenum_t end;
			int eoff;
			bool value;
			CListEntry sub_chains; /* subordinate chains */

		public:
			Cond(CondChain *, COND_TYPE, bool, uint32_t, int, const char *);
			void append(CondChain *cc);
			void sanity_check();

			friend class CondChain;
			friend class IncludedFile;
		};

		/*-------------------------------------------------------------
		 *  class for file-based conditional chain
		 *-----------------------------------------------------------*/
		class CondChain {
		private:
#if SANITY_CHECK
			char tag[4];
			static const char TAG[4];
#endif
			CListEntry  link;
			CListEntry  chain; /* The conditional chain */
			size_t      begin;
			size_t      end;   /* The end line number */
			Cond      *superior;

		public:
			CondChain();
			void mark_end(linenum_t);
			void add_if(Cond *);
			void add_elif(Cond*);
			void add_else(Cond*);
			void add_endif(linenum_t);
			void sanity_check();

			friend class Cond;
			friend class IncludedFile;
		};

		typedef void (*ON_CONDITIONAL_CALLBACK)(void *, Cond *, int);
		typedef void (*ON_CONDITIONAL_CHAIN_CALLBACK)(void *, CondChain *);

		/*---------------------------------------------------------------------
		 *  class for walking through all conditional chains within a file
		 *-------------------------------------------------------------------*/
		class WalkThrough {
		public:
			enum WT_METHOD {
				MED_BF, /* breadth-first */
				MED_DF, /* depth-first   */
			};
		private:
			enum WT_METHOD method;
			void *context;
			int  rh; /* Recursive hierarchy */
			ON_CONDITIONAL_CALLBACK       on_conditional_callback;
			ON_CONDITIONAL_CHAIN_CALLBACK on_conditional_chain_callback;
			void enter_conditional(Cond *c);
			void enter_conditional_chain(CondChain *cc);
			void dump_and_exit(CondChain *cc, const char *msg);
		public:
			WalkThrough(Cond*, WT_METHOD, ON_CONDITIONAL_CHAIN_CALLBACK, ON_CONDITIONAL_CALLBACK, void *);
		};

		size_t   nh; /* The hierarchy number of including */

		Cond *virtual_root;
		Cond *cursor;
		uint32_t false_level;

		static void delete_conditional(void *, Cond *, int);
		static void delete_conditional_chain(void *, CondChain *);

		static void save_conditional(void *, Cond *, int);
		void save_conditional(Cond *, int);

	public:
		File   *ifile; /* Source File */
		FILE    *ofile; /* Dest File   */
		size_t   np;    /* The number of preporcessors */
		bool     in_compiler_dir; /* The included file is inside the compiler root directory. */
		CC_STRING cr_text;

		IncludedFile(File *s, FILE *of, size_t np, bool in_compiler_dir, size_t nh);
		~IncludedFile();
		void add_if(bool value);    // #if
		void add_elif(bool value);  // #elif
		void add_else(bool value);  // #else
		void add_endif();           // #endif
		void produce_cr_text();
	};
	CC_STACK<IncludedFile*> included_files;

	inline IncludedFile& GetCurrentFile();
	inline const CC_STRING& GetCurrentFileName();
	inline const size_t GetCurrentLineNumber();

	inline void PushIncludedFile(File *srcfile, FILE *of, size_t np, bool in_compiler_dir, size_t nh);
	inline IncludedFile *PopIncludedFile();

	/*-------------------------------------------------------------*/
	InternalTables   *intab;

	CC_STRING      deptext;
	CC_STRING errmsg;

	struct ParsedLine {
		ssize_t comment_start;
		sym_t   pp_id;
		CC_STRING parsed; /* The comment-stripped line */
		CC_STRING from;   /* The original input line */
		CC_STRING to;     /* The processed output line */
		int content;      /* The offset of the preprocessing contents */
	};
	ParsedLine pline;

	CC_STRING  do_elif(int mode);
	bool       do_define(const char *line);
	bool       do_include(sym_t preprocess, const char *line, const char **output);

	void   Reset(InternalTables *intab, size_t num_preprocessors, ParserContext *ctx);
	sym_t  GetPreprocessor(const char *line, const char **pos);
	int    ReadLine();
	bool   SM_Run();
	void   AddDependency(const char *prefix, const CC_STRING& filename);
	File* GetIncludedFile(sym_t preprocessor, const char *line, FILE** outf, bool& in_compiler_dir);

	bool GetCmdLineIncludeFiles(const CC_ARRAY<CC_STRING>& ifiles, size_t np);
	bool RunEngine(size_t n);

	inline bool has_dep_file();
	bool check_file_processed(const CC_STRING& filename);

	inline void mark_comment_start();

	void SaveDepInfo(const CC_STRING& s);
	void SaveCondValInfo(const CC_STRING& s);

	const char *TransToken(SynToken *tokp);
	int Compute(const char *line);
	int CheckSymbolDefined(const char *line, bool reverse, SynToken *result);
	bool CalculateOnStackTop(sym_t opr, CC_STACK<SynToken>& opnd_stack, CC_STACK<sym_t>& opr_stack);
	bool DoCalculate(SynToken& opnd1, sym_t opr, SynToken& opnd2, SynToken& result);

public:
	inline Parser();
	inline const char *GetError();
	bool DoFile(InternalTables *intab, size_t num_preprocessors, File  *infile, ParserContext *cp_ctx);
};


#include "Parser.inl"

#endif


