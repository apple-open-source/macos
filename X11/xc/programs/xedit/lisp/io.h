/*
 * Copyright (c) 2002 by The XFree86 Project, Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *  
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE XFREE86 PROJECT BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF
 * OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * Except as contained in this notice, the name of the XFree86 Project shall
 * not be used in advertising or otherwise to promote the sale, use or other
 * dealings in this Software without prior written authorization from the
 * XFree86 Project.
 *
 * Author: Paulo César Pereira de Andrade
 */

/* $XFree86: xc/programs/xedit/lisp/io.h,v 1.8 2002/12/06 03:25:27 paulo Exp $ */

#ifndef Lisp_io_h
#define Lisp_io_h

#include "private.h"

#define	FILE_READ	0x01
#define FILE_WRITE	0x02
#define FILE_IO		0x03
#define FILE_APPEND	0x06	/* append mode, write bit also set */
#define FILE_BUFFERED	0x08	/* force buffered mode */
#define FILE_UNBUFFERED	0x10	/* force unbuffered mode */
#define FILE_BINARY	0x20

/*
 * Types
 */
typedef ssize_t (*io_write_fn)(int, const void*, size_t);

struct _LispFile {
    char *buffer;
    int line;			/* input line number */
    int column;			/* output column number */
    int descriptor;
    int length;			/* number of bytes used */
    int offset;			/* read/write offset */
    int unget : 8;		/* unread char */
    int readable : 1;
    int writable : 1;
    int regular : 1;		/* regular file */
    int buffered : 1;
    int available : 1;		/* unget field holds a char */
    int nonblock : 1;		/* in nonblock mode */
    int binary : 1;		/* if set, don't calculate column/line-number */
    io_write_fn io_write;
};

struct _LispString {
    char *string;
    int line;			/* input line number */
    int column;			/* output column number */
    int space;			/* number of bytes alocated */
    int length;			/* number of bytes used */
    int input;			/* input offset, for read operations */
    int output;			/* output offset, for write operations */
    int fixed : 1;		/* if set, don't try to reallocate string */
    int binary : 1;		/* if set, don't calculate column/line-number */
};

/*
 * Prototypes
 */
	/* higher level functions */
int LispGet(void);
int LispUnget(int);
void LispPushInput(LispObj*);
void LispPopInput(LispObj*);

	/* functions that read/write using the LispFile structure */
LispFile *LispFdopen(int, int);
LispFile *LispFopen(char*, int);
void LispFclose(LispFile*);
int LispFflush(LispFile*);
int LispFungetc(LispFile*, int);
int LispFgetc(LispFile*);
int LispFputc(LispFile*, int);
char *LispFgets(LispFile*, char*, int);
int LispFputs(LispFile*, char*);
int LispFread(LispFile*, void*, int);
int LispFwrite(LispFile*, void*, int);
int LispRename(char*, char*);
int LispUnlink(char*);

	/* io wrappers */
io_write_fn LispSetFileWrite(LispFile*, io_write_fn);

	/* functions that read/write using the LispString structure */
int LispSgetc(LispString*);
int LispSputc(LispString*, int);
int LispSputs(LispString*, char*);
int LispSwrite(LispString*, void*, int);

char *LispGetSstring(LispString*, int*);

#endif /* Lisp_io_h */
