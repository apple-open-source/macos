/* ===EmacsMode: -*- Mode: C; tab-width:4; c-basic-offset: 4; -*- === */
/* ===FileName: ===
   Copyright (c) 1998 Go Watanabe, All rights reserved.
   Copyright (c) 1998 Takuya SHIOZAKI, All rights reserved.
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

#include "xttversion.h"

static char const * const releaseID =
    _XTT_RELEASE_NAME;

#include "xttcommon.h"
#include "fontmisc.h"

/**************************************************************************
  Functions
 */

/* compare strings, ignoring case */
Bool /* False == equal, True == not equal */
mystrcasecmp(char const *s1, char const *s2)
{
    Bool result = True;
    
#if (defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__) ||\
     defined(__bsdi__)) && !defined(FONTMODULE)
    /* 4.4BSD has strcasecmp function. */
    result = strcasecmp(s1, s2) != 0;
#else
    {
        unsigned int len1 = strlen(s1);
        
        if (len1 == strlen(s2)) {
            int i;
            for (i=0; i<len1; i++) {
                if (toupper(*s1++) != toupper(*s2++))
                    goto quit;
            }
            result = False;
        } else
            /* len1 != len2 -> not equal*/
            ;
    }
  quit:
    ;
#endif

    return result;
}


/* strdup clone with using the allocator of X server */
char *
XttXstrdup(char const *str)
{
    char *result;
    
    result = (char *)xalloc(strlen(str)+1);

    if (result)
        strcpy(result, str);

    return result;
}


/* end of file */
