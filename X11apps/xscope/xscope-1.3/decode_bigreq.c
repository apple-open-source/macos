/*
 * Copyright © 2000 Keith Packard, member of The XFree86 Project, Inc.
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
#include "scope.h"
#include "x11.h"
#include "bigreqscope.h"
#include "extensions.h"

static unsigned char BIGREQRequest;

static void
bigreq_decode_req (
    FD fd,
    const unsigned char *buf)
{
  short Major = IByte (&buf[0]);
  short Minor = IByte (&buf[1]);

  switch (Minor) {
  case 0: 
    CS[fd].bigreqEnabled = 1;
    BigreqEnable (fd, buf);
    ExtendedReplyExpected (fd, Major, Minor); break;
  default:
    break;
  }
}

static void
bigreq_decode_reply (
    FD fd,
    const unsigned char *buf,
    short RequestMinor)
{
    switch (RequestMinor) {
    case 0: BigreqEnableReply (fd, buf); break;
    }
}

void
InitializeBIGREQ(
    const unsigned char *buf)
{
  TYPE    p;

  BIGREQRequest = (unsigned char)(buf[9]);

  DefineEValue (&TD[REQUEST], (unsigned long) BIGREQRequest, "BigreqRequest");
  DefineEValue (&TD[REPLY], (unsigned long) BIGREQRequest, "BigreqReply");

  p = DefineType(BIGREQREQUEST, ENUMERATED, "BIGREQREQUEST", (PrintProcType) PrintENUMERATED);
  DefineEValue(p, 0L, "BigreqEnable");

  p = DefineType(BIGREQREPLY, ENUMERATED, "BIGREQREPLY", (PrintProcType) PrintENUMERATED);
  DefineEValue (p, 0L, "BigreqEnable");

  InitializeExtensionDecoder(BIGREQRequest, bigreq_decode_req,
			     bigreq_decode_reply);
}
