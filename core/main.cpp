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

#include "c++-if.h"

#include "tcc.h"
#include "precedence-matrix.h"


#define BLOCK_START   do {
#define BLOCK_END     } while(0);

#define PROGRAM_NAME  "pxcc"

static int precedence_matrix_setup(TCC_CONTEXT *tc, size_t oprnum, const char **oprset, const char **matrix)
{
	size_t i;
	int rc = 0;

	if( (tc->symtab = symtab_create()) == NULL )
		goto fail;
	if( (tc->matrix = oprmx_create()) == NULL ) 
		goto fail;

	symtab_init(tc->symtab, oprnum, oprset);
	oprmx_init(tc->matrix);
	for(i = 0; i < oprnum * oprnum; i++) {
		char opr1[16], opr2[16];
		int result, id1, id2;
		char prec;
		sscanf(matrix[i],"%s %s %c", opr1, opr2, &prec);
		switch(prec) {
		case '=': result = OP_EQUAL; break;
		case '<': result = OP_LOWER; break;
		case '>': result = OP_HIGHER; break;
		case 'X': result = OP_ERROR; break;
		}
		id1 = symbol_to_id(tc->symtab, opr1);
		id2 = symbol_to_id(tc->symtab, opr2);
		oprmx_add(tc->matrix, id1, id2, result);
//		fprintf(stderr, "(%s %s %c)\n", opr1, opr2, prec);
	}
	for(i = 0; i < COUNT_OF(reserved_symbols); i++)
		symtab_insert(tc->symtab, reserved_symbols[i]);

fail:
	if(rc < 0) {
		if(tc->matrix != NULL)    oprmx_destroy(tc->matrix);
		if(tc->symtab  != NULL)   symtab_destroy(tc->symtab);
	}
	return rc;
}

int operator_compare(TCC_CONTEXT *tc, const char *op1, const char *op2)
{
	int id1, id2;

	id1 = symbol_to_id(tc->symtab, op1);
	if(id1 < 0)
		return -1;
	id2 = symbol_to_id(tc->symtab, op2);
	if(id2 < 0)
		return -1;
	return oprmx_compare(tc->symtab, id1, id2);
}

static CC_ARRAY<CC_STRING>  std_search_dirs[2]; /* 0 -- #include <file>
												 * 1 -- #include "file"
												 **/
static CC_ARRAY<CC_STRING>  tcc_sys_search_dirs;

bool find(const CC_ARRAY<CC_STRING>& haystack, const CC_STRING& needle)
{
	size_t i;

	for(i = 0; i < haystack.size(); i++) {
		if( haystack[i] == needle )
			return true;
	}
	return false;
}


static void add_to_search_dir(CC_ARRAY<CC_STRING>& search_dirs, const char *dir)
{
	size_t i;
	for(i = 0; i < search_dirs.size(); i++) {
		if( search_dirs[i] == dir )
			return;
	}
	search_dirs.push_back(dir);
}


static void join(CC_ARRAY<CC_STRING>& x, const CC_ARRAY<CC_STRING>& y)
{
	size_t i;
	for(i = 0; i < y.size(); i++) 
		x.push_back(y[i]);
}


static void new_define(CC_STRING& total, const char *option)
{
	char *p, c;
	CC_STRING s;
	bool assignment = false;

	s = "#define ";
	p = (char *) option;
	SKIP_WHITE_SPACES(p);
	while( (c = *p++) != '\0') {
		if( ! assignment && c == '=') {
			s += ' ' ;
			assignment = 1;
		} else
			s += c;
	}

	total += s;
	if( ! assignment )
		total += " 1";
	total += '\n';
	return;

error:
	return;
}

static void new_undef(CC_STRING& total, const char *option)
{
	total += "#undef ";
	total += option;
	total += '\n';
}

