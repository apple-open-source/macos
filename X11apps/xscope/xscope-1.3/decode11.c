/* ************************************************************ *
 *						     		*
 *  Decoding and switching routines for the X11 protocol	*
 *						      		*
 *	James Peterson, 1988			      		*
 * Copyright (C) 1988 MCC
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of MCC not be used in
 * advertising or publicity pertaining to distribution of the software without
 * specific, written prior permission.  MCC makes no
 * representations about the suitability of this software for any purpose.  It
 * is provided "as is" without express or implied warranty.
 *
 * MCC DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL MCC BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 *						      		*
 *						      		*
 * Copyright (c) 2002, Oracle and/or its affiliates. All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * ************************************************************ */

#include "scope.h"
#include "x11.h"
#include "extensions.h"
#include "lbxscope.h"

/*
  There are 4 types of things in X11: requests, replies, errors, and events.

  Each of them has a format defined by a small integer that defines
  the type of the thing.

  Requests have an opcode in the first byte.
  Events have a code in the first byte.
  Errors have a code in the second byte (the first byte is 0)
  Replies ...

  Replies have a sequence number in bytes 2 and 3.  The sequence
  number should be used to identify the request that was sent, and
  from that request we can determine the type of the reply.
*/


/* ************************************************************ */
/*								*/
/*								*/
/* ************************************************************ */


/*
  We need to keep the sequence number for a request to match it with
  an expected reply.  The sequence number is associated only with the
  particular connection that we have. We would expect these replies
  to be handled as a FIFO queue.
*/

struct QueueEntry
{
  struct QueueEntry  *Next;
  long    SequenceNumber;
  short   Request;
  short	  RequestMinor;
};

/* free space list of Q entries */

static struct QueueEntry   *FreeQEntries = NULL;

/* ************************************************************ */
static struct QueueEntry  *
NewQEntry (
    long    SequenceNumber,
    short   Request,
    short   RequestMinor)
{
  struct QueueEntry  *p;

  /* Get a Queue Entry */
  if (FreeQEntries == NULL)
    {
      /* create new queue entry */
      p = (struct QueueEntry *) Malloc ((long)(sizeof (*p)));
    }
  else
    {
      /* reuse an old queue entry */
      p = FreeQEntries;
      FreeQEntries = FreeQEntries->Next;
    }

  /* fill in its values */
  p->Next = NULL;
  p->SequenceNumber = SequenceNumber;
  p->Request = Request;
  p->RequestMinor = RequestMinor;
  return(p);
}
/* ************************************************************ */

/* define a queue of entries for each FD */

struct QueueHeader
{
  struct QueueEntry  *Head;
  struct QueueEntry  *Tail;
};

static struct QueueHeader  ReplyQ[StaticMaxFD];

/* ************************************************************ */

void
InitReplyQ (void)
{
  short   i;
  for (i = 0; i < StaticMaxFD; i++)
    {
      ReplyQ[i].Head = NULL;
      ReplyQ[i].Tail = NULL;
    }
}

void
FlushReplyQ (
    FD fd)
{
  struct QueueEntry  *p;
  struct QueueEntry  *NextQEntry;

  /* go down the reply queue and free all entries */
  for (p = ReplyQ[fd].Head; p != NULL; p = NextQEntry)
    {
      NextQEntry = p->Next;
      
      /* put freed entry on list of free entries (for later reuse)  */
      p->Next = FreeQEntries;
      FreeQEntries = p;
    }
  
  ReplyQ[fd].Head = NULL;
  ReplyQ[fd].Tail = NULL;
}

static void
DumpReplyQ (
    FD fd)
{
  fprintf(stderr, "ReplyQ[%d] = { Head 0x%lx; Tail 0x%lx }\n",
	  fd, (unsigned long)ReplyQ[fd].Head, (unsigned long)ReplyQ[fd].Tail);
  {
    struct QueueEntry  *p;
    for (p = ReplyQ[fd].Head; p != NULL; p = p->Next)
      fprintf(stderr,
	      "0x%lx = { Next 0x%lx; SequenceNumber %ld; Request %d }\n",
	      (unsigned long) p, (unsigned long) p->Next,
	      (unsigned long) p->SequenceNumber, p->Request);
  }
}
/* ************************************************************ */
/*								*/
/*								*/
/* ************************************************************ */

