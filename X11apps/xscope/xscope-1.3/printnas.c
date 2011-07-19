/* ************************************************** *
 *						      *
 *  Request, Reply, Event, Error Printing	      *
 *						      *
 *	James Peterson, 1988			      *
 *	(c) Copyright MCC, 1988 		      *
 *						      *
 * ************************************************** */

#include "scope.h"
#include "nas.h"


/* ************************************************************ */
/*								*/
/*								*/
/* ************************************************************ */

/*
  In printing the contents of the fields of the X11 packets, some
  fields are of obvious value, and others are of lesser value.  To
  control the amount of output, we generate our output according
  to the level of Verbose-ness that was selected by the user.

  NasVerbose = 0 ==  Headers only, time and request/reply/... names.

  NasVerbose = 1 ==  Very useful content fields.

  NasVerbose = 2 ==  Almost everything.

  NasVerbose = 3 ==  Every single bit and byte.

*/

/*
  To aid in making the choice between level 1 and level 2, we
  define the following define, which does not print relatively
  unimportant fields.
*/

#define printfield(a,b,c,d,e) if (NasVerbose > 1) PrintField(a,b,c,d,e)

static void PrintFailedAudioSetUpReply (const unsigned char *buf);
static void PrintSuccessfulAudioSetUpReply (const unsigned char *buf);

/* ************************************************************ */
/*								*/
/*								*/
/* ************************************************************ */

void
PrintAudioSetUpMessage(
    const unsigned char *buf)
{
  short   n;
  short   d;

  enterprocedure("PrintSetUpMessage");
  if (NasVerbose < 1)
    return;
  SetIndentLevel(PRINTCLIENT);
  PrintField(buf, 0, 1, BYTEMODE, "byte-order");
  PrintField(buf, 2, 2, CARD16, "major-version");
  PrintField(buf, 4, 2, CARD16, "minor-version");
  printfield(buf, 6, 2, DVALUE2(n), "length of name");
  n = IShort(&buf[6]);
  printfield(buf, 8, 2, DVALUE2(d), "length of data");
  d = IShort(&buf[8]);
  PrintString8(&buf[12], (long)n, "authorization-protocol-name");
  PrintString8(&buf[pad((long)(12+n))], (long)d, "authorization-protocol-data");
}

void
PrintAudioSetUpReply(const unsigned char *buf)
{
  enterprocedure("PrintSetUpReply");
  SetIndentLevel(PRINTSERVER);
  if (IByte(&buf[0]))
    PrintSuccessfulAudioSetUpReply(buf);
  else
    PrintFailedAudioSetUpReply(buf);
}

static void
PrintFailedAudioSetUpReply(const unsigned char *buf)
{
  short   n;

  PrintField(buf, 0, 1, 0, "SetUp Failed");
  if (NasVerbose < 1)
    return;
  printfield(buf, 1, 1, DVALUE1(n), "length of reason in bytes");
  n = IByte(&buf[1]);
  PrintField(buf, 2, 2, CARD16, "major-version");
  PrintField(buf, 4, 2, CARD16, "minor-version");
  printfield(buf, 6, 2, DVALUE2((n + p) / 4), "length of data");
  PrintString8(&buf[8], (long)n, "reason");
}

static void
PrintSuccessfulAudioSetUpReply(
    const unsigned char *buf)
{
  short   v;
  short   n;
  short   m;

  if (NasVerbose < 1)
    return;
  PrintField(buf, 2, 2, CARD16, "protocol-major-version");
  PrintField(buf, 4, 2, CARD16, "protocol-minor-version");
  printfield(buf, 6, 2, DVALUE2(8 + 2*n + (v + p + m) / 4), "length of data");
  PrintField(buf, 8, 4, CARD32, "release-number");
  PrintField(buf, 12, 4, CARD32, "resource-id-base");
  PrintField(buf, 16, 4, CARD32, "resource-id-mask");
  PrintField(buf, 20, 2, CARD16, "min-sample-rate");
  PrintField(buf, 22, 2, CARD16, "max-sample-rate");
  printfield(buf, 24, 2, DVALUE2(v), "length of vendor");
  v = IShort(&buf[24]);
  printfield(buf, 26, 2, CARD16, "maximum-request-length");
  printfield(buf, 28, 1, CARD8, "max-tracks");
  m = IByte(&buf[28]);
  printfield(buf, 29, 1, DVALUE1(n), "number of audio formats");
  n = IByte(&buf[29]);
  PrintField(buf, 30, 1, CARD8, "number of element-types");
  PrintField(buf, 31, 1, CARD8, "number of wave-forms");
  PrintField(buf, 32, 1, CARD8, "number of actions");
  PrintField(buf, 33, 1, CARD8, "number of devices");
  PrintField(buf, 34, 1, CARD8, "number of buckets");
  PrintField(buf, 35, 1, CARD8, "number of radios");
  PrintString8(&buf[36], (long)v, "vendor");
}



