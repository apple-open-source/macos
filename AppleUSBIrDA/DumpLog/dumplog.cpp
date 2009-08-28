#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <netdb.h>
#include <setjmp.h>
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

#include <arpa/inet.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/IOCFSerialize.h>
#include <IOKit/network/IONetworkLib.h>

#include <mach/mach.h>
#include <mach/mach_interface.h>
#include <netinet/in.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/file.h>

#include "kdp_protocol.h"
#include "IrDALog.h"

#pragma mark -- Prototypes

// Prototypes
int DumpRemoteLog(int argc, char ** argv);
int DumpLocalLog();

// common subs
void DumpLog(void);         // dump the log to disk once we have it
void OutputBuffer(IrDALogHdr *hdr, IrDAEventDesc *events, char *msgs, FILE *out);
Boolean CheckLog(IrDALogHdr *obj);
FILE *CreateLogFile(void);  // make the output file

// network subs
Boolean SetPeer(char **argv);
Boolean DoRequest(kdp_req_t command, UInt32 length);
Boolean DoConnect(UInt16 localport);
Boolean DoDisconnect(void);
Boolean ReadLog(void *addr);        // addr is the kernal address of the info record
Boolean DoRead(void *remote_addr, void *local_addr, int length);

// iokit stuff
#if 0
// find this elsewhere ...
extern "C" kern_return_t io_connect_method_structureI_structureO
(
	mach_port_t connection,
	int selector,
	io_struct_inband_t input,
	mach_msg_type_number_t inputCnt,
	io_struct_inband_t output,
	mach_msg_type_number_t *outputCnt
);
#endif

typedef struct IrDACommand
{
    unsigned char commandID;    // one of the commands above (tbd)
    char data[1];               // this is not really one byte, it is as big as I like
				// I set it to 1 just to make the compiler happy
} IrDACommand;
typedef IrDACommand *IrDACommandPtr;

kern_return_t doCommand(io_connect_t con, unsigned char commandID,
			void *inputData, unsigned long inputDataSize,
			void *outputData, size_t *outputDataSize);

kern_return_t openDevice(io_object_t obj, io_connect_t * con);
kern_return_t closeDevice(io_connect_t con);
io_object_t   getInterfaceWithName(mach_port_t masterPort, char *className);

#pragma mark -- Globals

// Globals

char bigbuffer[10*1024*1024];   // fix: do two passes and allocate the appropriate size
IrDALogInfo info;               // pointers and sizes returned directly
UInt32      infoaddr;           // address of info block on remote machine

// globals for network

int f;                                  // our socket
struct  sockaddr_in peeraddr;
char    hostname[100];                  // hostname of peer

UInt8   packet[MAX_KDP_PKT_SIZE];       // just need one packet

					// kdp header stuff
UInt32  key;                            // session key (unique we hope)
UInt8   seq;                            // sequence number within session

// simple punt macro
#define Punt(x)     { fprintf(stderr, "%s: %s\n", argv[0], x); return -1; }
// punt macro with errno text too
#define PuntErr(x)  { fprintf(stderr, "%s: %s [%s]\n", argv[0], x, strerror(errno)); return 1; }

int
main(int argc, char ** argv)
{
    if (argc == 1)
	return DumpLocalLog();
    else if (argc == 3)
	return DumpRemoteLog(argc, argv);
    
    printf("usage: %s   # to dump irdalog on local machine\n", argv[0]);
    printf("       %s hostname 0xLogInfoAddress   # remote log\n", argv[0]);
    return 1;
}                   

#pragma mark -- Dump local log

int
DumpLocalLog()
{
    mach_port_t     masterPort;
    kern_return_t   kr;
    io_object_t     netif;
    io_connect_t    conObj;

    // Get master device port
    //
    kr = IOMasterPort(bootstrap_port, &masterPort);
    if (kr != KERN_SUCCESS) {
	printf("IOMasterPort() failed: %08lx\n", (unsigned long)kr);
	return -1;
    }
    netif = getInterfaceWithName(masterPort, "AppleIrDA");
    if (netif) {
	//printf("netif=0x%x\n", netif);
	kr = openDevice(netif, &conObj);
	if (kr == kIOReturnSuccess) {
	    UInt32 inbuf[2];            // big buffer address passed to userclient
	    size_t infosize;
	
	    inbuf[0] = (UInt32)&bigbuffer[0];
	    inbuf[1] = sizeof(bigbuffer);
	    infosize = sizeof(info);

	    //printf("bigbuf at 0x%x\n", (int)&bigbuffer[0]);
	    kr = doCommand(conObj, 0x12, &inbuf, sizeof(inbuf), &info, &infosize);
	    if (kr == kIOReturnSuccess) {
		//printf("command/request worked we think\n");
		DumpLog();
	    }
	    else printf("command/request failed 0x%x\n", kr);
	    closeDevice(conObj);
	}
	else printf("open device failed 0x%x\n", kr);
	IOObjectRelease(netif);
    }
	
    //printf("toodles\n");
    exit(0);
}