/* A reply is expected to the type of request given for the fd associated
   with this one */

static void
SequencedReplyExpected (
    FD fd,
    long SequenceNumber,
    short   RequestType,
    short   RequestMinorType)
{
  struct QueueEntry  *p;

  debug(8,(stderr,
	   "Reply expected: sequence %ld and request type %d,%d for fd %d\n",
	   SequenceNumber, RequestType, RequestMinorType, fd));
  /* create a new queue entry */
  p = NewQEntry(SequenceNumber, RequestType, RequestMinorType);

  /* find the server associated with this client */
  fd = FDPair(fd);
  if (fd < 0 || fd >= StaticMaxFD) return;

  /* attach the new queue entry to the end of the queue for the Server */
  if (ReplyQ[fd].Tail != NULL)
    (ReplyQ[fd].Tail)->Next = p;
  else
    ReplyQ[fd].Head = p;
  ReplyQ[fd].Tail = p;

  debug(8,(stderr, "Save sequence %ld and request type %d,%d for fd %d\n",
	   p->SequenceNumber, p->Request, p->RequestMinor, fd));
}


static FD Lastfd;
static long LastSequenceNumber;
static short LastReplyType;
static short LastReplyMinorType;

/* search for the type of request that is associated with a reply
   to the given sequence number for this fd */

short
CheckReplyTable (
    FD fd,
    short   SequenceNumber,
    short  *minorp)
{
  struct QueueEntry  *p;
  struct QueueEntry  *trailer;

  if (debuglevel & 128) DumpReplyQ(fd);
  for (trailer = NULL, p = ReplyQ[fd].Head;
       p != NULL;
       trailer = p, p = p->Next)
    {
      /* look for matching sequence number in queue of this fd */
      if (SequenceNumber == ((short)(0xFFFF & p->SequenceNumber)))
	{
	  /* save the Request type */
	  Lastfd = fd;
	  LastSequenceNumber = p->SequenceNumber;
	  LastReplyType = p->Request;
	  LastReplyMinorType = p->RequestMinor;

	  /* pull the queue entry out of the queue for this fd */
	  if (trailer == NULL)
	      ReplyQ[fd].Head = p->Next;
	  else
	    trailer->Next = p->Next;
	  if (ReplyQ[fd].Tail == p) ReplyQ[fd].Tail = trailer;


	  /* put freed entry on list of free entries (for later reuse)  */
	  p->Next = FreeQEntries;
	  FreeQEntries = p;

	  debug(8,(stderr, "Reply on fd %d for sequence %d is type %d,%x\n",
		   fd, SequenceNumber, LastReplyType, LastReplyMinorType));
	  *minorp = LastReplyMinorType;
	  return(LastReplyType);
	}
    }

  /* not expecting a reply for that sequence number */
  debug(8,(stderr, "Reply on fd %d for sequence %d is not found\n",
	   fd, SequenceNumber));
  return(0);
}


/* ************************************************************ */
/* A reply is expected to the type of request given for the
   sequence number associated with this fd */

void
ReplyExpected (
    FD fd,
    short   Request)
{
  SequencedReplyExpected(fd, CS[fd].SequenceNumber, Request, 0);
}

void
ExtendedReplyExpected (
    FD fd,
    short   Request,
    short   RequestMinor)
{
  SequencedReplyExpected(fd, CS[fd].SequenceNumber, Request, RequestMinor);
}

/* ************************************************************ */
/* another reply is expected for the same reply as we just had */
/* This is only used with ListFontsWithInfo */

void 
KeepLastReplyExpected (void)
{
  SequencedReplyExpected(Lastfd, LastSequenceNumber, LastReplyType, 
			 LastReplyMinorType);
}

#define DUMP_REQUEST	0
#define DUMP_REPLY	1
#define DUMP_EVENT	2
#define DUMP_ERROR	3

static const char * const simple_names[] = {
    "REQUEST",
    "REPLY  ",
    "EVENT  ",
    "ERROR  ",
};

