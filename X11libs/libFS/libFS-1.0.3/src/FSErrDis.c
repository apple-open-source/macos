/*
 * Copyright 1990 Network Computing Devices;
 * Portions Copyright 1987 by Digital Equipment Corporation
 *
 * Permission to use, copy, modify, distribute, and sell this software 
 * and its documentation for any purpose is hereby granted without fee, 
 * provided that the above copyright notice appear in all copies and 
 * that both that copyright notice and this permission notice appear 
 * in supporting documentation, and that the names of Network Computing 
 * Devices or Digital not be used in advertising or publicity pertaining 
 * to distribution of the software without specific, written prior 
 * permission. Network Computing Devices or Digital make no representations 
 * about the suitability of this software for any purpose.  It is provided 
 * "as is" without express or implied warranty.
 *
 * NETWORK COMPUTING DEVICES AND  DIGITAL DISCLAIM ALL WARRANTIES WITH
 * REGARD TO THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF 
 * MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL NETWORK COMPUTING DEVICES
 * OR DIGITAL BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES 
 * OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, 
 * WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, 
 * ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS 
 * SOFTWARE.
 */

/*

Copyright 1987, 1994, 1998  The Open Group

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

*/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <stdio.h>
#include "FSlibint.h"
#include <X11/Xos.h>

static const char *FSErrorList[] = {
     /* FSBadRequest	 */ "BadRequest, invalid request code or no such operation",
     /* FSBadFormat	 */ "BadFormat, bad font format mask",
     /* FSBadFont	 */ "BadFont, invalid Font parameter",
     /* FSBadRange	 */ "BadRange, invalid character range attributes",
     /* FSBadEventMask	 */ "BadEventMask, illegal event mask",
     /* FSBadAccessContext */ "BadAccessContext, insufficient permissions for operation",
     /* FSBadIDChoice  */ "BadIDChoice, invalid resource ID chosen for this connection",
     /* FSBadName	 */ "BadName, named font does not exist",
     /* FSBadResolution	 */ "BadResolution, improperly formatted resolution",
     /* FSBadAlloc	 */ "BadAlloc, insufficient resources for operation",
     /* FSBadLength	 */ "BadLength, request too large or internal FSlib length error",
     /* FSBadImplementation */ "BadImplementation, request unsupported",
};
static int FSErrorListSize = sizeof(FSErrorList);


/* ARGSUSED */
int FSGetErrorDatabaseText(
    FSServer		*svr,
    const char		*name,
    const char		*type,
    const char		*defaultp,
    char		*buffer,
    int			 nbytes)
{
    if (nbytes == 0)
	return 0;
    (void) strncpy(buffer, defaultp, nbytes);
    if ((strlen(defaultp) + 1) > nbytes)
	buffer[nbytes - 1] = '\0';
    return 1;
}

int FSGetErrorText(
    register FSServer	*svr,
    register int	 code,
    char		*buffer,
    int			 nbytes)
{

    const char *defaultp = NULL;
    char        buf[32];
    register _FSExtension *ext;

    if (nbytes == 0)
	return 0;
    snprintf(buf, sizeof(buf), "%d", code);
    if (code <= (FSErrorListSize / sizeof(char *)) && code > 0) {
	defaultp = FSErrorList[code];
	FSGetErrorDatabaseText(svr, "FSProtoError", buf, defaultp, buffer, nbytes);
    }
    ext = svr->ext_procs;
    while (ext) {		/* call out to any extensions interested */
	if (ext->error_string != NULL)
	    (*ext->error_string) (svr, code, &ext->codes, buffer, nbytes);
	ext = ext->next;
    }
    return 1;
}

