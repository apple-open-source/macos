/*
 * Copyright (c) 1999 - 2004 Apple Computer, Inc. All rights reserved.
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
 * client.c
 * - client side program to talk to ipconfigd
 */
/* 
 * Modification History
 *
 * September, 1999 	Dieter Siegmund (dieter@apple.com)
 * - initial revision
 * July 31, 2000	Dieter Siegmund (dieter@apple.com)
 * - changed to add set, and new implementation of waitall
 * - removed waitif
 * February 24, 2003	Dieter Siegmund (dieter@apple.com)
 * - added support to retrieve information about the netboot (bsdp)
 *   packet stored in the device tree
 */
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <mach/mach.h>
#include <mach/mach_error.h>
#include <servers/bootstrap.h>
#include <ctype.h>
#include <string.h>
#include <sysexits.h>

#include <netinet/in.h>
#include <netinet/udp.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/bootp.h>
#include <arpa/inet.h>

#include "ipconfig_ext.h"
#include "ipconfig_types.h"
#include "dhcp_options.h"
#include "dhcplib.h"
#include "bsdp.h"
#include "bsdplib.h"
#include "ioregpath.h"
#include "ipconfig.h"
#include <SystemConfiguration/SystemConfiguration.h>
#include <SystemConfiguration/SCValidation.h>

typedef int func_t(mach_port_t server, int argc, char * argv[]);
typedef func_t * funcptr_t;

static char * progname;
static char * command_name;

#define STARTUP_KEY	CFSTR("Plugin:IPConfiguration")

#define SHADOW_MOUNT_PATH_COMMAND	"shadow_mount_path"
#define SHADOW_FILE_PATH_COMMAND	"shadow_file_path"
#define MACHINE_NAME_COMMAND		"machine_name"

static void
on_alarm(int sigraised)
{
    exit(0);
}

#define WAIT_ALL_DEFAULT_TIMEOUT	90
#define WAIT_ALL_MAX_TIMEOUT		120

static void
key_appeared(SCDynamicStoreRef session, CFArrayRef changes, void * arg)
{
    exit(0);
}

static int
S_wait_all(mach_port_t server, int argc, char * argv[])
{
    CFMutableArrayRef	keys;
    SCDynamicStoreRef 	session;
    CFRunLoopSourceRef	rls;
    unsigned long	t = WAIT_ALL_DEFAULT_TIMEOUT;
    CFPropertyListRef	value;
    struct itimerval 	v;

    if (argc > 0) {
	t = strtoul(argv[0], 0, 0);
	if (t > WAIT_ALL_MAX_TIMEOUT) {
	    t = WAIT_ALL_MAX_TIMEOUT;
	}
    }

    session = SCDynamicStoreCreate(NULL, CFSTR("ipconfig command"), 
				   key_appeared, NULL);
    if (session == NULL) {
	fprintf(stderr, "SCDynamicStoreCreate failed: %s\n", 
		SCErrorString(SCError()));
	return (0);
    }
    keys = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
    CFArrayAppendValue(keys, STARTUP_KEY);
    SCDynamicStoreSetNotificationKeys(session, keys, NULL);
    CFRelease(keys);

    rls = SCDynamicStoreCreateRunLoopSource(NULL, session, 0);
    CFRunLoopAddSource(CFRunLoopGetCurrent(), rls, kCFRunLoopDefaultMode);
    CFRelease(rls);

    signal(SIGALRM, on_alarm);
    bzero(&v, sizeof(v));
    v.it_value.tv_sec = t;
    if (setitimer(ITIMER_REAL, &v, NULL) < 0) {
	perror("setitimer");
	return (0);
    }
    value = SCDynamicStoreCopyValue(session, STARTUP_KEY);
    if (value == NULL) {
	CFRunLoopRun();
	return (0);
    }
    CFRelease(session);
    return (0);
}

