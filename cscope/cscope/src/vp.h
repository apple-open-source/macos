/*===========================================================================
 Copyright (c) 1998-2000, The Santa Cruz Operation 
 All rights reserved.
 
 Redistribution and use in source and binary forms, with or without
 modification, are permitted provided that the following conditions are met:

 *Redistributions of source code must retain the above copyright notice,
 this list of conditions and the following disclaimer.

 *Redistributions in binary form must reproduce the above copyright notice,
 this list of conditions and the following disclaimer in the documentation
 and/or other materials provided with the distribution.

 *Neither name of The Santa Cruz Operation nor the names of its contributors
 may be used to endorse or promote products derived from this software
 without specific prior written permission. 

 THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS ``AS
 IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE
 LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 INTERRUPTION)
 HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 DAMAGE. 
 =========================================================================*/

/* $Id: vp.h,v 1.1.1.2 2002/01/09 18:50:34 umeshv Exp $ */

/*
 *	VPATH assumptions:
 *		VPATH is the environment variable containing the view path 
 *		where each path name is followed by ':', '\n', or '\0'.
 *		Embedded blanks are considered part of the path.
 */

#define MAXPATH	200		/* max length for entire name */

#include <fcntl.h>
#include <sys/stat.h>

/* In view of DOS portability, we may need the vale of the O_BINARY
 * bit mask. On Unix platforms, it's not defined, nor is it needed -->
 * set it to a no-op value */
#ifndef O_BINARY
# ifdef _O_BINARY
#  define O_BINARY _O_BINARY
# else
#  define O_BINARY 0x00
# endif
#endif 

#if !NOMALLOC
extern	char	**vpdirs;	/* directories (including current) in view path */
#else
#define	MAXDIR	25		/* same as libVP */
#define	DIRLEN	80		/* same as libVP */
extern	char	vpdirs[MAXDIR][DIRLEN + 1];
#endif
extern	int	vpndirs;	/* number of directories in view path */

void	vpinit(char *currentdir);
int	vpopen(char *path, int oflag);
int	vpaccess(char *path, mode_t amode);
