/* $XConsortium: xev.c,v 1.15 94/04/17 20:45:20 keith Exp $ */
/*

Copyright (c) 1988  X Consortium

Permission is hereby granted, free of charge, to any person obtaining
a copy of this software and associated documentation files (the
"Software"), to deal in the Software without restriction, including
without limitation the rights to use, copy, modify, merge, publish,
distribute, sublicense, and/or sell copies of the Software, and to
permit persons to whom the Software is furnished to do so, subject to
the following conditions:

The above copyright notice and this permission notice shall be included
in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE X CONSORTIUM BE LIABLE FOR ANY CLAIM, DAMAGES OR
OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
OTHER DEALINGS IN THE SOFTWARE.

Except as contained in this notice, the name of the X Consortium shall
not be used in advertising or otherwise to promote the sale, use or
other dealings in this Software without prior written authorization
from the X Consortium.

*/
/* $XFree86: xc/programs/xev/xev.c,v 1.13 2003/10/24 20:38:17 tsi Exp $ */

/*
 * Author:  Jim Fulton, MIT X Consortium
 */

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <X11/Xlocale.h>
#include <X11/Xos.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xproto.h>

#define INNER_WINDOW_WIDTH 50
#define INNER_WINDOW_HEIGHT 50
#define INNER_WINDOW_BORDER 4
#define INNER_WINDOW_X 10
#define INNER_WINDOW_Y 10
#define OUTER_WINDOW_MIN_WIDTH (INNER_WINDOW_WIDTH + \
				2 * (INNER_WINDOW_BORDER + INNER_WINDOW_X))
#define OUTER_WINDOW_MIN_HEIGHT (INNER_WINDOW_HEIGHT + \
				2 * (INNER_WINDOW_BORDER + INNER_WINDOW_Y))
#define OUTER_WINDOW_DEF_WIDTH (OUTER_WINDOW_MIN_WIDTH + 100)
#define OUTER_WINDOW_DEF_HEIGHT (OUTER_WINDOW_MIN_HEIGHT + 100)
#define OUTER_WINDOW_DEF_X 100
#define OUTER_WINDOW_DEF_Y 100
				

typedef unsigned long Pixel;

const char *Yes = "YES";
const char *No = "NO";
const char *Unknown = "unknown";

const char *ProgramName;
Display *dpy;
int screen;

XIC xic = NULL;

Atom wm_delete_window;
Atom wm_protocols;

static void
prologue (XEvent *eventp, char *event_name)
{
    XAnyEvent *e = (XAnyEvent *) eventp;

    printf ("\n%s event, serial %ld, synthetic %s, window 0x%lx,\n",
	    event_name, e->serial, e->send_event ? Yes : No, e->window);
}

static void
dump (char *str, int len)
{
    printf("(");
    len--;
    while (len-- > 0)
        printf("%02x ", (unsigned char) *str++);
    printf("%02x)", (unsigned char) *str++);
}

static void
do_KeyPress (XEvent *eventp)
{
    XKeyEvent *e = (XKeyEvent *) eventp;
    KeySym ks;
    KeyCode kc = 0;
    Bool kc_set = False;
    char *ksname;
    int nbytes, nmbbytes = 0;
    char str[256+1];
    static char *buf = NULL;
    static int bsize = 8;
    Status status;

    if (buf == NULL)
      buf = malloc (bsize);

    nbytes = XLookupString (e, str, 256, &ks, NULL);

    /* not supposed to call XmbLookupString on a key release event */
    if (e->type == KeyPress && xic) {
        do {
            nmbbytes = XmbLookupString (xic, e, buf, bsize - 1, &ks, &status);
            buf[nmbbytes] = '\0';

            if (status == XBufferOverflow) {
                bsize = nmbbytes + 1;
                buf = realloc (buf, bsize);
            }
        } while (status == XBufferOverflow);
    }

    if (ks == NoSymbol)
	ksname = "NoSymbol";
    else {
	if (!(ksname = XKeysymToString (ks)))
	    ksname = "(no name)";
	kc = XKeysymToKeycode(dpy, ks);
	kc_set = True;
    }
	
    printf ("    root 0x%lx, subw 0x%lx, time %lu, (%d,%d), root:(%d,%d),\n",
	    e->root, e->subwindow, e->time, e->x, e->y, e->x_root, e->y_root);
    printf ("    state 0x%x, keycode %u (keysym 0x%lx, %s), same_screen %s,\n",
	    e->state, e->keycode, (unsigned long) ks, ksname,
	    e->same_screen ? Yes : No);
    if (kc_set && e->keycode != kc)
	printf ("    XKeysymToKeycode returns keycode: %u\n",kc);
    if (nbytes < 0) nbytes = 0;
    if (nbytes > 256) nbytes = 256;
    str[nbytes] = '\0';
    printf ("    XLookupString gives %d bytes: ", nbytes);
    if (nbytes > 0) {
        dump (str, nbytes);
        printf (" \"%s\"\n", str);
    } else {
    	printf ("\n");
    }

    /* not supposed to call XmbLookupString on a key release event */
    if (e->type == KeyPress && xic) {
        printf ("    XmbLookupString gives %d bytes: ", nmbbytes);
        if (nmbbytes > 0) {
           dump (buf, nmbbytes);
           printf (" \"%s\"\n", buf);
        } else {
    	   printf ("\n");
        }
    }

    printf ("    XFilterEvent returns: %s\n", 
	    XFilterEvent (eventp, e->window) ? "True" : "False");
}

