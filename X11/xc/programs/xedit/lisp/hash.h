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

/* $XFree86: xc/programs/xedit/lisp/hash.h,v 1.2 2002/11/08 08:00:56 paulo Exp $ */

#ifndef Lisp_hash_h
#define Lisp_hash_h

#include "private.h"

typedef struct _LispHashEntry {
    LispObj **keys;
    LispObj **values;
    long cache;
    long count;
} LispHashEntry;

struct _LispHashTable {
    LispHashEntry *entries;
    long num_entries;
    long count;
    int function;		/* Function is EQ, EQL, EQUAL, or EQUALP */
    double rehash_size;
    double rehash_threshold;
};

/*
 * Prototypes
 */
void LispFreeHashTable(LispHashTable*);

LispObj *Lisp_Clrhash(LispBuiltin*);
LispObj *Lisp_Gethash(LispBuiltin*);
LispObj *Lisp_HashTableCount(LispBuiltin*);
LispObj *Lisp_HashTableP(LispBuiltin*);
LispObj *Lisp_HashTableRehashSize(LispBuiltin*);
LispObj *Lisp_HashTableRehashThreshold(LispBuiltin*);
LispObj *Lisp_HashTableSize(LispBuiltin*);
LispObj *Lisp_HashTableTest(LispBuiltin*);
LispObj *Lisp_Maphash(LispBuiltin*);
LispObj *Lisp_MakeHashTable(LispBuiltin*);
LispObj *Lisp_Remhash(LispBuiltin*);
LispObj *Lisp_XeditPuthash(LispBuiltin*);

#endif /* Lisp_hash_h */
