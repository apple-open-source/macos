/* $Xorg: cache.c,v 1.4 2001/02/09 02:05:47 xorgcvs Exp $ */

/**** module cache.c ****/
/******************************************************************************

Copyright 1993, 1994, 1998  The Open Group

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


				NOTICE
                              
This software is being provided by AGE Logic, Inc. under the
following license.  By obtaining, using and/or copying this software,
you agree that you have read, understood, and will comply with these
terms and conditions:

     Permission to use, copy, modify, distribute and sell this
     software and its documentation for any purpose and without
     fee or royalty and to grant others any or all rights granted
     herein is hereby granted, provided that you agree to comply
     with the following copyright notice and statements, including
     the disclaimer, and that the same appears on all copies and
     derivative works of the software and documentation you make.
     
     "Copyright 1993, 1994 by AGE Logic, Inc."
     
     THIS SOFTWARE IS PROVIDED "AS IS".  AGE LOGIC MAKES NO
     REPRESENTATIONS OR WARRANTIES, EXPRESS OR IMPLIED.  By way of
     example, but not limitation, AGE LOGIC MAKE NO
     REPRESENTATIONS OR WARRANTIES OF MERCHANTABILITY OR FITNESS
     FOR ANY PARTICULAR PURPOSE OR THAT THE SOFTWARE DOES NOT
     INFRINGE THIRD-PARTY PROPRIETARY RIGHTS.  AGE LOGIC 
     SHALL BEAR NO LIABILITY FOR ANY USE OF THIS SOFTWARE.  IN NO
     EVENT SHALL EITHER PARTY BE LIABLE FOR ANY INDIRECT,
     INCIDENTAL, SPECIAL, OR CONSEQUENTIAL DAMAGES, INCLUDING LOSS
     OF PROFITS, REVENUE, DATA OR USE, INCURRED BY EITHER PARTY OR
     ANY THIRD PARTY, WHETHER IN AN ACTION IN CONTRACT OR TORT OR
     BASED ON A WARRANTY, EVEN IF AGE LOGIC LICENSEES
     HEREUNDER HAVE BEEN ADVISED OF THE POSSIBILITY OF SUCH
     DAMAGES.
    
     The name of AGE Logic, Inc. may not be used in
     advertising or publicity pertaining to this software without
     specific, written prior permission from AGE Logic.

     Title to this software shall at all times remain with AGE
     Logic, Inc.
*****************************************************************************
  
	cache.c -- Code to support caching of photomaps in the server by 
		   xieperf. 

	Syd Logan -- AGE Logic, Inc.
  
*****************************************************************************/
/* $XFree86: xc/programs/xieperf/cache.c,v 1.6 2001/12/14 20:01:46 dawes Exp $ */

#include "xieperf.h"
#include <stdio.h>

int	CacheSizeMax = 4;	/* cache size, can be increased with -cache arg */

/* cache list element definition */

typedef struct _cache
{
        XiePhotomap     pId;
        Bool            active;		/* unused */
        time_t          timeStamp;
        XIEimage        *image;
        struct _cache   *next;
} Cache;

static Cache * PrivIsImageInCache ( XIEimage *image, Cache **prev );
static Cache * PrivIsPhotomapInCache ( XiePhotomap pId, Cache **prev );
static int SetAndScootch ( Cache *cPtr );
static int RemoveFromCacheLRU ( void );

static Cache *cHead = ( Cache * ) NULL;
static int cSize;

/* cache functions */

void
CacheInit(void)
{
	Cache *ptr, *next;

	ptr = cHead;
	while ( ptr != ( Cache * ) NULL )
	{
		next = ptr->next;
		free( ptr );
		ptr = next;
	}
	cHead = ( Cache * ) NULL;
	cSize = 0;
}

void
FlushCache(void)
{
	Cache *ptr, *next;

	ptr = cHead;
	while ( ptr != ( Cache * ) NULL )
	{
		XieDestroyPhotomap( GetDisplay(), ptr->pId );
		next = ptr->next;
		free( ptr );
		ptr = next;
	}
	cHead = ( Cache * ) NULL;
	cSize = 0;
}

int
SetImageActiveState(XIEimage *image, Bool active)
{
	Cache *cPtr, *prev;
	int retval = 0;

	if ( ( cPtr = PrivIsImageInCache( image, &prev ) ) != ( Cache * ) NULL )	
	{
		retval = 1;
                cPtr->active = active;
	}
	return( retval );
}

/* unused */

int
SetPhotomapActiveState(XiePhotomap pId, Bool active )
{
	Cache *cPtr, *prev;
	int retval = 0;

        if ( ( cPtr = PrivIsPhotomapInCache( pId, &prev ) ) != ( Cache * ) NULL )
	{
		retval = 1;
                cPtr->active = active;
	}
	return( retval );
}

