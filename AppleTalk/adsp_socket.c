/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * "Portions Copyright (c) 1999 Apple Computer, Inc.  All Rights
 * Reserved.  This file contains Original Code and/or Modifications of
 * Original Code as defined in and that are subject to the Apple Public
 * Source License Version 1.0 (the 'License').  You may not use this file
 * except in compliance with the License.  Please obtain a copy of the
 * License at http://www.apple.com/publicsource and read it before using
 * this file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License."
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
/*
 *	Copyright (c) 1994, 1998 Apple Computer, Inc. 
 *
 *	The information contained herein is subject to change without
 *	notice and  should not be  construed as a commitment by Apple
 *	Computer, Inc. Apple Computer, Inc. assumes no responsibility
 *	for any errors that may appear.
 *
 *	Confidential and Proprietary to Apple Computer, Inc.
 *
 *  Change Log:
 *      Created November 22, 1994 by Tuyen Nguyen
 *
 */

#include <unistd.h>
#include <string.h>
#include <sys/time.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/uio.h>

#include <netat/appletalk.h>
#include <netat/adsp.h>

#include "at_proto.h"

#define	SET_ERRNO(e) errno = e

static int at_sndcmd();
static int at_getcmd();
static int at_pollcmd();

/*
 * Name: ADSPaccept
 */
int
ADSPaccept(int fd, void *name, int *namelen)
{
	int newfd, tmp_errno;
	struct {
		struct at_inet remaddr;
		unsigned short cid;
		unsigned short filler;
		unsigned long  sendseq;
		unsigned long  attnsendseq;
		unsigned long  sendwindow;
	} accept;
	struct adspcmd cmd;

	if ((name != NULL) &&
			((namelen == NULL) || (*namelen < sizeof(at_inet_t))) ) {
		SET_ERRNO(EINVAL);
		return -1;
	}

l_again:
	if (read(fd, &cmd, sizeof(cmd)) < 0)
		return -1;

	accept.cid = cmd.u.openParams.remoteCID;
	accept.sendseq = cmd.u.openParams.sendSeq;
	accept.attnsendseq = cmd.u.openParams.attnSendSeq;
	accept.sendwindow = cmd.u.openParams.sendWindow;
	accept.remaddr = cmd.u.openParams.remoteAddress;

	if ((newfd = ADSPsocket(AF_APPLETALK, SOCK_STREAM, 0)) < 0) {
l_err:
		tmp_errno = errno;
		if (newfd >= 0)
			close(newfd);
		ADSPlisten(fd, 1);
		SET_ERRNO(tmp_errno);
		if (tmp_errno == 0)
			goto l_again;
		return -1;
	}

	bzero((char*)&cmd, sizeof(cmd));	
	cmd.csCode = dspOpen;
	cmd.u.openParams.remoteCID = accept.cid;
	cmd.u.openParams.sendSeq = accept.sendseq;
	cmd.u.openParams.attnSendSeq = accept.attnsendseq;
	cmd.u.openParams.sendWindow = accept.sendwindow;
	cmd.u.openParams.remoteAddress = accept.remaddr;
	cmd.u.openParams.ocMode = ocAccept;
	if (at_sndcmd(newfd, &cmd, ADSPOPEN) < 0)
		goto l_err;

	if (cmd.ioResult < 0)
		goto l_err;
	if (cmd.ioResult == 1) {
		if (at_getcmd(newfd, NULL) < 0)
			goto l_err;
	}

	if (name != NULL) {
		*((at_inet_t*)name) = accept.remaddr;
		*namelen = sizeof(at_inet_t);
	}

	ADSPlisten(fd, 1);

	return newfd;
}

/*
 * Name: ADSPbind
 */
int
ADSPbind(int fd, void *name, int namelen)
{
	int len;
	at_socket newsock;

	newsock = (name == NULL) ? 0 : ((at_inet_t *)name)->socket;
	len = sizeof(newsock);
	return at_send_to_dev(fd, ADSPBINDREQ, &newsock, &len);
}

/*
 * Name: ADSPclose
 */
int
ADSPclose(int fd)
{
	return close(fd);
}

/*
 * Name: ADSPconnect
 */
