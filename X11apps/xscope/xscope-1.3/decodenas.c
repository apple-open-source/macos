/* ************************************************************ *
 *						     		*
 *  Decoding and switching routines for the NAS protocol	*
 *						      		*
 *	James Peterson, 1988			      		*
 *	(c) Copyright MCC, 1988 		      		*
 *						      		*
 * ************************************************************ */

#include "scope.h"
#include "nas.h" 

/*
  There are 4 types of things in NAS: requests, replies, errors, and events.

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
AudioSimpleDump (int type, FD fd, short Major, short Minor, long bytes)
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
DecodeAudioRequest(
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

  if (NasVerbose == 0)
  {
    AudioSimpleDump (DUMP_REQUEST, fd, Request, RequestMinor, n);
    return;
  }
  
  if (NasVerbose > 3)
    DumpItem("Request", fd, buf, n);
  switch (Request)
    {
    case 1:
	AudioListDevices (buf);
	ReplyExpected(fd, Request);
	break;
    case 2:
	AudioGetDeviceAttributes(buf);
	ReplyExpected(fd, Request);
	break;
    case 3:
	AudioSetDeviceAttributes (buf);
	break;
    case 4:
	AudioCreateBucket (buf);
	break;
    case 5:
	AudioDestroyBucket (buf);
	break;
    case 6:
	AudioListBuckets (buf);
	ReplyExpected (fd, Request);
	break;
    case 7:
	AudioGetBucketAttributes (buf);
	ReplyExpected (fd, Request);
	break;
    case 8:
	AudioSetBucketAttributes (buf);
	break;
    case 9:
	AudioCreateRadio (buf);
	break;
    case 10:
	AudioDestroyRadio (buf);
	break;
    case 11:
	AudioListRadios (buf);
	ReplyExpected (fd, Request);
	break;
    case 12:
	AudioGetRadioAttributes (buf);
	ReplyExpected (fd, Request);
	break;
    case 13:
	AudioSetRadioAttributes (buf);
	break;
    case 14:
	AudioCreateFlow (buf);
	break;
    case 15:
	AudioDestroyFlow (buf);
	break;
    case 16:
	AudioGetFlowAttributes (buf);
	ReplyExpected (fd, Request);
	break;
    case 17:
	AudioSetFlowAttributes (buf);
	break;
    case 18:
	AudioGetElements (buf);
	ReplyExpected (fd, Request);
	break;
    case 19:
	AudioSetElements (buf);
	break;
    case 20:
	AudioGetElementStates (buf);
	ReplyExpected (fd, Request);
	break;
    case 21:
	AudioSetElementStates (buf);
	break;
    case 22:
	AudioGetElementParameters (buf);
	ReplyExpected (fd, Request);
	break;
    case 23:
	AudioSetElementParameters (buf);
	break;
    case 24:
	AudioWriteElement (buf);
	break;
    case 25:
	AudioReadElement (buf);
	ReplyExpected (fd, Request);
	break;
    case 26:
	AudioGrabComponent (buf);
	break;
    case 27:
	AudioUngrabComponent (buf);
	break;
    case 28:
	AudioSendEvent (buf);
	break;
    case 29:
	AudioGetAllowedUsers (buf);
	ReplyExpected (fd, Request);
	break;
    case 30:
	AudioSetAllowedUsers (buf);
	break;
    case 31:
	AudioListExtensions (buf);
	ReplyExpected (fd, Request);
	break;
    case 32:
	AudioQueryExtension (buf);
	ReplyExpected (fd, Request);
	break;
    case 33:
	AudioGetCloseDownMode (buf);
	ReplyExpected (fd, Request);
	break;
    case 34:
	AudioSetCloseDownMode (buf);
	break;
    case 35:
	AudioKillClient (buf);
	break;
    case 36:
	AudioGetServerTime (buf);
	ReplyExpected (fd, Request);
	break;
    case 37:
	AudioNoOperation (buf);
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
DecodeAudioReply(
    FD fd,
    const unsigned char *buf,
    long    n)
{
    short   SequenceNumber = IShort (&buf[2]);
    short	  RequestMinor;
    short   Request = CheckReplyTable (fd, SequenceNumber, &RequestMinor);

    if (NasVerbose == 0)
    {
	AudioSimpleDump (DUMP_REPLY, fd, Request, RequestMinor, n);
	return;
    }

    SetIndentLevel(PRINTSERVER);
    RBf[0] = Request /* for the PrintField in the Reply procedure */ ;
    RBf[1] = RequestMinor;
    if (NasVerbose > 3)
	DumpItem("Reply", fd, buf, n);
    switch (Request)
    {
    case 0:
	UnknownAudioReply(buf);
	break;
    case 1:
	AudioListDevicesReply (buf);
	break;
    case 2:
	AudioGetDeviceAttributesReply (buf);
	break;
    case 6:
	AudioListBucketsReply (buf);
	break;
    case 7:
	AudioGetBucketAttributesReply (buf);
	break;
    case 11:
	AudioListRadiosReply (buf);
	break;
    case 12:
	AudioGetRadioAttributesReply (buf);
	break;
    case 16:
	AudioGetFlowAttributesReply (buf);
	break;
    case 18:
	AudioGetElementsReply (buf);
	break;
    case 20:
	AudioGetElementStatesReply (buf);
	break;
    case 22:
	AudioGetElementParametersReply (buf);
	break;
    case 25:
	AudioReadElementReply (buf);
	break;
    case 29:
	AudioGetAllowedUsersReply (buf);
	break;
    case 31:
	AudioListExtensionsReply (buf);
	break;
    case 32:
	AudioQueryExtensionReply (buf);
	break;
    case 33:
	AudioGetCloseDownModeReply (buf);
	break;
    case 36:
	AudioGetServerTimeReply (buf);
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
DecodeAudioError(
    FD fd,
    const unsigned char *buf,
    long    n)
{
    short   Error = IByte (&buf[1]);
    short   Request = 0;
    short   RequestMinor = 0;

    Request = CheckReplyTable (fd, (short)IShort(&buf[2]), &RequestMinor);

    if (NasVerbose == 0)
    {
	AudioSimpleDump (DUMP_ERROR, fd, Request, RequestMinor, n);
	return;
    }

    SetIndentLevel(PRINTSERVER);
    if (NasVerbose > 3)
	DumpItem("Error", fd, buf, n);
}

/* ************************************************************ */
/*								*/
/*								*/
/* ************************************************************ */

void
DecodeAudioEvent(
    FD fd,
    const unsigned char *buf,
    long    n)
{
    short   Event = IByte (&buf[0]);
    short   EventMinor = 0;

    if (NasVerbose == 0) 
    {
	AudioSimpleDump (DUMP_EVENT, fd, Event, EventMinor, n);
	return;
    }

    SetIndentLevel(PRINTSERVER);
    if (NasVerbose > 3)
	DumpItem("Event", fd, buf, n);
    /* high-order bit means SendEvent generated */
    if (Event & 0x80)
    {
	debug(8,(stderr, "SendEvent generated event 0x%x\n", Event));
	Event = Event & 0x7F;
    }
    switch (Event)
    {
    case 2:
	AudioElementNotify (buf);
	break;
    case 3:
	AudioGrabNotify (buf);
	break;
    case 4:
	AudioMonitorNotify (buf);
	break;
    case 5:
	AudioBucketNotify (buf);
	break;
    case 6:
	AudioDeviceNotify (buf);
	break;
    default:
	warn("Unimplemented event code");
	break;
    }
}

void
InitializeAudioDecode (void)
{
    TYPE    p;
    
    p = DefineType (NASREQUEST, ENUMERATED, "NASREQUEST", (PrintProcType) PrintENUMERATED);
    DefineEValue (p, 1L, "ListDevices");
    DefineEValue (p, 2L, "GetDeviceAttributes");
    DefineEValue (p, 3L, "SetDeviceAttributes");
    DefineEValue (p, 4L, "CreateBucket");
    DefineEValue (p, 5L, "DestroyBucket");
    DefineEValue (p, 6L, "ListBuckets");
    DefineEValue (p, 7L, "GetBucketAttributes");
    DefineEValue (p, 8L, "SetBucketAttributes");
    DefineEValue (p, 9L, "CreateRadio");
    DefineEValue (p, 10L, "DestroyRadio");
    DefineEValue (p, 11L, "ListRadios");
    DefineEValue (p, 12L, "GetRadioAttributes");
    DefineEValue (p, 13L, "SetRadioAttributes");
    DefineEValue (p, 14L, "CreateFlow");
    DefineEValue (p, 15L, "DestroyFlow");
    DefineEValue (p, 16L, "GetFlowAttributes");
    DefineEValue (p, 17L, "SetFlowAttributes");
    DefineEValue (p, 18L, "GetElements");
    DefineEValue (p, 19L, "SetElements");
    DefineEValue (p, 20L, "GetElementStates");
    DefineEValue (p, 21L, "SetElementStates");
    DefineEValue (p, 22L, "GetElementParameters");
    DefineEValue (p, 23L, "SetElementParameters");
    DefineEValue (p, 24L, "WriteElement");
    DefineEValue (p, 25L, "ReadElement");
    DefineEValue (p, 26L, "GrabComponent");
    DefineEValue (p, 27L, "UngrabComponent");
    DefineEValue (p, 28L, "SendEvent");
    DefineEValue (p, 29L, "GetAllowedUsers");
    DefineEValue (p, 30L, "SetAllowedUsers");
    DefineEValue (p, 31L, "ListExtensions");
    DefineEValue (p, 32L, "QuerExtension");
    DefineEValue (p, 33L, "GetCloseDownMode");
    DefineEValue (p, 34L, "SetCloseDownMode");
    DefineEValue (p, 35L, "KillClient");
    DefineEValue (p, 36L, "GetServerTime");
    DefineEValue (p, 37L, "NoOperation");
    p = DefineType (NASREPLY, ENUMERATED, "NASREPLY", (PrintProcType) PrintENUMERATED);
    DefineEValue (p, 1L, "ListDevices");
    DefineEValue (p, 2L, "GetDeviceAttributes");
    DefineEValue (p, 6L, "ListBuckets");
    DefineEValue (p, 7L, "GetBucketAttributes");
    DefineEValue (p, 11L, "ListRadios");
    DefineEValue (p, 12L, "GetRadioAttributes");
    DefineEValue (p, 16L, "GetFlowAttributes");
    DefineEValue (p, 18L, "GetElements");
    DefineEValue (p, 20L, "GetElementStates");
    DefineEValue (p, 22L, "GetElementParameters");
    DefineEValue (p, 25L, "ReadElement");
    DefineEValue (p, 29L, "GetAllowedUsers");
    DefineEValue (p, 31L, "ListExtensions");
    DefineEValue (p, 32L, "QueryExtension");
    DefineEValue (p, 33L, "GetCloseDownMode");
    DefineEValue (p, 36L, "GetServerTime");
    p = DefineType (NASEVENT, ENUMERATED, "NASEVENT", (PrintProcType) PrintENUMERATED);
    DefineEValue(p, 2L, "ElementNotify");
    DefineEValue(p, 3L, "GrabNotify");
    DefineEValue(p, 4L, "MonitorNotify");
    DefineEValue(p, 5L, "BucketNotify");
    DefineEValue(p, 6L, "DeviceNotify");
    
}
