#ifndef __CPLUSPLUS_INTERFACE_H
#define __CPLUSPLUS_INTERFACE_H

#include "tcc.h"

template <class T>
class CC_ARRAY {

protected:
	void   *priv;

public:
	CC_ARRAY();
	~CC_ARRAY();

    void   erase(size_t begin, size_t end);
    void   erase(size_t pos);
    void   insert(size_t pos, const T *buf, size_t nr);
    void   insert(size_t pos, const T& x);
    void   insert(size_t pos, const CC_ARRAY& array2);
    void   push_back(const T& x);
    bool   pop_back(T& x);
    void   clear();
   
	      T& operator [](size_t pos);
    const T& operator [](size_t pos) const;

	size_t  size() const;
    T*  get_buffer(void);

	T&    front();
	T&    back();
};

template <class T1, class T2>
class CC_MAP {

protected:
	void   *priv;

public:
	CC_MAP();
	~CC_MAP();

    void   erase(size_t begin, size_t end);
    void   erase(size_t pos);
    void   insert(const T1& x1, const T2& x2);
    void   clear();
	bool   find(const T1& x1);
	bool   find(const T1& x1, T2& x2);

	size_t  size() const;
};

#include "platform.h"


typedef CC_ARRAY<TOKEN>     TOKEN_ARRAY;  /* Lexical Token Array */


class GENERIC_FILE {
	friend class ConditionalParser;
protected:
	CC_STRING  name;
	size_t     line;
public:
	virtual int  read() = 0;
	virtual bool open() = 0;
	virtual void close() = 0;
	virtual ~GENERIC_FILE() {} ;
};

class REAL_FILE : public GENERIC_FILE {
protected:
	FILE *fp;

	virtual int read()
	{
		return ::fgetc(fp);
	}

public:
	virtual bool open()
	{
		fp    = fopen(name.c_str(), "rb");
		line  = 0;
		return fp != NULL;
	}

	virtual void close()
	{
		if(fp) {
			fclose(fp);
			fp = NULL;
			name.clear();
		}
	}

	virtual ~REAL_FILE() {
		if(fp)
			fclose(fp);
	}

	REAL_FILE() {
		fp   = NULL;
	};

	void set_file(const CC_STRING& name)
	{
		this->name = name;
		if(fp != NULL) {
			fclose(fp);
			fp = NULL;
		}
	}

	bool open(const char *filename)
	{
		fp    = fopen(filename, "rb");
		name  = filename;
		line  = 0;
		return fp != NULL;
	}


	void set_fp(FILE *fp, const char *filename)
	{
		this->fp   = fp;
		this->name = filename;
		this->line = 0;
	}

	void set_fp(FILE *fp, const CC_STRING& filename)
	{
		this->fp   = fp;
		this->name = filename;
		this->line = 0;
	}
};

class MEMORY_FILE : public GENERIC_FILE {
protected:
	size_t     pos;

	virtual int read()
	{
		if(pos < contents.size())
			return contents[pos++];
		return EOF;
	}

public:
	CC_STRING contents;
	MEMORY_FILE() {
		pos  = 0;
	}
	void set_filename(const char *name) {
		this->name = name;
		this->line = 0;
	}

	virtual bool open()  { pos = 0; return true; }
	virtual void close() {}
	virtual ~MEMORY_FILE() {}
};


class ConditionalParser {
protected:
	CC_STRING cword;
	CC_STRING last_error;
	const char **keywords;
	size_t num_keywords;

	bool is_keyword(const char *cword);
	bool is_delimiter(char c);
	int common_handler(char c, TOKEN_ARRAY& token);
	
	void get_new_token(TOKEN_ARRAY& token);
	void get_operator(TOKEN_ARRAY& token, const char *opr);
	TCC_CONTEXT  *tc;

	struct CONDITIONAL {
		int8_t  if_value;
		int8_t  elif_value;
//		bool bypassing;
		int8_t  curr_value;
	};

	FILE *outdev;
	FILE *dep_fp;
	CONDITIONAL            current;
	CC_ARRAY<CONDITIONAL>  conditionals;

	CC_STRING handle_elif(int mode);
	bool  current_state_pop();
	bool  under_false_conditional();

	ssize_t   comment_start;
	bool      have_output;
	CC_ARRAY<GENERIC_FILE *> include_files;
	GENERIC_FILE *current_file;

	CC_STRING    line;
	CC_STRING    raw_line;
	
	void initialize(TCC_CONTEXT *tc, const char **keywords, size_t num_keywords, GENERIC_FILE *infile, FILE *outdev, FILE *dep_fp);
	sym_t preprocessed_line(const char *line, const char **pos);
	const char *run(sym_t id, const char *line, bool *anything_changed);

	inline void mark_comment_start() { comment_start = raw_line.size() - 2; }
	inline void enable_output()  { have_output = true; }
	inline void disable_output() { have_output = false; }

public:
	ConditionalParser() {
		tc          = NULL;
		outdev      = NULL;
		have_output = true;
	}
	const char *split(sym_t id, const char *line, TOKEN_ARRAY& tokens);
	const char *do_parse(TCC_CONTEXT *tc, const char **keywords, size_t num_keywords, GENERIC_FILE *file,
		const char *outfile, FILE *depf, bool *anything_changed);
};

typedef struct _MESSAGE_LEVEL_TYPE *MESSAGE_LEVEL;

