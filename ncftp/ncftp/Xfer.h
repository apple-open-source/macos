/* Xfer.h */

#ifndef _xfer_h_
#define _xfer_h_	1

#ifndef _ftp_h_
#include "FTP.h"
#endif

#ifndef _rcmd_h_
#include "RCmd.h"
#endif

/* This is for block transfer mode.  See RFC 959. */
typedef struct FTPBlockHeader {
	unsigned char desc;
	unsigned char byteCount[2];
} FTPBlockHeader;

/* See RFC 959. */
#define kRegularBlock	 0
#define kLastBlock	 64

#define kXferBufSize 32768			       /* Must be 2^16 to support block mode. */

/* These coincide with the definitions of kAcceptForWriting, etc., in FTP.h. */
#define kNetWriting kAcceptForWriting
#define kNetReading kAcceptForReading

#define NETREADING(xp) ((xp)->netMode == kNetReading)
#define NETWRITING(xp) ((xp)->netMode == kNetWriting)

/* More stuff for XferSpec setup.
 * Typically, all of these are used at once when
 * you don't want progress reports.
 */
#define kNoReports 0
#define kFileSizeDontCare 0L
#define kLocalFileIsStdout	"-"

/* Some BlockProcs will timeout after the defined interval, and they
 * return this instead of just -1 for a regular error.
 */
#define kTimeoutErr (-2)

/* typedef struct XferSpec *XferSpecPtr;  done in RCmd.h.  */

#define kNoTransfer ((XferSpecPtr) 0)

typedef int (*XferProc) (XferSpecPtr);

/* Each progress meter function follows this calling format. */
typedef int (*ProgressMeterProc) (XferSpecPtr, int);

typedef struct XferSpec {
	/* These must be filled in by you. */
	int netMode;		       /* Reading or writing Net I/O? */
	XferProc xProc;
	int inStream;
	int outStream;

	/* You can use this to point to another structure if you like. */
	void *miscPtr;

	/* Filled in by you if you want progress reports. */
	int doReports;
	char *localFileName;
	char *remoteFileName;
	long expectedSize;
	long startPoint;

	/* These are filled in by RDataCmd. */
	ResponsePtr cmdResp;
	ResponsePtr xferResp;

	/* These are filled in by Progress routines. */
	int progMeterInUse;
	ProgressMeterProc prProc;
	long bytesTransferred;
	long bytesLeft;
	double frac;
	double bytesPerSec;
	double secsElap;
	struct timeval startTime;
	struct timeval endTime;
	long timeOfNextUpdate;

	/* Needed to guarantee that the file times get set. */
	int doUTime;
	time_t remoteModTime;
	int aborted;
	int outIsTTY;
	int inIsTTY;
	Sig_t origPipe;
	Sig_t origIntr;
	int atEof;		       /* Block mode needs this. */
} XferSpec;

#define CLEARXFERSPEC(R)	PTRZERO(R, sizeof(XferSpec))

typedef int (*NetReadProc) (XferSpecPtr xp);
typedef int (*NetWriteProc) (XferSpecPtr xp, char *, int);

/* Xfer.c */
void InitXferBuffer(void);
int BufferGets(char *, size_t, XferSpecPtr);
void XferSigHandler(int);
XferSpecPtr InitXferSpec(void);
void DoneWithXferSpec(XferSpecPtr);
void AbortDataTransfer(XferSpecPtr);
void EndTransfer(XferSpecPtr);
void StartTransfer(XferSpecPtr);
int StdAsciiFileReceive(XferSpecPtr);
int StdAsciiFileSend(XferSpecPtr);
int StdFileReceive(XferSpecPtr);
int StdFileSend(XferSpecPtr);
int StreamModeRead(XferSpecPtr);
int StreamModeWrite(XferSpecPtr, char *, int);
int BlockModeRead(XferSpecPtr);
int BlockModeWrite(XferSpecPtr, char *, int);

#endif /* _xfer_h_ */