#if 0
static int
S_wait_if(mach_port_t server, int argc, char * argv[])
{
    return (0);
    if_name_t		name;
    kern_return_t	status;

    strlcpy(name, argv[0], sizeof(name));
    status = ipconfig_wait_if(server, name);
    if (status == KERN_SUCCESS) {
	return (0);
    }
    fprintf(stderr, "wait if %s failed, %s\n", name,
	    mach_error_string(status));
    return (1);
}
#endif 0

static int
S_bsdp_get_packet(mach_port_t server, int argc, char * argv[])
{
    CFDictionaryRef	chosen = NULL;
    struct dhcp *	dhcp;
    int			length;
    CFDataRef		response = NULL;
    int			ret = 1;

    if (getuid() != 0) {
	return (EX_NOPERM);
    }

    chosen = myIORegistryEntryCopyValue("IODeviceTree:/chosen");
    if (chosen == NULL) {
	goto done;
    }
    response = CFDictionaryGetValue(chosen, CFSTR("bsdp-response"));
    if (isA_CFData(response) == NULL) {
	response = CFDictionaryGetValue(chosen, CFSTR("bootp-response"));
	if (isA_CFData(response) == NULL) {
	    goto done;
	}
    }
    dhcp = (struct dhcp *)CFDataGetBytePtr(response);
    length = CFDataGetLength(response);
    bsdp_print_packet(dhcp, length, 0);
    ret = 0;
 done:
    return (ret);
}

static int
S_bsdp_option(mach_port_t server, int argc, char * argv[])
{
    CFDictionaryRef	chosen = NULL;
    void *		data = NULL;
    int			data_len;
    struct dhcp *	dhcp;
    int			length;
    dhcpol_t		options;
    CFDataRef		response = NULL;
    int			ret = 1;
    int			tag = 0;
    dhcpol_t		vendor_options;
    int			vendor_tag = 0;

    if (getuid() != 0) {
	return (EX_NOPERM);
    }

    chosen = myIORegistryEntryCopyValue("IODeviceTree:/chosen");
    if (chosen == NULL) {
	goto done;
    }
    response = CFDictionaryGetValue(chosen, CFSTR("bsdp-response"));
    if (isA_CFData(response) == NULL) {
	response = CFDictionaryGetValue(chosen, CFSTR("bootp-response"));
	if (isA_CFData(response) == NULL) {
	    goto done;
	}
    }
    dhcp = (struct dhcp *)CFDataGetBytePtr(response);
    length = CFDataGetLength(response);
    if (dhcpol_parse_packet(&options, dhcp, length, NULL) == FALSE) {
	goto done;
    }
    if (strcmp(argv[0], SHADOW_MOUNT_PATH_COMMAND) == 0) {
	tag = dhcptag_vendor_specific_e;
	vendor_tag = bsdptag_shadow_mount_path_e;
    }
    else if (strcmp(argv[0], SHADOW_FILE_PATH_COMMAND) == 0) {
	tag = dhcptag_vendor_specific_e;
	vendor_tag = bsdptag_shadow_file_path_e;
    }
    else if (strcmp(argv[0], MACHINE_NAME_COMMAND) == 0) {
	tag = dhcptag_vendor_specific_e;
	vendor_tag = bsdptag_machine_name_e;
    }
    else {
	tag = atoi(argv[0]);
	if (argc == 2) {
	    vendor_tag = atoi(argv[1]);
	}
    }
    if (tag == dhcptag_vendor_specific_e && vendor_tag != 0) {
	if (dhcpol_parse_vendor(&vendor_options, &options, NULL) == FALSE) {
	    goto done;
	}
	data = dhcpol_get(&vendor_options, vendor_tag, &data_len);
	if (data != NULL) {
	    dhcptype_print(bsdptag_type(vendor_tag), data, data_len);
	    ret = 0;
	}
	dhcpol_free(&vendor_options);
    }
    else {
	const dhcptag_info_t * entry;

	entry = dhcptag_info(tag);
	if (entry == NULL) {
	    goto done;
	}
	data = dhcpol_get(&options, tag, &data_len);
	if (data != NULL) {
	    dhcptype_print(entry->type, data, data_len);
	    ret = 0;
	}
    }
 done:
    if (data != NULL) {
	free(data);
    }
    if (chosen != NULL) {
	CFRelease(chosen);
    }
    return (ret);
}

