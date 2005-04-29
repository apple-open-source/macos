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

/* $Id: library.h,v 1.2 2004/07/09 21:34:45 nicolai Exp $ */

#ifndef CSCOPE_LIBRARY_H
#define CSCOPE_LIBRARY_H

#include <stdio.h>		/* need FILE* type def. */

/* private library */
char	*compath(char *pathname);
char	*egrepinit(char *egreppat);
char	*logdir(char *name);
char	*mybasename(char *path);
void	*mycalloc(int nelem, int size);
FILE	*myfopen(char *path, char *mode);
char	*mygetenv(char *variable, char *deflt);
void	*mymalloc(int size);
int	myopen(char *path, int flag, int mode);
void	*myrealloc(void *p, int size);
char	*stralloc(char *s);
FILE	*mypopen(char *cmd, char *mode);
int	mypclose(FILE *ptr);
FILE	*vpfopen(char *filename, char *type);
void	egrepcaseless(int i);

/* Programmer's Workbench library (-lPW) */
/* FIXME HBB 20010705: these should never be here. We should just
 * #include the relevant header, instead.  Moreover, they don't seem
 * to be used, anyway ... */
#if 0
char	*regcmp(), *regex();
#endif 
#endif /* CSCOPE_LIBRARY_H */
