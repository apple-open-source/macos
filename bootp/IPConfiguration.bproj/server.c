/*
 * Copyright (c) 2000-2023 Apple Inc. All rights reserved.
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
#include <unistd.h>
#include <stdlib.h>
#include <mach/mach.h>
#include <mach/mach_error.h>
#include <servers/bootstrap.h>
#include <syslog.h>
#include <CoreFoundation/CFMachPort.h>
#include <CoreFoundation/CFData.h>
#include <CoreFoundation/CFPropertyList.h>
#include <SystemConfiguration/SCPrivate.h>
#include <bsm/libbsm.h>
#include <TargetConditionals.h>

#include "symbol_scope.h"
#include "cfutil.h"
#include "ipconfigServer.h"
#include "ipconfigd.h"
#include "ipconfig_ext.h"
#include "globals.h"
#include "IPConfigurationUtilPrivate.h"
#include <dispatch/private.h>

static uid_t S_uid = -1;
static pid_t S_pid = -1;


#if TARGET_OS_OSX
#define	kIPConfigurationServiceEntitlement	NULL

static boolean_t
S_has_entitlement(audit_token_t token, CFStringRef entitlement)
{
    return (FALSE);
}

#else /* TARGET_OS_OSX */
#define	kIPConfigurationServiceEntitlement	CFSTR("com.apple.IPConfigurationService")	/* boolean */

#include <Security/SecTask.h>
static boolean_t
S_has_entitlement(audit_token_t token, CFStringRef entitlement)
{
    boolean_t		ret = FALSE;
    SecTaskRef		task;

    task = SecTaskCreateWithAuditToken(NULL, token);
    if (task != NULL) {
	CFBooleanRef	allow;

	allow = SecTaskCopyValueForEntitlement(task, entitlement, NULL);
	if (allow != NULL) {
	    if (isA_CFBoolean(allow) != NULL) {
		ret = CFBooleanGetValue(allow);
	    }
	    CFRelease(allow);
	}
	CFRelease(task);
    }
    return (ret);
}
#endif /* TARGET_OS_OSX */

static void
S_process_audit_token(audit_token_t audit_token)
{
    S_uid = -1;
    S_pid = -1;
    audit_token_to_au32(audit_token,
			NULL,		/* auidp */
			&S_uid,		/* euid */
			NULL,		/* egid */
			NULL,		/* ruid */
			NULL,		/* rgid */
			&S_pid,		/* pid */
			NULL,		/* asid */
			NULL);		/* tid */
    return;
}

static boolean_t
S_IPConfigurationServiceOperationAllowed(audit_token_t audit_token)
{
    S_process_audit_token(audit_token);
    if (S_uid == 0
	|| S_has_entitlement(audit_token,
			     kIPConfigurationServiceEntitlement)) {
	return (TRUE);
    }
    return (FALSE);
}

static ipconfig_status_t
method_info_from_xml_data(xmlData_t xml_data,
			  mach_msg_type_number_t xml_data_len,
			  ipconfig_method_info_t info)
{
    CFPropertyListRef	plist = NULL;
    ipconfig_status_t	status;

    if (xml_data != NULL) {
	plist = my_CFPropertyListCreateWithBytePtrAndLength(xml_data,
							    xml_data_len);
    }
    status = ipconfig_method_info_from_plist(plist, info);
    if (plist != NULL) {
	CFRelease(plist);
    }
    return (status);
}

PRIVATE_EXTERN kern_return_t
_ipconfig_if_addr(mach_port_t p, InterfaceName name,
		  ip_address_t * addr, ipconfig_status_t * status)
{
    *status = get_if_addr(InterfaceNameNulTerminate(name), addr);
    return (KERN_SUCCESS);
}

PRIVATE_EXTERN kern_return_t
_ipconfig_if_count(mach_port_t p, int * count)
{
    *count = get_if_count();
    return (KERN_SUCCESS);
}

PRIVATE_EXTERN kern_return_t
_ipconfig_get_option(mach_port_t p, InterfaceName name, int option_code,
		     dataOut_t * option_data,
		     mach_msg_type_number_t * option_dataCnt,
		     ipconfig_status_t * status)

{
    *status = get_if_option(InterfaceNameNulTerminate(name), option_code,
			    option_data, option_dataCnt);
    return (KERN_SUCCESS);
}

PRIVATE_EXTERN kern_return_t
_ipconfig_get_packet(mach_port_t p, InterfaceName name,
		     dataOut_t * packet_data,
		     mach_msg_type_number_t * packet_dataCnt,
		     ipconfig_status_t * status)
{
    *status = get_if_packet(InterfaceNameNulTerminate(name),
			    packet_data, packet_dataCnt);
    return (KERN_SUCCESS);
}

