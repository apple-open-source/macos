
/*
 * Mesa 3-D graphics library
 * Version:  4.0.2
 *
 * Copyright (C) 1999-2002  Brian Paul   All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * BRIAN PAUL BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN
 * AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */
/* $XFree86: xc/extras/Mesa/src/mem.h,v 1.9 2003/01/12 03:55:44 tsi Exp $ */

#ifndef MEM_H
#define MEM_H


#include "glheader.h"
/* Do not reference mtypes.h from this file.
 */

/*
 * Memory allocation
 */
#if defined(WIN32) && defined(_DEBUG)
#include <malloc.h>  
#ifndef _CRTDBG_MAP_ALLOC
# define _CRTDBG_MAP_ALLOC 1
#endif
#include <crtdbg.h>

#define _mesa_malloc(n)  _malloc_dbg( n, _NORMAL_BLOCK, __FILE__ , __LINE__ )
#define _mesa_calloc(n)  _calloc_dbg( 1, n, _NORMAL_BLOCK, __FILE__ , __LINE__ )
#define _mesa_free(p)    _free_dbg(p, _NORMAL_BLOCK )

extern void *_mesa_align_malloc_dbg(size_t bytes, unsigned long alignment, const char *_file, int _line );
extern void *_mesa_align_calloc_dbg(size_t bytes, unsigned long alignment, const char *_file, int _line );
extern void _mesa_align_free_dbg(void *ptr, const char *_file, int _line);

#define _mesa_align_malloc( s, a ) _mesa_align_malloc_dbg( s, a, __FILE__ , __LINE__ )
#define _mesa_align_calloc( s, a ) _mesa_align_calloc_dbg( s, a, __FILE__ , __LINE__ )
#define _mesa_align_free(p)        _mesa_align_free_dbg(p, __FILE__ , __LINE__)

#else /* WIN32 && _DEBUG */

extern void *_mesa_malloc(size_t bytes);
extern void *_mesa_calloc(size_t bytes);
extern void _mesa_free(void *ptr);

extern void *_mesa_align_malloc(size_t bytes, unsigned long alignment);
extern void *_mesa_align_calloc(size_t bytes, unsigned long alignment);
extern void _mesa_align_free(void *ptr);

#endif /* WIN32 && _DEBUG */


#ifdef DEBUG

/* call Mesa memory functions */
#define MALLOC(BYTES)      _mesa_malloc(BYTES)
#define CALLOC(BYTES)      _mesa_calloc(BYTES)
#define MALLOC_STRUCT(T)   (struct T *) _mesa_malloc(sizeof(struct T))
#define CALLOC_STRUCT(T)   (struct T *) _mesa_calloc(sizeof(struct T))
#define FREE(PTR)          _mesa_free(PTR)

#else

/* directly call C lib memory functions */
#define MALLOC(BYTES)      (void *) malloc(BYTES)
#define CALLOC(BYTES)      (void *) calloc(1, BYTES)
#define MALLOC_STRUCT(T)   (struct T *) malloc(sizeof(struct T))
#define CALLOC_STRUCT(T)   (struct T *) calloc(1,sizeof(struct T))
#define FREE(PTR)          free(PTR)

#endif

/* call Mesa N-byte aligned memory functions */
#define ALIGN_MALLOC(BYTES, N)     (void *) _mesa_align_malloc(BYTES, N)
#define ALIGN_CALLOC(BYTES, N)     (void *) _mesa_align_calloc(BYTES, N)
#define ALIGN_MALLOC_STRUCT(T, N)  (struct T *) _mesa_align_malloc(sizeof(struct T), N)
#define ALIGN_CALLOC_STRUCT(T, N)  (struct T *) _mesa_align_calloc(sizeof(struct T), N)
#define ALIGN_FREE(PTR)            _mesa_align_free(PTR)


#ifdef MESA_EXTERNAL_BUFFERALLOC
/*
 * If you want Mesa's depth/stencil/accum/etc buffers to be allocated
 * with a specialized allocator you can define MESA_EXTERNAL_BUFFERALLOC
 * and implement _ext_mesa_alloc/free_pixelbuffer() in your app.
 * Contributed by Gerk Huisma (gerk@five-d.demon.nl).
 */
extern void *_ext_mesa_alloc_pixelbuffer( unsigned int size );
extern void _ext_mesa_free_pixelbuffer( void *pb );

