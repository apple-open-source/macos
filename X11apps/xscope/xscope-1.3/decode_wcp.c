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

#include <stdio.h>
#include <X11/X.h>
#include <X11/Xproto.h>
#define _WCP_SERVER_
#include "scope.h"
#include "x11.h"
#include "wcpscope.h"
#include "extensions.h"

static unsigned char WCPRequest, WCPError;

static void
wcp_decode_req (
    FD fd,
    const unsigned char *buf)
{
  short Major = IByte (&buf[0]);
  short Minor = IByte (&buf[1]);

  switch (Minor) {
  case 0:
    WcpQueryVersion (fd, buf);
    ExtendedReplyExpected (fd, Major, Minor);
    break;
  case 1:
    WcpPutImage (fd, buf);
    break;
  case 2:
    WcpGetImage (fd, buf);
    ExtendedReplyExpected (fd, Major, Minor);
    break;
  case 3:
    WcpCreateColorCursor (fd, buf);
    break;
  case 4:
    WcpCreateLut (fd, buf);
    break;
  case 5:
    WcpFreeLut (fd, buf);
    break;
  case 6:
    WcpCopyArea (fd, buf);
    break;
  default:
    break;
  }
}

static void
wcp_decode_reply (
    FD fd,
    const unsigned char *buf,
    short RequestMinor)
{
    switch (RequestMinor) {
    case 0:
	WcpQueryVersionReply (fd, buf);
	break;
    case 2:
	WcpGetImageReply (fd, buf);
	break;
    default:
	break;
    }
}

static void
wcp_decode_error (
    FD fd,
    const unsigned char *buf)
{
    short error = IByte(&buf[1]) - WCPError;
  
    switch (error) {
    case 0:
	break;
    default:
	break;
    }
}

void
InitializeWCP (
    const unsigned char *buf)
{
  TYPE    p;

  WCPRequest = (unsigned char)(buf[9]);
  WCPError = (unsigned char)(buf[11]);

  DefineEValue (&TD[REQUEST], (unsigned long) WCPRequest, "WcpRequest");
  DefineEValue (&TD[REPLY], (unsigned long) WCPRequest, "WcpReply");
  DefineEValue (&TD[ERROR], (unsigned long) WCPError, "WcpError");

  p = DefineType(WCPREQUEST, ENUMERATED, "WCPREQUEST", (PrintProcType) PrintENUMERATED);
  DefineEValue(p, 0L, "WcpQueryVersion");
  DefineEValue(p, 1L, "WcpPutImage");
  DefineEValue(p, 2L, "WcpGetImage");
  DefineEValue(p, 3L, "WcpCreateColorCursor");
  DefineEValue(p, 4L, "WcpCreateLut");
  DefineEValue(p, 5L, "WcpFreeLut");
  DefineEValue(p, 6L, "WcpCopyArea");

  p = DefineType(WCPREPLY, ENUMERATED, "WCPREPLY", (PrintProcType) PrintENUMERATED);
  DefineEValue (p, 0L, "QueryVersion");

  InitializeExtensionDecoder(WCPRequest, wcp_decode_req, wcp_decode_reply);
  InitializeExtensionErrorDecoder(WCPError, wcp_decode_error);
}
