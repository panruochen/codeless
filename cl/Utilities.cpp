#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <ctype.h>
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <libgen.h>

#include "cc_string.h"
#include "cc_array.h"
#include "Utilities.h"

bool find(const CC_ARRAY<CC_STRING>& haystack, const CC_STRING& needle)
{
	size_t i;

	for(i = 0; i < haystack.size(); i++) {
		if( haystack[i] == needle )
			return true;
	}
	return false;
}

void join(CC_ARRAY<CC_STRING>& x, const CC_ARRAY<CC_STRING>& y)
{
	size_t i;
	for(i = 0; i < y.size(); i++)
		x.push_back(y[i]);
}


void ujoin(CC_ARRAY<CC_STRING>& uarray, const char *elem)
{
	size_t i;
	for(i = 0; i < uarray.size(); i++) {
		if( strcmp(uarray[i].c_str(), elem) == 0 )
			return;
	}
	uarray.push_back(elem);
}

ssize_t getline(FILE *fp, CC_STRING& line)
{
	char c;
	if(fp == NULL)
		return -EINVAL;

	while( 1 ) {
		switch( fread(&c, 1, 1, fp) ) {
		case 0:
			break;
		case -1:
			return -EIO;
		}
		if(c == '\n')
			break;
		line += c;
	}
	return line.size();
}

void fatal(int exit_status, const char *format, ...)
{
    va_list ap;

    va_start(ap, format);
    vfprintf(stderr, format, ap);
	fputc('\n', stderr);
    va_end(ap);
	exit(exit_status);
}

bool fol_exist(const CC_STRING& filename)
{
	struct stat stb;
	return stat(filename, &stb) == 0;
}


CC_STRING fol_dirname(const CC_STRING& path)
{
	char *tmp;
	CC_STRING dir;

	tmp = strdup(path.c_str());
	dir = dirname(tmp);
	free(tmp);
	return dir;
}

#include "realpath.h"
CC_STRING fol_realpath(const CC_STRING& src)
{
    char dest[4096];

	if( ! GetRealPath(dest, sizeof(dest), src.c_str()) )
		return CC_STRING("");
	return CC_STRING(dest);
}