PRIVATE_EXTERN kern_return_t
_ipconfig_get_v6_packet(mach_port_t p, InterfaceName name,
			dataOut_t * packet_data,
			mach_msg_type_number_t * packet_dataCnt,
			ipconfig_status_t * status)
{
    *status = get_if_v6_packet(InterfaceNameNulTerminate(name),
			       packet_data, packet_dataCnt);
    return (KERN_SUCCESS);
}

PRIVATE_EXTERN kern_return_t
_ipconfig_set(mach_port_t p, InterfaceName name,
	      xmlData_t xml_data,
	      mach_msg_type_number_t xml_data_len,
	      ipconfig_status_t * ret_status,
	      audit_token_t audit_token)
{
    ipconfig_method_info	info;
    ipconfig_status_t		status;

    S_process_audit_token(audit_token);
    if (S_uid != 0) {
	status = ipconfig_status_permission_denied_e;
	goto done;
    }
    ipconfig_method_info_init(&info);
    status = method_info_from_xml_data(xml_data, xml_data_len, &info);
    if (status != ipconfig_status_success_e) {
	goto done;
    }
    status = set_if(InterfaceNameNulTerminate(name), &info);
    ipconfig_method_info_free(&info);

 done:
    if (xml_data != NULL) {
	(void)vm_deallocate(mach_task_self(), (vm_address_t)xml_data,
			    xml_data_len);
    }
    *ret_status = status;
    return (KERN_SUCCESS);
}

PRIVATE_EXTERN kern_return_t
_ipconfig_set_verbose(mach_port_t p, int verbose,
		      ipconfig_status_t * status,
		      audit_token_t audit_token)
{
    *status = ipconfig_status_permission_denied_e;
    return (KERN_SUCCESS);
}

#ifdef IPCONFIG_TEST_NO_ENTRY
PRIVATE_EXTERN kern_return_t
_ipconfig_set_something(mach_port_t p, int verbose,
			ipconfig_status_t * status)
{
    return (KERN_SUCCESS);
}
#endif /* IPCONFIG_TEST_NO_ENTRY */

PRIVATE_EXTERN kern_return_t
_ipconfig_add_service(mach_port_t p, 
		      InterfaceName name,
		      xmlData_t xml_data,
		      mach_msg_type_number_t xml_data_len,
		      ServiceID service_id,
		      ipconfig_status_t * ret_status,
		      audit_token_t audit_token)
{
    ipconfig_method_info	info;
    CFPropertyListRef		plist = NULL;
    ipconfig_status_t		status;

    if (!S_IPConfigurationServiceOperationAllowed(audit_token)) {
	status = ipconfig_status_permission_denied_e;
	goto done;
    }
    if (xml_data != NULL) {
	plist = my_CFPropertyListCreateWithBytePtrAndLength(xml_data,
							    xml_data_len);
    }
    ipconfig_method_info_init(&info);
    status = ipconfig_method_info_from_plist(plist, &info);
    if (status != ipconfig_status_success_e) {
	goto done;
    }
    ServiceIDClear(service_id);
    status = add_service(InterfaceNameNulTerminate(name),
			 &info, service_id, plist, S_pid);
    ipconfig_method_info_free(&info);

 done:
    if (plist != NULL) {
	CFRelease(plist);
    }
    if (xml_data != NULL) {
	(void)vm_deallocate(mach_task_self(), (vm_address_t)xml_data,
			    xml_data_len);
    }
    *ret_status = status;
    return (KERN_SUCCESS);
}

kern_return_t
_ipconfig_set_service(mach_port_t p, 
		      InterfaceName name,
		      xmlData_t xml_data,
		      mach_msg_type_number_t xml_data_len,
		      ServiceID service_id,
		      ipconfig_status_t * ret_status,
		      audit_token_t audit_token)
{
    ipconfig_method_info	info;
    ipconfig_status_t		status;

    if (!S_IPConfigurationServiceOperationAllowed(audit_token)) {
	status = ipconfig_status_permission_denied_e;
	goto done;
    }
    ipconfig_method_info_init(&info);
    status = method_info_from_xml_data(xml_data, xml_data_len, &info);
    if (status != ipconfig_status_success_e) {
	goto done;
    }
    ServiceIDClear(service_id);
    status = set_service(InterfaceNameNulTerminate(name), &info, service_id);
    ipconfig_method_info_free(&info);

 done:
    if (xml_data != NULL) {
	(void)vm_deallocate(mach_task_self(), (vm_address_t)xml_data,
			    xml_data_len);
    }
    *ret_status = status;
    return (KERN_SUCCESS);
}