static int
S_if_addr(mach_port_t server, int argc, char * argv[])
{
    struct in_addr	ip;
    if_name_t		name;
    kern_return_t	status;

    strlcpy(name, argv[0], sizeof(name));
    status = ipconfig_if_addr(server, name, (ip_address_t *)&ip);
    if (status == KERN_SUCCESS) {
	printf("%s\n", inet_ntoa(ip));
	return (0);
    }
    fprintf(stderr, "get if addr %s failed, %s\n", name,
	    mach_error_string(status));
    return (1);
}

static int
S_if_count(mach_port_t server, int argc, char * argv[])
{
    int 		count = 0;
    kern_return_t	status;

    status = ipconfig_if_count(server, &count);
    if (status == KERN_SUCCESS) {
	printf("%d\n", count);
	return (0);
    }
    fprintf(stderr, "get if count failed, %s\n", mach_error_string(status));
    return (1);
}

static int
S_get_option(mach_port_t server, int argc, char * argv[])
{
    char		buf[1024];
    inline_data_t 	data;
    unsigned int 	data_len = sizeof(data);
    if_name_t		name;
    dhcpo_err_str_t	err;
    int			tag;
    kern_return_t	status;

    strlcpy(name, argv[0], sizeof(name));

    tag = dhcptag_with_name(argv[1]);
    if (tag == -1)
	tag = atoi(argv[1]);
    status = ipconfig_get_option(server, name, tag, data, &data_len);
    if (status == KERN_SUCCESS) {
	if (dhcptag_to_str(buf, sizeof(buf), tag, data, data_len, &err)) {
	    printf("%s\n", buf);
	    return (0);
	}
	fprintf(stderr, "couldn't convert the option, %s\n",
		err.str);
    }
    else {
	fprintf(stderr, "ipconfig_get_option failed, %s\n", 
		mach_error_string(status));
    }
    return (1);
}

static int
S_get_packet(mach_port_t server, int argc, char * argv[])
{
    inline_data_t 	data;
    unsigned int 	data_len = sizeof(data);
    if_name_t		name;
    int			ret = 1;
    kern_return_t	status;

    strlcpy(name, argv[0], sizeof(name));
    status = ipconfig_get_packet(server, name, data, &data_len);
    if (status == KERN_SUCCESS) {
	dhcp_print_packet((struct dhcp *)data, data_len);
	ret = 0;
    }
    return (ret);
}

static __inline__ boolean_t
ipconfig_method_from_string(const char * m, ipconfig_method_t * method)
{
    if (strcmp(m, "BOOTP") == 0) {
	*method = ipconfig_method_bootp_e;
    }
    else if (strcmp(m, "DHCP") == 0) {
	*method = ipconfig_method_dhcp_e;
    }
    else if (strcmp(m, "MANUAL") == 0) {
	*method = ipconfig_method_manual_e;
    }
    else if (strcmp(m, "INFORM") == 0) {
	*method = ipconfig_method_inform_e;
    }
    else if (strcmp(m, "LINKLOCAL") == 0) {
	*method = ipconfig_method_linklocal_e;
    }
    else if (strcmp(m, "FAILOVER") == 0) {
	*method = ipconfig_method_failover_e;
    }
    else if (strcmp(m, "NONE") == 0) {
	*method = ipconfig_method_none_e;
    }
    else {
	return (FALSE);
    }
    return (TRUE);
}

