/* Xfer.c */

#include "Sys.h"

#include <signal.h>
#include <setjmp.h>
#include <errno.h>

#define _xfer_c_ 1

#include "Util.h"
#include "Main.h"
#include "Xfer.h"
#include "RCmd.h"
#include "FTP.h"
#include "Progress.h"
#include "Sio.h"

/* Large buffer to hold blocks of data during transferring. */
char *gXferBuf = NULL;
char *gAsciiSendBuf = NULL;

/* Size of the transfer buffer.  */
size_t gXferBufSize = kXferBufSize;

/* Stores whether we had an interrupt occur during the transfer. */
int gXferAbortFlag = 0;

char *gSecondaryBufPtr;
char *gSecondaryBufLimit;

int gUsingBufferGets;
jmp_buf gXferTimeoutJmp;

NetReadProc gNetReadProc;
NetWriteProc gNetWriteProc;

extern int gDebug, gMode;
extern int gStdout;
extern int gNetworkTimeout;


void 
InitXferBuffer(void)
{
	gXferBufSize = (size_t) kXferBufSize;
	gXferBuf = malloc(gXferBufSize);
	if (gXferBuf == NULL) {
		fprintf(stderr, "Not enough memory for transfer buffer.\n");
		Exit(kExitOutOfMemory);
	}
}						       /* InitXferBuffer */




int 
BufferGets(char *buf, size_t bufsize, XferSpecPtr xp)
{
	int err;
	char *src;
	char *dst;
	char *dstlim;
	int len;
	int nr;

	gUsingBufferGets = 1;
	err = 0;
	dst = buf;
	dstlim = dst + bufsize - 1;		       /* Leave room for NUL. */
	src = gSecondaryBufPtr;
	for (; dst < dstlim;) {
		if (src >= gSecondaryBufLimit) {
			/* Fill the buffer. */
			nr = (*gNetReadProc) (xp);
			if (nr == 0) {
				/* EOF. */
				goto done;
			} else if (nr < 0) {
				/* Error. */
				err = -1;
				goto done;
			}
			gSecondaryBufPtr = gXferBuf;
			gSecondaryBufLimit = gXferBuf + nr;
			src = gSecondaryBufPtr;
		}
		if (*src == '\r') {
			++src;
		} else {
			if (*src == '\n') {
				*dst++ = *src++;
				goto done;
			}
			*dst++ = *src++;
		}
	}

      done:
	gSecondaryBufPtr = src;
	*dst = '\0';
	len = (int) (dst - buf);
	if (err < 0)
		return (err);
	return (len);
}						       /* BufferGets */




/* We get here upon a signal we can handle during transfers. */
void 
XferSigHandler(int sigNum)
{
	gXferAbortFlag = sigNum;
#if 1
	/* Not a good thing to do in general from a signal handler... */
	TraceMsg("XferSigHandler: SIG %d.\n", sigNum);
#endif
	return;
}						       /* XferSigHandler */




/* This initializes a transfer information block to zeroes, and
 * also initializes the two Response blocks.
 */
XferSpecPtr 
InitXferSpec(void)
{
	XferSpecPtr xp;

	xp = (XferSpecPtr) calloc(SZ(1), sizeof(XferSpec));
	if (xp == NULL)
		OutOfMemory();
	xp->cmdResp = InitResponse();
	xp->xferResp = InitResponse();
	return (xp);
}						       /* InitXferSpec */




/* Disposes the transfer information block, and the responses within it. */
void 
DoneWithXferSpec(XferSpecPtr xp)
{
	DoneWithResponse(xp->cmdResp);
	DoneWithResponse(xp->xferResp);
	CLEARXFERSPEC(xp);
	free(xp);
}						       /* DoneWithXferSpec */




void 
AbortDataTransfer(XferSpecPtr xp)
{
	DebugMsg("Start Abort\n");
	SendTelnetInterrupt();			       /* Probably could get by w/o doing this. */

	/* If we aborted too late, and the server already sent the whole thing,
	 * it will just respond a 226 Transfer completed to our ABOR.
	 * But if we actually aborted, we'll get a 426 reply instead, then the
	 * server will send another 226 reply.  So if we get a 426 we'll
	 * print that quick and get rid of it by NULLing it out;  RDataCmd()
	 * will then do its usual GetResponse and get the pending 226.
	 *
	 * If we get the 226 here, we don't want RDataCmd() to try and get
	 * another response.  It will check to see if there is already is
	 * one, and if so, not get a response.
	 */
	(void) RCmd(xp->xferResp, "ABOR");

	if (xp->xferResp->code == 426) {
		TraceMsg("(426) Aborted in time.\n");
		ReInitResponse(xp->xferResp);
	}
	CloseDataConnection(1);			       /* Must close (by protocol). */

	DebugMsg("End Abort\n");
}						       /* AbortDataTransfer */




