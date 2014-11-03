#ifndef __STD_PATH_H__
#define __STD_PATH_H__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

#define IS_DIR_DELIM(c)   ((c) == '/' || (c) == '\\')
#define UNIX_DD    '/'
#define DOS_DD     '\\'

typedef char CHAR_TYPE;

static CHAR_TYPE *__stdpath_uplevel(const CHAR_TYPE *const path, CHAR_TYPE *pos)
{
	int nd = 0;
	while(pos >= path) {
		const CHAR_TYPE c = *pos;
		if( IS_DIR_DELIM(c) ) {
			nd++;
			if(nd == 2)
				return pos + 1;
		}
		pos --;
	}
	return NULL;
}

#define  HAVE_TRAILING_SPLASH  0
static CHAR_TYPE *regular_pathname_get(CHAR_TYPE *dest, size_t n, const CHAR_TYPE *src)
{
	const CHAR_TYPE *s;
	CHAR_TYPE *d;
	uint8_t state = 0;
	
	if( *src != '/' ) {
		getcwd(dest, n);
		d = dest + strlen(dest);
		*d ++ = '/';
	} else
		d = dest;
	s = src;
	while(1) {
		const CHAR_TYPE c = *s++;
		switch(state) {
		case 0:
			switch(c) {
			case '\0':    *d = c; state = 1; break;
			case UNIX_DD:
			case DOS_DD:  *d++ = c; state = 3; break;
			case '.':     *d++ = c; state = 4; break;
			default:      *d++ = c; break;
			}
			break;
		case 1:	
			#if !HAVE_TRAILING_SPLASH
				if( *(d-1) == UNIX_DD )
					*--d   = '\0';
			#else
				if( *(d-1) != UNIX_DD )
					*d++ = UNIX_DD, *d = '\0';
			#endif
			goto done;
		case 2:	goto fail;
		case 3:	
			switch(c) {
			case '\0':    *d = c; state = 1; break;
			case UNIX_DD:
			case DOS_DD:  break;
			case '.':     *d++ = c; state = 4; break;
			default:      *d++ = c; state = 0; break;
			}
			break;
		case 4:
			switch(c) {
			case '\0':    *d = c; state = 1; break;
			case UNIX_DD:
			case DOS_DD:  state = 3; d -= 1; break;
			case '.':     *d++ = c; state = 5; break;
			default:      *d++ = c; state = 0; break;
			}
			break;
		case 5:
			switch(c) {
			case '\0':    state = 1; goto uplevel;
			case UNIX_DD:
			case DOS_DD:  state = 3; 
				{
					CHAR_TYPE *t;
				uplevel:
					t = __stdpath_uplevel(dest, d);
					if(t == NULL)
						goto fail;
					d = t;
					if(c == '\0') *d = '\0';
					break;
				}
			default:      state = 2; break;
			}
			break;
		}
	}
done:
	return dest;
fail:
	return NULL;
}

#ifdef __cplusplus
}
#endif

#endif