/* ************************************************************ */
/*								*/
/*								*/
/* ************************************************************ */

/* Error Printing procedures */

static void
AudioRequestError(
    const unsigned char *buf)
{
  PrintField(buf, 1, 1, ERROR, ERRORHEADER) /* Request */ ;
  if (NasVerbose < 1)
    return;
  printfield(buf, 2, 2, CARD16, "sequence number");
  PrintField(buf, 8, 2, CARD16, "minor opcode");
  PrintField(buf, 10, 1, CARD8, "major opcode");
}

static void
AudioValueError(
    const unsigned char *buf)
{
  PrintField(buf, 1, 1, ERROR, ERRORHEADER) /* Value */ ;
  if (NasVerbose < 1)
    return;
  printfield(buf, 2, 2, CARD16, "sequence number");
  PrintField(buf, 4, 4, INT32, "bad value");
  PrintField(buf, 8, 2, CARD16, "minor opcode");
  PrintField(buf, 10, 1, CARD8, "major opcode");
}

static void
AudioMatchError(
    const unsigned char *buf)
{
  PrintField(buf, 1, 1, ERROR, ERRORHEADER) /* Match */ ;
  if (NasVerbose < 1)
    return;
  printfield(buf, 2, 2, CARD16, "sequence number");
  PrintField(buf, 8, 2, CARD16, "minor opcode");
  PrintField(buf, 10, 1, CARD8, "major opcode");
}

static void
AudioAccessError(
    const unsigned char *buf)
{
  PrintField(buf, 1, 1, ERROR, ERRORHEADER) /* Access */ ;
  if (NasVerbose < 1)
    return;
  printfield(buf, 2, 2, CARD16, "sequence number");
  PrintField(buf, 8, 2, CARD16, "minor opcode");
  PrintField(buf, 10, 1, CARD8, "major opcode");
}

static void
AudioAllocError(
    const unsigned char *buf)
{
  PrintField(buf, 1, 1, ERROR, ERRORHEADER) /* Alloc */ ;
  if (NasVerbose < 1)
    return;
  printfield(buf, 2, 2, CARD16, "sequence number");
  PrintField(buf, 8, 2, CARD16, "minor opcode");
  PrintField(buf, 10, 1, CARD8, "major opcode");
}

static void
AudioIDChoiceError(
    const unsigned char *buf)
{
  PrintField(buf, 1, 1, ERROR, ERRORHEADER) /* IDChoice */ ;
  if (NasVerbose < 1)
    return;
  printfield(buf, 2, 2, CARD16, "sequence number");
  PrintField(buf, 4, 4, CARD32, "bad resource id");
  PrintField(buf, 8, 2, CARD16, "minor opcode");
  PrintField(buf, 10, 1, CARD8, "major opcode");
}

static void
AudioLengthError(
    const unsigned char *buf)
{
  PrintField(buf, 1, 1, ERROR, ERRORHEADER) /* Length */ ;
  if (NasVerbose < 1)
    return;
  printfield(buf, 2, 2, CARD16, "sequence number");
  PrintField(buf, 8, 2, CARD16, "minor opcode");
  PrintField(buf, 10, 1, CARD8, "major opcode");
}

static void
AudioImplementationError(
    const unsigned char *buf)
{
  PrintField(buf, 1, 1, ERROR, ERRORHEADER) /* Implementation */ ;
  if (NasVerbose < 1)
    return;
  printfield(buf, 2, 2, CARD16, "sequence number");
  PrintField(buf, 8, 2, CARD16, "minor opcode");
  PrintField(buf, 10, 1, CARD8, "major opcode");
}

