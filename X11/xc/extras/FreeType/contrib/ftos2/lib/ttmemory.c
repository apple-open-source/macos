/*******************************************************************
 *
 *  ttmemory.c                                               1.2
 *
 *    Memory management component (body).
 *
 *  Copyright 1996, 1997 by
 *  David Turner, Robert Wilhelm, and Werner Lemberg.
 *
 *  Portions Copyright 1998 by Michal Necasek
 *
 *  This file is part of the FreeType project, and may only be used
 *  modified and distributed under the terms of the FreeType project
 *  license, LICENSE.TXT.  By continuing to use, modify, or distribute
 *  this file you indicate that you have read the license and
 *  understand and accept it fully.
 *
 *
 *  Changes between 1.1 and 1.2:
 *
 *  - the font pool is gone.
 *
 *  - introduced the FREE macro and the Free function for
 *    future use in destructors.
 *
 *  - Init_FontPool() is now a macro to allow the compilation of
 *    'legacy' applications (all four test programs have been updated).
 *
 *  Note:  This code was slightly adapted for use in the OS/2
 *         Font Driver (FreeType/2).
 *
 ******************************************************************/
/* $XFree86: xc/extras/FreeType/contrib/ftos2/lib/ttmemory.c,v 1.2 2003/01/12 03:55:44 tsi Exp $ */

#include "ttdebug.h"
#include "ttmemory.h"
#include "ttengine.h"

#define INCL_DEV
#include <os2.h>
#include <pmddi.h>

#include <stdlib.h>

#undef  DEBUG_MEM

/* -------------------- debugging defs ----------------------- */
/* DEBUG_MEM creates a file and logs all actions to it         */

#ifdef DEBUG_MEM
  static HFILE MemLogHandle = NULLHANDLE;
  static ULONG Written      = 0;
  static char  log[2048]    = "";
  static char  buf[2048]    = "";


char*  itoa10( int i, char* buffer ) {
    char*  ptr  = buffer;
    char*  rptr = buffer;
    char   digit;

    if (i == 0) {
      buffer[0] = '0';
      buffer[1] =  0;
      return buffer;
    }

    if (i < 0) {
      *ptr = '-';
       ptr++; rptr++;
       i   = -i;
    }

    while (i != 0) {
      *ptr = (char) (i % 10 + '0');
       ptr++;
       i  /= 10;
    }

    *ptr = 0;  ptr--;

    while (ptr > rptr) {
      digit = *ptr;
      *ptr  = *rptr;
      *rptr = digit;
       ptr--;
       rptr++;
    }

    return buffer;
}

static const char*  hexstr = "0123456789abcdef";

char* itohex2( int i, char* buffer )
 {
  buffer[0] = hexstr[ (i >> 12) & 0xF ];
  buffer[1] = hexstr[ (i >> 8 ) & 0xF ];
  buffer[2] = hexstr[ (i >> 4 ) & 0xF ];
  buffer[3] = hexstr[ (i      ) & 0xF ];
  buffer[4] = '\0';
  return buffer;
}

char* itohex4( long i, char* buffer )
{
  itohex2( (i >> 16) & 0xFFFF, buffer );
  /* We separate the high and low part with a dot to make it */
  /* more readable                                           */
  buffer[4] = '.';
  itohex2( i & 0xFFFF, buffer+5 );
  return buffer;
}

#   define  COPY(s)     strcpy(log, s)
#   define  CAT(s)      strcat(log, s)
#   define  CATI(v)     strcat(log, itoa10( (int)v, buf ))
#   define  CATH(v)     strcat(log, itohex4( (long)v, buf ))
#   define  CATW(v)     strcat(log, itohex2( (short)v, buf ))
#   define  WRITE       DosWrite(MemLogHandle, log, strlen(log), &Written)
#   define  ERRRET(e)   { COPY("Error at ");  \
                          CATI(__LINE__);    \
                          CAT("\r\n");       \
                          WRITE;             \
                          return(e);         \
                       }

#else

#   define  COPY(s)
#   define  CAT(s)
#   define  CATI(v)
#   define  CATH(v)
#   define  CATW(v)
#   define  WRITE
#   define  ERRRET(e)  return(e);

#endif /* DEBUG_MEM */


#undef  TRACK_MEM
/* TRACK_MEM allows online tracking of memory usage online (via shared */
/* memory). It is used in conjunction with the FTMEM utility.          */

#ifdef TRACK_MEM
   /* name of shared memory used for memory usage reporting  */
#  define MEM_NAME  "\\sharemem\\freetype"

   /* structure containing memory usage information */
   typedef struct _INFOSTRUCT {
      ULONG  signature;      /* signature (0x46524545, 'FREE') */
      ULONG  used;           /* bytes actually used */
      ULONG  maxused;        /* maximum amount ever used */
      ULONG  num_err;        /* number of (de)allocation errors */
   } INFOSTRUCT, *PINFOSTRUCT;

   /* structure (in named shared memory) pointing to the above struct */
   typedef struct _INFOPTR {
      PINFOSTRUCT  address;        /* pointer to actual memory info  */
   } INFOPTR, *PINFOPTR;

   PINFOSTRUCT  meminfo;      /* struct in shared memory holding usage info */
   PINFOPTR     memptr;
#endif

