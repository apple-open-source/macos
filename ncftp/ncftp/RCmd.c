/* RCmd.c */

#include "Sys.h"

#include <ctype.h>
#include <setjmp.h>
#include <arpa/telnet.h>
#include "Util.h"
#include "RCmd.h"

#include <signal.h>
#include <setjmp.h>

#include "Open.h"
#include "Main.h"
#include "Xfer.h"
#include "FTP.h"

/* NOTE: There is more commentary at the bottom of this source file. */

/* User-settable verbosity level.
 * This isn't really used at the moment.
 */
int gVerbosity = kTerse;

int gRemoteMsgs = kAllRmtMsgs;

int gNetworkTimeout = kDefaultNetworkTimeout;

jmp_buf	gTStrTimeOut;

extern FILE *gControlIn, *gControlOut;
extern int gDataSocket, gDoneApplication;
extern int gDebug, gConnected, gTrace;
extern jmp_buf gCmdLoopJmp;
extern int gPreferredDataPortMode, gReadingStartup;
extern int gAttemptingConnection, gMode;

/* The user can control the level of output printed by the program
 * with this.
 */
int SetVerbose(int newVerbose)
{
	int old;
	
	old = gVerbosity;
	gVerbosity = newVerbose;
	return (old);
}	/* SetVerbose */




/* A 'Response' parameter block is simply zeroed to be considered init'ed. */
ResponsePtr InitResponse(void)
{
	ResponsePtr rp;
	
	rp = (ResponsePtr) calloc(SZ(1), sizeof(Response));
	if (rp == NULL)
		OutOfMemory();	/* Won't return. */
	InitLineList(&rp->msg);
	return (rp);
}	/* InitResponse */




/* If we don't print it to the screen, we may want to save it to our
 * trace log.
 */
void TraceResponse(ResponsePtr rp)
{
	LinePtr lp;
	
	if (rp != NULL)	{
		lp = rp->msg.first;
		if (lp != NULL) {
			TraceMsg("%3d: %s\n", rp->code, lp->line);
			for (lp = lp->next; lp != NULL; lp = lp->next)
				TraceMsg("     %s\n", lp->line);
		}
	}
}	/* TraceResponse */




/* We print a response, which may be several lines of text. 
 * For old time's sake, we will also print the numeric code preceding
 * the first line, since the old ftp client used to always print the
 * response codes too.
 */
void PrintResponse(ResponsePtr rp)
{
	LinePtr lp;
	longstring buf2;

	MultiLineInit();
	if (rp != NULL)	{
		if (gDebug == kDebuggingOff) {
			for (lp = rp->msg.first; lp != NULL; lp = lp->next) {
				MakeStringPrintable(buf2, (unsigned char *) lp->line, sizeof(buf2));
				MultiLinePrintF("%s\n", buf2);
			}
		} else {
			lp = rp->msg.first;
			if (lp != NULL) {
				MakeStringPrintable(buf2, (unsigned char *) lp->line, sizeof(buf2));
				MultiLinePrintF("%03d: %s\n", rp->code, buf2);
				for (lp = lp->next; lp != NULL; lp = lp->next) {
					MakeStringPrintable(buf2, (unsigned char *) lp->line, sizeof(buf2));
					MultiLinePrintF("     %s\n", buf2);
				}
			}
		}
	}
}	/* PrintResponse */




/* This determines if a response should be printed to the screen, given
 * the current verbosity level, the response code level, or the parameters
 * in the Response block.
 */
