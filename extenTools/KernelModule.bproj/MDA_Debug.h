/*
 * Copyright (c) 2000 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.1 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
/* Copyright (c) 1994-1996 NeXT Software, Inc.  All rights reserved. 
 *
 * MDA_Debug.h - This file contains definitions for using a Monochrome
 *               Display Adapter (MDA) as a synchronous debug monitor.
 *
 * HISTORY
 * 23-June-94	Dean Reece at NeXT
 *      Created.
 */

#ifdef DEBUG


/* Region Type Codes (pick one) */
#define MDA_TYPE_FIELD		0x00
#define	MDA_TYPE_WRAP		0x01
#define	MDA_TYPE_CSCROLL	0x02
#define	MDA_TYPE_LSCROLL	0x03

/* Base Attribute Codes (pick one) */
#define MDA_ATTR_UNDERLINE	0x01
#define MDA_ATTR_NORMAL		0x07
#define MDA_ATTR_REVERSE	0x70

/* Optional attribute codes (none or more may be or'd into a Base Attr Code) */
#define MDA_ATTR_BOLD		0x08
#define MDA_ATTR_BLINK		0x80

#define ROW(r) ((r)*160)
#define COL(c) ((c)*2)
#define POS(r,c) (ROW(r) + COL(c))

typedef struct {
    unsigned int	start, end, cursor, type;
    unsigned char	attr, *label;
} MDA_Region_t;


#import <bsd/string.h>
#define MDA_MEM_BASE ((char*)0x0b0000)
extern MDA_Region_t MDA_Region[];


#define MDA_PutChar(r,c) MDA_PutCharAttr((r),(c),MDA_Region[r].attr)
static inline void
MDA_PutCharAttr(int r, unsigned c, unsigned char attr)
{
    register int i, l;

    if ((MDA_Region[r].type != MDA_TYPE_FIELD) ||
        (MDA_Region[r].cursor <= MDA_Region[r].end)) {
        MDA_MEM_BASE[MDA_Region[r].cursor++] = c;
        MDA_MEM_BASE[MDA_Region[r].cursor++] = attr;
        if (MDA_Region[r].cursor > MDA_Region[r].end) {
            if (MDA_Region[r].label != NULL)
                l = strlen(MDA_Region[r].label) << 1;
            else
                l = 0;
            switch (MDA_Region[r].type) {
              case MDA_TYPE_FIELD :
              case MDA_TYPE_WRAP :
                MDA_Region[r].cursor = MDA_Region[r].start + l;
                break;
              case MDA_TYPE_CSCROLL :
                bcopy (MDA_MEM_BASE+MDA_Region[r].start+2+l,
                       MDA_MEM_BASE+MDA_Region[r].start+l,
                       (MDA_Region[r].end-(MDA_Region[r].start+l)));
                MDA_Region[r].cursor -= 2;
                break;
              case MDA_TYPE_LSCROLL :
                bcopy (MDA_MEM_BASE+MDA_Region[r].start+160+l,
                       MDA_MEM_BASE+MDA_Region[r].start+l,
                       (MDA_Region[r].end-MDA_Region[r].start-158-l));
                MDA_Region[r].cursor -= 160;
                for (i=MDA_Region[r].cursor; i<=MDA_Region[r].end;) {
                    MDA_MEM_BASE[i++]=0x20;
                    MDA_MEM_BASE[i++]=MDA_Region[r].attr;
                }
                break;
            }
        }
    }
}

static inline void
MDA_UpdateCursor(int r)
{
    switch (MDA_Region[r].type) {
      case MDA_TYPE_FIELD :
        MDA_Region[r].cursor = MDA_Region[r].start;
        break;
      case MDA_TYPE_WRAP :
      case MDA_TYPE_CSCROLL :
      case MDA_TYPE_LSCROLL :
        MDA_MEM_BASE[MDA_Region[r].cursor] = 0xae;
        MDA_MEM_BASE[MDA_Region[r].cursor+1] = MDA_ATTR_REVERSE;
        break;
    }
}


static inline void
MDA_SetAttribute(int r, unsigned char attr)
{
    MDA_Region[r].attr = (unsigned char)attr;
}

#define MDA_PrintChar(r,c) MDA_PrintCharAttr((r),(c),MDA_Region[r].attr)
static inline void
MDA_PrintCharAttr(int r, unsigned char c, unsigned char attr)
{
    MDA_PutCharAttr(r, c, attr);
    MDA_UpdateCursor(r);
}

#define MDA_PrintString(r,s) MDA_PrintStringAttr((r),(s),MDA_Region[r].attr)
static inline void
MDA_PrintStringAttr(int r, unsigned char *s, unsigned char attr)
{
    while (*s)
        MDA_PutCharAttr(r, *s++, attr);
    MDA_UpdateCursor(r);
}


