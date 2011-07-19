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
#include "shmscope.h"
#include "extensions.h"

static unsigned char MITSHMRequest, MITSHMError, MITSHMEvent;

static void
mitshm_decode_req (
    FD fd,
    const unsigned char *buf)
{
  short Major = IByte (&buf[0]);
  short Minor = IByte (&buf[1]);

  switch (Minor) {
  case 0: MitshmQueryVersion (fd, buf); ExtendedReplyExpected (fd, Major, Minor); break;
  case 1: MitshmAttach (fd, buf); break;
  case 2: MitshmDetach (fd, buf); break;
  case 3: MitshmPutImage (fd, buf); break;
  case 4: MitshmGetImage (fd, buf); ExtendedReplyExpected (fd, Major, Minor); break;
  case 5: MitshmCreatePixmap (fd, buf); break;
  default:
    break;
  }
}

static void
mitshm_decode_reply (
    FD fd,
    const unsigned char *buf,
    short RequestMinor)
{
    switch (RequestMinor) {
    case 0: MitshmQueryVersionReply (fd, buf); break;
    case 4: MitshmGetImageReply (fd, buf); break;
    }
}

static void
mitshm_decode_event (
    FD fd,
    const unsigned char *buf)
{
}
  
static void
mitshm_decode_error (
    FD fd,
    const unsigned char *buf)
{
    short error = IByte(&buf[0]) - MITSHMError;
  
    switch (error) {
    case 0: MitshmShmSegError (fd, buf); break;
    default:
	break;
    }
}


void
InitializeMITSHM (
    const unsigned char *buf)
{
  TYPE    p;

  MITSHMRequest = (unsigned char)(buf[9]);
  MITSHMEvent = (unsigned char)(buf[10]);
  MITSHMError = (unsigned char)(buf[11]);

  DefineEValue (&TD[REQUEST], (unsigned long) MITSHMRequest, "MitshmRequest");
  DefineEValue (&TD[REPLY], (unsigned long) MITSHMRequest, "MitshmReply");
  DefineEValue (&TD[EVENT], (unsigned long) MITSHMEvent, "MitshmEvent");
  DefineEValue (&TD[ERROR], (unsigned long) MITSHMError, "MitshmError");

  p = DefineType(MITSHMREQUEST, ENUMERATED, "MITSHMREQUEST", (PrintProcType) PrintENUMERATED);
  DefineEValue(p, 0L, "MitshmQueryVersion");
  DefineEValue(p, 1L, "MitshmAttach");
  DefineEValue(p, 2L, "MitshmDetach");
  DefineEValue(p, 3L, "MitshmPutImage");
  DefineEValue(p, 4L, "MitshmGetImage");
  DefineEValue(p, 5L, "MitshmCreatePixmap");

  p = DefineType(MITSHMREPLY, ENUMERATED, "MITSHMREPLY", (PrintProcType) PrintENUMERATED);
  DefineEValue (p, 0L, "QueryVersion");
  DefineEValue (p, 4L, "GetImage");

  p = DefineType(MITSHMEVENT, ENUMERATED, "MITSHMEVENT", (PrintProcType) PrintENUMERATED);
  DefineEValue (p, 0L, "CompletionEvent");

  InitializeExtensionDecoder(MITSHMRequest, mitshm_decode_req,
			     mitshm_decode_reply);
  InitializeExtensionErrorDecoder(MITSHMError, mitshm_decode_error);
  InitializeExtensionEventDecoder(MITSHMEvent, mitshm_decode_event);
}
