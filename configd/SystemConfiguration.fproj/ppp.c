/*
 * Copyright (c) 2001 Apple Computer, Inc. All rights reserved.
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

#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/errno.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <CoreFoundation/CoreFoundation.h>

#include <SystemConfiguration/SystemConfiguration.h>
#include <SystemConfiguration/SCPrivate.h>

#include "ppp_msg.h"
#include "ppp.h"

__private_extern__
int
PPPInit(int *ref)
{
	int			sock;
	int			status;
	struct sockaddr_un	sun;

	sock = socket(AF_LOCAL, SOCK_STREAM, 0);

	bzero(&sun, sizeof(sun));
	sun.sun_family = AF_LOCAL;
	strncpy(sun.sun_path, PPP_PATH, sizeof(sun.sun_path));

	status = connect(sock,  (struct sockaddr *)&sun, sizeof(sun));
	if (status < 0) {
		return errno;
	}

	*ref = sock;
	return 0;
}


__private_extern__
int
PPPDispose(int ref)
{
	if (close(ref) < 0) {
		return errno;
	}
	return 0;
}


__private_extern__
int
PPPExec(int		ref,
	u_long		link,
	u_int32_t	cmd,
	void		*request,
	u_long		requestLen,
	void		**reply,
	u_long		*replyLen)
{
	struct ppp_msg_hdr	msg;
	char			*buf		= NULL;
	ssize_t			n;

	bzero(&msg, sizeof(msg));
	msg.m_type = cmd;
	msg.m_link = link;
	msg.m_len  = ((request != NULL) && (requestLen > 0)) ? requestLen : 0;

	//  send the command
	n = write(ref, &msg, sizeof(msg));
	if (n == -1) {
		SCLog(_sc_verbose, LOG_ERR, CFSTR("PPPExec write() failed: %s"), strerror(errno));
		return errno;
	} else if (n != sizeof(msg)) {
		SCLog(_sc_verbose, LOG_ERR, CFSTR("PPPExec write() failed: wrote=%d"), n);
		return -1;
	}

	if ((request != NULL) && (requestLen > 0)) {
		n = write(ref, request, requestLen);
		if (n == -1) {
			SCLog(_sc_verbose, LOG_ERR, CFSTR("PPPExec write() failed: %s"), strerror(errno));
			return errno;
		} else if (n != requestLen) {
			SCLog(_sc_verbose, LOG_ERR, CFSTR("PPPExec write() failed: wrote=%d"), n);
			return -1;
		}
	}

	// always expect a reply
	n = read(ref, &msg, sizeof(msg));
	if (n == -1) {
		SCLog(_sc_verbose, LOG_ERR, CFSTR("PPPExec read() failed: error=%s"), strerror(errno));
		return errno;
	} else if (n != sizeof(msg)) {
		SCLog(_sc_verbose, LOG_ERR, CFSTR("PPPExec read() failed: insufficent data, read=%d"), n);
		return -1;
	}

	if (msg.m_len) {
		buf = CFAllocatorAllocate(NULL, msg.m_len, 0);
		if (buf) {
			// read reply
			n = read(ref, buf, msg.m_len);
			if (n == -1) {
				SCLog(_sc_verbose, LOG_ERR, CFSTR("PPPExec read() failed: error=%s"), strerror(errno));
				CFAllocatorDeallocate(NULL, buf);
				return errno;
			} else if (n != msg.m_len) {
				SCLog(_sc_verbose, LOG_ERR, CFSTR("PPPExec read() failed: insufficent data, read=%d"), n);
				CFAllocatorDeallocate(NULL, buf);
				return -1;
			}
		}
	}

	if (reply && replyLen) {
		*reply    = buf;
		*replyLen = msg.m_len;
	} else if (buf) {
		// if additional returned data is unwanted
		CFAllocatorDeallocate(NULL, buf);
	}

	return msg.m_result;
}


#ifdef	NOT_NEEDED
int
PPPConnect(int ref, u_long link)
{
	int	status;

	status = PPPExec(ref,
			 link,
			 PPP_CONNECT,
			 NULL,
			 0,
			 NULL,
			 NULL);
	if (status != 0) {
		SCLog(_sc_verbose, LOG_ERR, CFSTR("PPPExec(PPP_CONNECT) failed: status = %d"), status);
		return status;
	}

	return status;
}


int
PPPDisconnect(int ref, u_long link)
{
	int	status;

	status = PPPExec(ref,
			 link,
			 PPP_DISCONNECT,
			 NULL,
			 0,
			 NULL,
			 NULL);
	if (status != 0) {
		SCLog(_sc_verbose, LOG_ERR, CFSTR("PPPExec(PPP_DISCONNECT) failed: status = %d"), status);
		return status;
	}

	return status;
}


int
PPPListen(int ref, u_long link)
{
	int	status;

	status = PPPExec(ref,
			 link,
			 PPP_LISTEN,
			 NULL,
			 0,
			 NULL,
			 NULL);
	if (status != 0) {
		SCLog(_sc_verbose, LOG_ERR, CFSTR("PPPExec(PPP_LISTEN) failed: status = %d"), status);
		return status;
	}

	return status;
}


int
PPPApply(int ref, u_long link)
{
	int	status;

	status = PPPExec(ref,
			 link,
			 PPP_APPLY,
			 NULL,
			 0,
			 NULL,
			 NULL);
	if (status != 0) {
		SCLog(_sc_verbose, LOG_ERR, CFSTR("PPPExec(PPP_APPLY) failed: status = %d"), status);
		return status;
	}

	return status;
}
#endif	/* NOT_NEEDED */


