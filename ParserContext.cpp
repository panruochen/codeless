#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/stat.h>

#include <sys/types.h>
#include <getopt.h>
#include <libgen.h>
#include <utime.h>

#include "utils.h"
#include "cc_string.h"
#include "cc_array.h"
#include "ParserContext.h"
#include "log.h"
#include "GlobalVars.h"
#include "Fol.h"

static void new_define(MemFile& mfile, const char *option)
{
	char *p, c;
	CC_STRING s;
	bool assignment = false;

	s = "#define ";
	p = (char *) option;
	skip_blanks(p);
	while( (c = *p++) != '\0') {
		if( ! assignment && c == '=') {
			s += ' ' ;
			assignment = 1;
		} else
			s += c;
	}

	mfile << s;
	if( ! assignment )
		mfile << " 1";
	mfile << '\n';
	return;
}

static void new_undef(MemFile& mfile, const char *option)
{
	mfile << "#undef ";
	mfile << option;
	mfile << '\n';
}

static CC_STRING DoQuotes(const CC_STRING& o)
{
	CC_STRING ns;
	int flags = 0;
	const char *p;
	enum {
		F_SQ   = 0x0001,
		F_DQ   = 0x0002,
		F_BS   = 0x0004,
		F_MC   = 0x0008, /* other meta chars */
	};

	for(p = o.c_str(); *p; p++) {
		switch(*p) {
		case '\'':
			flags |= F_SQ;
			break;
		case '"':
			flags |= F_DQ;
			break;
		case '\\':
			flags |= F_BS;
			break;
		case '*':
		case '(':
		case ')':
		case '{':
		case '}':
		case '<':
		case '>':
		case ';':
		case '&':
		case '|':
			flags |= F_MC;
			break;
		}
	}

	if( *(p-1) == '\\' )
		flags |= F_SQ;

	if(flags & F_SQ) {
		ns += '"';
		for(p = o.c_str(); *p; p++) {
			if(*p == '"' || *p == '\\')
				ns += '\\';
			ns += *p;
		}
		ns += '"';
	} else if(flags != 0) {
		ns += '\'';
		ns += o;
		ns += '\'';
	} else
		ns = o;
	return ns;
}

enum {
	COP_IMACROS = 324,
	COP_INCLUDE,
	COP_ISYSTEM,
	COP_NOSTDINC,
	COP_SOURCE,
	COP_IDIRAFTER,

	COP_SAVE_COMMAND_LINE,
	COP_SAVE_DEPENDENCY,
	COP_SAVE_CONDVALS,
	COP_IN_PLACE,
	COP_NO_OUTPUT,
	COP_CC,
	COP_CC_PATH,
	COP_CLEANER_MODE,
	COP_IGNORE,
	COP_VERBOSE,
	COP_TOPDIR,
	COP_HELP,
};

extern "C" {
	struct gnu_option_s {
		const char *name;
		int value;
	};
}

static const struct gnu_option_s *parse_again(const char *arg)
{
	static const struct gnu_option_s gops[] = {
		{ "-idirafter", COP_IDIRAFTER },
	};
	size_t i;
	for(i = 0; i < COUNT_OF(gops); i++) {
		const unsigned len = strlen(gops[i].name);
		if( memcmp(arg, gops[i].name, len) == 0 ) {
			optarg = (char *) arg + len;
			return &gops[i];
		}
	}
	return NULL;
}

