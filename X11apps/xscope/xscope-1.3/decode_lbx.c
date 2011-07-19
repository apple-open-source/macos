/*
 * Copyright 1992 Network Computing Devices
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
#define _XLBX_SERVER_
#include "scope.h"
#include "x11.h"
#include "lbxscope.h"
#include "extensions.h"

static unsigned char LBXRequest, LBXError;
unsigned char LBXEvent; /* exported for DecodeEvent in decode11.c */

static void
lbx_decode_req (
    FD fd,
    const unsigned char *buf)
{
  short Major = IByte (&buf[0]);
  short Minor = IByte (&buf[1]);

  switch (Minor) {
  case 0:
    LbxQueryVersion (fd, buf);
    ExtendedReplyExpected (fd, Major, Minor);
    break;
  case 1:
    LbxStartProxy (fd, buf);
    break;
  case 2:
    LbxStopProxy (fd, buf);
    break;
  case 3:
    LbxSwitch (fd, buf);
    break;
  case 4:
    LbxNewClient(fd, buf);
    break;
  case 5:
    LbxCloseClient (fd, buf);
    break;
  case 6:
    LbxModifySequence (fd, buf);
    break;
  default:
    break;
  }
}

static void
lbx_decode_reply (
    FD fd,
    const unsigned char *buf,
    short RequestMinor)
{
    switch (RequestMinor) {
    case 0:
	LbxQueryVersionReply (fd, buf);
	break;
    default:
	break;
    }
}

static void
lbx_decode_error (
    FD fd,
    const unsigned char *buf)
{
    short error = IByte(&buf[1]) - LBXError;
  
    switch (error) {
    case 0:
	break;
    default:
	break;
    }
}

static void
lbx_decode_event (
    FD  fd,
    const unsigned char *buf)
{
  short	event = IByte(&buf[0]) - LBXEvent;

  switch (event) {
  case 0:
    LbxSwitchEvent (fd, buf);
    break;
  case 1:
    LbxCloseEvent (fd, buf);
    break;
  default:
    break;
  }
}

void
InitializeLBX (
    const unsigned char *buf)
{
  TYPE    p;

  LBXRequest = (unsigned char)(buf[9]);
  LBXEvent = (unsigned char)(buf[10]);
  LBXError = (unsigned char)(buf[11]);

  DefineEValue (&TD[REQUEST], (unsigned long) LBXRequest, "LbxRequest");
  DefineEValue (&TD[REPLY], (unsigned long) LBXRequest, "LbxReply");
  DefineEValue (&TD[EVENT], (unsigned long) LBXEvent, "LbxEvent");
  DefineEValue (&TD[ERROR], (unsigned long) LBXError, "LbxError");

  p = DefineType(LBXREQUEST, ENUMERATED, "LBXREQUEST", (PrintProcType) PrintENUMERATED);
  DefineEValue(p, 0L, "QueryVersion");
  DefineEValue(p, 1L, "StartProxy");
  DefineEValue(p, 2L, "StopProxy");
  DefineEValue(p, 3L, "Switch");
  DefineEValue(p, 4L, "NewClient");
  DefineEValue(p, 5L, "CloseClient");
  DefineEValue(p, 6L, "ModifySequence");

  p = DefineType(LBXREPLY, ENUMERATED, "LBXREPLY", (PrintProcType) PrintENUMERATED);
  DefineEValue (p, 0L, "QueryVersion");

  p = DefineType(LBXEVENT, ENUMERATED, "LBXEVENT", (PrintProcType) PrintENUMERATED);
  DefineEValue (p, 0L, "SwitchEvent");
  DefineEValue (p, 1L, "CloseEvent");

  InitializeExtensionDecoder(LBXRequest, lbx_decode_req, lbx_decode_reply);
  InitializeExtensionErrorDecoder(LBXError, lbx_decode_error);
  InitializeExtensionEventDecoder(LBXEvent, lbx_decode_event);
}