/* ==========================================
 * open/close device.
 * ========================================== */

kern_return_t
openDevice(io_object_t obj, io_connect_t * con)
{
    return IOServiceOpen(obj, mach_task_self(), 123, con);
}

kern_return_t
closeDevice(io_connect_t con)
{
    return IOServiceClose(con);
}

/* ==========================================
 * Look through the registry and search for an
 * IONetworkInterface objects with the given
 * name.
 * If a match is found, the object is returned.
 * =========================================== */

io_object_t
getInterfaceWithName(mach_port_t masterPort, char *className)
{
    kern_return_t   kr;
    io_iterator_t   ite;
    io_object_t     obj = 0;

    kr = IORegistryCreateIterator(masterPort,
				  kIOServicePlane,
				  true,                 /* recursive */
				  &ite);

    if (kr != kIOReturnSuccess) {
	printf("IORegistryCreateIterator() error %08lx\n", (unsigned long)kr);
	return 0;
    }

    while ((obj = IOIteratorNext(ite))) {
	if (IOObjectConformsTo(obj, (char *) className)) {
	    printf("Found IrDA UserClient !!\n");
	    break;
	}
    else {
      io_name_t name;
      kern_return_t rc;
      rc = IOObjectGetClass(obj, name);
      if (rc == kIOReturnSuccess) {
	//printf("Skipping class %s\n", name);
      }
    }
	IOObjectRelease(obj);
	obj = 0;
    }

    IOObjectRelease(ite);

    return obj;
}

kern_return_t
doCommand(io_connect_t con,
	    unsigned char commandID,
	    void *inputData, unsigned long inputDataSize,
	    void *outputData, size_t *outputDataSize)
{
	kern_return_t   err = KERN_SUCCESS;
	//mach_msg_type_number_t  outSize = outputDataSize;
       IrDACommandPtr command = NULL;

    // Creates a command block:
	command = (IrDACommandPtr)malloc (inputDataSize + sizeof (unsigned char));
	if (!command)
		return KERN_FAILURE;
	command->commandID = commandID;

    // Adds the data to the command block:
	if ((inputData != NULL) && (inputDataSize != 0))
		memcpy(command->data, inputData, inputDataSize);

    // Now we can (hopefully) transfer everything:
    err = IOConnectCallStructMethod(
			con,
			0,                                  /* method index */
			(char *) command,                   /* input[] */
			inputDataSize+sizeof(unsigned char),/* inputCount */
			(char *) outputData,                /* output */
			outputDataSize);                   /* buffer size, then result */
	free (command);
	return err;
}





#pragma mark -- Dump remote log

		    

int
DumpRemoteLog(int argc, char ** argv)
{ 
    struct sockaddr_in sin;     // our local address
    UInt16  localport;          // our port number
    Boolean ok;
    int rc;
	
    rc = sscanf(argv[2], "0x%lx", &infoaddr);
    if (rc != 1) PuntErr("failed to parse info address");
    
    printf("info address 0x%lx\n", infoaddr);
    
    (void)time((time_t *)&key);         // session key had better be a long
    
    f = socket(AF_INET, SOCK_DGRAM, 0);
    if (f < 0) PuntErr("create socket");
    
    // make the socket non-blocking
    rc = fcntl(f, F_SETFL, O_NONBLOCK);
    if (rc) PuntErr("failed to set non-blocking io");
    
    bzero((char *)&sin, sizeof(sin));
    sin.sin_family = AF_INET;           // system picks our port number
    rc = bind(f, (struct sockaddr *)&sin, sizeof(sin));
    if (rc) PuntErr("failed to bind to local udp port");

    if (1) {                // bind doesn't return our port number, sheesh.  get it now.    
	sockaddr buffer;
	socklen_t sz = sizeof(buffer);
	rc = getsockname(f, &buffer, &sz);      // udp returns length, family, port high, port low
	if (rc) PuntErr("Failed to get local udp port number");
	// seems our udp port number is in the first two bytes, wonder where this
	// is documented ...
	localport = (UInt8)buffer.sa_data[0] << 8 | (UInt8)buffer.sa_data[1];
	printf("Using local port number %d\n", localport);
    }

    ok = SetPeer(argv);                 // lookup the hostname
    if (!ok) return -1;
    
    printf("Debugging with %s [%s]\n", hostname, inet_ntoa(peeraddr.sin_addr));
    
    ok = DoConnect(localport);
    if (!ok) Punt("Connect failed");
    
    printf("Connected!\n");
    //sleep(5);
    
    ok = ReadLog((void *)infoaddr);
    if (!ok) printf("read failed\n");
    else DumpLog();
    
    ok = DoDisconnect();
    if (!ok) Punt("Disconnect failed");
    
    close(f);
    
    printf("Toodles!\n");
    return 0;
}