static void
SimpleDump (int type, FD fd, short Major, short Minor, long bytes)
{
    PrintTime ();
    fprintf (stdout, "@@%s %3d %3d %3d %7ld\n",
	     simple_names[type],
	     ClientNumber(fd),
	     Major, Minor, bytes);
}
	    
/* ************************************************************ */
/*								*/
/*								*/
/* ************************************************************ */

void
DecodeRequest (
    FD fd,
    const unsigned char *buf,
    long    n)
{
  short   Request = IByte (&buf[0]);
  short	  RequestMinor = Request >= 128 ? IByte (&buf[1]) : 0;
  unsigned long	seq;
    
  CS[fd].SequenceNumber += 1;
  seq = CS[fd].SequenceNumber;
  if (CS[fd].littleEndian) {
    SBf[0] = seq;
    SBf[1] = seq >> 8;
    SBf[2] = seq >> 16;
    SBf[3] = seq >> 24;
  } else {
    SBf[0] = seq >> 24;
    SBf[1] = seq >> 16;
    SBf[2] = seq >> 8;
    SBf[3] = seq;
  }
  SetIndentLevel(PRINTCLIENT);

  /* Special handling of QueryExtension to save extension names */
  if (Request == 98)
  {
    ProcessQueryExtensionRequest(seq, buf);
  }

  if (Verbose < 0)
  {
    SimpleDump (DUMP_REQUEST, fd, Request, RequestMinor, n);
    return;
  }
  
  if (Raw || (Verbose > 3))
    DumpItem("Request", fd, buf, n);

  if (Request < 0 || 127 < Request)
  {
    ExtensionRequest(fd, buf, Request);
  }
  else switch (Request)
  {
	    case 1:
		    CreateWindow(fd, buf);
		    break;
	    case 2:
		    ChangeWindowAttributes(fd, buf);
		    break;
	    case 3:
		    GetWindowAttributes(fd, buf);
		    ReplyExpected(fd, Request);
		    break;
	    case 4:
		    DestroyWindow(fd, buf);
		    break;
	    case 5:
		    DestroySubwindows(fd, buf);
		    break;
	    case 6:
		    ChangeSaveSet(fd, buf);
		    break;
	    case 7:
		    ReparentWindow(fd, buf);
		    break;
	    case 8:
		    MapWindow(fd, buf);
		    break;
	    case 9:
		    MapSubwindows(fd, buf);
		    break;
	    case 10:
		    UnmapWindow(fd, buf);
		    break;
	    case 11:
		    UnmapSubwindows(fd, buf);
		    break;
	    case 12:
		    ConfigureWindow(fd, buf);
		    break;
	    case 13:
		    CirculateWindow(fd, buf);
		    break;
	    case 14:
		    GetGeometry(fd, buf);
		    ReplyExpected(fd, Request);
		    break;
	    case 15:
		    QueryTree(fd, buf);
		    ReplyExpected(fd, Request);
		    break;
	    case 16:
		    InternAtom(fd, buf);
		    ReplyExpected(fd, Request);
		    break;
	    case 17:
		    GetAtomName(fd, buf);
		    ReplyExpected(fd, Request);
		    break;
	    case 18:
		    ChangeProperty(fd, buf);
		    break;
	    case 19:
		    DeleteProperty(fd, buf);
		    break;
	    case 20:
		    GetProperty(fd, buf);
		    ReplyExpected(fd, Request);
		    break;
	    case 21:
		    ListProperties(fd, buf);
		    ReplyExpected(fd, Request);
		    break;
	    case 22:
		    SetSelectionOwner(fd, buf);
		    break;
	    case 23:
		    GetSelectionOwner(fd, buf);
		    ReplyExpected(fd, Request);
		    break;
	    case 24:
		    ConvertSelection(fd, buf);
		    break;
	    case 25:
		    SendEvent(fd, buf);
		    break;
	    case 26:
		    GrabPointer(fd, buf);
		    ReplyExpected(fd, Request);
		    break;
	    case 27:
		    UngrabPointer(fd, buf);
		    break;
	    case 28:
		    GrabButton(fd, buf);
		    break;
	    case 29:
		    UngrabButton(fd, buf);
		    break;
	    case 30:
		    ChangeActivePointerGrab(fd, buf);
		    break;
	    case 31:
		    GrabKeyboard(fd, buf);
		    ReplyExpected(fd, Request);
		    break;
	    case 32:
		    UngrabKeyboard(fd, buf);
		    break;
	    case 33:
		    GrabKey(fd, buf);
		    break;
	    case 34:
		    UngrabKey(fd, buf);
		    break;
	    case 35:
		    AllowEvents(fd, buf);
		    break;
	    case 36:
		    GrabServer(fd, buf);
		    break;
	    case 37:
		    UngrabServer(fd, buf);
		    break;
	    case 38:
		    QueryPointer(fd, buf);
		    ReplyExpected(fd, Request);
		    break;
	    case 39:
		    GetMotionEvents(fd, buf);
		    ReplyExpected(fd, Request);
		    break;
	    case 40:
		    TranslateCoordinates(fd, buf);
		    ReplyExpected(fd, Request);
		    break;
	    case 41:
		    WarpPointer(fd, buf);
		    break;
	    case 42:
		    SetInputFocus(fd, buf);
		    break;
	    case 43:
		    GetInputFocus(fd, buf);
		    ReplyExpected(fd, Request);
		    break;
	    case 44:
		    QueryKeymap(fd, buf);
		    ReplyExpected(fd, Request);
		    break;
	    case 45:
		    OpenFont(fd, buf);
		    break;
	    case 46:
		    CloseFont(fd, buf);
		    break;
	    case 47:
		    QueryFont(fd, buf);
		    ReplyExpected(fd, Request);
		    break;
	    case 48:
		    QueryTextExtents(fd, buf);
		    ReplyExpected(fd, Request);
		    break;
	    case 49:
		    ListFonts(fd, buf);
		    ReplyExpected(fd, Request);
		    break;
	    case 50:
		    ListFontsWithInfo(fd, buf);
		    ReplyExpected(fd, Request);
		    break;
	    case 51:
		    SetFontPath(fd, buf);
		    break;
	    case 52:
		    GetFontPath(fd, buf);
		    ReplyExpected(fd, Request);
		    break;
	    case 53:
		    CreatePixmap(fd, buf);
		    break;
	    case 54:
		    FreePixmap(fd, buf);
		    break;
	    case 55:
		    CreateGC(fd, buf);
		    break;
	    case 56:
		    ChangeGC(fd, buf);
		    break;
	    case 57:
		    CopyGC(fd, buf);
		    break;
	    case 58:
		    SetDashes(fd, buf);
		    break;
	    case 59:
		    SetClipRectangles(fd, buf);
		    break;
	    case 60:
		    FreeGC(fd, buf);
		    break;
	    case 61:
		    ClearArea(fd, buf);
		    break;
	    case 62:
		    CopyArea(fd, buf);
		    break;
	    case 63:
		    CopyPlane(fd, buf);
		    break;
	    case 64:
		    PolyPoint(fd, buf);
		    break;
	    case 65:
		    PolyLine(fd, buf);
		    break;
	    case 66:
		    PolySegment(fd, buf);
		    break;
	    case 67:
		    PolyRectangle(fd, buf);
		    break;
	    case 68:
		    PolyArc(fd, buf);
		    break;
	    case 69:
		    FillPoly(fd, buf);
		    break;
	    case 70:
		    PolyFillRectangle(fd, buf);
		    break;
	    case 71:
		    PolyFillArc(fd, buf);
		    break;
	    case 72:
		    PutImage(fd, buf);
		    break;
	    case 73:
		    GetImage(fd, buf);
		    ReplyExpected(fd, Request);
		    break;
	    case 74:
		    PolyText8(fd, buf);
		    break;
	    case 75:
		    PolyText16(fd, buf);
		    break;
	    case 76:
		    ImageText8(fd, buf);
		    break;
	    case 77:
		    ImageText16(fd, buf);
		    break;
	    case 78:
		    CreateColormap(fd, buf);
		    break;
	    case 79:
		    FreeColormap(fd, buf);
		    break;
	    case 80:
		    CopyColormapAndFree(fd, buf);
		    break;
	    case 81:
		    InstallColormap(fd, buf);
		    break;
	    case 82:
		    UninstallColormap(fd, buf);
		    break;
	    case 83:
		    ListInstalledColormaps(fd, buf);
		    ReplyExpected(fd, Request);
		    break;
	    case 84:
		    AllocColor(fd, buf);
		    ReplyExpected(fd, Request);
		    break;
	    case 85:
		    AllocNamedColor(fd, buf);
		    ReplyExpected(fd, Request);
		    break;
	    case 86:
		    AllocColorCells(fd, buf);
		    ReplyExpected(fd, Request);
		    break;
	    case 87:
		    AllocColorPlanes(fd, buf);
		    ReplyExpected(fd, Request);
		    break;
	    case 88:
		    FreeColors(fd, buf);
		    break;
	    case 89:
		    StoreColors(fd, buf);
		    break;
	    case 90:
		    StoreNamedColor(fd, buf);
		    break;
	    case 91:
		    QueryColors(fd, buf);
		    ReplyExpected(fd, Request);
		    break;
	    case 92:
		    LookupColor(fd, buf);
		    ReplyExpected(fd, Request);
		    break;
	    case 93:
		    CreateCursor(fd, buf);
		    break;
	    case 94:
		    CreateGlyphCursor(fd, buf);
		    break;
	    case 95:
		    FreeCursor(fd, buf);
		    break;
	    case 96:
		    RecolorCursor(fd, buf);
		    break;
	    case 97:
		    QueryBestSize(fd, buf);
		    ReplyExpected(fd, Request);
		    break;
	    case 98:
		    QueryExtension(fd, buf);
		    ReplyExpected(fd, Request);
		    break;
	    case 99:
		    ListExtensions(fd, buf);
		    ReplyExpected(fd, Request);
		    break;
	    case 100:
		    ChangeKeyboardMapping(fd, buf);
		    break;
	    case 101:
		    GetKeyboardMapping(fd, buf);
		    ReplyExpected(fd, Request);
		    break;
	    case 102:
		    ChangeKeyboardControl(fd, buf);
		    break;
	    case 103:
		    GetKeyboardControl(fd, buf);
		    ReplyExpected(fd, Request);
		    break;
	    case 104:
		    Bell(fd, buf);
		    break;
	    case 105:
		    ChangePointerControl(fd, buf);
		    break;
	    case 106:
		    GetPointerControl(fd, buf);
		    ReplyExpected(fd, Request);
		    break;
	    case 107:
		    SetScreenSaver(fd, buf);
		    break;
	    case 108:
		    GetScreenSaver(fd, buf);
		    ReplyExpected(fd, Request);
		    break;
	    case 109:
		    ChangeHosts(fd, buf);
		    break;
	    case 110:
		    ListHosts(fd, buf);
		    ReplyExpected(fd, Request);
		    break;
	    case 111:
		    SetAccessControl(fd, buf);
		    break;
	    case 112:
		    SetCloseDownMode(fd, buf);
		    break;
	    case 113:
		    KillClient(fd, buf);
		    break;
	    case 114:
		    RotateProperties(fd, buf);
		    break;
	    case 115:
		    ForceScreenSaver(fd, buf);
		    break;
	    case 116:
		    SetPointerMapping(fd, buf);
		    ReplyExpected(fd, Request);
		    break;
	    case 117:
		    GetPointerMapping(fd, buf);
		    ReplyExpected(fd, Request);
		    break;
	    case 118:
		    SetModifierMapping(fd, buf);
		    ReplyExpected(fd, Request);
		    break;
	    case 119:
		    GetModifierMapping(fd, buf);
		    ReplyExpected(fd, Request);
		    break;
	    case 127:
		    NoOperation(fd, buf);
		    break;
	    default:
		    warn("Unimplemented request opcode");
		    break;
   }
}