int ParserContext::get_options(int argc, char *argv[],const char *short_options, const struct option *long_options)
{
	int retval = -1;
	int last_c = -1;
	bool no_output = false;
	this->argc = argc;
	this->argv = argv;
	optind = 1;
	levels++;

	while (1) {
		int c;

		last_optind = optind;
		/* Prevent getopt re-order the argv array */
		if(last_c == '?' && optind < argc && *argv[optind] != '-') {
			optind++;
			save_cc_args();
			last_c = 0;
			continue;
		}

		c = getopt_long_only(argc, argv, short_options, long_options, NULL);

		if (c == -1)
			break;
retry:
		switch (c) {
		case COP_IMACROS:
			imacro_files.push_back(optarg);
			save_cc_args();
			break;
		case COP_INCLUDE:
			include_files.push_back(optarg);
			save_cc_args();
			break;
		case COP_ISYSTEM:
			isystem_dirs.push_back(optarg);
			save_cc_args();
			break;
		case COP_NOSTDINC:
			no_stdinc = true;
			save_cc_args();
			break;
		case COP_IDIRAFTER:
			after_dirs.push_back(optarg);
			save_cc_args();
			break;
		case 'o':
			save_cc_args();
			break;
		case 'I':
			ujoin(i_dirs, optarg);
			save_cc_args();
			break;
		case 'D':
			new_define(predef_macros, optarg);
			save_cc_args();
			break;
		case 'U':
			new_undef(predef_macros, optarg);
			save_cc_args();
			break;

		case COP_SOURCE:
			source_files.push_back(optarg);
			save_my_args();
			break;
		case COP_SAVE_COMMAND_LINE:
			of_cl = optarg;
			save_my_args();
			break;
		case COP_SAVE_DEPENDENCY:
			if(optarg != NULL)
				of_dep = optarg;
			else
				of_dep = '\x1';
			save_my_args();
			break;
		case COP_SAVE_CONDVALS:
			of_con = optarg;
			save_my_args();
			break;
		case COP_IN_PLACE:
			if(optarg == NULL || optarg[0] == MAGIC_CHAR)
				baksuffix = MAGIC_CHAR;
			else
				baksuffix = optarg;
			save_my_args();
			break;
		case COP_NO_OUTPUT:
			no_output = true;
			break;
		case COP_CC:
			cc = optarg;
			save_my_args();
			break;
		case COP_CC_PATH:
			cc_path = optarg;
			save_my_args();
			break;

		case COP_CLEANER_MODE:
			gv_preprocess_mode = false;
			save_my_args();
			break;

		case COP_IGNORE :
			ignore_list.push_back(optarg);
			save_my_args();
			break;

		case COP_VERBOSE:
		CB_BEGIN
			LOG_VERB level;
			level = (LOG_VERB) atol(optarg);
			set_log_verbosity(level);
			save_my_args();
		CB_END
			break;

		case COP_TOPDIR:
			topdir = optarg;
			save_my_args();
			break;

		case COP_HELP:
			print_help = true;
			save_my_args();
			break;

		case ':':
		case '?':
		default:
		CB_BEGIN
			const struct gnu_option_s *gop;
			gop = parse_again(argv[last_optind]);
			if(gop) {
				c = gop->value;
				goto retry;
			}
			save_cc_args();
		CB_END
			break;
		}

		last_c = c;
	}

	if(!baksuffix.isnull()) {
		if (no_output) {
			errmsg = "Conflicting options: --yz-in-place and --yz-no-output";
			goto error;
		}
		outfile = OF_NORM;
	} else if(no_output)
		outfile = OF_NULL;

	CB_BEGIN
	const char *xe = !of_con.isnull() ? "--yz-save-condvals" :
		(!of_dep.isnull() ? "--yz-save-dep" : NULL);
	if( xe && topdir.isnull() ) {
		errmsg.format("--yz-topdir must be specified if %s exists", xe);
		goto error;
	}
	CB_END

	for(int i = optind; i < argc; i++) {
		uint8_t type;

		type = check_source_type(argv[i]);
		switch(type) {
		case SOURCE_TYPE_CPP:
			predef_macros << "#define __cplusplus\n";
			predef_macros << "#define and    &&\n";
			predef_macros << "#define or     ||\n";
			predef_macros << "#define compl  ~\n";
			predef_macros << "#define not    !\n";
			predef_macros << "#define not_eq !=\n";
			predef_macros << "#define bitand &\n";
			predef_macros << "#define bitor  |\n";
			predef_macros << "#define xor    ^\n";
		case SOURCE_TYPE_C:
		case SOURCE_TYPE_S:
			source_files.push_back(argv[i]);
		}

		if(levels == 1) {
			cc_args += ' ' ;
			cc_args += DoQuotes(argv[i]);
		}
	}
	retval = 0;

error:
	levels--;
	return retval;
}

void __NO_USE__ dump_args(int argc, char *argv[]);