static void
get_method_information(int argc, char * argv[],
		       const char * cmd, ipconfig_method_t * method_p,
		       ipconfig_method_data_t * data, int * data_len_p)
{
    char *			method_name;
    
    method_name = argv[0];
    if (ipconfig_method_from_string(method_name, method_p) == FALSE) {
	fprintf(stderr, "ipconfig: %s: method '%s' unknown\n", 
		cmd, method_name);
	fprintf(stderr, 
		"must be MANUAL, INFORM, BOOTP, DHCP, FAILOVER, or NONE\n");
	exit(1);
    }
    argc--;
    argv++;
    switch (*method_p) {
      case ipconfig_method_failover_e:
	  if (argc != 1 && argc != 2 && argc != 3) {
	      fprintf(stderr, "usage: ipconfig %s en0 %s <ip> "
		      "[ <mask> [ <timeout> ] ]\n",
		      cmd, method_name);
	      exit(1);
	  }
	  goto common;
      case ipconfig_method_inform_e:
      case ipconfig_method_manual_e:
	  if (argc != 1 && argc != 2) {
	      fprintf(stderr, "usage: ipconfig %s en0 %s <ip> [ <mask> ]\n",
		      cmd, method_name);
	      exit(1);
	  }
      common:
	  bzero(data, *data_len_p);
	  if (*data_len_p < (sizeof(*data) + sizeof(data->ip[0]))) {
	      fprintf(stderr, "Invalid size passed %d < %ld\n",
		      *data_len_p, sizeof(*data) + sizeof(data->ip[0]));
	      exit(1);
	  }
	  data->n_ip = 1;
	  if (inet_aton(argv[0], &data->ip[0].addr) == 0) {
	      fprintf(stderr, "Invalid IP address %s\n", argv[0]);
	      exit(1);
	  }
	  if (argc >= 2) {
	      if (inet_aton(argv[1], &data->ip[0].mask) == 0) {
		  fprintf(stderr, "Invalid IP mask %s\n", argv[1]);
		  exit(1);
	      }
	  }
	  if (argc >= 3) {
	      data->u.failover_timeout = strtoul(argv[2], NULL, 0);
	  }
	  *data_len_p = sizeof(*data) + sizeof(data->ip[0]);
	  break;
      default:
	  if (argc) {
	      fprintf(stderr, "too many arguments for method\n");
	      exit(1);
	  }
	  *data_len_p = 0;
	  break;
    }
    return;
}

static int
S_set(mach_port_t server, int argc, char * argv[])
{
    char			buf[IPCONFIG_METHOD_DATA_MIN_SIZE];
    ipconfig_method_data_t *	data = (ipconfig_method_data_t *)buf;
    int				data_len;
    if_name_t			name;
    char *			method_name;
    ipconfig_method_t		method;
    kern_return_t		status;
    ipconfig_status_t		ipstatus = ipconfig_status_success_e;

    strcpy(name, argv[0]);
    argv++;
    argc--;

    method_name = argv[0];
    data_len = sizeof(buf);
    get_method_information(argc, argv, command_name, &method, data, &data_len);
    status = ipconfig_set(server, name, method, (void *)data, data_len,
			  &ipstatus);
    if (status != KERN_SUCCESS) {
	mach_error("ipconfig_set failed", status);
	return (1);
    }
    if (ipstatus != ipconfig_status_success_e) {
	fprintf(stderr, "ipconfig_set %s %s failed: %s\n",
		name, method_name, ipconfig_status_string(ipstatus));
	return (1);
    }
    return (0);
}

static int
S_set_verbose(mach_port_t server, int argc, char * argv[])
{
    ipconfig_status_t		ipstatus = ipconfig_status_success_e;
    kern_return_t		status;
    int				verbose;

    verbose = strtol(argv[0], NULL, 0);
    errno = 0;
    if (verbose == 0 && errno != 0) {
	fprintf(stderr, "conversion to integer of %s failed\n", argv[0]);
	return (1);
    }
    status = ipconfig_set_verbose(server, verbose, &ipstatus);
    if (status != KERN_SUCCESS) {
	return (1);
    }
    if (ipstatus != ipconfig_status_success_e) {
	fprintf(stderr, "setverbose failed: %s\n",
		ipconfig_status_string(ipstatus));
	return (1);
    }
    return (0);
}

