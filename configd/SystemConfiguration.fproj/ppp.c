/*
 * Copyright (c) 2000-2003 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

/*
 * Modification History
 *
 * Nov 7, 2002                 Allan Nathanson <ajn@apple.com>
 * - use ServiceID *or* LinkID
 *
 * Oct 25, 2002                 Christophe Allie <callie@apple.com>
 * - use ServiceID instead of LinkID
 *
 * Feb 28, 2002                 Christophe Allie <callie@apple.com>
 * - socket API fixes
 *
 * Feb 10, 2001                 Allan Nathanson <ajn@apple.com>
 * - cleanup API
 *
 * Feb 2000                     Christophe Allie <callie@apple.com>
 * - initial revision
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

#include <ppp/ppp_msg.h>
#include "ppp.h"


static int
readn(int ref, void *data, int len)
{
	int	left	= len;
	int	n;
	void	*p	= data;

	while (left > 0) {
		if ((n = read(ref, p, left)) < 0) {
			if (errno != EINTR) {
				return -1;
			}
			n = 0;
		} else if (n == 0) {
			break; /* EOF */
		}

		left -= n;
		p += n;
	}
	return (len - left);
}


static int
writen(int ref, void *data, int len)
{
	int	left	= len;
	int	n;
	void	*p	= data;

	while (left > 0) {
		if ((n = write(ref, p, left)) <= 0) {
			if (errno != EINTR) {
				return -1;
			}
			n = 0;
		}
		left -= n;
		p += n;
	}
	return len;
}


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


