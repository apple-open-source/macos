.\" Use -ms
.de Cs
.IP
.nf
.ft C
..
.de Ce
.ft P
.fi
..
.de Bu
.br
.ti +.5i
.ie \\n(.$ \\$1
.el \\(bu
..
.EH ''''
.OH ''''
.EF ''''
.OF ''''
.ps 10
.nr PS 10
\&
.TL
\s+2\fBMIT-SHM\(emThe MIT Shared Memory Extension\fP\s-2
.sp
How the shared memory extension works
.AU
Jonathan Corbet
.AI
Atmospheric Technology Division
National Center for Atmospheric Research
corbet@ncar.ucar.edu
.sp
Formatted and edited for release 5 by
.AU
Keith Packard
.AI
MIT X Consortium
.ps 9
.nr PS 9
.sp 8
.AB
This document briefly describes how to use the MIT-SHM shared memory
extension.  I have tried to make it accurate, but it would not surprise me
if some errors remained.  If you find anything wrong, do let me know and I
will incorporate the corrections.  Meanwhile, please take this document ``as
is''\(eman improvement over what was there before, but certainly not the
definitive word.
.AE
.LP
.DS C
Copyright \(co 1991 X Consortium
.DE
.sp 2
.LP
Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the ``Software''), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:
.LP
The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.
.LP
THE SOFTWARE IS PROVIDED ``AS IS'', WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
X CONSORTIUM BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN
AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
.LP
Except as contained in this notice, the name of the X Consortium shall not be
used in advertising or otherwise to promote the sale, use or other dealings
in this Software without prior written authorization from the X Consortium.
.ps 10
.nr PS 10
.bp 1
.de PT
.tl ''MIT Shared Memory Extension''
..
.de BT
.tl ''\fB % \fP''
..
.NH 1
REQUIREMENTS
.LP
The shared memory extension is provided only by some X servers.  To find out
if your server supports the extension, use xdpyinfo(1).  In particular, to
be able to use this extension, your system must provide the SYSV shared
memory primitives.  There is not an mmap-based version of this extension.
To use shared memory on Sun systems, you must have built your kernel with
SYSV shared memory enabled -- which is not the default configuration.
Additionally, the shared memeory maximum size will need to be increased on
both Sun and Digital systems; the defaults are far too small for any useful
work.
.NH 1
WHAT IS PROVIDED
.LP
The basic capability provided is that of shared memory XImages.  This is
essentially a version of the ximage interface where the actual image data
is stored in a shared memory segment, and thus need not be moved through
the Xlib interprocess communication channel.  For large images, use of this
facility can result in some real performance increases.
.LP
Additionally, some implementations provided shared memory pixmaps.  These
are 2 dimensional arrays of pixels in a format specified by the X server,
where the image data is stored in the shared memory segment.  Through use of
shared memory pixmaps, it is possible to change the contents of these
pixmaps without using any Xlib routines at all.  Shared memory pixmaps can
only be supported when the X server can use regular virtual memory for
pixmap data; if the pixmaps are stored in some magic graphics hardware, your
application will not be able to share them with the server.  Xdpyinfo(1)
doesn't print this particular nugget of information.
.NH 1
HOW TO USE THE SHARED MEMORY EXTENSION
.LP
Code which uses the shared memory extension must include a number of header
files:
.Cs
# include <X11/Xlib.h>		/* of course */
# include <sys/ipc.h>
# include <sys/shm.h>
# include <X11/extensions/XShm.h>
.Ce
.LP
Of course, if the system you are building on does not support shared
memory, the file XShm.h may not be present.  You may want to make
liberal use of #ifdefs.
.LP
Any code which uses the shared memory extension should first check to see
that the server provides the extension.  You could always be running over
the net, or in some other environment where the extension will not work.
To perform this check, call either
.Cs
Status XShmQueryExtension (display)
        Display *display
.Ce
.LP
or
.Cs
Status XShmQueryVersion (display, major, minor, pixmaps)
        Display *display;
        int *major, *minor;
        Bool *pixmaps
.Ce
.LP
Where ``display'' is, of course, the display on which you are running.  If
the shared memory extension may be used, the return value from either
function will be True; otherwise your program should operate using
conventional Xlib calls.  When the extension is available,
\fCXShmQueryVersion\fP also returns ``major'' and ``minor'' which are the
version numbers of the extension implementation, and ``pixmaps'' which is
True iff shared memory pixmaps are supported.
.NH 1
USE OF SHARED MEMORY XIMAGES
.LP
The basic sequence of operations for shared memory XImages is as follows:
.LP
.Bu "1 \-"
Create the shared memory XImage structure
.Bu "2 \-"
Create a shared memory segment to store the image data
.Bu "3 \-"
Inform the server about the shared memory segment
.Bu "4 \-"
Use the shared memory XImage, much like a normal one.
.LP
To create a shared memory XImage, use:
.Cs
XImage *XShmCreateImage (display, visual, depth, format, data, 
                         shminfo, width, height)
        Display *display;
        Visual *visual;
        unsigned int depth, width, height;
        int format;
        char *data;
        XShmSegmentInfo *shminfo;
.Ce
.LP
Most of the arguments are the same as for XCreateImage; I will not go
through them here.  Note, however, that there are no ``offset'', ``bitmap_pad'',
or ``bytes_per_line'' arguments.  These quantities will be defined by the
server itself, and your code needs to abide by them.  Unless you have already
allocated the shared memory segment (see below), you should pass in NULL for
the ``data'' pointer.
.LP
There is one additional argument: ``shminfo'', which is a pointer to a
structure of type XShmSegmentInfo.  You must allocate one of these
structures such that it will have a lifetime at least as long as that of
the shared memory XImage.  There is no need to initialize this structure
before the call to XShmCreateImage.
.LP
The return value, if all goes well, will be an XImage structure, which you
can use for the subsequent steps.
.LP
The next step is to create the shared memory segment.  This is
best done after the creation of the XImage, since you need to make use of
the information in that XImage to know how much memory to allocate.  To
create the segment, you need a call like:
.Cs
shminfo.shmid = shmget (IPC_PRIVATE,
		image->bytes_per_line * image->height, IPC_CREAT|0777);
.Ce
.LP
(assuming that you have called your shared memory XImage ``image'').  You
should, of course, follow the Rules and do error checking on all of these
system calls.  Also, be sure to use the bytes_per_line field, not the width
you used to create the XImage as they may well be different.
.LP
Note that the shared memory ID returned by the system is stored in the
shminfo structure.  The server will need that ID to attach itself to the
segment.
.LP
Also note that, on many systems for security reasons, the X server
will only accept to attach to the shared memory segment if it's
readable and writeable by ``other''. On systems where the X server is
able to determine the uid of the X client over a local transport, the
shared memory segment can be readable and writeable only by the uid of
the client.
.LP
Next, attach this shared memory segment to your process:
.Cs
shminfo.shmaddr = image->data = shmat (shminfo.shmid, 0, 0);
.Ce
.LP
The address returned by shmat should be stored in *both* the XImage
structure and the shminfo structure.
.LP
To finish filling in the shminfo structure, you need to decide how you want
the server to attach to the shared memory segment, and set the ``readOnly''
field as follows.  Normally, you would code:
.Cs
shminfo.readOnly = False;
.Ce
.LP
If you set it to True, the server will not be able to write to this
segment, and thus XShmGetImage calls will fail.
.LP
Finally, tell the server to attach to your shared memory segment with:
.Cs
Status XShmAttach (display, shminfo);
.Ce
.LP
If all goes well, you will get a non-zero status back, and your XImage is
ready for use.
.LP
To write a shared memory XImage into an X drawable, use XShmPutImage:
.Cs
Status XShmPutImage (display, d, gc, image, src_x, src_y, 
                     dest_x, dest_y, width, height, send_event)
        Display *display;
        Drawable d;
        GC gc;
        XImage *image;
        int src_x, src_y, dest_x, dest_y;
        unsigned int width, height;
        bool send_event;
.Ce
.LP
The interface is identical to that of XPutImage, so I will spare my fingers
and not repeat that documentation here.  There is one additional parameter,
however, called ``send_event''.  If this parameter is passed as True, the
server will generate a ``completion'' event when the image write is complete;
thus your program can know when it is safe to begin manipulating the shared
memory segment again.
.LP
The completion event has type XShmCompletionEvent, which is defined as the
following:
.Cs
typedef struct {
    int	type;		    /* of event */
    unsigned long serial;   /* # of last request processed */
    Bool send_event;	    /* true if came from a SendEvent request */
    Display *display;	    /* Display the event was read from */
    Drawable drawable;	    /* drawable of request */
    int major_code;	    /* ShmReqCode */
    int minor_code;	    /* X_ShmPutImage */
    ShmSeg shmseg;	    /* the ShmSeg used in the request */
    unsigned long offset;   /* the offset into ShmSeg used */
} XShmCompletionEvent;
.Ce
.LP
The event type value that will be used can be determined at run time with a
line of the form:
.Cs
int CompletionType = XShmGetEventBase (display) + ShmCompletion;
.Ce
.LP
If you modify the shared memory segment before the arrival of the
completion event, the results you see on the screen may be inconsistent.
.LP
To read image data into a shared memory XImage, use the following:
.Cs
Status XShmGetImage (display, d, image, x, y, plane_mask)
	Display *display;
	Drawable d;
	XImage *image;
	int x, y;
	unsigned long plane_mask;
.Ce
.LP
Where ``display'' is the display of interest, ``d'' is the source drawable,
``image'' is the destination XImage, ``x'' and ``y'' are the offsets within
``d'', and ``plane_mask'' defines which planes are to be read.
.LP
To destroy a shared memory XImage, you should first instruct the server to
detach from it, then destroy the segment itself, as follows:
.Cs
XShmDetach (display, shminfo);
XDestroyImage (image);
shmdt (shminfo.shmaddr);
shmctl (shminfo.shmid, IPC_RMID, 0);
.Ce
.NH 1
USE OF SHARED MEMORY PIXMAPS
.LP
Unlike X images, for which any image format is usable, the shared memory
extension supports only a single format (i.e. XYPixmap or ZPixmap) for the
data stored in a shared memory pixmap.  This format is independent of the
depth of the image (for 1-bit pixmaps it doesn't really matter what this
format is) and independent of the screen.  Use XShmPixmapFormat to get the
format for the server:
.Cs
int XShmPixmapFormat (display)
        Display *display;
.Ce
.LP
If your application can deal with the server pixmap data format (including
bits-per-pixel et al.), create a shared memory segment and ``shminfo''
structure in exactly the same way as is listed above for shared memory
XImages.  While it is, not strictly necessary to create an XImage first,
doing so incurs little overhead and will give you an appropriate
bytes_per_line value to use.
.LP
Once you have your shminfo structure filled in, simply call:
.Cs
Pixmap XShmCreatePixmap (display, d, data, shminfo, width,
                         height, depth);
        Display *display;
        Drawable d;
        char *data;
        XShmSegmentInfo *shminfo;
        unsigned int width, height, depth;
.Ce
.LP
The arguments are all the same as for XCreatePixmap, with two additions:
``data'' and ``shminfo''.  The second of the two is the same old shminfo
structure that has been used before.  The first is the pointer to the shared
memory segment, and should be the same as the shminfo.shmaddr field.  I am
not sure why this is a separate parameter.
.LP
If everything works, you will get back a pixmap, which you can manipulate in
all of the usual ways, with the added bonus of being able to tweak its
contents directly through the shared memory segment.  Shared memory pixmaps
are destroyed in the usual manner with XFreePixmap, though you should detach
and destroy the shared memory segment itself as shown above.