void PrintResponseIfNeeded(ResponsePtr rp)
{
	int printIt = 0;
	LinePtr lp;

	if ((gDebug == kDebuggingOn) || (gVerbosity == kVerbose)) {
		/* Always print if debugging or in verbose mode. */
		printIt = 1;
	} else if (rp->printMode == kDontPrint) {
		/* Never print if we were explicitly told not to. */
		printIt = 0;
	} else if (rp->printMode == kDoPrint) {
		/* Always print if we were explicitly told to. */
		printIt = 1;
	} else if (gVerbosity == kErrorsOnly) {
		/* Most of the time we want to print error responses. */
		if (rp->codeType == 5)
			printIt = 1;
	} else if (gVerbosity == kTerse) {
		/* Most of the time we want to print error responses. */
		if (rp->codeType >= 4) {
			printIt = 1;
		} else {
			switch (rp->code) {
				case 250:
					/* Print 250 lines if they aren't
					 * "250 CWD command successful."
					 * or "250 "/a/b/c" is new cwd."
					 */
					if ((gRemoteMsgs & kNoChdirMsgs) == 0) {
						printIt = 1;
						for (lp = rp->msg.first; lp != NULL; ) {
							if (STRNEQ(lp->line, "CWD command", 11) || (STREQ(lp->line + strlen(lp->line) - 4, "cwd."))) {
								lp = RemoveLine(&rp->msg, lp);
								break;
							} else {
								lp = lp->next;
							}
						}
					}
					break;
				case 220:
					/* But skip the foo FTP server ready line. */
					if ((gRemoteMsgs & kNoConnectMsg) == 0) {
						printIt = 1;
						for (lp = rp->msg.first; lp != NULL; ) {
							if (strstr(lp->line, "ready.") != NULL) {
								lp = RemoveLine(&rp->msg, lp);
								break;
							} else {
								lp = lp->next;
							}
						}
					}
					break;

				case 230:	/* User logged in, proceed. */
					if ((gRemoteMsgs & kNoConnectMsg) == 0) {
						/* I'll count 230 as a connect message. */
						printIt = 1;
					}
					break;

				case 214:	/* Help message. */
				case 331:	/* Enter password. */
					printIt = 1;
					break;
			}
		}
	}
	
	if (printIt)
		PrintResponse(rp);
	else if (gTrace == kTracingOn)
		TraceResponse(rp);
}	/* PrintResponseIfNeeded */




void DoneWithResponse(ResponsePtr rp)
{
	/* Dispose space taken up by the Response, and clear it out
	 * again.  For some reason, I like to return memory to zeroed
	 * when not in use.
	 */
	if (rp != NULL) {
		PrintResponseIfNeeded(rp);
		DisposeLineListContents(&rp->msg);
		CLEARRESPONSE(rp);
		free(rp);
	}
}	/* DoneWithResponse */




/* This takes an existing Response and recycles it, by clearing out
 * the current contents.
 */
void ReInitResponse(ResponsePtr rp)
{
	PrintResponseIfNeeded(rp);
	DisposeLineListContents(&rp->msg);
	CLEARRESPONSE(rp);
}	/* ReInitResponse */



static
void TimeoutGetTelnetString(void)
{
	alarm(0);
	longjmp(gTStrTimeOut, 1);
}	/* TimeoutGetTelnetString */



/* Since the control stream is defined by the Telnet protocol (RFC 854),
 * we follow Telnet rules when reading the control stream.  We use this
 * routine when we want to read a response from the host.
 */
