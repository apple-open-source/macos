/*******************************************************************
*                                                                  *
*             This software is part of the ast package             *
*                Copyright (c) 1982-2004 AT&T Corp.                *
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
*                David Korn <dgk@research.att.com>                 *
*                                                                  *
*******************************************************************/
#pragma prototyped
/*
 *   Routines to implement fast character input
 *
 *   David Korn
 *   AT&T Labs
 *
 */

#include	<ast.h>
#include	<sfio.h>
#include	<error.h>
#include	<fcin.h>

Fcin_t _Fcin = {0};

/*
 * open stream <f> for fast character input
 */
int	fcfopen(register Sfio_t* f)
{
	register int	n;
	char		*buff;
	Fcin_t		save;
	errno = 0;
	_Fcin.fcbuff = _Fcin.fcptr;
	_Fcin._fcfile = f;
	fcsave(&save);
	if(!(buff=(char*)sfreserve(f,SF_UNBOUND,1)))
	{
		fcrestore(&save);
		_Fcin.fcchar = 0;
		_Fcin.fcptr = _Fcin.fcbuff = &_Fcin.fcchar;
		_Fcin.fclast = 0;
		_Fcin._fcfile = (Sfio_t*)0;
		return(EOF);
	}
	n = sfvalue(f);
	fcrestore(&save);
	sfread(f,buff,0);
	_Fcin.fcoff = sftell(f);;
	buff = (char*)sfreserve(f,SF_UNBOUND,1);
	_Fcin.fclast = (_Fcin.fcptr=_Fcin.fcbuff=(unsigned char*)buff)+n;
	if(sffileno(f) >= 0)
		*_Fcin.fclast = 0;
	return(n);
}


/*
 * With _Fcin.fcptr>_Fcin.fcbuff, the stream pointer is advanced and
 * If _Fcin.fclast!=0, performs an sfreserve() for the next buffer.
 * If a notify function has been set, it is called
 * If last is non-zero, and the stream is a file, 0 is returned when
 * the previous character is a 0 byte.
 */
int	fcfill(void)
{
	register int		n;
	register Sfio_t	*f;
	register unsigned char	*last=_Fcin.fclast, *ptr=_Fcin.fcptr;
	if(!(f=fcfile()))
	{
		/* see whether pointer has passed null byte */
		if(ptr>_Fcin.fcbuff && *--ptr==0)
			_Fcin.fcptr=ptr;
		else
			_Fcin.fcoff = 0;
		return(0);
	}
	if(last)
	{
		if( ptr<last && ptr>_Fcin.fcbuff && *(ptr-1)==0)
			return(0);
		if(_Fcin.fcchar)
			*last = _Fcin.fcchar;
		if(ptr > last)
			_Fcin.fcptr = ptr = last;
	}
	if((n = ptr-_Fcin.fcbuff) && _Fcin.fcfun)
		(*_Fcin.fcfun)(f,(const char*)_Fcin.fcbuff,n);
	sfread(f, (char*)_Fcin.fcbuff, n);
	_Fcin.fcoff +=n;
	_Fcin._fcfile = 0;
	if(!last)
		return(0);
	else if(fcfopen(f) < 0)
		return(EOF);
	return(*_Fcin.fcptr++);
}

/*
 * Synchronize and close the current stream
 */
int fcclose(void)
{
	register unsigned char *ptr;
	if(_Fcin.fclast==0)
		return(0);
	if((ptr=_Fcin.fcptr)>_Fcin.fcbuff && *(ptr-1)==0)
		_Fcin.fcptr--;
	if(_Fcin.fcchar)
		*_Fcin.fclast = _Fcin.fcchar;
	_Fcin.fclast = 0;
	return(fcfill());
}

/*
 * Set the notify function that is called for each fcfill()
 */
void fcnotify(void (*fun)(Sfio_t*,const char*,int))
{
	_Fcin.fcfun = fun;
}

#ifdef __EXPORT__
#   define extern __EXPORT__
#endif

#undef fcsave
extern void fcsave(Fcin_t *fp)
{
	*fp = _Fcin;
}

#undef fcrestore
extern void fcrestore(Fcin_t *fp)
{
	_Fcin = *fp;
}

