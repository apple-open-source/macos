/* ************************************************** *
 *						      *
 *  Code to decode and print X11 protocol	      *
 *						      *
 *	James Peterson, 1988			      *
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
 *						      *
 *						      *
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
 * ************************************************** */

#include "scope.h"
#include "x11.h"

struct TypeDef  TD[MaxTypes];
unsigned char    RBf[2];
unsigned char    SBf[4];
struct ConnState    CS[StaticMaxFD];

/* ************************************************************ */
/*								*/
/*								*/
/* ************************************************************ */

void
ReportFromClient(
    FD fd,
    const unsigned char *buf,
    long    n)
{
    if (Verbose > 0)
    {
	if (ScopeEnabled) {
	    PrintTime();
	    fprintf(stdout, "Client%s --> %4ld %s\n",
		    ClientName(fd), n, (n == 1 ? "byte" : "bytes"));
	}
    }
    ProcessBuffer(fd, buf, n);
}

void
ReportFromServer (
    FD fd,
    const unsigned char *buf,
    long    n)
{
    if (Verbose > 0) {
	if (ScopeEnabled) {
	    PrintTime();
	    fprintf(stdout, "\t\t\t\t\t%4ld %s <-- X11 Server%s\n",
		    n, (n == 1 ? "byte" : "bytes"), ClientName(fd));
	}
    }
    ProcessBuffer(fd, buf, n);
}


/* ************************************************************ */
/*								*/
/*								*/
/* ************************************************************ */

static long ZeroTime1 = -1;
static long ZeroTime2 = -1;
static struct timeval   tp;

/* print the time since we started in hundredths (1/100) of seconds */

void
PrintTime(void)
{
  static long lastsec = 0;
  long    sec /* seconds */ ;
  long    hsec /* hundredths of a second */ ;

  gettimeofday(&tp, (struct timezone *)NULL);
  if (ZeroTime1 == -1 || (tp.tv_sec - lastsec) >= 1000)
    {
      ZeroTime1 = tp.tv_sec;
      ZeroTime2 = tp.tv_usec / 10000;
    }

  lastsec = tp.tv_sec;
  sec = tp.tv_sec - ZeroTime1;
  hsec = tp.tv_usec / 10000 - ZeroTime2;
  if (hsec < 0)
    {
      hsec += 100;
      sec -= 1;
    }
  fprintf(stdout, "%2ld.%02ld: ", sec, hsec);
}

/* ************************************************************ */
/*								*/
/*								*/
/* ************************************************************ */

/* we will need to be able to interpret the values stored in the
   requests as various built-in types.  The following routines
   support the types built into X11 */

long
pad (
    long n)
{
  /* round up to next multiple of 4 */
  return((n + 3) & ~0x3);
}

unsigned long
ILong (
    const unsigned char   buf[])
{
  /* check for byte-swapping */
  if (littleEndian)
    return((((((buf[3] << 8) | buf[2]) << 8) | buf[1]) << 8) | buf[0]);
  return((((((buf[0] << 8) | buf[1]) << 8) | buf[2]) << 8) | buf[3]);
}

unsigned short
IShort (
    const unsigned char   buf[])
{
  /* check for byte-swapping */
  if (littleEndian)
    return (buf[1] << 8) | buf[0];
  return((buf[0] << 8) | buf[1]);
}

unsigned short
IChar2B (
    const unsigned char   buf[])
{
  /* CHAR2B is like an IShort, but not byte-swapped */
  return((buf[0] << 8) | buf[1]);
}

unsigned short
IByte (
    const unsigned char   buf[])
{
  return(buf[0]);
}

Boolean
IBool (
    const unsigned char   buf[])
{
  if (buf[0] != 0)
    return(true);
  else
    return(false);
}


/* ************************************************************ */
/*								*/
/*								*/
/* ************************************************************ */

/* we will need to save bytes until we get a complete request to
   interpret.  The following procedures provide this ability */

