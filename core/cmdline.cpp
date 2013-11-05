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


static int compare_option(const char **_s1, const char *s2)
{
	const char *s1 = *_s1;
	while( *s1 && *s2 ) {
		const int c = *s1 - *s2;
		if(c != 0) 
			return c;
		s1 ++;
		s2 ++;
	}
	*_s1 = s1;
	return 0;
}


static int match_argument_against_options(const char *arg, const struct option *options)
{
	const struct option *o;
	for(o = options; o->name != NULL ; o++ ) {
		int short_op = !!(o->name[1] == '\0');
		const char *p = arg;

		if(*p == '-') p++;
		if( ! short_op )
			if(*p == '-') p++;
		if( compare_option(&p, o->name) == 0  ) {
			if( o->has_arg == 1 ) {
				if( *p == '=' )
					return 2; /* A option with param */
				else if( *p == '\0' )
					return 1; /* The following is a param */
				return short_op ? 2 : 0;
			}
		}
	}
	return 0;
}


int parse_option(char ***_argv, char **argv_end, const struct option *options,
	CC_STRING& optval)
{
	int ret;
	char **argv = * _argv;

	ret = match_argument_against_options(*argv, options);
	if(ret == 2) {
		const char *p = strchr(*argv, '=');
		if(p != NULL)
			p++;
		else
			p = (*argv) + 2;
		optval = p;
	} else if(ret == 1) {
		if(++argv < argv_end)
			optval = * argv;
	}
	* _argv = argv;
	return ret;
}

static void take_favorite_args(char **argv, char **argv_end, struct option *options, CC_STRING& new_args)
{
	int ret;
	CC_STRING optval;

	for( ; argv < argv_end; argv++ ) {
		ret = parse_option(&argv, argv_end, options, optval);
		switch(ret) {
		case 1:
			new_args += ' ';
			new_args += *(argv-1);
		case 2:
			new_args += ' ';
			new_args += *argv;
		}
	}
}

void setup_host_cc_env(int argc, char *argv[])
{
	char **a, **aend = &argv[argc];
	static struct option options[] = {
		{ "y-cc", 1, NULL, 0 },
		{ NULL, 0, NULL, 0},
	};
	static struct option armcc_options[] = {
		{ "cpu", 1, NULL, 0 },
		{ "O",   1, NULL, 0 },
		{ NULL,  0, NULL, 0 },
	};

	int ret;
	CC_STRING optval, new_args;

	for(a = argv ; a < aend; a++ ) {
		ret = parse_option(&a, aend, options, optval);
		if(ret > 0) {
			printf("Host CC: %s\n", optval.c_str());
			if(optval == "armcc") {
				take_favorite_args(argv, aend, armcc_options, new_args);
				printf("armcc %s\n", new_args.c_str());
			} else if( strstr(optval.c_str(),"gcc") != NULL ) {
			}
			return;
		}
	}
}


void preprocess_command_line(int argc, char *argv[], CC_ARRAY<CC_STRING> &new_options)
{
	char **argv_end = &argv[argc];
	int ret;
	static const option options[] = {
		{ "via", 1, NULL, 0 },
		{ NULL,  0, NULL, 0 },
	};

	for( ; argv < argv_end ; argv++) {
		FILE *fp;
		CC_STRING viafile;
		ret = parse_option(&argv, argv_end, options, viafile);
		if( ret == 2 || ret == 1 ) {
			if(viafile.size() == 0)
				continue;
		} else {
			new_options.push_back(*argv);
			continue;
		}
		fp = fopen(viafile.c_str(), "r");
		if(fp == NULL)
			exit(2);
		while(1) {
			char buf[2048];
			if( fscanf(fp, "%s", buf) == EOF )
				break;
			new_options.push_back(buf);
		}
		fclose(fp);
	}
}