/* ************************************************************ */
/*								*/
/*								*/
/* ************************************************************ */

void
DecodeReply (
    FD fd,
    const unsigned char *buf,
    long    n)
{
  short   SequenceNumber = IShort (&buf[2]);
  short	  RequestMinor;
  short   Request = CheckReplyTable (fd, SequenceNumber, &RequestMinor);
  
  /* Special handling of QueryExtension to save extension names */
  if (Request == 98)
  {
    ProcessQueryExtensionReply(SequenceNumber, buf);
  }

  if (Verbose < 0)
  {
    SimpleDump (DUMP_REPLY, fd, Request, RequestMinor, n);
    return;
  }
    
  SetIndentLevel(PRINTSERVER);
  RBf[0] = Request /* for the PrintField in the Reply procedure */ ;
  RBf[1] = RequestMinor;
  if (Raw || (Verbose > 3))
    DumpItem("Reply", fd, buf, n);
  if (Request < 0 || 127 < Request)
  {
    ExtensionReply(fd, buf, Request, RequestMinor);
  }
  else switch (Request)
    {
	    case 0:
		    UnknownReply(buf);
		    break;
	    case 3:
		    GetWindowAttributesReply(buf);
		    break;
	    case 14:
		    GetGeometryReply(buf);
		    break;
	    case 15:
		    QueryTreeReply(buf);
		    break;
	    case 16:
		    InternAtomReply(buf);
		    break;
	    case 17:
		    GetAtomNameReply(buf);
		    break;
	    case 20:
		    GetPropertyReply(buf);
		    break;
	    case 21:
		    ListPropertiesReply(buf);
		    break;
	    case 23:
		    GetSelectionOwnerReply(buf);
		    break;
	    case 26:
		    GrabPointerReply(buf);
		    break;
	    case 31:
		    GrabKeyboardReply(buf);
		    break;
	    case 38:
		    QueryPointerReply(buf);
		    break;
	    case 39:
		    GetMotionEventsReply(buf);
		    break;
	    case 40:
		    TranslateCoordinatesReply(buf);
		    break;
	    case 43:
		    GetInputFocusReply(buf);
		    break;
	    case 44:
		    QueryKeymapReply(buf);
		    break;
	    case 47:
		    QueryFontReply(buf);
		    break;
	    case 48:
		    QueryTextExtentsReply(buf);
		    break;
	    case 49:
		    ListFontsReply(buf);
		    break;
	    case 50:
		    ListFontsWithInfoReply(buf);
		    break;
	    case 52:
		    GetFontPathReply(buf);
		    break;
	    case 73:
		    GetImageReply(buf);
		    break;
	    case 83:
		    ListInstalledColormapsReply(buf);
		    break;
	    case 84:
		    AllocColorReply(buf);
		    break;
	    case 85:
		    AllocNamedColorReply(buf);
		    break;
	    case 86:
		    AllocColorCellsReply(buf);
		    break;
	    case 87:
		    AllocColorPlanesReply(buf);
		    break;
	    case 91:
		    QueryColorsReply(buf);
		    break;
	    case 92:
		    LookupColorReply(buf);
		    break;
	    case 97:
		    QueryBestSizeReply(buf);
		    break;
	    case 98:
		    QueryExtensionReply(buf);
		    break;
	    case 99:
		    ListExtensionsReply(buf);
		    break;
	    case 101:
		    GetKeyboardMappingReply(buf);
		    break;
	    case 103:
		    GetKeyboardControlReply(buf);
		    break;
	    case 106:
		    GetPointerControlReply(buf);
		    break;
	    case 108:
		    GetScreenSaverReply(buf);
		    break;
	    case 110:
		    ListHostsReply(buf);
		    break;
	    case 116:
		    SetPointerMappingReply(buf);
		    break;
	    case 117:
		    GetPointerMappingReply(buf);
		    break;
	    case 118:
		    SetModifierMappingReply(buf);
		    break;
	    case 119:
		    GetModifierMappingReply(buf);
		    break;
	    default:
		    warn("Unimplemented reply opcode");
		    break;
    }
}