// return true if hostname on command line is ok
Boolean
SetPeer(char **argv)
{
    struct hostent *host;

    host = gethostbyname(argv[1]);
    if (host) {
	peeraddr.sin_family = host->h_addrtype;
	bcopy(host->h_addr, &peeraddr.sin_addr, host->h_length);
	strcpy(hostname, host->h_name);
    } else {
	peeraddr.sin_family = AF_INET;
	peeraddr.sin_addr.s_addr = inet_addr(argv[1]);
	if (peeraddr.sin_addr.s_addr == (unsigned)-1) {
	    printf("%s: unknown host '%s'\n", argv[0], argv[1]);
	    return false;
	}
	strcpy(hostname, argv[1]);
    }
    peeraddr.sin_port = KDP_REMOTE_PORT;
    return true;
}

Boolean
DoConnect(UInt16 localport)
{
    kdp_connect_req_t   *request    = (kdp_connect_req_t   *)packet;
    kdp_connect_reply_t *reply      = (kdp_connect_reply_t *)packet;
    char *msg = "Boo";
    int length;
    Boolean ok;
    
    bzero(packet, sizeof(packet));
    
    request->req_reply_port = localport;
    request->exc_note_port  = localport;        // ****** REVIEW ******
    strcpy(request->greeting, msg);             // not really referenced
    length = sizeof(kdp_connect_req_t) + strlen(msg) + 1;
    
    ok = DoRequest(KDP_CONNECT, length);    // send packet, wait for response
    if (ok && reply->error == 0)
	return true;
    
    return false;
}

//
// Fill in the header and send off a request.
// Wait for and read the response.
// Do minimal checking on response packet
// todo: if no response, resend a couple times
//
Boolean
DoRequest(kdp_req_t command, UInt32 length)
{
    socklen_t fromlen;
    struct sockaddr_in from;
    kdp_hdr_t   *hdr = (kdp_hdr_t *)packet;
    ssize_t rc;
    fd_set  fdset;
    time_t  timeout;
	
    // fill in the header
    hdr->request    = command;
    hdr->is_reply   = 0;
    hdr->seq        = seq++;
    hdr->len        = length;
    hdr->key        = key;
    
    // send the request in the packet buffer
    rc = sendto(f, packet, length, 0,
		    (struct sockaddr *)&peeraddr, sizeof(peeraddr));
    if (rc != (ssize_t)length) return false;
    
    // now wait for a response
    FD_ZERO(&fdset);
    FD_SET(f, &fdset);                  // prepare to wait until data read on f
    timeout = time(NULL) + 5;           // 5 second max timeout
    while (1) {                         // keep trying until timeout
	struct timeval t;
	time_t  now, delta;
	
	now = time(NULL);               // get current time
	delta = timeout - now;          // seconds left until timeout
	if (delta <= 0) return false;   // timeout, bail
	
	t.tv_sec = delta + 1;           // wait a bit more
	t.tv_usec = 0;
	rc = select(f+1, &fdset, NULL, NULL, &t);
	if (rc <= 0) continue;          // loop if no data ready
	fromlen = sizeof(from);
	rc = recvfrom(f, packet, sizeof(packet), 0,
	    (struct sockaddr *)&from, &fromlen);
	if (rc > 0) {
	    if (hdr->request == command &&      // if same request type
		hdr->is_reply &&                // but a reply
		hdr->seq == (UInt8)(seq-1) &&   // right sequence number
		hdr->key == key)                // and right session key
		    return true;                // then have an ack (data in packet for caller)
	}
    }
    return false;       // timeout
}   
  