#ifdef IPCONFIG_TEST_NO_ENTRY
static int
S_set_something(mach_port_t server, int argc, char * argv[])
{
    ipconfig_status_t		ipstatus = ipconfig_status_success_e;
    kern_return_t		status;
    int				verbose;

    verbose = strtol(argv[0], NULL, 0);
    errno = 0;
    if (verbose == 0 && errno != 0) {
	fprintf(stderr, "conversion to integer of %s failed\n", argv[0]);
	return (1);
    }
    status = ipconfig_set_something(server, verbose, &ipstatus);
    if (status != KERN_SUCCESS) {
	return (1);
    }
    if (ipstatus != ipconfig_status_success_e) {
	fprintf(stderr, "setsomething failed: %s\n",
		ipconfig_status_string(ipstatus));
	return (1);
    }
    return (0);
}
#endif IPCONFIG_TEST_NO_ENTRY

static int
S_add_or_set_service(mach_port_t server, int argc, char * argv[], bool add)
{
    char			buf[IPCONFIG_METHOD_DATA_MIN_SIZE];
    ipconfig_method_data_t *	data = (ipconfig_method_data_t *)buf;
    int				data_len;
    if_name_t			name;
    char *			method_name;
    ipconfig_method_t		method;
    inline_data_t 		service_id;
    unsigned int 		service_id_len = sizeof(service_id);
    kern_return_t		status;
    ipconfig_status_t		ipstatus = ipconfig_status_success_e;

    strcpy(name, argv[0]);
    argv++;
    argc--;

    method_name = argv[0];
    data_len = sizeof(buf);
    get_method_information(argc, argv, command_name, &method, data, &data_len);
    if (add) {
	status = ipconfig_add_service(server, name, method,
				      (void *)data, data_len,
				      service_id, &service_id_len, &ipstatus);
    }
    else {
	status = ipconfig_set_service(server, name, method,
				      (void *)data, data_len,
				      service_id, &service_id_len, &ipstatus);
    }
    if (status != KERN_SUCCESS) {
	fprintf(stderr, "ipconfig_%s_service failed, %s\n", add ? "add" : "set",
		mach_error_string(status));
	return (1);
    }
    if (ipstatus != ipconfig_status_success_e) {
	fprintf(stderr, "ipconfig_%s_service %s %s failed: %s\n",
		add ? "add" : "set",
		name, method_name, ipconfig_status_string(ipstatus));
	return (1);
    }
    printf("%.*s\n", service_id_len, service_id);
    return (0);
}

static int
S_add_service(mach_port_t server, int argc, char * argv[])
{
    return (S_add_or_set_service(server, argc, argv, TRUE));
}

static int
S_set_service(mach_port_t server, int argc, char * argv[])
{
    return (S_add_or_set_service(server, argc, argv, FALSE));
}

static int
S_remove_service_with_id(mach_port_t server, int argc, char * argv[])
{
    inline_data_t 		service_id;
    unsigned int 		service_id_len;
    kern_return_t		status;
    ipconfig_status_t		ipstatus = ipconfig_status_success_e;

    service_id_len = strlen(argv[0]);
    if (service_id_len > sizeof(service_id)) {
	service_id_len = sizeof(service_id); 
    }
    memcpy(service_id, argv[0], service_id_len);
    status = ipconfig_remove_service_with_id(server, service_id, service_id_len,
					     &ipstatus);
    if (status != KERN_SUCCESS) {
	mach_error("ipconfig_remove_service_with_id failed", status);
	return (1);
    }
    if (ipstatus != ipconfig_status_success_e) {
	fprintf(stderr, "ipconfig_remove_service_with_id %s failed: %s\n",
		argv[0], ipconfig_status_string(ipstatus));
	return (1);
    }
    return (0);
}