/* ************************************************************ */
/*								*/
/*								*/
/* ************************************************************ */

void
DecodeError (
    FD fd,
    const unsigned char *buf,
    long    n)
{
    short   Error = IByte (&buf[1]);
    short   Request = 0;
    short   RequestMinor = 0;

    Request = CheckReplyTable (fd, (short)IShort(&buf[2]), &RequestMinor);

    if (Verbose < 0)
    {
	SimpleDump (DUMP_ERROR, fd, Request, RequestMinor, n);
	return;
    }

    SetIndentLevel(PRINTSERVER);
    if (Raw || (Verbose > 3))
	DumpItem("Error", fd, buf, n);

    if (Error < 1 || Error > 17)
    {
	ExtensionError(fd, buf, Error);
    }
    else switch (Error)
    {
    case 1:
	RequestError(buf);
	break;
    case 2:
	ValueError(buf);
	break;
    case 3:
	WindowError(buf);
	break;
    case 4:
	PixmapError(buf);
	break;
    case 5:
	AtomError(buf);
	break;
    case 6:
	CursorError(buf);
	break;
    case 7:
	FontError(buf);
	break;
    case 8:
	MatchError(buf);
	break;
    case 9:
	DrawableError(buf);
	break;
    case 10:
	AccessError(buf);
	break;
    case 11:
	AllocError(buf);
	break;
    case 12:
	ColormapError(buf);
	break;
    case 13:
	GContextError(buf);
	break;
    case 14:
	IDChoiceError(buf);
	break;
    case 15:
	NameError(buf);
	break;
    case 16:
	LengthError(buf);
	break;
    case 17:
	ImplementationError(buf);
	break;
    default:
	warn("Unimplemented error code");
	UnknownError(buf);
	break;
    }
}