__private_extern__
int
PPPGetNumberOfLinks(int ref, u_long *nLinks)
{
	void	*replyBuf	= NULL;
	u_long	replyBufLen	= 0;
	int	status;

	status = PPPExec(ref,
			    -1,
			    PPP_GETNBLINKS,
			    NULL,
			    0,
			    &replyBuf,
			    &replyBufLen);
	if (status != 0) {
		SCLog(_sc_verbose, LOG_ERR, CFSTR("PPPExec(PPP_GETNBLINKS) failed: status = %d"), status);
		return status;
	}

	*nLinks = (replyBufLen == sizeof(u_long)) ? *(u_long *)replyBuf : 0;
	if (replyBuf)	CFAllocatorDeallocate(NULL, replyBuf);

	return status;
}


__private_extern__
int
PPPGetLinkByIndex(int ref, int index, u_int32_t *link)
{
	u_int32_t	i		= index;
	void		*replyBuf	= NULL;
	u_long		replyBufLen	= 0;
	int		status;

	status = PPPExec(ref,
			    -1,
			    PPP_GETLINKBYINDEX,
			    (void *)&i,
			    sizeof(i),
			    &replyBuf,
			    &replyBufLen);
	if (status != 0) {
		SCLog(_sc_verbose, LOG_ERR, CFSTR("PPPExec(PPP_GETLINKBYINDEX) failed: status = %d"), status);
		return status;
	}

	if (replyBuf && (replyBufLen == sizeof(u_int32_t))) {
		*link = *(u_int32_t *)replyBuf;
	} else {
		status = -2;	/* if not found */
	}
	if (replyBuf)	CFAllocatorDeallocate(NULL, replyBuf);

	return status;
}