int ParserContext::get_options(int argc, char *argv[])
{
	static const struct option long_options[] = {
		{"yz-save-cl",         1, 0, COP_SAVE_COMMAND_LINE },
		{"yz-save-dep",        2, 0, COP_SAVE_DEPENDENCY },
		{"yz-save-condvals",   1, 0, COP_SAVE_CONDVALS },
		{"yz-topdir",          1, 0, COP_TOPDIR },
		{"yz-source",          1, 0, COP_SOURCE },
		{"yz-in-place",        2, 0, COP_IN_PLACE },
		{"yz-no-output",       0, 0, COP_NO_OUTPUT },
		{"yz-cc",              1, 0, COP_CC },
		{"yz-cc-path",         1, 0, COP_CC_PATH },
		{"yz-cleaner-mode",    0, 0, COP_CLEANER_MODE },
		{"yz-ignore",          1, 0, COP_IGNORE },
		{"yz-verbose",         1, 0, COP_VERBOSE },
		{"yz-help",            0, 0, COP_HELP },
		{"include",  1, 0, COP_INCLUDE},
		{"imacros",  1, 0, COP_IMACROS},
		{"isystem",  1, 0, COP_ISYSTEM},
		{"nostdinc", 0, 0, COP_NOSTDINC},
		{"idirafter",1, 0, COP_IDIRAFTER},
		{0, 0, 0, 0}
	};
	int retval;
	opterr = 0;
	levels = 0;
	retval = get_options(argc, argv, "I:D:U:o:", long_options);
	return retval;
}

void ParserContext::save_cc_args()
{
	if(levels != 1)
		return;
	for(int i = last_optind; i < optind; i++) {
		cc_args += ' ';
		cc_args += DoQuotes(argv[i]);
	}
}

void ParserContext::save_my_args()
{
	if(levels != 1)
		return;
	for(int i = last_optind; i < optind; i++) {
		my_args += ' ';
		my_args += DoQuotes(argv[i]);
	}
}

bool ParserContext::check_ignore(const CC_STRING& filename)
{
	if( ignore_list.size() == 0 || filename.isnull())
		return false;
	for(size_t i = 0; i < ignore_list.size(); i++) {
		const CC_STRING& p = ignore_list[i];
		if( p.size() < filename.size() &&
			strcmp(filename.c_str() + filename.size() - p.size(), p.c_str()) == 0 )
			return true;
	}
	return false;
}

static bool is_beyond(const CC_STRING& dir, const CC_STRING& topdir)
{
	/* Assume a path which is not absolute is not beyond the top directory. This is not absolutely correct, but usually correct */
	return dir[0] == '/' && dir.find(topdir) != 0;
}

CC_STRING ParserContext::get_include_file_path(const CC_STRING& included_file, const CC_STRING& current_file,
	bool quote_include, bool include_next, bool *in_sys_dir)
{
	size_t i, count;
	CC_STRING path;
	CC_STRING curdir;
	struct stat st;

	if( included_file.size() == 0 )
		return CC_STRING("");
	if( included_file[0] == '/' )
		return included_file;
	if(!current_file.isnull()) {
		curdir = fol_dirname(current_file.c_str());
		curdir += '/';
	}
	count = 0;
	path  = curdir;
	path += included_file;
	if(quote_include && stat(path.c_str(), &st) == 0 ) {
		++count;
		if( ! include_next ) {
			if(in_sys_dir != NULL)
//				*in_sys_dir = find(compiler_dirs, curdir);
				*in_sys_dir = is_beyond(curdir, topdir);
			return path;
		}
	}

	CC_ARRAY<CC_STRING> *search_orders[] = { &i_dirs, &isystem_dirs, no_stdinc ? NULL : &compiler_dirs, &after_dirs } ;
	for(i = 0; i < COUNT_OF(search_orders); i++) {
		size_t j;
		if( ! search_orders[i] )
			continue;
		CC_ARRAY<CC_STRING>& order = *search_orders[i];
		for(j = 0; j < order.size(); j++) {
			path = order[j];
			path += '/';
			path += included_file;

			if( stat(path.c_str(), &st) == 0 ) {
				++count;
				if( ! include_next || count == 2) {
					if(in_sys_dir != NULL)
//						*in_sys_dir = v = find(compiler_dirs, order[j]);
						*in_sys_dir = is_beyond(order[j], topdir);
					return path;
				}
			}
		}
	}
	path.clear();
	return path;
}

ParserContext::ParserContext()
{
	as_lc_char = 0;
	no_stdinc  = false;
	print_help = false;
	outfile    = OF_STDOUT;
}

const char ParserContext::MAGIC_CHAR = '\x0';