/* ************************************************************ */
/*								*/
/*								*/
/* ************************************************************ */

/* Event Printing procedures */

void
AudioElementNotify (
    const unsigned char *buf)
{
    PrintField(buf, 0, 1, NASEVENT, EVENTHEADER);
    if (NasVerbose < 1)
	return;
    printfield(buf, 1, 1, CARD8, "detail");
    printfield(buf, 2, 2, CARD16, "sequence number");
    PrintField(buf, 4, 4, TIMESTAMP, "time");
    PrintField(buf, 8, 4, CARD32, "flow");
    printfield(buf, 12, 2, CARD16, "element num");
    printfield(buf, 14, 2, CARD16, "kind");
    printfield(buf, 16, 2, CARD16, "prev state");
    printfield(buf, 18, 2, CARD16, "cur state");
    printfield(buf, 20, 2, CARD16, "reason");
    printfield(buf, 24, 4, CARD32, "num bytes");
}

void
AudioGrabNotify (
    const unsigned char *buf)
{
}

void
AudioMonitorNotify (
    const unsigned char *buf)
{
    PrintField(buf, 0, 1, NASEVENT, EVENTHEADER);
    if (NasVerbose < 1)
	return;
    printfield(buf, 1, 1, CARD8, "detail");
    printfield(buf, 2, 2, CARD16, "sequence number");
    PrintField(buf, 4, 4, TIMESTAMP, "time");
    PrintField(buf, 8, 4, CARD32, "flow");
    printfield(buf, 12, 2, CARD16, "element num");
    printfield(buf, 14, 1, CARD8, "format");
    printfield(buf, 15, 1, CARD8, "num tracks");
    printfield(buf, 16, 2, CARD16, "count");
    printfield(buf, 18, 2, CARD16, "num fields");
    PrintField(buf, 20, 4, CARD32, "data");
    PrintField(buf, 24, 4, CARD32, "date1");
    PrintField(buf, 28, 4, CARD32, "data2");
}

void
AudioBucketNotify (
    const unsigned char *buf)
{
}

void
AudioDeviceNotify (
    const unsigned char *buf)
{
}

/* ************************************************************ */
/*								*/
/*								*/
/* ************************************************************ */

/* Request and Reply Printing procedures */

void
UnknownAudioReply(
    const unsigned char *buf)
{
  long n;
  
  PrintField(RBf, 0, 1, NASREPLY, REPLYHEADER);
  PrintField(buf, 1, 1, CARD8, "data");
  printfield (buf, 2, 2, CARD16, "sequence number");
  printfield (buf, 4, 4, DVALUE4(n), "reply length");
  n = ILong (&buf[4]) + 6;
  (void) PrintList (&buf[8], n, CARD32, "data");
}

void
AudioListDevices (
    const unsigned char *buf)
{
    PrintField (buf, 0, 1, NASREQUEST, REQUESTHEADER) /* ListDevices */;
    if (NasVerbose < 1)
	return;
    if (NasVerbose > 1)
	PrintField(SBf, 0, 4, CARD32, "sequence number");
    printfield(buf, 2, 2, DVALUE2(8 + n), "request length");
}

void
AudioListDevicesReply(
    const unsigned char *buf)
{
    long	n;
    PrintField(RBf, 0, 1, REPLY, REPLYHEADER) /* GetWindowAttributes */ ;
    if (NasVerbose < 1)
	return;
    printfield(buf, 2, 2, CARD16, "sequence number");
    printfield(buf, 4, 4, CARD32, "reply length");
    printfield(buf, 8, 4, CARD32, "num devices");
    n = ILong(&buf[8]);
}

static long
AuString (
    const unsigned char *buf)
{
    long    n;
    
    printfield(buf, 0, 1, CARD8, "type");
    printfield(buf, 4, 4, CARD32, "length");
    n = ILong(&buf[8]);
    PrintString8(&buf[8], n, "string");
    return 8 + pad(n);
}

