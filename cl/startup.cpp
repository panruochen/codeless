#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <ctype.h>
#include <unistd.h>

#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <getopt.h>
#include <libgen.h>

#include "Parser.h"
#include "GlobalVars.h"
#include "utils.h"
#include "misc.h"
#include "defconfig.h"

static void __NO_USE__ show_usage_and_exit(int exit_code);

static CC_STRING my_dir;

const void show_search_dirs(const ParserContext& yctx)
{
	size_t i;
	FILE *dev = stderr;

	fprintf(dev, "Search directories:\n");
	for(i = 0; i < yctx.i_dirs.size(); i++)
		fprintf(dev, " %s\n", yctx.i_dirs[i].c_str());
	fprintf(dev, "System search directories:\n");
	for(i = 0; i < yctx.compiler_dirs.size(); i++)
		fprintf(dev, " %s\n", yctx.compiler_dirs[i].c_str());
	fprintf(dev, "\n");
}


SOURCE_TYPE check_source_type(const CC_STRING& filename)
{
	const char *p = strrchr(filename.c_str(), '.');
	if(p != NULL) {
		p++;
		if( strcasecmp(p, "c") == 0 ) {
			return SOURCE_TYPE_C;
		} else if( strcasecmp(p, "cc") == 0 || strcasecmp(p, "cxx") == 0 || strcasecmp(p, "cpp") == 0 ) {
			return SOURCE_TYPE_CPP;
		} else if( strcmp(p, "S") == 0 )
			return SOURCE_TYPE_S;
	}
	return SOURCE_TYPE_ERR;
}

static CC_STRING MakeDepFileName(const char *filename)
{
	CC_STRING s;
	const char *p = filename + strlen(filename);
	while((--p) >= filename) {
		if(*p == '.') {
			p++;
			s.strcat(filename, p );
			s += "xdd";
			break;
		}
	}
	return s;
}

static void save_command_line(const CC_STRING& filename, const CC_STRING& host_cc, const CC_STRING& cc_args, const CC_STRING& my_args )
{
	if(gvar_file_writers[MSGT_CL]) {
		char cwd[260];
		CC_STRING s;

		getcwd(cwd, sizeof(cwd));
		s += "cd ";
		s += cwd;
		s += '\n';

		s += host_cc;
		s += ' ';
		s += cc_args;
		s += "  ## ";
		s += my_args;
		s += "\n\n";
		gvar_file_writers[MSGT_CL]->Write(s.c_str(), s.size());
	}
}

static CC_STRING get_cc(const CC_STRING& cc)
{
	const char *cc1 = cc.c_str();
	CC_STRING cc2;
	int n;

	if(cc.size() >= 3) {
		n = cc.size() - 3;
		if( strcmp(cc1+n, "gcc") == 0 )
			cc2 = "gcc";
		else if(strcmp(cc1+n, "armcc") == 0)
			cc2 = "armcc";
	}
	return cc2;
}

static FILE *pyext_exec(const CC_STRING& host_cc, const CC_STRING entryf, const CC_STRING& cl_args)
{
	FILE *fp;
	CC_STRING cl;
	int status;
	CC_STRING cc2;

	cc2 = get_cc(host_cc);
	if(cc2.isnull())
		fatal(127, "%s is not supported yet", host_cc.c_str());

	cl  = "python -c 'import sys\nsys.path.append(\""
		+ my_dir
		+ "/pyext\")\nimport "
		+ cc2
		+ "\n";
	cl += cc2 + "." + entryf + "()";
	cl += "' " + cl_args;
	fp = popen(cl.c_str(), "r");
	if(fp == NULL)
		fatal(131, "Cannot execute \"%s\"\n", cl.c_str());

	wait(&status);
	if(status != 0) {
		pclose(fp);
		fp = NULL;
	}
	return fp;
}

static void get_host_cc_search_dirs(const CC_STRING& host_cc, CC_ARRAY<CC_STRING>& search_dirs, const CC_STRING& cl_args)
{
	FILE *fp;
	char buf[1024];

	fp = pyext_exec(host_cc, "get_search_dirs", cl_args);
	while( fgets(buf, sizeof(buf), fp) != NULL ) {
		const ssize_t n = strlen(buf);
		if(n > 0 && buf[n-1] == '\n')
			buf[n-1] = '\0';
		search_dirs.push_back(buf);
	}
	if(fp != NULL)
		pclose(fp);
}

