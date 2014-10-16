/*
 * Copyright (c) 2010-2014 Apple Inc. All rights reserved.
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
 * eapolcfg_auth.c
 * - launch-on-demand daemon running as root to perform privileged operations
 *   required by certain EAPOLClientConfiguration.h APIs
 * - uses AuthorizationRef for validating that caller has permission to
 *   perform the operation
 */

/*
 * Modification History
 *
 * February 22, 2010		Dieter Siegmund (dieter@apple.com)
 * - initial revision
 */
#include <stdlib.h>
#include <unistd.h>
#include <bsm/libbsm.h>
#include <sys/types.h>
#include <sysexits.h>
#include <servers/bootstrap.h>

#include <CoreFoundation/CFRunLoop.h>
#include <CoreFoundation/CFRuntime.h>
#include <SystemConfiguration/SystemConfiguration.h>
#include <SystemConfiguration/SCPrivate.h>
#include <SystemConfiguration/SCValidation.h>
#include "EAPOLClientConfiguration.h"
#include "EAPOLClientConfigurationPrivate.h"
#include "EAPCertificateUtil.h"
#include "symbol_scope.h"
#include "eapolcfg_authServer.h"
#include "eapolcfg_auth_types.h"
#include "myCFUtil.h"

STATIC Boolean	S_handled_request;

INLINE void
my_vm_deallocate(vm_address_t data, int data_length)
{
    if (data != 0) {
	(void)vm_deallocate(mach_task_self(), data, data_length);
    }
    return;
}

STATIC Boolean
authorization_is_valid(const void * auth_data, int auth_data_length)
{
    AuthorizationExternalForm *	auth_ext_p;
    AuthorizationRef 		authorization;
    AuthorizationFlags		flags;
    AuthorizationItem		item;
    AuthorizationRights		rights;
    OSStatus			status;
    
    if (auth_data == NULL
	|| auth_data_length != sizeof(auth_ext_p->bytes)) {
	syslog(LOG_ERR, "eapolcfg_auth: authorization NULL/invalid size");
	return (FALSE);
    }
    auth_ext_p = (AuthorizationExternalForm *)auth_data;
    status = AuthorizationCreateFromExternalForm(auth_ext_p, &authorization);
    if (status != errAuthorizationSuccess) {
	syslog(LOG_ERR, "eapolcfg_auth: authorization is invalid (%d)",
	       (int)status);
	return (FALSE);
    }
    rights.count = 1;
    rights.items = &item;
    item.name = "system.preferences";
    item.value = NULL;
    item.valueLength = 0;
    item.flags = 0;
    flags = kAuthorizationFlagDefaults;
    flags |= kAuthorizationFlagExtendRights;
    flags |= kAuthorizationFlagInteractionAllowed;
    status = AuthorizationCopyRights(authorization,
				     &rights,
				     kAuthorizationEmptyEnvironment,
				     flags,
				     NULL);
    AuthorizationFree(authorization, kAuthorizationFlagDefaults);
    return (status == errAuthorizationSuccess);
}

STATIC int
init_itemID_and_cfg(xmlData_t itemID_data,
		    mach_msg_type_number_t itemID_data_length,
		    EAPOLClientItemIDRef * itemID_p,
		    EAPOLClientConfigurationRef * cfg_p)
{
    EAPOLClientConfigurationRef	cfg = NULL;
    EAPOLClientItemIDRef	itemID = NULL;
    CFDictionaryRef		itemID_dict = NULL;
    int				ret = 0;

    itemID_dict 
	= my_CFPropertyListCreateWithBytePtrAndLength(itemID_data,
						      itemID_data_length);
    if (isA_CFDictionary(itemID_dict) == NULL) {
	ret = EINVAL;
	goto done;
    }
    cfg = EAPOLClientConfigurationCreate(NULL);
    if (cfg == NULL) {
	ret = ENOMEM;
	goto done;
    }
    itemID = EAPOLClientItemIDCreateWithDictionary(cfg, itemID_dict);
    if (itemID == NULL) {
	ret = EINVAL;
	goto done;
    }
 done:
    my_CFRelease(&itemID_dict);
    if (ret == 0) {
	*cfg_p = cfg;
	*itemID_p = itemID;
    }
    else {
	my_CFRelease(&itemID);
	my_CFRelease(&cfg);
    }
    return (ret);
}