static int
S_find_service(mach_port_t server, int argc, char * argv[])
{
    char			buf[IPCONFIG_METHOD_DATA_MIN_SIZE];
    ipconfig_method_data_t *	data = (ipconfig_method_data_t *)buf;
    int				data_len;
    if_name_t			name;
    boolean_t			exact = FALSE;
    char *			method_name;
    ipconfig_method_t		method;
    inline_data_t 		service_id;
    unsigned int 		service_id_len = sizeof(service_id);
    kern_return_t		status;
    ipconfig_status_t		ipstatus = ipconfig_status_success_e;

    strcpy(name, argv[0]);
    argv++;
    argc--;

    if (strcasecmp(argv[0], "exact") == 0) {
	exact = TRUE;
	argc--;
	argv++;
    }
    method_name = argv[0];
    data_len = sizeof(buf);
    get_method_information(argc, argv, command_name, &method, data, &data_len);
    status = ipconfig_find_service(server, name, exact,
				   method, (void *)data, data_len,
				   service_id, &service_id_len, &ipstatus);
    if (status != KERN_SUCCESS) {
	mach_error("ipconfig_find_service failed", status);
	return (1);
    }
    if (ipstatus != ipconfig_status_success_e) {
	fprintf(stderr, "ipconfig_find_service %s %s failed: %s\n",
		name, method_name, ipconfig_status_string(ipstatus));
	return (1);
    }
    printf("%.*s\n", service_id_len, service_id);
    return (0);
}

static int
S_remove_service(mach_port_t server, int argc, char * argv[])
{
    char			buf[IPCONFIG_METHOD_DATA_MIN_SIZE];
    ipconfig_method_data_t *	data = (ipconfig_method_data_t *)buf;
    int				data_len;
    if_name_t			name;
    char *			method_name;
    ipconfig_method_t		method;
    kern_return_t		status;
    ipconfig_status_t		ipstatus = ipconfig_status_success_e;

    strcpy(name, argv[0]);
    argv++;
    argc--;

    method_name = argv[0];
    data_len = sizeof(buf);
    get_method_information(argc, argv, command_name, &method, data, &data_len);
    status = ipconfig_remove_service(server, name, method,
				     (void *)data, data_len, &ipstatus);
    if (status != KERN_SUCCESS) {
	mach_error("ipconfig_remove_service failed", status);
	return (1);
    }
    if (ipstatus != ipconfig_status_success_e) {
	fprintf(stderr, "ipconfig_remove_service %s %s failed: %s\n",
		name, method_name, ipconfig_status_string(ipstatus));
	return (1);
    }
    return (0);
}


static const struct command_info {
    const char *command;
    funcptr_t	func;
    int		argc;
    const char *usage;
    int		display;
    int		no_server;
} commands[] = {
    { "waitall", S_wait_all, 0, "[ timeout secs ]", 1, 1 },
    { "getifaddr", S_if_addr, 1, "<interface name>", 1, 0 },
#if 0
    { "waitif", S_wait_if, 1, " <interface name>", 1, 0 },
#endif 0
    { "ifcount", S_if_count, 0, "", 1, 0 },
    { "getoption", S_get_option, 2, 
      " <interface name | \"\" > <option name> | <option code>", 1, 0 },
    { "getpacket", S_get_packet, 1, " <interface name>", 1, 0 },
    { "set", S_set, 2, 
      " <interface name> < BOOTP | MANUAL | DHCP | INFORM | FAILOVER | NONE > <method args>", 1, 0 },
    { "netbootoption", S_bsdp_option, 1, "<option> [<vendor option>] | " 
      SHADOW_MOUNT_PATH_COMMAND " | " SHADOW_FILE_PATH_COMMAND
      " | " MACHINE_NAME_COMMAND, 0, 1 },
    { "netbootpacket", S_bsdp_get_packet, 0, "", 0, 1 },
    { "setverbose", S_set_verbose, 1, "0 | 1", 1, 0 },
#ifdef IPCONFIG_TEST_NO_ENTRY
    { "setsomething", S_set_something, 1, "0 | 1", 1, 0 },
#endif IPCONFIG_TEST_NO_ENTRY
    { "addService", S_add_service, 2, 
      " <interface name> < BOOTP | MANUAL | DHCP | INFORM | FAILOVER | NONE > <method args>", 0, 0 },
    { "setService", S_set_service, 2, 
      " <interface name> < BOOTP | MANUAL | DHCP | INFORM | FAILOVER | NONE > <method args>", 0, 0 },
    { "findService", S_find_service, 2, 
      " <interface name> [ noexact ] < BOOTP | MANUAL | DHCP | INFORM | FAILOVER | NONE > <method args>", 0, 0 },
    { "removeServiceWithId", S_remove_service_with_id, 1, 
      " <service ID>", 0, 0 },
    { "removeService", S_remove_service, 2, 
      " <interface name> < BOOTP | MANUAL | DHCP | INFORM | FAILOVER | NONE > <method args>", 0, 0 },
    { NULL, NULL, 0, NULL, 0, 0 },
};