#define   DML_RUNTIME ((MESSAGE_LEVEL) 0)
#define   DML_ERROR   ((MESSAGE_LEVEL) 1)
#define   DML_DEBUG   ((MESSAGE_LEVEL) 2)
#define   DML_INFO    ((MESSAGE_LEVEL) 3)

class BASIC_CONSOLE {
protected:
	FILE *stream;

public:
	BASIC_CONSOLE() { stream = NULL; }
	void init(FILE *stream) { this->stream = stream; }

	BASIC_CONSOLE& operator << (const char *);
	BASIC_CONSOLE& operator << (const CC_STRING&);
	BASIC_CONSOLE& operator << (const char);
	BASIC_CONSOLE& operator << (const ssize_t);
	BASIC_CONSOLE& operator << (const size_t);
};

typedef struct __TCC_MACRO_TABLE  *TCC_MACRO_TABLE;

class TCC_DEBUG_CONSOLE : public BASIC_CONSOLE {
protected:
	TCC_CONTEXT *tc;
	static const MESSAGE_LEVEL default_gate_level;
	MESSAGE_LEVEL gate_level;
	MESSAGE_LEVEL current_level;

	inline bool can_print()
	{  return (ssize_t)current_level <= (ssize_t) gate_level; }

public:
	TCC_DEBUG_CONSOLE() {
		stream = NULL;
		tc     = NULL;
		current_level = (MESSAGE_LEVEL) (100 + (size_t)DML_INFO);
		gate_level = default_gate_level;
	}
	void init(FILE *stream, TCC_CONTEXT *tc) {
		this->stream = stream;
		this->tc     = tc;
	}
	void set_gate_level(MESSAGE_LEVEL level)
	{
		if((ssize_t)level < (ssize_t)DML_RUNTIME)
			level = DML_RUNTIME;
		else if((ssize_t)level > (ssize_t)DML_INFO)
			level = DML_INFO;
		gate_level = level;
	}
	void clear_gate_level(MESSAGE_LEVEL level)
	{
		gate_level = default_gate_level;
	}

	TCC_DEBUG_CONSOLE& operator << (const char *);
	TCC_DEBUG_CONSOLE& operator << (const CC_STRING&);
	TCC_DEBUG_CONSOLE& operator << (const char);
	TCC_DEBUG_CONSOLE& operator << (const ssize_t);
	TCC_DEBUG_CONSOLE& operator << (const size_t);

	TCC_DEBUG_CONSOLE& operator << (const TOKEN_ARRAY&);
	TCC_DEBUG_CONSOLE& operator << (const CC_ARRAY<sym_t>&);
	TCC_DEBUG_CONSOLE& operator << (const TOKEN *token);
	TCC_DEBUG_CONSOLE& operator << (const TCC_MACRO_TABLE macro_table);
	TCC_DEBUG_CONSOLE  operator << (MESSAGE_LEVEL);

	TCC_DEBUG_CONSOLE& precedence_matrix_dump(void);
};

class TCC_NULL_CONSOLE {
private:
	void current_message_level_reset() {}

public:
	void init(FILE *stream, TCC_CONTEXT *tc) {}
	void set_gate_level(MESSAGE_LEVEL level) {}

#define IMPLEMENT_OPERATOR(__type) \
	TCC_NULL_CONSOLE& operator << (__type) { return *this; }

	IMPLEMENT_OPERATOR (const char *)
    IMPLEMENT_OPERATOR (const CC_STRING&)
    IMPLEMENT_OPERATOR (const char)
	IMPLEMENT_OPERATOR (const ssize_t)
	IMPLEMENT_OPERATOR (const size_t)
	IMPLEMENT_OPERATOR (const TOKEN_ARRAY&)
	IMPLEMENT_OPERATOR (const CC_ARRAY<sym_t>&)
	IMPLEMENT_OPERATOR (const TOKEN *token)
	
	TCC_NULL_CONSOLE  operator << (MESSAGE_LEVEL) { return *this; }

	TCC_NULL_CONSOLE& precedence_matrix_dump(void) { return *this; }
	TCC_NULL_CONSOLE& macro_table_dump(MESSAGE_LEVEL) { return *this;}
#undef IMPLEMENT_OPERATOR
};

#if HAVE_DEBUG_CONSOLE
typedef TCC_DEBUG_CONSOLE   DEBUG_CONSOLE;
#else
typedef TCC_NULL_CONSOLE    DEBUG_CONSOLE;
#endif

void preprocess_command_line(int argc, char *argv[], CC_ARRAY<CC_STRING> &new_options);
void handle_undef(TCC_CONTEXT *tc, const char *line);
CC_STRING macro_expand(TCC_CONTEXT *tc, const char *line, const char **errs);
const char *check_file(const char *inc_file, const char *cur_file, bool include_current_dir, bool include_next = false, bool *in_sys_dir = NULL);

bool add_ignore_pattern(const char *pattern);
bool check_file_ignored(const char *filename);

extern CC_ARRAY<CC_STRING>  dx_traced_macros, dx_traced_lines;
extern BASIC_CONSOLE runtime_console;
extern DEBUG_CONSOLE debug_console;
extern bool          rtm_preprocess;
extern bool          rtm_silent;

bool find(const CC_ARRAY<CC_STRING>& haystack, const CC_STRING& needle);
void join(CC_STRING& s, const char *start, const char *end);
void setup_host_cc_env(int argc, char *argv[]);

#include <ctype.h>
#define SKIP_WHITE_SPACES(ptr)    while( isspace(*ptr) ) ptr++

#endif