static void
AuDeviceAttributes (
    const unsigned char *buf)
{
    int	l;
    printfield(buf, 0, 4, CARD32, "value mask");
    printfield(buf, 4, 4, CARD32, "changable mask");
    printfield(buf, 8, 4, CARD32, "id");
    printfield(buf, 12, 1, CARD8, "kind");
    printfield(buf, 13, 1, CARD8, "use");
    printfield(buf, 14, 1, CARD8, "format");
    printfield(buf, 15, 1, CARD8, "num tracks");
    printfield(buf, 16, 4, CARD32, "access");
    l = 20 + AuString(&buf[20]);
}

void
AudioGetDeviceAttributes (
    const unsigned char *buf)
{
    PrintField (buf, 0, 1, NASREQUEST, REQUESTHEADER);
    if (NasVerbose < 1)
	return;
    if (NasVerbose > 1)
	PrintField(SBf, 0, 4, CARD32, "sequence number");
    printfield(buf, 2, 2, DVALUE2(8 + n), "request length");
}

void
AudioGetDeviceAttributesReply (
    const unsigned char *buf)
{
    PrintField (RBf, 0, 1, NASREPLY, REPLYHEADER);
    if (NasVerbose < 1)
	return;
    printfield(buf, 2, 2, CARD16, "sequence number");
    printfield(buf, 4, 4, DVALUE4((n + p) / 4), "reply length");
    AuDeviceAttributes (&buf[32]);
}

void
AudioSetDeviceAttributes (
    const unsigned char *buf)
{
    PrintField (buf, 0, 1, NASREQUEST, REQUESTHEADER);
    if (NasVerbose < 1)
	return;
    if (NasVerbose > 1)
	PrintField(SBf, 0, 4, CARD32, "sequence number");
    printfield(buf, 2, 2, DVALUE2(8 + n), "request length");
    printfield(buf, 4, 4, CARD32, "device");    
    AuDeviceAttributes (&buf[8]);
}

void
AudioCreateBucket (
    const unsigned char *buf)
{
    PrintField (buf, 0, 1, NASREQUEST, REQUESTHEADER);
    if (NasVerbose < 1)
	return;
    if (NasVerbose > 1)
	PrintField(SBf, 0, 4, CARD32, "sequence number");
    printfield(buf, 2, 2, DVALUE2(8 + n), "request length");
}

void
AudioDestroyBucket (
    const unsigned char *buf)
{
    PrintField (buf, 0, 1, NASREQUEST, REQUESTHEADER);
    if (NasVerbose < 1)
	return;
    if (NasVerbose > 1)
	PrintField(SBf, 0, 4, CARD32, "sequence number");
    printfield(buf, 2, 2, DVALUE2(8 + n), "request length");
}

void
AudioListBuckets (
    const unsigned char *buf)
{
    PrintField (buf, 0, 1, NASREQUEST, REQUESTHEADER);
    if (NasVerbose < 1)
	return;
    if (NasVerbose > 1)
	PrintField(SBf, 0, 4, CARD32, "sequence number");
    printfield(buf, 2, 2, DVALUE2(8 + n), "request length");
}

void
AudioListBucketsReply (
    const unsigned char *buf)
{
    PrintField (RBf, 0, 1, NASREPLY, REPLYHEADER);
    if (NasVerbose < 1)
	return;
    printfield(buf, 2, 2, CARD16, "sequence number");
    printfield(buf, 4, 4, DVALUE4((n + p) / 4), "reply length");
}

void
AudioGetBucketAttributes (
    const unsigned char *buf)
{
    PrintField (buf, 0, 1, NASREQUEST, REQUESTHEADER);
    if (NasVerbose < 1)
	return;
    if (NasVerbose > 1)
	PrintField(SBf, 0, 4, CARD32, "sequence number");
    printfield(buf, 2, 2, DVALUE2(8 + n), "request length");
}

void
AudioGetBucketAttributesReply (
    const unsigned char *buf)
{
    PrintField (RBf, 0, 1, NASREPLY, REPLYHEADER);
    if (NasVerbose < 1)
	return;
    printfield(buf, 2, 2, CARD16, "sequence number");
    printfield(buf, 4, 4, DVALUE4((n + p) / 4), "reply length");
}

