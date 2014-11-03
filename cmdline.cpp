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


void new_define(CMemFile& mfile, const char *option)
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



int CP_CONTEXT::via_file_read(const char *filename, char ***pargv)
{
	const int N = 64;
	int argc, n;
	char **argv;
	FILE *fp = fopen(filename, "r");
	if(fp == NULL)
		return -errno;
	n    = 0;
	argc = 1;
	argv = NULL;
	while(1) {
		char buf[2048];
		if( fscanf(fp, "%s", buf) == EOF )
			break;
		if(argc >= n) {
			n += N;
			argv = (char **) realloc(argv, sizeof(char *) * n);
		}
		argv[argc++] = strdup(buf);
	}
	fclose(fp);
	*pargv = argv;
	return argc;
}


enum {
	C_OPTION_IMACROS = 324,
	C_OPTION_INCLUDE,
	C_OPTION_ISYSTEM,
	C_OPTION_NOSTDINC,
	C_OPTION_SOURCE,
	C_OPTION_VIA,

	C_OPTION_PRINT_COMMAND_LINE,
	C_OPTION_PRINT_DEPENDENCY,
	C_OPTION_IN_PLACE,
	C_OPTION_CC,
	C_OPTION_CC_PATH,
	C_OPTION_STRICT_MODE,
	C_OPTION_DEBUG
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
			ujoin(search_dirs_I, optarg);
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
		case 'J':
			ujoin(search_dirs_J, optarg);
			save_cc_args();
			break;
		case C_OPTION_SOURCE:
			source_files.push_back(optarg);
			break;
		case C_OPTION_VIA:
			do {
				int i, argc2, saved_optind, saved_argc;
				char **argv2, **saved_argv;
				
				argc2 = via_file_read(optarg, &argv2);
				if(argc2 < 0) {
					retval = argc2;
					goto done;
				}
				saved_optind = optind;
				saved_argc   = this->argc;
				saved_argv   = this->argv;
				get_options(argc2, argv2, short_options, long_options);
				optind       = saved_optind;
				this->argc   = saved_argc;
				this->argv   = saved_argv;
				for(i = 1; i < argc2; i++)
					free(argv2[i]);
				free(argv2);
				save_cc_args();
			} while(0);
		  break;

		case C_OPTION_PRINT_COMMAND_LINE:
			clfile = optarg;
			save_my_args();
			break;
		case C_OPTION_PRINT_DEPENDENCY:
			if(optarg != NULL)
				save_dep_file = optarg;
			else
				save_dep_file = '\0';
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
		
		case C_OPTION_DEBUG:
			do {
				int level;
				level = atol(optarg);
				((MSG_LEVEL)level);
				save_my_args();
			} while(0);
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

done:
	levels--;
	return retval;
}

void __NO_USE__ dump_args(int argc, char *argv[]);

int CP_CONTEXT::get_options(int argc, char *argv[])
{
	static const struct option long_options[] = {
		{"yz-print-command-line", 1, 0, C_OPTION_PRINT_COMMAND_LINE },
		{"yz-print-dependency",   2, 0, C_OPTION_PRINT_DEPENDENCY },
		{"yz-in-place",           2, 0, C_OPTION_IN_PLACE },
		{"yz-cc",                 1, 0, C_OPTION_CC },
		{"yz-cc-path",            1, 0, C_OPTION_CC_PATH },
		{"yz-strict-mode",        1, 0, C_OPTION_STRICT_MODE },
		{"yz-debug",              1, 0, C_OPTION_DEBUG },
		{"via",      1, 0, C_OPTION_VIA},
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
	retval = get_options(argc, argv, "I:D:U:J:", long_options);
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
