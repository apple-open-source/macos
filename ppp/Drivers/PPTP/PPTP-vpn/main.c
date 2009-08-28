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
 *  PPTP plugin for vpnd
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
#include <netinet/in_var.h>
#include <CoreFoundation/CFNumber.h>
#include <CoreFoundation/CFBundle.h>
#include <SystemConfiguration/SystemConfiguration.h>

#include "../../../Helpers/vpnd/vpnplugins.h"
#include "../../../Helpers/vpnd/vpnd.h"
#include "../PPTP-extension/PPTP.h"
#include "../PPTP-plugin/pptp.h"



// ----------------------------------------------------------------------------
//	¥ Private Globals
// ----------------------------------------------------------------------------
static CFBundleRef 	bundle = 0;
static int 		listen_sockfd = -1;

int pptpvpn_get_pppd_args(struct vpn_params *params, int reload);
int pptpvpn_listen(void);
int pptpvpn_accept(void);
int pptpvpn_refuse(void);
void pptpvpn_close(void);

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
int start(struct vpn_channel* the_vpn_channel, CFBundleRef ref, CFBundleRef pppref, int debug, int log_verbose)
{
    int 	s;
    char 	name[MAXPATHLEN]; 
    CFURLRef	url;

    /* first load the kext if we are loaded as part of a ppp bundle */
    if (pppref) {
        s = socket(PF_PPP, SOCK_DGRAM, PPPPROTO_PPTP);
        if (s < 0) {
            if (url = CFBundleCopyBundleURL(pppref)) {
                name[0] = 0;
                CFURLGetFileSystemRepresentation(url, 0, (UInt8 *)name, MAXPATHLEN - 1);
                CFRelease(url);
                strlcat(name, "/", sizeof(name));
                if (url = CFBundleCopyBuiltInPlugInsURL(pppref)) {
                    CFURLGetFileSystemRepresentation(url, 0, (UInt8 *)(name + strlen(name)), 
                                MAXPATHLEN - strlen(name) - strlen(PPTP_NKE) - 1);
                    CFRelease(url);
                    strlcat(name, "/", sizeof(name));
                    strlcat(name, PPTP_NKE, sizeof(name));
#ifndef TARGET_EMBEDDED_OS
                    if (!load_kext(name, 0))
#else
                    if (!load_kext(PPTP_NKE_ID, 1))
#endif
                        s = socket(PF_PPP, SOCK_DGRAM, PPPPROTO_PPTP);
                }	
            }
            if (s < 0) {
                vpnlog(LOG_ERR, "PPTP plugin: Unable to load PPTP kernel extension\n");
                return -1;
            }
        }
        close (s);
    }
    
    /* retain reference */
    bundle = ref;
    CFRetain(bundle);
         
    // hookup our socket handlers
    bzero(the_vpn_channel, sizeof(struct vpn_channel));
    the_vpn_channel->get_pppd_args = pptpvpn_get_pppd_args;
    the_vpn_channel->listen = pptpvpn_listen;
    the_vpn_channel->accept = pptpvpn_accept;
    the_vpn_channel->refuse = pptpvpn_refuse;
    the_vpn_channel->close = pptpvpn_close;

    return 0;
}

/* ----------------------------------------------------------------------------- 
    pptpvpn_get_pppd_args
----------------------------------------------------------------------------- */
int pptpvpn_get_pppd_args(struct vpn_params *params, int reload)
{
    if (params->serverRef)		
        /* arguments from the preferences file */
        addstrparam(params->exec_args, &params->next_arg_index, "pptpmode", "answer");

    return 0;
}


/* ----------------------------------------------------------------------------- 
    system call wrappers
----------------------------------------------------------------------------- */
int pptp_sys_accept(int sockfd, struct sockaddr *cliaddr, int *addrlen)
{
    int fd;
    
    while ((fd = accept(sockfd, cliaddr, addrlen)) == -1)
        if (errno != EINTR) {
            vpnlog(LOG_ERR, "PPTP plugin: error calling accept = %s\n", strerror(errno));
            return -1;
        }
    return fd;
}