static void
SaveBytes (
    FD fd,
    const unsigned char *buf,
    long    n)
{
  /* check if there is enough space to hold the bytes we want */
  if (CS[fd].NumberofSavedBytes + n > CS[fd].SizeofSavedBytes)
    {
      /* not enough room so far; malloc more space and copy */
      long    SizeofNewBytes = (CS[fd].NumberofSavedBytes + n + 1);
      unsigned char   *NewBytes = (unsigned char *)Malloc (SizeofNewBytes);
      bcopy(/* from  */(char *)CS[fd].SavedBytes,
	    /* to    */(char *)NewBytes,
	    /* count */(int)CS[fd].SizeofSavedBytes);
      Free((char *)CS[fd].SavedBytes);
      CS[fd].SavedBytes = NewBytes;
      CS[fd].SizeofSavedBytes = SizeofNewBytes;
    }

  /* now copy the new bytes onto the end of the old bytes */
  bcopy(/* from  */(char *)buf,
	/* to    */(char *)(CS[fd].SavedBytes + CS[fd].NumberofSavedBytes),
	/* count */(int)n);
  CS[fd].NumberofSavedBytes += n;
}

static void
RemoveSavedBytes (
    FD fd,
    long    n)
{
  /* check if all bytes are being removed -- easiest case */
  if (CS[fd].NumberofSavedBytes <= n)
    CS[fd].NumberofSavedBytes = 0;
  else if (n == 0)
    return;
  else
    {
      /* not all bytes are being removed -- shift the remaining ones down  */
      register unsigned char  *p = CS[fd].SavedBytes;
      register unsigned char  *q = CS[fd].SavedBytes + n;
      register long   i = CS[fd].NumberofSavedBytes - n;
      while (i-- > 0)
	*p++ = *q++;
      CS[fd].NumberofSavedBytes -= n;
    }
}


/* ************************************************************ */
/*								*/
/*								*/
/* ************************************************************ */


/* following are the possible values for ByteProcessing */
/* forward declarations */
static long FinishSetUpMessage(FD fd, const unsigned char *buf, long n);
static long StartRequest(FD fd, const unsigned char *buf, long n);
static long FinishRequest(FD fd, const unsigned char *buf, long n);

static long FinishSetUpReply(FD fd, const unsigned char *buf, long n);
static long ServerPacket(FD fd, const unsigned char *buf, long n);
static long FinishReply(FD fd, const unsigned char *buf, long n);


/* ************************************************************ */
/*								*/
/*								*/
/* ************************************************************ */

int littleEndian;

void
ProcessBuffer (
    FD fd,
    const unsigned char *buf,
    long    n)
{
  const unsigned char   *BytesToProcess;
  long    NumberofUsedBytes;

  /* as long as we have enough bytes to do anything -- do it */

    if (Verbose > 4)
    {
	fprintf (stdout, "\nRead from fd %d\n", fd);
	DumpHexBuffer (buf, n);
    }
  while (CS[fd].NumberofSavedBytes + n >= CS[fd].NumberofBytesNeeded)
    {
      /*
	we have enough bytes to do something.  We want the bytes to be
	grouped together into one contiguous block of bytes. We have three
	cases:

	(1) NumberofSavedBytes = 0; so all needed bytes are in the
	read buffer, buf.

	(2) NumberofSavedBytes >= NumberofBytesNeeded;	in this case we
	will not need to copy any extra bytes into the save buffer.

	(3) 0 < NumberofSavedBytes < NumberofBytesNeeded; so
	some bytes are in the save buffer and others are in the read
	buffer.  In this case we need to copy some of the bytes from the
	read buffer to the save buffer to get as many bytes as we need,
	then use these bytes.
      */

      if (CS[fd].NumberofSavedBytes == 0)
	{
	  /* no saved bytes, so just process the first bytes in the
	     read buffer */
	  BytesToProcess = buf /* address of request bytes */;
	}
      else
	{
	  if (CS[fd].NumberofSavedBytes < CS[fd].NumberofBytesNeeded)
	    {
	      /* first determine the number of bytes we need to
		 transfer; then transfer them and remove them from
		 the read buffer. (there may be additional requests
		 in the read buffer) */
	      long    m;
	      m = CS[fd].NumberofBytesNeeded - CS[fd].NumberofSavedBytes;
	      SaveBytes(fd, buf, m);
	      buf += m;
	      n -= m;
	    }
	  BytesToProcess = CS[fd].SavedBytes /* address of request bytes */;
	}

      /*
	BytesToProcess points to a contiguous block of NumberofBytesNeeded
	bytes that we should process.  The type of processing depends upon
	the state we are in. The processing routine should return the
	number of bytes that it actually used.
      */
      littleEndian = CS[fd].littleEndian;
      NumberofUsedBytes = (*CS[fd].ByteProcessing)
                             (fd, BytesToProcess, CS[fd].NumberofBytesNeeded);

      /* the number of bytes that were actually used is normally (but not
	 always) the number of bytes needed.  Discard the bytes that were
	 actually used, not the bytes that were needed. The number of used
	 bytes must be less than or equal to the number of needed bytes. */

      if (NumberofUsedBytes > 0)
	{
	  CS[fd].NumberofBytesProcessed += NumberofUsedBytes;
	  if (CS[fd].NumberofSavedBytes > 0)
	    RemoveSavedBytes(fd, NumberofUsedBytes);
	  else
	    {
	      /* there are no saved bytes, so the bytes that were
		 used must have been in the read buffer */
	      buf += NumberofUsedBytes;
	      n -= NumberofUsedBytes;
	    }
	}
    } /* end of while (NumberofSavedBytes + n >= NumberofBytesNeeded) */

    if (Verbose > 3)
	fprintf (stdout, "Have %ld need %ld\n",
		 CS[fd].NumberofSavedBytes + n,
		 CS[fd].NumberofBytesNeeded);
  /* not enough bytes -- just save the new bytes for more later */
  if (n > 0)
  {
    SaveBytes(fd, buf, n);
  }
  return;
}



