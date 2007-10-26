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
 *	Copyright (c) 1995, 1998 Apple Computer, Inc. 
 *
 *	The information contained herein is subject to change without
 *	notice and  should not be  construed as a commitment by Apple
 *	Computer, Inc. Apple Computer, Inc. assumes no responsibility
 *	for any errors that may appear.
 *
 *	Confidential and Proprietary to Apple Computer, Inc.
 *
 *  Change Log:
 *      Created February 20, 1995 by Tuyen Nguyen
 *      Modified for FutureShare, April 15, 1996 by Tuyen Nguyen
 */

#include <stdlib.h>
#include <unistd.h>

#include <sys/time.h>
#include <sys/errno.h>
#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/times.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>

#include <netat/appletalk.h>
#include <netat/sysglue.h>
#include <netat/asp.h>
#include <netat/atp.h>

#include <AppleTalk/at_proto.h>



extern int ATsocket(int protocol);
	/* Used to create an old-style (pre-BSD) AppleTalk socket */

#define	SET_ERRNO(e) errno = e

/*#define MSGSTR(num,str)		catgets(catd, MS_ASP, num,str)*/
#define MSGSTR(num,str)         str

#define DEFAULT_AFP_PORT 548
#define ASP_SIG_STR "M10"

#define ASPDEF_BufSize     sizeof(ioc_t)
#define ASPDEF_MaxCmdSize  578
#define ASPDEF_QuantumSize 4624
#define ASPDEF_DefMaxDescr FD_SETSIZE
#define ASPDEF_MaxNumOfSLS 8			  /* for Rhapsody we currently need 1 */
#define ASPDT_ATP_AS       1
#define ASPDT_TCP_FS       2
#define ASPFS_AFPCfgName   "/etc/afpovertcp.cfg"
#define ASPFS_Reply        0x01
#define ASPFS_LastPacket   0x02
#define ASPFS_AFPWrCmdSize 12
#define ASPFS_SigSuppBit   0x10
#define ASPFS_TCPSuppBit   0x20
#define ASPFS_SigDataLen      16
#define ASPFS_DataQuantumTag  0
#define ASPFS_AttnQuantumTag  1
#define ASPFS_MaxOptLen       128
#define ASP_LINE_LEN	      128
#define ASP_SESS_TIMEOUT 120
#define ASP_CONN_TIMEOUT 20
#define ASP_TICKLE_INT 30

#ifdef ASPDEBUG
static FILE *logfp = 0;
static char *newline = "\n";
#define LOG_MSG(name, param)  {if (logfp) { \
	fprintf(logfp, "%s(%d): ref=%d %s", name, getpid(), param, newline); \
	fflush(logfp); }}
#define LOG_ERR(name)     {if (logfp) { \
	fprintf(logfp, "%s(%d): err=%d,errno=%d %s", name, getpid(), *SPError, errno, newline); \
	fflush(logfp); }}
#else
#define LOG_MSG(name, param)
#define LOG_ERR(name)
#endif

typedef struct {
	unsigned short MachineTypeOffset;
	unsigned short AFPVersionsOffset;
	unsigned short UAMsOffset;
	unsigned short VolumeIconOffset;
	unsigned short Flags;
} asp_svrinfo_t;

typedef struct {
	unsigned short SigDataOffset;
	unsigned short TCPDataOffset;
} asp_tcpinfo_t;

typedef struct {
	unsigned char  len;
	unsigned char  addr_tag;
	unsigned char  addr_value[4];
	unsigned short port;
} asp_tcpaddr_t;

typedef struct {
	unsigned char tag;
	unsigned char len;
	unsigned char val[1];
} asp_opt_t;

typedef struct {
	unsigned char Flags;
	unsigned char Command;
	unsigned short RequestID;
	int ErrorCode;
	int TotalLen;
	int Reserved;
} asp_fspkt_t;

static int strpfx();
static int SendCommand();
static int GetQuantumSize();
static void GetSigInfo();
static unsigned char GetTCPInfo();
static unsigned short GetListenPort();

static unsigned short DefSessionTimer = ASP_SESS_TIMEOUT;
static unsigned short DefTickleInterval = ASP_TICKLE_INT;
static at_retry_t CmdRetry = {1, 10, 1};

/*
 * Per-session state. No lock acquisition is needed when
 * creating a session, since the session ID is actually a socket descriptor
 * returned by the kernel, and these will be unique per-thread, per-socket
 * call.
 */
typedef struct {
	char		SessType;	/* Session Type (ATP or TCP) */
	char		UserType;	/* Session User Type 
					   0 for server; 1 for client */
	int		Quantum;	/* Session Quantum size */
	asp_fspkt_t	SessFSPkt;
	u_short		FSRangeErr;
	u_short 	NewReqRefNum;
	int 		NewReqType;
	int 		NewReqLen;
	char 		NewReqBuf[ASPDEF_MaxCmdSize];
} session_t;

/*
 * List of per-session state. See above comment for session_t definition.
 */
static session_t **Session;

static char *FSStatusBlk;
static int FSStatusLen = 0;
static int FSQuantumSize = 0;
static unsigned short FSRequestID = 0;
static unsigned short FSListenPort = 0;

typedef struct {
  int SLSRefNum;		/* ASP descriptor - used to register service with NBP
						    and for ASP listen */
  int IN_descr;			/* TCP descriptor */
  at_inet_t AT_addr;		/* AppleTalk internet address: net, node, and socket */
  unsigned short IN_port;	/* TCP port */
} listener_t;

static listener_t *Listener;

/* 
	These routines need to know about the structure of the
	Session and/or the Listener arrays.
*/

/*	initSess() is used to initialize a Session data element.
*/
void
initSess(sp)
     session_t *sp;
{


	if (sp) {
		(void)memset(sp, 0, sizeof(session_t));
	}
}

/*	findSess() returns a pointer to the Session data matching 
	the fd parameter.
*/
static session_t *
findSess(fd, SPError)
     int fd;
     int *SPError;
{
	session_t *sp = (session_t *)NULL;

	if ((fd < 0) || (fd >= ASPDEF_DefMaxDescr))
		*SPError = ASPERR_ParamErr;
	else if (!Session || !(sp = Session[fd]))
		*SPError = ASPERR_NoSuchDevice;
	else
		*SPError = ASPERR_NoError;
	return(sp);
}

/*	createSess() returns a pointer to the Session data matching
        fd, allocating one if necessary.
*/
static session_t *
createSess(fd, SPError)
     int fd;
     int *SPError;
{
	session_t *sp = (session_t *)NULL;

	if (!(sp = findSess(fd, SPError))) {
	    if (*SPError != ASPERR_ParamErr) {
			if (Session && (sp = malloc(sizeof(session_t)))) {
				*SPError = ASPERR_NoError;
				Session[fd] = sp;
				initSess(sp);
			} else 
				*SPError = ASPERR_NoMoreSessions;
		}
	}
	return(sp);
}

/*	deleteSess() is used to reinitialize a Session data element
	that is no longer in use.
*/
void
deleteSess(fd)
     int fd;
{
	int err;
	session_t *sp = findSess(fd, &err);

	if (sp)
		initSess(sp); /* or instead, we could deallocate */
}

/*	readSess()
*/
static session_t *
readSess(fd, SPError)
     int fd;
     int *SPError;
{
	session_t *sp = findSess(fd, SPError);

	return(sp);
}

/*	writeSess()
*/
static session_t *
writeSess(fd, SPError)
     int fd;
     int *SPError;
{
	session_t *sp = findSess(fd, SPError);

	return(sp);
}

static listener_t *
find_SLS(refNum)
     int refNum;
{
	int i;

	if (Listener) {
		for (i = 0; i < ASPDEF_MaxNumOfSLS; i++) {
			if (Listener[i].SLSRefNum == refNum)
				return(&Listener[i]);
		}
	}
	return((listener_t *)NULL);
}

static listener_t *
find_lsock(socket)
     int socket;
{
	int i;

	if (Listener) {
		for (i = 0; i < ASPDEF_MaxNumOfSLS; i++) {
			if (Listener[i].AT_addr.socket == socket)
				return(&Listener[i]);
		}
	}
	return((listener_t *)NULL);
}

static ASP_init_done = 0;
void
asp_init(void)
{
	int i;

	if (ASP_init_done++)
		return;
	
	if (!Session) {
		Session = calloc(ASPDEF_DefMaxDescr, sizeof(session_t *));
	}
	
	if (!Listener) {
		Listener = calloc(ASPDEF_MaxNumOfSLS, sizeof(listener_t));
	}
	
}

/* End of routines with knowlege of the Session array structure. */

/* 
 * Name: SPAttention (server only)
 */
