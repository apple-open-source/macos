/* $XFree86: xc/programs/xfs/include/difs.h,v 1.5 2004/02/13 23:58:53 dawes Exp $ */
/*
 * Copyright (C) 1999 The XFree86 Project, Inc.
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
#ifndef _DIFS_H
#define _DIFS_H

#include "difsfn.h"
#include "globals.h"
#include "fontstruct.h"

typedef int (*InitialFunc)(ClientPtr);
typedef int (*ProcFunc)(ClientPtr);
typedef int (*SwappedProcFunc)(ClientPtr);
typedef void (*EventSwapFunc)(fsError *, fsError *);
typedef void (*ReplySwapFunc)(ClientPtr, int, void *);

extern InitialFunc InitialVector[3];
extern ReplySwapFunc ReplySwapVector[NUM_PROC_VECTORS];

/* FIXME: this is derived from fontstruct.h; should integrate it */

/* difs/atom.c */
extern Atom MakeAtom ( char *string, unsigned len, Bool makeit );
extern int ValidAtom ( Atom atom );
extern char * NameForAtom ( Atom atom );
extern void InitAtoms ( void );

/* difs/charinfo.c */
extern int GetExtents ( ClientPtr client, FontPtr pfont, Mask flags, unsigned long num_ranges, fsRange *range, unsigned long *num_extents, fsXCharInfo **data );
extern int GetBitmaps ( ClientPtr client, FontPtr pfont, fsBitmapFormat format, Mask flags, unsigned long num_ranges, fsRange *range, int *size, unsigned long *num_glyphs, fsOffset32 **offsets, pointer *data, int *freeData );

/* difs/initfonts.c */
extern void InitFonts ( void );

/* difs/fonts.c */
extern int FontToFSError ( int err );
extern void UseFPE ( FontPathElementPtr fpe );
extern void FreeFPE ( FontPathElementPtr fpe );
extern void QueueFontWakeup ( FontPathElementPtr fpe );
extern void RemoveFontWakeup ( FontPathElementPtr fpe );
extern void FontWakeup ( pointer data, int count, unsigned long *LastSelectMask );
extern int OpenFont ( ClientPtr client, Font fid, fsBitmapFormat format, fsBitmapFormatMask format_mask, int namelen, char *name );
extern int CloseClientFont ( ClientFontPtr cfp, FSID fid );
extern int SetFontCatalogue ( char *str, int *badpath );
extern int ListFonts ( ClientPtr client, int length, unsigned char *pattern, int maxNames );
#if 0
extern int StartListFontsWithInfo ( ClientPtr client, int length, unsigned char *pattern, int maxNames );
#endif
extern int LoadGlyphRanges ( ClientPtr client, FontPtr pfont, Bool range_flag, int num_ranges, int item_size, fsChar2b *data );
extern void FreeFonts ( void );
extern FontPtr find_old_font ( FSID id );
extern void DeleteClientFontStuff ( ClientPtr client );

/* difs/fontinfo.c */
extern void CopyCharInfo ( CharInfoPtr ci, fsXCharInfo *dst );
extern int convert_props ( FontInfoPtr pinfo, fsPropInfo **props );
extern int QueryExtents ( ClientPtr client, ClientFontPtr cfp, int item_size, int nranges, Bool range_flag, pointer range_data );
extern int QueryBitmaps ( ClientPtr client, ClientFontPtr cfp, int item_size, fsBitmapFormat format, int nranges, Bool range_flag, pointer range_data );

/* difs/main.c */
extern int NotImplemented(void);

#endif
