/*
 * Copyright (c) 2000 Apple Computer, Inc. All rights reserved.
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

#include <stdio.h>
#include <fcntl.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>		
#include <netdb.h>		
#include <paths.h>		
#include <unistd.h>		
#include <arpa/inet.h>	
#include <netinet/in.h>	
#include <sys/socket.h>
#include <sys/queue.h>	
#include <sys/wait.h>
#include <sys/stat.h>

#include <CoreFoundation/CoreFoundation.h>
#include <SystemConfiguration/SystemConfiguration.h>

#include "vpnplugins.h"
#include "vpnd.h"

struct vpn_address {
    TAILQ_ENTRY(vpn_address)	next;
    int				pid;
    char 			ip_address[16];
};

// ----------------------------------------------------------------------------
//	Private Globals
// ----------------------------------------------------------------------------

static int			listen_sockfd = -1;
static struct vpn_channel 	the_vpn_channel;

TAILQ_HEAD(, vpn_address) 	free_address_list;
TAILQ_HEAD(, vpn_address) 	child_list;

// ----------------------------------------------------------------------------
//	Function Prototypes
// ----------------------------------------------------------------------------
extern int got_sig_chld(void);
extern int got_terminate(void);

static pid_t fork_child(int fdSocket);
static int reap_children(void);
static int terminate_children(void);


// ----------------------------------------------------------------------------
//	init_address_lists
// ----------------------------------------------------------------------------
void init_address_lists(void)
{
    TAILQ_INIT(&free_address_list);
    TAILQ_INIT(&child_list);
}

// ----------------------------------------------------------------------------
//	add_address
// ----------------------------------------------------------------------------
int add_address(char* ip_address)
{
    struct vpn_address *address_slot;
    int		size;
    
    address_slot = (struct vpn_address*)malloc(sizeof(struct vpn_address));
    if (address_slot == 0)
        return -1;
    if ((size = strlen(ip_address) + 1) > 16)
        return -1;
        
    /* %%%% this address stuff needs to be redone for IPv6 addresses */
    memcpy(address_slot->ip_address, ip_address, strlen(ip_address) + 1);
    TAILQ_INSERT_TAIL(&free_address_list, address_slot, next);
    return 0;
}

// ----------------------------------------------------------------------------
//	add_address_range
// ----------------------------------------------------------------------------
int add_address_range(char* ip_addr_start, char* ip_addr_end)
{
    struct in_addr	start_addr;
    struct in_addr	end_addr;
    char		addr_str[16];
    char		*ip_addr;

    if (!ip_addr_end)
        return add_address(ip_addr_start);
    if (inet_pton(AF_INET, ip_addr_start, &start_addr) < 1)
        return -1;
    if (inet_pton(AF_INET, ip_addr_end, &end_addr) < 1)
        return -1;
    if (start_addr.s_addr > end_addr.s_addr)
        return -1;
    if (start_addr.s_addr == end_addr.s_addr)
        return add_address(ip_addr_start);
        
    for (; start_addr.s_addr <= end_addr.s_addr; start_addr.s_addr++) {
        if (ip_addr = inet_ntop(AF_INET, &start_addr, addr_str, 16)) {
            if (add_address(ip_addr))
                return -1;
        } else
            return -1;
    }
    
    return 0;
}

// ----------------------------------------------------------------------------
//	address_avail
// ----------------------------------------------------------------------------
int address_avail(void)
{
    return (TAILQ_FIRST(&free_address_list) != 0);
}