PRIVATE_EXTERN kern_return_t 
_ipconfig_remove_service_on_interface(mach_port_t server,
				      InterfaceName name,
				      ServiceID service_id,
				      ipconfig_status_t * ret_status,
				      audit_token_t audit_token)
{
    if (!S_IPConfigurationServiceOperationAllowed(audit_token)) {
	*ret_status = ipconfig_status_permission_denied_e;
    }
    else {
	char *	ifname;

	if (name[0] == '\0') {
	    ifname = NULL;
	}
	else {
	    ifname = InterfaceNameNulTerminate(name);
	}
	ServiceIDNulTerminate(service_id);
	*ret_status = remove_service_with_id(ifname, service_id);
    }
    return (KERN_SUCCESS);
}

PRIVATE_EXTERN kern_return_t 
_ipconfig_find_service(mach_port_t server,
		       InterfaceName name,
		       boolean_t exact,
		       xmlData_t xml_data,
		       mach_msg_type_number_t xml_data_len,
		       ServiceID service_id,
		       ipconfig_status_t * ret_status)
{
    ipconfig_method_info	info;
    ipconfig_status_t		status;

    ServiceIDClear(service_id);
    ipconfig_method_info_init(&info);
    status = method_info_from_xml_data(xml_data, xml_data_len, &info);
    if (status != ipconfig_status_success_e) {
	goto done;
    }
    status = find_service(InterfaceNameNulTerminate(name),
			  exact, &info, service_id);
    ipconfig_method_info_free(&info);

 done:
    if (xml_data != NULL) {
	(void)vm_deallocate(mach_task_self(), (vm_address_t)xml_data,
			    xml_data_len);
    }
    *ret_status = status;
    return (KERN_SUCCESS);
}

PRIVATE_EXTERN kern_return_t 
_ipconfig_remove_service(mach_port_t server,
			 InterfaceName name,
			 xmlData_t xml_data,
			 mach_msg_type_number_t xml_data_len,
			 ipconfig_status_t * ret_status,
			 audit_token_t audit_token)
{
    ipconfig_method_info	info;
    ipconfig_status_t		status;

    if (!S_IPConfigurationServiceOperationAllowed(audit_token)) {
	status = ipconfig_status_permission_denied_e;
	goto done;
    }
    ipconfig_method_info_init(&info);
    status = method_info_from_xml_data(xml_data, xml_data_len, &info);
    if (status != ipconfig_status_success_e) {
	goto done;
    }
    status = remove_service(InterfaceNameNulTerminate(name), &info);
    ipconfig_method_info_free(&info);

 done:
    if (xml_data != NULL) {
	(void)vm_deallocate(mach_task_self(), (vm_address_t)xml_data,
			    xml_data_len);
    }
    *ret_status = status;
    return (KERN_SUCCESS);
}

PRIVATE_EXTERN kern_return_t 
_ipconfig_refresh_service(mach_port_t server,
			  InterfaceName name,
			  ServiceID service_id,
			  ipconfig_status_t * ret_status,
			  audit_token_t audit_token)
{
    if (!S_IPConfigurationServiceOperationAllowed(audit_token)) {
	*ret_status = ipconfig_status_permission_denied_e;
    }
    else {
	ServiceIDNulTerminate(service_id);
	*ret_status = refresh_service(InterfaceNameNulTerminate(name),
				      service_id);
    }
    return (KERN_SUCCESS);
}

PRIVATE_EXTERN kern_return_t
_ipconfig_is_service_valid(mach_port_t server,
			   InterfaceName name,
			   ServiceID service_id,
			   ipconfig_status_t * ret_status)
{
    InterfaceNameNulTerminate(name);
    ServiceIDNulTerminate(service_id);
    *ret_status = is_service_valid(name, service_id);
    return (KERN_SUCCESS);
}


PRIVATE_EXTERN kern_return_t
_ipconfig_forget_network(mach_port_t server,
			 InterfaceName name,
			 xmlData_t xml_data,
			 mach_msg_type_number_t xml_data_len,
			 ipconfig_status_t * ret_status,
			 audit_token_t audit_token)
{
    CFDictionaryRef		dict = NULL;
    ipconfig_status_t		status;
    CFStringRef			ssid = NULL;

    S_process_audit_token(audit_token);
    if (S_uid != 0) {
	status = ipconfig_status_permission_denied_e;
	goto done;
    }
    if (xml_data != NULL) {
	dict = my_CFPropertyListCreateWithBytePtrAndLength(xml_data,
							   xml_data_len);
	if (isA_CFDictionary(dict) != NULL) {
	    ssid = CFDictionaryGetValue(dict,
					kIPConfigurationForgetNetworkSSID);
	    ssid = isA_CFString(ssid);
	}
    }
    status = forget_network(InterfaceNameNulTerminate(name), ssid);
    my_CFRelease(&dict);

 done:
    if (xml_data != NULL) {
	(void)vm_deallocate(mach_task_self(), (vm_address_t)xml_data,
			    xml_data_len);
    }
    *ret_status = status;
    return (KERN_SUCCESS);
}