static void
do_KeyRelease (XEvent *eventp)
{
    do_KeyPress (eventp);		/* since it has the same info */
}

static void
do_ButtonPress (XEvent *eventp)
{
    XButtonEvent *e = (XButtonEvent *) eventp;

    printf ("    root 0x%lx, subw 0x%lx, time %lu, (%d,%d), root:(%d,%d),\n",
	    e->root, e->subwindow, e->time, e->x, e->y, e->x_root, e->y_root);
    printf ("    state 0x%x, button %u, same_screen %s\n",
	    e->state, e->button, e->same_screen ? Yes : No);
}

static void
do_ButtonRelease (XEvent *eventp)
{
    do_ButtonPress (eventp);		/* since it has the same info */
}

static void
do_MotionNotify (XEvent *eventp)
{
    XMotionEvent *e = (XMotionEvent *) eventp;

    printf ("    root 0x%lx, subw 0x%lx, time %lu, (%d,%d), root:(%d,%d),\n",
	    e->root, e->subwindow, e->time, e->x, e->y, e->x_root, e->y_root);
    printf ("    state 0x%x, is_hint %u, same_screen %s\n",
	    e->state, e->is_hint, e->same_screen ? Yes : No);
}

static void
do_EnterNotify (XEvent *eventp)
{
    XCrossingEvent *e = (XCrossingEvent *) eventp;
    char *mode, *detail;
    char dmode[10], ddetail[10];

    switch (e->mode) {
      case NotifyNormal:  mode = "NotifyNormal"; break;
      case NotifyGrab:  mode = "NotifyGrab"; break;
      case NotifyUngrab:  mode = "NotifyUngrab"; break;
      case NotifyWhileGrabbed:  mode = "NotifyWhileGrabbed"; break;
      default:  mode = dmode, sprintf (dmode, "%u", e->mode); break;
    }

    switch (e->detail) {
      case NotifyAncestor:  detail = "NotifyAncestor"; break;
      case NotifyVirtual:  detail = "NotifyVirtual"; break;
      case NotifyInferior:  detail = "NotifyInferior"; break;
      case NotifyNonlinear:  detail = "NotifyNonlinear"; break;
      case NotifyNonlinearVirtual:  detail = "NotifyNonlinearVirtual"; break;
      case NotifyPointer:  detail = "NotifyPointer"; break;
      case NotifyPointerRoot:  detail = "NotifyPointerRoot"; break;
      case NotifyDetailNone:  detail = "NotifyDetailNone"; break;
      default:  detail = ddetail; sprintf (ddetail, "%u", e->detail); break;
    }

    printf ("    root 0x%lx, subw 0x%lx, time %lu, (%d,%d), root:(%d,%d),\n",
	    e->root, e->subwindow, e->time, e->x, e->y, e->x_root, e->y_root);
    printf ("    mode %s, detail %s, same_screen %s,\n",
	    mode, detail, e->same_screen ? Yes : No);
    printf ("    focus %s, state %u\n", e->focus ? Yes : No, e->state);
}

static void
do_LeaveNotify (XEvent *eventp)
{
    do_EnterNotify (eventp);		/* since it has same information */
}

