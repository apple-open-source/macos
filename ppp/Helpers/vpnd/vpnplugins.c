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
#include <ifaddrs.h>
#include <net/if.h>
#include <netinet/in.h>
#include <sys/ioctl.h>	
#include <sys/socket.h>
#include <sys/queue.h>	
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <sys/kern_event.h>
#include <arpa/inet.h>
#include <sys/un.h>

#include <net/if_var.h>

#include <netinet/in_var.h>
#include <mach/mach_time.h>

#include <CoreFoundation/CoreFoundation.h>
#include <SystemConfiguration/SystemConfiguration.h>

#include "vpnoptions.h"
#include "vpnplugins.h"
#include "vpnd.h"

#define VPN_ADDR_DELETE 0x1

struct vpn_address {
    TAILQ_ENTRY(vpn_address)	next;
    int				pid;
    int				flags;
    char 			ip_address[16];
};

#define LB_MAX_SLAVE_AGE	10
#define LB_IPCONFIG_TIMEOUT 60

struct lb_slave {
    TAILQ_ENTRY(lb_slave)	next;
    struct sockaddr_in server_address;
    int				age;
	struct in_addr	redirect_address;
    u_int16_t 		max_connection;
	u_int16_t 		cur_connection;
	u_int32_t 		ratio;
};

enum {
	HEALTH_UNKNOWN = -1, 
	HEALTH_OK,
	HEALTH_SICK,
	HEALTH_DEAD
};

// ----------------------------------------------------------------------------
//	Private Globals
// ----------------------------------------------------------------------------
static int			listen_sockfd = -1;
static int			lb_sockfd = -1;
static int			evt_sockfd = -1;
static int			health_sockfd = -1;
static int			health_state = HEALTH_UNKNOWN;
static struct vpn_channel 	the_vpn_channel;

static double	 		timeScaleSeconds;	/* scale factor for machine absolute time to seconds */
static double	 		timeScaleMicroSeconds;	/* scale factor for machine absolute time to microseconds */

/* load balancing state information */
int					lb_is_started = 0;		// is load balancing currently started ?
int					lb_is_master = 0;		// are we currently the master ?
struct sockaddr_in	lb_master_address;	// address of a master, as discovered by the slave
TAILQ_HEAD(, lb_slave) 	lb_slaves_list; // list opf slaves, as discoverd by the master server
u_int16_t			lb_cur_connections = 0;		// nb of connections currently active
u_int16_t			lb_max_connections = 0;		// max number of connections
struct lb_slave		*lb_next_slave = 0; // next slave to redirect the call to
int					lb_ipconfig_time = 0; // ip config confirmation timer



TAILQ_HEAD(, vpn_address) 	save_address_list;
TAILQ_HEAD(, vpn_address) 	free_address_list;
TAILQ_HEAD(, vpn_address) 	child_list;

// ----------------------------------------------------------------------------
//	Function Prototypes
// ----------------------------------------------------------------------------
extern int got_sig_chld(void);
extern int got_sig_hup(void);
extern int got_sig_usr1(void);
extern int got_terminate(void);

static pid_t fork_child(int fdSocket);
static int reap_children(void);
static int terminate_children(void);
static int getabsolutetime(struct timeval *timenow);
static void determine_next_slave(struct vpn_params* params);
int start_load_balancing(struct vpn_params *params, fd_set *out_fds, int *out_fdmax);
int stop_load_balancing(struct vpn_params *params,  fd_set *out_fds);


// ----------------------------------------------------------------------------
//	init_address_lists
// ----------------------------------------------------------------------------
void init_address_lists(void)
{
    TAILQ_INIT(&free_address_list);
    TAILQ_INIT(&save_address_list);
    TAILQ_INIT(&child_list);
	TAILQ_INIT(&lb_slaves_list);

	lb_max_connections = 0;
	lb_cur_connections = 0;
}

