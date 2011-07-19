/*
 * Copyright (c) 2000-2003 Apple Computer, Inc. All rights reserved.
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

/* -----------------------------------------------------------------------------
 *
 *  Theory of operation :
 *
 *  L2TP plugin for vpnd
 *
----------------------------------------------------------------------------- */


/* -----------------------------------------------------------------------------
  Includes
----------------------------------------------------------------------------- */

#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <fcntl.h>
#include <syslog.h>
#include <netdb.h>
#include <pwd.h>
#include <setjmp.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <syslog.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <net/route.h>
#include <pthread.h>
#include <sys/kern_event.h>
#include <sys/sysctl.h>
#include <netinet/in_var.h>
#include <sys/un.h>
#include <CoreFoundation/CFNumber.h>
#include <CoreFoundation/CFBundle.h>
#include <SystemConfiguration/SystemConfiguration.h>

#define APPLE 1

//#include "../L2TP-extension/l2tpk.h"
#include "../../../Helpers/vpnd/ipsec_utils.h"
#include "../../../Helpers/vpnd/vpnplugins.h"
#include "../../../Helpers/vpnd/vpnd.h"
#include "../../../Helpers/vpnd/RASSchemaDefinitions.h"
#include "../../../Helpers/vpnd/cf_utils.h"
#include "l2tp.h"

#include "vpn_control.h"


// ----------------------------------------------------------------------------
//	¥ Private Globals
// ----------------------------------------------------------------------------
#define MAXSECRETLEN	256	/* max length of secret */

static CFBundleRef 		bundle = 0;
static CFBundleRef 		pppbundle = 0;
static int 			listen_sockfd = -1;
static int			opt_noipsec = 0;
static int 			key_preference = -1;
static struct sockaddr_in 	listen_address;
static struct sockaddr_in 	our_address;
static struct sockaddr_in 	any_address;
static int			debug = 0;
static int			racoon_sockfd = -1;
static int			sick_timeleft = 0;
static int			ping_timeleft = 0;
static int			racoon_ping_seed = 0;
#define IPSEC_SICK_TIME 60 //seconds
#define IPSEC_PING_TIME 5 //seconds
#define IPSEC_REPAIR_TIME 10 //seconds

static CFMutableDictionaryRef	ipsec_dict = NULL;
static CFMutableDictionaryRef	ipsec_settings = NULL;

int l2tpvpn_get_pppd_args(struct vpn_params *params, int reload);
int l2tpvpn_health_check(int *outfd, int event);
int l2tpvpn_lb_redirect(struct in_addr *cluster_addr, struct in_addr *redirect_addr);
int l2tpvpn_listen(void);
int l2tpvpn_accept(void);
int l2tpvpn_refuse(void);
void l2tpvpn_close(void);

static u_long load_kext(char *kext, int byBundleID);

