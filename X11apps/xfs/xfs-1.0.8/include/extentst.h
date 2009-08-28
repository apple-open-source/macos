/* $Xorg: extentst.h,v 1.4 2001/02/09 02:05:44 xorgcvs Exp $ */
/*
Copyright 1987, 1998  The Open Group

Permission to use, copy, modify, distribute, and sell this software and its
documentation for any purpose is hereby granted without fee, provided that
the above copyright notice appear in all copies and that both that
copyright notice and this permission notice appear in supporting
documentation.

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
OPEN GROUP BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN
AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Except as contained in this notice, the name of The Open Group shall not be
used in advertising or otherwise to promote the sale, use or other dealings
in this Software without prior written authorization from The Open Group.
 * Copyright 1990, 1991 Network Computing Devices;
 * Portions Copyright 1987 by Digital Equipment Corporation 
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the names of Network Computing Devices,
 * or Digital not be used in advertising or
 * publicity pertaining to distribution of the software without specific,
 * written prior permission.  Network Computing Devices, or Digital
 * make no representations about the
 * suitability of this software for any purpose.  It is provided "as is"
 * without express or implied warranty.
 *
 * NETWORK COMPUTING DEVICES, AND DIGITAL DISCLAIM ALL WARRANTIES WITH
 * REGARD TO THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS, IN NO EVENT SHALL NETWORK COMPUTING DEVICES, OR DIGITAL BE
 * LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * @(#)extentst.h	4.1	91/05/02
 *
 */
/* $XFree86: xc/programs/xfs/include/extentst.h,v 1.6 2001/12/14 20:01:38 dawes Exp $ */

#ifndef _EXTENTST_H_
#define _EXTENTST_H_

typedef struct _ExtensionEntry {
    int         index;
    void        (*CloseDown) (struct _ExtensionEntry*);
    char       *name;
    int         base;
    int         eventBase;
    int         eventLast;
    int         errorBase;
    int         errorLast;
    int         num_aliases;
    char      **aliases;
    pointer     extPrivate;
    unsigned short (*MinorOpcode) (ClientPtr);
}           ExtensionEntry;

extern void (*EventSwapVector[]) (fsError *, fsError *);

#if 0
typedef void (*ExtensionLookupProc) (char *name, GCPtr pGC);

typedef struct _ProcEntry {
    char       *name;
    ExtensionLookupProc proc;
}           ProcEntryRec, *ProcEntryPtr;

extern ExtensionEntry *AddExtension();
extern ExtensionLookupProc LookupProc(char *name, GCPtr pGC);
extern Bool RegisterProc();
#endif

extern ExtensionEntry * AddExtension ( char *name, int num_events, int num_errors, int (*main_proc) (ClientPtr), int (*smain_proc) (ClientPtr), void (*closedown_proc) (struct _ExtensionEntry *), unsigned short (*minorop_proc) (ClientPtr) );

extern Bool AddExtensionAlias(char *alias, ExtensionEntry *ext);
extern int  ProcListExtensions(ClientPtr client);
extern int  ProcQueryExtension(ClientPtr client);
extern unsigned short MinorOpcodeOfRequest(ClientPtr client);
extern unsigned short StandardMinorOpcode(ClientPtr client);
extern void CloseDownExtensions(void);
extern void InitExtensions(void);

#endif				/* _EXTENTST_H_ */
