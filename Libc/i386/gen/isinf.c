/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
/*
 * Copyright (c) 1987 NeXT, INC.
 */

struct words {
#if defined(__BIG_ENDIAN__)
        unsigned int hi;
        unsigned int lo;
#else
        unsigned int lo;
        unsigned int hi;
#endif
};

union double_words {
        double d;
        struct words w;
};

/*
 * isinf -- returns 1 if positive IEEE infinity, -1 if negative
 *	    IEEE infinity, 0 otherwise.
 */
int
isinf(d)
double d;
{
        union double_words dw;
        dw.d = d;
        if (dw.w.hi == 0x7ff00000)
             return(1);
        if (dw.w.hi == 0xfff00000)
                return(-1);
        return(0);
}