static void get_host_cc_predefined_macros(const CC_STRING& host_cc, MemFile& predef_macros, const CC_STRING& cl_args, char& as_lc_char)
{
	FILE *fp;
	char buf[1024];

	fp = pyext_exec(host_cc, "get_predefines", cl_args);
	while( fgets(buf, sizeof(buf), fp) != NULL ) {
		predef_macros << buf;
		if(strstr(buf, "#define __arm__"))
			as_lc_char = '@';
		else if(strstr(buf, "#define __x86") || strstr(buf, "#define i386"))
			as_lc_char = '#';
	}
	if(fp != NULL)
		pclose(fp);
}

static bool create_file_writers(ParserContext *ctx)
{
	unsigned int i;

	for(i = 0; i < MSGT_MAX; i++) {
		if(ctx->of_array[i].isnull())
			continue;
		if(ctx->server_addr.isnull() || getenv("CL_FORCE_LOCAL_WRITE") )
			gvar_file_writers[i] = new OsFileWriter(i, ctx->of_array[i]);
		else {
			DsFileWriter *fw = new DsFileWriter(i);
			if( fw->Connect(ctx->runtime_dir.isnull() ? DEF_RT_DIR : ctx->runtime_dir.c_str(), ctx->server_addr.c_str()) < 0 ) {
				const int t = i;
				for(i = 0; i < MSGT_MAX; i++)
					if(gvar_file_writers[i])
						delete gvar_file_writers[i];
				fprintf(stderr, "Cannot create socket for %u\n", t);
				return false;
			}
			gvar_file_writers[i] = fw;
		}
	}
	return true;
}

int main(int argc, char *argv[])
{
	InternalTables tcc_context, *pstate = &tcc_context;
	size_t i;
	ParserContext yctx;
	char as_lc_char = 0;

	my_dir = fol_dirname(argv[0]);

	if( yctx.get_options(argc, argv) != 0 )
		fatal(128, "%s\n", yctx.errmsg.c_str());

	if( yctx.print_help )
		show_usage_and_exit(0);

	if( ! create_file_writers(&yctx) )
		fatal(EPERM, "Can not create file writers\n");

	gvar_sm = ipsc_open("/var/tmp/SM0");

	if(!yctx.of_array[MSGT_CL].isnull())
		save_command_line(yctx.of_array[MSGT_CL].c_str(), yctx.cc, yctx.cc_args, yctx.my_args);

	if( gv_preprocess_mode ) {
		if(yctx.cc.isnull())
			fatal(4, "--yz-cc must be specified");
		if(yctx.cc_path.isnull()) {
			FILE *pipe;
			CC_STRING cmd = "which " + yctx.cc;
			pipe = popen(cmd.c_str(), "r");
			if( getline(pipe, yctx.cc_path) <= 0 )
				fatal(2, "Cannot run: \"%s\"\n", cmd.c_str());
		}
		setenv("YZ_CC_PATH", yctx.cc_path.c_str(), 1);
		unsetenv("LANG");
		unsetenv("LANGUAGE");
		get_host_cc_predefined_macros(yctx.cc, yctx.predef_macros, yctx.cc_args, as_lc_char); /* FIXME, should get as_lc_char in cleaner mode, too */
		get_host_cc_search_dirs(yctx.cc, yctx.compiler_dirs, yctx.cc_args);
	}

	paserd_state_init(pstate);
	Parser yc;

	if( yctx.source_files.size() == 0 )
		exit(0);

	if(yctx.predef_macros.Size() > 0) {
		yctx.predef_macros.SetFileName("<command line>");
		if( ! yc.DoFile(pstate, 2, &yctx.predef_macros, NULL) )
			fatal(121, "Error on parsing command line\n") ;
	}

	RealFile file;
	for(i = 0; i < yctx.source_files.size(); i++) {
		const char *current_file = yctx.source_files[i].c_str();
		CC_STRING s;

		if( ! yctx.of_array[MSGT_DEP].isnull() ) {
			if( yctx.of_array[MSGT_DEP][0] == '\x1' )
				yctx.of_array[MSGT_DEP] = MakeDepFileName(current_file);
		}

		file.SetFileName(current_file);

		if(check_source_type(current_file) == SOURCE_TYPE_S)
			yctx.as_lc_char = as_lc_char;

		if( ! yc.DoFile(pstate, (size_t)-1, &file, &yctx))
			fatal(120, "%s", yc.GetError());
		break;
	}
//	show_search_dirs(yctx); exit(2);
	return 0;
}


#include "help.h"
static void __NO_USE__ show_usage_and_exit(int exit_code)
{
	log(LOGV_RUNTIME, "%s", help_msg);
	exit(exit_code);
}