/* ************************************************************ */
/*								*/
/*								*/
/* ************************************************************ */
/*
  Byte Processing Routines.  Each routine MUST set NumberofBytesNeeded
  and ByteProcessing.  It probably needs to do some computation first.
*/

void
SetBufLimit (
    FD  fd)
{
  int ServerFD = FDPair (fd);
  FDinfo[ServerFD].buflimit = (CS[fd].NumberofBytesProcessed + 
			       CS[fd].NumberofBytesNeeded);
}

void
ClearBufLimit (
    FD  fd)
{
  int ServerFD = FDPair (fd);
  FDinfo[ServerFD].buflimit = -1;
}

static void
StartStuff (
    FD  fd)
{
  if (BreakPoint)
  {
    int ServerFD = FDPair (fd);
    FDinfo[ServerFD].buflimit = (CS[fd].NumberofBytesProcessed + 
				 CS[fd].NumberofBytesNeeded);
    FlushFD (ServerFD);
  }
}

static void
FinishStuff (
    FD fd,
    const unsigned char	*buf,
    long		n)
{
  if (BreakPoint)
  {
    int	ServerFD = FDPair (fd);
    
    FlushFD (ServerFD);
    if (SingleStep)
      ReadCommands ();
    else if (BreakPoint)
      TestBreakPoints (buf, n);
    if (!BreakPoint)
    {
      FDinfo[ServerFD].buflimit = -1;
      FlushFD (ServerFD);
    }
  }
}

void
StartClientConnection (
    FD fd)
{
  enterprocedure("StartClientConnection");
  /* when a new connection is started, we have no saved bytes */
  CS[fd].SavedBytes = NULL;
  CS[fd].SizeofSavedBytes = 0;
  CS[fd].NumberofSavedBytes = 0;
  CS[fd].NumberofBytesProcessed = 0;

  /* when a new connection is started, we have no reply Queue */
  FlushReplyQ(fd);

  /* each new connection gets a request sequence number */
  CS[fd].SequenceNumber = 0;

  /* we need 12 bytes to start a SetUp message */
  CS[fd].ByteProcessing = StartSetUpMessage;
  CS[fd].NumberofBytesNeeded = 12;
  StartStuff (fd);
}

void
StopClientConnection (
    FD fd)
{
  enterprocedure("StopClientConnection");
  /* when a new connection is stopped, discard the old buffer */

  if (CS[fd].SizeofSavedBytes > 0)
    Free((char*)CS[fd].SavedBytes);
}