int
ADSPconnect(int fd, void *name, int namelen)
{
	struct adspcmd cmd;

	if ((name == NULL) || (namelen != sizeof(at_inet_t))) {
		SET_ERRNO(EINVAL);
		return -1;
	}

	bzero((char *)&cmd, sizeof(cmd));
	cmd.csCode = dspOpen;
	cmd.u.openParams.remoteAddress = *((at_inet_t*)name);
	cmd.u.openParams.ocMode = ocRequest;
	if (at_sndcmd(fd, &cmd, ADSPOPEN) < 0)
		return -1;

	if (cmd.ioResult < 0)
		return -1;
	if (cmd.ioResult == 1) {
		if (at_getcmd(fd, &cmd) < 0) {
			if (cmd.ioResult)
				SET_ERRNO(ETIMEDOUT);
			return -1;
		}
	}

	return 0;
}

/*
 * Name: ADSPdisconnect
 */
int
ADSPdisconnect(int fd, int abort)
{
	struct adspcmd cmd;

	bzero((char*)&cmd, sizeof(cmd));
	cmd.csCode = dspClose;
	cmd.u.closeParams.abort = abort;
	if (at_sndcmd(fd, &cmd, ADSPCLOSE) < 0)
		return -1;

	if (cmd.ioResult < 0)
		return -1;
	if (cmd.ioResult == 1) {
		if (at_getcmd(fd) < 0)
			return -1;
	}

	return 0;
}

/*
 * Name: ADSPfwdreset
 */
int
ADSPfwdreset(int fd)
{
	struct adspcmd cmd;

	bzero((char*)&cmd, sizeof(cmd));
	cmd.csCode = dspReset;
	if (at_sndcmd(fd, &cmd, ADSPRESET) < 0)
		return -1;

	if (cmd.ioResult < 0)
		return -1;
	if (cmd.ioResult == 1) {
		if (at_getcmd(fd) < 0)
			return -1;
	}

	return 0;
}

/*
 * Name: ADSPgetpeername
 */
int
ADSPgetpeername(int fd, void *name, int *namelen)
{
	int len;

	if ((name == NULL) || (namelen == NULL) || (*namelen < sizeof(at_inet_t))) {
		SET_ERRNO(EINVAL);
		return -1;
	}

	len = 0;
	if (at_send_to_dev(fd, ADSPGETPEER, name, &len) == -1)
		return -1;

	*namelen = len;
	return 0;
}

/*
 * Name: ADSPgetsockname
 */
int
ADSPgetsockname(int fd, void *name, int *namelen)
{
	int len;

	if ((name == NULL) || (namelen == NULL) || (*namelen < sizeof(at_inet_t))) {
		SET_ERRNO(EINVAL);
		return -1;
	}

	len = 0;
	if (at_send_to_dev(fd, ADSPGETSOCK, name, &len) == -1)
		return -1;

	*namelen = len;
	return 0;
}

/*
 * Name: ADSPgetsockopt
 */
int
ADSPgetsockopt(int fd, int level, int optname, char *optval, int *optlen)
{
	SET_ERRNO(EOPNOTSUPP);
	return -1;
}

/*
 * Name: ADSPlisten
 */
int
ADSPlisten(int fd, int backlog)
{
	struct adspcmd cmd;

	bzero((char*)&cmd, sizeof(cmd));
	cmd.csCode = dspCLListen;
	cmd.socket = (at_socket)backlog;	
	if (at_sndcmd(fd, &cmd, ADSPCLLISTEN) < 0)
		return -1;

	if (cmd.ioResult < 0)
		return -1;

	return 0;
}

/*
 * Name: ADSPrecv
 */
int
ADSPrecv(int fd, char *buf, int len, int flags)
{
	return ADSPrecvfrom(fd, buf, len, flags, NULL, 0);
}

/*
 * Name: ADSPrecvfrom
 */
