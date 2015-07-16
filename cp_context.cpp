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
#include "ycpp.h"

static void new_define(CMemFile& mfile, const char *option)
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


static void new_undef(CMemFile& mfile, const char *option)
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
	C_OPTION_IMACROS = 324,
	C_OPTION_INCLUDE,
	C_OPTION_ISYSTEM,
	C_OPTION_NOSTDINC,
	C_OPTION_SOURCE,

	C_OPTION_SAVE_COMMAND_LINE,
	C_OPTION_SAVE_BYPASS,
	C_OPTION_SAVE_DEPENDENCY,
	C_OPTION_IN_PLACE,
	C_OPTION_CC,
	C_OPTION_CC_PATH,
	C_OPTION_STRICT_MODE,
	C_OPTION_BYPASS,
	C_OPTION_IMPORT_BYPASS,
	C_OPTION_VERBOSE
};


int CP_CONTEXT::get_options(int argc, char *argv[], const char *short_options, const struct option *long_options)
{
	int retval = -1;
	int last_c = -1;
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
		switch (c) {
		case C_OPTION_IMACROS:
			imacro_files.push_back(optarg);
			save_cc_args();
			break;
		case C_OPTION_INCLUDE:
			include_files.push_back(optarg);
			save_cc_args();
			break;
		case C_OPTION_ISYSTEM:
			isystem_dirs.push_back(optarg);
			save_cc_args();
			break;
		case C_OPTION_NOSTDINC:
			nostdinc = true;
			save_cc_args();
			break;
		case 'I':
			ujoin(search_dirs, optarg);
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

		case C_OPTION_SOURCE:
			source_files.push_back(optarg);
			break;
		case C_OPTION_SAVE_COMMAND_LINE:
			save_clfile = optarg;
			save_my_args();
			break;
		case C_OPTION_SAVE_BYPASS:
			save_byfile = optarg;
			save_my_args();
			break;
		case C_OPTION_SAVE_DEPENDENCY:
			if(optarg != NULL)
				save_depfile = optarg;
			else
				save_depfile = '\x1';
			save_my_args();
			break;
		case C_OPTION_IN_PLACE:
			if(optarg != NULL)
				baksuffix = optarg;
			else
				baksuffix = '\0';
			save_my_args();
			break;
		case C_OPTION_CC:
			cc = optarg;
			save_my_args();
			break;
		case C_OPTION_CC_PATH:
			cc_path = optarg;
			save_my_args();
			break;

		case C_OPTION_STRICT_MODE:
			gv_strict_mode = true;
			save_my_args();
			break;

		case C_OPTION_BYPASS :
			bypass_list.push_back(optarg);
			save_my_args();
			break;
		case C_OPTION_IMPORT_BYPASS:
		CB_BEGIN
			FILE *fp;
			char buf[2048];
			fp = fopen(optarg, "r");
			if(fp == NULL)
				goto error;
			while( fgets(buf, sizeof(buf), fp) != NULL ) {
				char *p = buf + strlen(buf) - 1;
				while(p >= buf && (*p == '\r' || *p == '\n') )
					p--;
				* ++p = '\0';
				bypass_list.push_back(buf);
			}
			fclose(fp);
		CB_END
			break;

		case C_OPTION_VERBOSE:
		CB_BEGIN
			LOG_VERB level;
			level = (LOG_VERB) atol(optarg);
			set_log_verbosity(level);
			save_my_args();
		CB_END
			break;

		case ':':
		case '?':
		default:
			save_cc_args();
			break;
		}
		last_c = c;
	}

	for(int i = optind; i < argc; i++) {
		uint8_t type;

		type = check_source_type(argv[i]);
		switch(type) {
		case SOURCE_TYPE_CPP:
//			new_define(predef_macros, "__cplusplus");
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

int CP_CONTEXT::get_options(int argc, char *argv[])
{
	static const struct option long_options[] = {
		{"yz-save-command-line",  1, 0, C_OPTION_SAVE_COMMAND_LINE },
		{"yz-save-bypass",        1, 0, C_OPTION_SAVE_BYPASS },
		{"yz-save-dependency",    2, 0, C_OPTION_SAVE_DEPENDENCY },
		{"yz-in-place",           2, 0, C_OPTION_IN_PLACE },
		{"yz-cc",                 1, 0, C_OPTION_CC },
		{"yz-cc-path",            1, 0, C_OPTION_CC_PATH },
		{"yz-strict-mode",        1, 0, C_OPTION_STRICT_MODE },
		{"yz-bypass",             1, 0, C_OPTION_BYPASS },
		{"yz-import-bypass",      1, 0, C_OPTION_IMPORT_BYPASS },
		{"yz-verbose",            1, 0, C_OPTION_VERBOSE },
		{"include",  1, 0, C_OPTION_INCLUDE},
		{"imacros",  1, 0, C_OPTION_IMACROS},
		{"isystem",  1, 0, C_OPTION_ISYSTEM},
		{"nostdinc", 0, 0, C_OPTION_NOSTDINC},
		{0, 0, 0, 0}
	};
	int retval;
	opterr = 0;
	levels = 0;
	nostdinc = false;
	retval = get_options(argc, argv, "I:D:U:", long_options);
	return retval;
}

void CP_CONTEXT::save_cc_args()
{
	if(levels != 1)
		return;
	for(int i = last_optind; i < optind; i++) {
		cc_args += ' ';
		cc_args += DoQuotes(argv[i]);
	}
}

void CP_CONTEXT::save_my_args()
{
	if(levels != 1)
		return;
	for(int i = last_optind; i < optind; i++) {
		my_args += ' ';
		my_args += DoQuotes(argv[i]);
	}
}

bool CP_CONTEXT::check_if_bypass(const CC_STRING& filename)
{
	if( bypass_list.size() == 0 || filename.isnull())
		return false;
	for(size_t i = 0; i < bypass_list.size(); i++) {
		const CC_STRING& p = bypass_list[i];
		if( p.size() < filename.size() &&
			strcmp(filename.c_str() + filename.size() - p.size(), p.c_str()) == 0 )
			return true;
	}
	return false;
}

/*
-I directory
    Change the algorithm for searching for headers whose names are not absolute pathnames to look
	in the directory named by the directory pathname before looking in the usual places. Thus,
	headers whose names are enclosed in double-quotes ( "" ) shall be searched for first in the directory
	of the file with the #include line, then in directories named in -I options, and last in the usual places.
	For headers whose names are enclosed in angle brackets ( "<>" ), the header shall be searched for only
	in directories named in -I options and then in the usual places. Directories named in -I options shall
	be searched in the order specified. Implementations shall support at least ten instances of this option
	in a single c99 command invocation.
*/

CC_STRING CP_CONTEXT::get_include_file_path(const CC_STRING& included_file, const CC_STRING& current_file,
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
				*in_sys_dir = find(compiler_search_dirs, curdir);
			return path;
		}
	}

	CC_ARRAY<CC_STRING>& dirs = search_dirs;
	for(i = 0; i < dirs.size(); i++) {
		path = dirs[i];
		path += '/';
		path += included_file;

		if( stat(path.c_str(), &st) == 0 ) {
			++count;
			if( ! include_next || count == 2) {
				if(in_sys_dir != NULL)
					*in_sys_dir = find(compiler_search_dirs, dirs[i]);
				return path;
			}
		}
	}
	path.clear();
	return path;
}