/**
 ** MiG server routines
 **/
PRIVATE_EXTERN kern_return_t
eapolclientitemid_set_identity(mach_port_t server,
			       OOBData_t auth_data,
			       mach_msg_type_number_t auth_data_length,
			       xmlData_t itemID_data,
			       mach_msg_type_number_t itemID_data_length,
			       OOBData_t id_handle,
			       mach_msg_type_number_t id_handle_length,
			       int * result)
{
    EAPOLClientConfigurationRef	cfg = NULL;
    SecIdentityRef		identity = NULL;
    EAPOLClientItemIDRef	itemID = NULL;
    int				ret = 0;

    if (authorization_is_valid(auth_data, auth_data_length) == FALSE) {
	ret = EPERM;
	goto done;
    }
    ret = init_itemID_and_cfg(itemID_data, itemID_data_length, &itemID, &cfg);
    if (ret != 0) {
	goto done;
    }
    if (id_handle != NULL) {
	CFDataRef		data;
	OSStatus		status;

	data = CFDataCreateWithBytesNoCopy(NULL,
					   (const UInt8 *)id_handle,
					   id_handle_length,
					   kCFAllocatorNull);
	if (data == NULL) {
	    ret = EINVAL;
	    goto done;
	}
	status = EAPSecIdentityHandleCreateSecIdentity(data, &identity); 
	CFRelease(data);
	if (status != noErr) {
	    ret = ENOENT;
	    goto done;
	}
    }
    if (EAPOLClientItemIDSetIdentity(itemID,
				     kEAPOLClientDomainSystem,
				     identity) == FALSE) {
	ret = ENXIO;
    }

 done:
    my_vm_deallocate((vm_address_t)auth_data, auth_data_length);
    my_vm_deallocate((vm_address_t)itemID_data, itemID_data_length);
    my_vm_deallocate((vm_address_t)id_handle, id_handle_length);
    my_CFRelease(&cfg);
    my_CFRelease(&itemID);
    my_CFRelease(&identity);
    *result = ret;
    return (KERN_SUCCESS);
}

kern_return_t 
eapolclientitemid_set_password(mach_port_t server,
			       OOBData_t auth_data,
			       mach_msg_type_number_t auth_data_length,
			       xmlData_t itemID_data,
			       mach_msg_type_number_t itemID_data_length,
			       uint32_t flags,
			       OOBData_t name,
			       mach_msg_type_number_t name_length,
			       OOBData_t password,
			       mach_msg_type_number_t password_length,
			       int * result)
{
    EAPOLClientConfigurationRef	cfg = NULL;
    EAPOLClientItemIDRef	itemID = NULL;
    CFDataRef			name_data = NULL;
    CFDataRef			password_data = NULL;
    int				ret = 0;

    if (authorization_is_valid(auth_data, auth_data_length) == FALSE) {
	ret = EPERM;
	goto done;
    }
    ret = init_itemID_and_cfg(itemID_data, itemID_data_length, &itemID, &cfg);
    if (ret != 0) {
	goto done;
    }
    if ((flags & keapolcfg_auth_set_name) != 0) {
	name_data = CFDataCreate(NULL, (const UInt8 *)name,
				 name_length);
    }
    else {
	syslog(LOG_NOTICE, "not setting name");
    }
    if ((flags & keapolcfg_auth_set_password) != 0) {
	password_data = CFDataCreate(NULL, (const UInt8 *)password,
				     password_length);
    }
    else {
	syslog(LOG_NOTICE, "not setting password");
    }
    if (EAPOLClientItemIDSetPasswordItem(itemID,
					 kEAPOLClientDomainSystem,
					 name_data, password_data) == FALSE) {
	ret = ENXIO;
    }

 done:
    my_vm_deallocate((vm_address_t)auth_data, auth_data_length);
    my_vm_deallocate((vm_address_t)itemID_data, itemID_data_length);
    my_vm_deallocate((vm_address_t)name, name_length);
    my_vm_deallocate((vm_address_t)password, password_length);
    my_CFRelease(&name_data);
    my_CFRelease(&password_data);
    my_CFRelease(&cfg);
    my_CFRelease(&itemID);
    *result = ret;
    return (KERN_SUCCESS);
}

