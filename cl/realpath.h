#ifndef __A_REALPATH_H
#define __A_REALPATH_H

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

static CHAR_TYPE *go_upper(const CHAR_TYPE *const path, CHAR_TYPE *pos)
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
CHAR_TYPE *y_realpath(CHAR_TYPE *dest, size_t n, const CHAR_TYPE *src)
{
#define NEXT_STATE(next,step)  do { state = Y9STA_##next; *d = c; d += step; } while(0)

	const CHAR_TYPE *s;
	CHAR_TYPE *d;
	int state = 0;

	enum {
		Y9STA_INIT,
		Y9STA_OK,
		Y9STA_ERR,
		Y9STA_SPLASH,
		Y9STA_DOT,
		Y9STA_DOTDOT,
	};

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
		case Y9STA_INIT:
			switch(c) {
			case '\0':
				NEXT_STATE(OK, 0);
				break;
			case UNIX_DD:
			case DOS_DD:
				NEXT_STATE(SPLASH, 1);
				break;
			case '.':
				NEXT_STATE(DOT, 1);
				break;
			default:
				*d++ = c;
				break;
			}
			break;
		case Y9STA_OK:
			#if !HAVE_TRAILING_SPLASH
				if( *(d-1) == UNIX_DD )
					*--d   = '\0';
			#else
				if( *(d-1) != UNIX_DD )
					*d++ = UNIX_DD, *d = '\0';
			#endif
			goto done;
		case Y9STA_ERR:
			goto fail;
		case Y9STA_SPLASH:
			switch(c) {
			case '\0':
				NEXT_STATE(OK, 0);
				break;
			case UNIX_DD:
			case DOS_DD:
				break;
			case '.':
				NEXT_STATE(DOT, 1);
				break;
			default:
				NEXT_STATE(INIT, 1);
				break;
			}
			break;
		case Y9STA_DOT:
			switch(c) {
			case '\0':
				NEXT_STATE(OK, 0);
				break;
			case UNIX_DD:
			case DOS_DD:
				NEXT_STATE(SPLASH, -1);
				break;
			case '.':
				NEXT_STATE(DOTDOT, 0);
				break;
			default:
				NEXT_STATE(INIT, 1);
			}
			break;
		case Y9STA_DOTDOT:
			switch(c) {
			case '\0':
				state = Y9STA_OK;
				goto uplevel;
			case UNIX_DD:
			case DOS_DD:
				state = Y9STA_SPLASH;
				{
					CHAR_TYPE *t;
				uplevel:
					t = go_upper(dest, d);
					if(t == NULL)
						goto fail;
					d = t;
					if(c == '\0') *d = '\0';
					break;
				}
			default:
				state = Y9STA_ERR;
				break;
			}
			break;
		}
	}
done:
	return dest;
fail:
	return NULL;

#undef NEXT_STATE
}

#ifdef __cplusplus
}
#endif

#endif