void
AudioSetBucketAttributes (
    const unsigned char *buf)
{
    PrintField (buf, 0, 1, NASREQUEST, REQUESTHEADER);
    if (NasVerbose < 1)
	return;
    if (NasVerbose > 1)
	PrintField(SBf, 0, 4, CARD32, "sequence number");
    printfield(buf, 2, 2, DVALUE2(8 + n), "request length");
}

void
AudioCreateRadio (
    const unsigned char *buf)
{
    PrintField (buf, 0, 1, NASREQUEST, REQUESTHEADER);
    if (NasVerbose < 1)
	return;
    if (NasVerbose > 1)
	PrintField(SBf, 0, 4, CARD32, "sequence number");
    printfield(buf, 2, 2, DVALUE2(8 + n), "request length");
}

void
AudioDestroyRadio (
    const unsigned char *buf)
{
    PrintField (buf, 0, 1, NASREQUEST, REQUESTHEADER);
    if (NasVerbose < 1)
	return;
    if (NasVerbose > 1)
	PrintField(SBf, 0, 4, CARD32, "sequence number");
    printfield(buf, 2, 2, DVALUE2(8 + n), "request length");
}

void
AudioListRadios (
    const unsigned char *buf)
{
    PrintField (buf, 0, 1, NASREQUEST, REQUESTHEADER);
    if (NasVerbose < 1)
	return;
    if (NasVerbose > 1)
	PrintField(SBf, 0, 4, CARD32, "sequence number");
    printfield(buf, 2, 2, DVALUE2(8 + n), "request length");
}

void
AudioListRadiosReply (
    const unsigned char *buf)
{
    PrintField (RBf, 0, 1, NASREPLY, REPLYHEADER);
    if (NasVerbose < 1)
	return;
    printfield(buf, 2, 2, CARD16, "sequence number");
    printfield(buf, 4, 4, DVALUE4((n + p) / 4), "reply length");
}

void
AudioGetRadioAttributes (
    const unsigned char *buf)
{
    PrintField (buf, 0, 1, NASREQUEST, REQUESTHEADER);
    if (NasVerbose < 1)
	return;
    if (NasVerbose > 1)
	PrintField(SBf, 0, 4, CARD32, "sequence number");
    printfield(buf, 2, 2, DVALUE2(8 + n), "request length");
}

void
AudioGetRadioAttributesReply (
    const unsigned char *buf)
{
    PrintField (RBf, 0, 1, NASREPLY, REPLYHEADER);
    if (NasVerbose < 1)
	return;
    printfield(buf, 2, 2, CARD16, "sequence number");
    printfield(buf, 4, 4, DVALUE4((n + p) / 4), "reply length");
}

void
AudioSetRadioAttributes (
    const unsigned char *buf)
{
    PrintField (buf, 0, 1, NASREQUEST, REQUESTHEADER);
    if (NasVerbose < 1)
	return;
    if (NasVerbose > 1)
	PrintField(SBf, 0, 4, CARD32, "sequence number");
    printfield(buf, 2, 2, DVALUE2(8 + n), "request length");
}

void
AudioCreateFlow (
    const unsigned char *buf)
{
    PrintField (buf, 0, 1, NASREQUEST, REQUESTHEADER);
    if (NasVerbose < 1)
	return;
    if (NasVerbose > 1)
	PrintField(SBf, 0, 4, CARD32, "sequence number");
    printfield(buf, 2, 2, DVALUE2(8 + n), "request length");
}

void
AudioDestroyFlow (
    const unsigned char *buf)
{
    PrintField (buf, 0, 1, NASREQUEST, REQUESTHEADER);
    if (NasVerbose < 1)
	return;
    if (NasVerbose > 1)
	PrintField(SBf, 0, 4, CARD32, "sequence number");
    printfield(buf, 2, 2, DVALUE2(8 + n), "request length");
}

void
AudioGetFlowAttributes (
    const unsigned char *buf)
{
    PrintField (buf, 0, 1, NASREQUEST, REQUESTHEADER);
    if (NasVerbose < 1)
	return;
    if (NasVerbose > 1)
	PrintField(SBf, 0, 4, CARD32, "sequence number");
    printfield(buf, 2, 2, DVALUE2(8 + n), "request length");
}

