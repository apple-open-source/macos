/*
 * Copyright (c) 2000, 2001 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
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
	u_int8_t	*serviceid,
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
	if (serviceid) {
            // serviceid is present, use it
            msg.m_flags = USE_SERVICEID;
            msg.m_link = strlen(serviceid);
        }
        else {
            // no service ID, use the default link
            msg.m_link = -1;
        }
	msg.m_type = cmd;
	msg.m_len  = ((request != NULL) && (requestLen > 0)) ? requestLen : 0;

	//  send the command
	if (writen(ref, &msg, sizeof(msg)) < 0) {
                fprintf(stderr, "PPPExec write() failed: %s\n", strerror(errno));
                return errno;
        }

        if (serviceid) {
            if (writen(ref, serviceid, msg.m_link) < 0) {
                fprintf(stderr, "PPPExec write() failed: %s\n", strerror(errno));
                return errno;
            }
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

	if (serviceid && msg.m_link) {
		buf = CFAllocatorAllocate(NULL, msg.m_link, 0);
		if (buf) {
			// read reply
			n = readn(ref, buf, msg.m_link);
			if (n == -1) {
				fprintf(stderr, "PPPExec read() failed: error=%s\n", strerror(errno));
				CFAllocatorDeallocate(NULL, buf);
				return errno;
			} else if (n != msg.m_link) {
				fprintf(stderr, "PPPExec read() failed: insufficent data, read=%d\n", n);
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
PPPConnect(int ref, u_int8_t *serviceid)
{
	int	status;

	status = PPPExec(ref,
			 serviceid,
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
PPPDisconnect(int ref, u_int8_t *serviceid)
{
	int	status;

	status = PPPExec(ref,
			 serviceid,
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
PPPGetOption(int ref, u_int8_t *serviceid, u_int32_t option, void **data, u_int32_t *dataLen)
{
	struct ppp_opt_hdr 	opt;
	void			*replyBuf	= NULL;
	u_int32_t		replyBufLen	= 0;
	int			status;

	bzero(&opt, sizeof(opt));
	opt.o_type = option;

	status = PPPExec(ref,
			    serviceid,
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
PPPSetOption(int ref, u_int8_t *serviceid, u_int32_t option, void *data, u_int32_t dataLen)
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
			 serviceid,
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
PPPStatus(int ref, u_int8_t *serviceid, struct ppp_status **stat)
{
	void		*replyBuf	= NULL;
	u_int32_t	replyBufLen	= 0;
	int		status;

	status = PPPExec(ref,
			    serviceid,
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
PPPEnableEvents(int ref, u_int8_t *serviceid, u_char enable)
{
	int	status;

	status = PPPExec(ref,
			 serviceid,
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