/* -----------------------------------------------------------------------------
plugin entry point, called by vpnd
ref is the vpn bundle reference
pppref is the ppp bundle reference
bundles can be layout in two different ways
- As simple vpn bundles (bundle.vpn). the bundle contains the vpn bundle binary.
- As full ppp bundles (bundle.ppp). The bundle contains the ppp bundle binary, 
and also the vpn kext and the vpn bundle binary in its Plugins directory.
if a simple vpn bundle was used, pppref will be NULL.
if a ppp bundle was used, the vpn plugin will be able to get access to the 
Plugins directory and load the vpn kext.
----------------------------------------------------------------------------- */
int start(struct vpn_channel* the_vpn_channel, CFBundleRef ref, CFBundleRef pppref, int debug_mode, int log_verbose)
{
    char 	name[MAXPATHLEN]; 
    CFURLRef	url;
    size_t		len; 
	int			nb_cpu = 1, nb_threads = 0;

    debug = debug_mode;
    
    /* first load the kext if we are loaded as part of a ppp bundle */
    if (pppref) {
        while ((listen_sockfd = socket(PF_PPP, SOCK_DGRAM, PPPPROTO_L2TP)) < 0)
            if (errno != EINTR)
                break;
        if (listen_sockfd < 0) {
            vpnlog(LOG_DEBUG, "L2TP plugin: first call to socket failed - attempting to load kext\n");
            if (url = CFBundleCopyBundleURL(pppref)) {
                name[0] = 0;
                CFURLGetFileSystemRepresentation(url, 0, (UInt8 *)name, MAXPATHLEN - 1);
                CFRelease(url);
                strlcat(name, "/", sizeof(name));
                if (url = CFBundleCopyBuiltInPlugInsURL(pppref)) {
                    CFURLGetFileSystemRepresentation(url, 0, (UInt8 *)(name + strlen(name)), 
                                MAXPATHLEN - strlen(name) - strlen(L2TP_NKE) - 1);
                    CFRelease(url);
                    strlcat(name, "/", sizeof(name));
                    strlcat(name, L2TP_NKE, sizeof(name));
#if !TARGET_OS_EMBEDDED // This file is not built for Embedded
                    if (!load_kext(name, 0))
#else
                    if (!load_kext(L2TP_NKE_ID, 1))
#endif
                        while ((listen_sockfd = socket(PF_PPP, SOCK_DGRAM, PPPPROTO_L2TP)) < 0)
                            if (errno != EINTR)
                                break;
                }	
            }
            if (listen_sockfd < 0) {
                vpnlog(LOG_ERR, "L2TP plugin: Unable to load L2TP kernel extension\n");
                return -1;
            }
        }
    }
    
#if !TARGET_OS_EMBEDDED // This file is not built for Embedded
	/* increase the number of threads for l2tp to nb cpus - 1 */
    len = sizeof(int); 
	sysctlbyname("hw.ncpu", &nb_cpu, &len, NULL, 0);
    if (nb_cpu > 1) {
		sysctlbyname("net.ppp.l2tp.nb_threads", &nb_threads, &len, 0, 0);
		if (nb_threads < (nb_cpu - 1)) {
			nb_threads = nb_cpu - 1;
			sysctlbyname("net.ppp.l2tp.nb_threads", 0, 0, &nb_threads, sizeof(int));
		}
	}
#endif

    /* retain reference */
    bundle = ref;
    CFRetain(bundle);
    
    pppbundle = pppref;
    if (pppbundle)
        CFRetain(pppbundle);
            
    // hookup our socket handlers
    bzero(the_vpn_channel, sizeof(struct vpn_channel));
    the_vpn_channel->get_pppd_args = l2tpvpn_get_pppd_args;
    the_vpn_channel->listen = l2tpvpn_listen;
    the_vpn_channel->accept = l2tpvpn_accept;
    the_vpn_channel->refuse = l2tpvpn_refuse;
    the_vpn_channel->close = l2tpvpn_close;
    the_vpn_channel->health_check = l2tpvpn_health_check;
    the_vpn_channel->lb_redirect = l2tpvpn_lb_redirect;

    return 0;
}