int
SPAttention(int SessRefNum,
	unsigned short AttentionCode,
	int *SPError,
	int NoWait)
{
	int CmdResult, rval, len;
	session_t *sp;
	char tmpBuf[ASPDEF_MaxCmdSize];
#ifdef ASPDEBUG
	char *rn = "SPAttention";
#endif

	LOG_MSG(rn, SessRefNum);

	/*
	 * send the attention to peer
	 */
	if (SendCommand(SessRefNum,
			&AttentionCode, 2, SPError, ASPFUNC_Attention) == -1)
		return -1;

	/*
	 * return if caller doesn't want to wait for the response
	 */
	if (NoWait) {
		*SPError = ASPERR_NoError;
		return 0;
	}

	/*
	 * get the response
	 */
	if ((rval = SPGetReply(SessRefNum, tmpBuf, sizeof(tmpBuf), 
			      &CmdResult, &len, SPError))) {
		/*
		 * this is a request from peer - not the response to the attention
		 * message. save the request and wait for the response again.
		 */
		if (rval == -1)
			return rval;
		/* A copy is necesary in this (unusual) case because the
		   read lock is taken in SPGetReply. */
		if ((sp = writeSess(SessRefNum, SPError))) {
			sp->NewReqLen = len;
			memcpy(sp->NewReqBuf, tmpBuf, sizeof(tmpBuf));
			sp->NewReqType = rval;
			sp->NewReqRefNum = CmdResult;
		}
		if ((rval = SPGetReply(SessRefNum, 0, 0, &CmdResult, 0, 
				       SPError)))
			return rval;
	}
	*SPError = ASPERR_NoError;
	return 0;
}

/*
 * Name: SPCloseSession (client & server)
 *
 * Acquires write lock on session descriptor and holds lock until
 * close operation is complete.
 */
int
SPCloseSession(int SessRefNum,
	int *SPError)
{
	session_t *sp;
#ifdef ASPDEBUG
	char *rn = "SPCloseSession";
#endif

	if (!(sp = writeSess(SessRefNum, SPError))) {
		LOG_ERR(rn);
		return -1; 
	}
	if (sp->SessType != ASPDT_TCP_FS) { 
		/* send close notification to peer for AppleTalk session */ 
		(void)at_send_to_dev(SessRefNum, ASPIOC_CloseSession, 0, 0);
	}

	/*
	 * close the session descriptor
	 */
	close(SessRefNum);

	deleteSess(SessRefNum);

	LOG_MSG(rn, SessRefNum);
	*SPError = ASPERR_NoError;
	return 0;
}

/*
 * Name: SPCmdReply (server only)
 */
int
SPCmdReply(int SessRefNum,
	unsigned short ReqRefNum,
	int CmdResult,
	char *CmdReplyData,
	int CmdReplyDataSize,
	int *SPError)
{
	struct iovec iov[2];
	union asp_primitives *primitives;
	strbuf_t data;
	strbuf_t ctrl;
	char ctrlbuf[ASPDEF_BufSize];
	session_t *sp;
#ifdef ASPDEBUG
	char *rn = "SPCmdReply";
#endif

	if (!(sp = readSess(SessRefNum, SPError))) {
		LOG_ERR(rn);
		return -1;
	}
	if (sp->SessType == ASPDT_TCP_FS) {
		int quantumSize;
		char fspktbuf[sizeof(asp_fspkt_t)+256];
		asp_fspkt_t *fspkt = (asp_fspkt_t *)fspktbuf;

		/*
		 * make sure the amount of reply data is not more than the
		 * maximum size allowed
		 */
		if (sp->UserType)
			quantumSize = sp->Quantum;
		else {
			if (FSQuantumSize == 0)
				FSQuantumSize = GetQuantumSize();
			quantumSize = FSQuantumSize;
		}
		if (CmdReplyDataSize > quantumSize) {
			SET_ERRNO(ERANGE);
			*SPError = ASPERR_SizeErr;
			LOG_ERR(rn);
			return -1;
		}

		/*
		 * create and send the reply to peer
		 */
		fspkt->Flags = ASPFS_Reply;
/*To fix bug 2210045*/
		fspkt->Command = ASPFUNC_Command;
		fspkt->RequestID = ReqRefNum;
		fspkt->ErrorCode = CmdResult;
		fspkt->TotalLen = CmdReplyDataSize;
		fspkt->Reserved = 0;
		iov[0].iov_base = (caddr_t)fspkt;
		iov[0].iov_len = sizeof(*fspkt);
		iov[1].iov_base = (caddr_t)CmdReplyData;
		iov[1].iov_len = CmdReplyDataSize;
		if (writev(SessRefNum, iov, 2) == -1) {
			*SPError = (errno == EINVAL) ? ASPERR_ParamErr : ASPERR_SessClosed;
			LOG_ERR(rn);
			return -1;
		}
	} else {

		/*
		 * make sure the amount of reply data is not more than the
		 * maximum size allowed
		 */
		if (CmdReplyDataSize > ASPDEF_QuantumSize) {
			SET_ERRNO(ERANGE);
			*SPError = ASPERR_SizeErr;
			LOG_ERR(rn);
			return -1;
		}

		/*
		 * create and send the reply to peer
		 */
		primitives = (union asp_primitives *)ctrlbuf;
		primitives->CmdReplyReq.Primitive = ASPFUNC_CmdReply;
		primitives->CmdReplyReq.CmdResult = CmdResult;
		primitives->CmdReplyReq.ReqRefNum = ReqRefNum;
		ctrl.len = sizeof(ctrlbuf);
		ctrl.buf = ctrlbuf;
		data.len = CmdReplyDataSize;
		data.buf = CmdReplyData;
		if (ATputmsg(SessRefNum, &ctrl, &data, 1) == -1) {
			if (errno == EINVAL)
				*SPError = ASPERR_ParamErr;
			else if (errno == EAGAIN)
				*SPError = ASPERR_ProtoErr;
			else
				*SPError = ASPERR_SessClosed;
			LOG_ERR(rn);
			return -1;
		}
	}

	*SPError = ASPERR_NoError;
	LOG_MSG(rn, SessRefNum);
	return 0;
} /* SPCmdReply */

/*
 * Name: SPGetParms (client & server)
 *
 */
void
SPGetParms(int *MaxCmdSize,
	int *QuantumSize,
	int SessRefNum)
{
	int err;
	session_t *sp;
#ifdef ASPDEBUG
	int	error = ASPERR_NoError;
	int *SPError = &error;
	char *rn = "SPGetParms";
#endif

	LOG_MSG(rn, SessRefNum);

	/* set the defaults, even if no valid SessRefNum has been provided */
	*MaxCmdSize = ASPDEF_MaxCmdSize;
	*QuantumSize = ASPDEF_QuantumSize;

	if (!(sp = readSess(SessRefNum, &err))) {
		LOG_ERR(rn);
		return;
	}
	if (sp->SessType == ASPDT_TCP_FS)
		if (sp->UserType)
			*QuantumSize = sp->Quantum;
		else
			*QuantumSize = GetQuantumSize();
} /* SPGetParms */

/*
 * Name: SPGetRequest (server only)
 */