int GetTelnetString(char *str, size_t siz0, FILE *cin, FILE *cout)
{
	int c;
	size_t n;
	int eofError;
	char *volatile cp;
	volatile size_t siz;

	cp = str;
	siz = siz0 - 1;	/* We'll need room for the \0. */
	if ((cin == NULL) || (cout == NULL)) {
		eofError = -1;
		goto done;
	}

	if (setjmp(gTStrTimeOut) != 0) {
		Error(kDontPerror, "Host is not responding to commands, hanging up.\n");
		if (gAttemptingConnection == 0) {
			HangupOnServer();
			if (!gDoneApplication) {
				alarm(0);
				longjmp(gCmdLoopJmp, 1);
			}
		} else {
			/* Give up and return to Open(). */
			DoClose(0);
		}
		eofError = -1;
		goto done;
	}
	SIGNAL(SIGALRM, TimeoutGetTelnetString);
	alarm(gNetworkTimeout);

	for (n = (size_t)0, eofError = 0; ; ) {
		c = fgetc(cin);
checkChar:
		if (c == EOF) {
eof:
			eofError = -1;
			break;
		} else if (c == '\r') {
			/* A telnet string can have a CR by itself.  But to denote that,
			 * the protocol uses \r\0;  an end of line is denoted \r\n.
			 */
			c = fgetc(cin);
			if (c == '\n') {
				/* Had \r\n, so done. */
				goto done;
			} else if (c == EOF) {
				goto eof;
			} else if (c == '\0') {
				c = '\r';
				goto addChar;
			} else {
				/* Telnet protocol violation! */
				goto checkChar;
			}
		} else if (c == '\n') {
			/* Really shouldn't get here.  If we do, the other side
			 * violated the TELNET protocol, since eoln's are CR/LF,
			 * and not just LF.
			 */
			DebugMsg("TELNET protocol violation:  raw LF.\n");
			goto done;
		} else if (c == IAC) {
			/* Since the control connection uses the TELNET protocol,
			 * we have to handle some of its commands ourselves.
			 * IAC is the protocol's escape character, meaning that
			 * the next character after the IAC (Interpret as Command)
			 * character is a telnet command.  But, if there just
			 * happened to be a character in the text stream with the
			 * same numerical value of IAC, 255, the sender denotes
			 * that by having an IAC followed by another IAC.
			 */
			
			/* Get the telnet command. */
			c = fgetc(cin);
			
			switch (c) {
				case WILL:
				case WONT:
					/* Get the option code. */
					c = fgetc(cin);
					
					/* Tell the other side that we don't want
					 * to do what they're offering to do.
					 */
					(void) fprintf(cout, "%c%c%c",IAC,DONT,c);
					(void) fflush(cout);
					break;
				case DO:
				case DONT:
					/* Get the option code. */
					c = fgetc(cin);
					
					/* The other side said they are DOing (or not)
					 * something, which would happen if our side
					 * asked them to.  Since we didn't do that,
					 * ask them to not do this option.
					 */
					(void) fprintf(cout, "%c%c%c",IAC,WONT,c);
					(void) fflush(cout);
					break;

				case EOF:
					goto eof;

				default:
					/* Just add this character, since it was most likely
					 * just an escaped IAC character.
					 */
					goto addChar;
			}
		} else {
addChar:
			/* If the buffer supplied has room, add this character to it. */
			if (n < siz) {
				*cp++ = c;				
				++n;
			}
		}
	}

done:
	*cp = '\0';
	alarm(0);
	return (eofError);
}	/* GetTelnetString */



/* Returns the code class of the command, or 5 if an error occurs, which
 * coincides with the error class anyway.  This reads the entire response
 * text into a LineList, which is kept in the 'Response' structure.
 */
int GetResponse(ResponsePtr rp)
{
	string str;
	int eofError;
	str16 code;
	char *cp;
	int continuation;
	int usedTmpRp;
	int codeType;

	/* You can tell us to do the default action on the response,
	 * or tell us to ignore the response if the caller doesn't want
	 * to handle it.
	 */
	usedTmpRp = 1;
	if (rp == kDefaultResponse) {
		rp = InitResponse();
	} else if (rp == kIgnoreResponse) {
		rp = InitResponse();
		rp->printMode = kDontPrint;
	} else
		usedTmpRp = 0;

	/* RFC 959 states that a reply may span multiple lines.  A single
	 * line message would have the 3-digit code <space> then the msg.
	 * A multi-line message would have the code <dash> and the first
	 * line of the msg, then additional lines, until the last line,
	 * which has the code <space> and last line of the msg.
	 *
	 * For example:
	 *	123-First line
	 *	Second line
	 *	234 A line beginning with numbers
	 *	123 The last line
	 */

	/* Get the first line of the response. */
	eofError = GetTelnetString(str, sizeof(str), gControlIn, gControlOut);
	if (eofError < 0)
		goto eof;

	cp = str;
	if (!isdigit(*cp)) {
		Error(kDontPerror, "Invalid reply: \"%s\"\n", cp);
		return (5);
	}

	codeType = rp->codeType = *cp - '0';
	cp += 3;
	continuation = (*cp == '-');
	*cp++ = '\0';
	STRNCPY(code, str);
	rp->code = atoi(code);
	AddLine(&rp->msg, cp);
	
	while (continuation) {
		eofError = GetTelnetString(str, sizeof(str), gControlIn, gControlOut);
		if (eofError < 0) {
			/* Most of the time, we don't want EOFs from the other side. */
eof:
			if (*str)
				AddLine(&rp->msg, str);
eofMsg:
			if (rp->eofOkay == 0)
				Error(kDontPerror, "Remote host has closed the connection.\n");
			rp->hadEof = 1;
			if (gAttemptingConnection == 0) {
				HangupOnServer();
				if (!gDoneApplication) {
					alarm(0);
					longjmp(gCmdLoopJmp, 1);
				}
			} else if (rp->eofOkay == 0) {
				/* Give up and return to Open(). */
				DoClose(0);
			}	/* else rp->eofOkay, which meant we already closed. */
			return (5);
		}
		cp = str;
		if (strncmp(code, cp, SZ(3)) == 0) {
			cp += 3;
			if (*cp == ' ')
				continuation = 0;
			++cp;
		}
		AddLine(&rp->msg, cp);
	}

	if (rp->code == 421) {
		/*
	     *   421 Service not available, closing control connection.
	     *       This may be a reply to any command if the service knows it
	     *       must shut down.
		 */
		goto eofMsg;
	}

	/* From above, if the caller didn't want to handle it, we kept our
	 * own copy and now it's time to dispose of it.  Depending on
	 * whether we're ignoring it altogether or doing the default
	 * action, we may or may not print it before deallocating it.
	 */
	if (usedTmpRp)
		DoneWithResponse(rp);

	return (codeType);
}	/* GetResponse */