static CC_STRING xdirname(const char *path)
{
	char *tmp;
	CC_STRING dir;

	tmp = strdup(path);
	dir = dirname(tmp);
	free(tmp);
	return dir;
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

/*
Table 2.2. Include file search paths
Compiler option	<include> search order	"include" search order

Neither -I nor -J
	RVCT31INCdirs                     	CP, RVCT31INCdirs

-I
	RVCT31INCdirs, Idirs              	CP, Idirs, RVCT31INCdirs

-J
	Jdirs                              	CP, and Jdirs

Both -I and -J
	Jdirs, Idirs                       	CP, Idirs, Jdirs

--sys_include
	No effect                           Removes CP from the search path

--kandr_include
	No effect                       	Uses Kernighan and Ritchie search rules
 */
const char *check_file(const char *filename, const char *cur_file, bool quote_include,
	bool include_next, bool *in_sys_dir)
{
	size_t i, count;
	static CC_STRING path;
	char *tmp;
	CC_STRING curdir;
	struct stat st;

	if(cur_file != NULL) {
		curdir = xdirname(cur_file);
	} else
		curdir = ".";
	count = 0;
	path  = curdir;
	path += '/';
	path += filename;
	if(quote_include && stat(path.c_str(), &st) == 0 ) {
		++count;
		if( ! include_next ) {
			if(in_sys_dir != NULL)
				*in_sys_dir = find(tcc_sys_search_dirs, curdir);
			return path.c_str();
		}
	}
	
	{
		CC_ARRAY<CC_STRING>& dirs = std_search_dirs[!!quote_include];
		for(i = 0; i < dirs.size(); i++) {
			path = dirs[i];
			path += '/';
			path += filename;
	
			if( stat(path.c_str(), &st) == 0 ) {
				++count;
				if( ! include_next || count == 2) {
					if(in_sys_dir != NULL)
						*in_sys_dir = find(tcc_sys_search_dirs, dirs[i]);
					return path.c_str();
				}
			}
		}
	}
	return NULL;
}

const void show_search_dirs()
{
	size_t i, n;
	FILE *dev = stderr;

	fprintf(dev, "Search directories:\n");
	for(i = 0; i < std_search_dirs[0].size(); i++)
		fprintf(dev, " %s\n", std_search_dirs[0][i].c_str());
	fprintf(dev, "System search directories:\n");
	for(i = 0; i < tcc_sys_search_dirs.size(); i++)
		fprintf(dev, " %s\n", tcc_sys_search_dirs[i].c_str());
	fprintf(dev, "\n");
}


static int tcc_init(TCC_CONTEXT *tc)
{
	if( precedence_matrix_setup(tc, COUNT_OF(g1_oprset), g1_oprset, g1_oprmx) < 0) {
		runtime_console << "Cannot build precedence matrix\n";
		return  -1;
	}
	tc->mtab = macro_table_create();
	if(tc->mtab == NULL)
		return -ENOMEM;
	return 0;
}

static void show_usage_and_exit(int exit_code)
{
	const char *help =
"NAME\n"
"       "PROGRAM_NAME" - Pan's  C/C++ souce code CleanTool\n"
"\n"
"SYNOPSIS\n"
"       "PROGRAM_NAME" [OPTION]... [FILE]\n"
"\n"
"DESCRIPTION\n"
"       Parse and remove conditional compilation blocks from C/C++ source files.\n"
"\n"
"  OPTIONS INHERITED FROM COMMAND LINE\n\n"
"       -imacro FILE\n"
"              Obtain explicit macro definitions from FILE\n"
"\n"
"       -I DIR\n"
"              Search DIR for header files\n"
"\n"
"       -D name\n"
"              Predefine name as a macro, with definition 1.\n"
"\n"
"       -D name=definition\n"
"              Predefine name as a macro, with definition.\n"
"\n"
"       -U name\n"
"              Cancel any previous definition of name.\n"
"\n"
"  OPTIONS FOR SELF\n\n"
"       -y-in-place [SUFFIX]\n"
"              Overwrite the input files instead of outputting to STDOUT\n"
"              Back up if SUFFIX is specified\n"
"\n"
#if HAVE_DEBUG_CONSOLE
"       -y-debug LEVEL\n"
"              Set debug message level\n"
"\n"
#endif
"       -y-preprocess\n"
"              Force to work in preprocessor mode. In this mode,\n"
"               1. Include files are fetched and handled\n"
"               2. Undefined macros are regarded as blanks during evaluation\n"
"               3. Exit upon any error\n"
"\n"
"       -y-I DIR\n"
"              Search DIR for header files only for `#include <>'\n"
"\n"
"       -y-get-macros=definitions\n"
"              Predefine macros with #define directive. Multiple definitions are seperated by new-lines.\n"
"\n"
"       -y-print-dependency=FILE\n"
"              Print path names of dependent header files to FILE, excluding those in system search directories.\n"
"\n"
"       -y-source=FILE\n"
"              Force FILE to be parsed.\n"
"\n"
"Report " PROGRAM_NAME " bugs to <coderelease@163.com>"
;
	runtime_console << help;
	exit(exit_code);
}

BASIC_CONSOLE  runtime_console;
DEBUG_CONSOLE  debug_console;

enum {
	SOURCE_TYPE_ERR = 0,
	SOURCE_TYPE_C   = 1,
	SOURCE_TYPE_CPP = 2,
};

static uint8_t check_ext(const char *filename)
{
	const char *p = filename + strlen(filename);
	while((--p) >= filename) {
		if(*p == '.') {
			p++;
			if( strcasecmp(p, "c") == 0 ) {
				return SOURCE_TYPE_C;
			} else if( strcasecmp(p, "cc") == 0 || strcasecmp(p, "cxx") == 0 || strcasecmp(p, "cpp") == 0 ) {
				return SOURCE_TYPE_CPP;
			}
			return SOURCE_TYPE_ERR;
		}
	}
	return SOURCE_TYPE_ERR;
}

static CC_STRING get_dep_filename(const char *filename)
{
	CC_STRING s;
	const char *p = filename + strlen(filename);
	while((--p) >= filename) {
		if(*p == '.') {
			p++;
			s.insert(s.begin(), filename, p );
			s += "xdd";
			break;
		}
	}
	return s;
}

static bool get_utb(const char *path, struct utimbuf *times)
{
	struct stat stbuf;

	if( stat(path, &stbuf) != 0 )
		return false;

	times->actime = stbuf.st_atime;
	times->modtime = stbuf.st_mtime;
	return true;
}


static void save_command_line(const char *filename, int argc, char *argv[])
{
	int i;
	FILE *fp;
	char cwd[260];

	fp = fopen(filename, "ab+");
	getcwd(cwd, sizeof(cwd));
	fprintf(fp, "cd %s\n", cwd);
	for(i = 0; i < argc; i++)
		fprintf(fp, "%s ", argv[i]);
	fputc('\n', fp);
	fputc('\n', fp);
	fputc('\n', fp);
	fclose(fp);
}


int main(int argc, char *argv[])
{
	TCC_CONTEXT tcc_context, *tc = &tcc_context;
	CC_ARRAY<CC_STRING> imacro_files;
	CC_ARRAY<CC_STRING> source_files;
	const char *suffix = NULL;
	CC_STRING outfile = "/dev/stdout";
	const char *tmpfile = "/dev/stdout";
	int i;
	MEMORY_FILE cl_info;
	const char *open_depfile_mode = NULL;
	CC_STRING dep_file;
	const char *errmsg;
	CC_ARRAY<CC_STRING>  new_options;
	int new_argc;
	char **new_argv;

//	save_cl_options(argc, argv);

	runtime_console.init(stderr);
	debug_console.init(stderr, tc);

BLOCK_START
	enum {
		C_OPTION_DEBUG     = 311,
		C_OPTION_CC,
		C_OPTION_TRACE_MACRO,
		C_OPTION_TRACE_LINE,
		C_OPTION_IGNORE,
		C_OPTION_IMACROS,
		C_OPTION_IN_PLACE,
		C_OPTION_PREPROCESS,
		C_OPTION_INCDIR,
		C_OPTION_GET_MACROS,
		C_OPTION_PRINT_DEPENDENCY,
		C_OPTION_PRINT_COMMAND_LINE,
		C_OPTION_SOURCE,
		C_OPTION_OUTPUT,
		C_OPTION_VIA,
		C_OPTION_SILENT,
	};
	CC_ARRAY<CC_STRING>  search_dirs_I, search_dirs_J, search_dirs_quote;
	CC_STRING warning;

	preprocess_command_line(argc, argv, new_options);

	new_argc = new_options.size();
	new_argv = (char **) malloc(sizeof(char*) * new_argc);
	for(i = 0; i < new_argc; i++)
		new_argv[i] = (char *) new_options[i].c_str();

#if 0
	for(i = 0; i < argc; i++)
		printf("%s ", argv[i]);
	printf("\n");
	for(i = 0; i < new_argc; i++)
		printf("%s ", new_argv[i]);
	printf("\n");
	exit(0);
#endif

	opterr = 0;
	while (1) {
		int c;
		int option_index = 0;
		static struct option long_options[] = {
			{"include",  1, 0, 0},
			{"imacros",  1, 0, C_OPTION_IMACROS},
			{"isysroot", 1, 0, 0},
/*--------------------------------------------------------------------*/
			{"y-debug",    1, 0, C_OPTION_DEBUG},
			{"y-trace-macro", 1, 0, C_OPTION_TRACE_MACRO },
			{"y-trace-line", 1, 0, C_OPTION_TRACE_LINE },
			{"y-ignore", 1, 0, C_OPTION_IGNORE },
			{"y-in-place", 2, 0, C_OPTION_IN_PLACE},
			{"y-preprocess", 0, 0, C_OPTION_PREPROCESS},
			{"y-silent", 0, 0, C_OPTION_SILENT},
			{"y-I", 1, 0, C_OPTION_INCDIR},
			{"y-get-macros", 1, 0, C_OPTION_GET_MACROS},
			{"y-print-dependency", 2, 0, C_OPTION_PRINT_DEPENDENCY},
			{"y-print-command-line", 1, 0, C_OPTION_PRINT_COMMAND_LINE},
			{"y-source", 1, 0, C_OPTION_SOURCE},
			{"y-output", 1, 0, C_OPTION_OUTPUT},
			{0, 0, 0, 0}
		};

		c = getopt_long_only(new_argc, new_argv, "I:D:U:J:", long_options, &option_index);
		if (c == -1)
			break;
		switch (c) {
		case 0:
			warning = "option ";
			warning += long_options[option_index].name;
			if (optarg) {
				warning += " with arg ";
				warning += optarg;
			}
			warning += "\n";
            break;

		case C_OPTION_PREPROCESS:
			rtm_preprocess = true;
			break;
		case C_OPTION_DEBUG:
			{
				MESSAGE_LEVEL level;
				level = (MESSAGE_LEVEL) atol(optarg);
				debug_console.set_gate_level(level);
			}
			break;
		case C_OPTION_TRACE_MACRO:
			dx_traced_macros.push_back(optarg);
			break;
		case C_OPTION_TRACE_LINE:
			dx_traced_lines.push_back(optarg);
			break;
		case C_OPTION_IGNORE:
			add_ignore_pattern(optarg);
			break;
		case C_OPTION_IMACROS:
			imacro_files.push_back(optarg);
			break;
		case C_OPTION_IN_PLACE:
			suffix = optarg;
			if(suffix == NULL)
				suffix = "";
			break;
		case C_OPTION_OUTPUT:
			break;
		case C_OPTION_INCDIR:
			add_to_search_dir(search_dirs_I, optarg);
			add_to_search_dir(tcc_sys_search_dirs, optarg);
			break;
		case C_OPTION_GET_MACROS:
			cl_info.contents += optarg;
			cl_info.contents += '\n';
//			runtime_console << "Predefines:\n" << cl_info.contents << "\n\n";
			break;
		case C_OPTION_PRINT_DEPENDENCY:
			if(optarg != NULL) {
				dep_file = optarg;
				open_depfile_mode = "ab+";
			} else
				open_depfile_mode = "wb";
			break;
		case C_OPTION_PRINT_COMMAND_LINE:
			save_command_line(optarg, argc, argv);
			break;
		
		case 'I':
			add_to_search_dir(search_dirs_I, optarg);
			break;
		case 'D':
			new_define(cl_info.contents, optarg);
			break;
		case 'U':
			new_undef(cl_info.contents, optarg);
			break;
		case 'J':
			add_to_search_dir(search_dirs_J, optarg);
			break;
		case C_OPTION_SOURCE:
			source_files.push_back(optarg);
			break;
		case C_OPTION_SILENT:
			rtm_silent = true;
			break;

		case ':':
		case '?':
		default:
			;
		}
	}

	// #include <file>
	join(std_search_dirs[0], search_dirs_J);
	join(std_search_dirs[0], search_dirs_I);

	// #include "file"
	join(std_search_dirs[1], search_dirs_I);
	join(std_search_dirs[1], search_dirs_J);

BLOCK_END

	tcc_init(tc);
	ConditionalParser parser;
	static const char *keywords[] = {
		"#define", "#undef", "#if", "#ifdef", "#ifndef", "#elif", "#else", "#endif", "#include", "#include_next"
	};
	
	for(i = optind; i < new_argc; ) {
		const char *current_file = new_argv[i++];
		uint8_t type;

		/* Pick up one source filename from lots of unrecognized options 
		 * by checking the extension name.
		 */
		type = check_ext(current_file);
		if( type == SOURCE_TYPE_ERR )
			continue;
		else if( type == SOURCE_TYPE_CPP ) {
			new_define(cl_info.contents, "__cplusplus");
		}
		source_files.push_back(current_file);
	}

	if( source_files.size() == 0 ) 
		exit(0);
	if( source_files.size() > 1 ) {
		if( check_ext(argv[i]) ) {
			runtime_console << "Multiple input files" << '\n';
			return -2;
		}
	}

	if(cl_info.contents.size() > 0) {
		cl_info.set_filename("<command line>");
		errmsg = parser.do_parse(tc, keywords, 2, &cl_info, NULL, NULL, NULL);
		if(errmsg != NULL) {
			runtime_console << "Error on parsing command line\n" ;
			return -2;
		}
	}

	REAL_FILE file;
	if(imacro_files.size() > 0)
		for(size_t i = 0; i < imacro_files.size(); i++) {
			const char *path;
	
			path = check_file(imacro_files[i].c_str(), NULL, true);
			if( path == NULL || ! file.open(path) )
				continue;
			parser.do_parse(tc, keywords, 2, &file, NULL, NULL, NULL);
			file.close();
		}

	for(i = 0; i < 1; i++) {
		char tmpbuf[256];
		bool file_changed;
		const char *current_file = source_files[i].c_str();
		FILE *depf = NULL;
		struct utimbuf utb;

#if 0
		if ( strstr(current_file, "config/version.c") ) {
			for(i = 0; i < argc; i++)
				runtime_console << argv[i] << ' ';
			runtime_console << '\n';
			exit(2);
		}
#endif

		if( check_file_ignored(current_file) ) {
			debug_console << current_file << " is ignored!\n";
			continue;
		}

		if( strstr(current_file, "xmltok.c") != NULL )
			save_command_line("cl-options.txt", argc, argv);

		if(suffix != NULL) {
			strcpy(tmpbuf, PROGRAM_NAME "XXXXXX");
			if( mktemp(tmpbuf) == NULL ) {
				runtime_console << "Cannot create temporary files" << '\n' ;
				return -EIO;
			}
			tmpfile = tmpbuf;
			outfile = current_file;
			outfile += suffix;
		}

		if( open_depfile_mode )  {
			if( dep_file.size() == 0 )
				dep_file = get_dep_filename(current_file);
			depf = fopen(dep_file.c_str(), open_depfile_mode);
			if(depf == NULL) {
				snprintf(tmpbuf, sizeof(tmpbuf), "Cannot write to %s", dep_file.c_str());
				errmsg = tmpbuf;
				goto error;
			}
		}

		file.set_file(current_file);
		if( ! get_utb(current_file, &utb) )
			continue;
		errmsg = parser.do_parse(tc, keywords, COUNT_OF(keywords), &file, tmpfile, depf, &file_changed);
		if(errmsg == NULL) {
			if(suffix != NULL && file_changed) {
				if(suffix[0] != '\0')
					rename(current_file, outfile.c_str());
				rename(tmpfile, current_file);
				utime(current_file, &utb);
				runtime_console << "Success on handling " << current_file << '\n';
			}
		} else {
error:
			runtime_console << "Error on handling " << current_file << '\n';
			show_search_dirs();
			remove(tmpfile);
			return 2;
		}
		break;
	}
	return 0;
}


bool rtm_preprocess = false;
bool rtm_silent = false;
CC_ARRAY<CC_STRING>  dx_traced_macros;
CC_ARRAY<CC_STRING>  dx_traced_lines;