// ----------------------------------------------------------------------------
//	add_address
// ----------------------------------------------------------------------------
int add_address(char* ip_address)
{
    struct vpn_address *address_slot;
    int		size;

    if ((size = strlen(ip_address) + 1) > 16)
        return -1;
    
    address_slot = (struct vpn_address*)malloc(sizeof(struct vpn_address));
    if (address_slot == 0)
        return -1;
        
	lb_max_connections++;

    /* %%%% this address stuff needs to be redone for IPv6 addresses */
    memcpy(address_slot->ip_address, ip_address, strlen(ip_address) + 1);
    address_slot->flags = 0;
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
    struct in_addr	cur_addr;
    char		addr_str[16];
    char		*ip_addr;

    if (!ip_addr_end)
        return add_address(ip_addr_start);
    if (inet_pton(AF_INET, ip_addr_start, &start_addr) < 1)
        return -1;
    if (inet_pton(AF_INET, ip_addr_end, &end_addr) < 1)
        return -1;
	start_addr.s_addr = ntohl(start_addr.s_addr);
	end_addr.s_addr = ntohl(end_addr.s_addr);
    if (start_addr.s_addr > end_addr.s_addr)
        return -1;
    if (start_addr.s_addr == end_addr.s_addr)
        return add_address(ip_addr_start);
        
    for (; start_addr.s_addr <= end_addr.s_addr; start_addr.s_addr++) {
		cur_addr = start_addr;
		cur_addr.s_addr = htonl(cur_addr.s_addr);
        if (ip_addr = (char*)inet_ntop(AF_INET, &cur_addr, addr_str, 16)) {
            if (add_address(ip_addr))
                return -1;
        } else
            return -1;
    }
    
    return 0;
}

// ----------------------------------------------------------------------------
//	begin_address_update
// ----------------------------------------------------------------------------
void begin_address_update(void)
{
    struct vpn_address *address_slot;
    
    // copy the free addresses to the save list
	while (address_slot = (struct vpn_address*)TAILQ_FIRST(&free_address_list)) {
        TAILQ_REMOVE(&free_address_list, address_slot, next);        
        TAILQ_INSERT_TAIL(&save_address_list, address_slot, next);
    }
	
}

// ----------------------------------------------------------------------------
//	cancel_address_update
// ----------------------------------------------------------------------------
void cancel_address_update(void)
{
    struct vpn_address *address_slot;
    
    // remove any new addresses
	while (address_slot = (struct vpn_address*)TAILQ_FIRST(&free_address_list)) {
        TAILQ_REMOVE(&free_address_list, address_slot, next);
        free(address_slot);
		lb_max_connections--;
    }
    
    // copy the free addresses back from the save list
	while (address_slot = (struct vpn_address*)TAILQ_FIRST(&save_address_list)) {
        TAILQ_REMOVE(&save_address_list, address_slot, next);        
        TAILQ_INSERT_TAIL(&free_address_list, address_slot, next);
    }
}