int
SPGetRequest(int SessRefNum,
	char *ReqBuff,
	int ReqBuffSize,
	unsigned short *ReqRefNum,
	int *ReqType,
	int *ActRcvdReqLen,
	int *SPError)
{
	int rval;
	union asp_primitives *primitives;
	strbuf_t data;
	strbuf_t ctrl;
	char ctrlbuf[ASPDEF_BufSize];
	asp_fspkt_t *fspkt;
	session_t *sp;
#ifdef ASPDEBUG
	char *rn = "SPGetRequest";
#endif

	if (!(sp = readSess(SessRefNum, SPError))) {
		SET_ERRNO(EBADF);
		LOG_ERR(rn);
		return -1;
	}

	if (sp->NewReqType) {
		/*
		 * there is a saved request received during the previous
		 * attention transaction. return this saved request
		 * to the caller.
		 */
		memcpy(ReqBuff, sp->NewReqBuf, sp->NewReqLen);
		*ReqType = sp->NewReqType;
		*ReqRefNum = sp->NewReqRefNum;
		*ActRcvdReqLen = sp->NewReqLen;
		sp->NewReqType = 0;
		*SPError = ASPERR_NoError;
		LOG_MSG(rn, SessRefNum);
		return 0;
	}

	fspkt = (asp_fspkt_t *)&sp->SessFSPkt;
	if (sp->SessType == ASPDT_TCP_FS) {
		int len, len_left, sum = 0;

		if ((!sp->FSRangeErr) || (sp->SessFSPkt.Flags & ASPFS_Reply)) {
			while (1) {
				if (recv(SessRefNum, (char *)fspkt, sizeof(*fspkt), 0) != sizeof(*fspkt)) {
					*SPError = ASPERR_SessClosed;
					LOG_ERR(rn);
					return -1;
				}
				if (fspkt->Command != ASPFUNC_Tickle)
					break;
				send(SessRefNum, (char *)fspkt, sizeof(*fspkt),MSG_EOR);
			}
		}

		if (fspkt->Command == ASPFUNC_Write) {
			if (fspkt->ErrorCode == 0)
				fspkt->ErrorCode = fspkt->TotalLen;
			len_left = fspkt->ErrorCode;
		} else
			len_left = fspkt->TotalLen;

		sp->FSRangeErr = 0;
		if (len_left > ReqBuffSize) {
			sp->FSRangeErr = 1;
			SET_ERRNO(ERANGE);
			*SPError = ASPERR_BufTooSmall;
			LOG_ERR(rn);
			return -1;
		}
		while (len_left && ((len = recv(SessRefNum,
			(char *)&ReqBuff[sum], len_left, 0)) > 0)) {
				sum += len;
				len_left -= len;
		}
		if (len_left) {
			*SPError = ASPERR_SessClosed;
			LOG_ERR(rn);
			return -1;
		}
		*ActRcvdReqLen = sum;
		if (fspkt->Flags & ASPFS_Reply) {
			*ReqRefNum = fspkt->ErrorCode;
			SET_ERRNO(EPROTOTYPE);
			*SPError = ASPERR_CmdReply;
			LOG_ERR(rn);
			return 1;
		}
		*ReqRefNum = fspkt->RequestID;
		*ReqType = (int)fspkt->Command;
	} else {
		/*
		 * get the next message from peer
		 */
		primitives = (union asp_primitives *)ctrlbuf;
		ctrl.maxlen = sizeof(ctrlbuf);
		ctrl.len = 0;
		ctrl.buf = ctrlbuf;
		data.maxlen = ReqBuffSize;
		data.len = 0;
		data.buf = ReqBuff;
		while ((rval = ATgetmsg(SessRefNum, &ctrl, &data, 0)) == -1) {
			if (errno == EINTR)
				continue;
			if (errno == EINVAL)
				*SPError = ASPERR_ParamErr;
			else
				*SPError = ASPERR_SessClosed;
			LOG_ERR(rn);
			return -1;
		}

		/*
		 * if the size of the received message is larger than
		 * the given buffer, return size error indication.
		 */
		if (rval & MOREDATA) {
			SET_ERRNO(ERANGE);
			*SPError = ASPERR_BufTooSmall;
			LOG_ERR(rn);
			return -1;
		}

		/*
		 * if there's no control info, the session
		 * has probably been closed.
		 */
		if (ctrl.len == 0) {
			SET_ERRNO(ENOTCONN);
			*SPError = ASPERR_SessClosed;
			return -1;
		}

		if (primitives->CommandInd.Primitive == ASPFUNC_CmdReply) {
			if (ctrl.len == sizeof(asp_cmdreply_ind_t)) {
				/*
				 * this is the reply to the previous
				 * async request - not a new request.
				 * let the caller know as such.
				 */
				if ((*ActRcvdReqLen = data.len) < 0)
					*ActRcvdReqLen = 0;
				*ReqRefNum = primitives->CmdReplyInd.CmdResult;
				SET_ERRNO(EPROTOTYPE);
				*SPError = ASPERR_CmdReply;
				return 1;	/* XXX should be -1 ?? */
			} else {
				SET_ERRNO(EPROTOTYPE);
				*SPError = ASPERR_ProtoErr;
			}
			LOG_ERR(rn);
			return -1;
		}

		*ReqRefNum = primitives->CommandInd.ReqRefNum;
		*ReqType = (int)primitives->CommandInd.ReqType;
		if ((*ActRcvdReqLen = data.len) < 0)
			*ActRcvdReqLen = 0;
	}

	*SPError = ASPERR_NoError;
	LOG_MSG(rn, SessRefNum);
	return 0;
} /* SPGetRequest */

/*
 * Name: SPGetSession (server only)
 */
int
SPGetSession(int SLSRefNum,
	int *SessRefNum,
	int *SPError)
{
	int ret;
	char descr_type;
	int ws, fs_conn_descr = -1;
	int ic_len, descr = -1;
	at_inet_t addr;
	fd_set readfds;
	int nfds;
	struct timeval tv;
	listener_t *lp;
	session_t *sp;
#ifdef ASPDEBUG
	char *rn = "SPGetSession";
#endif

	asp_init();	/* XXX */

	LOG_MSG(rn, SLSRefNum);
	
	if (!(lp = find_SLS(SLSRefNum))) {
		SET_ERRNO(ENOENT);
		*SPError = ASPERR_NoSuchEntity;
		LOG_ERR(rn);
		return -1;
	}

	if (FSListenPort && lp->IN_descr)
		fs_conn_descr = lp->IN_descr;

l_again:
  if (fs_conn_descr != -1) {
	FD_ZERO(&readfds);
	FD_SET(SLSRefNum, &readfds);
	FD_SET(fs_conn_descr, &readfds);
	nfds = ((SLSRefNum > fs_conn_descr)? SLSRefNum : fs_conn_descr) + 1;
	if ((ret = select(nfds, &readfds, 0, 0, 0)) <= 0) {
		if ((errno == EINTR) || (ret == 0))
			goto l_again;
		else {
			*SPError = ASPERR_SystemErr;
			goto l_error;
		}
	} else {
		if (FD_ISSET(SLSRefNum, &readfds))
			descr_type = ASPDT_ATP_AS;
		else if (FD_ISSET(fs_conn_descr, &readfds))
			descr_type = ASPDT_TCP_FS;
		else
			goto l_again;
	}
  } else {
	while (ATgetmsg(SLSRefNum, 0, 0, 0) == -1) {
		if (errno == EINTR)
			continue;
		if (errno == EINVAL)
			*SPError = ASPERR_ParamErr;
		else
			*SPError = ASPERR_SessClosed;
		LOG_ERR(rn);
		return -1;
	}
	descr_type = ASPDT_ATP_AS;
  }

  if (descr_type == ASPDT_TCP_FS) {
	char fspktbuf[32], optbuf[ASPFS_MaxOptLen];
	asp_fspkt_t *fspkt = (asp_fspkt_t *)fspktbuf;
	struct sockaddr_in in_addr;
	int in_addrlen = sizeof(in_addr);
	int len, len_left;
	asp_opt_t *opt;

	if ((descr = accept(fs_conn_descr, (void *)&in_addr, &in_addrlen)) == -1) {
		*SPError = ASPERR_ProtoErr;
		goto l_error;
	}

	tv.tv_sec = 10; tv.tv_usec = 0;
	FD_ZERO(&readfds);
	FD_SET(descr, &readfds);
	nfds = descr + 1;
	if ((select(nfds, &readfds, 0, 0, (void *)&tv) <= 0)
	|| (recv(descr, (char *)fspkt, sizeof(*fspkt), 0) != sizeof(*fspkt))) {
		close(descr);
		goto l_again;
	}

	switch (fspkt->Command) {
	case ASPFUNC_OpenSess:
		len_left = fspkt->TotalLen;
		if ((len_left < 0) || (len_left > ASPFS_MaxOptLen))
			goto l_paramErr;

		tv.tv_sec = ASP_CONN_TIMEOUT; tv.tv_usec = 0;
		setsockopt(descr, SOL_SOCKET,SO_RCVTIMEO,(void *)&tv, sizeof(tv));
		while (len_left
		&& ((len = recv(descr, &optbuf[fspkt->TotalLen-len_left],
				len_left,0)) > 0)) {
					len_left -= len;
		}
		if (len_left)
			goto l_paramErr;

		tv.tv_sec = ASP_SESS_TIMEOUT; tv.tv_usec = 0;
		ws = 0x10000;
		setsockopt(descr, SOL_SOCKET,SO_SNDBUF,(void *)&ws, sizeof(ws));
		setsockopt(descr, SOL_SOCKET,SO_RCVBUF,(void *)&ws, sizeof(ws));
		setsockopt(descr, SOL_SOCKET,SO_RCVTIMEO,(void *)&tv, sizeof(tv));
		fspkt->Flags = ASPFS_Reply;
/*To fix bug 2210045*/
		fspkt->Command = ASPFUNC_OpenSess;
		fspkt->ErrorCode = 0;
		fspkt->TotalLen = 6;
		fspkt->Reserved = 0;
		opt = (asp_opt_t *)(fspkt+1);
		opt->tag = ASPFS_DataQuantumTag;
		opt->len = sizeof(int);
		*(int *)opt->val = FSQuantumSize;
		send(descr, (char *)fspkt, sizeof(*fspkt)+fspkt->TotalLen,MSG_EOR);
		break;

	case ASPFUNC_GetStatus:
	  if (fspkt->TotalLen == 0) {
		*(asp_fspkt_t *)FSStatusBlk = *fspkt;
		fspkt = (asp_fspkt_t *)FSStatusBlk;
		fspkt->Flags = ASPFS_Reply;
/*To fix bug 2210045*/
		fspkt->Command = ASPFUNC_GetStatus;
		fspkt->ErrorCode = 0;
		fspkt->TotalLen = FSStatusLen;
		fspkt->Reserved = 0;
		send(descr, FSStatusBlk, sizeof(*fspkt)+fspkt->TotalLen,MSG_EOR);
		goto l_cmdDone;
	  }

	default: /* ignore unexpected commands */
l_paramErr:
		fspkt->Flags = ASPFS_Reply;
/*To fix bug 2210045*/
		/*fspkt->Command = 0;*/
		fspkt->ErrorCode = ASPERR_ParamErr;
		fspkt->TotalLen = 0;
		fspkt->Reserved = 0;
		send(descr, (char *)fspkt, sizeof(*fspkt),MSG_EOR);
l_cmdDone:
		tv.tv_sec = 2; tv.tv_usec = 0;
		FD_ZERO(&readfds);
		FD_SET(descr, &readfds);
		nfds = descr + 1;
		select(nfds, &readfds, 0, 0, (void *)&tv);
		close(descr);
		goto l_again;
	}
  } else {

	/*
	 * get the server identity
	 */
    	addr.socket = lp->AT_addr.socket;
	addr.net = (u_short)DefSessionTimer;
	addr.node = (unsigned char)DefTickleInterval;

	/*
	 * allocate a session channel
	 */
	descr = ATsocket(ATPROTO_ASP);
	if (descr == -1) {
		*SPError = ASPERR_NoSuchDevice;
		LOG_ERR(rn);
		return -1;
	}

	/*
	 * accept session opening from a client
	 */
	ic_len = sizeof(addr);
	while (at_send_to_dev(descr, ASPIOC_GetSession, (char *) &addr, &ic_len) == -1) {
	  /* previously when errno was EPIPE it was "goto l_again" 
	     however this caused an infinite loop (Rhapsody) */
	  if (errno != EAGAIN) {
		*SPError = ASPERR_SystemErr;
		goto l_error;
	  }
	  ic_len = sizeof(addr);
	  sleep(1);
	}
  }

	if (!(sp = createSess(descr, SPError))) {
		goto l_error;
	}
	sp->SessType = descr_type;

	*SessRefNum = descr;
	*SPError = ASPERR_NoError;
	LOG_MSG(rn, descr);
	return 0;

l_error:
	if (descr != -1)
		close(descr);
	LOG_ERR(rn);
	return -1;
} /* SPGetSession */