static void
do_FocusIn (XEvent *eventp)
{
    XFocusChangeEvent *e = (XFocusChangeEvent *) eventp;
    char *mode, *detail;
    char dmode[10], ddetail[10];

    switch (e->mode) {
      case NotifyNormal:  mode = "NotifyNormal"; break;
      case NotifyGrab:  mode = "NotifyGrab"; break;
      case NotifyUngrab:  mode = "NotifyUngrab"; break;
      case NotifyWhileGrabbed:  mode = "NotifyWhileGrabbed"; break;
      default:  mode = dmode, sprintf (dmode, "%u", e->mode); break;
    }

    switch (e->detail) {
      case NotifyAncestor:  detail = "NotifyAncestor"; break;
      case NotifyVirtual:  detail = "NotifyVirtual"; break;
      case NotifyInferior:  detail = "NotifyInferior"; break;
      case NotifyNonlinear:  detail = "NotifyNonlinear"; break;
      case NotifyNonlinearVirtual:  detail = "NotifyNonlinearVirtual"; break;
      case NotifyPointer:  detail = "NotifyPointer"; break;
      case NotifyPointerRoot:  detail = "NotifyPointerRoot"; break;
      case NotifyDetailNone:  detail = "NotifyDetailNone"; break;
      default:  detail = ddetail; sprintf (ddetail, "%u", e->detail); break;
    }

    printf ("    mode %s, detail %s\n", mode, detail);
}

static void
do_FocusOut (XEvent *eventp)
{
    do_FocusIn (eventp);		/* since it has same information */
}

static void
do_KeymapNotify (XEvent *eventp)
{
    XKeymapEvent *e = (XKeymapEvent *) eventp;
    int i;

    printf ("    keys:  ");
    for (i = 0; i < 32; i++) {
	if (i == 16) printf ("\n           ");
	printf ("%-3u ", (unsigned int) e->key_vector[i]);
    }
    printf ("\n");
}

static void
do_Expose (XEvent *eventp)
{
    XExposeEvent *e = (XExposeEvent *) eventp;

    printf ("    (%d,%d), width %d, height %d, count %d\n",
	    e->x, e->y, e->width, e->height, e->count);
}

static void
do_GraphicsExpose (XEvent *eventp)
{
    XGraphicsExposeEvent *e = (XGraphicsExposeEvent *) eventp;
    char *m;
    char mdummy[10];

    switch (e->major_code) {
      case X_CopyArea:  m = "CopyArea";  break;
      case X_CopyPlane:  m = "CopyPlane";  break;
      default:  m = mdummy; sprintf (mdummy, "%d", e->major_code); break;
    }

    printf ("    (%d,%d), width %d, height %d, count %d,\n",
	    e->x, e->y, e->width, e->height, e->count);
    printf ("    major %s, minor %d\n", m, e->minor_code);
}

static void
do_NoExpose (XEvent *eventp)
{
    XNoExposeEvent *e = (XNoExposeEvent *) eventp;
    char *m;
    char mdummy[10];

    switch (e->major_code) {
      case X_CopyArea:  m = "CopyArea";  break;
      case X_CopyPlane:  m = "CopyPlane";  break;
      default:  m = mdummy; sprintf (mdummy, "%d", e->major_code); break;
    }

    printf ("    major %s, minor %d\n", m, e->minor_code);
    return;
}

static void
do_VisibilityNotify (XEvent *eventp)
{
    XVisibilityEvent *e = (XVisibilityEvent *) eventp;
    char *v;
    char vdummy[10];

    switch (e->state) {
      case VisibilityUnobscured:  v = "VisibilityUnobscured"; break;
      case VisibilityPartiallyObscured:  v = "VisibilityPartiallyObscured"; break;
      case VisibilityFullyObscured:  v = "VisibilityFullyObscured"; break;
      default:  v = vdummy; sprintf (vdummy, "%d", e->state); break;
    }

    printf ("    state %s\n", v);
}

static void
do_CreateNotify (XEvent *eventp)
{
    XCreateWindowEvent *e = (XCreateWindowEvent *) eventp;

    printf ("    parent 0x%lx, window 0x%lx, (%d,%d), width %d, height %d\n",
	    e->parent, e->window, e->x, e->y, e->width, e->height);
    printf ("border_width %d, override %s\n",
	    e->border_width, e->override_redirect ? Yes : No);
}

static void
do_DestroyNotify (XEvent *eventp)
{
    XDestroyWindowEvent *e = (XDestroyWindowEvent *) eventp;

    printf ("    event 0x%lx, window 0x%lx\n", e->event, e->window);
}

static void
do_UnmapNotify (XEvent *eventp)
{
    XUnmapEvent *e = (XUnmapEvent *) eventp;

    printf ("    event 0x%lx, window 0x%lx, from_configure %s\n",
	    e->event, e->window, e->from_configure ? Yes : No);
}

static void
do_MapNotify (XEvent *eventp)
{
    XMapEvent *e = (XMapEvent *) eventp;

    printf ("    event 0x%lx, window 0x%lx, override %s\n",
	    e->event, e->window, e->override_redirect ? Yes : No);
}

