/* $Xorg: pm.h,v 1.4 2001/02/09 02:05:45 xorgcvs Exp $ */
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
/* $XFree86: xc/programs/xfwp/pm.h,v 1.5 2001/12/14 20:01:43 dawes Exp $ */

#ifndef _PM_H
#define _PM_H

extern void FWPprocessMessages(
    IceConn iceConn,
    IcePointer * client_data,
    int opcode,
    unsigned long length,
    Bool swap);

extern Bool
FWPHostBasedAuthProc (
    char * hostname);

extern Status
FWPprotocolSetupProc(
    IceConn iceConn,
    int major_version,
    int minor_version,
    char * vendor,
    char * release,
    IcePointer * clientDataRet,
    char ** failureReasonRet);

extern int 
doSetupPMListen(
    char *  pm_port,
    int * size_pm_listen_array,
    int ** pm_listen_array,
    IceListenObj ** listen_objects,
    int * nfds,
    fd_set * rinit);

extern void 
doInstallIOErrorHandler (void);

#endif /* _PM_H */