#define MESA_PBUFFER_ALLOC(BYTES)  (void *) _ext_mesa_alloc_pixelbuffer(BYTES)
#define MESA_PBUFFER_FREE(PTR)     _ext_mesa_free_pixelbuffer(PTR)
#else
/* Default buffer allocation uses the aligned allocation routines: */
#define MESA_PBUFFER_ALLOC(BYTES)  (void *) _mesa_align_malloc(BYTES, 512)
#define MESA_PBUFFER_FREE(PTR)     _mesa_align_free(PTR)
#endif


/* Memory copy: */
#ifdef SUNOS4
#define MEMCPY( DST, SRC, BYTES) \
	memcpy( (char *) (DST), (char *) (SRC), (int) (BYTES) )
#else
#define MEMCPY( DST, SRC, BYTES) \
	memcpy( (void *) (DST), (void *) (SRC), (size_t) (BYTES) )
#endif


/* Memory set: */
#ifdef SUNOS4
#define MEMSET( DST, VAL, N ) \
	memset( (char *) (DST), (int) (VAL), (int) (N) )
#else
#define MEMSET( DST, VAL, N ) \
	memset( (void *) (DST), (int) (VAL), (size_t) (N) )
#endif

extern void _mesa_memset16( GLushort *dst, GLushort val, size_t n );

#define MEMSET16( DST, VAL, N ) \
        _mesa_memset16( (GLushort *) (DST), (GLushort) (VAL), (size_t) (N) )


/* On some systems we might want to use bzero() (but is bzero portable?) */
#if defined(__FreeBSD__)
#define BZERO( ADDR, N ) \
	bzero( (void *) (ADDR), (size_t) (N) )
#else
#define BZERO( ADDR, N ) \
	memset( (void *) (ADDR), 0, (size_t) (N) )
#endif


/* MACs and BeOS don't support static larger than 32kb, so... */
#if defined(macintosh) && !defined(__MRC__)
/*extern char *AGLAlloc(int size);*/
/*extern void AGLFree(char* ptr);*/
#  define DEFARRAY(TYPE,NAME,SIZE)  			TYPE *NAME = (TYPE*)MALLOC(sizeof(TYPE)*(SIZE))
#  define DEFMARRAY(TYPE,NAME,SIZE1,SIZE2)		TYPE (*NAME)[SIZE2] = (TYPE(*)[SIZE2])MALLOC(sizeof(TYPE)*(SIZE1)*(SIZE2))
#  define DEFMNARRAY(TYPE,NAME,SIZE1,SIZE2,SIZE3)	TYPE (*NAME)[SIZE2][SIZE3] = (TYPE(*)[SIZE2][SIZE3])MALLOC(sizeof(TYPE)*(SIZE1)*(SIZE2)*(SIZE3))

#  define CHECKARRAY(NAME,CMD)				do {if (!(NAME)) {CMD;}} while (0)
#  define UNDEFARRAY(NAME)          			do {if ((NAME)) {FREE((char*)NAME);}  }while (0)
#elif defined(__BEOS__)
#  define DEFARRAY(TYPE,NAME,SIZE)  			TYPE *NAME = (TYPE*)malloc(sizeof(TYPE)*(SIZE))
#  define DEFMARRAY(TYPE,NAME,SIZE1,SIZE2)  		TYPE (*NAME)[SIZE2] = (TYPE(*)[SIZE2])malloc(sizeof(TYPE)*(SIZE1)*(SIZE2))
#  define DEFMNARRAY(TYPE,NAME,SIZE1,SIZE2,SIZE3)	TYPE (*NAME)[SIZE2][SIZE3] = (TYPE(*)[SIZE2][SIZE3])malloc(sizeof(TYPE)*(SIZE1)*(SIZE2)*(SIZE3))
#  define CHECKARRAY(NAME,CMD)				do {if (!(NAME)) {CMD;}} while (0)
#  define UNDEFARRAY(NAME)          			do {if ((NAME)) {free((char*)NAME);}  }while (0)
#else
#  define DEFARRAY(TYPE,NAME,SIZE)  			TYPE NAME[SIZE]
#  define DEFMARRAY(TYPE,NAME,SIZE1,SIZE2)		TYPE NAME[SIZE1][SIZE2]
#  define DEFMNARRAY(TYPE,NAME,SIZE1,SIZE2,SIZE3)	TYPE NAME[SIZE1][SIZE2][SIZE3]
#  define CHECKARRAY(NAME,CMD)				do {} while(0)
#  define UNDEFARRAY(NAME)
#endif




#endif
