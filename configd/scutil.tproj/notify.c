/*
 * Copyright (c) 2000 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.1 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 *
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 *
 * @APPLE_LICENSE_HEADER_END@
 */

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include "scutil.h"

static int		osig;
static struct sigaction	*oact = NULL;

void
do_notify_list(int argc, char **argv)
{
	int		regexOptions = 0;
	SCDStatus	status;
	CFArrayRef	list;
	CFIndex		listCnt;
	int		i;

	if (argc == 1)
		regexOptions = kSCDRegexKey;

	status = SCDNotifierList(session, regexOptions, &list);
	if (status != SCD_OK) {
		printf("SCDNotifierList: %s\n", SCDError(status));
		return;
	}

	listCnt = CFArrayGetCount(list);
	if (listCnt > 0) {
		for (i=0; i<listCnt; i++) {
			SCDLog(LOG_NOTICE, CFSTR("  notifierKey [%d] = %@"), i, CFArrayGetValueAtIndex(list, i));
		}
	} else {
		SCDLog(LOG_NOTICE, CFSTR("  no notifierKey's"));
	}
	CFRelease(list);

	return;
}


void
do_notify_add(int argc, char **argv)
{
	CFStringRef	key;
	int		regexOptions = 0;
	SCDStatus	status;

	key = CFStringCreateWithCString(NULL, argv[0], kCFStringEncodingMacRoman);

	if (argc == 2)
		regexOptions = kSCDRegexKey;

	status = SCDNotifierAdd(session, key, regexOptions);
	CFRelease(key);
	if (status != SCD_OK) {
		printf("SCDNotifierAdd: %s\n", SCDError(status));
	}
	return;
}


void
do_notify_remove(int argc, char **argv)
{
	SCDStatus	status;
	CFStringRef	key;
	int		regexOptions = 0;

	key   = CFStringCreateWithCString(NULL, argv[0], kCFStringEncodingMacRoman);

	if (argc == 2)
		regexOptions = kSCDRegexKey;

	status = SCDNotifierRemove(session, key, regexOptions);
	CFRelease(key);
	if (status != SCD_OK) {
		printf("SCDNotifierRemove: %s\n", SCDError(status));
	}
	return;
}


void
do_notify_changes(int argc, char **argv)
{
	CFArrayRef	list;
	CFIndex		listCnt;
	SCDStatus	status;
	int		i;

	status = SCDNotifierGetChanges(session, &list);
	if (status != SCD_OK) {
		printf("SCDNotifierGetChanges: %s\n", SCDError(status));
		return;
	}

	listCnt = CFArrayGetCount(list);
	if (listCnt > 0) {
		for (i=0; i<listCnt; i++) {
			SCDLog(LOG_NOTICE, CFSTR("  changedKey [%d] = %@"), i, CFArrayGetValueAtIndex(list, i));
		}
	} else {
		SCDLog(LOG_NOTICE, CFSTR("  no changedKey's"));
	}
	CFRelease(list);

	return;
}


void
do_notify_wait(int argc, char **argv)
{
	SCDStatus	status;

	status = SCDNotifierWait(session);
	if (status != SCD_OK) {
		printf("SCDNotifierWait: %s\n", SCDError(status));
		return;
	}

	printf("OK, something changed!\n");
	return;
}


static boolean_t
notificationWatcher(SCDSessionRef session, void *arg)
{
	printf("notification callback (session address = %p)\n", session);
	printf("  arg = %s\n", (char *)arg);
	return TRUE;
}


static boolean_t
notificationWatcherVerbose(SCDSessionRef session, void *arg)
{
	printf("notification callback (session address = %p)\n", session);
	printf("  arg = %s\n", (char *)arg);
	do_notify_changes(0, NULL);	/* report the keys which changed */
	return TRUE;
}


void
do_notify_callback(int argc, char **argv)
{
	SCDStatus		status;
	SCDCallbackRoutine_t	func  = notificationWatcher;

	if ((argc == 1) && (strcmp(argv[0], "verbose") == 0)) {
		func = notificationWatcherVerbose;
	}

	status = SCDNotifierInformViaCallback(session,
					      func,
					      "Changed detected by callback handler!");
	if (status != SCD_OK) {
		printf("SCDNotifierInformViaCallback: %s\n", SCDError(status));
		return;
	}

	return;
}