int
ADSPrecvfrom(int fd, char *buf, int len, int flags, void *from, int fromlen)
{
	int msgflags;
	strbuf_t msgdata;
	struct timeval polltime;
	struct adspcmd cmd;

	if (len == 0)
		return 0;
	if (buf == NULL) {
		SET_ERRNO(EINVAL);
		return -1;
	}

	bzero((char *)&cmd, sizeof(cmd));

	if (flags & 0x01) { /* read attention data? */
		cmd.csCode = dspAttention;
		cmd.u.attnParams.attnSize = len-2;
		cmd.u.attnParams.attnData = (unsigned char *)0;
		if (at_sndcmd(fd, &cmd, ADSPATTENTION) < 0)
			return -1;
	} else {
		cmd.csCode = dspRead;
		cmd.u.ioParams.reqCount = len;
		cmd.u.ioParams.dataPtr = (unsigned char *)0;
		if (at_sndcmd(fd, &cmd, ADSPREAD) < 0)
			return -1;
	}
	if (cmd.ioResult < 0)
		return -1;

	if (cmd.ioResult == 1) {
		if (flags & 01) {
			polltime.tv_sec = 0;
			polltime.tv_usec = 0;
			if (at_pollcmd(fd, &polltime) == 0)
				return 0;
			SET_ERRNO(EPROTOTYPE);
			return -1;
		}
		if (at_getcmd(fd, &cmd) < 0)
			return -1;
		else if ((cmd.csCode != dspRead) && (cmd.csCode != dspAttention)) {
			SET_ERRNO(EPROTOTYPE);
			return -1;
		}
	}

	if (flags & 01) {
		len = cmd.u.attnParams.attnSize;
		buf[0] = (char)(cmd.u.attnParams.attnCode >> 8);
		buf[1] = (char)(cmd.u.attnParams.attnCode);
		if (len > 0) {
			msgdata.maxlen = len;
			msgdata.len = 0;
			msgdata.buf = &buf[2];
			msgflags = 0;
			if (ATgetmsg(fd, 0, &msgdata, &msgflags) < 0)
				len = -1;
			else
				len = msgdata.len + 2;
		} else
			len = 2;
	} else {
		len = cmd.u.ioParams.actCount;
		if (len > 0)
			len = read(fd, buf, len);
	}

	if (from && (fromlen >= sizeof(at_inet_t)))
		ADSPgetpeername(fd, from, &fromlen);

	return len;
}

/*
 * Name: ADSPsendto
 */
#define MAX_PKTSIZE 0x1000
int
ADSPsendto(int fd, char *buf, int len, int flags, void *to, int tolen)
{
	struct adspcmd cmd;
	int size, snd_len;
	struct iovec iov[2];

	if (len == 0)
		return 0;
	if (buf == NULL) {
		SET_ERRNO(EINVAL);
		return -1;
	}

	for (snd_len=0; snd_len < len; snd_len += size) {
	  bzero((char *)&cmd, sizeof(cmd));

	  if (flags & 0x01) { /* write attention data? */
		if ((len < 2) || (len > ADSP_MAX_DATA_LEN)) {
			SET_ERRNO(ERANGE);
			return -1;
		}
		size = len;
		cmd.csCode = dspAttention;
		cmd.u.attnParams.attnCode = (((unsigned short)buf[0])<<8)
			+ (unsigned short)buf[1];
		cmd.u.attnParams.attnSize = size-2;
		cmd.u.attnParams.attnData = (unsigned char *)0;
		iov[1].iov_base = (caddr_t)&buf[2];
		iov[1].iov_len  = (int)size-2;
	  } else {
		size = ((len-snd_len) < MAX_PKTSIZE) ? (len-snd_len) : MAX_PKTSIZE;
		cmd.csCode = dspWrite;
		cmd.u.ioParams.reqCount = size;
		cmd.u.ioParams.dataPtr = (unsigned char *)0;
		cmd.u.ioParams.flush = 1;
		cmd.u.ioParams.eom = (size == (len-snd_len)) ? 1 : 0;
		if (cmd.u.ioParams.eom && (flags & 0x10000000))
			cmd.u.ioParams.eom = 0; /* caller requests for NO eom */
		iov[1].iov_base = (caddr_t)&buf[snd_len];
		iov[1].iov_len  = (int)size;
	  }
	  iov[0].iov_base = (caddr_t)&cmd;
	  iov[0].iov_len  = (int)sizeof(cmd);

	  while (writev(fd, iov, 2) == -1) {
		if (errno != EINTR)
			return snd_len ? snd_len : -1;
	  }
	}

	return len;
} /* ADSPsendto */

