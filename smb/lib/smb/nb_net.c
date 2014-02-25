/*
 * Copyright (c) 2000, Boris Popov
 * All rights reserved.
 *
 * Portions Copyright (C) 2001 - 2010 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *    This product includes software developed by Boris Popov.
 * 4. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
*/
#include <string.h>

#include <sys/socket.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <netdb.h>
#include <net/if.h>
#include <ifaddrs.h>
#include <SystemConfiguration/SCNetworkConnectionPrivate.h>

#include <netsmb/netbios.h>
#include <netsmb/smb_lib.h>
#include <netsmb/nb_lib.h>
#include "charsets.h"

int getaddrinfo_ipv6(const char *hostname, const char *servname,
                     const struct addrinfo *hints,
                     struct addrinfo **res);

static int SocketUtilsIncrementIfReqIter(UInt8** inIfReqIter, struct ifreq* ifr)
{
    *inIfReqIter += sizeof(ifr->ifr_name) + ifr->ifr_addr.sa_len;
	
    /* If the length of the addr is 0, use the family to determine the addr size */
    if (ifr->ifr_addr.sa_len == 0) {
        switch (ifr->ifr_addr.sa_family) {
		case AF_INET:
			*inIfReqIter += sizeof(struct sockaddr_in);
			break;
		default:
			*inIfReqIter += sizeof(struct sockaddr);
			return FALSE;
        }
    }
    return TRUE;
}

/*
 * Check to see if the AF_INET address is a local address. 
 */