#define MDA_PrintByte(r,d) MDA_PrintByteAttr((r),(d),MDA_Region[r].attr)
static inline void
MDA_PrintByteAttr(int r, unsigned char d, unsigned char attr)
{
    const char cvt[] = "0123456789abcdef";
    MDA_PutCharAttr(r, cvt[0x0f & (d >> 4)], attr);
    MDA_PutCharAttr(r, cvt[0x0f &  d], attr);
    MDA_UpdateCursor(r);
}

#define MDA_PrintWord(r,d) MDA_PrintWordAttr((r),(d),MDA_Region[r].attr)
static inline void
MDA_PrintWordAttr(int r, unsigned short d, unsigned char attr)
{
    const char cvt[] = "0123456789abcdef";
    MDA_PutCharAttr(r, cvt[0x0f & (d >> 12)], attr);
    MDA_PutCharAttr(r, cvt[0x0f & (d >>  8)], attr);
    MDA_PutCharAttr(r, cvt[0x0f & (d >>  4)], attr);
    MDA_PutCharAttr(r, cvt[0x0f &  d], attr);
    MDA_UpdateCursor(r);
}

#define MDA_PrintLong(r,d) MDA_PrintLongAttr((r),(d),MDA_Region[r].attr)
static inline void
MDA_PrintLongAttr(int r, unsigned long d, unsigned char attr)
{
    const char cvt[] = "0123456789abcdef";
    MDA_PutCharAttr(r, cvt[0x0f & (d >> 28)], attr);
    MDA_PutCharAttr(r, cvt[0x0f & (d >> 24)], attr);
    MDA_PutCharAttr(r, cvt[0x0f & (d >> 20)], attr);
    MDA_PutCharAttr(r, cvt[0x0f & (d >> 16)], attr);
    MDA_PutCharAttr(r, cvt[0x0f & (d >> 12)], attr);
    MDA_PutCharAttr(r, cvt[0x0f & (d >>  8)], attr);
    MDA_PutCharAttr(r, cvt[0x0f & (d >>  4)], attr);
    MDA_PutCharAttr(r, cvt[0x0f &  d], attr);
    MDA_UpdateCursor(r);
}

#define MDA_PrintNumber(r,n) MDA_PrintNumberAttr((r),(n),MDA_Region[r].attr)
static inline void
MDA_PrintNumberAttr(int r, unsigned long n, unsigned char attr)
{
    if (n>=10)
        MDA_PrintNumberAttr(r, n/10, attr);
    MDA_PrintCharAttr(r, '0'+(n%10), attr);
}

#define MDA_PrintSigned(r,n) MDA_PrintSignedAttr((r),(n),MDA_Region[r].attr)
static inline void
MDA_PrintSignedAttr(int r, signed long n, unsigned char attr)
{
    if (n<0) {
        MDA_PrintCharAttr(r, '-', attr);
	MDA_PrintNumberAttr(r, (((unsigned long)~(n))+1), attr);
    } else
	MDA_PrintNumberAttr(r, ((unsigned long)(n)), attr);
}

static inline void
MDA_ResetRegion(int r)
{
    register int i;
    for (i=MDA_Region[r].start; i<=MDA_Region[r].end;) {
        MDA_MEM_BASE[i++]=0x20;
        MDA_MEM_BASE[i++]=MDA_Region[r].attr;
    }
    MDA_Region[r].cursor = MDA_Region[r].start;
    if (MDA_Region[r].label)
        MDA_PrintStringAttr(r, MDA_Region[r].label, MDA_Region[r].attr);
    MDA_UpdateCursor(r);
}

static inline void
MDA_Reset(int rc)
{
    register int i;

    for (i=0; i<rc; i++)
        MDA_ResetRegion(i);
}


#else

#define MDA_Reset(rc)		   {}
#define MDA_ResetRegion(r)	   {}
#define MDA_SetAttribute(r,a)	   {}
#define MDA_PrintChar(r,c)	   {}
#define MDA_PrintString(r,s)	   {}
#define MDA_PrintByte(r,d)	   {}
#define MDA_PrintWord(r,d)	   {}
#define MDA_PrintLong(r,d)	   {}
#define MDA_PrintNumber(r,n)	   {}
#define MDA_PrintSigned(r,n)	   {}
#define MDA_PrintCharAttr(r,c,a)   {}
#define MDA_PrintStringAttr(r,s,a) {}
#define MDA_PrintByteAttr(r,d,a)   {}
#define MDA_PrintWordAttr(r,d,a)   {}
#define MDA_PrintLongAttr(r,d,a)   {}
#define MDA_PrintNumberAttr(r,n,a) {}
#define MDA_PrintSignedAttr(r,n,a) {}

#endif