__private_extern__
int
PPPGetLinkByServiceID(int ref, CFStringRef serviceID, u_int32_t *link)
{
	int		i;
	u_long		nLinks;
	int		status;
	CFDataRef	sID;

	sID = CFStringCreateExternalRepresentation(NULL,
						   serviceID,
						   kCFStringEncodingMacRoman,
						   0);

	status = PPPGetNumberOfLinks(ref, &nLinks);
	if (status != 0) {
		SCLog(_sc_verbose, LOG_ERR, CFSTR("PPPGetNumberOfLinks() failed: %d"), status);
		goto done;
	}

	status = -2;	/* assume no link */

	for (i=0; i<nLinks; i++) {
		u_int32_t	iLink;
		void		*data	= NULL;
		u_long		dataLen	= 0;

		status = PPPGetLinkByIndex(ref, i, &iLink);
		if (status != 0) {
			SCLog(_sc_verbose, LOG_ERR, CFSTR("PPPGetLinkByIndex() failed: %d"), status);
			goto done;
		}

		status = PPPGetOption(ref,
				      iLink,
				      PPP_OPT_SERVICEID,
				      &data,
				      &dataLen);
		if (status != 0) {
			SCLog(_sc_verbose, LOG_ERR, CFSTR("PPPGetOption(PPP_OPT_SERVICEID) failed: %d"), status);
			goto done;
		}

		if ((dataLen != CFDataGetLength(sID)) ||
		    (strncmp(data, CFDataGetBytePtr(sID), dataLen) != 0)) {
			/* if link not found */
			status = -2;
		}

		CFAllocatorDeallocate(NULL, data);
		if (status == 0) {
			*link = iLink;
			goto done;
		}
	}

    done :

	CFRelease(sID);
	return status;
}


__private_extern__
int
PPPGetOption(int ref, u_long link, u_long option, void **data, u_long *dataLen)
{
	struct ppp_opt_hdr 	opt;
	void			*replyBuf	= NULL;
	u_long			replyBufLen	= 0;
	int			status;

	bzero(&opt, sizeof(opt));
	opt.o_type = option;

	status = PPPExec(ref,
			    link,
			    PPP_GETOPTION,
			    (void *)&opt,
			    sizeof(opt),
			    &replyBuf,
			    &replyBufLen);
	if (status != 0) {
		SCLog(_sc_verbose, LOG_ERR, CFSTR("PPPExec(PPP_GETOPTION) failed: status = %d"), status);
		*data = NULL;
		*dataLen = 0;
		return status;
	}

	if (replyBuf && (replyBufLen > sizeof(struct ppp_opt_hdr))) {
		*dataLen = replyBufLen - sizeof(struct ppp_opt_hdr);
		*data    = CFAllocatorAllocate(NULL, *dataLen, 0);
		bcopy(((struct ppp_opt *)replyBuf)->o_data, *data, *dataLen);
	}
	if (replyBuf)	CFAllocatorDeallocate(NULL, replyBuf);

	return status;
}


#ifdef	NOT_NEEDED
__private_extern__
int
PPPSetOption(int ref, u_long link, u_long option, void *data, u_long dataLen)
{
	void			*buf;
	u_long			bufLen;
	int			status;

	bufLen = sizeof(struct ppp_opt_hdr) + dataLen;
	buf    = CFAllocatorAllocate(NULL, bufLen, 0);

	bzero((struct ppp_opt_hdr *)buf, sizeof(struct ppp_opt_hdr));
	((struct ppp_opt_hdr *)buf)->o_type = option;
	bcopy(data, ((struct ppp_opt *)buf)->o_data, dataLen);

	status = PPPExec(ref,
			 link,
			 PPP_SETOPTION,
			 buf,
			 bufLen,
			 NULL,
			 NULL);
	if (status != 0) {
		SCLog(_sc_verbose, LOG_ERR, CFSTR("PPPExec(PPP_SETOPTION) failed: status = %d"), status);
	}

	CFAllocatorDeallocate(NULL, buf);

	return status;
}
#endif	/* NOT_NEEDED */


__private_extern__
int
PPPStatus(int ref, u_long link, struct ppp_status **stat)
{
	void	*replyBuf	= NULL;
	u_long	replyBufLen	= 0;
	int	status;

	status = PPPExec(ref,
			    link,
			    PPP_STATUS,
			    NULL,
			    0,
			    &replyBuf,
			    &replyBufLen);
	if (status != 0) {
		SCLog(_sc_verbose, LOG_ERR, CFSTR("PPPExec(PPP_STATUS) failed: status = %d"), status);
		return status;
	}

	if (replyBuf && (replyBufLen == sizeof(struct ppp_status))) {
		*stat = (struct ppp_status *)replyBuf;
	} else {
		if (replyBuf)	CFAllocatorDeallocate(NULL, replyBuf);
		*stat = NULL;
		status = -1;
	}

	return status;
}
