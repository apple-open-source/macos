/*
 * Copyright (c) 2000, 2001 Apple Computer, Inc. All rights reserved.
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

/*
 * Modification History
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

#include "ppp_msg.h"
#include "ppplib.h"

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
readn(int ref, void *data, int len)
{
    int 	n, left = len;
    void 	*p = data;
    
    while (left > 0) {
        if ((n = read(ref, p, left)) < 0) {
            if (errno != EINTR) 
                return -1;
            n = 0;
        }
        else if (n == 0)
            break; /* EOF */
            
        left -= n;
        p += n;
    }
    return (len - left);
}        

static int
writen(int ref, void *data, int len)
{	
    int 	n, left = len;
    void 	*p = data;
    
    while (left > 0) {
        if ((n = write(ref, p, left)) <= 0) {
            if (errno != EINTR) 
                return -1;
            n = 0;
        }
        left -= n;
        p += n;
    }
    return len;
}        

static int
PPPExec(int		ref,
	u_int32_t	link,
	u_int32_t	cmd,
	void		*request,
	u_int32_t	requestLen,
	void		**reply,
	u_int32_t	*replyLen)
{
	struct ppp_msg_hdr	msg;
	char			*buf		= NULL;
	ssize_t			n;

	bzero(&msg, sizeof(msg));
	msg.m_type = cmd;
	msg.m_link = link;
	msg.m_len  = ((request != NULL) && (requestLen > 0)) ? requestLen : 0;

	//  send the command
	if (writen(ref, &msg, sizeof(msg)) < 0) {
                fprintf(stderr, "PPPExec write() failed: %s\n", strerror(errno));
                return errno;
        }

	if ((request != NULL) && (requestLen > 0)) {
		if (writen(ref, request, requestLen) < 0) {
			fprintf(stderr, "PPPExec write() failed: %s\n", strerror(errno));
			return errno;
		}
	}

	// always expect a reply
	n = readn(ref, &msg, sizeof(msg));
	if (n == -1) {
		fprintf(stderr, "PPPExec read() failed: error=%s\n", strerror(errno));
		return errno;
	} else if (n != sizeof(msg)) {
		fprintf(stderr, "PPPExec read() failed: insufficent data, read=%d\n", n);
		return -1;
	}

	if (msg.m_len) {
		buf = CFAllocatorAllocate(NULL, msg.m_len, 0);
		if (buf) {
			// read reply
			n = readn(ref, buf, msg.m_len);
			if (n == -1) {
				fprintf(stderr, "PPPExec read() failed: error=%s\n", strerror(errno));
				CFAllocatorDeallocate(NULL, buf);
				return errno;
			} else if (n != msg.m_len) {
				fprintf(stderr, "PPPExec read() failed: insufficent data, read=%d\n", n);
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


int
PPPConnect(int ref, u_int32_t link)
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
		fprintf(stderr, "PPPExec(PPP_CONNECT) failed: status = %d\n", status);
		return status;
	}

	return status;
}


int
PPPDisconnect(int ref, u_int32_t link)
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
		fprintf(stderr, "PPPExec(PPP_DISCONNECT) failed: status = %d\n", status);
		return status;
	}

	return status;
}


__private_extern__
int
PPPGetNumberOfLinks(int ref, u_int32_t *nLinks)
{
	void		*replyBuf	= NULL;
	u_int32_t	replyBufLen	= 0;
	int		status;

	status = PPPExec(ref,
			    -1,
			    PPP_GETNBLINKS,
			    NULL,
			    0,
			    &replyBuf,
			    &replyBufLen);
	if (status != 0) {
		fprintf(stderr, "PPPExec(PPP_GETNBLINKS) failed: status = %d\n", status);
		return status;
	}

	*nLinks = (replyBufLen == sizeof(u_long)) ? *(u_long *)replyBuf : 0;
	if (replyBuf)	CFAllocatorDeallocate(NULL, replyBuf);

	return status;
}


__private_extern__
int
PPPGetLinkByIndex(int ref, u_int32_t index, u_int32_t *link)
{
	u_int32_t	i		= index;
	void		*replyBuf	= NULL;
	u_int32_t	replyBufLen	= 0;
	int		status;

	status = PPPExec(ref,
			    -1,
			    PPP_GETLINKBYINDEX,
			    (void *)&i,
			    sizeof(i),
			    &replyBuf,
			    &replyBufLen);
	if (status != 0) {
		fprintf(stderr, "PPPExec(PPP_GETLINKBYINDEX) failed: status = %d\n", status);
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
	u_int32_t	nLinks;
	int		status;
	CFDataRef	sID = NULL;

	if (serviceID == NULL)
		return EINVAL;

	sID = CFStringCreateExternalRepresentation(NULL,
						   serviceID,
						   kCFStringEncodingMacRoman,
						   0);
	if (sID == NULL)
		return ENOMEM;
	 
	status = PPPGetNumberOfLinks(ref, &nLinks);
	if (status != 0) {
		fprintf(stderr, "PPPGetNumberOfLinks() failed: %d\n", status);
		goto done;
	}

	status = -2;	/* assume no link */

	for (i=0; i<nLinks; i++) {
		u_int32_t	iLink;
		void		*data	= NULL;
		u_int32_t	dataLen	= 0;

		status = PPPGetLinkByIndex(ref, i, &iLink);
		if (status != 0) {
			fprintf(stderr, "PPPGetLinkByIndex() failed: %d\n", status);
			goto done;
		}

		status = PPPGetOption(ref,
				      iLink,
				      PPP_OPT_SERVICEID,
				      &data,
				      &dataLen);
		if (status != 0) {
			fprintf(stderr, "PPPGetOption(PPP_OPT_SERVICEID) failed: %d\n", status);
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
PPPGetOption(int ref, u_int32_t link, u_int32_t option, void **data, u_int32_t *dataLen)
{
	struct ppp_opt_hdr 	opt;
	void			*replyBuf	= NULL;
	u_int32_t		replyBufLen	= 0;
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
		fprintf(stderr, "PPPExec(PPP_GETOPTION) failed: status = %d\n", status);
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


__private_extern__
int
PPPSetOption(int ref, u_int32_t link, u_int32_t option, void *data, u_int32_t dataLen)
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
		fprintf(stderr, "PPPExec(PPP_SETOPTION) failed: status = %d\n", status);
	}

	CFAllocatorDeallocate(NULL, buf);

	return status;
}


__private_extern__
int
PPPStatus(int ref, u_int32_t link, struct ppp_status **stat)
{
	void		*replyBuf	= NULL;
	u_int32_t	replyBufLen	= 0;
	int		status;

	status = PPPExec(ref,
			    link,
			    PPP_STATUS,
			    NULL,
			    0,
			    &replyBuf,
			    &replyBufLen);
	if (status != 0) {
		fprintf(stderr, "PPPExec(PPP_STATUS) failed: status = %d\n", status);
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


__private_extern__
int
PPPEnableEvents(int ref, u_int32_t link, u_char enable)
{
	int	status;

	status = PPPExec(ref,
			 link,
			 enable ? PPP_ENABLE_EVENT : PPP_DISABLE_EVENT,
			 NULL,
			 0,
			 NULL,
			 NULL);
	if (status != 0) {
		fprintf(stderr,
		        "PPPExec(%s) failed: status = %d\n",
			enable ? "PPP_ENABLE_EVENT" : "PPP_DISABLE_EVENT",
			status);
		return status;
	}

	return status;
}