static void
do_MapRequest (XEvent *eventp)
{
    XMapRequestEvent *e = (XMapRequestEvent *) eventp;

    printf ("    parent 0x%lx, window 0x%lx\n", e->parent, e->window);
}

static void
do_ReparentNotify (XEvent *eventp)
{
    XReparentEvent *e = (XReparentEvent *) eventp;

    printf ("    event 0x%lx, window 0x%lx, parent 0x%lx,\n",
	    e->event, e->window, e->parent);
    printf ("    (%d,%d), override %s\n", e->x, e->y, 
	    e->override_redirect ? Yes : No);
}

static void
do_ConfigureNotify (XEvent *eventp)
{
    XConfigureEvent *e = (XConfigureEvent *) eventp;

    printf ("    event 0x%lx, window 0x%lx, (%d,%d), width %d, height %d,\n",
	    e->event, e->window, e->x, e->y, e->width, e->height);
    printf ("    border_width %d, above 0x%lx, override %s\n",
	    e->border_width, e->above, e->override_redirect ? Yes : No);
}

static void
do_ConfigureRequest (XEvent *eventp)
{
    XConfigureRequestEvent *e = (XConfigureRequestEvent *) eventp;
    char *detail;
    char ddummy[10];

    switch (e->detail) {
      case Above:  detail = "Above";  break;
      case Below:  detail = "Below";  break;
      case TopIf:  detail = "TopIf";  break;
      case BottomIf:  detail = "BottomIf"; break;
      case Opposite:  detail = "Opposite"; break;
      default:  detail = ddummy; sprintf (ddummy, "%d", e->detail); break;
    }

    printf ("    parent 0x%lx, window 0x%lx, (%d,%d), width %d, height %d,\n",
	    e->parent, e->window, e->x, e->y, e->width, e->height);
    printf ("    border_width %d, above 0x%lx, detail %s, value 0x%lx\n",
	    e->border_width, e->above, detail, e->value_mask);
}

static void
do_GravityNotify (XEvent *eventp)
{
    XGravityEvent *e = (XGravityEvent *) eventp;

    printf ("    event 0x%lx, window 0x%lx, (%d,%d)\n",
	    e->event, e->window, e->x, e->y);
}

static void
do_ResizeRequest (XEvent *eventp)
{
    XResizeRequestEvent *e = (XResizeRequestEvent *) eventp;

    printf ("    width %d, height %d\n", e->width, e->height);
}

static void
do_CirculateNotify (XEvent *eventp)
{
    XCirculateEvent *e = (XCirculateEvent *) eventp;
    char *p;
    char pdummy[10];

    switch (e->place) {
      case PlaceOnTop:  p = "PlaceOnTop"; break;
      case PlaceOnBottom:  p = "PlaceOnBottom"; break;
      default:  p = pdummy; sprintf (pdummy, "%d", e->place); break;
    }

    printf ("    event 0x%lx, window 0x%lx, place %s\n",
	    e->event, e->window, p);
}

static void
do_CirculateRequest (XEvent *eventp)
{
    XCirculateRequestEvent *e = (XCirculateRequestEvent *) eventp;
    char *p;
    char pdummy[10];

    switch (e->place) {
      case PlaceOnTop:  p = "PlaceOnTop"; break;
      case PlaceOnBottom:  p = "PlaceOnBottom"; break;
      default:  p = pdummy; sprintf (pdummy, "%d", e->place); break;
    }

    printf ("    parent 0x%lx, window 0x%lx, place %s\n",
	    e->parent, e->window, p);
}

static void
do_PropertyNotify (XEvent *eventp)
{
    XPropertyEvent *e = (XPropertyEvent *) eventp;
    char *aname = XGetAtomName (dpy, e->atom);
    char *s;
    char sdummy[10];

    switch (e->state) {
      case PropertyNewValue:  s = "PropertyNewValue"; break;
      case PropertyDelete:  s = "PropertyDelete"; break;
      default:  s = sdummy; sprintf (sdummy, "%d", e->state); break;
    }

    printf ("    atom 0x%lx (%s), time %lu, state %s\n",
	   e->atom, aname ? aname : Unknown, e->time,  s);

    if (aname) XFree (aname);
}

static void
do_SelectionClear (XEvent *eventp)
{
    XSelectionClearEvent *e = (XSelectionClearEvent *) eventp;
    char *sname = XGetAtomName (dpy, e->selection);

    printf ("    selection 0x%lx (%s), time %lu\n",
	    e->selection, sname ? sname : Unknown, e->time);

    if (sname) XFree (sname);
}