/* -----------------------------------------------------------------

   A brief explanation of the memory allocator :

   - We store the block's size in front of it. The size implies the nature
     of the block, and selects a de-allocation scheme..

   - A note on the memory debugging schemes: logging and online tracking
     are independent of each other and none, either or both may be used.

   ----------------------------------------------------------------- */

  /****************************************************************/
  /*                                                              */
  /* Allocate a block of memory                                   */
  /*                                                              */
  static
  void*  ft2_malloc( long size )
  {
    long*  head;
    void*  base;
    int    rc;

    /* add header size */
    size += sizeof(long);

    /* Allocate memory accessible from all processes */
    if (( rc = SSAllocMem( (PVOID)&head, size, 0 )))
    {
      COPY( "ft2_malloc: block SSAllocMem failed with rc = " );
      CATH( rc );
      CAT ( "\r\n" );
      WRITE;
      return NULL;
    }
    *head = size;
    base  = (void*)(head + 1);
#   ifdef TRACK_MEM
       meminfo->used += size;
       if (meminfo->used > meminfo->maxused)
          meminfo->maxused = meminfo->used;
#   endif
    return base;
  }

  /****************************************************************/
  /*                                                              */
  /* Release a block of memory                                    */
  /*                                                              */
  int  ft2_free( void* block )
  {
    long*  head;
    long   size, offset;
    int    rc, h;

    if (!block)
      return -1;

    head = ((long*)block) - 1;
    size = *head;

    if (size <= 0)
    {
      COPY( "ft2_free: negative size !!\r\n" );
      WRITE;
      return -1;
    }

    rc = SSFreeMem( (PVOID)head );
    if (rc)
    {
      COPY( "ft2_free: block SSFreeMem failed with rc = " );
      CATI( rc );
      CAT ( "\r\n" );
      WRITE;
    }
#   ifdef TRACK_MEM
       meminfo->used -= size;
#   endif
    return rc;
  }

/*******************************************************************
 *
 *  Function    :  TT_Alloc
 *
 *  Description :  Allocates memory from the heap buffer.
 *
 *  Input  :  Size      size of the memory to be allocated
 *            P         pointer to a buffer pointer
 *
 *  Output :  Error code.
 *
 *  NOTE:  The newly allocated block should _always_ be zeroed
 *         on return.  Many parts of the engine rely on this to
 *         work properly.
 *
 ******************************************************************/

  TT_Error  TT_Alloc( long  Size, void**  P )
  {
    if ( Size )
    {
      *P = ft2_malloc( Size );
      if (!*P) {
#       ifdef TRACK_MEM
           meminfo->num_err++;
#       endif
        return TT_Err_Out_Of_Memory;
      }

      /* MEM_Set( *P, 0, Size); */ /* not necessary, SSAllocMem does it */
    }
    else
      *P = NULL;

    return TT_Err_Ok;
  }


/*******************************************************************
 *
 *  Function    :  TT_Free
 *
 *  Description :  Releases a previously allocated block of memory.
 *
 *  Input  :  P    pointer to memory block
 *
 *  Output :  Always SUCCESS.
 *
 *  Note : The pointer must _always_ be set to NULL by this function.
 *
 ******************************************************************/

  TT_Error  TT_Free( void**  P )
  {
    if ( !P || !*P )
      return TT_Err_Ok;

    if (ft2_free( *P )) {
#       ifdef TRACK_MEM
           meminfo->num_err++;
#       endif
    }
    *P = NULL;
    return TT_Err_Ok;
  }


/*******************************************************************
 *
 *  Function    :  TTMemory_Init
 *
 *  Description :  Initializes the memory.
 *
 *  Output :  Always SUCCESS.
 *
 ******************************************************************/

  TT_Error  TTMemory_Init()
  {
    int  rc;

#   ifdef DEBUG_MEM
       ULONG  Action;

       DosOpen("C:\\FTMEM.LOG", &MemLogHandle, &Action, 0, FILE_NORMAL,
               OPEN_ACTION_CREATE_IF_NEW | OPEN_ACTION_REPLACE_IF_EXISTS,
               OPEN_FLAGS_NO_CACHE | OPEN_FLAGS_WRITE_THROUGH |
               OPEN_FLAGS_SEQUENTIAL | OPEN_SHARE_DENYWRITE | OPEN_ACCESS_WRITEONLY,
               NULL);

       COPY("FTMEM Init.\r\n");
       WRITE;

#   endif /* DEBUG */

#   ifdef TRACK_MEM
    /* allocate named shared memory and global shared memory */

       SSAllocMem(&meminfo, 4096, 0);
       DosAllocSharedMem((PVOID*)&memptr, MEM_NAME, 4096, fALLOC);
       memptr->address = meminfo;
       meminfo->signature = 0x46524545;   /* 'FREE' */
       meminfo->maxused = 0;
       meminfo->used = 0;
#   endif /* TRACK */

    return TT_Err_Ok;
  }


/*******************************************************************
 *
 *  Function    :  TTMemory_Done
 *
 *  Description :  Finalizes memory usage.
 *
 *  Output :  Always SUCCESS.
 *
 ******************************************************************/

  TT_Error  TTMemory_Done()
  {
    /* Never called by the font driver (beats me why). We do not
       release the heaps */

#   ifdef TRACK_MEM
       DosFreeMem(memptr);  /* free shared memory */
       SSFreeMem(meminfo);
#   endif
#   ifdef DEBUG_MEM
       COPY("FTMEM Done.\r\n");
       WRITE;
       DosClose(MemLogHandle);
#   endif
    return TT_Err_Ok;
  }


/* END */