//-----------------------------------------------------------------------------
//	init_plugin
//-----------------------------------------------------------------------------
int init_plugin(struct vpn_params *params)
{
    char		path[MAXPATHLEN], name[MAXPATHLEN], *p;
    CFBundleRef		pluginbdl, bdl;
    CFURLRef		pluginurl, url;
    int 		(*start)(struct vpn_channel*, CFBundleRef, CFBundleRef, int debug) = 0;
    bool		isPPP;
    int 		len, err = -1;

    listen_sockfd = -1;

    len = strlen(params->plugin_path);
    if (len > 4 && !strcmp(&params->plugin_path[len - 4], ".ppp")) 
        isPPP = 1;
    else if (len > 4 && !strcmp(&params->plugin_path[len - 4], ".vpn")) 
        isPPP = 0;
    else {
        vpnlog(LOG_ERR, "VPND: plugin ''%s' has an incorrect suffix (must end with '.ppp' or '.vpn')\n", params->plugin_path);
        return -1;
    }

    if (params->plugin_path[0] == '/') {
        strcpy(path, params->plugin_path);
        for (p = &params->plugin_path[len - 1]; *p != '/'; p--);
        strncpy(name, p + 1, strlen(p) - 5);
        *(name + (strlen(p) - 5)) = 0;
    }
    else {
        strcpy(path, PLUGINS_DIR);
        strcat(path, params->plugin_path);
        strncpy(name, params->plugin_path, len - 4);
        *(name + len - 4) = 0;
    } 

    if (url = CFURLCreateFromFileSystemRepresentation(NULL, path, strlen(path), TRUE)) {
        if (bdl =  CFBundleCreate(NULL, url)) {

            if (isPPP) {
                if (pluginurl = CFBundleCopyBuiltInPlugInsURL(bdl)) {
                    strcat(path, "/");
                    CFURLGetFileSystemRepresentation(pluginurl, 0, path+strlen(path), MAXPATHLEN-strlen(path));
                    CFRelease(pluginurl);
                    strcat(path, "/");
                    strcat(path, name);
                    strcat(path, ".vpn");
                    
                    if (pluginurl = CFURLCreateFromFileSystemRepresentation(NULL, path, strlen(path), TRUE)) {

                        if (pluginbdl =  CFBundleCreate(NULL, pluginurl)) {

                            // load the executable from the vpn plugin
                            if (CFBundleLoadExecutable(pluginbdl)
                                && (start = CFBundleGetFunctionPointerForName(pluginbdl, CFSTR("start"))))
                                err = (*start)(&the_vpn_channel, pluginbdl, bdl, params->debug);
 
                            CFRelease(pluginbdl);
                        }
                        CFRelease(pluginurl);
                   }	
                }
            }
            else {
                // load the default executable
                if (CFBundleLoadExecutable(bdl)
                    && (start = CFBundleGetFunctionPointerForName(bdl, CFSTR("start"))))
                    err = (*start)(&the_vpn_channel, bdl, NULL, params->debug);
            }
            
            CFRelease(bdl);
        }
        CFRelease(url);
    }

    if (err) {
        vpnlog(LOG_ERR, "VPND: unable to load vpn plugin\n");
        return -1;
    }
    
    if (the_vpn_channel.listen == 0 || the_vpn_channel.accept == 0
            || the_vpn_channel.refuse == 0 || the_vpn_channel.close == 0) {
        vpnlog(LOG_ERR, "VPND: Plugin channel not properly initialized\n");
        return -1;
    }
    
    vpnlog(LOG_INFO, "VPND: vpn plugin loaded\n");
    
    return 0;
}

// ----------------------------------------------------------------------------
//	get_plugin_args
// ----------------------------------------------------------------------------
int get_plugin_args(struct vpn_params* params)
{

    /* get any extra params from the plugin */
    if (the_vpn_channel.get_pppd_args)
        if (the_vpn_channel.get_pppd_args(params))
            return -1;
    
    return 0;
} 


