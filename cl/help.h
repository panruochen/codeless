#ifndef __HELP_H
#define __HELP_H
static const char help_msg[] =
"NAME\n"
"       PROGRAM_NAME - Yet another C/C++ Preprocessor\n"
"\n"
"SYNOPSIS\n"
"       PROGRAM_NAME [OPTION]... [FILE]\n"
"\n"
"DESCRIPTION\n"
"       Parse and remove conditional compilation blocks from C/C++ source files.\n"
"\n"
"  OPTIONS INHERITED FROM COMMAND LINE\n"
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
"  OPTIONS FOR SELF\n"
"       --yz-in-place [SUFFIX]\n"
"              Overwrite the input files instead of outputting to STDOUT\n"
"              Back up if SUFFIX is specified\n"
"\n"
"       --yz-debug LEVEL\n"
"              Set debug message level\n"
"\n"
"       --yz-preprocess\n"
"              Force the parser to work in preprocessor mode, where\n"
"              1. Included files are fetched and parsed\n"
"              2. Undefined macros are regarded as blanks on evaluation\n"
"              3. Exit upon any error\n"
"\n"
"       --yz-print-dep FILE\n"
"              Print path names of dependent header files to FILE, excluding those in the compiler installation directories.\n"
"\n"
"       --yz-source FILE\n"
"              Force FILE to be parsed.\n"
"\n"
"Report bugs to <ijkxyz@msn.com>\n"
"\n"
;
#endif