static void
AbortXfer(XferSpecPtr xp)
{
	Sig_t origIntr, origPipe;

	/* Don't interrupt while aborting. */
	origIntr = SIGNAL(SIGINT, SIG_IGN);
	origPipe = SIGNAL(SIGPIPE, SIG_IGN);

	/* It's important to make sure that the local file gets it's times 
	 * set correctly, so that reget works like it should.
	 * When we call AbortDataTransfer, often the server just hangs up.
	 */
	if ((xp->netMode == kNetReading) && (xp->outStream != gStdout))
		SetLocalFileTimes(xp->doUTime, xp->remoteModTime, xp->localFileName);
	if (gXferAbortFlag == SIGPIPE) {
		if (gDebug)
			EPrintF("\r** Broken pipe: Lost data connection **\n");
		else
			EPrintF("\r** Lost data connection **\n");
	} else if ((gXferAbortFlag == SIGINT) || (gXferAbortFlag == 0)) {
		EPrintF("\r** Aborting Transfer **\n");
	} else {
		EPrintF("\r** Aborting Transfer (%d) **\n", gXferAbortFlag);
	}
	AbortDataTransfer(xp);
	(void) SIGNAL(SIGINT, origIntr);
	(void) SIGNAL(SIGPIPE, origPipe);
}						       /* AbortXfer */



void
EndTransfer(XferSpecPtr xp)
{
	if (xp->aborted == 1)
		AbortXfer(xp);
	else if ((gMode == 'B') && (xp->netMode != kNetReading)) {
		/* Send EOF block if using Block mode store. */
		BlockModeWrite(xp, NULL, 0);
	}
	gXferAbortFlag = 0;

	/* Always call EndProgress, because that does logging too. */
	EndProgress(xp);
	(void) SIGNAL(SIGINT, xp->origIntr);
	(void) SIGNAL(SIGPIPE, xp->origPipe);
}						       /* EndTransfer */



void
StartTransfer(XferSpecPtr xp)
{
	gXferAbortFlag = 0;
	errno = 0;

	/* In case we happen to use BufferGets, this line sets the buffer pointer
	 * so that the first thing BufferGets will do is reset and fill the buffer
	 * using real I/O.
	 */
	gSecondaryBufPtr = gXferBuf + gXferBufSize;
	gUsingBufferGets = 0;

	xp->outIsTTY = (xp->outStream < 0) ? 0 : isatty(xp->outStream);
	xp->inIsTTY = (xp->inStream < 0) ? 0 : isatty(xp->inStream);

	/* Always call StartProgress, because that initializes the logging * stuff too. */
	StartProgress(xp);

	if (gMode == 'B') {
		gNetReadProc = BlockModeRead;
		gNetWriteProc = BlockModeWrite;
	} else {
		gNetReadProc = StreamModeRead;
		gNetWriteProc = StreamModeWrite;
	}

	xp->origIntr = SIGNAL(SIGINT, XferSigHandler);
	xp->origPipe = SIGNAL(SIGPIPE, XferSigHandler);
}						       /* StartTransfer */



int
StdAsciiFileReceive(XferSpecPtr xp)
{
	int nread, nwrote;
	int fd;
	char *xbuf;
	char *i, *o;
	int ct;

	fd = xp->outStream;
	xbuf = gXferBuf;

	for (;;) {
		nread = (*gNetReadProc) (xp);
		if (nread <= 0)
			break;

		/* In ASCII mode, all end-of-lines are denoted by CR/LF.
		 * For UNIX, we don't want that, we want just LFs, so
		 * skip all the CR's.
		 */
		for (i = o = xbuf, ct = 0; ct < nread; ct++, ++i) {
			if (*i != '\r')
				*o++ = *i;
		}
		nread = (int) (o - xbuf);

		nwrote = Swrite(fd, xbuf, nread, gNetworkTimeout);
		if (nwrote <= 0)
			break;
	}
	return (nread);				       /* 0 or -1 */
}						       /* StdAsciiFileReceive */