/*
 * Name: SPInit (server only)
 */
int
SPInit(at_inet_t *SLSEntityIdentifier,
	char *ServiceStatusBlock,
	int ServiceStatusBlockSize,
	int *SLSRefNum,
	int *SPError)
{
	listener_t *lp;

#ifdef ASPDEBUG
	char *rn = "SPInit";
#endif

	if (!(lp = find_lsock(SLSEntityIdentifier->socket))) {
		SET_ERRNO(ENOENT);
		*SPError = ASPERR_NoSuchEntity;
		LOG_ERR(rn);
		return -1;
	}

	*SLSRefNum = lp->SLSRefNum;

	LOG_MSG(rn, *SLSRefNum);
	
	if (!FSStatusBlk) {
		if (!(FSStatusBlk = calloc(sizeof(asp_fspkt_t)+ASPDEF_QuantumSize, sizeof(char)))) {
			SET_ERRNO(ENOMEM);
			*SPError = ASPERR_SystemErr;
			LOG_ERR(rn);
			return -1;
		}
	}
	
	return SPNewStatus(*SLSRefNum, ServiceStatusBlock,
		ServiceStatusBlockSize, SPError);
}

/*
 * Name: SPNewStatus (server only)
 */
int
SPNewStatus(int SLSRefNum,
	char *ServiceStatusBlock,
	int ServiceStatusBlockSize,
	int *SPError)
{
	int ic_len;
	listener_t *lp;
#ifdef ASPDEBUG
	char *rn = "SPNewStatus";
#endif

	if (ServiceStatusBlockSize > ASPDEF_QuantumSize) {
		SET_ERRNO(ERANGE);
		*SPError = ASPERR_SizeErr;
		LOG_ERR(rn);
		return -1;
	}

	if (FSListenPort) {
	  	if (!(lp = find_SLS(SLSRefNum))) {
			SET_ERRNO(ENOENT);
			*SPError = ASPERR_NoSuchEntity;
			LOG_ERR(rn);
			return -1;
		}
		if (lp->IN_descr) {
			int len, sblk_len, tcp_len, pad;
			unsigned char *sblk_ptr;
			asp_svrinfo_t *svrinfo;
			asp_tcpinfo_t *tcpinfo;
			unsigned short VolumeIconOffset;

			/*
			 * modify the status block to indicate support
			 * for TCP connections
			 */
			if ((VolumeIconOffset = ((asp_svrinfo_t *)
				ServiceStatusBlock)->VolumeIconOffset) == 0)
			VolumeIconOffset = ServiceStatusBlockSize;
			len = sizeof(asp_svrinfo_t) + 1 + (int)((unsigned char)ServiceStatusBlock[sizeof(asp_svrinfo_t)]);
			sblk_ptr = &FSStatusBlk[sizeof(asp_fspkt_t)];
			memcpy(sblk_ptr, ServiceStatusBlock, len);
			pad = (len & 1) ? 1 : 0;
			sblk_len = len + pad;
			tcpinfo = (asp_tcpinfo_t *)&sblk_ptr[sblk_len];
			sblk_len += sizeof(asp_tcpinfo_t);
			tcpinfo->SigDataOffset = VolumeIconOffset + pad+sizeof(asp_tcpinfo_t);
			tcpinfo->TCPDataOffset = tcpinfo->SigDataOffset + ASPFS_SigDataLen;
			GetSigInfo(&sblk_ptr[tcpinfo->SigDataOffset]);
			sblk_ptr[tcpinfo->TCPDataOffset] = 
				GetTCPInfo(&sblk_ptr[tcpinfo->TCPDataOffset+1], lp->IN_port);
			tcp_len = (ASPFS_SigDataLen + 1 +
				(sizeof(asp_tcpaddr_t)*sblk_ptr[tcpinfo->TCPDataOffset]));
			svrinfo = (asp_svrinfo_t *)&FSStatusBlk[sizeof(asp_fspkt_t)];
			if (svrinfo->MachineTypeOffset)
				svrinfo->MachineTypeOffset += (pad+sizeof(asp_tcpinfo_t));
			if (svrinfo->AFPVersionsOffset)
				svrinfo->AFPVersionsOffset += (pad+sizeof(asp_tcpinfo_t));
			if (svrinfo->UAMsOffset)
				svrinfo->UAMsOffset += (pad+sizeof(asp_tcpinfo_t));
			if (svrinfo->VolumeIconOffset) {
				svrinfo->VolumeIconOffset += (pad+sizeof(asp_tcpinfo_t)+tcp_len);
				memcpy((char *)&sblk_ptr[svrinfo->VolumeIconOffset],
					&ServiceStatusBlock[VolumeIconOffset],
					ServiceStatusBlockSize-VolumeIconOffset);
			}
			svrinfo->Flags |= (ASPFS_SigSuppBit | ASPFS_TCPSuppBit);
			memcpy((char *)&sblk_ptr[sblk_len],
				&ServiceStatusBlock[len], VolumeIconOffset-len);
			ServiceStatusBlockSize += (pad+sizeof(asp_tcpinfo_t)+tcp_len);
			FSStatusLen = ServiceStatusBlockSize;
			ServiceStatusBlock = &FSStatusBlk[sizeof(asp_fspkt_t)];
		}
	}

	ic_len = ServiceStatusBlockSize;
	if (at_send_to_dev(SLSRefNum, ASPIOC_StatusBlock,
			ServiceStatusBlock, &ic_len) == -1) {
		*SPError = ASPERR_ParamErr;
		LOG_ERR(rn);
		return -1;
	}

	*SPError = ASPERR_NoError;
	LOG_MSG(rn, SLSRefNum);
	return 0;
}

/*
 * Name: SPWrtContinue (server only)
 */