/* ************************************************************ */
/*								*/
/*								*/
/* ************************************************************ */

void
DecodeEvent (
    FD fd,
    const unsigned char *buf,
    long    n)
{
    short   Event = IByte (&buf[0]);
    short   EventMinor = Event == LBXEvent ? IByte (&buf[1]) : 0;

    if (Verbose < 0) 
    {
	SimpleDump (DUMP_EVENT, fd, Event, EventMinor, n);
	return;
    }

    SetIndentLevel(PRINTSERVER);
    if (Raw || (Verbose > 3))
	DumpItem("Event", fd, buf, n);
    /* high-order bit means SendEvent generated */
    if (Event & 0x80)
    {
	debug(8,(stderr, "SendEvent generated event 0x%x\n", Event));
	Event = Event & 0x7F;
    }

    if (Event < 2 || Event > 34)
    {
	ExtensionEvent(fd, buf, Event);
    }
    else switch (Event)
    {
    case 2:
	KeyPressEvent(buf);
	break;
    case 3:
	KeyReleaseEvent(buf);
	break;
    case 4:
	ButtonPressEvent(buf);
	break;
    case 5:
	ButtonReleaseEvent(buf);
	break;
    case 6:
	MotionNotifyEvent(buf);
	break;
    case 7:
	EnterNotifyEvent(buf);
	break;
    case 8:
	LeaveNotifyEvent(buf);
	break;
    case 9:
	FocusInEvent(buf);
	break;
    case 10:
	FocusOutEvent(buf);
	break;
    case 11:
	KeymapNotifyEvent(buf);
	break;
    case 12:
	ExposeEvent(buf);
	break;
    case 13:
	GraphicsExposureEvent(buf);
	break;
    case 14:
	NoExposureEvent(buf);
	break;
    case 15:
	VisibilityNotifyEvent(buf);
	break;
    case 16:
	CreateNotifyEvent(buf);
	break;
    case 17:
	DestroyNotifyEvent(buf);
	break;
    case 18:
	UnmapNotifyEvent(buf);
	break;
    case 19:
	MapNotifyEvent(buf);
	break;
    case 20:
	MapRequestEvent(buf);
	break;
    case 21:
	ReparentNotifyEvent(buf);
	break;
    case 22:
	ConfigureNotifyEvent(buf);
	break;
    case 23:
	ConfigureRequestEvent(buf);
	break;
    case 24:
	GravityNotifyEvent(buf);
	break;
    case 25:
	ResizeRequestEvent(buf);
	break;
    case 26:
	CirculateNotifyEvent(buf);
	break;
    case 27:
	CirculateRequestEvent(buf);
	break;
    case 28:
	PropertyNotifyEvent(buf);
	break;
    case 29:
	SelectionClearEvent(buf);
	break;
    case 30:
	SelectionRequestEvent(buf);
	break;
    case 31:
	SelectionNotifyEvent(buf);
	break;
    case 32:
	ColormapNotifyEvent(buf);
	break;
    case 33:
	ClientMessageEvent(buf);
	break;
    case 34:
	MappingNotifyEvent(buf);
	break;
    default:
	warn("Unimplemented event code");
	UnknownEvent(buf);
	break;
    }
}