void
AudioGetFlowAttributesReply (
    const unsigned char *buf)
{
    PrintField (RBf, 0, 1, NASREPLY, REPLYHEADER);
    if (NasVerbose < 1)
	return;
    printfield(buf, 2, 2, CARD16, "sequence number");
    printfield(buf, 4, 4, DVALUE4((n + p) / 4), "reply length");
}

void
AudioSetFlowAttributes (
    const unsigned char *buf)
{
    PrintField (buf, 0, 1, NASREQUEST, REQUESTHEADER);
    if (NasVerbose < 1)
	return;
    if (NasVerbose > 1)
	PrintField(SBf, 0, 4, CARD32, "sequence number");
    printfield(buf, 2, 2, DVALUE2(8 + n), "request length");
}

void
AudioGetElements (
    const unsigned char *buf)
{
    PrintField (buf, 0, 1, NASREQUEST, REQUESTHEADER);
    if (NasVerbose < 1)
	return;
    if (NasVerbose > 1)
	PrintField(SBf, 0, 4, CARD32, "sequence number");
    printfield(buf, 2, 2, DVALUE2(8 + n), "request length");
}

void
AudioGetElementsReply (
    const unsigned char *buf)
{
    PrintField (RBf, 0, 1, NASREPLY, REPLYHEADER);
    if (NasVerbose < 1)
	return;
    printfield(buf, 2, 2, CARD16, "sequence number");
    printfield(buf, 4, 4, DVALUE4((n + p) / 4), "reply length");
}

void
AudioSetElements (
    const unsigned char *buf)
{
    PrintField (buf, 0, 1, NASREQUEST, REQUESTHEADER);
    if (NasVerbose < 1)
	return;
    if (NasVerbose > 1)
	PrintField(SBf, 0, 4, CARD32, "sequence number");
    printfield(buf, 2, 2, DVALUE2(8 + n), "request length");
}

void
AudioGetElementStates (
    const unsigned char *buf)
{
    PrintField (buf, 0, 1, NASREQUEST, REQUESTHEADER);
    if (NasVerbose < 1)
	return;
    if (NasVerbose > 1)
	PrintField(SBf, 0, 4, CARD32, "sequence number");
    printfield(buf, 2, 2, DVALUE2(8 + n), "request length");
}

void
AudioElementState (
    const unsigned char *buf)
{
    printfield(buf, 0, 4, CARD32, "flow");
    printfield(buf, 4, 1, CARD8, "element number");
    printfield(buf, 5, 1, CARD8, "state");
}

void
AudioGetElementStatesReply (
    const unsigned char *buf)
{
    int	    n, i;
    int	    o;
    
    PrintField (RBf, 0, 1, NASREPLY, REPLYHEADER);
    if (NasVerbose < 1)
	return;
    printfield(buf, 2, 2, CARD16, "sequence number");
    printfield(buf, 4, 4, DVALUE4((n + p) / 4), "reply length");
    n = ILong(&buf[8]);
    o = 32;
    for (i = 0; i < n; i++)
    {
	AudioElementState (buf+o);
	o += 8;
    }
}

void
AudioSetElementStates (
    const unsigned char *buf)
{
    int	nstates, i;
    int	o;
    PrintField (buf, 0, 1, NASREQUEST, REQUESTHEADER);
    if (NasVerbose < 1)
	return;
    if (NasVerbose > 1)
	PrintField(SBf, 0, 4, CARD32, "sequence number");
    printfield(buf, 2, 2, DVALUE2(8 + n), "request length");
    printfield(buf, 4, 4, CARD32, "number states");
    nstates = ILong(&buf[4]);
    o = 8;
    for (i = 0; i < nstates; i++)
    {
	AudioElementState (buf+o);
	o += 8;
    }
}

void
AudioGetElementParameters (
    const unsigned char *buf)
{
    PrintField (buf, 0, 1, NASREQUEST, REQUESTHEADER);
    if (NasVerbose < 1)
	return;
    if (NasVerbose > 1)
	PrintField(SBf, 0, 4, CARD32, "sequence number");
    printfield(buf, 2, 2, DVALUE2(8 + n), "request length");
}

