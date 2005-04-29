/*******************************************************************
*                                                                  *
*             This software is part of the ast package             *
*                Copyright (c) 1992-2004 AT&T Corp.                *
*        and it may only be used by you under license from         *
*                       AT&T Corp. ("AT&T")                        *
*         A copy of the Source Code Agreement is available         *
*                at the AT&T Internet web site URL                 *
*                                                                  *
*       http://www.research.att.com/sw/license/ast-open.html       *
*                                                                  *
*    If you have copied or used this software without agreeing     *
*        to the terms of the license you are infringing on         *
*           the license and copyright and are violating            *
*               AT&T's intellectual property rights.               *
*                                                                  *
*            Information and Software Systems Research             *
*                        AT&T Labs Research                        *
*                         Florham Park NJ                          *
*                                                                  *
*               Glenn Fowler <gsf@research.att.com>                *
*                David Korn <dgk@research.att.com>                 *
*                                                                  *
*******************************************************************/
#pragma prototyped
/*
 * David Korn
 * AT&T Bell Laboratories
 *
 * library interface for word count
 */

#include <cmdlib.h>
#include <wc.h>
#include <ctype.h>

#define endline(c)	(((signed char)-1)<0?(c)<0:(c)==((char)-1))

Wc_t *wc_init(char *space)
{
	register int  n;
	Wc_t *wp;

	if(!(wp = (Wc_t*)stakalloc(sizeof(Wc_t))))
		return(0);
	if(space)
		memcpy(wp->space, space, (1<<CHAR_BIT));
	else
	{
		for(n=(1<<CHAR_BIT);--n >=0;)
			wp->space[n] = (isspace(n)!=0);
		wp->space['\n'] = -1;
	}
	return(wp);
}

/*
 * compute the line, word, and character count for file <fd>
 */
int wc_count(Wc_t *wp, Sfio_t *fd)
{
	register signed char	*space = wp->space;
	register unsigned char	*cp;
	register Sfoff_t	nwords;
	register Sfoff_t	nlines;
	register ssize_t	c;
	register unsigned char	*endbuff;
	register int		lasttype = 1;
	unsigned int		lastchar;
	unsigned char		*buff;

	nlines = nwords = wp->chars = 0;
	sfset(fd,SF_WRITE,1);
	for (;;)
	{
		/* fill next buffer and check for end-of-file */
		if (!(buff = (unsigned char*)sfreserve(fd, 0, 0)) || (c = sfvalue(fd)) <= 0)
			break;
		sfread(fd,(char*)(cp=buff),c);
		wp->chars += c;
		/* check to see whether first character terminates word */
		if(c==1)
		{
			if(endline(lasttype))
				nlines++;
			if((c = space[*cp]) && !lasttype)
				nwords++;
			lasttype = c;
			continue;
		}
		if(!lasttype && space[*cp])
			nwords++;
		lastchar = cp[--c];
		cp[c] = '\n';
		endbuff = cp+c;
		c = lasttype;
		/* process each buffer */
		for (;;)
		{
			/* process spaces and new-lines */
			do if (endline(c))
			{
				for (;;)
				{
					/* check for end of buffer */
					if (cp > endbuff)
						goto eob;
					nlines++;
					if (*cp != '\n')
						break;
					cp++;
				}
			} while (c = space[*cp++]);
			/* skip over word characters */
			while(!(c = space[*cp++]));
			nwords++;
		}
	eob:
		if((cp -= 2) >= buff)
			c = space[*cp];
		else
			c  = lasttype;
		lasttype = space[lastchar];
		/* see if was in word */
		if(!c && !lasttype)
			nwords--;
	}
	if(endline(lasttype))
		nlines++;
	else if(!lasttype)
		nwords++;
	wp->words = nwords;
	wp->lines = nlines;
	return(0);
}