PRIVATE_EXTERN kern_return_t
eapolclientitemid_remove_password(mach_port_t server,
				  OOBData_t auth_data,
				  mach_msg_type_number_t auth_data_length,
				  xmlData_t itemID_data,
				  mach_msg_type_number_t itemID_data_length,
				  int * result)
{
    EAPOLClientConfigurationRef	cfg = NULL;
    SecIdentityRef		identity = NULL;
    EAPOLClientItemIDRef	itemID = NULL;
    CFDataRef			name_data = NULL;
    CFDataRef			password_data = NULL;
    int				ret = 0;

    if (authorization_is_valid(auth_data, auth_data_length) == FALSE) {
	ret = EPERM;
	goto done;
    }
    ret = init_itemID_and_cfg(itemID_data, itemID_data_length, &itemID, &cfg);
    if (ret != 0) {
	goto done;
    }
    if (EAPOLClientItemIDRemovePasswordItem(itemID, kEAPOLClientDomainSystem)
	== FALSE) {
	ret = ENXIO;
    }

 done:
    my_vm_deallocate((vm_address_t)auth_data, auth_data_length);
    my_vm_deallocate((vm_address_t)itemID_data, itemID_data_length);
    my_CFRelease(&cfg);
    my_CFRelease(&itemID);
    *result = ret;
    return (KERN_SUCCESS);
}

PRIVATE_EXTERN kern_return_t
eapolclientitemid_check_password(mach_port_t server,
				 OOBData_t auth_data,
				 mach_msg_type_number_t auth_data_length,
				 xmlData_t itemID_data,
				 mach_msg_type_number_t itemID_data_length,
				 OOBDataOut_t * name,
				 mach_msg_type_number_t * name_length,
				 boolean_t * password_set,
				 int * result)
{
    EAPOLClientConfigurationRef	cfg = NULL;
    SecIdentityRef		identity = NULL;
    EAPOLClientItemIDRef	itemID = NULL;
    CFDataRef			name_data = NULL;
    vm_address_t		name_vm_data = 0;
    mach_msg_type_number_t	name_vm_length = 0;
    CFDataRef			password_data = NULL;
    vm_address_t		password_vm_data = 0;
    mach_msg_type_number_t	password_vm_length = 0;
    int				ret = 0;

    *password_set = FALSE;
    if (authorization_is_valid(auth_data, auth_data_length) == FALSE) {
	ret = EPERM;
	goto done;
    }
    ret = init_itemID_and_cfg(itemID_data, itemID_data_length, &itemID, &cfg);
    if (ret != 0) {
	goto done;
    }
    if (EAPOLClientItemIDCopyPasswordItem(itemID, kEAPOLClientDomainSystem,
					  &name_data, &password_data) 
	== FALSE) {
	ret = ENOENT;
    }
    else {
	kern_return_t	kret;

	if (name_data != NULL) {
	    name_vm_length = CFDataGetLength(name_data);
	    kret = vm_allocate(mach_task_self(), &name_vm_data, 
			       name_vm_length, TRUE);
	    if (kret != KERN_SUCCESS) {
		ret = ENOMEM;
		goto done;
	    }
	    bcopy((char *)CFDataGetBytePtr(name_data),
		  (char *)(name_vm_data), name_vm_length);
	    CFRelease(name_data);
	}
	if (password_data != NULL) {
	    *password_set = TRUE;
	    CFRelease(password_data);
	}
    }

 done:
    if (ret != 0) {
	if (name_vm_data != 0) {
	    (void)vm_deallocate(mach_task_self(), name_vm_data,
				name_vm_length);
	    name_vm_data = 0;
	    name_vm_length = 0;
	}
    }
    *name = (char *)name_vm_data;
    *name_length = name_vm_length;
    my_vm_deallocate((vm_address_t)auth_data, auth_data_length);
    my_vm_deallocate((vm_address_t)itemID_data, itemID_data_length);
    my_CFRelease(&cfg);
    my_CFRelease(&itemID);
    *result = ret;
    return (KERN_SUCCESS);
}