/* This creates the complete command text to send, and writes it
 * on the stream.
 */
static
void SendCommand(char *cmdspec, va_list ap)
{
	longstring command;
	int result;

	(void) vsprintf(command, cmdspec, ap);
	if (strncmp(command, "PASS", SZ(4)))
		DebugMsg("RCmd:  \"%s\"\n", command);
	else
		DebugMsg("RCmd:  \"%s\"\n", "PASS xxxxxxxx");
	STRNCAT(command, "\r\n");	/* Use TELNET end-of-line. */
	if (gControlOut != NULL) {
		result = fputs(command, gControlOut);
		if (result < 0)
			Error(kDoPerror, "Could not write to control stream.\n");
		(void) fflush(gControlOut);
	}
}	/* SendCommand */




/* For "simple" (i.e. not data transfer) commands, this routine is used
 * to send the command and receive one response.  It returns the codeClass
 * field of the 'Response' as the result.
 */

/*VARARGS*/
#ifndef HAVE_STDARG_H
int RCmd(va_alist)
        va_dcl
#else
int RCmd(ResponsePtr rp0, char *cmdspec0, ...)
#endif
{
	va_list ap;
	char *cmdspec;
	ResponsePtr rp;
	int result;

#ifndef HAVE_STDARG_H
	va_start(ap);
	rp = va_arg(ap, ResponsePtr);
	cmdspec = va_arg(ap, char *);
#else
	va_start(ap, cmdspec0);
	cmdspec = cmdspec0;
	rp = rp0;
#endif
	if (gControlOut == NULL) {
		va_end(ap);
		return (-1);
	}

	SendCommand(cmdspec, ap);
	va_end(ap);

	/* Get the response to the command we sent. */
	result = GetResponse(rp);

	return (result);
}	/* RCmd */



/* Returns -1 if an error occurred, or 0 if not.
 * This differs from RCmd, which returns the code class of a response.
 */

/*VARARGS*/
#ifndef HAVE_STDARG_H
int RDataCmd(va_alist)
        va_dcl