// ----------------------------------------------------------------------------
//	accept_connections
// ----------------------------------------------------------------------------
void accept_connections(struct vpn_params* params)
{

    pid_t		pid_child;
    fd_set		fds;
    char 		addr_str[32];
    int			i, child_sockfd;
    struct vpn_address	*address_slot;

    listen_sockfd = the_vpn_channel.listen();		// initialize the plugin and get listen socket

    if (listen_sockfd < 0) {
        vpnlog(LOG_ERR, "VPND: Unable to intitialize vpn plugin\n");
        goto fail;
    }
    
    /* loop - listening for connection requests */
    while (!got_terminate()) {
        FD_ZERO(&fds);
        FD_SET(listen_sockfd, &fds);
        i = select(listen_sockfd + 1, &fds, NULL, NULL, NULL);
        if (i > 0) {
            if (FD_ISSET (listen_sockfd, &fds)) {
                address_slot = (struct vpn_address*)TAILQ_FIRST(&free_address_list);
                if (address_slot == 0) {
                    if ((child_sockfd = the_vpn_channel.refuse()) < 0) {
                        vpnlog(LOG_ERR, "VPND: error while refusing incomming call %s\n", strerror(errno));
                        continue;
                    }
                } else {
                    if ((child_sockfd = the_vpn_channel.accept()) < 0) {
                        vpnlog(LOG_ERR, "VPND: error accepting incomming call %s\n", strerror(errno));
                        continue;
                    }
                }
                if (child_sockfd == 0)
                    continue;
                // Turn this connection over to a child.
                while ((pid_child = fork_child(child_sockfd)) < 0) {
                    if (errno != EINTR) {
                        vpnlog(LOG_ERR, "VPND: error during fork = %s\n", strerror(errno));
                        goto fail;
                    } else if (got_terminate())
                        goto fail;
                }
                if (pid_child) {			// parent
                    vpnlog(LOG_DEBUG, "Incoming call... Address given to client = %s\n", address_slot->ip_address);
                    TAILQ_REMOVE(&free_address_list, address_slot, next);
                    address_slot->pid = pid_child;
                    TAILQ_INSERT_TAIL(&child_list, address_slot, next);
                } else {	
                    // child
                    sprintf(addr_str, ":%s", address_slot->ip_address);
                    params->exec_args[params->next_arg_index] = addr_str;	// setup ip address in arg list
                    params->exec_args[params->next_arg_index + 1] = 0;		// make sure arg list end with zero
                    execv(PATH_PPPD, params->exec_args);			// launch it
                    
                    /* not reached except if there is an error */
                    vpnlog(LOG_ERR, "VPND: execv failed during exec of /usr/sbin/pppd\nARGUMENTS\n");
                    for (i = 1; i < MAXARG && i < params->next_arg_index; i++) {
                        if (params->exec_args[i])
                            vpnlog(LOG_DEBUG, "%d :  %s\n", i, params->exec_args[i]);
                    }
                    vpnlog(LOG_DEBUG, "\n");
                        
                    exit(1);
                }
            }
        } else if (i < 0) {
            if (errno == EINTR)
                reap_children();
            else {
                vpnlog(LOG_ERR, "VPND: Unexpected result from select - err = %s\n", strerror(errno));
                goto fail;
            }
        }
    }

fail:
    the_vpn_channel.close();
    terminate_children();
}



//-----------------------------------------------------------------------------
//	fork_child
//-----------------------------------------------------------------------------
static pid_t fork_child(int fdSocket)
{
    register pid_t	pidChild = 0 ;
    int 		i;
    
    errno = 0 ;

    switch (pidChild = fork ()) {
        case 0:		// in child
            break ;
        case -1:	// error
            syslog(LOG_ERR, "VPND: fork() failed: %m") ;
            /* FALLTHRU */
        default:	// parent
            close (fdSocket) ;
            return pidChild ;
    }

    for (i = getdtablesize() - 1; i >= 0; i--) 
        if (i != fdSocket) 
            close(i);
    dup2 (fdSocket, STDIN_FILENO) ;
    open("/dev/null", O_RDWR, 0);
    open("/dev/null", O_RDWR, 0);
    close (fdSocket) ;

    return pidChild ;
}


//-----------------------------------------------------------------------------
//	reap_children
//-----------------------------------------------------------------------------
static int reap_children(void)
{

    int pid, status;
    struct vpn_address *address_slot;

    if (!got_sig_chld())
        return 0;

    if (!TAILQ_FIRST(&child_list))
        return 0;        
    
    /* loop on waitpid collecting children and freeing the addresses */
    while ((pid = waitpid(-1, &status, WNOHANG)) != -1 && pid != 0) {
        // find the child in the child list - remove it and free the address
        // back to the free address list				  
        TAILQ_FOREACH(address_slot, &child_list, next)	{
            if (address_slot->pid == pid) {
                vpnlog(LOG_DEBUG, "   --> Client with address = %s has hungup\n", address_slot->ip_address);
                TAILQ_REMOVE(&child_list, address_slot, next);
                TAILQ_INSERT_TAIL(&free_address_list, address_slot, next);
                if (WIFSIGNALED(status))
                    vpnlog(LOG_WARNING, "VPND: Child process (pid %d) terminated with signal %d", pid, WTERMSIG(status));
                break;
            }
        }
    }
    if (pid == -1)
	if (errno != EINTR) {
	    //syslog(LOG_ERR, "VPND: Error waiting for child process: %m");
            return -1;
        }
    return 0;
}

//-----------------------------------------------------------------------------
//	terminate_children
//-----------------------------------------------------------------------------
static int terminate_children(void)
{

    struct vpn_address *child;
    
    /* loop on waitpid collecting children and freeing the addresses */
    while (child = (struct vpn_address*)TAILQ_FIRST(&child_list)) {
        while (kill(child->pid, SIGTERM) < 0)
            if (errno != EINTR) {
                vpnlog(LOG_ERR, "VPND: error terminating child - err = %s\n", strerror(errno));
                break;
            }
    	TAILQ_REMOVE(&child_list, child, next);
        TAILQ_INSERT_TAIL(&free_address_list, child, next);
    }
        
    return 0;
}