int
SPWrtContinue(int SessRefNum,
	unsigned short ReqRefNum,
	char *Buff,
	int BuffSize,
	int *ActLenRcvd,
	int *SPError,
	int NoWait)
{
	char CmdBlock[2];
	int CmdBlockSize;
	int CmdResult;
	session_t *sp;
#ifdef ASPDEBUG
	char *rn = "SPWrtContinue";
#endif

	LOG_MSG(rn, SessRefNum);

	if (!(sp = readSess(SessRefNum, SPError))) {
		LOG_ERR(rn);
		return -1;
	}
	if (sp->SessType == ASPDT_TCP_FS) {
		asp_fspkt_t *fspkt = &sp->SessFSPkt;
		int len, len_left, sum = 0, d_len = 0;
		char d_buf[1024];

		len_left = fspkt->TotalLen - fspkt->ErrorCode;
		if (len_left > BuffSize) {
			d_len = len_left - BuffSize;
			len_left = BuffSize;
		}
		while (len_left && ((len = recv(SessRefNum,
				(char *)&Buff[sum], len_left, 0)) > 0)) {
			sum += len;
			len_left -= len;
		}
		if (len_left) {
			*SPError = ASPERR_SessClosed;
			LOG_ERR(rn);
			return -1;
		}
		if (d_len) {
			len = (d_len < sizeof(d_buf)) ? d_len : sizeof(d_buf);
			while (d_len && ((len = recv(SessRefNum, d_buf, len, 0)) > 0)) {
				d_len -= len;
				len = (d_len < sizeof(d_buf)) ? d_len : sizeof(d_buf);
			}
		}
		*ActLenRcvd = sum;
		*SPError = ASPERR_NoError;
		return 0;
	} else {

		CmdBlockSize = 2;
		*(unsigned short *)&CmdBlock[0] = BuffSize;

		/*
		 * send the write continue request to peer
		 */
		if (SendCommand(SessRefNum,
			CmdBlock, CmdBlockSize, SPError, ASPFUNC_WriteContinue) == -1) {
				return -1;
		}

		/*
		 * return if caller doesn't want to wait for the response
		 */
		if (NoWait) {
			*SPError = ASPERR_NoError;
			return 0;
		}

		/*
		 * get the response
		 */
		return SPGetReply(SessRefNum, Buff,
			BuffSize, &CmdResult, ActLenRcvd, SPError);
	}
}

/*
 * Name: SPWrtReply (server only)
 */
int
SPWrtReply(int SessRefNum,
	unsigned short ReqRefNum,
	int CmdResult,
	char *CmdReplyData,
	int CmdReplyDataSize,
	int *SPError)
{
#ifdef ASPDEBUG
	char *rn = "SPWrtReply";
#endif

	LOG_MSG(rn, SessRefNum);

	return SPCmdReply(SessRefNum, ReqRefNum,
		CmdResult, CmdReplyData, CmdReplyDataSize, SPError);
}

/*
 * Name: SPGetReply (client & server, supplemental call)
 */
int
SPGetReply(int SessRefNum,
	char *ReplyBuffer,
	int ReplyBufferSize,
	int *CmdResult,
	int *ActRcvdReplyLen,
	int *SPError)
{
	int rval;
	union asp_primitives *primitives;
	strbuf_t data;
	strbuf_t ctrl;
	char ctrlbuf[ASPDEF_BufSize];
	session_t *sp;
#ifdef ASPDEBUG
	char *rn = "SPGetReply";
#endif

	if (!(sp = readSess(SessRefNum, SPError))) {
		SET_ERRNO(EBADF);
		LOG_ERR(rn);
		return -1;
	}
	if (sp->SessType == ASPDT_TCP_FS) {
		asp_fspkt_t fspktbuf, *fspkt = &fspktbuf;
		int len, len_left, sum = 0;

		if (sp->FSRangeErr && (sp->SessFSPkt.Flags & ASPFS_Reply)) {
			*fspkt = sp->SessFSPkt;
			sp->SessFSPkt.Flags = 0;
		} else {
			if (recv(SessRefNum, (char *)fspkt, sizeof(*fspkt), 0) != sizeof(*fspkt)) {
				*SPError = ASPERR_SessClosed;
				LOG_ERR(rn);
				return -1;
			}
		}
		if (fspkt->Flags & ASPFS_Reply)
			len_left = fspkt->TotalLen;
		else {
			len_left = fspkt->TotalLen;
			if (fspkt->Command == ASPFUNC_Write) {
				sp->SessFSPkt = *fspkt;
				if (fspkt->ErrorCode)
					len_left = fspkt->ErrorCode;
			}
		}
		sp->FSRangeErr = 0;
		if (len_left > ReplyBufferSize) {
			if (fspkt->Flags & ASPFS_Reply) {
				sp->FSRangeErr = 1;
				sp->SessFSPkt = *fspkt;
			}
			SET_ERRNO(ERANGE);
			*SPError = ASPERR_BufTooSmall;
			LOG_ERR(rn);
			return -1;
		}
		while (len_left && ((len = recv(SessRefNum,
			(char *)&ReplyBuffer[sum], len_left, 0)) > 0)) {
				sum += len;
				len_left -= len;
		}
		if (len_left) {
			*SPError = ASPERR_SessClosed;
			LOG_ERR(rn);
			return -1;
		}
		if (!(fspkt->Flags & ASPFS_Reply)) {
			SET_ERRNO(EPROTOTYPE);
			*SPError = ASPERR_CmdRequest;
			*CmdResult = (int)fspkt->RequestID;
			LOG_ERR(rn);
			return (int)fspkt->Command;
		}
		*CmdResult = (int)fspkt->ErrorCode;
		if (ReplyBufferSize)
			*ActRcvdReplyLen = sum;
	} else {

		primitives = (union asp_primitives *)ctrlbuf;
		ctrl.maxlen = sizeof(ctrlbuf);
		ctrl.len = 0;
		ctrl.buf = ctrlbuf;
		data.maxlen = ReplyBufferSize;
		data.len = 0;
		data.buf = ReplyBuffer;
		while ((rval = ATgetmsg(SessRefNum, &ctrl, &data, 0)) == -1) {
			if (errno == EINTR)
				continue;
			if (errno == EINVAL)
				*SPError = ASPERR_ParamErr;
			else
				*SPError = ASPERR_SessClosed;
			LOG_ERR(rn);
			return -1;
		}
		if (rval & MOREDATA) {
			SET_ERRNO(ERANGE);
			*SPError = ASPERR_BufTooSmall;
			LOG_ERR(rn);
			return -1;
		}
		if (ctrl.len == 0) {
			SET_ERRNO(ENOTCONN);
			*SPError = ASPERR_SessClosed;
			LOG_ERR(rn);
			return -1;
		}
		if (primitives->CmdReplyInd.Primitive != ASPFUNC_CmdReply) {
			if (ctrl.len == sizeof(asp_command_ind_t)) {
				if ((*ActRcvdReplyLen = data.len) < 0)
					*ActRcvdReplyLen = 0;
				SET_ERRNO(EPROTOTYPE);
				*SPError = ASPERR_CmdRequest;
				*CmdResult = (int)primitives->CommandInd.ReqRefNum;
				return (int)primitives->CommandInd.ReqType;
			} else {
				SET_ERRNO(EPROTOTYPE);
				*SPError = ASPERR_ProtoErr;
			}
			LOG_ERR(rn);
			return -1;
		}

		*CmdResult = primitives->CmdReplyInd.CmdResult;
		if (ReplyBufferSize) {
			if ((*ActRcvdReplyLen = data.len) < 0)
				*ActRcvdReplyLen = 0;
		}
	}

	*SPError = ASPERR_NoError;
	LOG_MSG(rn, SessRefNum);
	return 0;
}

/*
 * Name: SPLook (client & server, supplemental call)
 */
int
SPLook(int SessRefNum,
	int *SPError)
{
	int rval, nfds;
	session_t *sp;
#ifdef ASPDEBUG
	char *rn = "SPLook";
#endif

	LOG_MSG(rn, SessRefNum);

	if (!(sp = readSess(SessRefNum, SPError))) {
		LOG_ERR(rn);
		return -1;
	}
	if (sp->SessType == ASPDT_TCP_FS) {
		asp_fspkt_t fspktbuf, *fspkt = &fspktbuf;
		struct timeval tv;
		fd_set readfds;

		SET_ERRNO(0);
		*SPError = ASPERR_NoError; 
		tv.tv_sec = 0; tv.tv_usec = 0;
		FD_ZERO(&readfds);
		FD_SET(SessRefNum, &readfds);
		nfds = SessRefNum + 1;
		if ((select(nfds, &readfds, 0, 0, (void *)&tv) <= 0)
		|| (recv(SessRefNum, (char *)fspkt, sizeof(*fspkt), MSG_PEEK)
			!= sizeof(*fspkt))) {
				if (errno) {
					if (errno != ENXIO)
						*SPError = ASPERR_SystemErr;
					else
						*SPError = ASPERR_SessClosed;
				}
			LOG_ERR(rn);
			return -1;
		}
		/*
		 * We have something to receive, now figure out what it is.
		 * If it's an ASPFUNC_Tickle message, handle it here and ask
		 * the caller to retry the SPLook request. Otherwise, return
		 * 0 if it's an ASPFS_Reply, or 1 if it's ASPFS_Request.
		 */
		if (fspkt->Command == ASPFUNC_Tickle) {
			if ((recv(SessRefNum, (char *)fspkt, sizeof(*fspkt), 0)
			    != sizeof(*fspkt))
			|| (send(SessRefNum, (char *)fspkt, sizeof(*fspkt), MSG_EOR)
			   != sizeof(*fspkt))) {
				/*
				 * we have an error on send/recv
				 */
				if (errno == ENXIO)
					*SPError = ASPERR_SessClosed;
				else if (errno)
					*SPError = ASPERR_SystemErr;

				LOG_ERR(rn);
			}
			/*
			 * Whether or not we have a send/recv error
			 * when handling the tickle, return -1 to indicate
			 * to the caller that no user data is available.
			 */
			return -1;
		}
		rval = (fspkt->Flags & ASPFS_Reply) ? 0 : 1;
		return rval;
	} else {

		int ic_len;

		*SPError = ASPERR_NoError;
		ic_len = sizeof(rval);
		if (at_send_to_dev(SessRefNum, ASPIOC_Look, (char *) &rval, &ic_len) == -1) {
			if (errno != ENXIO)
				*SPError = ASPERR_SystemErr;
			else
				*SPError = ASPERR_SessClosed;
			LOG_ERR(rn);
			return -1;
		}
		return rval;
	}
}

