/* $Xorg: miNS.h,v 1.4 2001/02/09 02:04:08 xorgcvs Exp $ */

/***********************************************************

Copyright 1989, 1990, 1991, 1998  The Open Group

Permission to use, copy, modify, distribute, and sell this software and its
documentation for any purpose is hereby granted without fee, provided that
the above copyright notice appear in all copies and that both that
copyright notice and this permission notice appear in supporting
documentation.

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
OPEN GROUP BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN
AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Except as contained in this notice, the name of The Open Group shall not be
used in advertising or otherwise to promote the sale, use or other dealings
in this Software without prior written authorization from The Open Group.

Copyright 1989, 1990, 1991 by Sun Microsystems, Inc. 

                        All Rights Reserved

Permission to use, copy, modify, and distribute this software and its 
documentation for any purpose and without fee is hereby granted, 
provided that the above copyright notice appear in all copies and that
both that copyright notice and this permission notice appear in 
supporting documentation, and that the name of Sun Microsystems,
not be used in advertising or publicity pertaining to distribution of 
the software without specific, written prior permission.  

SUN MICROSYSTEMS DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, 
INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT 
SHALL SUN MICROSYSTEMS BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL 
DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,
ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
SOFTWARE.

******************************************************************/

#include "ddpex.h"

#ifndef MINS_H
#define MINS_H

typedef ddULONG	ddNamePiece;
typedef ddNamePiece	*ddNamePtr;

#define	MINS_MIN_NAME	0
#define	MINS_MAX_NAME	255
#define	MINS_NAMESET_SIZE	(MINS_MAX_NAME - MINS_MIN_NAME +1)

#define	MINS_VALID_NAME(name)			\
	( ((name) >= MINS_MIN_NAME) && ((name) <= MINS_MAX_NAME) )

#define	MINS_COUNT_BITS(type_or_var)	(sizeof(type_or_var) * 8)

/* Name N is present if bit MINS_NAMESET_BIT(N) is set in
 * nameset word[ MINS_NAMESET_WORD(N) ]
 */
#define	MINS_NAMESET_WORD_COUNT		\
	(MINS_NAMESET_SIZE / MINS_COUNT_BITS(ddNamePiece))

#define	MINS_NAMESET_WORD_NUM(one_name)		\
	((one_name) / MINS_COUNT_BITS(ddNamePiece))

#define	MINS_NAMESET_BIT(one_name)	\
	(1 << ((one_name) % MINS_COUNT_BITS(ddNamePiece)))

#define	MINS_EMPTY_NAMESET(nameset)					\
	{ register ddNamePtr	n = nameset;				\
	  register ddNamePtr	end = &nameset[MINS_NAMESET_WORD_COUNT];	\
	  do	{ *n = 0; n++; } while (n < end); }

#define	MINS_FILL_NAMESET(nameset)					\
	{ register ddNamePtr	n = nameset;				\
	  register ddNamePtr	end = &nameset[MINS_NAMESET_WORD_COUNT];	\
	  do	{ *n = ~0; n++; } while (n < end);	}

#define	MINS_COPY_NAMESET(source, dest)			\
	{ register ddNamePtr	s = (source), d = (dest);	\
	  register ddNamePtr	end = &(d)[MINS_NAMESET_WORD_COUNT];	\
	  do	{ *d = *s; d++; s++; } while (d < end);	}

#define	MINS_OR_NAMESETS(source, dest)			\
	{ register ddNamePtr	s = (source), d = (dest);	\
	  register ddNamePtr	end = &(d)[MINS_NAMESET_WORD_COUNT];	\
	  do	{ *d |= *s; d++; s++; } while (d < end);	}

#define	MINS_IS_NAME_IN_SET(one_name, nameset)		\
	((nameset)[MINS_NAMESET_WORD_NUM(one_name)] & 	\
		MINS_NAMESET_BIT(one_name))

#define MINS_ADD_TO_NAMESET(one_name, nameset)		\
	(nameset)[MINS_NAMESET_WORD_NUM(one_name)] |=	\
		MINS_NAMESET_BIT(one_name)

#define MINS_REMOVE_FROM_NAMESET(one_name, nameset)	\
	(nameset)[MINS_NAMESET_WORD_NUM(one_name)] &=	\
		~MINS_NAMESET_BIT(one_name)

#define	MINS_NOT_NAMESET(nameset)			\
	{ register ddNamePtr	n = nameset;		\
	  register ddNamePtr	end = &nameset[MINS_NAMESET_WORD_COUNT];\
	  do	{ *n = ~(*n); n++; } 			\
	  while (n < end);	}

#define	MINS_IS_NAMESET_EMPTY(nameset, isempty)		\
	{ register ddNamePtr	n = nameset;		\
	  register ddNamePtr	end = &nameset[MINS_NAMESET_WORD_COUNT];\
	  (isempty) = ~0;				\
	  do { (isempty) = (isempty) && !(*n); n++; }	\
	  while (n < end);	}
	
#define	MINS_MATCH_NAMESETS(names1, names2, match)			\
	{ register ddNamePtr	n1 = names1, n2=names2;		\
	  register ddNamePtr	end = &names1[MINS_NAMESET_WORD_COUNT];	\
	  (match) = 0;					\
	  do	{ (match) = (match) || ((*n1) & (*n2)); n1++; n2++; } 	\
	  while (n1 < end);	}

typedef struct _miNSHeader {
	/* the resource id is in the dipex resource structure */
	listofObj		*wksRefList;
	listofObj		*rendRefList;
	ddULONG			refCount;	/* pick & search context*/
	ddULONG			nameCount;
	ddNamePiece		names[ MINS_NAMESET_WORD_COUNT ];
	ddBOOL			freeFlag; 
} miNSHeader;
	
#endif	/* MINS_H */