long
StartSetUpMessage (
    FD fd,
    const unsigned char *buf,
    long    n)
{
  unsigned short   namelength;
  unsigned short   datalength;

  enterprocedure("StartSetUpMessage");
  /*
    we need the first 12 bytes to be able to determine if, and how many,
    additional bytes we need for name and data authorization.  However, we
    can't process the first 12 bytes until we get all of them, so
    return zero bytes used, and increase the number of bytes needed
  */
  CS[fd].littleEndian = (buf[0] == 'l');
  CS[ServerHalf(fd)].littleEndian = CS[fd].littleEndian;
  littleEndian = CS[fd].littleEndian;
  
  namelength = IShort(&buf[6]);
  datalength = IShort(&buf[8]);
  CS[fd].ByteProcessing = FinishSetUpMessage;
  CS[fd].NumberofBytesNeeded = n
                               + pad((long)namelength) + pad((long)datalength);
  debug(8,(stderr, "need %ld bytes to finish startup\n",
	   CS[fd].NumberofBytesNeeded - n));
  StartStuff (fd);
  return(0);
}

static long
FinishSetUpMessage (
    FD fd,
    const unsigned char *buf,
    long    n)
{
  enterprocedure("FinishSetUpMessage");
  if( Raw || (Verbose > 3) )
	DumpItem("Client Connect", fd, buf, n) ;
  CS[fd].littleEndian = (buf[0] == 'l');
  CS[ServerHalf(fd)].littleEndian = CS[fd].littleEndian;
  littleEndian = CS[fd].littleEndian;
  if (ScopeEnabled)
    PrintSetUpMessage(buf);

  /* after a set-up message, we expect a string of requests */
  CS[fd].ByteProcessing = StartRequest;
  CS[fd].NumberofBytesNeeded = 4;
  FinishStuff (fd, buf, n);
  return(n);
}

static long
StartBigRequest (
    FD fd,
    const unsigned char *buf,
    long    n)
{
  enterprocedure("StartBigRequest");

  /* bytes 0-3 are ignored now; bytes 4-8 tell us the request length */
  CS[fd].requestLen = ILong(&buf[4]);

  CS[fd].ByteProcessing = FinishRequest;
  CS[fd].NumberofBytesNeeded = 4 * CS[fd].requestLen;
  debug(8,(stderr, "need %ld more bytes to finish request\n",
	   CS[fd].NumberofBytesNeeded - n));
  StartStuff (fd);
  return(0);
}

static long
StartRequest (
    FD fd,
    const unsigned char *buf,
    long    n)
{
  enterprocedure("StartRequest");

  /* bytes 0,1 are ignored now; bytes 2,3 tell us the request length */
  CS[fd].requestLen = IShort(&buf[2]);
  if (CS[fd].requestLen == 0 && CS[fd].bigreqEnabled)
  {
    CS[fd].ByteProcessing = StartBigRequest;
    CS[fd].NumberofBytesNeeded = 8;
  }
  else
  {
    if (CS[fd].requestLen == 0)
      CS[fd].requestLen = 1;
    CS[fd].ByteProcessing = FinishRequest;
    CS[fd].NumberofBytesNeeded = 4 * CS[fd].requestLen;
    debug(8,(stderr, "need %ld more bytes to finish request\n",
	     CS[fd].NumberofBytesNeeded - n));
  }
  StartStuff (fd);
  return(0);
}


static long
FinishRequest (
    FD fd,
    const unsigned char *buf,
    long    n)
{
  enterprocedure("FinishRequest");
  CS[fd].ByteProcessing = StartRequest;
  CS[fd].NumberofBytesNeeded = 4;
  if (ScopeEnabled)
      DecodeRequest(fd, buf, n);
  FinishStuff (fd, buf, n);
  return(n);
}

/* ************************************************************ */
/*								*/
/*								*/
/* ************************************************************ */

void
StartServerConnection (
    FD fd)
{
  enterprocedure("StartServerConnection");
  /* when a new connection is started, we have no saved bytes */
  CS[fd].SavedBytes = NULL;
  CS[fd].SizeofSavedBytes = 0;
  CS[fd].NumberofSavedBytes = 0;
  CS[fd].NumberofBytesProcessed = 0;

  /* when a new connection is started, we have no reply Queue */
  FlushReplyQ(fd);

  /* we need 8 bytes to start a SetUp reply */
  CS[fd].ByteProcessing = StartSetUpReply;
  CS[fd].NumberofBytesNeeded = 8;
}

