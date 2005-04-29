/* $XFree86: xc/programs/Xserver/cfb/cfb32.h,v 1.5 2004/02/13 23:58:30 dawes Exp $ */
/*
 * Copyright (C) 1994-1998 The XFree86 Project, Inc.
 * All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject
 * to the following conditions:
 *
 *   1.  Redistributions of source code must retain the above copyright
 *       notice, this list of conditions, and the following disclaimer.
 *
 *   2.  Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer
 *       in the documentation and/or other materials provided with the
 *       distribution, and in the same place and form as other copyright,
 *       license and disclaimer information.
 *
 *   3.  The end-user documentation included with the redistribution,
 *       if any, must include the following acknowledgment: "This product
 *       includes software developed by The XFree86 Project, Inc
 *       (http://www.xfree86.org/) and its contributors", in the same
 *       place and form as other third-party acknowledgments.  Alternately,
 *       this acknowledgment may appear in the software itself, in the
 *       same form and location as other such third-party acknowledgments.
 *
 *   4.  Except as contained in this notice, the name of The XFree86
 *       Project, Inc shall not be used in advertising or otherwise to
 *       promote the sale, use or other dealings in this Software without
 *       prior written authorization from The XFree86 Project, Inc.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESSED OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE XFREE86 PROJECT, INC OR ITS CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
 * OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _CFB32_H_
#define _CFB32_H_

/*
 * C's preprocessing language substitutes >text<, not values...
 */

#ifdef OLDPSZ
# undef OLDPSZ
#endif

#ifdef PSZ

# if (PSZ == 8)
#  define OLDPSZ 8
# endif

# if (PSZ == 16)
#  define OLDPSZ 16 
# endif

# if (PSZ == 24)
#  define OLDPSZ 24 
# endif

# if (PSZ == 32)
#  define OLDPSZ 32 
# endif

# ifndef OLDPSZ
   /* Maybe an #error here ? */
# endif

# undef PSZ

#endif

#define PSZ 32
#define CFB_PROTOTYPES_ONLY
#include "cfb.h"
#undef CFB_PROTOTYPES_ONLY
#include "cfbunmap.h"

#undef PSZ
#ifdef OLDPSZ

# if (OLDPSZ == 8)
#  define PSZ 8
# endif

# if (OLDPSZ == 16)
#  define PSZ 16 
# endif

# if (OLDPSZ == 24)
#  define PSZ 24 
# endif

# if (OLDPSZ == 32)
#  define PSZ 32 
# endif

# undef OLDPSZ

#endif

#endif /* _CFB32_H_ */