static int
PPPExec(int		ref,
	CFStringRef	serviceID,
	uint32_t	link,
	uint32_t	cmd,
	u_int16_t	flags,
	void		*request,
	uint32_t	requestLen,
	void		**reply,
	uint32_t	*replyLen)
{
	struct ppp_msg_hdr	msg;
	char			*buf		= NULL;
	ssize_t			n;
	CFDataRef		sID		= NULL;

	bzero(&msg, sizeof(msg));

	// first send request, if necessary
	if (cmd) {
		msg.m_type = cmd;
		msg.m_flags = flags;
		if (serviceID) {
			sID = CFStringCreateExternalRepresentation(NULL,
								   serviceID,
								   kCFStringEncodingUTF8,
								   0);
			// serviceID is present, use it
			msg.m_flags |= USE_SERVICEID;
			msg.m_link = CFDataGetLength(sID);
		} else {
			// no service ID, use the requested link
			msg.m_link = link;
		}
		msg.m_len  = ((request != NULL) && (requestLen > 0)) ? requestLen : 0;

		//  send the command
		if (writen(ref, &msg, sizeof(msg)) < 0) {
			SCLog(_sc_verbose, LOG_ERR, CFSTR("PPPExec write() failed: %s"), strerror(errno));
			if (sID) CFRelease(sID);
			return errno;
		}

		if (sID) {
			if (writen(ref, (void *)CFDataGetBytePtr(sID), msg.m_link) < 0) {
				SCLog(_sc_verbose, LOG_ERR, CFSTR("PPPExec write() failed: %s"), strerror(errno));
				CFRelease(sID);
				return errno;
			}
			CFRelease(sID);
		}

		if ((request != NULL) && (requestLen > 0)) {
			if (writen(ref, request, requestLen) < 0) {
				SCLog(_sc_verbose, LOG_ERR, CFSTR("PPPExec write() failed: %s"), strerror(errno));
				return errno;
			}
		}
	}

	// then read replies or incoming message
	n = readn(ref, &msg, sizeof(msg));
	if (n == -1) {
		SCLog(_sc_verbose, LOG_ERR, CFSTR("PPPExec readn() failed: error=%s"), strerror(errno));
		return errno;
	} else if (n != sizeof(msg)) {
		SCLog(_sc_verbose, LOG_ERR, CFSTR("PPPExec readn() failed: insufficent data, read=%d"), n);
		return -1;
	}

	if ((msg.m_flags & USE_SERVICEID) && msg.m_link) {
		buf = CFAllocatorAllocate(NULL, msg.m_link, 0);
		if (buf) {
			// read reply
			n = readn(ref, buf, msg.m_link);
			if (n == -1) {
				SCLog(_sc_verbose, LOG_ERR, CFSTR("PPPExec readn() failed: error=%s"), strerror(errno));
				CFAllocatorDeallocate(NULL, buf);
				return errno;
			} else if (n != (ssize_t)msg.m_link) {
				SCLog(_sc_verbose, LOG_ERR, CFSTR("PPPExec readn() failed: error=%s"), strerror(errno));
				CFAllocatorDeallocate(NULL, buf);
				return -1;
			}
			// buf contains the service id we passed in the request
			CFAllocatorDeallocate(NULL, buf);
			buf = NULL;
		}
	}

	if (msg.m_len) {
		buf = CFAllocatorAllocate(NULL, msg.m_len, 0);
		if (buf) {
			// read reply
			n = readn(ref, buf, msg.m_len);
			if (n == -1) {
				SCLog(_sc_verbose, LOG_ERR, CFSTR("PPPExec readn() failed: error=%s"), strerror(errno));
				CFAllocatorDeallocate(NULL, buf);
				return errno;
			} else if (n != (ssize_t)msg.m_len) {
				SCLog(_sc_verbose, LOG_ERR, CFSTR("PPPExec readn() failed: insufficent data, read=%d"), n);
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


__private_extern__
int
PPPGetLinkByInterface(int ref, char *if_name, uint32_t *link)
{
	void		*replyBuf	= NULL;
	uint32_t	replyBufLen	= 0;
	int		status;

	status = PPPExec(ref,
			 NULL,
			 -1,
			 PPP_GETLINKBYIFNAME,
			 0,
			 (void *)if_name,
			 strlen(if_name),
			 &replyBuf,
			 &replyBufLen);
	if (status != 0) {
		return status;
	}

	if (replyBuf && (replyBufLen == sizeof(uint32_t))) {
		*link = *(uint32_t *)replyBuf;
	} else {
		status = -2;	/* if not found */
	}
	if (replyBuf)	CFAllocatorDeallocate(NULL, replyBuf);

	return status;
}


__private_extern__
int
PPPConnect(int ref, CFStringRef serviceID, uint32_t link, void *data, uint32_t dataLen, int linger)
{
	int	status;

	status = PPPExec(ref,
			 serviceID,
			 link,
			 PPP_CONNECT,
			 CONNECT_ARBITRATED_FLAG + (linger ? 0 : CONNECT_AUTOCLOSE_FLAG),
			 data,
			 dataLen,
			 NULL,
			 NULL);
	return status;
}


__private_extern__
int
PPPDisconnect(int ref, CFStringRef serviceID, uint32_t link, int force)
{
	int	status;

	status = PPPExec(ref,
			 serviceID,
			 link,
			 PPP_DISCONNECT,
			 force ? 0 : DISCONNECT_ARBITRATED_FLAG,
			 NULL,
			 0,
			 NULL,
			 NULL);
	return status;
}


__private_extern__
int
PPPSuspend(int ref, CFStringRef serviceID, uint32_t link)
{
	int	status;

	status = PPPExec(ref,
			 serviceID,
			 link,
			 PPP_SUSPEND,
			 0,
			 NULL,
			 0,
			 NULL,
			 NULL);
	return status;
}


__private_extern__
int
PPPResume(int ref, CFStringRef serviceID, uint32_t link)
{
	int	status;

	status = PPPExec(ref,
			 serviceID,
			 link,
			 PPP_RESUME,
			 0,
			 NULL,
			 0,
			 NULL,
			 NULL);
	return status;
}


__private_extern__
int
PPPGetOption(int ref, CFStringRef serviceID, uint32_t link, uint32_t option, void **data, uint32_t *dataLen)
{
	struct ppp_opt_hdr	opt;
	void			*replyBuf	= NULL;
	uint32_t		replyBufLen	= 0;
	int			status;

	*dataLen = 0;
	*data = NULL;

	bzero(&opt, sizeof(opt));
	opt.o_type = option;

	status = PPPExec(ref,
			 serviceID,
			 link,
			 PPP_GETOPTION,
			 0,
			 (void *)&opt,
			 sizeof(opt),
			 &replyBuf,
			 &replyBufLen);
	if (status != 0) {
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


__private_extern__
int
PPPSetOption(int ref, CFStringRef serviceID, uint32_t link, uint32_t option, void *data, uint32_t dataLen)
{
	void		*buf;
	uint32_t	bufLen;
	int		status;

	bufLen = sizeof(struct ppp_opt_hdr) + dataLen;
	buf    = CFAllocatorAllocate(NULL, bufLen, 0);

	bzero((struct ppp_opt_hdr *)buf, sizeof(struct ppp_opt_hdr));
	((struct ppp_opt_hdr *)buf)->o_type = option;
	bcopy(data, ((struct ppp_opt *)buf)->o_data, dataLen);

	status = PPPExec(ref,
			 serviceID,
			 link,
			 PPP_SETOPTION,
			 0,
			 buf,
			 bufLen,
			 NULL,
			 NULL);

	CFAllocatorDeallocate(NULL, buf);
	return status;
}


__private_extern__
int
PPPGetConnectData(int ref, CFStringRef serviceID, uint32_t link, void **data, uint32_t *dataLen)
{
	int	status;

	*dataLen = 0;
	*data = NULL;

	status = PPPExec(ref,
			 serviceID,
			 link,
			 PPP_GETCONNECTDATA,
			 0,
			 NULL,
			 0,
			 data,
			 dataLen);
	return status;
}


__private_extern__
int
PPPStatus(int ref, CFStringRef serviceID, uint32_t link, struct ppp_status **stat)
{
	void		*replyBuf	= NULL;
	uint32_t	replyBufLen	= 0;
	int		status;

	*stat = NULL;

	status = PPPExec(ref,
			 serviceID,
			 link,
			 PPP_STATUS,
			 0,
			 NULL,
			 0,
			 &replyBuf,
			 &replyBufLen);
	if (status != 0) {
		return status;
	}

	if (replyBuf && (replyBufLen == sizeof(struct ppp_status))) {
		*stat = (struct ppp_status *)replyBuf;
	} else {
		if (replyBuf)	CFAllocatorDeallocate(NULL, replyBuf);
		status = -1;
	}

	return status;
}


__private_extern__
int
PPPExtendedStatus(int ref, CFStringRef serviceID, uint32_t link, void **data, uint32_t *dataLen)
{
	int	status;

	*dataLen = 0;
	*data = NULL;

	status = PPPExec(ref,
			 serviceID,
			 link,
			 PPP_EXTENDEDSTATUS,
			 0,
			 NULL,
			 0,
			 data,
			 dataLen);
	return status;
}


__private_extern__
int
PPPEnableEvents(int ref, CFStringRef serviceID, uint32_t link, u_char enable)
{
	int		status;
	uint32_t	lval	= 2;	// status notifications

	status = PPPExec(ref,
			 serviceID,
			 link,
			 enable ? PPP_ENABLE_EVENT : PPP_DISABLE_EVENT,
			 0,
			 &lval,
			 sizeof(lval),
			 NULL,
			 NULL);
	return status;
}


__private_extern__
int
PPPReadEvent(int ref, uint32_t *event)
{

	*event = PPPExec(ref, NULL, 0, 0, 0, NULL, 0, NULL, NULL);
	return 0;
}


__private_extern__
CFDataRef
PPPSerialize(CFPropertyListRef obj, void **data, uint32_t *dataLen)
{
	CFDataRef		xml;

	xml = CFPropertyListCreateXMLData(NULL, obj);
	if (xml) {
		*data = (void*)CFDataGetBytePtr(xml);
		*dataLen = CFDataGetLength(xml);
	}
	return xml;
}


__private_extern__
CFPropertyListRef
PPPUnserialize(void *data, uint32_t dataLen)
{
	CFDataRef		xml;
	CFStringRef		xmlError;
	CFPropertyListRef	ref	= NULL;

	xml = CFDataCreateWithBytesNoCopy(NULL, data, dataLen, kCFAllocatorNull);
	if (xml) {
		ref = CFPropertyListCreateFromXMLData(NULL,
						      xml,
						      kCFPropertyListImmutable,
						      &xmlError);
		if (!ref) {
			if (xmlError) {
				SCLog(TRUE,
				      LOG_ERR,
				      CFSTR("CFPropertyListCreateFromXMLData() failed: %@"),
				      xmlError);
				CFRelease(xmlError);
			}
		}

		CFRelease(xml);
	}

	return ref;
}