// ----------------------------------------------------------------------------
//	apply_address_update
// ----------------------------------------------------------------------------
void apply_address_update(void)
{
    struct vpn_address *address_slot;
    struct vpn_address *child_address;
    
    // remove the old free addresses
	while (address_slot = (struct vpn_address*)TAILQ_FIRST(&save_address_list)) {
        TAILQ_REMOVE(&save_address_list, address_slot, next);
        free(address_slot);
		lb_max_connections--;
    }	

    // search the child address list and match up with new addresses
    // kill children using invalid addresses
    TAILQ_FOREACH(child_address, &child_list, next) {
        child_address->flags |= VPN_ADDR_DELETE;
        TAILQ_FOREACH(address_slot, &free_address_list, next) {
            if (!strcmp(child_address->ip_address, address_slot->ip_address)) {
                TAILQ_REMOVE(&free_address_list, address_slot, next);	// address match - remove from free list
                free(address_slot);
				lb_max_connections--;
                child_address->flags &= ~VPN_ADDR_DELETE;		// don't kill child
                break;
            }
        }
        if (child_address->flags & VPN_ADDR_DELETE)
            while (kill(child_address->pid, SIGTERM) < 0)
                if (errno != EINTR) {
                    vpnlog(LOG_ERR, "VPND: error terminating child - err = %s\n", strerror(errno));
                    break;
                }
    }

	vpnlog(LOG_DEBUG, "address list updated\n");

    reap_children();

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
    int 		(*start)(struct vpn_channel*, CFBundleRef, CFBundleRef, int debug, int log_verbose) = 0;
    bool		isPPP;
    int 		len, err = -1;

	bzero(&the_vpn_channel, sizeof(struct vpn_channel));
	if (params->plugin_path == 0) {
		err = add_builtin_plugin(params, &the_vpn_channel);
		if (err)
			vpnlog(LOG_ERR, "Cannot initialize built-in channel\n");
        return err;
    }

    len = strlen(params->plugin_path);
    if (len > 4 && !strcmp(&params->plugin_path[len - 4], ".ppp")) 
        isPPP = 1;
    else if (len > 4 && !strcmp(&params->plugin_path[len - 4], ".vpn")) 
        isPPP = 0;
    else {
        vpnlog(LOG_ERR, "Plugin ''%s' has an incorrect suffix (must end with '.ppp' or '.vpn')\n", params->plugin_path);
        return -1;
    }

    if (params->plugin_path[0] == '/') {
        strlcpy(path, params->plugin_path, sizeof(path));
        for (p = &params->plugin_path[len - 1]; *p != '/'; p--);
        strncpy(name, p + 1, strlen(p) - 5);
        *(name + (strlen(p) - 5)) = 0;
    }
    else {
        strlcpy(path, PLUGINS_DIR, sizeof(path));
        strlcat(path, params->plugin_path, sizeof(path));
        strlcpy(name, params->plugin_path, sizeof(name) - 4); // leave space for .vpn
		*(name + len - 4) = 0;
    } 

    vpnlog(LOG_NOTICE, "Loading plugin %s\n", path);

    if (url = CFURLCreateFromFileSystemRepresentation(NULL, (UInt8 *)path, strlen(path), TRUE)) {
        if (bdl =  CFBundleCreate(NULL, url)) {

            if (isPPP) {
                if (pluginurl = CFBundleCopyBuiltInPlugInsURL(bdl)) {
                    strlcat(path, "/", sizeof(path));
                    CFURLGetFileSystemRepresentation(pluginurl, 0, (UInt8 *)(path+strlen(path)), MAXPATHLEN-strlen(path));
                    CFRelease(pluginurl);
                    strlcat(path, "/", sizeof(path));
                    strlcat(path, name, sizeof(path));
                    strlcat(path, ".vpn", sizeof(path));
                    
                    if (pluginurl = CFURLCreateFromFileSystemRepresentation(NULL, (UInt8 *)path, strlen(path), TRUE)) {

                        if (pluginbdl =  CFBundleCreate(NULL, pluginurl)) {

                            // load the executable from the vpn plugin
                            if (CFBundleLoadExecutable(pluginbdl)
                                && (start = CFBundleGetFunctionPointerForName(pluginbdl, CFSTR("start"))))
                                err = (*start)(&the_vpn_channel, pluginbdl, bdl, params->debug, params->log_verbose);
 
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
                    err = (*start)(&the_vpn_channel, bdl, NULL, params->debug, params->log_verbose);
            }
            
            CFRelease(bdl);
        }
        CFRelease(url);
    }

    if (err) {
        vpnlog(LOG_ERR, "Unable to load plugin (error = %d)\n", err);
        return -1;
    }
    
    if (the_vpn_channel.listen == 0 || the_vpn_channel.accept == 0
            || the_vpn_channel.refuse == 0 || the_vpn_channel.close == 0) {
        vpnlog(LOG_ERR, "Plugin channel not properly initialized\n");
        return -1;
    }
    
    //vpnlog(LOG_INFO, "Plugin loaded\n");
    
    return 0;
}

// ----------------------------------------------------------------------------
//	get_plugin_args
// ----------------------------------------------------------------------------
int get_plugin_args(struct vpn_params* params, int reload)
{

    /* get any extra params from the plugin */
    if (the_vpn_channel.get_pppd_args) 
        if (the_vpn_channel.get_pppd_args(params, reload))
            return -1;
    
	// Load Balancing is only supported for L2TP/IPSec protocol
	if (params->lb_enable && !the_vpn_channel.lb_redirect) {
        vpnlog(LOG_ERR, "Plugin does not support Load Balancing\n");
		params->lb_enable = 0;
	}

    return 0;
} 

// ----------------------------------------------------------------------------
//	call the ipconfig command
// ----------------------------------------------------------------------------
int call_ipconfig(char *command, char *interface, struct in_addr *address, int timeout) 
{

	int pid, exitcode = -1, status;
	char str[32], str1[32];
	

    if ((pid = fork()) < 0)
        return 1;

    if (pid == 0) {
		int i;
		for (i = getdtablesize() - 1; i >= 0; i--) close(i);
		open("/dev/null", O_RDWR, 0);
		dup(0);
		dup(0);
		inet_ntop(AF_INET, address, str, sizeof(str));
		sprintf(str1, "%d", timeout);
        execle("/usr/sbin/ipconfig", "ipconfig",  command, interface, "FAILOVER", str, "255.255.255.255", str1, (char *)0, (char *)0);
        exit(1);
    }

    while (waitpid(pid, &status, 0) < 0) {
        if (errno == EINTR)
            continue;
       return 1;
    }
	
	exitcode = WIFEXITED(status) ? WEXITSTATUS(status) : -1;

	if (exitcode)
		vpnlog(LOG_ERR, "Unable to configure IP Failover Service (error %d)\n", exitcode);
		
    return exitcode;
}

// ----------------------------------------------------------------------------
//	configure/unconfigure failover
// ----------------------------------------------------------------------------
int configure_failover(char *interface, struct in_addr *address, int timeout) 
{
	int err = -1;

	err = call_ipconfig("setservice", interface, address, timeout);
	if (err)
		vpnlog(LOG_ERR, "Unable to configure IP Failover Service (error %d)\n", err);
		
    return err;
}

// ----------------------------------------------------------------------------
//	delete load balancing pool address
// ----------------------------------------------------------------------------
int unconfigure_failover(char *interface, struct in_addr *address) 
{
	int err = -1;
	
	err = call_ipconfig("removeservice", interface, address, 0);
	if (err)
		vpnlog(LOG_ERR, "Unable to unconfigure IP Failover Service (error %d)\n", err);
		
    return err;
}

// ----------------------------------------------------------------------------
//	health_check
// ----------------------------------------------------------------------------
int health_check(struct vpn_params *params, int event, fd_set *out_fds, int *out_fdmax)
{
	int fd = health_sockfd, err, ret = 0;
	
	err = the_vpn_channel.health_check(&fd, event);

	/* check if health socket has changed */
	if (fd != health_sockfd) {
		if (health_sockfd != -1)
			FD_CLR(health_sockfd, out_fds);
		health_sockfd = fd;
		if (health_sockfd != -1) {
			FD_SET(health_sockfd, out_fds);
			if (*out_fdmax <= health_sockfd)
				*out_fdmax = health_sockfd + 1;
		}
	}

	switch (err) {
		case 0:
			if (health_state == HEALTH_SICK) {
				vpnlog(LOG_ERR, "Health control check: server is back to normal...\n");
				// feeling better...
				if (params->lb_enable) {
					if (start_load_balancing(params, out_fds, out_fdmax) < 0)
						ret = -1;
				}				
			}
			health_state = HEALTH_OK;
			break;

		case -2: 
			if (health_state == HEALTH_OK) {
				vpnlog(LOG_ERR, "Health control check: server is sick...\n");
			}
			health_state = HEALTH_SICK;
			
			if (lb_is_started)
				stop_load_balancing(params, out_fds);
			break;
		
		default:
		case -1:
			health_state = HEALTH_DEAD;
			vpnlog(LOG_ERR, "Health control check: server is dead...\n");
			ret = -1;
			break;
	}
	
	return ret;
}

// ----------------------------------------------------------------------------
//	start load balancing
// ----------------------------------------------------------------------------
int start_load_balancing(struct vpn_params *params, fd_set *out_fds, int *out_fdmax) 
{
	struct sockaddr_in	listen_addr;
	fd_set fds = *out_fds;
	int fdmax = *out_fdmax; 
	struct kev_request	kev_req;

	if (lb_is_started)
		return 0;
		
    	// Load Balancing is only supported for L2TP/IPSec protocol
	if (params->lb_enable && !the_vpn_channel.lb_redirect) {
        vpnlog(LOG_ERR, "Plugin does not support Load Balancing\n");
		params->lb_enable = 0;
	}


	evt_sockfd = socket(PF_SYSTEM, SOCK_RAW, SYSPROTO_EVENT);
	if (evt_sockfd < 0) {
		vpnlog(LOG_ERR, "Unable to create event socket (errno = %d)\n", errno);
		goto fail;
	}

	FD_SET(evt_sockfd, &fds);
	if (fdmax <= evt_sockfd)
		fdmax = evt_sockfd + 1;

	kev_req.vendor_code = KEV_VENDOR_APPLE;
	kev_req.kev_class = KEV_NETWORK_CLASS;
	kev_req.kev_subclass = KEV_INET_SUBCLASS;
	ioctl(evt_sockfd, SIOCSKEVFILT, &kev_req);

	lb_sockfd = socket(PF_INET, SOCK_DGRAM, 0);
	if (lb_sockfd < 0) {
		vpnlog(LOG_ERR, "Unable to create load balancing socket (errno = %d)\n", errno);
		goto fail;
	}

	FD_SET(lb_sockfd, &fds);
	if (fdmax <= lb_sockfd)
		fdmax = lb_sockfd + 1;

	bzero(&listen_addr, sizeof(listen_addr));
	listen_addr.sin_family = PF_INET;
	listen_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	listen_addr.sin_port = params->lb_port;
	
	if (bind(lb_sockfd, (struct sockaddr*)&listen_addr, sizeof(listen_addr)) < 0 ){
		vpnlog(LOG_ERR, "Unable to bind load balancing socket (errno = %d)\n", errno);
		goto fail;
	}

	// set master address
	bzero(&lb_master_address, sizeof(lb_master_address));
	lb_master_address.sin_family = AF_INET;
	lb_master_address.sin_len = sizeof(lb_master_address);
	lb_master_address.sin_addr = params->lb_cluster_address;
	lb_master_address.sin_port = params->lb_port;

	configure_failover(params->lb_interface, &params->lb_cluster_address, LB_IPCONFIG_TIMEOUT);
	lb_ipconfig_time = LB_IPCONFIG_TIMEOUT - 10;

	lb_is_master = find_address(&lb_master_address, params->lb_interface);
	
	*out_fds = fds;
	*out_fdmax = fdmax;
	lb_is_started = 1;
	lb_next_slave = 0;
	vpnlog(LOG_NOTICE, "Load Balancing: Started\n");

	return 0;
	
fail:
	if (evt_sockfd >= 0) {
		close(evt_sockfd);
		evt_sockfd = -1;
	}
	if (lb_sockfd >= 0) {
		close(lb_sockfd);
		lb_sockfd = -1;
	}
	return -1;
}

// ----------------------------------------------------------------------------
//	stop load balancing
// ----------------------------------------------------------------------------
int stop_load_balancing(struct vpn_params *params,  fd_set *out_fds) 
{

	if (!lb_is_started)
		return 0;
		
	if (evt_sockfd >= 0) {
		FD_CLR(evt_sockfd, out_fds);
		close(evt_sockfd);
		evt_sockfd = -1;
	}
	if (lb_sockfd >= 0) {
		FD_CLR(lb_sockfd, out_fds);
		close(lb_sockfd);
		lb_sockfd = -1;
	}

	unconfigure_failover(params->lb_interface, &params->lb_cluster_address);

	lb_is_master = 0;
	lb_is_started = 0;
	lb_next_slave = 0;
	
	vpnlog(LOG_NOTICE, "Load Balancing: Stopped\n");
	
	return 0;
}

// ----------------------------------------------------------------------------
//	determine which slave takes next call
// ----------------------------------------------------------------------------
static void determine_next_slave(struct vpn_params *params) 
{
	struct lb_slave *slave, *oldslave;
	u_int32_t			a;
	
	oldslave = lb_next_slave;
	
	// determine the server that will take the next call 
	lb_next_slave = TAILQ_FIRST(&lb_slaves_list);
	TAILQ_FOREACH(slave, &lb_slaves_list, next)	{				
		if (slave->ratio < lb_next_slave->ratio)
			lb_next_slave = slave;
	}
	
	// and inform racoon about the redirection
	if (lb_next_slave != oldslave) {

		a = ntohl(lb_next_slave->redirect_address.s_addr);
		vpnlog(LOG_NOTICE, "Load Balancing: Next call will be redirected to slave with IP address %d.%d.%d.%d. Current slave load %d/%d.\n", 
			a >> 24 & 0xFF, a >> 16 & 0xFF, a >> 8 & 0xFF, a & 0xFF, lb_next_slave->cur_connection, lb_next_slave->max_connection);

		if (the_vpn_channel.lb_redirect) {	
			the_vpn_channel.lb_redirect(&params->lb_cluster_address, &lb_next_slave->redirect_address);
		}
	}
	
}

#define LB_MSG_TYPE_UPDATE	1

struct lb_message {

// header
	u_int16_t	type;	
	u_int16_t	len;	
	u_int8_t	version;
	u_int8_t	reserved1;	
	u_int16_t	reserved2;	

// version 1
	u_int32_t 		redirect_address;	 //ip v4
	u_int16_t 		max_connection;     // 100
	u_int16_t 		cur_connection;		// 50
	u_int32_t 		reserved3;
	u_int32_t 		reserved4;
	
};

// ----------------------------------------------------------------------------
//	accept_connections
// ----------------------------------------------------------------------------
void accept_connections(struct vpn_params* params)
{

    pid_t				pid_child;
    fd_set				fds, fds_save;
    char				addr_str[32];
    int					i, child_sockfd, fdmax, err, hastimeout;
    struct vpn_address	*address_slot;
	struct sockaddr_in	addr;
	socklen_t			addrlen;
	size_t				datalen;
	char				data[1000];
	struct timeval		timenow, timeout, timeend;
	struct lb_message	*lbmsg;
	u_int16_t			lbmsgtype;
	struct lb_slave		*slave;
	u_int32_t			a;
	
    FD_ZERO(&fds_save);
	fdmax = 0;

	/*
		open the new connection listening socket
	*/
    listen_sockfd = 0;
	if (the_vpn_channel.listen)
		listen_sockfd = the_vpn_channel.listen();		// initialize the plugin and get listen socket

    if (listen_sockfd < 0) {
        vpnlog(LOG_ERR, "Unable to initialize vpn plugin\n");
        goto fail;
    }

	FD_SET(listen_sockfd, &fds_save);
	if (fdmax <= listen_sockfd)
		fdmax = listen_sockfd + 1;

	/*
		health monitoring
	*/
    health_sockfd = -1;

	/*
		start the load balancing
	*/
	if (params->lb_enable) {
		if (start_load_balancing(params, &fds_save, &fdmax) < 0) {
			goto fail;
		}
	}
	
	hastimeout = lb_is_started || the_vpn_channel.health_check;
	
	if (hastimeout) {
		getabsolutetime(&timeend);
		timeend.tv_sec += 1; // timeout immediatly
	}
		
    /* 
		loop - listening for connection requests and other events
	*/
    while (!got_terminate()) {

		if (hastimeout) {
			getabsolutetime(&timenow);
			timeout.tv_sec = timeend.tv_sec > timenow.tv_sec ? timeend.tv_sec - timenow.tv_sec : 0;
			timeout.tv_usec = timeend.tv_usec - timenow.tv_usec;
			if (timeout.tv_usec < 0) {
				timeout.tv_usec += 1000000;
				timeout.tv_sec -= 1;
			}
			if (timeout.tv_sec < 0)
				timeout.tv_sec = timeout.tv_usec = 0;
			
		}
     
		fds = fds_save;
        i = select(fdmax, &fds, NULL, NULL, hastimeout ? &timeout : 0);
		
		// --------------- file descriptor selected --------------
        if (i > 0) {
		
			/* event on new kernel event socket */
            if (lb_is_started && FD_ISSET (evt_sockfd, &fds)) {
				char                 	buf[256];
				struct kern_event_msg	*ev_msg;
				struct kev_in_data     	*inetdata;

				if (recv(evt_sockfd, &buf, sizeof(buf), 0) != -1) {
					ev_msg = (struct kern_event_msg *) &buf;
					switch (ev_msg->event_code) {
						case KEV_INET_NEW_ADDR:
							inetdata = (struct kev_in_data *) &ev_msg->event_data[0];
							if (inetdata->ia_addr.s_addr == params->lb_cluster_address.s_addr) {
								// our master address has been assigned. we are now the master server
								vpnlog(LOG_NOTICE, "Load Balancing: Cluster address assigned. Server is becoming master...\n");
								lb_is_master = 1;
							}
							break;
						case KEV_INET_ADDR_DELETED:
							inetdata = (struct kev_in_data *) &ev_msg->event_data[0];
							if (inetdata->ia_addr.s_addr == params->lb_cluster_address.s_addr) {
								// our master address has been deleted. we are not master anymore
								vpnlog(LOG_NOTICE, "Load Balancing: Cluster address deleted. Server is no longer master...\n");
								lb_is_master = 0;
								while (slave = TAILQ_FIRST(&lb_slaves_list)) {
									TAILQ_REMOVE(&lb_slaves_list, slave, next);
									free(slave);
								}
							}
							break;
						
					}
				}
			}
			
			/* event on load balancing socket */
            if (lb_is_started && FD_ISSET (lb_sockfd, &fds)) {

				addrlen = sizeof(addr);
				datalen = recvfrom (lb_sockfd, data, sizeof(data) - 1, 0, (struct sockaddr*)&addr, &addrlen);
				if (datalen > 0) {
				
					data[datalen] = 0;
					lbmsg = (struct lb_message *)data;
					lbmsgtype = ntohs(lbmsg->type);

					if (lb_is_master && lbmsgtype == LB_MSG_TYPE_UPDATE) {
								
						// search for slave in the list
						TAILQ_FOREACH(slave, &lb_slaves_list, next)	{				
							if (slave->server_address.sin_addr.s_addr == addr.sin_addr.s_addr)
								break;
						}
						if (!slave) {
							slave = malloc(sizeof(struct lb_slave));
							if (slave == 0) {
								vpnlog(LOG_ERR, "cannot allocate memory for slave server.\n");
								break;
							}
							
							a = ntohl(addr.sin_addr.s_addr);
							vpnlog(LOG_NOTICE, "Load Balancing: Slave server appeared with IP address %d.%d.%d.%d\n", a >> 24 & 0xFF, a >> 16 & 0xFF, a >> 8 & 0xFF, a & 0xFF);
							bcopy(&addr, &slave->server_address, sizeof(addr));
							TAILQ_INSERT_TAIL(&lb_slaves_list, slave, next);

						}

						slave->age = LB_MAX_SLAVE_AGE;
						slave->redirect_address.s_addr = lbmsg->redirect_address;
						slave->max_connection = ntohs(lbmsg->max_connection);
						slave->cur_connection = ntohs(lbmsg->cur_connection);
						slave->ratio = (lbmsg->cur_connection < lbmsg->max_connection) ? (lbmsg->cur_connection * 1000)/lbmsg->max_connection : 1000;
						a = ntohl(lbmsg->redirect_address);
//						vpnlog(LOG_DEBUG, "Load Balancing: Slave server update. Redirection address is %d.%d.%d.%d, current load is %d/%d\n", 
//							a >> 24 & 0xFF, a >> 16 & 0xFF, a >> 8 & 0xFF, a & 0xFF, slave->cur_connection, slave->max_connection);

						determine_next_slave(params);
					}
				}
			}

			/* event on health control socket */
            if (health_sockfd != -1 && FD_ISSET (health_sockfd, &fds)) {
				err = health_check(params, 1, &fds_save, &fdmax);
				if (err < 0)
					goto fail;
			}

			/* event on new connection listening socket */
            if (FD_ISSET (listen_sockfd, &fds)) {

                address_slot = (struct vpn_address*)TAILQ_FIRST(&free_address_list);
                if (address_slot == 0) {
                    if ((child_sockfd = the_vpn_channel.refuse()) < 0) {
                        vpnlog(LOG_ERR, "Error while refusing incoming call %s\n", strerror(errno));
                        continue;
                    }
                } else {
                    if ((child_sockfd = the_vpn_channel.accept()) < 0) {
                        vpnlog(LOG_ERR, "Error accepting incoming call %s\n", strerror(errno));
                        continue;
                    }
                }
                if (child_sockfd == 0)
                    continue;
                // Turn this connection over to a child.
                while ((pid_child = fork_child(child_sockfd)) < 0) {
                    if (errno != EINTR) {
                        vpnlog(LOG_ERR, "Error during fork = %s\n", strerror(errno));
                        goto fail;
                    } else if (got_terminate())
                        goto fail;
                }
                if (pid_child) {			// parent
                    vpnlog(LOG_NOTICE, "Incoming call... Address given to client = %s\n", address_slot->ip_address);
                    TAILQ_REMOVE(&free_address_list, address_slot, next);
                    address_slot->pid = pid_child;
                    TAILQ_INSERT_TAIL(&child_list, address_slot, next);
					lb_cur_connections++;
                } else {	
                    // child
                    snprintf(addr_str, sizeof(addr_str), ":%s", address_slot->ip_address);
                    params->exec_args[params->next_arg_index] = addr_str;	// setup ip address in arg list
                    params->exec_args[params->next_arg_index + 1] = 0;		// make sure arg list end with zero
                    execve(PATH_PPPD, params->exec_args, NULL);			// launch it
                    
                    /* not reached except if there is an error */
                    vpnlog(LOG_ERR, "execve failed during exec of /usr/sbin/pppd\nARGUMENTS\n");
                    for (i = 1; i < MAXARG && i < params->next_arg_index; i++) {
                        if (params->exec_args[i])
                            vpnlog(LOG_DEBUG, "%d :  %s\n", i, params->exec_args[i]);
                    }
                    vpnlog(LOG_DEBUG, "\n");
				
                    exit(1);
                }
            }
        }
		
		// --------------- timeout expired --------------
		else if (i == 0) {
			
			// check health
			if (the_vpn_channel.health_check) {
				err = health_check(params, 0, &fds_save, &fdmax);
				if (err < 0)
					goto fail;
			}
			
			if (lb_is_started) {
				// update the master every second

				lbmsg = (struct lb_message *)data;
				bzero(lbmsg, sizeof(struct lb_message));
				lbmsg->type = htons(LB_MSG_TYPE_UPDATE);
				lbmsg->len = htons(sizeof(*lbmsg));
				
				// fill in actual load balancing data 
				lbmsg->redirect_address = params->lb_redirect_address.s_addr;
				lbmsg->max_connection = htons(lb_max_connections);
				lbmsg->cur_connection = htons(lb_cur_connections);
				
				a = ntohl(params->lb_redirect_address.s_addr);
	//			vpnlog(LOG_DEBUG, "Load Balancing: Sending update to master server. Updating my master. Redirection address is %d.%d.%d.%d, current load is %d/%d\n", 
	//							a >> 24 & 0xFF, a >> 16 & 0xFF, a >> 8 & 0xFF, a & 0xFF, lb_cur_connections, lb_max_connections);

				if (sendto(lb_sockfd, data, sizeof(*lbmsg), 0, (struct sockaddr*)&lb_master_address, sizeof(lb_master_address)) < 0) {
					vpnlog(LOG_ERR, "Load balancing: failed to send update (%s)", strerror(errno));
				}

				// if we are a master, check on slave age
				if (lb_is_master) {
					
					TAILQ_FOREACH(slave, &lb_slaves_list, next)	{
						if (--slave->age == 0) {
							a = ntohl(slave->server_address.sin_addr.s_addr);
							vpnlog(LOG_NOTICE, "Load Balancing: Slave server with IP address %d.%d.%d.%d disappeared\n", a >> 24 & 0xFF, a >> 16 & 0xFF, a >> 8 & 0xFF, a & 0xFF);
							TAILQ_REMOVE(&lb_slaves_list, slave, next);
							if (slave == lb_next_slave) {
								lb_next_slave = 0;
								determine_next_slave(params);
							}
							free(slave);
						}
					}
				}
				
				lb_ipconfig_time--;
				if (lb_ipconfig_time <= 0) {
					configure_failover(params->lb_interface, &params->lb_cluster_address, LB_IPCONFIG_TIMEOUT);
					lb_ipconfig_time = LB_IPCONFIG_TIMEOUT - 10;
				}
			}
			
			getabsolutetime(&timeend);
			timeend.tv_sec += 1; // 1 second probes
		
		}
		// --------------- error or interrupt --------------
		else {
            if (errno != EINTR) {
                vpnlog(LOG_ERR, "Unexpected result from select - err = %s\n", strerror(errno));
				
                goto fail;
            }
        }
		
		if (got_sig_chld())
			reap_children();
		if (got_sig_usr1())
			toggle_debug();
		if (got_sig_hup()) {

			if (lb_is_started)
				stop_load_balancing(params, &fds_save);

			update_prefs();
			
			// restart load balancing
			if (params->lb_enable) {
				if (start_load_balancing(params, &fds_save, &fdmax) < 0)
					goto fail;
			}
		}
    }

fail:
	if (lb_is_started)
		stop_load_balancing(params, &fds_save);
    if (the_vpn_channel.close)
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
            syslog(LOG_ERR, "fork() failed: %m") ;
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

    if (!TAILQ_FIRST(&child_list))
        return 0;        
    
    /* loop on waitpid collecting children and freeing the addresses */
    while ((pid = waitpid(-1, &status, WNOHANG)) != -1 && pid != 0) {
        // find the child in the child list - remove it and free the address
        // back to the free address list				  
        TAILQ_FOREACH(address_slot, &child_list, next)	{
            if (address_slot->pid == pid) {
                vpnlog(LOG_NOTICE, "   --> Client with address = %s has hungup\n", address_slot->ip_address);
                TAILQ_REMOVE(&child_list, address_slot, next);
				lb_cur_connections--;
                if (address_slot->flags & VPN_ADDR_DELETE) { // address no longer valid?
                    free(address_slot);
					lb_max_connections--;
				}
                else {
                    TAILQ_INSERT_TAIL(&free_address_list, address_slot, next);
				}
                if (WIFSIGNALED(status))
                    vpnlog(LOG_WARNING, "Child process (pid %d) terminated with signal %d", pid, WTERMSIG(status));
                break;
            }
        }
    }

    if (pid == -1)
		if (errno != EINTR) {
	    //syslog(LOG_ERR, "Error waiting for child process: %m");
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
                vpnlog(LOG_ERR, "Error terminating child - err = %s\n", strerror(errno));
                break;
            }
    	TAILQ_REMOVE(&child_list, child, next);
		lb_cur_connections--;
        TAILQ_INSERT_TAIL(&free_address_list, child, next);
    }
        
    return 0;
}

/* -----------------------------------------------------------------------------
get mach asbolute time, for timeout purpose independent of date changes
----------------------------------------------------------------------------- */
int getabsolutetime(struct timeval *timenow)
{
    double	now;

    if (timeScaleSeconds == 0) {
		mach_timebase_info_data_t   timebaseInfo;
		if (mach_timebase_info(&timebaseInfo) == KERN_SUCCESS) {	// returns scale factor for ns
			timeScaleMicroSeconds = ((double) timebaseInfo.numer / (double) timebaseInfo.denom) / 1000;
			timeScaleSeconds = timeScaleMicroSeconds / 1000000;
		}
        else 
			return -1;
    }
	
    now = mach_absolute_time();
    timenow->tv_sec = now * timeScaleSeconds;
    timenow->tv_usec =  (now * timeScaleMicroSeconds) - ((double)timenow->tv_sec * 1000000);
    return 0;
}