/*
 * Name: ADSPsend
 */
int
ADSPsend(int fd, char *buf, int len, int flags)
{
	return ADSPsendto(fd, buf, len, flags, NULL, 0);
}

/*
 * Name: ADSPsetsockopt
 */
int
ADSPsetsockopt(int fd, int level, int optname, char *optval, int optlen)
{
	struct adspcmd cmd;

	if ((optval == NULL) || (optlen != sizeof(struct TRoptionParams))) {
		SET_ERRNO(EINVAL);
		return -1;
	}

	bzero((char*)&cmd, sizeof(cmd));
	cmd.csCode = dspOptions;
	cmd.u.optionParams = *(struct TRoptionParams *)optval;
	if (at_sndcmd(fd, &cmd, ADSPOPTIONS) < 0)
		return -1;

	return 0;
}

/*
 * Name: ADSPsocket
 */
int
ADSPsocket(int domain, int type, int protocol)
{
	int fd, namelen, tmp_errno;
	at_inet_t name;

	if ((domain != PF_APPLETALK) || (type != SOCK_STREAM) || (protocol != 0)) {
		SET_ERRNO(EINVAL);
		return -1;
	}

	fd = ATsocket(ATPROTO_ADSP);
	if (fd < 0)
		return -1;

	namelen = sizeof(name);
	if (ADSPgetsockname(fd, &name, &namelen) < 0) {
		tmp_errno = errno;
		close(fd);
		SET_ERRNO(tmp_errno);
		return -1;
	}

	return fd;
}

/*
 * Name: at_sndcmd
 */
static int
at_sndcmd(int fd, struct adspcmd *cmd, int _cmd)
{
	int rc, len;
	
	len = sizeof(struct adspcmd);
	while (((rc = at_send_to_dev(fd,
		_cmd, cmd, &len)) < 0) && (errno == EINTR))
		;
	return rc;
}

/*
 * Name: at_getcmd
 */
static int
at_getcmd(int fd, struct adspcmd *cmd)
{
	struct adspcmd cmdbuf;

	if (cmd == NULL)
		cmd = (struct adspcmd *)&cmdbuf;

	while ((read(fd, cmd, sizeof(*cmd)) != sizeof(*cmd)) || cmd->ioResult) {
		if (errno != EINTR)
			return -1;
	}

	return 0;
}

/*
 * Name: at_pollcmd
 */
static int
at_pollcmd(int fd, struct timeval *polltime)
{
	int rc;
	fd_set readset;

	FD_ZERO(&readset);
	FD_SET(fd, &readset);
	while ((rc = select(FD_SETSIZE, &readset, NULL, NULL, polltime)) <= 0) {
		if (errno != EINTR)
			return rc;
	}

	return 1;
}

/*
 * Name: ASYNCread
 */
int
ASYNCread(int fd, char *buf, int len)
{
	struct adspcmd cmd;

	if (len == 0)
		return 0;
	if (buf == NULL) {
		SET_ERRNO(EINVAL);
		return -1;
	}

	bzero((char *)&cmd, sizeof(cmd));
	cmd.csCode = dspRead;
	cmd.u.ioParams.reqCount = len;
	cmd.u.ioParams.dataPtr = (unsigned char *)0;
	if (at_sndcmd(fd, &cmd, ADSPREAD) < 0)
		return -1;
	if (cmd.ioResult < 0)
		return -1;
	if (cmd.ioResult == 1)
		len = 0;
	else {
		len = cmd.u.ioParams.actCount;
		if (len > 0)
			len = read(fd, buf, len);
	}

	return len;
}

/*
 * Name: ASYNCread_complete
 */
int
ASYNCread_complete(int fd, char *buf, int len)
{
	struct adspcmd cmd;

	if (len == 0)
		return 0;
	if (buf == NULL) {
		SET_ERRNO(EINVAL);
		return -1;
	}

	if (at_getcmd(fd, &cmd) < 0)
		return -1;
	else if (cmd.csCode != dspRead) {
		SET_ERRNO(EPROTOTYPE);
		return -1;
	}
	len = cmd.u.ioParams.actCount;
	if (len > 0)
		len = read(fd, buf, len);

	return len;
}