int
AddToCache(XIEimage *image, XiePhotomap pId )
{
	Cache *cPtr, *prev;

	/* just in case it is already in the cache */

	if ( ( cPtr = PrivIsImageInCache( image, &prev ) ) != ( Cache * ) NULL )
	{
		if ( cPtr != cHead )
		{
			prev->next = cPtr->next;
			cPtr->next = cHead;
			cHead = cPtr;
		}
		SetAndScootch( cPtr );
	}
	else if (( cPtr = PrivIsPhotomapInCache( pId, &prev ) ) != ( Cache * ) NULL)
	{
		if ( cPtr != cHead )
		{
			prev->next = cPtr->next;
			cPtr->next = cHead;
			cHead = cPtr;
		}
		SetAndScootch( cPtr );
	}
	else
	{
		if ( cSize == CacheSizeMax )
		{
			/* annilate the least recently used entry */

			RemoveFromCacheLRU();
			cSize--;
		}
		cPtr = ( Cache * ) malloc( sizeof( Cache ) );
		if ( cPtr == ( Cache * ) NULL )
		{
			fprintf(stderr,"Error allocating image cache memory\n");
			return( 0 );
		}
		else 
		{
			cPtr->next = ( Cache * ) NULL;
			cPtr->active = False;
			cPtr->image = image;
			cPtr->pId = pId;
		}
		if ( cHead == ( Cache * ) NULL )
		{
			/* only one in the list */

			cHead = cPtr;
		}
		else
		{ 
			/* add it to the head of the list */

			cPtr->next = cHead;
			cHead = cPtr;
		}	

		/* put the list in LRU order */

		SetAndScootch( cPtr );
		cSize++;
	}	
	return( 1 );
}

Bool
IsImageInCache(XIEimage *image)
{
	Cache	*dummy;

	if ( PrivIsImageInCache( image, &dummy ) != ( Cache * ) NULL )
		return( True );
	return( False );
}

Bool
IsPhotomapInCache(XiePhotomap pId)
{
	Cache	*dummy;

	if ( PrivIsPhotomapInCache( pId, &dummy ) != ( Cache * ) NULL )
		return( True );
	return( False );
}

XiePhotomap
PhotomapOfImage(XIEimage *image)
{
	Cache *dummy, *cPtr;	

	cPtr = PrivIsImageInCache( image, &dummy );
	if ( cPtr != ( Cache * ) NULL )
		return( cPtr->pId );
	return( ( XiePhotomap ) NULL );	
}

static Cache *
PrivIsImageInCache(XIEimage *image, Cache **prev)
{
	Cache *cPtr, *retval;

	retval = ( Cache * ) NULL;	
	cPtr = *prev = cHead;
	while ( retval == ( Cache * ) NULL && cPtr != ( Cache * ) NULL )
	{
		if ( cPtr->image == image )
			retval = cPtr;
		else
		{
			*prev = cPtr;
			cPtr = cPtr->next;
		}
	}
	return( retval );
}

static Cache *
PrivIsPhotomapInCache(XiePhotomap pId, Cache **prev)
{
	Cache *cPtr, *retval;

	cPtr = *prev = cHead;
	retval = ( Cache * ) NULL;	
	while ( retval == ( Cache * ) NULL && cPtr != ( Cache * ) NULL )
	{
		if ( cPtr->pId == pId )
			retval = cPtr;
		else
		{
			*prev = cPtr;
			cPtr = cPtr->next;
		}
	}
	return( retval );
}

static int
SetAndScootch(Cache *cPtr)
{
	time_t	tLoc;
	int	found;
	Cache 	*cur, *prev;

	found = 0;
	time( &tLoc );
	cPtr->timeStamp = tLoc;

	/* find a node which 1) is not the node being moved and 2) has 
           a timeStamp greater than or equal to our timeStamp. We will 
	   insert ourselves right before it in the list */

	if ( cHead == ( Cache * ) NULL )
		return( 1 );		

	else if ( cHead->next == ( Cache * ) NULL )
		return( 1 );

	cur = prev = cHead;
	while ( !found && cur != ( Cache * ) NULL )
	{
		if ( cur != cPtr )
			if ( cur->timeStamp > cPtr->timeStamp )
				found = 1;
		if ( !found )
		{
			prev = cur;
			cur = cur->next;
		}
	}		

	if ( cur == ( Cache * ) NULL )
	{
		prev->next = cPtr;
		cHead = cHead->next;
		cPtr->next = ( Cache * ) NULL;
	}
	else if ( found )
	{
		cHead = cHead->next;
		prev->next = cPtr;
		cPtr->next = cur;
	}	
	return( 1 );
}

