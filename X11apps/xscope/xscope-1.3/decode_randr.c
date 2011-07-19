/*
 * Copyright © 2001 Keith Packard, member of The XFree86 Project, Inc.
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


#include <stdio.h>
#include <X11/X.h>
#include <X11/Xproto.h>
#define _RANDR_SERVER_
#include "scope.h"
#include "x11.h"
#include "randrscope.h"
#include "extensions.h"

static unsigned char RANDRRequest, RANDRError, RANDREvent;

static void
randr_decode_req (
    FD fd,
    const unsigned char *buf)
{
  short Major = IByte (&buf[0]);
  short Minor = IByte (&buf[1]);

  switch (Minor) {
  case 0: RandrQueryVersion (fd, buf); ExtendedReplyExpected (fd, Major, Minor); break;
  case 1: RandrGetScreenInfo (fd, buf); ExtendedReplyExpected (fd, Major, Minor); break;
  case 2: RandrSetScreenConfig (fd, buf); ExtendedReplyExpected (fd, Major, Minor); break;
  case 3: RandrScreenChangeSelectInput (fd, buf);
  default:
    ExtendedRequest(fd, buf);
    ExtendedReplyExpected(fd, Major, Minor);
    break;
  }
}

static void
randr_decode_reply (
    FD fd,
    const unsigned char *buf,
    short RequestMinor)
{
    switch (RequestMinor) {
    case 0: RandrQueryVersionReply (fd, buf); break;
    case 1: RandrGetScreenInfoReply (fd, buf); break;
    case 2: RandrSetScreenConfigReply (fd, buf); break;
    default: UnknownReply(buf); break;
    }
}

static void
randr_decode_event (
    FD fd,
    const unsigned char *buf)
{
  RandrScreenChangeNotifyEvent (buf);
}

void
InitializeRANDR (
    const unsigned char *buf)
{
  TYPE    p;

  RANDRRequest = (unsigned char)(buf[9]);
  RANDREvent = (unsigned char)(buf[10]);
  RANDRError = (unsigned char)(buf[11]);

  DefineEValue (&TD[REQUEST], (unsigned long) RANDRRequest, "RandrRequest");
  DefineEValue (&TD[REPLY], (unsigned long) RANDRRequest, "RandrReply");
  DefineEValue (&TD[EVENT], (unsigned long) RANDREvent, "RRScreenChangeNotify");

  p = DefineType(RANDRREQUEST, ENUMERATED, "RANDRREQUEST", (PrintProcType) PrintENUMERATED);
  DefineEValue(p, 0L, "RandrQueryVersion");
  DefineEValue(p, 1L, "RandrGetScreenInfo");
  DefineEValue(p, 2L, "RandrSetScreenConfig");
  DefineEValue(p, 3L, "RandrScreenChangeSelectInput");

  p = DefineType(RANDRREPLY, ENUMERATED, "RANDRREPLY", (PrintProcType) PrintENUMERATED);
  DefineEValue (p, 0L, "QueryVersion");
  DefineEValue (p, 1L, "GetScreenInfo");
  DefineEValue (p, 2L, "SetScreenConfig");

  InitializeExtensionDecoder(RANDRRequest, randr_decode_req,
			     randr_decode_reply);
  /* Not yet implemented:
     InitializeExtensionErrorDecoder(RANDRError, randr_decode_error); */
  InitializeExtensionEventDecoder(RANDREvent, randr_decode_event);

}
