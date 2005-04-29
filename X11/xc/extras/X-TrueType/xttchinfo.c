/* ===EmacsMode: -*- Mode: C; tab-width:4; c-basic-offset: 4; -*- === */
/* ===FileName: ===
   Copyright (c) 1997 Jyunji Takagi, All rights reserved.
   Copyright (c) 1998 Go Watanabe, All rights reserved.
   Copyright (c) 1998 Kazushi (Jam) Marukawa, All rights reserved.
   Copyright (c) 1998 X-TrueType Server Project, All rights reserved.
  
===Notice
   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions
   are met:
   1. Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
   2. Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.

   THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
   ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
   IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
   ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
   FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
   DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
   OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
   HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
   LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
   OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
   SUCH DAMAGE.

   Major Release ID: X-TrueType Server Version 1.3 [Aoi MATSUBARA Release 3]

Notice===
*/
/* $XFree86: xc/extras/X-TrueType/xttchinfo.c,v 1.2 2003/10/22 16:25:23 tsi Exp $ */

#include "xttversion.h"

#if 0
static char const * const releaseID =
    _XTT_RELEASE_NAME;
#endif

#include "xttcommon.h"
#include "fntfilst.h"
#include "xttcap.h"
#include "xttcconv.h"
#include "xttcache.h"
#include "xttstruct.h"

#define CharInfoPoolUnitSize 256

CharInfoPoolPtr
CharInfoPool_Alloc(void)
{
    CharInfoPoolPtr this = (CharInfoPoolPtr) xalloc(sizeof(*this));
    if (this != NULL) {
        /* Initialize CharInfoUnit portion */
        this->unit = NULL;
        this->n = this->index = 0;
        /* Initialize BitmapInfoUnit portion */
        this->bunit = NULL;
        this->bn = 0;
    }
    return this;
}

CharInfoPtr
CharInfoPool_Get(CharInfoPoolPtr this)
{
    CharInfoUnitPtr ptr;
    CharInfoPtr cptr;

    if ((this->n == 0) || (this->index == CharInfoPoolUnitSize)) {
        ptr = (CharInfoUnitPtr) xrealloc(this->unit, (this->n + 1) *
                                         sizeof(*ptr));
        if (ptr == NULL)
            return NULL;
        this->unit = ptr;
        
        cptr = (CharInfoPtr)xalloc(CharInfoPoolUnitSize * sizeof(CharInfoRec));
        if (cptr == NULL)
            return NULL;
        
        ptr = this->unit + this->n;
        ptr->charInfo   = cptr;
        
        this->n++;
        this->index = 0;
    } else
        ptr = this->unit + (this->n - 1);
    
    /* return new charInfo */
    ptr->charInfo[this->index].bits = NULL;
    return &ptr->charInfo[this->index++];
}

void
CharInfoPool_Set(CharInfoPoolPtr this, CharInfoPtr dat, int size)
{
    BitmapInfoUnitPtr ptr;
    char *bptr;
    int newsize;

    ptr = &this->bunit[this->bn - 1];
    if (this->bn == 0 || ptr->cur + size > ptr->bitmapsize) {
        /* make a new room. */
        ptr = (BitmapInfoUnitPtr) xrealloc(this->bunit, (this->bn + 1) *
                                           sizeof(*ptr));
        if (ptr == NULL)        
            return;
        this->bunit = ptr;
        
        newsize = 4096; /* magic number, 4KB */
        if (size > newsize / 2)
            newsize = size;
        
        bptr = (char *) xalloc(newsize);
        if (bptr == NULL)
            return;
        memset(bptr, 0, newsize);
        
        ptr = this->bunit + this->bn;
        ptr->bitmap     = bptr;
        ptr->bitmapsize = newsize;
        ptr->cur        = 0;
        
        this->bn++;
    }
  
    dat->bits = ptr->bitmap + ptr->cur;
    ptr->cur += size;
}       

void
CharInfoPool_Free(CharInfoPoolPtr this)
{
    int i;
    for (i = 0; i < this->n; i++)
        xfree(this->unit[i].charInfo);
    for (i = 0; i < this->bn; i++)
        xfree(this->bunit[i].bitmap);
    xfree(this->unit);
    xfree(this->bunit);
    this->unit  = NULL;
        this->bunit = NULL;
    this->n  = 0;
    this->bn = 0;
}


/* end of file */