PRIVATE_EXTERN kern_return_t
_ipconfig_get_ra(mach_port_t p, InterfaceName name,
		 xmlDataOut_t * ra_data,
		 mach_msg_type_number_t * ra_data_cnt,
		 ipconfig_status_t * status)
{
    *status = get_if_ra(InterfaceNameNulTerminate(name), ra_data, ra_data_cnt);
    return (KERN_SUCCESS);
}

PRIVATE_EXTERN kern_return_t
_ipconfig_get_summary(mach_port_t server,
                      InterfaceName name,
                      xmlData_t * xml_data,
                      mach_msg_type_number_t * xml_data_len,
                      ipconfig_status_t * ret_status)
{
    ipconfig_status_t	status;
    CFDictionaryRef	summary = NULL;

    *xml_data = NULL;
    *xml_data_len = 0;
    status = copy_if_summary(InterfaceNameNulTerminate(name), &summary);
    if (summary != NULL) {
        *xml_data = (xmlDataOut_t)
        my_CFPropertyListCreateVMData(summary, xml_data_len);
        if (*xml_data == NULL) {
            my_log(LOG_NOTICE, "failed to serialize data");
            status = ipconfig_status_allocation_failed_e;
        }
    }
    my_CFRelease(&summary);
    *ret_status = status;
    return (KERN_SUCCESS);
}

PRIVATE_EXTERN kern_return_t
_ipconfig_get_interface_list(mach_port_t server,
			     xmlData_t * xml_data,
			     mach_msg_type_number_t * xml_data_len,
			     ipconfig_status_t * ret_status)
{
    CFArrayRef		interface_list = NULL;
    ipconfig_status_t	status;

    *xml_data = NULL;
    *xml_data_len = 0;
    status = copy_interface_list(&interface_list);
    if (interface_list != NULL) {
        *xml_data = (xmlDataOut_t)
        my_CFPropertyListCreateVMData(interface_list, xml_data_len);
        if (*xml_data == NULL) {
            my_log(LOG_NOTICE, "failed to serialize data");
            status = ipconfig_status_allocation_failed_e;
        }
    }
    my_CFRelease(&interface_list);
    *ret_status = status;
    return (KERN_SUCCESS);
}

PRIVATE_EXTERN kern_return_t
_ipconfig_get_dhcp_duid(mach_port_t p,
			dataOut_t * dhcp_duid,
			mach_msg_type_number_t * dhcp_duid_cnt,
			ipconfig_status_t * status)
{
    *status = get_dhcp_duid(dhcp_duid, dhcp_duid_cnt);
    return (KERN_SUCCESS);
}

PRIVATE_EXTERN kern_return_t
_ipconfig_get_dhcp_ia_id(mach_port_t p,
			 InterfaceName name,
			 DHCPIAID * ia_id,
			 ipconfig_status_t * status)
{
    *status = get_dhcp_ia_id(InterfaceNameNulTerminate(name), ia_id);
    return (KERN_SUCCESS);
}

typedef union {
    union __RequestUnion___ipconfig_subsystem req;
    union __ReplyUnion___ipconfig_subsystem rep;
} IPConfigurationUnion;

PRIVATE_EXTERN void
server_init()
{
    dispatch_block_t	handler;
    mach_port_t		server_port;
    dispatch_source_t	source;
    kern_return_t 	status;

    status = bootstrap_check_in(bootstrap_port, IPCONFIG_SERVER, 
				&server_port);
    if (status != BOOTSTRAP_SUCCESS) {
	my_log(LOG_NOTICE,
	       "IPConfiguration: bootstrap_check_in failed, %s",
	       mach_error_string(status));
	return;
    }
    source = dispatch_source_create(DISPATCH_SOURCE_TYPE_MACH_RECV,
				    server_port,
				    0,
				    IPConfigurationAgentQueue());
    handler = ^{
	mach_msg_return_t ret;

	ret = dispatch_mig_server(source,
				  sizeof(IPConfigurationUnion),
				  ipconfig_server);
	switch (ret) {
	case MACH_MSG_SUCCESS:
	    break;
	default:
	    my_log(LOG_NOTICE, "%s: failed %d", __func__, ret);
	    break;
	}
    };
    dispatch_source_set_event_handler(source, handler);
    dispatch_activate(source);
    return;
}
