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

#include "scope.h"
#include "x11.h"
#include "randrscope.h"

void
RandrQueryVersion (FD fd, const unsigned char *buf)
{
  PrintField (buf, 0, 1, REQUEST, REQUESTHEADER) /* RandrRequest */ ;
  PrintField (buf, 1, 1, RANDRREQUEST, RANDRREQUESTHEADER) /* RandrSwitch */ ;
  if (Verbose < 1)
    return;
  if (Verbose > 1)
    PrintField(SBf, 0, 4, CARD32, "sequence number");

  printreqlen(buf, fd, CONST2(2));
}

void
RandrQueryVersionReply (FD fd, const unsigned char *buf)
{
  PrintField(RBf, 0, 1, REPLY, REPLYHEADER) /* RandrRequest reply */ ;
  PrintField(RBf, 1, 1, RANDRREPLY, RANDRREPLYHEADER) /* RandrQueryVersion reply */;
  if (Verbose < 1)
    return;
  printfield(buf, 2, 2, CARD16, "sequence number");
  printfield(buf, 4, 4, DVALUE4(0), "reply length");
  PrintField(buf, 8, 2, CARD16, "major-version");
  PrintField(buf, 10, 2, CARD16, "minor-version");
}

void
RandrGetScreenInfo (FD fd, const unsigned char *buf)
{
  PrintField (buf, 0, 1, REQUEST, REQUESTHEADER) /* RandrRequest */ ;
  PrintField (buf, 1, 1, RANDRREQUEST, RANDRREQUESTHEADER) /* RandrSwitch */ ;
  if (Verbose < 1)
    return;
  if (Verbose > 1)
    PrintField(SBf, 0, 4, CARD32, "sequence number");

  printreqlen(buf, fd, CONST2(2));
  PrintField(buf, 4, 4, WINDOW, "window");
}

void
RandrGetScreenInfoReply (FD fd, const unsigned char *buf)
{
  unsigned short  nsize;
  unsigned short  nvg;
  unsigned short  ngvg;

  PrintField(RBf, 0, 1, REPLY, REPLYHEADER) /* RandrRequest reply */ ;
  if (Verbose < 1)
    return;
  PrintField(buf, 1, 1, CARD8, "set-of-rotations");
  printfield(buf, 2, 2, CARD16, "sequence number");
  printfield(buf, 4, 4, DVALUE4(0), "reply length");
  PrintField(buf, 8, 4, WINDOW, "root");
  PrintField(buf,12, 4, TIMESTAMP, "timestamp");
  PrintField(buf,16, 4, TIMESTAMP, "config-timestamp");
  PrintField(buf,20, 2, CARD16, "num-visual-groups");
  nvg = IShort (&buf[20]);
  PrintField(buf,22, 2, CARD16, "num-groups-of-visual-groups");
  ngvg = IShort (&buf[22]);
  PrintField(buf,24, 2, CARD16, "num-sizes");
  nsize = IShort (&buf[24]);
  PrintField(buf,26, 2, CARD16, "size-id");
  PrintField(buf,28, 2, CARD16, "visual-group-id");
  PrintField(buf,30, 2, CARD16, "rotation");
}

void
RandrSetScreenConfig (FD fd, const unsigned char *buf)
{
  PrintField (buf, 0, 1, REQUEST, REQUESTHEADER) /* RandrRequest */ ;
  PrintField (buf, 1, 1, RANDRREQUEST, RANDRREQUESTHEADER) /* RandrSwitch */ ;
  if (Verbose < 1)
    return;
  if (Verbose > 1)
    PrintField(SBf, 0, 4, CARD32, "sequence number");

  printreqlen(buf, fd, CONST2(2));
  PrintField(buf, 4, 4, DRAWABLE, "drawable");
  PrintField(buf, 8, 4, TIMESTAMP, "timestamp");
  PrintField(buf,12, 4, TIMESTAMP, "config-timestamp");
  PrintField(buf,16, 2, CARD16, "size-id");
  PrintField(buf,18, 2, CARD16, "rotation");
  PrintField(buf,20, 2, CARD16, "visual-group-id");
}

void
RandrSetScreenConfigReply (FD fd, const unsigned char *buf)
{
  PrintField(RBf, 0, 1, REPLY, REPLYHEADER) /* RandrRequest reply */ ;
  PrintField(buf, 1, 1, BOOL, "success") /* RandrQueryVersion reply */;
  if (Verbose < 1)
    return;
  printfield(buf, 2, 2, CARD16, "sequence number");
  printfield(buf, 4, 4, DVALUE4(0), "reply length");
  PrintField(buf, 8, 4, TIMESTAMP, "new-timestamp");
  PrintField(buf,12, 4, TIMESTAMP, "new-config-timestamp");
  PrintField(buf,16, 4, WINDOW, "root");
}

void
RandrScreenChangeSelectInput (FD fd, const unsigned char *buf)
{
  PrintField (buf, 0, 1, REQUEST, REQUESTHEADER) /* RandrRequest */ ;
  PrintField (buf, 1, 1, RANDRREQUEST, RANDRREQUESTHEADER) /* RandrSwitch */ ;
  if (Verbose < 1)
    return;
  if (Verbose > 1)
    PrintField(SBf, 0, 4, CARD32, "sequence number");

  printreqlen(buf, fd, CONST2(2));
  PrintField(buf, 4, 4, WINDOW, "window");
  PrintField(buf, 8, 1, BOOL, "enable");
}

void
RandrScreenSizes (const unsigned char *buf)
{
  PrintField(buf, 0, 2, CARD16, "width-in-pixels");
  PrintField(buf, 2, 2, CARD16, "height-in-pixels");
  PrintField(buf, 4, 2, CARD16, "width-in-millimeters");
  PrintField(buf, 6, 2, CARD16, "height-in-millimeters");
  PrintField(buf, 8, 2, CARD16, "visual-group");
}

void
RandrScreenChangeNotifyEvent (const unsigned char *buf)
{
  PrintField(buf, 0, 1, EVENT, EVENTHEADER) /* RRScreenChangeNotify */ ;
  if (Verbose < 1)
    return;
  PrintField(buf, 1, 1, BOOL, "resident");
  printfield(buf, 2, 2, CARD16, "sequence number");
  PrintField(buf, 4, 4, TIMESTAMP, "timestamp");
  PrintField(buf, 8, 4, TIMESTAMP, "config-timestamp");
  PrintField(buf,12, 4, WINDOW, "root");
  PrintField(buf,16, 2, CARD16, "size id");
  PrintField(buf,18, 2, CARD16, "rotation");
  RandrScreenSizes (buf + 20);
}
