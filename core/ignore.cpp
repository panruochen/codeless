#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/stat.h>

#include <sys/types.h>
#include <getopt.h>
#include <regex.h>

#include "c++-if.h"
#include "tcc.h"

static CC_ARRAY<regex_t> ignore_patterns;

bool add_ignore_pattern(const char *pattern)
{
	regex_t reg;

	if( regcomp(&reg, pattern, 0) != 0 ) {
		return false;
	}
	ignore_patterns.push_back(reg);
	debug_console << DML_INFO << "Add Pattern: " << pattern << '\n';
	return true;
}

bool check_file_ignored(const char *filename)
{
	size_t i;
	regmatch_t match[2];

	for(i = 0; i < ignore_patterns.size(); i++) {
		regex_t *preg = &(ignore_patterns[i]);
		if( regexec(preg, filename, 2, match, 0) == 0 ) {
			debug_console << DML_INFO << "Checking for " << filename << ", matched\n";
			return true;
		}
	}
	debug_console << DML_INFO << "Checking for " << filename << ", not matched\n";
	return false;
}

