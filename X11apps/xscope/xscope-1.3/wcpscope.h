/*
 * Copyright 1996 Network Computing Devices
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of NCD. not be used in advertising or
 * publicity pertaining to distribution of the software without specific,
 * written prior permission.  NCD. makes no representations about the
 * suitability of this software for any purpose.  It is provided "as is"
 * without express or implied warranty.
 *
 * NCD. DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING ALL
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL NCD.
 * BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN 
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * Author:  Keith Packard, Network Computing Devices
 */

#ifndef _WCPSCOPE_H_
#define _WCPSCOPE_H_

#define WCPREQUESTHEADER  "WCPREQUEST"
#define WCPREPLYHEADER	  "WCPREPLY"

/*
  To aid in making the choice between level 1 and level 2, we
  define the following define, which does not print relatively
  unimportant fields.
*/

#define printfield(a,b,c,d,e) if (Verbose > 1) PrintField(a,b,c,d,e)

extern void WcpQueryVersion		(FD fd, const unsigned char *buf);
extern void WcpQueryVersionReply	(FD fd, const unsigned char *buf);
extern void WcpPutImage			(FD fd, const unsigned char *buf);
extern void WcpGetImage			(FD fd, const unsigned char *buf);
extern void WcpGetImageReply		(FD fd, const unsigned char *buf);
extern void WcpCreateColorCursor	(FD fd, const unsigned char *buf);
extern void WcpCreateLut		(FD fd, const unsigned char *buf);
extern void WcpFreeLut			(FD fd, const unsigned char *buf);
extern void WcpCopyArea			(FD fd, const unsigned char *buf);

#endif

