/* $Xorg: io.h,v 1.4 2001/02/09 02:05:45 xorgcvs Exp $ */
/*

Copyright "1986-1997, 1998  The Open Group 

Permission to use, copy, modify, distribute, and sell this software and its
documentation for any purpose is hereby granted without fee, provided that
the above copyright notice appear in all copies and that both that
copyright notice and this permission notice appear in supporting
documentation.

The above copyright notice and the following permission notice
shall be included in all copies of the Software:

THE SOFTWARE IS PROVIDED "AS IS ", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING, BUT NOT LIMITED TO, THE
WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE
AND NON-INFRINGEMENT. IN NO EVENT SHALL THE OPEN GROUP BE LIABLE
FOR ANY CLAIM, DAMAGES OR OTHER SIABILITIY, WHETHER IN AN ACTION
OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF, OR IN
CONNNECTION WITH THE SOFTWARE OR THE USE OF OTHER DEALINGS IN
THE SOFTWARE.

Except as contained in this notice, the name of The Open Group
shall not be used in advertising or otherwise to promote the use
or other dealings in this Software without prior written
authorization from The Open Group.

X Window System is a trademark of The Open Group.

*/


#ifndef _IO_H
#define _IO_H

#define         SERVER_REPLY_FAILURE            0
#define         SERVER_REPLY_SUCCESS            1
#define         SERVER_REPLY_AUTHENTICATE       2

extern void
doProcessSelect(
    int * nfds,
    int * nready,
    fd_set * readable,
    fd_set * writable,
    fd_set * rinit,
    fd_set * winit,
    int pm_listen_array[],
    struct config * config_info,
    IceListenObj ** listen_objects);

#endif /* _IO_H */