void
do_notify_file(int argc, char **argv)
{
	int32_t		reqID = 0;
	SCDStatus	status;
	int		fd;
	union {
		char	data[4];
		int32_t	gotID;
	} buf;
	char		*bufPtr;
	int		needed;

	if (argc == 1) {
		if ((sscanf(argv[0], "%d", &reqID) != 1)) {
			printf("invalid identifier\n");
			return;
		}
	}

	status = SCDNotifierInformViaFD(session, reqID, &fd);
	if (status != SCD_OK) {
		printf("SCDNotifierInformViaFD: %s\n", SCDError(status));
		return;
	}

	bzero(buf.data, sizeof(buf.data));
	bufPtr = &buf.data[0];
	needed = sizeof(buf.gotID);
	while (needed > 0) {
		int	got;

		got = read(fd, bufPtr, needed);
		if (got == -1) {
			/* if error detected */
			printf("read() failed: %s\n", strerror(errno));
			break;
		}

		if (got == 0) {
			/* if end of file detected */
			printf("read(): detected end of file\n");
			break;
		}

		printf("Received %d bytes\n", got);
		bufPtr += got;
		needed -= got;
	}

	if (needed != sizeof(buf.gotID)) {
		printf("  Received notification, identifier = %d\n", buf.gotID);
	}

	/* this utility only allows processes one notification per "n.file" request */
	(void)SCDNotifierCancel(session);

	(void) close(fd);	/* close my side of the file descriptor */

	return;
}


static char *signames[] = {
	""    , "HUP" , "INT"   , "QUIT", "ILL"  , "TRAP", "ABRT", "EMT" ,
	"FPE" , "KILL", "BUS"   , "SEGV", "SYS"  , "PIPE", "ALRM", "TERM",
	"URG" , "STOP", "TSTP"  , "CONT", "CHLD" , "TTIN", "TTOU", "IO"  ,
	"XCPU", "XFSZ", "VTALRM", "PROF", "WINCH", "INFO", "USR1",
	"USR2"
};


static void
signalCatcher(int signum)
{
	static int	n = 0;

	printf("Received SIG%s (#%d)\n", signames[signum], n++);
	return;
}


void
do_notify_signal(int argc, char **argv)
{
	int			sig;
	pid_t			pid;
	struct sigaction	nact;
	int			ret;
	SCDStatus		status;

	if (isdigit(*argv[0])) {
		if ((sscanf(argv[0], "%d", &sig) != 1) || (sig <= 0) || (sig >= NSIG)) {
			printf("signal must be in the range of 1 .. %d\n", NSIG-1);
			return;
		}
	} else {
		for (sig=1; sig<NSIG; sig++) {
			if (strcasecmp(argv[0], signames[sig]) == 0)
				break;
		}
		if (sig >= NSIG) {
			printf("Signal must be one of the following:");
			for (sig=1; sig<NSIG; sig++) {
				if ((sig % 10) == 1)
					printf("\n ");
				printf(" %-6s", signames[sig]);
			}
			printf("\n");
			return;
		}

	}

	if ((argc != 2) || (sscanf(argv[1], "%d", &pid) != 1)) {
		pid = getpid();
	}

	if (oact != NULL) {
		ret = sigaction(osig, oact, NULL);	/* restore original signal handler */
	} else {
		oact = malloc(sizeof(struct sigaction));
	}

	nact.sa_handler = signalCatcher;
	sigemptyset(&nact.sa_mask);
	nact.sa_flags = SA_RESTART;
	ret = sigaction(sig, &nact, oact);
	osig = sig;
	printf("signal handler started\n");

	status = SCDNotifierInformViaSignal(session, pid, sig);
	if (status != SCD_OK) {
		printf("SCDNotifierInformViaSignal: %s\n", SCDError(status));
		return;
	}

	return;
}


void
do_notify_cancel(int argc, char **argv)
{
	SCDStatus		status;
	int			ret;

	status = SCDNotifierCancel(session);
	if (status != SCD_OK) {
		printf("SCDNotifierCancel: %s\n", SCDError(status));
		return;
	}

	if (oact != NULL) {
		ret = sigaction(osig, oact, NULL);	/* restore original signal handler */
		free(oact);
		oact = NULL;
	}

	return;
}