static void
do_SelectionRequest (XEvent *eventp)
{
    XSelectionRequestEvent *e = (XSelectionRequestEvent *) eventp;
    char *sname = XGetAtomName (dpy, e->selection);
    char *tname = XGetAtomName (dpy, e->target);
    char *pname = XGetAtomName (dpy, e->property);

    printf ("    owner 0x%lx, requestor 0x%lx, selection 0x%lx (%s),\n",
	    e->owner, e->requestor, e->selection, sname ? sname : Unknown);
    printf ("    target 0x%lx (%s), property 0x%lx (%s), time %lu\n",
	    e->target, tname ? tname : Unknown, e->property,
	    pname ? pname : Unknown, e->time);

    if (sname) XFree (sname);
    if (tname) XFree (tname);
    if (pname) XFree (pname);
}

static void
do_SelectionNotify (XEvent *eventp)
{
    XSelectionEvent *e = (XSelectionEvent *) eventp;
    char *sname = XGetAtomName (dpy, e->selection);
    char *tname = XGetAtomName (dpy, e->target);
    char *pname = XGetAtomName (dpy, e->property);

    printf ("    selection 0x%lx (%s), target 0x%lx (%s),\n",
	    e->selection, sname ? sname : Unknown, e->target,
	    tname ? tname : Unknown);
    printf ("    property 0x%lx (%s), time %lu\n",
	    e->property, pname ? pname : Unknown, e->time);

    if (sname) XFree (sname);
    if (tname) XFree (tname);
    if (pname) XFree (pname);
}

static void
do_ColormapNotify (XEvent *eventp)
{
    XColormapEvent *e = (XColormapEvent *) eventp;
    char *s;
    char sdummy[10];

    switch (e->state) {
      case ColormapInstalled:  s = "ColormapInstalled"; break;
      case ColormapUninstalled:  s = "ColormapUninstalled"; break;
      default:  s = sdummy; sprintf (sdummy, "%d", e->state); break;
    }

    printf ("    colormap 0x%lx, new %s, state %s\n",
	    e->colormap, e->new ? Yes : No, s);
}

static void
do_ClientMessage (XEvent *eventp)
{
    XClientMessageEvent *e = (XClientMessageEvent *) eventp;
    char *mname = XGetAtomName (dpy, e->message_type);

    if (e->message_type == wm_protocols) {
        char *message = XGetAtomName (dpy, e->data.l[0]);
        printf ("    message_type 0x%lx (%s), format %d, message 0x%lx (%s)\n",
                e->message_type, mname ? mname : Unknown, e->format, e->data.l[0], message);
        if (message) XFree (message);
    }
    else {
        printf ("    message_type 0x%lx (%s), format %d\n",
                e->message_type, mname ? mname : Unknown, e->format);
    }

    if (mname) XFree (mname);

    if (e->format == 32 
        && e->message_type == wm_protocols 
        && (Atom) e->data.l[0] == wm_delete_window) 
        exit (0);
}

static void
do_MappingNotify (XEvent *eventp)
{
    XMappingEvent *e = (XMappingEvent *) eventp;
    char *r;
    char rdummy[10];

    switch (e->request) {
      case MappingModifier:  r = "MappingModifier"; break;
      case MappingKeyboard:  r = "MappingKeyboard"; break;
      case MappingPointer:  r = "MappingPointer"; break;
      default:  r = rdummy; sprintf (rdummy, "%d", e->request); break;
    }

    printf ("    request %s, first_keycode %d, count %d\n",
	    r, e->first_keycode, e->count);
    XRefreshKeyboardMapping(e);
}