/*
 * Name: SPRegister (server only, supplemental call)
 */
int
SPRegister(at_entity_t *SLSEntity,
    at_retry_t *Retry,
    at_inet_t *SLSEntityIdentifier,
    int *SPError)
{
    unsigned short fs_port = 0xdead;
    int on, ws, fs_conn_descr = -1;
    int ic_len;
    listener_t *lp;
    int descr = -1;
    struct sockaddr_in in_addr;
    session_t *sp;
#ifdef ASPDEBUG
    char *rn = "SPRegister";
#endif

#ifdef ASPDEBUG
    logfp = fopen("/tmp/asp.log", "a");
#endif
    asp_init();	/* XXX */

    FSQuantumSize = ASPDEF_QuantumSize;
 
/*
 *  We don't need the following for the fix for bug 2285307;  this change makes SPRegister to only create 
 * listning port for asp over appletalk;  If application need to open a listening port over tcp when
 * afpovertcp.cfg is present; then they must call SPRegisterWithTCPPossibility.
*/  
#if 0
    FSQuantumSize = GetQuantumSize();
    FSListenPort = GetListenPort(0, (void *)0, 0);

    if (FSListenPort
    && !strncmp(SLSEntity->type.str, "AFPServer", SLSEntity->type.len)) {
        if ((fs_conn_descr = socket(PF_INET, SOCK_STREAM, 0)) == -1) {
            *SPError = ASPERR_NoSuchDevice;
            goto l_error;
        }

        /*
         * set sizes of the send/recv windows
         */

        on = 1;
        ws = 0x10000;
        setsockopt(fs_conn_descr, SOL_SOCKET,SO_SNDBUF,(void *)&ws,sizeof(ws));
        setsockopt(fs_conn_descr, SOL_SOCKET,SO_RCVBUF,(char *)&ws,sizeof(ws));
        setsockopt(fs_conn_descr, SOL_SOCKET,SO_REUSEADDR,(void *)&on,sizeof(on));

        /*
         * bind socket to the well-known assigned port
         */
        memset((char *)&in_addr, 0, sizeof(in_addr));
        fs_port = FSListenPort;
        in_addr.sin_family = PF_INET;
        in_addr.sin_port = fs_port;
        if (bind(fs_conn_descr, (void *)&in_addr, sizeof(in_addr)) == -1) {
            if (errno == EADDRINUSE) {
                /*
                 * failed to use the well-known assigned
                 * port because someone is already using it,
                 * use a dynamic port instead.
                 */
                int in_addrlen = sizeof(in_addr);
                in_addr.sin_port = 0;
                if (bind(fs_conn_descr, (void *)&in_addr, sizeof(in_addr)) == -1) {
                    *SPError = ASPERR_BindErr;
                    goto l_error;
                }
                if (getsockname(fs_conn_descr, (void *)&in_addr, &in_addrlen) == -1) {
                    *SPError = ASPERR_SystemErr;
                    goto l_error;
                }
                fs_port = in_addr.sin_port;
            } else {
                *SPError = ASPERR_BindErr;
                goto l_error;
            }
        }

        /*
         * enable accepting TCP connections
         */
        if (listen(fs_conn_descr, 1) == -1) {
            *SPError = ASPERR_NoMoreSessions;
            goto l_error;
        }
        LOG_MSG(rn, fs_conn_descr);
    }
#endif
    /* find unused listener entry */
    if (!(lp = find_lsock(0))) {
        *SPError = ASPERR_NoMoreSessions;
        goto l_error;
    }

    descr = ATsocket(ATPROTO_ASP);
    if (descr == -1) {
        *SPError = ASPERR_NoSuchDevice;
        goto l_error;
    }

    ic_len = sizeof(at_inet_t);
    if (at_send_to_dev(descr, ASPIOC_ListenerBind,
            (char *) SLSEntityIdentifier, &ic_len) == -1) {
        *SPError = ASPERR_NoMoreSessions;
        goto l_error;
    }

    if (nbp_register(SLSEntity, descr, Retry) == -1) {
        *SPError = ASPERR_RegisterErr;
        goto l_error;
    }

    if (!(sp = createSess(descr, SPError))) {
        goto l_error;
    }
    sp->SessType = ASPDT_ATP_AS;
    lp->AT_addr = *SLSEntityIdentifier;
    lp->SLSRefNum = descr;
    if (fs_conn_descr != -1) {
        /* XXX suspicious of using fs_port before it's initialized */
        assert(fs_port != 0xdead);
        lp->IN_port = fs_port;
        lp->IN_descr = fs_conn_descr;
        if (!(sp = createSess(fs_conn_descr, SPError))) {
            goto l_error;
        }
        sp->SessType = ASPDT_TCP_FS;
    }

    *SPError = ASPERR_NoError;
    LOG_MSG(rn, descr);
    return 0;

l_error:
    if (fs_conn_descr != -1)
        close(fs_conn_descr);
    if (descr != -1)
        close(descr);
    LOG_ERR(rn);
    return -1;
}


/*
 * Name: SPRegisterWithTCPPossibility (server only, supplemental call)
 */

/*
 * This function (API) is added to fix bug #2285307;  Application that need asp over tcp if afpovertcp.cfg
 * file is present, need to call this function instead of SPRegister.  SPRegister now is asp ofer appletalk
 * only.
*/
int
SPRegisterWithTCPPossibility(at_entity_t *SLSEntity,
	at_retry_t *Retry,
	at_inet_t *SLSEntityIdentifier,
	int *SPError)
{
	unsigned short fs_port = 0xdead;
	int on, ws, fs_conn_descr = -1;
	int ic_len;
	listener_t *lp;
	int descr = -1;
	struct sockaddr_in in_addr;
	session_t *sp;
#ifdef ASPDEBUG
	char *rn = "SPRegister";
#endif

#ifdef ASPDEBUG
	logfp = fopen("/tmp/asp.log", "a");
#endif
	asp_init();	/* XXX */

	FSQuantumSize = GetQuantumSize();
	FSListenPort = GetListenPort(0, (void *)0, 0);

	if (FSListenPort
	&& !strncmp(SLSEntity->type.str, "AFPServer", SLSEntity->type.len)) {
		if ((fs_conn_descr = socket(PF_INET, SOCK_STREAM, 0)) == -1) {
			*SPError = ASPERR_NoSuchDevice;
			goto l_error;
		}

		/*
		 * set sizes of the send/recv windows
		 */
		on = 1;
		ws = 0x10000;
		setsockopt(fs_conn_descr, SOL_SOCKET,SO_SNDBUF,(void *)&ws,sizeof(ws));
		setsockopt(fs_conn_descr, SOL_SOCKET,SO_RCVBUF,(char *)&ws,sizeof(ws));
		setsockopt(fs_conn_descr, SOL_SOCKET,SO_REUSEADDR,(void *)&on,sizeof(on));

		/*
		 * bind socket to the well-known assigned port 
		 */
		memset((char *)&in_addr, 0, sizeof(in_addr));
		fs_port = FSListenPort;
		in_addr.sin_family = PF_INET;
		in_addr.sin_port = fs_port;
		if (bind(fs_conn_descr, (void *)&in_addr, sizeof(in_addr)) == -1) {
			if (errno == EADDRINUSE) {
				/*
				 * failed to use the well-known assigned
				 * port because someone is already using it,
				 * use a dynamic port instead.
				 */
				int in_addrlen = sizeof(in_addr);
				in_addr.sin_port = 0;
				if (bind(fs_conn_descr, (void *)&in_addr, sizeof(in_addr)) == -1) {
					*SPError = ASPERR_BindErr;
					goto l_error;
				}
				if (getsockname(fs_conn_descr, (void *)&in_addr, &in_addrlen) == -1) {
					*SPError = ASPERR_SystemErr;
					goto l_error;
				}
				fs_port = in_addr.sin_port;
			} else {
				*SPError = ASPERR_BindErr;
				goto l_error;
			}
		}

		/*
		 * enable accepting TCP connections
		 */
		if (listen(fs_conn_descr, 1) == -1) {
			*SPError = ASPERR_NoMoreSessions;
			goto l_error;
		}
		LOG_MSG(rn, fs_conn_descr);
	}

	/* find unused listener entry */
	if (!(lp = find_lsock(0))) {
	        *SPError = ASPERR_NoMoreSessions;
		goto l_error;
	}

	descr = ATsocket(ATPROTO_ASP);
	if (descr == -1) {
		*SPError = ASPERR_NoSuchDevice;
		goto l_error;
	}

	ic_len = sizeof(at_inet_t);
	if (at_send_to_dev(descr, ASPIOC_ListenerBind,
			(char *) SLSEntityIdentifier, &ic_len) == -1) {
		*SPError = ASPERR_NoMoreSessions;
		goto l_error;
	}

	if (nbp_register(SLSEntity, descr, Retry) == -1) {
		*SPError = ASPERR_RegisterErr;
		goto l_error;
	}

	if (!(sp = createSess(descr, SPError))) {
		goto l_error;
	}
	sp->SessType = ASPDT_ATP_AS;
	lp->AT_addr = *SLSEntityIdentifier;
	lp->SLSRefNum = descr;
	if (fs_conn_descr != -1) {
		/* XXX suspicious of using fs_port before it's initialized */
		assert(fs_port != 0xdead);
		lp->IN_port = fs_port;
		lp->IN_descr = fs_conn_descr;
		if (!(sp = createSess(fs_conn_descr, SPError))) {
			goto l_error;
		}
		sp->SessType = ASPDT_TCP_FS;
	}

	*SPError = ASPERR_NoError;
	LOG_MSG(rn, descr);
	return 0;

l_error:
	if (fs_conn_descr != -1)
		close(fs_conn_descr);
	if (descr != -1)
		close(descr);
	LOG_ERR(rn);
	return -1;
}