void
usage()
{
    int i;
    fprintf(stderr, "usage: %s <command> <args>\n", progname);
    fprintf(stderr, "where <command> is one of ");
    for (i = 0; commands[i].command; i++) {
	if (commands[i].display) {
	    fprintf(stderr, "%s%s",  i == 0 ? "" : ", ",
		    commands[i].command);
	}
    }
    fprintf(stderr, "\n");
    exit(1);
}

static const struct command_info *
S_lookup_command(char * cmd, int argc)
{
    int i;

    for (i = 0; commands[i].command; i++) {
	if (strcasecmp(cmd, commands[i].command) == 0) {
	    if (argc < commands[i].argc) {
		if (commands[i].display) {
		    fprintf(stderr, "usage: %s %s\n", commands[i].command,
			    commands[i].usage ? commands[i].usage : "");
		}
		exit(1);
	    }
	    return commands + i;
	}
    }
    return (NULL);
}

int
main(int argc, char * argv[])
{
    const struct command_info *	command;
    mach_port_t			server;
    kern_return_t		status;

    progname = argv[0];
    if (argc < 2)
	usage();

    argv++; argc--;

#if 0
    {
	struct in_addr	dhcp_ni_addr;
	char		dhcp_ni_tag[128];
	int		len;
	kern_return_t	status;

	len = sizeof(dhcp_ni_addr);
	status = ipconfig_get_option(server, IPCONFIG_IF_ANY,
				     112, (void *)&dhcp_ni_addr, &len);
	if (status != KERN_SUCCESS) {
	    fprintf(stderr, "get 112 failed, %s\n", mach_error_string(status));
	}
	else {
	    fprintf(stderr, "%d bytes:%s\n", len, inet_ntoa(dhcp_ni_addr));
	}
	len = sizeof(dhcp_ni_tag) - 1;
	status = ipconfig_get_option(server, IPCONFIG_IF_ANY,
				     113, (void *)&dhcp_ni_tag, &len);
	if (status != KERN_SUCCESS) {
	    fprintf(stderr, "get 113 failed, %s\n", mach_error_string(status));
	}
	else {
	    dhcp_ni_tag[len] = '\0';
	    fprintf(stderr, "%d bytes:%s\n", len, dhcp_ni_tag);
	}
	
    }
#endif 0
    command_name = argv[0];
    command = S_lookup_command(command_name, argc - 1);
    if (command == NULL)
	usage();
    argv++; argc--;
    if (command->no_server == 0) {
	status = ipconfig_server_port(&server);
	switch (status) {
	case BOOTSTRAP_SUCCESS:
	    break;
	case BOOTSTRAP_UNKNOWN_SERVICE:
	    fprintf(stderr, "ipconfig server not active\n");
	    /* start it maybe??? */
	    exit(1);
	default:
	    mach_error("ipconfig_server_port failed", status);
	    exit(1);
	}
    }
    exit ((*command->func)(server, argc, argv));
}