static void
set_sizehints (XSizeHints *hintp, int min_width, int min_height, 
	       int defwidth, int defheight, int defx, int defy, 
	       char *geom)
{
    int geom_result;

    /* set the size hints, algorithm from xlib xbiff */

    hintp->width = hintp->min_width = min_width;
    hintp->height = hintp->min_height = min_height;
    hintp->flags = PMinSize;
    hintp->x = hintp->y = 0;
    geom_result = NoValue;
    if (geom != NULL) {
        geom_result = XParseGeometry (geom, &hintp->x, &hintp->y,
				      (unsigned int *)&hintp->width,
				      (unsigned int *)&hintp->height);
	if ((geom_result & WidthValue) && (geom_result & HeightValue)) {
#ifndef max
#define max(a,b) ((a) > (b) ? (a) : (b))
#endif
	    hintp->width = max (hintp->width, hintp->min_width);
	    hintp->height = max (hintp->height, hintp->min_height);
	    hintp->flags |= USSize;
	}
	if ((geom_result & XValue) && (geom_result & YValue)) {
	    hintp->flags += USPosition;
	}
    }
    if (!(hintp->flags & USSize)) {
	hintp->width = defwidth;
	hintp->height = defheight;
	hintp->flags |= PSize;
    }
/*
    if (!(hintp->flags & USPosition)) {
	hintp->x = defx;
	hintp->y = defy;
	hintp->flags |= PPosition;
    }
 */
    if (geom_result & XNegative) {
	hintp->x = DisplayWidth (dpy, DefaultScreen (dpy)) + hintp->x -
		    hintp->width;
    }
    if (geom_result & YNegative) {
	hintp->y = DisplayHeight (dpy, DefaultScreen (dpy)) + hintp->y -
		    hintp->height;
    }
}


#if defined(__GNUC__) && \
    ((__GNUC__ > 2) || ((__GNUC__ == 2) && (__GNUC_MINOR__ >= 7)))
static void usage (void) __attribute__((__noreturn__));
#endif
static void
usage (void)
{
    static const char *msg[] = {
"    -display displayname                X server to contact",
"    -geometry geom                      size and location of window",
"    -bw pixels                          border width in pixels",
"    -bs {NotUseful,WhenMapped,Always}   backingstore attribute",
"    -id windowid                        use existing window",
"    -s                                  set save-unders attribute",
"    -name string                        window name",
"    -rv                                 reverse video",
"",
NULL};
    const char **cpp;

    fprintf (stderr, "usage:  %s [-options ...]\n", ProgramName);
    fprintf (stderr, "where options include:\n");

    for (cpp = msg; *cpp; cpp++)
	fprintf (stderr, "%s\n", *cpp);

    exit (1);
}

static int
parse_backing_store (char *s)
{
    int len = strlen (s);
    char *cp;

    for (cp = s; *cp; cp++) {
	if (isascii (*cp) && isupper (*cp))
	    *cp = tolower (*cp);
    }

    if (strncmp (s, "notuseful", len) == 0) return (NotUseful);
    if (strncmp (s, "whenmapped", len) == 0) return (WhenMapped);
    if (strncmp (s, "always", len) == 0) return (Always);

    usage ();
}