void
AudioGetElementParametersReply (
    const unsigned char *buf)
{
    PrintField (RBf, 0, 1, NASREPLY, REPLYHEADER);
    if (NasVerbose < 1)
	return;
    printfield(buf, 2, 2, CARD16, "sequence number");
    printfield(buf, 4, 4, DVALUE4((n + p) / 4), "reply length");
}

void
AudioSetElementParameters (
    const unsigned char *buf)
{
    PrintField (buf, 0, 1, NASREQUEST, REQUESTHEADER);
    if (NasVerbose < 1)
	return;
    if (NasVerbose > 1)
	PrintField(SBf, 0, 4, CARD32, "sequence number");
    printfield(buf, 2, 2, DVALUE2(8 + n), "request length");
}

void
AudioWriteElement (
    const unsigned char *buf)
{
    PrintField (buf, 0, 1, NASREQUEST, REQUESTHEADER);
    if (NasVerbose < 1)
	return;
    if (NasVerbose > 1)
	PrintField(SBf, 0, 4, CARD32, "sequence number");
    printfield(buf, 2, 2, DVALUE2(8 + n), "request length");
    printfield(buf, 4, 4, CARD32, "flow");
    printfield(buf, 8, 4, CARD32, "num bytes");
    printfield(buf, 12, 1, CARD8, "state");
}

void
AudioReadElement (
    const unsigned char *buf)
{
    PrintField (buf, 0, 1, NASREQUEST, REQUESTHEADER);
    if (NasVerbose < 1)
	return;
    if (NasVerbose > 1)
	PrintField(SBf, 0, 4, CARD32, "sequence number");
    printfield(buf, 2, 2, DVALUE2(8 + n), "request length");
}

void
AudioReadElementReply (
    const unsigned char *buf)
{
    PrintField (RBf, 0, 1, NASREPLY, REPLYHEADER);
    if (NasVerbose < 1)
	return;
    printfield(buf, 2, 2, CARD16, "sequence number");
    printfield(buf, 4, 4, DVALUE4((n + p) / 4), "reply length");
}

void
AudioGrabComponent (
    const unsigned char *buf)
{
    PrintField (buf, 0, 1, NASREQUEST, REQUESTHEADER);
    if (NasVerbose < 1)
	return;
    if (NasVerbose > 1)
	PrintField(SBf, 0, 4, CARD32, "sequence number");
    printfield(buf, 2, 2, DVALUE2(8 + n), "request length");
}

void
AudioUngrabComponent (
    const unsigned char *buf)
{
    PrintField (buf, 0, 1, NASREQUEST, REQUESTHEADER);
    if (NasVerbose < 1)
	return;
    if (NasVerbose > 1)
	PrintField(SBf, 0, 4, CARD32, "sequence number");
    printfield(buf, 2, 2, DVALUE2(8 + n), "request length");
}

void
AudioSendEvent (
    const unsigned char *buf)
{
    PrintField (buf, 0, 1, NASREQUEST, REQUESTHEADER);
    if (NasVerbose < 1)
	return;
    if (NasVerbose > 1)
	PrintField(SBf, 0, 4, CARD32, "sequence number");
    printfield(buf, 2, 2, DVALUE2(8 + n), "request length");
}

void
AudioGetAllowedUsers (
    const unsigned char *buf)
{
    PrintField (buf, 0, 1, NASREQUEST, REQUESTHEADER);
    if (NasVerbose < 1)
	return;
    if (NasVerbose > 1)
	PrintField(SBf, 0, 4, CARD32, "sequence number");
    printfield(buf, 2, 2, DVALUE2(8 + n), "request length");
}

void
AudioGetAllowedUsersReply (
    const unsigned char *buf)
{
    PrintField (RBf, 0, 1, NASREPLY, REPLYHEADER);
    if (NasVerbose < 1)
	return;
    printfield(buf, 2, 2, CARD16, "sequence number");
    printfield(buf, 4, 4, DVALUE4((n + p) / 4), "reply length");
}

void
AudioSetAllowedUsers (
    const unsigned char *buf)
{
    PrintField (buf, 0, 1, NASREQUEST, REQUESTHEADER);
    if (NasVerbose < 1)
	return;
    if (NasVerbose > 1)
	PrintField(SBf, 0, 4, CARD32, "sequence number");
    printfield(buf, 2, 2, DVALUE2(8 + n), "request length");
}