#else
int RDataCmd(XferSpecPtr xp0, char *cmdspec0, ...)
#endif
{
	va_list ap;
	char *cmdspec;
	XferSpecPtr xp;
	int result, didXfer;
	int respCode;
	int ioErrs;

#ifndef HAVE_STDARG_H
	va_start(ap);
	xp = va_arg(ap, XferSpecPtr);
	cmdspec = va_arg(ap, char *);
#else
	va_start(ap, cmdspec0);
	cmdspec = cmdspec0;
	xp = xp0;
#endif

	if (gControlOut == NULL) {
		va_end(ap);
		return (-1);
	}

	/* To transfer data, we do these things in order as specifed by
	 * the RFC.
	 * 
	 * First, we tell the other side to set up a data line.  This
	 * is done below by calling OpenDataConnection(), which sets up
	 * the socket.  When we do that, the other side detects a connection
	 * attempt, so it knows we're there.  Then tell the other side
	 * (by using listen()) that we're willing to receive a connection
	 * going to our side.
	 */
	didXfer = 0;

	CloseDataConnection(0);
	if ((result = OpenDataConnection(gPreferredDataPortMode)) < 0)
		goto done;

    /* If asked, attempt to start at a later position in the remote file. */
	if (xp->startPoint != SZ(0)) {
		if (SetStartOffset(xp->startPoint) < 0) {
			xp->startPoint = SZ(0);
			TruncReOpenReceiveFile(xp);
		}
	}

	/* Now we tell the server what we want to do.  This sends the
	 * the type of transfer we want (RETR, STOR, LIST, etc) and the
	 * parameters for that (files to send, directories to list, etc).
	 */
	SendCommand(cmdspec, ap);

	/* Get the response to the transfer command we sent, to see if
	 * they can accomodate the request.  If everything went okay,
	 * we will get a preliminary response saying that the transfer
	 * initiation was successful and that the data is there for
	 * reading (for retrieves;  for sends, they will be waiting for
	 * us to send them something).
	 */
	respCode = GetResponse(xp->cmdResp);
	DoneWithResponse(xp->cmdResp);
	xp->cmdResp = NULL;

	if (respCode > 2) {
		result = -1;
		goto done;
	}

	/* Now we accept the data connection that the other side is offering
	 * to us.  Then we can do the actual I/O on the data we want.
	 */
	if ((result = AcceptDataConnection()) < 0)
		goto done;

	if (NETREADING(xp)) {
		xp->inStream = gDataSocket;
	} else {
		xp->outStream = gDataSocket;
	}
	StartTransfer(xp);
	ioErrs = (*xp->xProc)(xp);

	didXfer = 1;
	result = ioErrs ? -1 : 0;

done:
	CloseDataConnection(0);
	if (didXfer) {
		/* Get the response to the data transferred.  Most likely a message
		 * saying that the transfer completed succesfully.  However, if
		 * we tried to abort the transfer using ABOR, we will have a response
		 * to that command instead.
		 */
		EndTransfer(xp);
		respCode = GetResponse(xp->xferResp);
		if (respCode != 2)
			result = -1;
	}
	va_end(ap);
	return (result);
}	/* RDataCmd */



/*****************************************************************************

How command responses are handled.
----------------------------------

Older versions of the program were built upon the BSD ftp client.  It's method
was just to send commands, and at arbitrary points, read and print the
response.

I want to keep responses together, and have them read automatically, but have
the option of not printing them, printing them some other time, or just
getting a response and parsing the results.  The BSD client needed to do that
a lot, but it used a verbosity variable, and when it wanted to do that, would
set the verbosity to some "quiet" value.  That turned out to be quite a mess
of setting, saving, and restoring the verbosity level, not to mention being
unflexible.  There is a verbosity variable, but it has a much-reduced role
than it did before.

I have a structure, the 'Response' structure, defined in the RCmd.h header.
Besides keeping the entire response text, it keeps with it the code class,
the actual response code, and some other stuff.

All commands are sent to a central function, which require a pointer to an
initialized Response as an argument.  After the command is sent, the Response
is filled in. The response text may be printed automatically if an error
occurred, and if the user had that setting on.  Otherwise, it is the caller's
responsibility to print it if it likes, and also to dispose of the structure
when finished.  Much of the time, we don't even need to print anything, to
keep the gory details from the user anyway.

Typically, if a calling function needs to examine the response after the
command completes, the caller will have something like this:
	ResponsePtr resp;
	resp = InitResponse();
	RCmd(...);
	[do something with the response]
	DoneWithResponse(resp);

Other times we may want to just do a command but don't care about the
result unless it was an error.  In that case we don't have to setup a
Response block.  We can pass a special parameter to RCmd, and maybe
take action depending if the command failed:
	if (RCmd(kDefaultResponse, ...) != 2) { error... }

Another thing we may want is to just do a command, and not even care
if it succeeded or not:
	(void) RCmd(kIgnoreResponse, ...);
	
*****************************************************************************/

/* eof */