int
StdAsciiFileSend(XferSpecPtr xp)
{
	int nread, nwrote;
	int fd;
	size_t aBufSize;
	char *cp1, *cp2, *lim;

	/* For ASCII sends, we have a special case where
	 * we use another buffer.  We don't want to write
	 * single lines at a time on the stream for
	 * efficiency (especially with block transfer mode).
	 * We need to assume that the entire block 
	 * we read from the file could be all \n's which is
	 * why we only read half the maximum size of the
	 * transfer buffer (because each \n must be
	 * converted to \r\n).
	 */
	aBufSize = (gXferBufSize / 2);
	if (gAsciiSendBuf == NULL) {
		/* Only allocate this on a need basis,
		 * since ASCII sends are very rare.
		 */
		gAsciiSendBuf = malloc(aBufSize + 1);
		gAsciiSendBuf[aBufSize] = '\0';
		if (gAsciiSendBuf == NULL) {
			fprintf(stderr, "Not enough memory for ascii send buffer.\n");
			Exit(kExitOutOfMemory);
		}
	}
	fd = xp->inStream;
	for (;;) {
		nread = Sread(fd, gAsciiSendBuf, aBufSize, gNetworkTimeout, 0);
		if (nread <= 0) {
			if ((nread < 0) && (errno == EINTR))
				continue;
			break;
		}
		cp1 = gAsciiSendBuf;
		lim = cp1 + nread;
		cp2 = gXferBuf;
		while (cp1 < lim) {
			if (*cp1 == '\n')
				*cp2++ = '\r';
			*cp2++ = *cp1++;
		}

		nread = (int) (cp2 - gXferBuf);
		nwrote = (*gNetWriteProc) (xp, gXferBuf, nread);
		if (nwrote <= 0)
			break;
	}
	return (nread);				       /* 0 or -1 */
}						       /* StdAsciiFileSend */




int
StdFileReceive(XferSpecPtr xp)
{
	int nread, nwrote;
	int fd;
	char *xbuf;

	xbuf = gXferBuf;
	fd = xp->outStream;

	/* Special case the most common ones,
	 * so we don't repeatedly have to evaluate
	 * the NetProc.
	 */
	if (gNetReadProc == StreamModeRead) {
		for (;;) {
			nread = StreamModeRead(xp);
			if (nread <= 0)
				break;
			nwrote = Swrite(fd, xbuf, nread, gNetworkTimeout);
			if (nwrote < 0) {
				nread = nwrote;
				break;
			}
		}
	} else {
		for (;;) {
			nread = (*gNetReadProc) (xp);
			if (nread <= 0)
				break;
			nwrote = Swrite(fd, xbuf, nread, gNetworkTimeout);
			if (nwrote < 0) {
				nread = nwrote;
				break;
			}
		}
	}
	return (nread);				       /* 0 or -1 */
}						       /* StdFileReceive */




int
StdFileSend(XferSpecPtr xp)
{
	int nread, nwrote;
	int fd;
	char *xbuf;
	size_t xbsize;

	xbuf = gXferBuf;
	xbsize = gXferBufSize;
	fd = xp->inStream;
	nwrote = 0;

	/* Special case the most common ones,
	 * so we don't repeatedly have to evaluate
	 * the NetProc.
	 */
	if (gNetWriteProc == StreamModeWrite) {
		for (;;) {
			nread = Sread(fd, xbuf, xbsize, gNetworkTimeout, 0);
			if (nread <= 0) {
				if ((nread < 0) && (errno == EINTR))
					continue;
				break;
			}
			nwrote = StreamModeWrite(xp, xbuf, nread);
			if (nwrote < 0)
				break;
		}
	} else {
		for (;;) {
			nread = Sread(fd, xbuf, xbsize, gNetworkTimeout, 0);
			if (nread <= 0) {
				if ((nread < 0) && (errno == EINTR))
					continue;
				break;
			}
			nwrote = (*gNetWriteProc) (xp, xbuf, nread);
			if (nwrote < 0)
				break;
		}
	}
	return (nwrote);			       /* 0 or -1 */
}						       /* StdFileSend */




int
StreamModeRead(XferSpecPtr xp)
{
	int in;
	int nRead;

	in = xp->inStream;

	for (;;) {
		if (gXferAbortFlag > 0) {
			xp->aborted = 1;
			nRead = -1;
			goto done;
		}
		nRead = Sread(in, gXferBuf, gXferBufSize, gNetworkTimeout, 0);
		if (nRead <= 0) {
			if (nRead == 0)
				break;		       /* EOF. */
			if (errno == EINTR) {
				if (xp->doReports)
					ProgressReport(xp, kOptionalUpdate);
				continue;
			}
			Error(kDoPerror, "Error occurred during read!\n");
			xp->aborted = 1;
			nRead = -1;
			goto done;
		}
		break;
	}

	xp->bytesTransferred += nRead;
	if (xp->doReports)
		ProgressReport(xp, kOptionalUpdate);

      done:
	return (nRead);
}						       /* StreamModeRead */



