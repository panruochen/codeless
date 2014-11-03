#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "ycpp.h"
#include "utils.h"


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
		if( uarray[i] == elem )
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



