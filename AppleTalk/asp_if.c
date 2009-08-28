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


#include "at_proto.h"

#define ASPDEF_MaxCmdSize 578
#define ASPDEF_QuantumSize 4624

#define ASPERR_SystemErr       -1062


#define SET_ERRNO(e) errno = e

/* 
 * Name: SPAttention (server only)
 */
int
SPAttention(int SessRefNum,
	unsigned short AttentionCode,
	int *SPError,
	int NoWait)
{
	*SPError = ASPERR_SystemErr;
	return -1;
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
	*SPError = ASPERR_SystemErr;
	return -1;
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
	SET_ERRNO(ENXIO);
	*SPError = ASPERR_SystemErr;
	return -1;
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
	/* set the defaults, even if no valid SessRefNum has been provided */
	*MaxCmdSize = ASPDEF_MaxCmdSize;
	*QuantumSize = ASPDEF_QuantumSize;

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
	SET_ERRNO(ENXIO);
	*SPError = ASPERR_SystemErr;
	return -1;
} /* SPGetRequest */

/*
 * Name: SPGetSession (server only)
 */
int
SPGetSession(int SLSRefNum,
	int *SessRefNum,
	int *SPError)
{
	SET_ERRNO(ENXIO);
	*SPError = ASPERR_SystemErr;
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
	SET_ERRNO(ENXIO);
	*SPError = ASPERR_SystemErr;
	return -1;
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
	SET_ERRNO(ENXIO);
	*SPError = ASPERR_SystemErr;
	return -1;

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
	SET_ERRNO(ENXIO);
	*SPError = ASPERR_SystemErr;
	return -1;

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
	SET_ERRNO(ENXIO);
	*SPError = ASPERR_SystemErr;
	return -1;
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
	SET_ERRNO(ENXIO);
	*SPError = ASPERR_SystemErr;
	return -1;
}

/*
 * Name: SPLook (client & server, supplemental call)
 */
int
SPLook(int SessRefNum,
	int *SPError)
{
	SET_ERRNO(ENXIO);
	*SPError = ASPERR_SystemErr;
	return -1;
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
	SET_ERRNO(ENXIO);
	*SPError = ASPERR_SystemErr;
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
	SET_ERRNO(ENXIO);
	*SPError = ASPERR_SystemErr;
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
	SET_ERRNO(ENXIO);
	*SPError = ASPERR_SystemErr;
	return -1;
} /* SPRemove */

/*
 * Name: SPConfigure (client & server, supplemental call)
 */
void
SPConfigure(unsigned short TickleInterval,
	unsigned short SessionTimer,
	at_retry_t *Retry)
{
	return;
}

/*
 * Name: SPGetLocEntity (client & server, supplemental call)
 */
int
SPGetLocEntity(int SessRefNum,
	void *SessLocEntityIdentifier,

	int *SPError)
{
	SET_ERRNO(ENXIO);
	*SPError = ASPERR_SystemErr;
	return -1;
}

/*
 * Name: SPGetRemEntity (client & server, supplemental call)
 */
int
SPGetRemEntity(int SessRefNum,
	void *SessRemEntityIdentifier,
	int *SPError)
{
	SET_ERRNO(ENXIO);
	*SPError = ASPERR_SystemErr;
	return -1;
}



/*
 * Name: SPSetPid (client & server, supplemental call)
 */
int
SPSetPid(int SessRefNum,
	int SessPid,
	int *SPError)
{
	SET_ERRNO(ENXIO);
	*SPError = ASPERR_SystemErr;
	return -1;
}

/*
 * Name: SPGetProtoFamily (client & server, supplemental call)
 */
int
SPGetProtoFamily(int SessRefNum,
	int *ProtoFamily,
	int *SPError)
{
	SET_ERRNO(ENXIO);
	*SPError = ASPERR_SystemErr;
	return -1;
}