int
StreamModeWrite(XferSpecPtr xp, char *wbuf, int nRead)
{
	int out;
	int nPut, totalPut;

	out = xp->outStream;

	for (totalPut = 0;;) {
		if (gXferAbortFlag > 0) {
			xp->aborted = 1;
			totalPut = -1;
			goto done;
		}
		nPut = Swrite(out, wbuf, (size_t) nRead, gNetworkTimeout);
		if (nPut < 0) {
			if (errno != EINTR) {
				Error(kDoPerror, "Error occurred during write!\n");
				xp->aborted = 1;
				totalPut = -1;
				goto done;
			}
		} else {
			totalPut += nPut;
			if (nPut == nRead)
				break;
			/* Short write; just write the rest * of it. */
			wbuf += nPut;
			nRead -= nPut;
		}
		if (xp->doReports)
			ProgressReport(xp, kOptionalUpdate);
	}

	xp->bytesTransferred += nRead;
	if (xp->doReports)
		ProgressReport(xp, kOptionalUpdate);

      done:
	return (totalPut);
}						       /* StreamModeWrite */




int
BlockModeRead(XferSpecPtr xp)
{
	int in;
	int nRead;
	size_t bsize;
	FTPBlockHeader h;

	in = xp->inStream;

	if (xp->atEof != 0) {
		nRead = 0;
	} else for (;;) {
		if (gXferAbortFlag > 0) {
			xp->aborted = 1;
			nRead = -1;
			goto done;
		}
		nRead = Sread(in, (char *) &h, (size_t) 3, gNetworkTimeout, 1);
		if (nRead < 3) {
			/* Could not read block header. */
			Error(kDoPerror, "Error occurred during read!\n");
			xp->aborted = 1;
			nRead = -1;
			goto done;
		}
		bsize = (h.byteCount[0] << 8) | (h.byteCount[1]);
		if (bsize == 0) {
			nRead = 0;
			if ((h.desc & kLastBlock) != 0) {
				xp->atEof = 1;
				break;	       /* EOF, done. */
			}
			continue;	       /* Empty block? */
		}
		if (gXferAbortFlag > 0) {
			xp->aborted = 1;
			nRead = -1;
			goto done;
		}
		nRead = Sread(in, gXferBuf, bsize, gNetworkTimeout, 1);
		if (nRead <= 0) {
			Error(kDoPerror, "Error occurred during read!\n");
			xp->aborted = 1;
			nRead = -1;
			goto done;
		}
		if ((h.desc & kLastBlock) != 0)
			xp->atEof = 1;
		break;
	}

	xp->bytesTransferred += nRead;
	if (xp->doReports)
		ProgressReport(xp, kOptionalUpdate);

      done:
	return (nRead);
}						       /* BlockModeRead */



int
BlockModeWrite(XferSpecPtr xp, char *wbuf, int nRead)
{
	int out;
	unsigned short c;
	int nPut;
	FTPBlockHeader h;

	out = xp->outStream;

	if (xp->atEof != 0) {
		/* Already sent EOF. */
		return (-1);
	}
	if (gXferAbortFlag > 0) {
		xp->aborted = 1;
		nPut = -1;
		goto done;
	}
	h.desc = (unsigned char) kRegularBlock;
	if (nRead == 0) {
		/* Send EOF block, which was just the header. */
		h.desc = kLastBlock;
	}
	c = ((unsigned short) (nRead & 0xffff));
	h.byteCount[0] = (unsigned char) ((c >> 8) & 0xff);
	h.byteCount[1] = (unsigned char) (c & 0xff);

	nPut = Swrite(out, (char *) &h, (size_t) 3, gNetworkTimeout);
	if (nPut <= 0) {
		Error(kDoPerror, "Error occurred during write!\n");
		xp->aborted = 1;
		nPut = -1;
		goto done;
	}
	if (nRead == 0) {
		/* Already sent EOF block, which was just the header. */
		xp->atEof = 1;
		return (1);	/* Probably should not return zero. */
	}
	nPut = Swrite(out, (char *) wbuf, (size_t) nRead, gNetworkTimeout);
	if (nPut <= 0) {
		Error(kDoPerror, "Error occurred during write!\n");
		xp->aborted = 1;
		nPut = -1;
		goto done;
	}

	xp->bytesTransferred += nRead;
	if (xp->doReports)
		ProgressReport(xp, kOptionalUpdate);

      done:
	return (nPut);
}						       /* BlockModeWrite */

/* eof */