void
AudioListExtensions (
    const unsigned char *buf)
{
    PrintField (buf, 0, 1, NASREQUEST, REQUESTHEADER);
    if (NasVerbose < 1)
	return;
    if (NasVerbose > 1)
	PrintField(SBf, 0, 4, CARD32, "sequence number");
    printfield(buf, 2, 2, DVALUE2(8 + n), "request length");
}

void
AudioListExtensionsReply (
    const unsigned char *buf)
{
    PrintField (RBf, 0, 1, NASREPLY, REPLYHEADER);
    if (NasVerbose < 1)
	return;
    printfield(buf, 2, 2, CARD16, "sequence number");
    printfield(buf, 4, 4, DVALUE4((n + p) / 4), "reply length");
}

void
AudioQueryExtension (
    const unsigned char *buf)
{
    PrintField (buf, 0, 1, NASREQUEST, REQUESTHEADER);
    if (NasVerbose < 1)
	return;
    if (NasVerbose > 1)
	PrintField(SBf, 0, 4, CARD32, "sequence number");
    printfield(buf, 2, 2, DVALUE2(8 + n), "request length");
}

void
AudioQueryExtensionReply (
    const unsigned char *buf)
{
    PrintField (RBf, 0, 1, NASREPLY, REPLYHEADER);
    if (NasVerbose < 1)
	return;
    printfield(buf, 2, 2, CARD16, "sequence number");
    printfield(buf, 4, 4, DVALUE4((n + p) / 4), "reply length");
}

void
AudioGetCloseDownMode (
    const unsigned char *buf)
{
    PrintField (buf, 0, 1, NASREQUEST, REQUESTHEADER);
    if (NasVerbose < 1)
	return;
    if (NasVerbose > 1)
	PrintField(SBf, 0, 4, CARD32, "sequence number");
    printfield(buf, 2, 2, DVALUE2(8 + n), "request length");
}

void
AudioGetCloseDownModeReply (
    const unsigned char *buf)
{
    PrintField (RBf, 0, 1, NASREPLY, REPLYHEADER);
    if (NasVerbose < 1)
	return;
    printfield(buf, 2, 2, CARD16, "sequence number");
    printfield(buf, 4, 4, DVALUE4((n + p) / 4), "reply length");
}

void
AudioSetCloseDownMode (
    const unsigned char *buf)
{
    PrintField (buf, 0, 1, NASREQUEST, REQUESTHEADER);
    if (NasVerbose < 1)
	return;
    if (NasVerbose > 1)
	PrintField(SBf, 0, 4, CARD32, "sequence number");
    printfield(buf, 2, 2, DVALUE2(8 + n), "request length");
}

void
AudioKillClient (
    const unsigned char *buf)
{
    PrintField (buf, 0, 1, NASREQUEST, REQUESTHEADER);
    if (NasVerbose < 1)
	return;
    if (NasVerbose > 1)
	PrintField(SBf, 0, 4, CARD32, "sequence number");
    printfield(buf, 2, 2, DVALUE2(8 + n), "request length");
}

void
AudioGetServerTime (
    const unsigned char *buf)
{
    PrintField (buf, 0, 1, NASREQUEST, REQUESTHEADER);
    if (NasVerbose < 1)
	return;
    if (NasVerbose > 1)
	PrintField(SBf, 0, 4, CARD32, "sequence number");
    printfield(buf, 2, 2, DVALUE2(8 + n), "request length");
}

void
AudioGetServerTimeReply (
    const unsigned char *buf)
{
    PrintField (RBf, 0, 1, NASREPLY, REPLYHEADER);
    if (NasVerbose < 1)
	return;
    printfield(buf, 2, 2, CARD16, "sequence number");
    printfield(buf, 4, 4, DVALUE4((n + p) / 4), "reply length");
}

void
AudioNoOperation (
    const unsigned char *buf)
{
    PrintField (buf, 0, 1, NASREQUEST, REQUESTHEADER);
    if (NasVerbose < 1)
	return;
    if (NasVerbose > 1)
	PrintField(SBf, 0, 4, CARD32, "sequence number");
    printfield(buf, 2, 2, DVALUE2(8 + n), "request length");
}
