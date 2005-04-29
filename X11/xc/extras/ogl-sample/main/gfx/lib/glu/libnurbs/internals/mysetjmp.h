/*
** License Applicability. Except to the extent portions of this file are
** made subject to an alternative license as permitted in the SGI Free
** Software License B, Version 1.1 (the "License"), the contents of this
** file are subject only to the provisions of the License. You may not use
** this file except in compliance with the License. You may obtain a copy
** of the License at Silicon Graphics, Inc., attn: Legal Services, 1600
** Amphitheatre Parkway, Mountain View, CA 94043-1351, or at:
**
** http://oss.sgi.com/projects/FreeB
**
** Note that, as provided in the License, the Software is distributed on an
** "AS IS" basis, with ALL EXPRESS AND IMPLIED WARRANTIES AND CONDITIONS
** DISCLAIMED, INCLUDING, WITHOUT LIMITATION, ANY IMPLIED WARRANTIES AND
** CONDITIONS OF MERCHANTABILITY, SATISFACTORY QUALITY, FITNESS FOR A
** PARTICULAR PURPOSE, AND NON-INFRINGEMENT.
**
** Original Code. The Original Code is: OpenGL Sample Implementation,
** Version 1.2.1, released January 26, 2000, developed by Silicon Graphics,
** Inc. The Original Code is Copyright (c) 1991-2000 Silicon Graphics, Inc.
** Copyright in any portions created by third parties is as indicated
** elsewhere herein. All Rights Reserved.
**
** Additional Notice Provisions: The application programming interfaces
** established by SGI in conjunction with the Original Code are The
** OpenGL(R) Graphics System: A Specification (Version 1.2.1), released
** April 1, 1999; The OpenGL(R) Graphics System Utility Library (Version
** 1.3), released November 4, 1998; and OpenGL(R) Graphics with the X
** Window System(R) (Version 1.3), released October 19, 1998. This software
** was created using the OpenGL(R) version 1.2.1 Sample Implementation
** published by SGI, but has not been independently verified as being
** compliant with the OpenGL(R) version 1.2.1 Specification.
*/
/* $XFree86: xc/extras/ogl-sample/main/gfx/lib/glu/libnurbs/internals/mysetjmp.h,v 1.4 2003/10/22 19:20:57 tsi Exp $ */

/*
 * mysetjmp.h
 *
 * $Date$ $Revision$
 * $Header: //depot/main/gfx/lib/glu/libnurbs/internals/mysetjmp.h#4 $
 */

#ifndef __glumysetjmp_h_
#define __glumysetjmp_h_

#ifdef STANDALONE
struct JumpBuffer;
extern "C" JumpBuffer *newJumpbuffer( void );
extern "C" void deleteJumpbuffer(JumpBuffer *);
extern "C" void mylongjmp( JumpBuffer *, int );
extern "C" int mysetjmp( JumpBuffer * );
#endif

#ifdef GLBUILD
#define setjmp		gl_setjmp
#define longjmp 	gl_longjmp
#endif

#if defined(LIBRARYBUILD) || defined(GLBUILD)
#include <setjmp.h>
#include <stdlib.h>

/* Fix up for libc5 Linux systems */
#ifndef LIBC5BUILD
#if defined(setjmp) && defined(__GNU_LIBRARY__) && \
    (!defined(__GLIBC__) || (__GLIBC__ < 2))
#define LIBC5BUILD 1
#else
#if !LIBC5BUILD
#undef LIBC5BUILD
#endif
#endif
#endif

#ifndef LIBC5BUILD
struct JumpBuffer {
    jmp_buf	buf;
};
#else
struct JumpBuffer {
    union {
	jmp_buf		jbuf;
	sigjmp_buf	sbuf;
    } buf;
};
#endif

inline JumpBuffer *
newJumpbuffer( void )
{
    return (JumpBuffer *) malloc( sizeof( JumpBuffer ) );
}

inline void
deleteJumpbuffer(JumpBuffer *jb)
{
   free( (void *) jb);
}

inline void
mylongjmp( JumpBuffer *j, int code ) 
{
#ifndef LIBC5BUILD
    longjmp( j->buf, code );
#else
    longjmp( j->buf.jbuf, code);
#endif
}

inline int
mysetjmp( JumpBuffer *j )
{
#ifndef LIBC5BUILD
    return setjmp( j->buf );
#else
    __sigjmp_save( j->buf.sbuf, 1);
    return __setjmp( j->buf.sbuf->__jmpbuf );
#endif
}
#endif

#endif /* __glumysetjmp_h_ */