int
RemoveImageFromCache(XIEimage *image)
{
        Cache *cPtr, *prev;

        if ( ( cPtr = PrivIsImageInCache( image, &prev ) ) != ( Cache * ) NULL )
	{
		XieDestroyPhotomap( GetDisplay(), PhotomapOfImage( image ) );
		if ( prev == cPtr )
		{
			/* must be head of list */

			cHead = cPtr->next;
			free( cPtr );
		}
		else
		{
			prev->next = cPtr->next;
			free( cPtr );
		}
	}
	return( 1 );
}

int
RemovePhotomapFromCache(XiePhotomap pId)
{
        Cache *cPtr, *prev;

        if ( ( cPtr = PrivIsPhotomapInCache( pId, &prev ) ) != ( Cache * ) NULL )
	{
		XieDestroyPhotomap( GetDisplay(), pId );
		if ( prev == cPtr )
		{
			/* must be head of list */

			cHead = cPtr->next;
			free( cPtr );
		}
		else
		{
			prev->next = cPtr->next;
			free( cPtr );
		}
	}
	return( 1 );
}

int
TouchImage(XIEimage *image)
{
	Cache	*cPtr, *prev;
	int	retval;

	retval = 0;
        if ( ( cPtr = PrivIsImageInCache( image, &prev ) ) != ( Cache * ) NULL )
	{
		if ( cPtr != cHead )
		{
			prev->next = cPtr->next;
			cPtr->next = cHead;
			cHead = cPtr;
		}
		SetAndScootch( cPtr );
		retval = 1;
	}
	return( retval );
}

int
TouchPhotomap(XiePhotomap pId)
{
	Cache	*cPtr, *prev;
	int	retval;

	retval = 0;
        if ( ( cPtr = PrivIsPhotomapInCache( pId, &prev ) ) != ( Cache * ) NULL )
	{
		if ( cPtr != cHead )
		{
			prev->next = cPtr->next;
			cPtr->next = cHead;
			cHead = cPtr;
		}
		SetAndScootch( cPtr );
		retval = 1;
	}
	return( retval );
}

static int
RemoveFromCacheLRU(void)
{
	Cache *ptr;

	if ( cHead != ( Cache * ) NULL )
	{
		XieDestroyPhotomap( GetDisplay(), PhotomapOfImage( cHead->image ) );
		ptr = cHead;
		cHead = cHead->next;
		free( ptr );
	}
	return( 1 );
}

XIEimage *
GetLRUImage(void)
{
	if ( cHead == ( Cache * ) NULL )
		return( ( XIEimage * ) NULL );
	return( cHead->image );
}

XiePhotomap 
GetLRUPhotomap(void)
{
	if ( cHead == ( Cache * ) NULL )
		return( ( XiePhotomap ) NULL );
	return( cHead->pId );
}

void
DumpCache(void)
{
	Cache *cPtr;

	cPtr = cHead;
	fprintf( stderr, "Head of list is %p\n", cHead );
	fprintf( stderr, "Cache list is:\n" );
	while ( cPtr != ( Cache * ) NULL )
	{
		fprintf( stderr, 
			"node %p pId %x active %s timeStamp %lx image %p next %p\n",
			cPtr,
			(unsigned int)cPtr->pId, 
			( cPtr->active == True ? "True" : "False" ),
			cPtr->timeStamp, 
			cPtr->image,
			cPtr->next 
		);
		cPtr = cPtr->next;
	}
}

#ifdef TESTCACHE

XIEimage	image1;
XIEimage	image2;
XIEimage	image3;
XIEimage	image4;
XIEimage	image5;
XiePhotomap	p1, p2, p3, p4, p5;

int
main(int argc, char *argv[])
{
	CacheInit();
	p1 = 0xaa;
	p2 = 0xbb;
	p3 = 0xcc;
	p4 = 0xdd;
	p5 = 0xee;
	AddToCache( &image1, p1 );
	DumpCache();
	sleep( 10 );
	AddToCache( &image2, p2 );
	DumpCache();
	sleep( 5 );
	AddToCache( &image3, p3 );
	DumpCache();
	sleep( 5 );
	AddToCache( &image4, p3 );
	DumpCache();
	sleep( 5 );
	AddToCache( &image5, p3 );
	DumpCache();
	sleep( 5 );
	AddToCache( &image5, p3 );
	DumpCache();
	sleep( 5 );
	AddToCache( &image4, p4 );
	DumpCache();
	sleep( 5 );
	AddToCache( &image5, p5 );
	DumpCache();
	RemovePhotomapFromCache( p1 );
	DumpCache();	
	RemovePhotomapFromCache( p2 );
	DumpCache();	
	RemovePhotomapFromCache( p3 );
	DumpCache();	
	RemovePhotomapFromCache( p4 );
	DumpCache();	
	RemovePhotomapFromCache( p5 );
	DumpCache();	
	exit( 0 );
}
#endif