/*
 * Name: SPRemove (server only, supplemental call)
 */
int
SPRemove(at_entity_t *SLSEntity,
	at_inet_t *SLSEntityIdentifier,
	int *SPError)
{
	listener_t *lp;
#ifdef ASPDEBUG
	char *rn = "SPRemove";
#endif

	if (!(lp = find_lsock(SLSEntityIdentifier->socket))) {
		SET_ERRNO(ENOENT);
		*SPError = ASPERR_NoSuchEntity;
		LOG_ERR(rn);
		return -1;
	}
	
	if ((nbp_remove(SLSEntity, lp->SLSRefNum) == -1)) {
		SET_ERRNO(ENOENT);
		*SPError = ASPERR_NoSuchEntity;
		LOG_ERR(rn);
		// just continue and close the socket
	}

	LOG_MSG(rn, lp->SLSRefNum);
	close(lp->SLSRefNum);
	lp->AT_addr.socket = 0;
	lp->SLSRefNum = 0;

	if (FSListenPort && lp->IN_descr) {
		LOG_MSG(rn, lp->IN_descr);
		close(lp->IN_descr);
		lp->IN_descr = 0;
	}

	*SPError = ASPERR_NoError;
	return 0;
} /* SPRemove */

/*
 * Name: SPConfigure (client & server, supplemental call)
 */
void
SPConfigure(unsigned short TickleInterval,
	unsigned short SessionTimer,
	at_retry_t *Retry)
{
#ifdef ASPDEBUG
	char *rn = "SPConfigure";
#endif

	LOG_MSG(rn, 0);
	if (TickleInterval)
		DefTickleInterval = TickleInterval;
	if (SessionTimer)
		DefSessionTimer = SessionTimer;
	if (Retry)
		CmdRetry = *Retry;
}

/*
 * Name: SPGetLocEntity (client & server, supplemental call)
 */
int
SPGetLocEntity(int SessRefNum,
	void *SessLocEntityIdentifier,

	int *SPError)
{
	int ic_len;
	session_t *sp;
#ifdef ASPDEBUG
	char *rn = "SPGetLocEntity";
#endif

	if (!(sp = readSess(SessRefNum, SPError))) {
		LOG_ERR(rn);
		return -1;
	}
	if (sp->SessType == ASPDT_TCP_FS) {
		struct sockaddr_in in_addr;
		int in_addrlen = sizeof(in_addr);

		if (getsockname(SessRefNum, (void *)&in_addr, &in_addrlen) == -1) {
			*SPError = ASPERR_SystemErr;
			LOG_ERR(rn);
			return -1;
		}
		*(unsigned long *)SessLocEntityIdentifier = in_addr.sin_addr.s_addr;
	} else {

		ic_len = sizeof(at_inet_t);
		if (at_send_to_dev(SessRefNum, ASPIOC_GetLocEntity,
			SessLocEntityIdentifier, &ic_len) == -1) {
				if (errno == EINVAL)
					*SPError = ASPERR_ParamErr;
				else
					*SPError = ASPERR_SystemErr;
				LOG_ERR(rn);
				return -1;
		}
	}

	*SPError = ASPERR_NoError;
	LOG_MSG(rn, SessRefNum);
	return 0;
}

/*
 * Name: SPGetRemEntity (client & server, supplemental call)
 */
int
SPGetRemEntity(int SessRefNum,
	void *SessRemEntityIdentifier,
	int *SPError)
{
	int ic_len;
	session_t *sp;
#ifdef ASPDEBUG
	char *rn = "SPGetRemEntity";
#endif

	if (!(sp = readSess(SessRefNum, SPError))) {
		LOG_ERR(rn);
		return -1;
	}
	if (sp->SessType == ASPDT_TCP_FS) {
		struct sockaddr_in in_addr;
		int in_addrlen = sizeof(in_addr);

		if (getpeername(SessRefNum, (void *)&in_addr, &in_addrlen) == -1) {
			*SPError = ASPERR_SystemErr;
			LOG_ERR(rn);
			return -1;
		}
		*(unsigned long *)SessRemEntityIdentifier = in_addr.sin_addr.s_addr;
	} else {

		ic_len = sizeof(at_inet_t);
		if (at_send_to_dev(SessRefNum, ASPIOC_GetRemEntity,
				SessRemEntityIdentifier, &ic_len) == -1) {
			if (errno == EINVAL)
				*SPError = ASPERR_ParamErr;
			else
				*SPError = ASPERR_SystemErr;
			LOG_ERR(rn);
			return -1;
		}
	}

	*SPError = ASPERR_NoError;
	LOG_MSG(rn, SessRefNum);
	return 0;
}

/*
 * Name: SPErrorStr (client & server, supplemental call)
 */
void
SPErrorStr(char *msg,
	int SPError)
{
	char *pMsg;

	if (SPError == 0) {
		fprintf(stderr,MSGSTR(M_NO_ASP_ERR, "%s: no ASP error.\n"), msg);
		return;
	} else if ((SPError > -1058) || (SPError < -1075)) {
		fprintf(stderr,MSGSTR(M_BAD_ASP_ERR,"%s: invalid ASP error\n"), msg);
		return;
	}

	SPError = -(SPError + 1058);
	switch (SPError) {
	case 0:
		pMsg = MSGSTR(M_NO_SUCH_DEV,"No such device");
		break;
	case 1:
		pMsg = MSGSTR(M_ASP_BIND,"Can't bind to an ASP address");
		break;
	case 2:
		pMsg = MSGSTR(M_CMD_REPLY,"Command reply message");
		break;
	case 3:
		pMsg = MSGSTR(M_CMD_REQ,"Command request message");
		break;
	case 4:
		pMsg = MSGSTR(M_SYSTEM_ERR,"System error");
		break;
	case 5:
		pMsg = MSGSTR(M_PROTO_ERR,"Protocol error");
		break;
	case 6:
		pMsg = MSGSTR(M_NO_SUCH_ENTITY,"No such entity");
		break;
	case 7:
		pMsg = MSGSTR(M_CANT_REG,"Can't register the entity");
		break;
	case 8:
		pMsg = MSGSTR(M_BAD_VER,"Version number not supported");
		break;
	case 9:
		pMsg = MSGSTR(M_BUFFER,"Reply buffer cannot hold the whole reply");
		break;
	case 10:
		pMsg = MSGSTR(M_SESSION,"Can't support another session");
		break;
	case 11:
		pMsg = MSGSTR(M_NO_RESP,"Server is not responding");
		break;
	case 12:
		pMsg = MSGSTR(M_REF_NO,"Reference number is unknown");
		break;
	case 13:
		pMsg = MSGSTR(M_BUSY,"Server is busy");
		break;
	case 14:
		pMsg = MSGSTR(M_CLOSED,"Session has been closed");
		break;
	case 15:
		pMsg = MSGSTR(M_TOO_LARGE,"Command block size is too larger");
		break;
	case 16:
		pMsg = MSGSTR(M_FULL,"Can't not support another client");
		break;
	case 17:
		pMsg = MSGSTR(M_NAK,"No acknowledge");
		break;
	default:
		pMsg=NULL;
	}
		
	if (pMsg)
		printf("%s: %s\n", msg, pMsg);
}

/*
 * Name: SPSetPid (client & server, supplemental call)
 */
