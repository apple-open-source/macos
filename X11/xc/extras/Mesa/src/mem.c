
/*
 * Mesa 3-D graphics library
 * Version:  3.5
 *
 * Copyright (C) 1999-2001  Brian Paul   All Rights Reserved.
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


/*
 * Memory allocation functions.  Called via the MALLOC, CALLOC and
 * FREE macros when DEBUG symbol is defined.
 * You might want to set breakpoints on these functions or plug in
 * other memory allocation functions.  The Mesa sources should only
 * use the MALLOC and FREE macros (which could also be overriden).
 */

#ifdef PC_HEADER
#include "all.h"
#else
#include "glheader.h"
#include "config.h"
#include "macros.h"
#include "mem.h"
#endif


#if defined(WIN32) && defined(_DEBUG)

/*
 * N-byte aligned memory allocation functions.  Called via the ALIGN_MALLOC,
 * ALIGN_CALLOC and ALIGN_FREE macros.  Debug versions?
 * These functions allow dynamically allocated memory to be correctly
 * aligned for improved cache utilization and specialized assembly
 * support.
 */

/*
 * Allocate N-byte aligned memory (uninitialized)
 */
void *
_mesa_align_malloc_dbg(size_t bytes, unsigned long alignment, const char *_file, int _line )
{
   unsigned long ptr, buf;

   ASSERT( alignment > 0 );

   ptr = (unsigned long) _malloc_dbg( bytes + alignment, _NORMAL_BLOCK, _file, _line );

   buf = (ptr + alignment) & ~(unsigned long)(alignment - 1);
   *(unsigned long *)(buf - sizeof(void *)) = ptr;

#ifdef DEBUG
   /* mark the non-aligned area */
   while ( ptr < buf - sizeof(void *) ) {
      *(unsigned long *)ptr = 0xcdcdcdcd;
      ptr += sizeof(unsigned long);
   }
#endif

   return (void *)buf;
}


/*
 * Allocate N-byte aligned memory and initialize to zero
 */
void *
_mesa_align_calloc_dbg(size_t bytes, unsigned long alignment, const char *_file, int _line)
{
   unsigned long ptr, buf;

   ASSERT( alignment > 0 );

   ptr = (unsigned long) _calloc_dbg( 1, bytes + alignment, _NORMAL_BLOCK, _file, _line );

   buf = (ptr + alignment) & ~(unsigned long)(alignment - 1);
   *(unsigned long *)(buf - sizeof(void *)) = ptr;

#ifdef DEBUG
   /* mark the non-aligned area */
   while ( ptr < buf - sizeof(void *) ) {
      *(unsigned long *)ptr = 0xcdcdcdcd;
      ptr += sizeof(unsigned long);
   }
#endif

   return (void *)buf;
}


/*
 * Free N-byte aligned memory
 */
void
_mesa_align_free_dbg(void *ptr, const char *_file, int _line)
{
   /* The actuall address to free is stuffed in the word immediately
    * before the address the client sees.
    */
   void **cubbyHole = (void **) ((char *) ptr - sizeof(void *));
   void *realAddr = *cubbyHole;
   _free_dbg(realAddr, _NORMAL_BLOCK );
}


#else  /* WIN32 && _DEBUG */


/*
 * Allocate memory (uninitialized)
 */
void *
_mesa_malloc(size_t bytes)
{
   return malloc(bytes);
}


/*
 * Allocate memory and initialize to zero.
 */
void *
_mesa_calloc(size_t bytes)
{
   return calloc(1, bytes);
}


/*
 * Free memory
 */
void
_mesa_free(void *ptr)
{
   free(ptr);
}



/*
 * N-byte aligned memory allocation functions.  Called via the ALIGN_MALLOC,
 * ALIGN_CALLOC and ALIGN_FREE macros.  Debug versions?
 * These functions allow dynamically allocated memory to be correctly
 * aligned for improved cache utilization and specialized assembly
 * support.
 */


/*
 * Allocate N-byte aligned memory (uninitialized)
 */
void *
_mesa_align_malloc(size_t bytes, unsigned long alignment)
{
   unsigned long ptr, buf;

   ASSERT( alignment > 0 );

   ptr = (unsigned long) MALLOC( bytes + alignment );

   buf = (ptr + alignment) & ~(unsigned long)(alignment - 1);
   *(unsigned long *)(buf - sizeof(void *)) = ptr;

#ifdef DEBUG
   /* mark the non-aligned area */
   while ( ptr < buf - sizeof(void *) ) {
      *(unsigned long *)ptr = 0xcdcdcdcd;
      ptr += sizeof(unsigned long);
   }
#endif

   return (void *)buf;
}


/*
 * Allocate N-byte aligned memory and initialize to zero
 */
void *
_mesa_align_calloc(size_t bytes, unsigned long alignment)
{
   unsigned long ptr, buf;

   ASSERT( alignment > 0 );

   ptr = (unsigned long) CALLOC( bytes + alignment );

   buf = (ptr + alignment) & ~(unsigned long)(alignment - 1);
   *(unsigned long *)(buf - sizeof(void *)) = ptr;

#ifdef DEBUG
   /* mark the non-aligned area */
   while ( ptr < buf - sizeof(void *) ) {
      *(unsigned long *)ptr = 0xcdcdcdcd;
      ptr += sizeof(unsigned long);
   }
#endif

   return (void *)buf;
}


/*
 * Free N-byte aligned memory
 */
void
_mesa_align_free(void *ptr)
{
#if 0
   FREE( (void *)(*(unsigned long *)((unsigned long)ptr - sizeof(void *))) );
#else
   /* The actuall address to free is stuffed in the word immediately
    * before the address the client sees.
    */
   void **cubbyHole = (void **) ((char *) ptr - sizeof(void *));
   void *realAddr = *cubbyHole;
   FREE(realAddr);
#endif
}


#endif  /* WIN32 && _DEBUG */


/*
 * Set a block of GLushorts to a particular value.
 */
void
_mesa_memset16( GLushort *dst, GLushort val, size_t n )
{
   while (n-- > 0)
      *dst++ = val;
}