Boolean
DoDisconnect()
{
    Boolean ok;

    bzero(packet, sizeof(packet));
    
    ok = DoRequest(KDP_DISCONNECT, sizeof(kdp_disconnect_req_t));
    
    return ok;  // check response?
}

Boolean
ReadLog(void *addr)
{
    Boolean ok;
    char *local = bigbuffer;
    
    // First, read the info block
    ok = DoRead(addr, &info, sizeof(info));
    if (!ok) return ok;
    
    printf("hdr  at 0x%lx, size %ld\n", (UInt32)info.hdr,       info.hdrSize);
    printf("log  at 0x%lx, size %ld\n", (UInt32)info.eventLog,  info.eventLogSize);
    printf("msgs at 0x%lx, size %ld\n", (UInt32)info.msgBuffer, info.msgBufferSize);
    
    ok = DoRead(info.hdr, local, info.hdrSize);
    if (!ok) return ok;
    local += info.hdrSize;
    
    ok = DoRead(info.eventLog, local, info.eventLogSize);
    if (!ok) return ok;
    local += info.eventLogSize;
    
    ok = DoRead(info.msgBuffer, local, info.msgBufferSize);
    return ok;
}

Boolean
DoRead(void *remote_addr, void *local_addr, int length)
{
    kdp_readmem_req_t   *req    = (kdp_readmem_req_t *)packet;
    kdp_readmem_reply_t *reply  = (kdp_readmem_reply_t *)packet;
    Boolean ok;

    while (length > MAX_KDP_DATA_SIZE) {
	ok = DoRead(remote_addr, local_addr, MAX_KDP_DATA_SIZE);
	if (!ok) return ok;
	remote_addr = (UInt8 *)remote_addr + MAX_KDP_DATA_SIZE;
	local_addr  = (UInt8 *)local_addr  + MAX_KDP_DATA_SIZE;
	length -= MAX_KDP_DATA_SIZE;
    }

    bzero(packet, sizeof(packet));
    req->address = remote_addr;
    req->nbytes = length;
    ok = DoRequest(KDP_READMEM, sizeof(kdp_readmem_req_t));
    if (!ok) return ok;
    
    if (reply->error != 0) {
	printf("read error %d\n", reply->error);
	return false;
    }
    
    // use it
    bcopy(&reply->data[0], local_addr, length);
    return true;
}

#pragma mark -- Common log output routines

void
DumpLog()
{
    IrDALogHdr *hdr = (IrDALogHdr *)&bigbuffer[0];
    IrDAEventDesc   *events = (IrDAEventDescPtr)&bigbuffer[info.hdrSize];
    char *msgs = (char *)&bigbuffer[info.hdrSize + info.eventLogSize];
    FILE *out;
    
    /***
    
    printf("Kernel data:\n");
    printf("Hdr    at 0x%lx, size %ld\n", (UInt32)info.hdr,       info.hdrSize);
    printf("Events at 0x%lx, size %ld\n", (UInt32)info.eventLog,  info.eventLogSize);
    printf("Msgs   at 0x%lx, size %ld\n", (UInt32)info.msgBuffer, info.msgBufferSize);
    
    printf("My addresses:\n");
    printf("Hdr    at 0x%lx\n", (UInt32)hdr);
    printf("Events at 0x%lx\n", (UInt32)events);
    printf("Msgs   at 0x%lx\n", (UInt32)msgs);
    
    printf("eventindex %ld, print %ld, count %ld, enabled %d, wrapped %d\n",
	hdr->fEventIndex, hdr->fPrintIndex, hdr->fNumEntries,
	hdr->fTracingOn, hdr->fWrapped);
    
    ***/
    
    if (CheckLog(hdr)) {                // if any new entries
	out = CreateLogFile();
	if (out) {
	    OutputBuffer(hdr, events, msgs, out);
	    fclose(out);
	}
	printf("Done\n");
    }
}


FILE *
CreateLogFile(void)
{   
    time_t  now;
    struct tm *lt;
    char filename[100];
    
    time(&now);
    lt = localtime(&now);
    sprintf(filename, "log.%d.%d.%02d%02d.%02d",
	    lt->tm_mon+1, lt->tm_mday, lt->tm_hour, lt->tm_min, lt->tm_sec); 
    printf("Writing log to %s\n", filename);
    
    return fopen(filename, "w");
}