int
main (int argc, char **argv)
{
    char *displayname = NULL;
    char *geom = NULL;
    int i;
    XSizeHints hints;
    int borderwidth = 2;
    Window w, subw;
    XSetWindowAttributes attr;
    XWindowAttributes wattr;
    unsigned long mask = 0L;
    int done;
    char *name = "Event Tester";
    Bool reverse = False;
    unsigned long back, fore;
    XIM xim;
    XIMStyles *xim_styles;
    XIMStyle xim_style = 0;
    char *modifiers;
    char *imvalret;

    ProgramName = argv[0];

    if (setlocale(LC_ALL,"") == NULL) {
	fprintf(stderr, "%s: warning: could not set default locale\n",
		ProgramName);
    }

    w = 0;
    for (i = 1; i < argc; i++) {
	char *arg = argv[i];

	if (arg[0] == '-') {
	    switch (arg[1]) {
	      case 'd':			/* -display host:dpy */
		if (++i >= argc) usage ();
		displayname = argv[i];
		continue;
	      case 'g':			/* -geometry geom */
		if (++i >= argc) usage ();
		geom = argv[i];
		continue;
	      case 'b':
		switch (arg[2]) {
		  case 'w':		/* -bw pixels */
		    if (++i >= argc) usage ();
		    borderwidth = atoi (argv[i]);
		    continue;
		  case 's':		/* -bs type */
		    if (++i >= argc) usage ();
		    attr.backing_store = parse_backing_store (argv[i]);
		    mask |= CWBackingStore;
		    continue;
		  default:
		    usage ();
		}
	      case 'i':			/* -id */
		if (++i >= argc) usage ();
		sscanf(argv[i], "0x%lx", &w);
		if (!w)
		    sscanf(argv[i], "%lu", &w);
		if (!w)
		    usage ();
		continue;
	      case 'n':			/* -name */
		if (++i >= argc) usage ();
		name = argv[i];
		continue;
	      case 'r':			/* -rv */
		reverse = True;
		continue;
	      case 's':			/* -s */
		attr.save_under = True;
		mask |= CWSaveUnder;
		continue;
	      default:
		usage ();
	    }				/* end switch on - */
	} else 
	  usage ();
    }					/* end for over argc */

    dpy = XOpenDisplay (displayname);
    if (!dpy) {
	fprintf (stderr, "%s:  unable to open display '%s'\n",
		 ProgramName, XDisplayName (displayname));
	exit (1);
    }

    /* we're testing the default input method */
    modifiers = XSetLocaleModifiers ("@im=none");
    if (modifiers == NULL) {
        fprintf (stderr, "%s:  XSetLocaleModifiers failed\n", ProgramName);
    }

    xim = XOpenIM (dpy, NULL, NULL, NULL);
    if (xim == NULL) {
        fprintf (stderr, "%s:  XOpenIM failed\n", ProgramName);
    }

    if (xim) {
        imvalret = XGetIMValues (xim, XNQueryInputStyle, &xim_styles, NULL);
        if (imvalret != NULL || xim_styles == NULL) {
            fprintf (stderr, "%s:  input method doesn't support any styles\n", ProgramName);
        }

        if (xim_styles) {
            xim_style = 0;
            for (i = 0;  i < xim_styles->count_styles;  i++) {
                if (xim_styles->supported_styles[i] ==
                    (XIMPreeditNothing | XIMStatusNothing)) {
                    xim_style = xim_styles->supported_styles[i];
                    break;
                }
            }

            if (xim_style == 0) {
                fprintf (stderr, "%s: input method doesn't support the style we support\n",
                         ProgramName);
            }
            XFree (xim_styles);
        }
    } 

    screen = DefaultScreen (dpy);

    /* select for all events */
    attr.event_mask = KeyPressMask | KeyReleaseMask | ButtonPressMask |
			   ButtonReleaseMask | EnterWindowMask |
			   LeaveWindowMask | PointerMotionMask | 
			   Button1MotionMask |
			   Button2MotionMask | Button3MotionMask |
			   Button4MotionMask | Button5MotionMask |
			   ButtonMotionMask | KeymapStateMask |
			   ExposureMask | VisibilityChangeMask | 
			   StructureNotifyMask | /* ResizeRedirectMask | */
			   SubstructureNotifyMask | SubstructureRedirectMask |
			   FocusChangeMask | PropertyChangeMask |
			   ColormapChangeMask | OwnerGrabButtonMask;

    if (w) {
	XGetWindowAttributes(dpy, w, &wattr);
	if (wattr.all_event_masks & ButtonPressMask)
	    attr.event_mask &= ~ButtonPressMask;
	attr.event_mask &= ~SubstructureRedirectMask;
	XSelectInput(dpy, w, attr.event_mask);
    } else {
	set_sizehints (&hints, OUTER_WINDOW_MIN_WIDTH, OUTER_WINDOW_MIN_HEIGHT,
		       OUTER_WINDOW_DEF_WIDTH, OUTER_WINDOW_DEF_HEIGHT, 
		       OUTER_WINDOW_DEF_X, OUTER_WINDOW_DEF_Y, geom);

	if (reverse) {
	    back = BlackPixel(dpy,screen);
	    fore = WhitePixel(dpy,screen);
	} else {
	    back = WhitePixel(dpy,screen);
	    fore = BlackPixel(dpy,screen);
	}

	attr.background_pixel = back;
	attr.border_pixel = fore;
	mask |= (CWBackPixel | CWBorderPixel | CWEventMask);

	w = XCreateWindow (dpy, RootWindow (dpy, screen), hints.x, hints.y,
			   hints.width, hints.height, borderwidth, 0,
			   InputOutput, (Visual *)CopyFromParent,
			   mask, &attr);

	XSetStandardProperties (dpy, w, name, NULL, (Pixmap) 0,
				argv, argc, &hints);

	subw = XCreateSimpleWindow (dpy, w, INNER_WINDOW_X, INNER_WINDOW_Y,
				    INNER_WINDOW_WIDTH, INNER_WINDOW_HEIGHT,
				    INNER_WINDOW_BORDER,
				    attr.border_pixel, attr.background_pixel);

        wm_protocols = XInternAtom(dpy, "WM_PROTOCOLS", False);
        wm_delete_window = XInternAtom(dpy, "WM_DELETE_WINDOW", False);
        XSetWMProtocols(dpy, w, &wm_delete_window, 1);

	XMapWindow (dpy, subw);		/* map before w so that it appears */
	XMapWindow (dpy, w);

	printf ("Outer window is 0x%lx, inner window is 0x%lx\n", w, subw);
    }

    if (xim && xim_style) {
        xic = XCreateIC (xim, 
                         XNInputStyle, xim_style, 
                         XNClientWindow, w, 
                         XNFocusWindow, w, 
                         NULL);

        if (xic == NULL) {
            fprintf (stderr, "XCreateIC failed\n");
        }
    }

    for (done = 0; !done; ) {
	XEvent event;

	XNextEvent (dpy, &event);

	switch (event.type) {
	  case KeyPress:
	    prologue (&event, "KeyPress");
	    do_KeyPress (&event);
	    break;
	  case KeyRelease:
	    prologue (&event, "KeyRelease");
	    do_KeyRelease (&event);
	    break;
	  case ButtonPress:
	    prologue (&event, "ButtonPress");
	    do_ButtonPress (&event);
	    break;
	  case ButtonRelease:
	    prologue (&event, "ButtonRelease");
	    do_ButtonRelease (&event);
	    break;
	  case MotionNotify:
	    prologue (&event, "MotionNotify");
	    do_MotionNotify (&event);
	    break;
	  case EnterNotify:
	    prologue (&event, "EnterNotify");
	    do_EnterNotify (&event);
	    break;
	  case LeaveNotify:
	    prologue (&event, "LeaveNotify");
	    do_LeaveNotify (&event);
	    break;
	  case FocusIn:
	    prologue (&event, "FocusIn");
	    do_FocusIn (&event);
	    break;
	  case FocusOut:
	    prologue (&event, "FocusOut");
	    do_FocusOut (&event);
	    break;
	  case KeymapNotify:
	    prologue (&event, "KeymapNotify");
	    do_KeymapNotify (&event);
	    break;
	  case Expose:
	    prologue (&event, "Expose");
	    do_Expose (&event);
	    break;
	  case GraphicsExpose:
	    prologue (&event, "GraphicsExpose");
	    do_GraphicsExpose (&event);
	    break;
	  case NoExpose:
	    prologue (&event, "NoExpose");
	    do_NoExpose (&event);
	    break;
	  case VisibilityNotify:
	    prologue (&event, "VisibilityNotify");
	    do_VisibilityNotify (&event);
	    break;
	  case CreateNotify:
	    prologue (&event, "CreateNotify");
	    do_CreateNotify (&event);
	    break;
	  case DestroyNotify:
	    prologue (&event, "DestroyNotify");
	    do_DestroyNotify (&event);
	    break;
	  case UnmapNotify:
	    prologue (&event, "UnmapNotify");
	    do_UnmapNotify (&event);
	    break;
	  case MapNotify:
	    prologue (&event, "MapNotify");
	    do_MapNotify (&event);
	    break;
	  case MapRequest:
	    prologue (&event, "MapRequest");
	    do_MapRequest (&event);
	    break;
	  case ReparentNotify:
	    prologue (&event, "ReparentNotify");
	    do_ReparentNotify (&event);
	    break;
	  case ConfigureNotify:
	    prologue (&event, "ConfigureNotify");
	    do_ConfigureNotify (&event);
	    break;
	  case ConfigureRequest:
	    prologue (&event, "ConfigureRequest");
	    do_ConfigureRequest (&event);
	    break;
	  case GravityNotify:
	    prologue (&event, "GravityNotify");
	    do_GravityNotify (&event);
	    break;
	  case ResizeRequest:
	    prologue (&event, "ResizeRequest");
	    do_ResizeRequest (&event);
	    break;
	  case CirculateNotify:
	    prologue (&event, "CirculateNotify");
	    do_CirculateNotify (&event);
	    break;
	  case CirculateRequest:
	    prologue (&event, "CirculateRequest");
	    do_CirculateRequest (&event);
	    break;
	  case PropertyNotify:
	    prologue (&event, "PropertyNotify");
	    do_PropertyNotify (&event);
	    break;
	  case SelectionClear:
	    prologue (&event, "SelectionClear");
	    do_SelectionClear (&event);
	    break;
	  case SelectionRequest:
	    prologue (&event, "SelectionRequest");
	    do_SelectionRequest (&event);
	    break;
	  case SelectionNotify:
	    prologue (&event, "SelectionNotify");
	    do_SelectionNotify (&event);
	    break;
	  case ColormapNotify:
	    prologue (&event, "ColormapNotify");
	    do_ColormapNotify (&event);
	    break;
	  case ClientMessage:
	    prologue (&event, "ClientMessage");
	    do_ClientMessage (&event);
	    break;
	  case MappingNotify:
	    prologue (&event, "MappingNotify");
	    do_MappingNotify (&event);
	    break;
	  default:
	    printf ("Unknown event type %d\n", event.type);
	    break;
	}
    }

    XCloseDisplay (dpy);
    return 0; 
}