int pptp_sys_close(int sockfd)
{
    while (close(sockfd) == -1)
        if (errno != EINTR) {
            vpnlog(LOG_ERR, "PPTP plugin: error calling close on socket = %s\n", strerror(errno));
            return -1;
        }
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
    pptpvpn_listen()  called by vpnd to setup listening socket
----------------------------------------------------------------------------- */
int pptpvpn_listen(void)
{

    struct sockaddr_in	addrListener;    
    int			val;
    
    // Create the requested socket
    while ((listen_sockfd = socket (AF_INET, SOCK_STREAM, 0)) < 0) 
        if (errno != EINTR) {
            vpnlog(LOG_ERR, "PPTP plugin: Could not create socket - err = %s\n", strerror(errno));
            return -1 ;
    }
    
    // Don't want children to have a copy of this.
//    while (fcntl(listen_sockfd, F_SETFD, 1) == -1)
//        if (errno != EINTR) {
//            syslog(LOG_ERR, "VPND PPTP plugin: error calling fcntl = %s\n", strerror(errno));
//            return -1;
//        }

    addrListener.sin_family = AF_INET;
    addrListener.sin_addr.s_addr = htonl(INADDR_ANY);
    addrListener.sin_port = htons(PPTP_TCP_PORT);

    val = 1;
    setsockopt(listen_sockfd, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val));
    
    while (bind(listen_sockfd, (struct sockaddr *) &addrListener, sizeof (addrListener)) < 0) 
        if (errno != EINTR) {
            vpnlog(LOG_ERR, "PPTP plugin: Unable to bind socket to port %d - err = %s\n", 
                        PPTP_TCP_PORT, strerror(errno));
            return -1;
        }

    while (listen(listen_sockfd, SOMAXCONN) < 0) 
        if (errno == EINTR) {
            vpnlog(LOG_ERR, "PPTP plugin: error calling listen = %s\n", strerror(errno));
            return -1;
        }

    return listen_sockfd; 
}


/* -----------------------------------------------------------------------------
    pptpvpn_accept() called by vpnd to listen for incomming connections.
----------------------------------------------------------------------------- */
int pptpvpn_accept(void) 
{

    int				fdConn;
    struct sockaddr_storage	ssSender;
    struct sockaddr		*sapSender = (struct sockaddr *)&ssSender;
    int				nSize = sizeof (ssSender);

    if ((fdConn = pptp_sys_accept(listen_sockfd, sapSender, &nSize)) < 0)
            return -1;
    if (sapSender->sa_family != AF_INET) {
        vpnlog(LOG_ERR, "PPTP plugin: Unexpected protocol family!\n");
        if (pptp_sys_close(fdConn) < 0)
            return -1;
        return 0;
    }
   
    return fdConn;
}

/* -----------------------------------------------------------------------------
    pptpvpn_refuse() called by vpnd to refuse incomming connections
    
        return values:  -1		error
                        0		handled - do not launch pppd
----------------------------------------------------------------------------- */
int pptpvpn_refuse(void) 
{

    int				fdConn;
    struct sockaddr_storage	ssSender;
    struct sockaddr		*sapSender = (struct sockaddr *)&ssSender;
    int				nSize = sizeof (ssSender);

    if ((fdConn = pptp_sys_accept(listen_sockfd, sapSender, &nSize)) < 0)
        return -1;
    
    if (pptp_sys_close(fdConn) < 0)
        return -1;
        
    return 0;
}

/* -----------------------------------------------------------------------------
    pptpvpn_close()  called by vpnd to close listening socket and cleanup.
----------------------------------------------------------------------------- */
void pptpvpn_close(void)
{
    if (listen_sockfd != -1) {
        if (pptp_sys_close(listen_sockfd) < 0)
            ;  // do nothing      
            
        listen_sockfd = -1;
    }
}