// return true if there are messages to dump
Boolean CheckLog(IrDALogHdr *obj)
{
    if (obj->fWrapped == false &&                   // if we've not wrapped around and
	obj->fEventIndex == obj->fPrintIndex) {     // print index == new event index, then nothing new
	printf("No new log entries\n" );
	printf("gIrDALog = 0x%lx\n", (UInt32)obj);
	printf("buffer   = 0x%lx\n", (UInt32)obj->fEventBuffer);
	printf("index    = %ld\n",   obj->fEventIndex);
	printf("prtindex = %ld\n",   obj->fPrintIndex);
	printf("#entries = %ld\n",   obj->fNumEntries);
	printf("traceon  = %d\n",   obj->fTracingOn);
	printf("wrapped  = %d\n",   obj->fWrapped);
	return false;
    }
    return true;
}


void OutputBuffer(IrDALogHdr *obj, IrDAEventDesc *events, char *msgs, FILE *out)
{   

    UInt16          len;
    char            buffer[256];
    IrDAEventDescPtr        eventPtr;                                   
    Boolean oldTracingFlag;
    
    /***/
    
    if (obj->fWrapped == false &&                   // if we've not wrapped around and
	obj->fEventIndex == obj->fPrintIndex) {     // print index == new event index, then nothing new
	printf("No new log entries\n" );
	printf("gIrDALog = 0x%lx\n", (UInt32)obj);
	printf("buffer   = 0x%lx\n", (UInt32)obj->fEventBuffer);
	printf("index    = %ld\n",   obj->fEventIndex);
	printf("prtindex = %ld\n",   obj->fPrintIndex);
	printf("#entries = %ld\n",   obj->fNumEntries);
	printf("traceon  = %d\n",   obj->fTracingOn);
	printf("wrapped  = %d\n",   obj->fWrapped);
	return;
    }
    /****/
    
    oldTracingFlag = obj->fTracingOn;           // Save old value of tracing enabled bit
    obj->fTracingOn = false;                    // temporarily turn off tracing during dcmd (let's not race)

    if (obj->fWrapped) {                        // if adding new entries wrapped over print index
	obj->fPrintIndex = obj->fEventIndex;    // then start printing at next "avail" entry
    }
    
    if( obj->fPrintIndex >= obj->fNumEntries )  // sanity check only, this shouldn't happen
	obj->fPrintIndex = 0;
	
    do{                     
	UInt32 secs, usecs;     // pulled out of timestamp
	static UInt32 lasttime = 0;
	SInt32 deltaTime;
	char *msg;
			    
		    
	eventPtr = &obj->fEventBuffer[obj->fPrintIndex];
	eventPtr = (eventPtr - info.eventLog) + events;         // adjust event ptr for kernel/user address space change
	msg = (eventPtr->msg - info.msgBuffer) + msgs;          // adjust msg ptr too
	
	//printf("msg before 0x%lx, after 0x%lx\n", (UInt32)eventPtr->msg, (UInt32)msg);
	//{ UInt32 *p = (UInt32 *)msg;
	//  printf("first two are %lx %lx\n", p[0], p[1]);
	//}
	
	if (eventPtr->timeStamp) {          // if log entry is timestamped
	    secs = eventPtr->timeStamp / 1000000;
	    usecs = eventPtr->timeStamp % 1000000;
	    
	    deltaTime = eventPtr->timeStamp - lasttime;
	    lasttime = eventPtr->timeStamp;
	    if (deltaTime > 999999 || deltaTime < 0) deltaTime = 999999;
	    len = sprintf( buffer, "%4ld.%06ld [%6ld]  D: %04hx,%04hx  %s", 
											secs, usecs,
											deltaTime,
											eventPtr->data1,
											eventPtr->data2,
											msg );
	}
	else    // timestamp not provided, blank out to match above spacing
	    len = sprintf( buffer, "   x.x      [      ]  D: %04hx,%04hx  %s", 
						    //secs, usecs,
						    //deltaTime,
						    eventPtr->data1,
						    eventPtr->data2,
						    msg );

	fprintf(out,  buffer );
	fprintf(out, "\n");

	obj->fPrintIndex++;
	
	if( obj->fPrintIndex >= obj->fNumEntries )  // wrap print index at end of circular buffer
	    obj->fPrintIndex = 0;
	    
    } while((obj->fPrintIndex != obj->fEventIndex) );
    
    obj->fPrintIndex = obj->fEventIndex;    // FLUSH PENDING LOG ENTRIES if aborted
					    // we shouldn't do this once we get a -C flag :-)
    obj->fWrapped = false;                  // reset wrapped flag
    obj->fTracingOn = oldTracingFlag;       // restore tracing state (enable again)
}