/**
 ** server message handling
 **/

STATIC boolean_t
process_notification(mach_msg_header_t * request)
{
    mach_no_senders_notification_t * notify;

    notify = (mach_no_senders_notification_t *)request;
    if (notify->not_header.msgh_id > MACH_NOTIFY_LAST
	|| notify->not_header.msgh_id < MACH_NOTIFY_FIRST) {
	return FALSE;	/* if this is not a notification message */
    }
    return (TRUE);
}

STATIC void
server_handle_request(CFMachPortRef port, void * msg, CFIndex size, void * info)
{
    mach_msg_return_t 	r;
    mach_msg_header_t *	request = (mach_msg_header_t *)msg;
    mach_msg_header_t *	reply;
    char		reply_s[eapolcfg_auth_subsystem.maxsize];

    if (process_notification(request) == FALSE) {
	reply = (mach_msg_header_t *)reply_s;
	if (eapolcfg_auth_server(request, reply) == FALSE) {
	    syslog(LOG_NOTICE,
		   "eapolcfg_auth: unknown message ID (%d)",
		   request->msgh_id);
	    mach_msg_destroy(request);
	}
	else {
	    int		options;

	    S_handled_request = TRUE;

	    options = MACH_SEND_MSG;
	    if (MACH_MSGH_BITS_REMOTE(reply->msgh_bits)
                != MACH_MSG_TYPE_MOVE_SEND_ONCE) {
		options |= MACH_SEND_TIMEOUT;
	    }
	    r = mach_msg(reply,
			 options,
			 reply->msgh_size,
			 0,
			 MACH_PORT_NULL,
			 MACH_MSG_TIMEOUT_NONE,
			 MACH_PORT_NULL);
	    if (r != MACH_MSG_SUCCESS) {
		syslog(LOG_NOTICE, "eapolcfg_auth: mach_msg(send): %s", 
		       mach_error_string(r));
		mach_msg_destroy(reply);
	    }
	}
    }
    return;
}

static void
start_service(mach_port_t service_port)
{
    CFMachPortRef	mp;
    CFRunLoopSourceRef	rls;
    
    mp = CFMachPortCreateWithPort(NULL, service_port, server_handle_request,
				  NULL, NULL);
    rls = CFMachPortCreateRunLoopSource(NULL, mp, 0);
    CFRunLoopAddSource(CFRunLoopGetCurrent(), rls, kCFRunLoopDefaultMode);
    CFRelease(mp);
    CFRelease(rls);
    return;
}

int
main(int argc, char **argv)
{
    kern_return_t	kr;
    mach_port_t		service_port = MACH_PORT_NULL;

    openlog("eapolcfg_auth", LOG_CONS | LOG_PID, LOG_DAEMON);
    if (geteuid() != 0) {
	syslog(LOG_ERR, "not running as root - exiting");
	exit(EX_CONFIG);
    }
    kr = bootstrap_check_in(bootstrap_port, EAPOLCFG_AUTH_SERVER,
			    &service_port);
    if (kr != BOOTSTRAP_SUCCESS) {
	syslog(LOG_ERR, "bootstrap_check_in() failed: %s",
	       bootstrap_strerror(kr));
	exit(EX_UNAVAILABLE);
    }
    start_service(service_port);
    while (1) {
	SInt32	rlStatus;

	rlStatus = CFRunLoopRunInMode(kCFRunLoopDefaultMode, 15.0, TRUE);
	if (rlStatus == kCFRunLoopRunTimedOut) {
	    if (S_handled_request == FALSE) {
		/* we didn't handle a request in the last time interval */
		break;
	    }
	    S_handled_request = FALSE;
	}
    }
    exit(EX_OK);
    return (0);
}
