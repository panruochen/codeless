#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/stat.h>

#include <sys/types.h>
#include <getopt.h>

#include "c++-if.h"

#include "tcc.h"
#include "precedence-matrix.h"

#define PROGRAM_NAME  "pxcc"


static int precedence_matrix_setup(TCC_CONTEXT *tc, int oprnum, const char **oprset, const char **matrix)
{
	int i;
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

static CC_ARRAY<CC_STRING>  __tcc_search_dirs[2];
static CC_ARRAY<CC_STRING>  *tcc_search_dirs[2][2];

static void add_to_search_dir(CC_ARRAY<CC_STRING>& search_dirs, const char *dir)
{
	size_t i;

	for(i = 0; i < search_dirs.size(); i++) {
		if( search_dirs[i] == dir )
			return;
	}
	search_dirs.push_back(dir);
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
const char *check_file(const char *filename, bool include_current_dir, bool include_next, bool *in_sys_dir)
{
	size_t i, n, count;
	static CC_STRING path;
	struct stat st;

	count = 0;
	if(include_current_dir && stat(filename, &st) == 0 ) {
		path = filename;
		++count;
		if( ! include_next )
			return path.c_str();
	}
	for(n = 0; n < 2; n++) {
		CC_ARRAY<CC_STRING>& dirs = *tcc_search_dirs[!!include_current_dir][n];
		for(i = 0; i < dirs.size(); i++) {
			path = dirs[i];
			path += '/';
			path += filename;
	
			if( stat(path.c_str(), &st) == 0 ) {
				++count;
				if( ! include_next || count == 2) {
					if(in_sys_dir != NULL)
						*in_sys_dir = (&dirs - __tcc_search_dirs);

					return path.c_str();
				}
			}
		}
	}
	return NULL;
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

static bool check_ext(const char *filename)
{
	const char *p = filename + strlen(filename);
	while((--p) >= filename) {
		if(*p == '.') {
			p++;
			if( strcasecmp(p, "c") == 0 || strcasecmp(p, "cc") == 0 ||
				strcasecmp(p, "cxx") == 0 || strcasecmp(p, "cpp") == 0 ) {
				return true;
			}
			return false;
		}
	}
	return false;
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


bool preprocess_mode = false;
int main(int argc, char *argv[])
{
	enum {
		C_OPTION_DEBUG     = 311,
		C_OPTION_IMACROS,
		C_OPTION_IN_PLACE,
		C_OPTION_PREPROCESS,
		C_OPTION_INCDIR,
		C_OPTION_GET_MACROS,
		C_OPTION_PRINT_DEPENDENCY,
		C_OPTION_APPEND_DEPENDENCY,
		C_OPTION_SOURCE,
	};

	TCC_CONTEXT tcc_context, *tc = &tcc_context;
	CC_ARRAY<CC_STRING> imacro_files;
	CC_ARRAY<CC_STRING> source_files;
	const char *suffix = NULL;
	CC_STRING outfile = "/dev/stdout";
	const char *tmpfile = "/dev/stdout";
	int i;
	MEMORY_FILE cl_info;
	CC_STRING warning;
	bool swap_search_order;
	const char *open_depfile_mode = NULL;
	CC_STRING dep_file;
	const char *errmsg;

	runtime_console.init(stderr);
	debug_console.init(stderr, tc);

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
			{"y-in-place", 2, 0, C_OPTION_IN_PLACE},
			{"y-preprocess", 0, 0, C_OPTION_PREPROCESS},
			{"y-I", 1, 0, C_OPTION_INCDIR},
			{"y-get-macros", 1, 0, C_OPTION_GET_MACROS},
			{"y-print-dependency", 2, 0, C_OPTION_PRINT_DEPENDENCY},
			{"y-source", 1, 0, C_OPTION_SOURCE},
			{0, 0, 0, 0}
		};

		c = getopt_long_only(argc, argv, "I:D:U:J:", long_options, &option_index);
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
			preprocess_mode = true;
			break;
		case C_OPTION_DEBUG:
			{
				MESSAGE_LEVEL level;
				level = (MESSAGE_LEVEL) atol(optarg);
				fprintf(stderr, "debug level: %u\n", level);
				debug_console.set_gate_level(level);
			}
			break;
		case C_OPTION_IMACROS:
			imacro_files.push_back(optarg);
			break;
		case C_OPTION_IN_PLACE:
			suffix = optarg;
			if(suffix == NULL)
				suffix = "";
			break;
		case C_OPTION_INCDIR:
//			fprintf(stderr, "include dirs B(1): %s\n", optarg);
			add_to_search_dir(__tcc_search_dirs[1], optarg);
			break;
		case C_OPTION_GET_MACROS:
//			fprintf(stderr, "get macros: %s\n", optarg);
			cl_info.contents += optarg;
			break;
		case C_OPTION_PRINT_DEPENDENCY:
			if(optarg != NULL) {
				dep_file = optarg;
				open_depfile_mode = "ab+";
			} else
				open_depfile_mode = "wb";
			break;
		case 'I':
//			fprintf(stderr, "include dirs A: %s\n", optarg);
			add_to_search_dir(__tcc_search_dirs[0], optarg);
			break;
		case 'D':
			new_define(cl_info.contents, optarg);
			break;
		case 'U':
			new_undef(cl_info.contents, optarg);
			break;
		case 'J':
//			fprintf(stderr, "include dirs B(2): %s\n", optarg);
			add_to_search_dir(__tcc_search_dirs[1], optarg);
			swap_search_order = true;
			break;
		case C_OPTION_SOURCE:
//			fprintf(stderr, "get macros: %s\n", optarg);
			source_files.push_back(optarg);
			break;


		case ':':
		case '?':
		default:
			;
		}
	}
	
	if(swap_search_order) {
		// #include <>
		tcc_search_dirs[0][0] = & __tcc_search_dirs[1];
		tcc_search_dirs[0][1] = & __tcc_search_dirs[0];
		// #include ""
		tcc_search_dirs[1][0] = & __tcc_search_dirs[0];
		tcc_search_dirs[1][1] = & __tcc_search_dirs[1];
	} else {
		tcc_search_dirs[0][0] = & __tcc_search_dirs[0];
		tcc_search_dirs[0][1] = & __tcc_search_dirs[1];
		tcc_search_dirs[1][0] = & __tcc_search_dirs[0];
		tcc_search_dirs[1][1] = & __tcc_search_dirs[1];
	}

	tcc_init(tc);
	ConditionalParser parser;
	static const char *keywords[] = {
		"#define", "#undef", "#if", "#ifdef", "#ifndef", "#elif", "#else", "#endif", "#include", "#include_next"
	};
	if(cl_info.contents.size() > 0) {
		errmsg = parser.do_parse(tc, keywords, 2, &cl_info, NULL, NULL);
		if(errmsg != NULL) {
			fprintf(stderr, "Error on parsing command line\n");
			return -2;
		}
//		debug_console << DML_DEBUG << (TCC_MACRO_TABLE) tc->mtab;
	}

	REAL_FILE file;
	if(imacro_files.size() > 0)
		for(size_t i = 0; i < imacro_files.size(); i++) {
			const char *path;
	
			path = check_file(imacro_files[i].c_str(), true);
			if( path == NULL || ! file.open(path) )
				continue;
			parser.do_parse(tc, keywords, 2, &file, NULL, NULL);
			file.close();
		}

//	fprintf(stderr, "%s", cl_info.contents.c_str());

//	fprintf(stderr, "suffix:%s\n", suffix);
	for(i = optind; i < argc; ) {
		const char *current_file = argv[i++];

		/* Pick up one source filename from lots of unrecognized options 
		 * by checking the extension name.
		 */
		if( ! check_ext(current_file) )
			continue;
		source_files.push_back(current_file);
	}

	if( source_files.size() == 0 ) 
		show_usage_and_exit(EXIT_FAILURE);
	if( source_files.size() > 1 ) {
		if( check_ext(argv[i]) ) {
			runtime_console << "Multiple input files" << BASIC_CONSOLE::endl;
			return -2;
		}
	}

	for(i = 0; i < 1; i++) {
		char tmpbuf[256];
		const char *current_file = source_files[i].c_str();
		FILE *depf = NULL;

		if(suffix != NULL) {
			strcpy(tmpbuf, PROGRAM_NAME "XXXXXX");
			if( mktemp(tmpbuf) == NULL ) {
				runtime_console << "Cannot create temporary files" << BASIC_CONSOLE::endl ;
				return -EIO;
			}
			tmpfile = tmpbuf;
			outfile = current_file;
			outfile += suffix;
		}

		if( open_depfile_mode )  {
			if( dep_file.size() == 0 )
				dep_file = get_dep_filename(current_file);
//			fprintf(stderr, "DEP %s, mode: %s\n", dep_file.c_str(), open_depfile_mode);
			depf = fopen(dep_file.c_str(), open_depfile_mode);
			if(depf == NULL) {
				snprintf(tmpbuf, sizeof(tmpbuf), "Cannot write to %s", dep_file.c_str());
				errmsg = tmpbuf;
				goto error;
			}
		}

//		fprintf(stderr, "DEP: %s\n", dep_file.c_str());
//		exit(2);

		file.set_file(current_file);
		errmsg = parser.do_parse(tc, keywords, COUNT_OF(keywords), &file, tmpfile, depf);
//		debug_console.macro_table_dump();
		if(errmsg == 0) {
			if(suffix != NULL) {
				if(suffix[0] != '\0') {
					rename(current_file, outfile.c_str());
				}
				rename(tmpfile, current_file);
				fprintf(stderr, "Success on handling %s\n", current_file);
			}
		} else {
error:
			fprintf(stderr, "Error on handling %s\n", current_file);
			fprintf(stderr, "**** %s\n", errmsg);
			remove(tmpfile);
			return -1;
		}
		break;
	}

	
	return 0;
}