static int IsLocalIPv4Address(uint32_t	addr)
{
    UInt32		kMaxAddrBufferSize = 2048;
    UInt8 		buffer[kMaxAddrBufferSize];
	int			so;
    UInt8* 		ifReqIter = NULL;
	struct ifconf ifc;
	struct ifreq ifreq, *ifr;
	int foundit = FALSE;
	
	if (addr == htonl(INADDR_LOOPBACK)) {
		return TRUE;
	}
	
	if ((so = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
		smb_log_info("%s: socket failed, syserr = %s", 
					 ASL_LEVEL_ERR, __FUNCTION__, strerror(errno));
		return foundit;
	}
	ifc.ifc_len = (int)sizeof (buffer);
    ifc.ifc_buf = (char*) buffer;
	if (ioctl(so, SIOCGIFCONF, (char *)&ifc) < 0) {
		smb_log_info("%s: ioctl (get interface configuration), syserr = %s", 
					 ASL_LEVEL_ERR, __FUNCTION__, strerror(errno));
		goto WeAreDone;
	}
    for (ifReqIter = buffer; ifReqIter < (buffer + ifc.ifc_len);) {
        ifr = (struct ifreq*)((void *)ifReqIter);
        if (!SocketUtilsIncrementIfReqIter(&ifReqIter, ifr)) {
			smb_log_info("%s: SocketUtilsIncrementIfReqIter failed!", 
						 ASL_LEVEL_ERR, __FUNCTION__);
            break;
        }
		ifreq = *ifr;
        if ((ifr->ifr_addr.sa_family != AF_INET) || (strncmp(ifr->ifr_name, "lo", 2) == 0))
			continue;
		
		if (ioctl(so, SIOCGIFFLAGS, (char *)&ifreq) < 0) {
			smb_log_info("%s: SIOCGIFFLAGS ioctl failed, syserr = %s",
						 ASL_LEVEL_ERR, __FUNCTION__, strerror(errno));
			continue;
		}
		if (ifreq.ifr_flags & IFF_UP) {
			struct sockaddr_in *laddr = (struct sockaddr_in *)((void *)&(ifreq.ifr_addr));
			if ((uint32_t)laddr->sin_addr.s_addr == addr) {
				foundit = TRUE;
				break;
			}
		}
	}
WeAreDone:
	(void) close(so);
	return foundit;
}

/*
 * Check to see if the AF_INET6 address is a local address. 
 */
static int IsLocalIPv6Address ( struct sockaddr_in6 *in6)
{
    struct ifaddrs* addr_list, *ifa;
    struct sockaddr_in6 *currAddress;
    
	if (IN6_IS_ADDR_LOOPBACK(&in6->sin6_addr)) {
		return TRUE;
	}
	
	/* Ignore any getifaddrs errors */
	if (getifaddrs(&addr_list)) {
		smb_log_info("%s: getifaddrs failed, syserr = %s",
					 ASL_LEVEL_DEBUG, __FUNCTION__, strerror(errno));
		return FALSE;
	}
	
	for (ifa = addr_list; ifa != NULL; ifa = ifa->ifa_next) {
		if (ifa->ifa_addr->sa_family != AF_INET6)
			continue;
		currAddress = (struct sockaddr_in6 *)((void *)ifa->ifa_addr);
		if (IN6_ARE_ADDR_EQUAL (&currAddress->sin6_addr, &in6->sin6_addr))
			return TRUE;
	}
	freeifaddrs(addr_list); /* release memory */;

    return FALSE;
}


/*
 * Check to see if this is a local address. We allow command line utilities to
 * do a loopback connect. Also if the user supplied the port then assume they
 * know what they are doing and allow the connection.
 */
int isLocalIPAddress(struct sockaddr *addr, uint16_t port, int allowLocalConn)
{
	/* Must be coming from a command line utility let them connect */
	if (allowLocalConn)
		return FALSE;
	/* Always allow loop back connection if the user supplied the port */
	if ((port != NBSS_TCP_PORT_139) && (port != SMB_TCP_PORT_445))
		return FALSE;
	
	if (addr->sa_family == AF_INET) {
		struct sockaddr_in *in = (struct sockaddr_in *)((void *)addr);
		
		if (IsLocalIPv4Address(in->sin_addr.s_addr)) {
			return TRUE;
		}
	} else if (addr->sa_family == AF_INET6) {
		if (IsLocalIPv6Address((struct sockaddr_in6 *)((void *)addr))) {
			return TRUE;
		}
	} else {
		smb_log_info("%s: Unknown address falmily %d?", 
					 ASL_LEVEL_DEBUG, __FUNCTION__, addr->sa_family);
	}

	return FALSE;
}

int getaddrinfo_ipv6(const char *hostname, const char *servname,
                     const struct addrinfo *hints,
                     struct addrinfo **res)
{
	int error;
    size_t len;
    char *temp_name = NULL;

    /* 
     * Note: getaddrinfo() and inet_pton() both will give errors if its
     * an IPv6 address enclosed by brackets. I cant find a way to detect
     * if the address is IPv6 or not if the brackets are present. Thus, the
     * check for '[' at the start and ']' at the end of the string.
     */
    
    len = strnlen(hostname, 1024);  /* assume hostname < 1024 */
    if ((len > 3) && (hostname[0] == '[') && (hostname[len - 1] == ']')) {
        /* Seems to be IPv6 with brackets */
        temp_name = malloc(len);
        
        if (temp_name != NULL) {
            /*
             * Copy string and skip beginning '[' (&hostname[1]) and
             * ending ']' (len - 1)
             */
            strlcpy(temp_name, &hostname[1], len - 1);
            
            /* Try without the [] and return that error */
            error = getaddrinfo (temp_name, servname, hints, res);
            
            free(temp_name);
        }
        else {
            error = ENOMEM;
        }
    }
    else {
        /* Not IPv6, just do getaddrinfo */
        error = getaddrinfo (hostname, servname, hints, res);
    }
    
    return (error);
}

/*
 * Resolve the name and retrieve all address associated with that name.  
 */
int resolvehost(const char *name, CFMutableArrayRef *outAddressArray, char *netbios_name, 
				uint16_t port, int allowLocalConn, int tryBothPorts)
{	
	int error;
	struct addrinfo hints, *res0, *res;
	CFMutableArrayRef addressArray = NULL;
	CFMutableDataRef addressData;
	CFStringRef hostName = NULL;

	/* If we are trying both ports always put port 139 in after port 445 */
	if (tryBothPorts && (port == NBSS_TCP_PORT_139))
		port = SMB_TCP_PORT_445;

	memset (&hints, 0, sizeof (hints));
	hints.ai_family = PF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;
	error = getaddrinfo_ipv6 (name, NULL, &hints, &res0);
	if (error != noErr) {
		hostName = CFStringCreateWithCString(kCFAllocatorDefault, name, kCFStringEncodingUTF8);
		if(hostName != NULL) {
			if (SCNetworkConnectionTriggerOnDemandIfNeeded(hostName, TRUE, 60, 0)) {
				error = getaddrinfo_ipv6 (name, NULL, &hints, &res0);
			}
		}
	}
	if (error) {
		if(hostName != NULL) {
			CFRelease(hostName);
		}
		return (error == EAI_SYSTEM) ? errno : EHOSTUNREACH;
	}
	addressArray = CFArrayCreateMutable(kCFAllocatorSystemDefault, 0, &kCFTypeArrayCallBacks);
	if (!addressArray) {
		error = ENOMEM;
		goto done;
	}
	for (res = res0; res; res = res->ai_next) {
		struct connectAddress conn; 
		
		/* We only support IPv4 or IPv6 */
		if ((res->ai_family != PF_INET6) && (res->ai_family != PF_INET)) {
			smb_log_info("Skipping address for `%s', unknown address family %d", 
						 ASL_LEVEL_DEBUG, name, res->ai_family);
			continue;
		}
		/* Check to make sure we are not connecting to ourself */		
		if (isLocalIPAddress((struct sockaddr *)res->ai_addr, port, allowLocalConn)) {
			smb_log_info("The address for `%s' is a loopback address, not allowed!",
						 ASL_LEVEL_DEBUG, name);
			error = ELOOP;	/* AFP returns ELOOP, so we will do the same */
			goto done; 
		}
		
		/* We don't support port 137 on IPv6 addresses currently? */		
		if ((res->ai_family == PF_INET6) && (port == NBNS_UDP_PORT_137)) {
			smb_log_info("Skipping address of `%s', we don't support port 137 on IPV6 addresses", 
						 ASL_LEVEL_DEBUG, name);
			continue;
		}
		/* We don't support port 139 on IPv6 addresses */		
		if ((res->ai_family == PF_INET6) && (port == NBSS_TCP_PORT_139)) {
			smb_log_info("Skipping address of `%s', we don't support port 139 on IPV6 addresses", 
						 ASL_LEVEL_DEBUG, name);
			continue;
		}
		memset(&conn, 0, sizeof(conn));
		conn.so = -1;	/* Default to socket create failed */
		memcpy(&conn.addr, res->ai_addr, res->ai_addrlen);
		if (res->ai_family == PF_INET6) {
			conn.in6.sin6_port = htons(port);
		} else {
			conn.in4.sin_port = htons(port);
		}
		addressData = CFDataCreateMutable(NULL, 0);
		if (addressData) {
			/* We have a netbios name, we need a netbios sockaddr */
			if ((port == NBSS_TCP_PORT_139) && (netbios_name))
				convertToNetBIOSaddr(&conn.storage, netbios_name);

			CFDataAppendBytes(addressData, (const UInt8 *)&conn, (CFIndex)sizeof(conn));
			CFArrayAppendValue(addressArray, addressData);
			CFRelease(addressData);
		}
		/* We only try both ports with IPv4 */
		if (tryBothPorts && (res->ai_family == PF_INET)) {
			conn.in4.sin_port = htons(NBSS_TCP_PORT_139);
			/* We have a netbios name, we need a netbios sockaddr */
			if (netbios_name)
				convertToNetBIOSaddr(&conn.storage, netbios_name);
			
			addressData = CFDataCreateMutable(NULL, 0);
			if (addressData) {
				CFDataAppendBytes(addressData, (const UInt8 *)&conn, (CFIndex)sizeof(conn));
				CFArrayAppendValue(addressArray, addressData);
				CFRelease(addressData);
			}
		}
	}
	if (CFArrayGetCount(addressArray) == 0) {
		error = EHOSTUNREACH;
		goto done;
	}
	
done:
	freeaddrinfo(res0);
	if (error) {
		if (addressArray)
			CFRelease(addressArray);
		addressArray = NULL;
	}
    if(hostName) {
        CFRelease(hostName);
	}
	*outAddressArray = addressArray;
	return error;
}

/* 
 * Is this a IPv6 Dot name.  
 */
int isIPv6NumericName(const char *name)
{	
	int error;
	struct addrinfo hints, *res0, *res;
	
	memset (&hints, 0, sizeof (hints));
	hints.ai_flags = AI_NUMERICHOST;
	hints.ai_family = PF_INET6;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;
	error = getaddrinfo_ipv6(name, NULL, &hints, &res0);
	if (error) {
		return FALSE;
    }
	
	for (res = res0; res; res = res->ai_next) {
		if (res->ai_family == PF_INET6) {
			freeaddrinfo(res0);
			return TRUE;
		}
	}

	freeaddrinfo(res0);
	return FALSE;
}

int
nb_enum_if(struct nb_ifdesc **iflist, int maxif)
{  
	struct ifconf ifc;
	struct ifreq *ifrqp;
	struct nb_ifdesc *ifd;
	struct in_addr iaddr, imask;
	char *ifrdata, *iname;
	int s, rdlen, error, iflags, i;
	unsigned len;

	*iflist = NULL;
	s = socket(AF_INET, SOCK_DGRAM, 0);
	if (s == -1)
		return errno;

	rdlen = (int)(maxif * sizeof(struct ifreq));
	ifrdata = malloc(rdlen);
	if (ifrdata == NULL) {
		error = ENOMEM;
		goto bad;
	}
	ifc.ifc_len = rdlen;
	ifc.ifc_buf = ifrdata;
	if (ioctl(s, SIOCGIFCONF, &ifc) != 0) {
		error = errno;
		goto bad;
	} 
	ifrqp = ifc.ifc_req;
	error = 0;
	/* freebsd bug: ifreq size is variable - must use _SIZEOF_ADDR_IFREQ */
	for (i = 0; i < ifc.ifc_len;
	     i += len, ifrqp = (struct ifreq *)(void *)((uint8_t *)ifrqp + len)) {
		len = (int)_SIZEOF_ADDR_IFREQ(*ifrqp);
		/* XXX for now, avoid IP6 broadcast performance costs */
		if (ifrqp->ifr_addr.sa_family != AF_INET)
			continue;
		if (ioctl(s, SIOCGIFFLAGS, ifrqp) != 0)
			continue;
		iflags = ifrqp->ifr_flags;
		if ((iflags & IFF_UP) == 0 || (iflags & IFF_BROADCAST) == 0)
			continue;

		if (ioctl(s, SIOCGIFADDR, ifrqp) != 0 ||
		    ifrqp->ifr_addr.sa_family != AF_INET)
			continue;
		iname = ifrqp->ifr_name;
		if (strlen(iname) >= sizeof(ifd->id_name))
			continue;
		iaddr = (*(struct sockaddr_in *)(void *)&ifrqp->ifr_addr).sin_addr;

		if (ioctl(s, SIOCGIFNETMASK, ifrqp) != 0)
			continue;
		imask = ((struct sockaddr_in *)(void *)&ifrqp->ifr_addr)->sin_addr;

		ifd = malloc(sizeof(struct nb_ifdesc));
		if (ifd == NULL)
			return ENOMEM;
		bzero(ifd, sizeof(struct nb_ifdesc));
		strlcpy(ifd->id_name, iname, sizeof(ifd->id_name));
		ifd->id_flags = iflags;
		ifd->id_addr = iaddr;
		ifd->id_mask = imask;
		ifd->id_next = *iflist;
		*iflist = ifd;
	}
bad:
	if (ifrdata)
		free(ifrdata);
	close(s);
	return error;
}  


#define kPollSeconds 5
#define kMaxTimeToWait 60

/* 
 * Get a non blocking socket to be used for the connect. Since a connection
 * failure always returns the same error, we don't worry about errno getting
 * overwritten by the close call. We just use errno for debug purposes here.
 */
static int nonBlockingSocket(int family)
{
	int so, flags;
	
	so = socket(family, SOCK_STREAM, 0);
	if (so < 0) {
		smb_log_info("%s: socket call failed for family %d, syserr = %s",
					 ASL_LEVEL_DEBUG, __FUNCTION__, family, strerror(errno));
		return -1;
	}
	if ( (flags = fcntl(so, F_GETFL, NULL)) < 0 ) {
		smb_log_info("%s: F_GETFL call failed for family %d, syserr = %s",
					 ASL_LEVEL_DEBUG, __FUNCTION__, family, strerror(errno));
		close(so);
		return -1;
	} 
	flags |= O_NONBLOCK; 
	if ( fcntl(so, F_SETFL, flags) < 0 ) { 
		smb_log_info("%s: F_SETFL call failed for sa_family %d, syserr = %s", 
					 ASL_LEVEL_DEBUG, __FUNCTION__, family, strerror(errno));
		close(so);
		return -1;
	} 
	return so;
}

int findReachableAddress(CFMutableArrayRef addressArray, uint16_t *cancel, struct connectAddress **dest)
{
	struct timeval tv;
	int error = 0;
	fd_set writefds;
	int	nfds = 0;
	CFIndex ii, numAddresses =  CFArrayGetCount(addressArray);
	int32_t totalWaitTime = 0;
	CFMutableDataRef dataRef;
	struct connectAddress *conn;
	struct connectAddress *conn_port139 = NULL;
	struct connectAddress *conn_port445 = NULL;
    in_port_t port;
	
	*dest = NULL;
	FD_ZERO(&writefds);
	
	/* Attempt to connect to all addresses non blocking */
	for (ii = 0; ii < numAddresses; ii++) {
		dataRef = (CFMutableDataRef)CFArrayGetValueAtIndex(addressArray, ii);
		if (!dataRef)
			continue;
        
		conn = (struct connectAddress *)((void *)CFDataGetMutableBytePtr(dataRef));
		if (!conn)
			continue;
					
		if ( (cancel) && (*cancel == TRUE) ) {
			smb_log_info("%s: Connection cancelled", ASL_LEVEL_DEBUG, __FUNCTION__);
			error = ECANCELED;
			goto done;
		}
		
		if (conn->addr.sa_family == AF_NETBIOS)
			conn->so = nonBlockingSocket(AF_INET);
		else	
			conn->so = nonBlockingSocket(conn->addr.sa_family);
		
		if (conn->so < 0) {
			/* Socket called failed, so skip this address */
			continue;
		}
        
		/* Connect to the address */
		if (conn->addr.sa_family == AF_NETBIOS) {
			error = connect(conn->so, (struct sockaddr *)&conn->nb.snb_addrin, conn->nb.snb_addrin.sin_len);
        }
		else {
			error = connect(conn->so, &conn->addr, conn->addr.sa_len);
        }
        
		if (error < 0) {
			/* This is a non blocking, so we expect EINPROGRESS */
			if (errno == EINPROGRESS) {
				FD_SET(conn->so, &writefds); /* add socket into set for the select call */
				if (conn->so > nfds)		/* save max fd for select call */
					nfds = conn->so;
			} else {
				/* Connection failed skip this address */
				smb_log_info("%s: Connection %ld failed, family = %d, syserr = %s", 
							 ASL_LEVEL_DEBUG, __FUNCTION__, ii, 
							 conn->addr.sa_family, strerror(errno));
				close (conn->so);
				conn->so = -1;
			}
			continue;
		}
	}
	
	/* Wait for one or more connects to complete */
	while (nfds && (totalWaitTime < kMaxTimeToWait)) { 
		tv.tv_sec = kPollSeconds; 
		tv.tv_usec = 0; 
		error = select(nfds + 1, NULL, &writefds, NULL, &tv);
        
		if (error < 0) {
			/* We treat EAGAIN or EINTR the same as a timeout */
			if ((errno == EAGAIN) || (errno == EINTR)) {
				error = 0;
			} else {
				/* Not sure what went wrong here just get out */
				error = errno;
				smb_log_info("%s: Select call failed, syserr = %s", 
							 ASL_LEVEL_DEBUG, __FUNCTION__, strerror(error));
				goto done;
			}
		}
        
		if (error > 0) {
			/* One or more sockets finished */
			nfds = 0;
			error = 0;
			for (ii = 0; ii < numAddresses; ii++) {
				socklen_t dummy;

				dataRef = (CFMutableDataRef)CFArrayGetValueAtIndex(addressArray, ii);
				if (!dataRef)
					continue;
				
				conn = (struct connectAddress *)((void *)CFDataGetMutableBytePtr(dataRef));
				if (!conn)
					continue;
				
				/* This socket already failed, so skip this connection */
				if (conn->so < 0)
					continue;
				
				if (FD_ISSET(conn->so, &writefds) == 0) {
					/* Connection hasn't completed, so skip it */
					FD_SET (conn->so, &writefds);
					if (conn->so > nfds)
						nfds = conn->so;
					continue;
				}
				
				/* 
				 * See what error came back.  SO_ERROR gives us an exact error 
				 * for why the connect failed 
				 */
				dummy = sizeof(int); 
				if (getsockopt(conn->so, SOL_SOCKET, SO_ERROR, (void*)(&error), &dummy) < 0) {
					error = errno;	/* Handle this below */
					smb_log_info("%s: getsockopt failed, syserr = %s", 
								 ASL_LEVEL_DEBUG, __FUNCTION__, strerror(errno));
				}
                
				if (error) {
					if (error != EINPROGRESS) {
						smb_log_info("%s: Connection failed, syserr = %s", 
									 ASL_LEVEL_DEBUG, __FUNCTION__, strerror(error));
						/* We treat all other errors as a connection failure */
						FD_CLR (conn->so, &writefds);
						close (conn->so);
						conn->so = -1;
					} else {
						FD_SET (conn->so, &writefds);
						if (conn->so > nfds)		/* save max fd for select call */
							nfds = conn->so;
					}
					error = 0;
					continue;
				}
                
				/* 
                 * A connection completed. If its port 445, then we are done.
                 * If its port 139, check the others and see if we have a port 
                 * 445 completed yet. We prefer port 445 over 139.
                 */
                switch (conn->addr.sa_family) {
                    case AF_NETBIOS:
                        port = ntohs(conn->nb.snb_addrin.sin_port);
                        break;
                    case PF_INET6:
                        port = ntohs(conn->in6.sin6_port);
                        break;
                        
                    default:
                        /* Must be IPv4 */
                        port = ntohs(conn->in4.sin_port);
                        break;
                }
                
                if (port == SMB_TCP_PORT_445) {
                    if (conn_port445 == NULL) {
                        /* save the first one that connected */
                        conn_port445 = conn;
                    }
                }
                else {
                    if (conn_port139 == NULL) {
                        /* save the first one that connected */
                        conn_port139 = conn;
                    }
                }

                if (conn_port445 != NULL) {
                    /* Found a port 445, so we are done */
                    goto done;
                }
			}
		} else {
			/* time limit expired */
			totalWaitTime += kPollSeconds;
			
			if ( (cancel) && (*cancel == TRUE) ) {
				smb_log_info("%s: Connection cancelled", ASL_LEVEL_DEBUG, __FUNCTION__);
				error = ECANCELED;
				goto done;
			}
			
            if ((conn_port139 != NULL) && (totalWaitTime > kPollSeconds)) {
                /* 
                 * Found a port 139 connection, we waited one more time to see
                 * if port 445 connection will be found and it was not found
                 * so just use the 139 connection. I dont think this can ever 
                 * happen as port 445 always seems to be supported and found.
                 */
                goto done;
            }

			
            /* we are going to do the select call again, so setup the FD list */
			nfds = 0;
			for (ii = 0; ii < numAddresses; ii++) {
				dataRef = (CFMutableDataRef)CFArrayGetValueAtIndex(addressArray, ii);
				if (!dataRef)
					continue;
				conn = (struct connectAddress *)((void *)CFDataGetMutableBytePtr(dataRef));
				if (!conn)
					continue;
				
				if (conn->so < 0)
					continue;
				
				if (FD_ISSET(conn->so, &writefds) == 0) {
					FD_SET (conn->so, &writefds);
					if (conn->so > nfds)		/* save max fd for select call */
						nfds = conn->so;
				}
			}
		}
	}
	
done:
    if (conn_port445 != NULL) {
        *dest = conn_port445;
    }
    else {
        if (conn_port139 != NULL) {
            smb_log_info("%s: Using port 139 family = %d",
                         ASL_LEVEL_ERR, __FUNCTION__, conn->addr.sa_family);
            *dest = conn_port139;
        }
    }
    
	if (!error && (*dest == NULL))
		error = ETIMEDOUT;
    
	/* close all open sockets */
	for (ii = 0; ii < numAddresses; ii++) {
		dataRef = (CFMutableDataRef)CFArrayGetValueAtIndex(addressArray, ii);
		if (!dataRef)
			continue;
		conn = (struct connectAddress *)((void *)CFDataGetMutableBytePtr(dataRef));
		if (!conn)
			continue;
		
		if (conn->so != -1)
			close (conn->so);
	}
    
	return error;
}
