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
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <fcntl.h>
#include <syslog.h>
#include <netdb.h>
#include <paths.h>
#include <sys/queue.h>
		
#include <sys/param.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/sysctl.h>
#include <pthread.h>
#include <net/if.h>

#include <CoreFoundation/CoreFoundation.h>
#include <SystemConfiguration/SystemConfiguration.h>

#include "vpnd.h"
#include "cf_utils.h"
#include "ipsec_utils.h"
#include "vpnoptions.h"
#include "vpnplugins.h"
#include "RASSchemaDefinitions.h"

extern int got_terminate(void);

static int			secure_transport = 0;
static int 			key_preference = -1;

static CFMutableDictionaryRef		ipsec_conf = NULL;

static struct vpn_params *current_params = 0;

static pthread_t resolverthread = 0;
static struct in_addr peer_address;
static char remoteaddress[255];
static int 	resolverfds[2];

static int plugin_listen();
static void plugin_close();
static int plugin_get_args(struct vpn_params *params, int reload);

//-----------------------------------------------------------------------------
//	process_options
//-----------------------------------------------------------------------------
void ipsec_process_options(struct vpn_params *params)
{
    
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
void *ipsec_resolver_thread(void *arg)
{
    struct hostent 	*host;
    char		result = -1;
	int			count, fd;
	u_int8_t	rd8; 

    if (pthread_detach(pthread_self()) == 0) {
        
        // try to resolve the name
        if ((host = gethostbyname(remoteaddress))) {

			for (count = 0; host->h_addr_list[count]; count++);
		
			rd8 = 0;
			fd = open("/dev/random", O_RDONLY);
			if (fd) {
				read(fd, &rd8, sizeof(rd8));
				close(fd);
			}

			if (count) {
				peer_address = *(struct in_addr *)host->h_addr_list[rd8 % count];
			} else {
				bzero(&peer_address, sizeof(peer_address));
			}
            result = 0;
        }
    }

    write(resolverfds[1], &result, 1);
    return 0;
}

// ----------------------------------------------------------------------------
//	process_prefs
// ----------------------------------------------------------------------------
int ipsec_process_prefs(struct vpn_params *params)
{
	char *errstr, c;
	CFStringRef	string;
	
	if (ipsec_conf) {
		CFRelease(ipsec_conf);
		ipsec_conf = NULL;
	}

	ipsec_conf = (CFMutableDictionaryRef)CFDictionaryGetValue(params->serverRef, kRASEntIPSec);
	if (ipsec_conf == NULL) {
		vpnlog(LOG_ERR, "IPSec plugin: IPSec dictionary not present\n");
		goto fail;
	}
		
	ipsec_conf = CFDictionaryCreateMutableCopy(NULL, 0, ipsec_conf);

	remoteaddress[0] = 0;
	string  = CFDictionaryGetValue(ipsec_conf, kRASPropIPSecRemoteAddress);
	if (isString(string))
		CFStringGetCString(string, remoteaddress, sizeof(remoteaddress), kCFStringEncodingUTF8);

	if (inet_aton(remoteaddress, &peer_address) == 0) {
			
		if (pipe(resolverfds) < 0) {
			vpnlog(LOG_ERR, "IPSec plugin: failed to create pipe for gethostbyname\n");
			goto fail;
		}

		if (pthread_create(&resolverthread, NULL, ipsec_resolver_thread, NULL)) {
			vpnlog(LOG_ERR, "IPSec plugin: failed to create thread for gethostbyname...\n");
			close(resolverfds[0]);
			close(resolverfds[1]);
			goto fail;
		}
		
		while (read(resolverfds[0], &c, 1) != 1) {
			if (got_terminate()) {
				pthread_cancel(resolverthread);
				break;
			}
		}
		
		close(resolverfds[0]);
		close(resolverfds[1]);
		
		if (got_terminate())
			goto fail;
		
		if (c) {
			vpnlog(LOG_ERR, "IPSec plugin: Host '%s' not found...\n", remoteaddress);
			goto fail;
		}
		
		string = CFStringCreateWithCString(0, addr2ascii(AF_INET, &peer_address, sizeof(peer_address), 0), kCFStringEncodingASCII);
		CFDictionarySetValue(ipsec_conf, kRASPropIPSecRemoteAddress, string);
		CFRelease(string);
	}

	// verify the dictionary
	if (IPSecValidateConfiguration(ipsec_conf, &errstr)) {

		vpnlog(LOG_ERR, "IPSec plugin: Incorrect preferences (%s)\n", errstr);
		goto fail;
	}
	
    return 0;

fail:
	if (ipsec_conf) {
		CFRelease(ipsec_conf);
		ipsec_conf = NULL;
	}
    return -1;
}

#if 0
//-----------------------------------------------------------------------------
//	convert a mask to a prefix
//-----------------------------------------------------------------------------
static int mask_to_prefix (in_addr_t mask)
{   
	u_int32_t   prefix = 0;
	int			i;
	
#define IS_SET(field, bit)  (field & (1 << bit))

	/* count the bits  set to 1 */
	for (i = 31; i >= 0 && IS_SET(mask, i); i--)
		prefix++;
	
	/* verify the other bits are set to 0 */
	for (; i >= 0 && !IS_SET(mask, i); i--);
	
	/* check for incorrect prefix */
	if (i >= 0)
		return 0;
		
	return prefix;
}
#endif

//-----------------------------------------------------------------------------
//	add_builtin_plugin for non plugin based connection
//-----------------------------------------------------------------------------
int ipsec_add_builtin_plugin(struct vpn_params* params, void *channel)
{
	struct vpn_channel *chan = (struct vpn_channel *)channel;
	
    bzero(chan, sizeof(struct vpn_channel));
    chan->get_pppd_args = plugin_get_args;
    chan->listen = plugin_listen;
    chan->close = plugin_close;

    return 0;
}

/* ----------------------------------------------------------------------------- 
    builtin ipsec get args: FIX ME
----------------------------------------------------------------------------- */
int plugin_get_args(struct vpn_params *params, int reload)
{
	current_params = params;
	return 0;
}

//-----------------------------------------------------------------------------
//	builtin ipsec listen
//-----------------------------------------------------------------------------
static int plugin_listen()
{
	int err;
	char *errstr;
	
     /* add security policies */
	err = IPSecApplyConfiguration(ipsec_conf, &errstr);
	if (err) {
		vpnlog(LOG_ERR, "IPSec plugin: cannot configure racoon files (%s)...\n", errstr);
		return -1;
	}

	err = IPSecInstallPolicies(ipsec_conf, -1, &errstr);
	if (err) {
		vpnlog(LOG_ERR, "IPSec plugin: cannot configure kernel policies (%s)...\n", errstr);
		IPSecRemoveConfiguration(ipsec_conf, &errstr);
		return -1;
	}

	/* set IPSec Key management to prefer most recent key */
	if (IPSecSetSecurityAssociationsPreference(&key_preference, 0))
		vpnlog(LOG_ERR, "IPSec plugin: cannot set IPSec Key management preference (error %d)\n", errno);


	secure_transport = 1;

    return 0;
}

//-----------------------------------------------------------------------------
//	builtin ipsec close
//-----------------------------------------------------------------------------
static void plugin_close()
{
	int err;
	char *errstr;

    /* remove security policies */
    if (secure_transport) {            
        err = IPSecRemoveConfiguration(ipsec_conf, &errstr);
		if (err)
			vpnlog(LOG_ERR, "IPSec plugin: cannot remove IPSec configuration (%s)...\n", errstr);
        err = IPSecRemovePolicies(ipsec_conf, -1, &errstr);            
		if (err)
			vpnlog(LOG_ERR, "IPSec plugin: cannot delete kernel policies (%s)...\n", errstr);
		/* restore IPSec Key management preference */
        if (IPSecSetSecurityAssociationsPreference(0, key_preference))
            vpnlog(LOG_ERR, "L2TP plugin: cannot reset IPSec Key management preference (error %d)\n", errno);
    }
}