int
SPSetPid(int SessRefNum,
	int SessPid,
	int *SPError)
{
	int ic_len;
	session_t *sp;
#ifdef ASPDEBUG
	char *rn = "SPSetPid";
#endif

	if (!(sp = readSess(SessRefNum, SPError))) {
		LOG_ERR(rn);
		return -1;
	}
	if (sp->SessType == ASPDT_ATP_AS) {
		ic_len = sizeof(SessPid);
		if (at_send_to_dev(SessRefNum, ASPIOC_SetPid, (char *) &SessPid, &ic_len) == -1) {
			if (errno == EINVAL)
				*SPError = ASPERR_ParamErr;
			else
				*SPError = ASPERR_SystemErr;
			LOG_ERR(rn);
			return -1;
		}
	}

	*SPError = ASPERR_NoError;
	LOG_MSG(rn, SessRefNum);
	return 0;
}

/*
 * Name: SPGetProtoFamily (client & server, supplemental call)
 */
int
SPGetProtoFamily(int SessRefNum,
	int *ProtoFamily,
	int *SPError)
{
	session_t *sp;
#ifdef ASPDEBUG
	char *rn = "SPGetProtoFamily";
#endif

	if (!(sp = readSess(SessRefNum, SPError))) {
		SET_ERRNO(EBADF);
		LOG_ERR(rn);
		return -1;
	}

	*ProtoFamily = (sp->SessType == ASPDT_TCP_FS) ?
		PF_INET : PF_APPLETALK;

	*SPError = ASPERR_NoError;
	LOG_MSG(rn, SessRefNum);
	return 0;
}

/*
 * Name: SendCommand
 *
 */
static int
SendCommand(int SessRefNum,
	char *CmdBlock,
	int CmdBlockSize,
	int *SPError,
	int Primitive)
{
	strbuf_t data;
	strbuf_t ctrl;
	char ctrlbuf[ASPDEF_BufSize];
	session_t *sp;
#ifdef ASPDEBUG
	char *rn = "SendCommand";
#endif

	if (!(sp = readSess(SessRefNum, SPError))) {
		SET_ERRNO(EBADF);
		LOG_ERR(rn);
		return -1;
	}
	if (sp->SessType == ASPDT_TCP_FS) {
		int fspktlen = sizeof(asp_fspkt_t);
		char fspktbuf[sizeof(asp_fspkt_t)+256];
		asp_fspkt_t *fspkt = (asp_fspkt_t *)fspktbuf;

		if (CmdBlockSize > ASPDEF_MaxCmdSize) {
			SET_ERRNO(ERANGE);
			*SPError = ASPERR_SizeErr;
			LOG_ERR(rn);
			return -1;
		}
		fspkt->Flags = 0;
		fspkt->Command = (unsigned char)Primitive;
		fspkt->RequestID = FSRequestID++;
		fspkt->ErrorCode = 0;
		fspkt->TotalLen = CmdBlockSize;
		fspkt->Reserved = 0;
		if (CmdBlockSize <= (sizeof(fspktbuf)-sizeof(*fspkt))) {
			memcpy((char *)(fspkt+1), CmdBlock, CmdBlockSize);
			fspktlen += CmdBlockSize;
			CmdBlockSize = 0;
		}
		if ( (send(SessRefNum, (char *)fspkt, fspktlen,
			   CmdBlockSize ? 0 : MSG_EOR) != fspktlen) ||
		     (CmdBlockSize && 
		      (send(SessRefNum, CmdBlock,
			    CmdBlockSize,MSG_EOR) != CmdBlockSize)) ) {
			*SPError = (errno == EINVAL) ? 
			  ASPERR_ParamErr : ASPERR_SessClosed;
			LOG_ERR(rn);
			return -1;
		}
	} else {

		if (CmdBlockSize > ASPDEF_MaxCmdSize) {
			SET_ERRNO(ERANGE);
			*SPError = ASPERR_SizeErr;
			LOG_ERR(rn);
			return -1;
		}

		/*
		 * send the request
		 */
		((union asp_primitives *)ctrlbuf)->Primitive = Primitive;
		ctrl.len = sizeof(ctrlbuf);
		ctrl.buf = ctrlbuf;
		data.len = CmdBlockSize;
		data.buf = CmdBlock;
		if (ATputmsg(SessRefNum, &ctrl, &data, 1) == -1) {
			if (errno == EINVAL)
				*SPError = ASPERR_ParamErr;
			else if (errno == EAGAIN)
				*SPError = ASPERR_ProtoErr;
			else
				*SPError = ASPERR_SessClosed;
			LOG_ERR(rn);
			return -1;
		}
	}

	*SPError = ASPERR_NoError;
	LOG_MSG(rn, SessRefNum);
	return 0;
} /* SendCommand */


/*
 * Name: GetQuantumSize
 */
static int
GetQuantumSize()
{
	int quantum = ASPDEF_QuantumSize;
	FILE *fp;

	if ((fp = fopen(ASPFS_AFPCfgName, "r")) != 0) {
		char *p, *q;
		char cfgbuf[ASP_LINE_LEN];

		while (!feof(fp)) {
			if (fgets(cfgbuf, sizeof(cfgbuf), fp) == 0)
				break;
			if (cfgbuf[0] == '#')
				continue;
			if ((p = strtok(cfgbuf, "\t \n")) == NULL) /* keyword */
				continue;
			q = strtok(NULL, "\t \n");	/* value */
			if (strpfx(p, "quantum") == 0) {
				quantum = (int)atoi(q);
				break;
			}
		}
	}

	return quantum;
}

/*
 * Name: GetListenPort
 */
static unsigned short
GetListenPort(int flag,
	void *addr,
	char *name)
{
	unsigned short port = 0;
	FILE *fp;

	if ((fp = fopen(ASPFS_AFPCfgName, "r")) != 0) {
		char *p, *q;
		char cfgbuf[ASP_LINE_LEN];

		port = DEFAULT_AFP_PORT;
		while (!feof(fp)) {
			if (fgets(cfgbuf, sizeof(cfgbuf), fp) == 0)
				break;
			if (cfgbuf[0] == '#')
				continue;
			if ((p = strtok(cfgbuf, "\t \n")) == NULL) /* keyword */
				continue;
			q = strtok(NULL, "\t \n");	/* value */
			if (strpfx(p, "disable") == 0) {
				port = 0;
				break;
			}
			if (strpfx(p, "port") == 0)
				port = (unsigned short)atoi(q);
		}
	}

	return port;
}

/*
 * Name: GetTCPInfo
 */
static unsigned char
GetTCPInfo(tcpaddr, port)
	asp_tcpaddr_t *tcpaddr;
	unsigned short port;
{
	unsigned char cnt = 0;
	unsigned long val;
	FILE *fp;

	if ((fp = fopen(ASPFS_AFPCfgName, "r")) != 0) {
		char *p, *q;
		char cfgbuf[ASP_LINE_LEN];

		while (!feof(fp)) {
			if (fgets(cfgbuf, sizeof(cfgbuf), fp) == 0)
				break;
			if (cfgbuf[0] == '#')
				continue;
			if ((p = strtok(cfgbuf, "\t \n")) == NULL) /* keyword */
				continue;
			q = strtok(NULL, "\t \n");	/* value */
			if (strpfx(p, "local") == 0) {
				val = (unsigned long)inet_addr(q);
				tcpaddr->len = 8;
				tcpaddr->addr_tag = 2;
				*(unsigned long *)tcpaddr->addr_value = val;
				tcpaddr->port = port;
				tcpaddr++;
				cnt++;
			}
		}
	}

	if (cnt == 0) { /* no user specified IP addresses? */
		struct hostent *hostent;
		char nambuf[ASP_LINE_LEN];

		if (gethostname(nambuf, sizeof(nambuf)))
			return 0;
		hostent = (struct hostent *)gethostbyname(nambuf);
		for (;  (cnt < 255) && hostent->h_addr_list[cnt]; tcpaddr++, cnt++) {
			val = *(unsigned long *)hostent->h_addr_list[cnt];
			tcpaddr->len = 8;
			tcpaddr->addr_tag = 2;
			*(unsigned long *)tcpaddr->addr_value = val;
			tcpaddr->port = port;
		}
	}

	return cnt;
}

/*
 * Name: GetSigInfo
 */
static void
GetSigInfo(buf)
	char *buf;
{
	static char nambuf[ASP_LINE_LEN];
	memset((char *)buf, 0, ASPFS_SigDataLen);
	gethostname(nambuf, sizeof(nambuf));
	memcpy(buf, ASP_SIG_STR, 3);
	sprintf(&buf[3], "%lx", gethostid());
	memcpy(&buf[11], nambuf, ASPFS_SigDataLen-11);
}

static int
strpfx(pat, str)
	char *pat;
	char *str;
{
	if ((pat == NULL) || (str == NULL) || (*pat == '\0') || (*str == '\0'))
		return 0;
	while (*pat && *str && (*pat == *str)) {
		pat++;
		str++;
	}

	return (int)*pat;
}
