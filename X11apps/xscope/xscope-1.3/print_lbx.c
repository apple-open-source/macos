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
 
#include "scope.h"
#include "x11.h"
#include "lbxscope.h"

static unsigned long	sequences[256];
static unsigned char	starting_server[256];
static unsigned char    starting_client[256];
static int		client_client, server_client;

void
LbxQueryVersion (
    FD fd,
    const unsigned char *buf)
{
  PrintField (buf, 0, 1, REQUEST, REQUESTHEADER) /* LbxRequest */ ;
  PrintField (buf, 1, 1, LBXREQUEST, LBXREQUESTHEADER) /* LbxSwitch */ ;
  if (Verbose < 1)
    return;
  if (Verbose > 1)
    PrintField(SBf, 0, 4, CARD32, "sequence number");

  printreqlen(buf, fd, CONST2(2));
}

void
LbxQueryVersionReply (
    FD fd,
    const unsigned char *buf)
{
  PrintField(RBf, 0, 1, REPLY, REPLYHEADER) /* LbxRequest reply */ ;
  PrintField(RBf, 1, 1, LBXREPLY, LBXREPLYHEADER) /* LbxQueryVersion reply */;
  if (Verbose < 1)
    return;
  printfield(buf, 2, 2, CARD16, "sequence number");
  printfield(buf, 4, 4, DVALUE4(0), "reply length");
  PrintField(buf, 8, 2, CARD16, "major-version");
  PrintField(buf, 10, 2, CARD16, "minor-version");
}
    
void
LbxStartProxy (
    FD fd,
    const unsigned char *buf)
{
  PrintField (buf, 0, 1, REQUEST, REQUESTHEADER) /* LbxRequest */ ;
  PrintField (buf, 1, 1, LBXREQUEST, LBXREQUESTHEADER) /* LbxSwitch */ ;
  if (Verbose < 1)
    return;
  if (Verbose > 1)
    PrintField(SBf, 0, 4, CARD32, "sequence number");

  printreqlen(buf, fd, CONST2(2));
}

void
LbxStopProxy (
    FD fd,
    const unsigned char *buf)
{
  PrintField (buf, 0, 1, REQUEST, REQUESTHEADER) /* LbxRequest */ ;
  PrintField (buf, 1, 1, LBXREQUEST, LBXREQUESTHEADER) /* LbxSwitch */ ;
  if (Verbose < 1)
    return;
  if (Verbose > 1)
    PrintField(SBf, 0, 4, CARD32, "sequence number");

  printreqlen(buf, fd, CONST2(2));
}

void
LbxNewClient (
    FD fd,
    const unsigned char *buf)
{
  unsigned long   c;

  c = ILong(&buf[4]);
  starting_client[c] = 1;
  starting_server[c] = 1;
  sequences[c] = 0;

  PrintField (buf, 0, 1, REQUEST, REQUESTHEADER) /* LbxRequest */ ;
  PrintField (buf, 1, 1, LBXREQUEST, LBXREQUESTHEADER) /* LbxSwitch */ ;
  if (Verbose < 1)
    return;
  if (Verbose > 1)
    PrintField(SBf, 0, 4, CARD32, "sequence number");

  printreqlen(buf, fd, CONST2(2));
  PrintField(buf, 4, 4, CARD32, "new-client-id");
}

void
LbxCloseClient (
    FD fd,
    const unsigned char *buf)
{
}

void
LbxSwitch (
    FD fd,
    const unsigned char *buf)
{
  unsigned long	c;
  c = ILong(&buf[4]);
  CS[fd].SequenceNumber--;
  sequences[client_client] = CS[fd].SequenceNumber;
  if (starting_client[c]) 
  {
    CS[fd].ByteProcessing = StartSetUpMessage;
    CS[fd].NumberofBytesNeeded = 12;
    starting_client[c] = 0;
  }
  CS[fd].SequenceNumber = sequences[c];
  client_client = c;


  PrintField (buf, 0, 1, REQUEST, REQUESTHEADER) /* LbxRequest */ ;
  PrintField (buf, 1, 1, LBXREQUEST, LBXREQUESTHEADER) /* LbxSwitch */ ;
  if (Verbose < 1)
    return;
  if (Verbose > 1)
    PrintField(SBf, 0, 4, CARD32, "sequence number");

  printreqlen(buf, fd, CONST2(2));
  PrintField(buf, 4, 4, CARD32, "client number");
}

void
LbxModifySequence (
    FD fd,
    const unsigned char *buf)
{
  int	mod;

  mod = ILong(&buf[4]);
  CS[fd].SequenceNumber += mod;

  PrintField (buf, 0, 1, REQUEST, REQUESTHEADER) /* LbxRequest */ ;
  PrintField (buf, 1, 1, LBXREQUEST, LBXREQUESTHEADER) /* LbxModifySequence */ ;
  if (Verbose < 1)
    return;
  if (Verbose > 1)
    PrintField(SBf, 0, 4, CARD32, "sequence number");

  printreqlen(buf, fd, CONST2(2));
  printfield(buf, 4, 4, INT32, "adjustment");
}

void
LbxSwitchEvent (
    FD fd,
    const unsigned char *buf)
{
  unsigned long	c;

  c = ILong(&buf[4]);
  if (starting_server[c])
  {
    CS[fd].ByteProcessing = StartSetUpReply;
    CS[fd].NumberofBytesNeeded = 8;
    starting_server[c] = 0;
  }

  PrintField(buf, 0, 1, EVENT, EVENTHEADER) /* LbxEvent */ ;
  PrintField(buf, 1, 1, LBXEVENT, LBXEVENTHEADER) /* LbxSwitchEvent */ ;
  if (Verbose < 1)
    return;
  printfield(buf, 2, 2, CARD16, "sequence number");
  PrintField(buf, 4, 4, CARD32, "client");
}

void
LbxCloseEvent (
    FD fd,
    const unsigned char *buf)
{
  PrintField(buf, 0, 1, EVENT, EVENTHEADER) /* LbxEvent */ ;
  PrintField(buf, 1, 1, LBXEVENT, LBXEVENTHEADER) /* LbxSwitchEvent */ ;
  if (Verbose < 1)
    return;
  printfield(buf, 2, 2, CARD16, "sequence number");
  PrintField(buf, 4, 4, CARD32, "client");
}
