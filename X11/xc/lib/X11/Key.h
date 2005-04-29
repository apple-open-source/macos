/* $XFree86: xc/lib/X11/Key.h,v 1.1 2003/04/13 19:22:16 dawes Exp $ */

#ifndef _KEY_H_
#define _KEY_H_

#ifndef NEEDKTABLE
extern const unsigned char _XkeyTable[];
#endif

extern int
_XKeyInitialize(
    Display *dpy);

extern XrmDatabase
_XInitKeysymDB(
        void);

#endif /* _KEY_H_ */
