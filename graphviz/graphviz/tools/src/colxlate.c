#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#ifdef HAVE_SEARCH_H
#include <search.h>
#endif
#include <ctype.h>
#ifndef FALSE
#define FALSE (0)
#endif
#ifndef TRUE
#define TRUE (!FALSE)
#endif
#ifndef NOT
#define NOT(x)                  (!(x))
#endif
#ifndef NIL
#define NIL(type)               ((type)0)
#endif
typedef struct hsbcolor_t {
	char			*name;
	unsigned char	h,s,b;
} hsbcolor_t;


#ifndef NOCOLORNAMES
#include "colortbl.h"

/*
	char *bsearch ((char *) key, (char *) base, nel, sizeof (*key), compar)
	unsigned nel;
	int (*compar)( );
*/

static char *
canoncolor(char* orig, char* out)
{
	char	c,*p = out;
	while ((c = *orig++)) {
		if (isalnum(c) == FALSE) continue;
		if (isupper(c)) c = tolower(c);
		*out++ = c;
	}
	*out = c;
	return p;
}

static int
colorcmpf(const void* a0, const void* a1)
{
    hsbcolor_t	*p0 = (hsbcolor_t*)a0;
    hsbcolor_t	*p1 = (hsbcolor_t*)a1;
	int		i = (p0->name[0] - p1->name[0]);
	return (i ? i : strcmp(p0->name,p1->name));
}

char *
colorxlate(char* str, char* buf)
{
	static	hsbcolor_t	*last;
	char				*p,canon[128];
	hsbcolor_t			fake;

	if ((last == NULL)||(last->name[0] != str[0])||(strcmp(last->name,str))) {
		fake.name = canoncolor(str,canon);
		last = (hsbcolor_t*) bsearch(&fake,color_lib,sizeof(color_lib)/sizeof(hsbcolor_t),sizeof(fake),colorcmpf);
	}
	if (last == NULL) {
		if (isdigit(canon[0]) == FALSE) {
			fprintf(stderr,"warning: %s is not a known color\n",str);
			strcpy(buf,str);
		}
		else for (p = buf; (*p = *str++); p++) if (*p == ',') *p = ' ';
	}
	else sprintf(buf,"%.3f %.3f %.3f",((double)last->h)/255,((double)last->s)/255,((double)last->b)/255);
	return buf;
}
#else
char * colorxlate(char* str, char* buf) {return str;}
#endif
