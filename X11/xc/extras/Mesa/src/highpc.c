
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


#ifdef PC_HEADER
#include "all.h"
#else
#include "glheader.h"
#include "mtypes.h"
#endif


/*
 * This is the highest address in Mesa
 */
extern void mesa_highpc(void);  /* silence compiler warning */
void mesa_highpc(void) { }

#if defined(__GNUC__) && defined(__linux__)

void monstartup( char *lowpc, char *highpc );
void _mcleanup( void );
void mesa_lowpc( void );
void mesa_highpc( void );

static int profile = 0;

extern void force_init_prof( void ); /* silence compiler warning */
void force_init_prof( void )
{
   FILE *fp;

   if (profile) return;

   profile = 1;

   monstartup( (char *)mesa_lowpc, (char *)mesa_highpc );

   fprintf(stderr, "Starting profiling, %x %x\n",
	   (unsigned int)mesa_lowpc,
	   (unsigned int)mesa_highpc);

   if ((fp = fopen( "mesa_lowpc", "w" )) != NULL) {
      fprintf( fp, "0x%08x ", (unsigned int)mesa_lowpc );
      fclose( fp );
   }
}

/*
 * Start profiling
 */
extern void init_prof(void); /* silence compiler warning */
void __attribute__ ((constructor))
init_prof( void )
{
   FILE *fp;
   char *s = getenv("MESA_MON");

   if (s == NULL || atoi(s) == 0)
      return;

   profile = 1;

   monstartup( (char *)mesa_lowpc, (char *)mesa_highpc );

      fprintf(stderr, "Starting profiling, %x %x\n",
	      (unsigned int)mesa_lowpc,
	      (unsigned int)mesa_highpc);

   if ((fp = fopen( "mesa_lowpc", "w" )) != NULL) {
      fprintf( fp, "0x%08x ", (unsigned int)mesa_lowpc );
      fclose( fp );
   }
}



/*
 * Finish profiling
 */
extern void fini_prof(void);  /* silence compiler warning */
void __attribute__ ((destructor))
fini_prof( void )
{
   if (profile) {
      _mcleanup( );

	 fprintf(stderr, "Finished profiling\n");
   }
}

#else

void force_init_prof( void )
{
}

#endif
