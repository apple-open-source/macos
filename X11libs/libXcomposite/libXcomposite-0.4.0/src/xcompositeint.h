/*
 * $Id$
 *
 * Copyright © 2006 Sun Microsystems
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of Sun Microsystems not be used in
 * advertising or publicity pertaining to distribution of the software without
 * specific, written prior permission.  Sun Microsystems makes no
 * representations about the suitability of this software for any purpose.  It
 * is provided "as is" without express or implied warranty.
 *
 * SUN MICROSYSTEMS DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL SUN MICROSYSTEMS BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 *
 * Copyright © 2003 Keith Packard
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of Keith Packard not be used in
 * advertising or publicity pertaining to distribution of the software without
 * specific, written prior permission.  Keith Packard makes no
 * representations about the suitability of this software for any purpose.  It
 * is provided "as is" without express or implied warranty.
 *
 * KEITH PACKARD DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL KEITH PACKARD BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef _XCOMPOSITEINT_H_
#define _XCOMPOSITEINT_H_

#define NEED_EVENTS
#define NEED_REPLIES
#include <stdio.h>
#include <X11/Xlib.h>
#include <X11/Xlibint.h>
#include <X11/Xutil.h>
#include <X11/extensions/compositeproto.h>
#include <X11/extensions/Xcomposite.h>

typedef struct _XCompositeExtDisplayInfo {
    struct _XCompositeExtDisplayInfo  *next;    /* keep a linked list */
    Display                 *display;	    /* which display this is */
    XExtCodes               *codes;	    /* the extension protocol codes */
    int			    major_version;  /* -1 means we don't know */
    int			    minor_version;  /* -1 means we don't know */
} XCompositeExtDisplayInfo;

/* replaces XExtensionInfo */
typedef struct _XCompositeExtInfo {
    XCompositeExtDisplayInfo    *head;          /* start of the list */
    XCompositeExtDisplayInfo    *cur;           /* most recently used */
    int                     ndisplays;      /* number of displays */
} XCompositeExtInfo;

extern XCompositeExtInfo XCompositeExtensionInfo;
extern const char XCompositeExtensionName[];

XCompositeExtDisplayInfo *
XCompositeFindDisplay (Display *dpy);

#define XCompositeHasExtension(i) ((i) && ((i)->codes))

#define XCompositeCheckExtension(dpy,i,val) \
  if (!XCompositeHasExtension(i)) { return val; }

#define XCompositeSimpleCheckExtension(dpy,i) \
  if (!XCompositeHasExtension(i)) { return; }

#endif /* _XCOMPOSITEINT_H_ */