/* ----------------------------------------------------------------------------- 
    l2tpvpn_get_pppd_args
----------------------------------------------------------------------------- */
int l2tpvpn_get_pppd_args(struct vpn_params *params, int reload)
{

    CFStringRef	string;
    int		noipsec = 0;
    CFMutableDictionaryRef	dict = NULL;
	
    if (reload) {
        noipsec = opt_noipsec;
    }
    
    if (params->serverRef) {			
        /* arguments from the preferences file */
        addstrparam(params->exec_args, &params->next_arg_index, "l2tpmode", "answer");

        string = get_cfstr_option(params->serverRef, kRASEntL2TP, kRASPropL2TPTransport);
        if (string && CFEqual(string, kRASValL2TPTransportIP)) {
            addparam(params->exec_args, &params->next_arg_index, "l2tpnoipsec");
            opt_noipsec = 1;
        }
			
		dict = (CFMutableDictionaryRef)CFDictionaryGetValue(params->serverRef, kRASEntIPSec);
		if (isDictionary(dict)) {
			/* get the parameters from the IPSec dictionary */
			dict = CFDictionaryCreateMutableCopy(0, 0, dict);
		}
		else {
			/* get the parameters from the L2TP dictionary */
			dict = CFDictionaryCreateMutable(0, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
			
			string = get_cfstr_option(params->serverRef, kRASEntL2TP, kRASPropL2TPIPSecSharedSecretEncryption);
			if (isString(string))
				CFDictionarySetValue(dict, kRASPropIPSecSharedSecretEncryption, string);

			string = get_cfstr_option(params->serverRef, kRASEntL2TP, kRASPropL2TPIPSecSharedSecret);
			if (isString(string))
				CFDictionarySetValue(dict, kRASPropIPSecSharedSecret, string);
		}

    } else {
        /* arguments from command line */
        if (opt_noipsec)
            addparam(params->exec_args, &params->next_arg_index, "l2tpnoipsec");
    }
    
    if (reload) {
        if (noipsec != opt_noipsec ||
            !CFEqual(dict, ipsec_settings)) {
				vpnlog(LOG_ERR, "reload prefs - IPSec shared secret cannot be changed\n");
				if (dict)
					CFRelease(dict);
				return -1;
		}
    }

	if (ipsec_settings) 
		CFRelease(ipsec_settings);
	ipsec_settings = dict;
	
    return 0;
}

/* ----------------------------------------------------------------------------- 
    l2tpvpn_health_check
----------------------------------------------------------------------------- */
int l2tpvpn_health_check(int *outfd, int event)
{

	size_t				size;
	struct	sockaddr_un sun;
	int					ret = -1, flags;
	
	char				data[256];
	struct vpnctl_hdr			*hdr = (struct vpnctl_hdr *)data;
	
	switch (event) {
		
		case 0: // periodic check
			
			// no ipsec, no need for health check
			if (opt_noipsec) {
				*outfd = -1;
				break;
			}
	
			if (sick_timeleft) {
				sick_timeleft--;
				if (sick_timeleft == 0)
					goto fail;
			}

			// racoon socket is already opened, just query racoon
			if (racoon_sockfd != -1) {

				if (ping_timeleft) {
					ping_timeleft--;
					if (ping_timeleft == 0) {
						// error on racoon socket. racoon exited ?
						ret = -2; // L2TP is sick, but don't die yet, give it 60 seconds to recover
						sick_timeleft = IPSEC_SICK_TIME;				
						goto fail;
					}
				}
				else {
					// query racoon here
					bzero(hdr, sizeof(struct vpnctl_hdr));
					hdr->msg_type = htons(VPNCTL_CMD_PING);
					hdr->cookie = htonl(++racoon_ping_seed);
					ping_timeleft = IPSEC_PING_TIME;		// give few seconds to get a reply
					writen(racoon_sockfd, hdr, sizeof(struct vpnctl_hdr));
				}
				break;
			}
	
			// attempt to kill and restart racoon every 10 seconds
			if ((sick_timeleft % IPSEC_REPAIR_TIME) == 0) {
				vpnlog(LOG_ERR, "IPSecSelfRepair\n");
				IPSecSelfRepair();
			}
	
			// racoon socket is not yet opened, so opened it
			/* open the racoon control socket  */
			racoon_sockfd = socket(PF_LOCAL, SOCK_STREAM, 0);
			if (racoon_sockfd < 0) {
				vpnlog(LOG_ERR, "Unable to create racoon control socket (errno = %d)\n", errno);
				goto fail;
			}

			bzero(&sun, sizeof(sun));
			sun.sun_family = AF_LOCAL;
			strncpy(sun.sun_path, "/var/run/vpncontrol.sock", sizeof(sun.sun_path));

			if (connect(racoon_sockfd,  (struct sockaddr *)&sun, sizeof(sun)) < 0) {
				vpnlog(LOG_ERR, "Unable to connect racoon control socket (errno = %d)\n", errno);
				ret = -2;
				goto fail;
			}

			if ((flags = fcntl(racoon_sockfd, F_GETFL)) == -1
				|| fcntl(racoon_sockfd, F_SETFL, flags | O_NONBLOCK) == -1) {
				vpnlog(LOG_ERR, "Unable to set racoon control socket in non-blocking mode (errno = %d)\n", errno);
				ret = -2;
				goto fail;
			}
				
			*outfd = racoon_sockfd;
			sick_timeleft = 0;
			ping_timeleft = 0;
			break;
			
		case 1: // event on racoon fd
			size = recvfrom (racoon_sockfd, data, sizeof(struct vpnctl_hdr), 0, 0, 0);
			if (size == 0) {
				// error on racoon socket. racoon exited ?
				ret = -2; // L2TP is sick, but don't die yet, give it 60 seconds to recover
				sick_timeleft = IPSEC_SICK_TIME;
				ping_timeleft = 0;
				goto fail;
			}
			
			/* read end of packet */
			if (ntohs(hdr->len)) {
				size = recvfrom (racoon_sockfd, data + sizeof(struct vpnctl_hdr), ntohs(hdr->len), 0, 0, 0);
				if (size == 0) {
					// error on racoon socket. racoon exited ?
					ret = -2; // L2TP is sick, but don't die yet, give it 60 seconds to recover
					sick_timeleft = IPSEC_SICK_TIME;
					ping_timeleft = 0;
					goto fail;
				}
			}
			
			switch (ntohs(hdr->msg_type)) {
			
				case VPNCTL_CMD_PING:
				
					if (racoon_ping_seed == ntohl(hdr->cookie)) {
						// good !
						ping_timeleft = 0;
					//vpnlog(LOG_DEBUG, "receive racoon PING REPLY cookie %d\n", ntohl(hdr->cookie));
					}
					break;
					
				default:	
					/* ignore other messages */
					//vpnlog(LOG_DEBUG, "receive racoon message type %d, result %d\n", ntohs(hdr->msg_type), ntohs(hdr->result));
					break;

			}
			break;
		
	}

	return 0;

fail:
	if (racoon_sockfd != -1) {
		close(racoon_sockfd);
		racoon_sockfd = -1;
		*outfd = -1;
	}
	return ret;
}

// ----------------------------------------------------------------------------
//	notify racoon of the new redirection
// ----------------------------------------------------------------------------
int l2tpvpn_lb_redirect(struct in_addr *cluster_addr, struct in_addr *redirect_addr) 
{

	if (racoon_sockfd == -1) {
		return -1;
	}
	
	struct vpnctl_cmd_redirect msg;
	bzero(&msg, sizeof(msg));
	msg.hdr.len = htons(sizeof(msg) - sizeof(msg.hdr));
	msg.hdr.msg_type = htons(VPNCTL_CMD_REDIRECT);
	msg.address = cluster_addr->s_addr;
	msg.redirect_address = redirect_addr->s_addr;
	msg.force = htons(1);
	writen(racoon_sockfd, &msg, sizeof(msg));
	return 0;
}

/* ----------------------------------------------------------------------------- 
    system call wrappers
----------------------------------------------------------------------------- */
int l2tp_sys_getsockopt(int sockfd, int level, int optname, void *optval, socklen_t *optlen)
{
    while (getsockopt(sockfd, level, optname, optval, optlen) < 0)
        if (errno != EINTR) {
            vpnlog(LOG_ERR, "L2TP plugin: error calling getsockopt for option %d (%s)\n", optname, strerror(errno));
            return -1;
        }
    return 0;
}

int l2tp_sys_setsockopt(int sockfd, int level, int optname, const void *optval, socklen_t optlen)
{
    while (setsockopt(sockfd, level, optname, optval, optlen) < 0)
        if (errno != EINTR) {
            vpnlog(LOG_ERR, "L2TP plugin: error calling setsockopt for option %d (%s)\n", optname, strerror(errno));
            return -1;
        }
    return 0;
}

int l2tp_sys_recvfrom(int sockfd, void *buff, size_t nbytes, int flags,
            struct sockaddr *from, socklen_t *addrlen)
{
    while (recvfrom(sockfd, buff, nbytes, flags, from, addrlen) < 0)
        if (errno != EINTR) {
            vpnlog(LOG_ERR, "L2TP plugin: error calling recvfrom = %s\n", strerror(errno));
            return -1;
        }
    return 0;
}

/* -----------------------------------------------------------------------------
    set_flag  
----------------------------------------------------------------------------- */
int set_flag(int fd, int set, u_int32_t flag)
{
    socklen_t 	optlen;
    u_int32_t	flags;
    
    optlen = 4;
    if (l2tp_sys_getsockopt(fd, PPPPROTO_L2TP, L2TP_OPT_FLAGS, &flags, &optlen) < 0) 
        return -1;

    flags = set ? (flags | flag) : (flags & ~flag);
    if (l2tp_sys_setsockopt(fd, PPPPROTO_L2TP, L2TP_OPT_FLAGS, &flags, 4) < 0)
        return -1;

    return 0;
}

/* -----------------------------------------------------------------------------
    closeall
----------------------------------------------------------------------------- */
static void closeall()
{
    int i;

    for (i = getdtablesize() - 1; i >= 0; i--) close(i);
    open("/dev/null", O_RDWR, 0);
    dup(0);
    dup(0);
    return;
}


/* -----------------------------------------------------------------------------
    load_kext
----------------------------------------------------------------------------- */
u_long load_kext(char *kext, int byBundleID)
{
    int pid;

    if ((pid = fork()) < 0)
        return 1;

    if (pid == 0) {
        closeall();
        // PPP kernel extension not loaded, try load it...
		if (byBundleID)
			execle("/sbin/kextload", "kextload", "-b", kext, (char *)0, (char *)0);
		else
			execle("/sbin/kextload", "kextload", kext, (char *)0, (char *)0);
        exit(1);
    }

    while (waitpid(pid, 0, 0) < 0) {
        if (errno == EINTR)
            continue;
       return 1;
    }
    return 0;
}

/* ----------------------------------------------------------------------------- 
----------------------------------------------------------------------------- */
int l2tp_set_ouraddress(int fd, struct sockaddr *addr)
{
    socklen_t optlen;

    setsockopt(fd, PPPPROTO_L2TP, L2TP_OPT_OURADDRESS, addr, sizeof(*addr));
    /* get the address to retrieve the actual port used */
    optlen = sizeof(*addr);
    getsockopt(fd, PPPPROTO_L2TP, L2TP_OPT_OURADDRESS, addr, &optlen);
    return 0;
}

/* -----------------------------------------------------------------------------
    l2tpvpn_listen - called by vpnd to setup listening socket
----------------------------------------------------------------------------- */
int l2tpvpn_listen(void)
{
	char *errstr;
        
    if (listen_sockfd <= 0)
        return -1;
    
    //set_flag(listen_sockfd, kerneldebug & 1, L2TP_FLAG_DEBUG);
    set_flag(listen_sockfd, 1, L2TP_FLAG_CONTROL);
    set_flag(listen_sockfd, !opt_noipsec, L2TP_FLAG_IPSEC);

    /* unknown src and dst addresses */
    any_address.sin_len = sizeof(any_address);
    any_address.sin_family = AF_INET;
    any_address.sin_port = 0;
    any_address.sin_addr.s_addr = INADDR_ANY;
    
    /* bind the socket in the kernel with L2TP port */
    listen_address.sin_len = sizeof(listen_address);
    listen_address.sin_family = AF_INET;
    listen_address.sin_port = htons(L2TP_UDP_PORT);
    listen_address.sin_addr.s_addr = INADDR_ANY;
    l2tp_set_ouraddress(listen_sockfd, (struct sockaddr *)&listen_address);
    our_address = listen_address;

    /* add security policies */
    if (!opt_noipsec) { 

		CFStringRef				auth_method;
		CFStringRef				string;
		CFDataRef				data;
		uint32_t						natt_multiple_users;

		/* get authentication method from the IPSec dict */
		auth_method = CFDictionaryGetValue(ipsec_settings, kRASPropIPSecAuthenticationMethod);
		if (!isString(auth_method))
			auth_method = kRASValIPSecAuthenticationMethodSharedSecret;

		/* get setting for nat traversal multiple user support - default is enabled for server */
		GetIntFromDict(ipsec_settings, kRASPropIPSecNattMultipleUsersEnabled, &natt_multiple_users, 1);
			
		ipsec_dict = IPSecCreateL2TPDefaultConfiguration(
			(struct sockaddr *)&our_address, (struct sockaddr *)&any_address, NULL, 
			auth_method, 0, natt_multiple_users, 0); 

		/* set the authentication information */
		if (CFEqual(auth_method, kRASValIPSecAuthenticationMethodSharedSecret)) {
			string = CFDictionaryGetValue(ipsec_settings, kRASPropIPSecSharedSecret);
			if (isString(string)) 
				CFDictionarySetValue(ipsec_dict, kRASPropIPSecSharedSecret, string);
			else if (isData(string) && ((CFDataGetLength((CFDataRef)string) % sizeof(UniChar)) == 0)) {
				CFStringEncoding    encoding;

				data = (CFDataRef)string;
#if     __BIG_ENDIAN__
				encoding = (*(CFDataGetBytePtr(data) + 1) == 0x00) ? kCFStringEncodingUTF16LE : kCFStringEncodingUTF16BE;
#else   // __LITTLE_ENDIAN__
				encoding = (*(CFDataGetBytePtr(data)    ) == 0x00) ? kCFStringEncodingUTF16BE : kCFStringEncodingUTF16LE;
#endif
				string = CFStringCreateWithBytes(NULL, (const UInt8 *)CFDataGetBytePtr(data), CFDataGetLength(data), encoding, FALSE);
				CFDictionarySetValue(ipsec_dict, kRASPropIPSecSharedSecret, string);
				CFRelease(string);
			}
			string = CFDictionaryGetValue(ipsec_settings, kRASPropIPSecSharedSecretEncryption);
			if (isString(string)) 
				CFDictionarySetValue(ipsec_dict, kRASPropIPSecSharedSecretEncryption, string);
		}
		else if (CFEqual(auth_method, kRASValIPSecAuthenticationMethodCertificate)) {
			data = CFDictionaryGetValue(ipsec_settings, kRASPropIPSecLocalCertificate);
			if (isData(data)) 
				CFDictionarySetValue(ipsec_dict, kRASPropIPSecLocalCertificate, data);
		}

		if (IPSecApplyConfiguration(ipsec_dict, &errstr)
			|| IPSecInstallPolicies(ipsec_dict, -1, &errstr)) {
			vpnlog(LOG_ERR, "L2TP plugin: cannot configure secure transport (%s).\n", errstr);
			IPSecRemoveConfiguration(ipsec_dict, &errstr);
			CFRelease(ipsec_dict);
			ipsec_dict = 0;
			return -1;
		}

        /* set IPSec Key management to prefer most recent key */
        if (IPSecSetSecurityAssociationsPreference(&key_preference, 0))
            vpnlog(LOG_ERR, "L2TP plugin: cannot set IPSec Key management preference (error %d)\n", errno);
		
		sick_timeleft = IPSEC_SICK_TIME;
		ping_timeleft = 0;

    }

    return listen_sockfd;
}


/* -----------------------------------------------------------------------------
    l2tpvpn_accept
----------------------------------------------------------------------------- */
int l2tpvpn_accept(void) 
{

    u_int8_t			recv_buf[1500];
    socklen_t				addrlen;
    struct sockaddr_in6		from;
    int 			newSockfd; 
    
    /* we should check if there are too many call from the same IP address 
    in the last xxx minutes, proving a denial of service attack */

    while((newSockfd = socket(PF_PPP, SOCK_DGRAM, PPPPROTO_L2TP)) < 0)
        if (errno != EINTR) {
            vpnlog(LOG_ERR, "L2TP plugin: Unable to open L2TP socket during accept\n");
            return -1;
        }
    
    /* accept the call. it will copy the data to the new socket */
    //set_flag(newSockfd, kerneldebug & 1, L2TP_FLAG_DEBUG);
    setsockopt(newSockfd, PPPPROTO_L2TP, L2TP_OPT_ACCEPT, 0, 0);
    
    /* read the duplicated SCCRQ from the listen socket and ignore for now */
    if (l2tp_sys_recvfrom(listen_sockfd, recv_buf, 1500, MSG_DONTWAIT, (struct sockaddr*)&from, &addrlen) < 0)
        return -1;
    
    return newSockfd;
}


/* -----------------------------------------------------------------------------
    l2tpvpn_refuse - called by vpnd to refuse an incomming connection.
        return values: 	 -1 		error
                         socket#	launch pppd with next server address  
                         0 			handled, do nothing
                        
----------------------------------------------------------------------------- */
int l2tpvpn_refuse(void) 
{
    u_int8_t			recv_buf[1500];
    socklen_t				addrlen;
    struct sockaddr_in6		from;    
    int 			newSockfd; 
    
    /* we should check if there are too many call from the same IP address 
    in the last xxx minutes, proving a denial of service attack */

    /* need t read the packet to empty the socket buffer */
    while((newSockfd = socket(PF_PPP, SOCK_DGRAM, PPPPROTO_L2TP)) < 0)
        if (errno != EINTR) {
            vpnlog(LOG_ERR, "L2TP plugin: Unable to open L2TP socket during refuse\n");
            return -1;
        }
    
    /* accept the call. it will copy the data to the new socket */
    setsockopt(newSockfd, PPPPROTO_L2TP, L2TP_OPT_ACCEPT, 0, 0);
    /* and close it right away */
    close(newSockfd);
    
    /* read the duplicated SCCRQ from the listen socket and ignore for now */
    if (l2tp_sys_recvfrom(listen_sockfd, recv_buf, 1500, MSG_DONTWAIT, (struct sockaddr*)&from, &addrlen) < 0)
        return -1;
    
    return 0;
}

/* -----------------------------------------------------------------------------
    l2tpvpn_close
----------------------------------------------------------------------------- */
void l2tpvpn_close(void)
{

	char *errstr;
	
	if (racoon_sockfd != -1) {
		close(racoon_sockfd);
		racoon_sockfd = -1;
	}

    if (listen_sockfd != -1) {
        while (close(listen_sockfd) < 0)
            if (errno == EINTR)
                continue;
        listen_sockfd = -1;
    }

    /* remove security policies */
    if (ipsec_dict) {            
        IPSecRemoveConfiguration(ipsec_dict, &errstr);
        IPSecRemovePolicies(ipsec_dict, -1, &errstr);     
        /* restore IPSec Key management preference */
        if (IPSecSetSecurityAssociationsPreference(0, key_preference))
            vpnlog(LOG_ERR, "L2TP plugin: cannot reset IPSec Key management preference (error %d)\n", errno);
		CFRelease(ipsec_dict);
		ipsec_dict = NULL;
    }

	if (ipsec_settings) {
		CFRelease(ipsec_settings);
		ipsec_settings = NULL;
	}

}