void
StopServerConnection (
    FD fd)
{
  enterprocedure("StopServerConnection");
  /* when a new connection is stopped, discard the old buffer */

  if (CS[fd].SizeofSavedBytes > 0)
    Free((char *)CS[fd].SavedBytes);
}

long
StartSetUpReply (
    FD fd,
    const unsigned char *buf,
    long    n)
{
  unsigned short   replylength;

  enterprocedure("StartSetUpReply");
  replylength = IShort(&buf[6]);
  CS[fd].ByteProcessing = FinishSetUpReply;
  CS[fd].NumberofBytesNeeded = n + 4 * replylength;
  debug(8,(stderr, "need %ld bytes to finish startup reply\n",
	   CS[fd].NumberofBytesNeeded - n));
  return(0);
}

static long
FinishSetUpReply (
    FD fd,
    const unsigned char *buf,
    long    n)
{
  enterprocedure("FinishSetUpReply");
  if( Raw || (Verbose > 3) )
	DumpItem("Server Connect", fd, buf, n) ;
  if (ScopeEnabled)
      PrintSetUpReply(buf);
  CS[fd].ByteProcessing = ServerPacket;
  CS[fd].NumberofBytesNeeded = 32;
  return(n);
}

/* ************************************************************ */

static long
ErrorPacket (
    FD fd,
    const unsigned char *buf,
    long    n)
{
  CS[fd].ByteProcessing = ServerPacket;
  CS[fd].NumberofBytesNeeded = 32;
  DecodeError(fd, buf, n);
  return(n);
}

/* FinishEvent/EventPacket: since GenericEvents may be longer than 32 bytes
   now, mirror the FinishReply/ReplyPacket model for getting the required
   data length to handle them. */

static long
FinishEvent (
    FD fd,
    const unsigned char *buf,
    long    n)
{
  CS[fd].ByteProcessing = ServerPacket;
  CS[fd].NumberofBytesNeeded = 32;
  enterprocedure("FinishEvent");
  if (ScopeEnabled)
    DecodeEvent(fd, buf, n);
  return(n);
}

static long
EventPacket (
    FD fd,
    const unsigned char *buf,
    long    n)
{
  short Event = IByte (&buf[0]);
  long	eventlength = 0;

  CS[fd].ByteProcessing = FinishEvent;
  CS[fd].NumberofBytesNeeded = 32;
  if (Event == Event_Type_Generic) {
    eventlength = ILong(&buf[4]);
    CS[fd].NumberofBytesNeeded += (4 * eventlength);
  }
  debug(8,(stderr, "need %ld bytes to finish reply\n", (4 * eventlength)));
  return(0);
}


static long
ReplyPacket (
    FD fd,
    const unsigned char *buf,
    long    n)
{
  long   replylength;

  replylength = ILong(&buf[4]);

  /*
    Replies may need more bytes, so we compute how many more
    bytes are needed and ask for them, not using any of the bytes
    we were given (return(0) to say that no bytes were used).
    If the replylength is zero (we don't need any more bytes), the
    number of bytes needed will be the same as what we have, and
    so the top-level loop will call the next routine immediately
    with the same buffer of bytes that we were given.
  */

  CS[fd].ByteProcessing = FinishReply;
  CS[fd].NumberofBytesNeeded = n + 4 * replylength;
  debug(8,(stderr, "need %ld bytes to finish reply\n", (4 * replylength)));
  return(0);
}

static long
ServerPacket (
    FD fd,
    const unsigned char *buf,
    long    n)
{
  short   PacketType;
  enterprocedure("ServerPacket");

  PacketType = IByte(&buf[0]);
  if (PacketType == 0)
    return(ErrorPacket(fd, buf, n));
  if (PacketType == 1)
    return(ReplyPacket(fd, buf, n));
  return(EventPacket(fd, buf, n));
}

long
FinishReply (
    FD fd,
    const unsigned char *buf,
    long    n)
{
  CS[fd].ByteProcessing = ServerPacket;
  CS[fd].NumberofBytesNeeded = 32;
  enterprocedure("FinishReply");
  if (ScopeEnabled)
    DecodeReply(fd, buf, n);
  return(n);
}

long
GetXRequestFromName (
    const char *name)
{
    long req = GetEValue (REQUEST, name);

    if (req < 0)
	req =  GetEValue (EXTENSION, name);

    return req;
}
