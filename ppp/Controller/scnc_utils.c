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


/* -----------------------------------------------------------------------------
includes
----------------------------------------------------------------------------- */
#include <mach/mach.h>
#include <string.h>
#include <stdio.h>
#include <termios.h>
#include <unistd.h>
#include <sys/errno.h>
#include <sys/signal.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <net/if.h>
#include <CoreFoundation/CoreFoundation.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <SystemConfiguration/SystemConfiguration.h>
#include <SystemConfiguration/SCPrivate.h>      // for SCLog()
#include <SystemConfiguration/SCValidation.h>
#include <sys/ioctl.h>
#include <net/if_dl.h>
#include <net/if_utun.h>
#include <notify.h>
#include <sys/kern_control.h>
#include <sys/sys_domain.h>
#include <servers/bootstrap.h>
#include <mach/task_special_ports.h>
#include "pppcontroller_types.h"
#include "pppcontroller.h"
#include <bsm/libbsm.h>
#include <sys/kern_event.h>
#include <netinet/in_var.h>
#include <netinet6/in6_var.h>
#include <netinet6/nd6.h>
#include <network_information.h>

#include "ppp_msg.h"
#include "../Family/if_ppplink.h"
#include "scnc_main.h"
#include "scnc_client.h"
#include "scnc_utils.h"
#include "ppp_option.h"
#include "PPPControllerPriv.h"
#include "../Family/ppp_domain.h"
#include "../Helpers/pppd/pppd.h"
#include "../Drivers/PPTP/PPTP-plugin/pptp.h"
#include "../Drivers/L2TP/L2TP-plugin/l2tp.h"
#include "../Drivers/PPPoE/PPPoE-extension/PPPoE.h"
#include "ne_sm_bridge_private.h"

/* -----------------------------------------------------------------------------
definitions
----------------------------------------------------------------------------- */
#undef CONSTSTR
#define CONSTSTR(str) (const char *)str

/* -----------------------------------------------------------------------------
Forward Declarations
----------------------------------------------------------------------------- */


/* -----------------------------------------------------------------------------
globals
----------------------------------------------------------------------------- */
const char *scnc_ctrl_stopped = "Controller Stopped";
const char *scnc_sys_sleep = "System will sleep";
const char *scnc_usr_logout = "User Logged out";
const char *scnc_usr_switch = "User Switched";
const char *scnc_sock_disco = "Socket disconnect";
const char *scnc_plugin_chg = "Plugin Change";
const char *scnc_app_rem = "App removed";
const char *scnc_usr_req = "User Requested";
const char *scnc_term_all = "Terminated All";
const char *scnc_serv_disp = "Service Disposed";
const char *ppp_fatal = "Fatal Error";
const char *ppp_option = "Option Error";
const char *ppp_not_root = "Not Root";
const char *ppp_no_kern = "No Kernel Support";
const char *ppp_user_req = "User requested";
const char *ppp_lock_fail = "Lock Failed";
const char *ppp_open_fail = "Open Failed";
const char *ppp_conn_fail = "Connect Failed";
const char *ppp_pty_fail = "Pty command Failed";
const char *ppp_nego_fail = "Negotiation Failed";
const char *ppp_peer_auth_fail = "Peer Authentication Failed";
const char *ppp_idle_tmo = "Idle Timeout";
const char *ppp_sess_tmo = "Session Timeout";
const char *ppp_callback = "Callback";
const char *ppp_peer_dead = "Peer Dead";
const char *ppp_disco_by_dev = "Disconnect by Device";
const char *ppp_loopback = "Loopback Error";
const char *ppp_init_fail = "Init Failed";
const char *ppp_auth_fail = "Authentication to Peer Failed";
const char *ppp_term_fail = "Terminal Failed";
const char *ppp_dev_err = "Device Error";
const char *ppp_peer_unauth = "Peer Not Authorized";
const char *ppp_cnid_auth_fail = "CNID Authentication Failed";
const char *ppp_peer_unreach = "Peer Unreachable";
const char *ppp_dev_no_srvr = "No Server";
const char *ppp_dev_no_ans = "No Answer";
const char *ppp_dev_prot_err = "Protocol Error";
const char *ppp_dev_net_chg = "Network Changed";
const char *ppp_dev_psk = "Shared Secret";
const char *ppp_dev_cert = "No Certificate";
const char *ppp_dev_no_car = "No Carrier";
const char *ppp_dev_no_num = "No Number";
const char *ppp_dev_bad_script = "Bad Script";
const char *ppp_dev_busy = "Busy";
const char *ppp_dev_no_dial = "No Dial Tone";
const char *ppp_dev_modem_err = "Modem Error";
const char *ppp_dev_hang = "Hang-up";
const char *ppp_dev_no_srvc = "No Service";
const char *ppp_dev_no_ac = "No AC";
const char *ppp_dev_no_ac_srvc = "No AC Service";
const char *ppp_dev_conn_refuse = "Connection Refused";
const char *ipsec_gen_err = "Generic Error";
const char *ipsec_no_srvr_addr = "No Server Address";
const char *ipsec_no_psk = "No Shared Secret";
const char *ipsec_no_cert = "No Certificate";
const char *ipsec_dns_err = "Resolve Address Error";
const char *ipsec_no_local = "No Local Network";
const char *ipsec_cfg_err = "Configuration Error";
const char *ipsec_ctrl_err = "Racoon Control Error";
const char *ipsec_conn_err = "Connection Error";
const char *ipsec_nego_err = "Negotiation Error";
const char *ipsec_psk_err = "Shared Secret Error";
const char *ipsec_srvr_cert_err = "Server Certificate Error";
const char *ipsec_cli_cert_err = "Client Certificate Error";
const char *ipsec_xauth_err = "Xauth Error";
const char *ipsec_net_chg = "Network Change";
const char *ipsec_peer_disco = "Peer Disconnect";
const char *ipsec_peer_dead = "Peer Dead";
const char *ipsec_edge_err = "Edge Activation Error";
const char *ipsec_idle_tmo = "Idle Timeout";
const char *ipsec_cli_cert_pre = "Client Certificate premature";
const char *ipsec_cli_cert_exp = "Client Certificate expired";
const char *ipsec_srvr_cert_pre = "Server Certificate premature";
const char *ipsec_srvr_cert_exp = "Server Certificate expired";
const char *ipsec_srvr_cert_id = "Server Certificate identity incorrect";
const char *vpn_gen_err = "Generic Error";
const char *vpn_no_srvr_addr = "No Server Address";
const char *vpn_no_cert = "No Certificate";
const char *vpn_dns_err = "Resolve Address Error";
const char *vpn_no_local = "No Local Network";
const char *vpn_cfg_err = "Configuration Error";
const char *vpn_ctrl_err = "Control Error";
const char *vpn_conn_err = "Connection Error";
const char *vpn_net_chg = "Network Change";
const char *vpn_peer_disco = "Peer Disconnect";
const char *vpn_peer_dead = "Peer Dead";
const char *vpn_peer_unresp = "Peer not responding";
const char *vpn_nego_err = "Negotiation Error";
const char *vpn_xauth_err = "Xauth Error";
const char *vpn_edge_err = "Edge Activation Error";
const char *vpn_idle_tmo = "Idle Timeout";
const char *vpn_addr_invalid = "Address invalid";
const char *vpn_ap_req = "AP required";
const char *vpn_cli_cert_pre = "Client Certificate premature";
const char *vpn_cli_cert_exp = "Client Certificate expired";
const char *vpn_srvr_cert_pre = "Server Certificate premature";
const char *vpn_srvr_cert_exp = "Server Certificate expired";
const char *vpn_srvr_cert_id = "Server Certificate identity incorrect";
const char *vpn_plugin_upd = "Plugin update required";
const char *vpn_plugin_dis = "Plugin disabled";



/* -----------------------------------------------------------------------------
 Given a string 'key' and a string prefix 'prefix',
 return the next component in the slash '/' separated
 key.  If no slash follows the prefix, return NULL.

 Examples:
 1. key = "a/b/c" prefix = "a/"    returns "b"
 2. key = "a/b/c" prefix = "a/b/"  returns NULL
----------------------------------------------------------------------------- */
CFStringRef parse_component(CFStringRef key, CFStringRef prefix)
{
    CFMutableStringRef	comp;
    CFRange		range;

    if (!CFStringHasPrefix(key, prefix))
    	return NULL;

    comp = CFStringCreateMutableCopy(NULL, 0, key);
    CFStringDelete(comp, CFRangeMake(0, CFStringGetLength(prefix)));
    range = CFStringFind(comp, CFSTR("/"), 0);
    if (range.location == kCFNotFound) {
        CFRelease(comp);
        return NULL;
    }
    range.length = CFStringGetLength(comp) - range.location;
    CFStringDelete(comp, range);
    return comp;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
CFDictionaryRef copyService(SCDynamicStoreRef store, CFStringRef domain, CFStringRef serviceID)
{
    CFTypeRef		data = NULL;
    CFMutableDictionaryRef	service = NULL;
    CFStringRef		key = NULL;
    int			i;
    CFStringRef         copy[] = {
        kSCEntNetPPP,
        kSCEntNetModem,
        kSCEntNetInterface,
    	kSCEntNetIPv4,
    	kSCEntNetIPv6,
#if !TARGET_OS_EMBEDDED
    	kSCEntNetSMB,
#endif
        kSCEntNetDNS,
        kSCEntNetL2TP,
        kSCEntNetPPTP,
        kSCEntNetIPSec,
        NULL,
    };

    key = SCDynamicStoreKeyCreate(0, CFSTR("%@/%@/%@/%@"), domain, kSCCompNetwork, kSCCompService, serviceID);
    if (key == 0)
        goto fail;
        
    data = SCDynamicStoreCopyValue(store, key);
    if (data == 0) {
		data = CFDictionaryCreate(NULL, 0, 0, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
		if (data == 0)
			goto fail;
	}
        
    CFRelease(key);
	key = NULL;
        
    service = CFDictionaryCreateMutableCopy(NULL, 0, data);
    if (service == 0)
        goto fail;
        
    CFRelease(data);

    for (i = 0; copy[i]; i++) {   
        data = copyEntity(store, domain, serviceID, copy[i]);
        if (data) {
        
            CFDictionaryAddValue(service, copy[i], data);
            CFRelease(data);
        }
    }

    return service;

fail:
    if (key) 
        CFRelease(key);
    if (data)
        CFRelease(data);
    if (service)
        CFRelease(service);
    return 0;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
CFDictionaryRef copyEntity(SCDynamicStoreRef store, CFStringRef domain, CFStringRef serviceID, CFStringRef entity)
{
    CFTypeRef		data = NULL;
    CFStringRef		key;

    if (serviceID)
        key = SCDynamicStoreKeyCreateNetworkServiceEntity(0, domain, serviceID, entity);
    else
        key = SCDynamicStoreKeyCreateNetworkGlobalEntity(0, domain, entity);

    if (key) {
        data = SCDynamicStoreCopyValue(store, key);
        CFRelease(key);
    }
    return data;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
int existEntity(SCDynamicStoreRef store, CFStringRef domain, CFStringRef serviceID, CFStringRef entity)
{
    CFTypeRef		data;

    data = copyEntity(store, domain, serviceID, entity);
    if (data) {
        CFRelease(data);
        return 1;
    }
    
    return 0;
}

/* -----------------------------------------------------------------------------
get a string from the dictionnary, in service/property
----------------------------------------------------------------------------- */
int getString(CFDictionaryRef service, CFStringRef property, u_char *str, u_int16_t maxlen)
{
    CFStringRef		string;
    CFDataRef		ref;
    const UInt8    *dataptr;
    int            len;

    str[0] = 0;
    ref  = CFDictionaryGetValue(service, property);
    if (ref) {
        if (CFGetTypeID(ref) == CFStringGetTypeID()) {
            CFStringGetCString((CFStringRef)ref, (char*)str, maxlen, kCFStringEncodingUTF8);
            return 1;
        }
        else if (CFGetTypeID(ref) == CFDataGetTypeID()) {
            CFStringEncoding    encoding;
            
            if ((len = CFDataGetLength(ref)) && (dataptr = CFDataGetBytePtr(ref))){
#if     __BIG_ENDIAN__
                encoding = (*(dataptr + 1) == 0x00) ? kCFStringEncodingUTF16LE : kCFStringEncodingUTF16BE;
#else   // __LITTLE_ENDIAN__
                encoding = (*dataptr == 0x00) ? kCFStringEncodingUTF16BE : kCFStringEncodingUTF16LE;
#endif
                string = CFStringCreateWithBytes(NULL, (const UInt8 *)dataptr, len, encoding, FALSE);
                if (string) {
                    CFStringGetCString((CFStringRef)string, (char*)str, maxlen, kCFStringEncodingUTF8);
                    CFRelease(string);
                    return 1;
                }
            }
        }
    }
    return 0;
}

/* -----------------------------------------------------------------------------
get a number from the dictionnary, in service/property
----------------------------------------------------------------------------- */
int getNumber(CFDictionaryRef dict, CFStringRef property, u_int32_t *outval)
{
    CFNumberRef		ref;

    ref  = CFDictionaryGetValue(dict, property);
    if (ref && (CFGetTypeID(ref) == CFNumberGetTypeID())) {
        CFNumberGetValue(ref, kCFNumberSInt32Type, outval);
        return 1;
    }
    return 0;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
int getNumberFromEntity(SCDynamicStoreRef store, CFStringRef domain, CFStringRef serviceID, 
        CFStringRef entity, CFStringRef property, u_int32_t *outval)
{
    CFTypeRef		data;
    int 		ok = 0;

    if ((data = copyEntity(store, domain, serviceID, entity))) {
        ok = getNumber(data, property, outval);
        CFRelease(data);
    }
    return ok;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
int getStringFromEntity(SCDynamicStoreRef store, CFStringRef domain, CFStringRef serviceID, 
        CFStringRef entity, CFStringRef property, u_char *str, u_int16_t maxlen)
{
    CFTypeRef		data;
    int 		ok = 0;

    data = copyEntity(store, domain, serviceID, entity);
    if (data) {
        ok = getString(data, property, str, maxlen);
        CFRelease(data);
    }
    return ok;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
CFStringRef copyCFStringFromEntity(SCDynamicStoreRef store, CFStringRef domain, CFStringRef serviceID, 
        CFStringRef entity, CFStringRef property)
{
    CFTypeRef		data;
    CFStringRef		string, ret = 0;

    data = copyEntity(store, domain, serviceID, entity);
    if (data) {
        string  = CFDictionaryGetValue(data, property);
        if (string && (CFGetTypeID(string) == CFStringGetTypeID())) {
            CFRetain(string);
            ret = string;
        }

        CFRelease(data);
    }
    return ret; 
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
u_int32_t CFStringAddrToLong(CFStringRef string)
{
    char 	str[100];
    u_int32_t	ret = 0;
    
    if (string) {
	str[0] = 0;
        CFStringGetCString(string, str, sizeof(str), kCFStringEncodingMacRoman);
        ret = ntohl(inet_addr(str));
    }
    return ret;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
int getAddressFromEntity(SCDynamicStoreRef store, CFStringRef domain, CFStringRef serviceID, 
        CFStringRef entity, CFStringRef property, u_int32_t *outval)
{
    CFTypeRef		data;
    int 		ok = 0;
    CFArrayRef		array;

    data = copyEntity(store, domain, serviceID, entity);
    if (data) {
        array = CFDictionaryGetValue(data, property);
        if (array && CFArrayGetCount(array)) {
            *outval = CFStringAddrToLong(CFArrayGetValueAtIndex(array, 0));
            ok = 1;
        }
        CFRelease(data);
    }
    return ok;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
Boolean my_CFEqual(CFTypeRef obj1, CFTypeRef obj2)
{
    if (obj1 == NULL && obj2 == NULL)
        return true;
    else if (obj1 == NULL || obj2 == NULL)
        return false;
    
    return CFEqual(obj1, obj2);
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
void my_CFRelease(void *t)
{
    void * * obj = (void * *)t;
    if (obj && *obj) {
	CFRelease(*obj);
	*obj = NULL;
    }
    return;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
void my_close(int fd)
{
    if (fd != -1)
        close(fd);
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
CFTypeRef my_CFRetain(CFTypeRef obj)
{
    if (obj)
        CFRetain(obj);
	return obj;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
Boolean isDictionary (CFTypeRef obj)
{
    return (obj && CFGetTypeID(obj) == CFDictionaryGetTypeID());
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
Boolean isArray (CFTypeRef obj)
{
    return (obj && CFGetTypeID(obj) == CFArrayGetTypeID());
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
Boolean isString (CFTypeRef obj)
{
    return (obj && CFGetTypeID(obj) == CFStringGetTypeID());
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
Boolean isNumber (CFTypeRef obj)
{
    return (obj && CFGetTypeID(obj) == CFNumberGetTypeID());
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
Boolean isData (CFTypeRef obj)
{
    return (obj && CFGetTypeID(obj) == CFDataGetTypeID());
}


/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
void AddNumber(CFMutableDictionaryRef dict, CFStringRef property, u_int32_t number) 
{
   CFNumberRef num;
    num = CFNumberCreate(NULL, kCFNumberSInt32Type, &number);
    if (num) {
        CFDictionaryAddValue(dict, property, num);
        CFRelease(num); 
    } 
}        

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
void AddNumber64(CFMutableDictionaryRef dict, CFStringRef property, u_int64_t number) 
{
   CFNumberRef num;
    num = CFNumberCreate(NULL, kCFNumberSInt64Type, &number);
    if (num) {
        CFDictionaryAddValue(dict, property, num);
        CFRelease(num); 
    } 
}        

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
void AddString(CFMutableDictionaryRef dict, CFStringRef property, char *string) 
{
    CFStringRef str;
    str = CFStringCreateWithCString(NULL, string, kCFStringEncodingUTF8);
    if (str) { 
        CFDictionaryAddValue(dict, property, str);
        CFRelease(str); 
    }
}        

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
void AddNumberFromState(SCDynamicStoreRef store, CFStringRef serviceID, CFStringRef entity, CFStringRef property, CFMutableDictionaryRef dict) 
{
    u_int32_t 	lval;
    
    if (getNumberFromEntity(store, kSCDynamicStoreDomainState, serviceID, entity, property, &lval)) 
        AddNumber(dict, property, lval);
}        

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
void AddStringFromState(SCDynamicStoreRef store, CFStringRef serviceID, CFStringRef entity, CFStringRef property, CFMutableDictionaryRef dict) 
{
    CFStringRef	string;
    
    if ((string = copyCFStringFromEntity(store, kSCDynamicStoreDomainState, serviceID, entity, property))) {
        CFDictionaryAddValue(dict, property, string);
        CFRelease(string);
    }
}        

/* -------------------------------------------------------------------------------------------
------------------------------------------------------------------------------------------- */
CFDataRef Serialize(CFPropertyListRef obj, void **data, u_int32_t *dataLen)
{
    CFDataRef           	xml;
    
    xml = CFPropertyListCreateXMLData(NULL, obj);
    if (xml) {
        *data = (void*)CFDataGetBytePtr(xml);
        *dataLen = CFDataGetLength(xml);
    }
    return xml;
}

/* -------------------------------------------------------------------------------------------
------------------------------------------------------------------------------------------- */
CFPropertyListRef Unserialize(void *data, u_int32_t dataLen)
{
    CFDataRef           	xml;
    CFPropertyListRef	ref = 0;

    xml = CFDataCreate(NULL, data, dataLen);
    if (xml) {
        ref = CFPropertyListCreateFromXMLData(NULL,
                xml,  kCFPropertyListImmutable, NULL);
        CFRelease(xml);
    }

    return ref;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
void *my_Allocate(int size) 
{
	void			*addr;
	kern_return_t	status;

	status = vm_allocate(mach_task_self(), (vm_address_t *)&addr, size, TRUE);
	if (status != KERN_SUCCESS) {
		return 0;
	}

	return addr;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
void my_Deallocate(void * addr, int size) 
{
	kern_return_t	status;

	if (addr == 0)
		return;

	status = vm_deallocate(mach_task_self(), (vm_address_t)addr, size);
	if (status != KERN_SUCCESS) {
	}

	return;
}

// ----------------------------------------------------------------------------
//	GetIntFromDict
// ----------------------------------------------------------------------------
Boolean GetIntFromDict (CFDictionaryRef dict, CFStringRef property, u_int32_t *outval, u_int32_t defaultval)
{
    CFNumberRef		ref;
	
	ref  = CFDictionaryGetValue(dict, property);
	if (isNumber(ref)
		&&  CFNumberGetValue(ref, kCFNumberSInt32Type, outval))
		return TRUE;
	
	*outval = defaultval;
	return FALSE;
}

// ----------------------------------------------------------------------------
//	GetStrFromDict
// ----------------------------------------------------------------------------
int GetStrFromDict (CFDictionaryRef dict, CFStringRef property, char *outstr, int maxlen, char *defaultval)
{
    CFStringRef		ref;

	ref  = CFDictionaryGetValue(dict, property);
	if (!isString(ref)
		|| !CFStringGetCString(ref, outstr, maxlen, kCFStringEncodingUTF8))
		strncpy(outstr, defaultval, maxlen);
	
	return strlen(outstr);
}

// ----------------------------------------------------------------------------
//	GetStrAddrFromDict
// ----------------------------------------------------------------------------
Boolean GetStrAddrFromDict (CFDictionaryRef dict, CFStringRef property, char *outstr, int maxlen)
{
    CFStringRef		ref;
	in_addr_t               addr;
	
	ref  = CFDictionaryGetValue(dict, property);
	if (isString(ref)
			&& CFStringGetCString(ref, outstr, maxlen, kCFStringEncodingUTF8)) {
					addr = inet_addr(outstr);
					return addr != INADDR_NONE;
	}
	
	return FALSE;
}

// ----------------------------------------------------------------------------
//	GetStrNetFromDict
// ----------------------------------------------------------------------------
Boolean GetStrNetFromDict (CFDictionaryRef dict, CFStringRef property, char *outstr, int maxlen)
{
    CFStringRef		ref;
	in_addr_t               net;

	ref  = CFDictionaryGetValue(dict, property);
	if (isString(ref)
			&& CFStringGetCString(ref, outstr, maxlen, kCFStringEncodingUTF8)) {
			net = inet_network(outstr);
			return net != INADDR_NONE;// && net != 0;
	}
	
	return FALSE;
}



/* -----------------------------------------------------------------------------
publish a dictionnary entry in the cache, given a key
----------------------------------------------------------------------------- */
int publish_keyentry(SCDynamicStoreRef store, CFStringRef key, CFStringRef entry, CFTypeRef value)
{
    CFMutableDictionaryRef	dict;
    CFPropertyListRef		ref;

    if ((ref = SCDynamicStoreCopyValue(store, key))) {
        dict = CFDictionaryCreateMutableCopy(0, 0, ref);
        CFRelease(ref);
    }
    else
        dict = CFDictionaryCreateMutable(0, 0, 
                &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    
    if (dict == 0)
        return 0;
    
    CFDictionarySetValue(dict,  entry, value);
    SCDynamicStoreSetValue(store, key, dict);
    CFRelease(dict);
    
    return 1;
 }
/* -----------------------------------------------------------------------------
unpublish a dictionnary entry from the cache, given the dict key
----------------------------------------------------------------------------- */
int unpublish_keyentry(SCDynamicStoreRef store, CFStringRef key, CFStringRef entry)
{
    CFMutableDictionaryRef	dict;
    CFPropertyListRef		ref;

    if ((ref = SCDynamicStoreCopyValue(store, key))) {
        if ((dict = CFDictionaryCreateMutableCopy(0, 0, ref))) {
            CFDictionaryRemoveValue(dict, entry);
            SCDynamicStoreSetValue(store, key, dict);
            CFRelease(dict);
        }
        CFRelease(ref);
    }
    return 0;
}


/* -----------------------------------------------------------------------------
publish a numerical entry in the cache, given a dictionary
----------------------------------------------------------------------------- */
int publish_dictnumentry(SCDynamicStoreRef store, CFStringRef serviceID, CFStringRef dict, CFStringRef entry, int val)
{
    int			ret = ENOMEM;
    CFNumberRef		num;
    CFStringRef		key;

    key = SCDynamicStoreKeyCreateNetworkServiceEntity(0, kSCDynamicStoreDomainState, serviceID, dict);
    if (key) {
        num = CFNumberCreate(NULL, kCFNumberIntType, &val);
        if (num) {
            ret = publish_keyentry(store, key, entry, num);
            CFRelease(num);
            ret = 0;
        }
        CFRelease(key);
    }
    return ret;
}


/* -----------------------------------------------------------------------------
 unpublish a dictionnary entry from the cache, given the dict name
 ----------------------------------------------------------------------------- */
int unpublish_dictentry(SCDynamicStoreRef store, CFStringRef serviceID, CFStringRef dict, CFStringRef entry)
{
    int			ret = ENOMEM;
    CFStringRef		key;
    
    key = SCDynamicStoreKeyCreateNetworkServiceEntity(0, kSCDynamicStoreDomainState, serviceID, dict);
    if (key) {
        ret = unpublish_keyentry(store, key, entry);
        CFRelease(key);
        ret = 0;
    }
    return ret;
}

/* -----------------------------------------------------------------------------
 publishes multiple dictionaries to the cache, given the dict names and the dicts. Indices must match up exactly
 ----------------------------------------------------------------------------- */
int publish_multiple_dicts(SCDynamicStoreRef store, CFStringRef serviceID, CFArrayRef dictNames, CFArrayRef dicts)
{
    CFDictionaryRef     dict = NULL;
    CFStringRef         dictName = NULL;
    int                 i;
    CFStringRef         key;
    CFMutableDictionaryRef   keys_to_add;
    int                 numDicts;
    int                 ret = ENOMEM;
    
	if (!store)
		return -1;
    
    /* Check arrays */
    if (!dictNames || !dicts)
        return -1;
    
    /* Verify sizes */
    numDicts = CFArrayGetCount(dictNames);
    if (numDicts != CFArrayGetCount(dicts))
        return -1;
    
    keys_to_add = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    
    for (i = 0; i < numDicts; i++) {
        dictName = CFArrayGetValueAtIndex(dictNames, i);
        dict = CFArrayGetValueAtIndex(dicts, i);
        if (isA_CFString(dictName) && isA_CFDictionary(dict)) {
            key = SCDynamicStoreKeyCreateNetworkServiceEntity(0, kSCDynamicStoreDomainState, serviceID, dictName);
            if (key) {
                CFDictionaryAddValue(keys_to_add, key, dict);
                CFRelease(key);
            }
        }
    }
    
    SCDynamicStoreSetMultiple(store, keys_to_add, NULL, NULL);
    
    my_CFRelease(&keys_to_add);
    
    return ret;
}

/* -----------------------------------------------------------------------------
 unpublish a dictionnary entry from the cache, given the dict name
 ----------------------------------------------------------------------------- */
int unpublish_dict(SCDynamicStoreRef store, CFStringRef serviceID, CFStringRef dict)
{
    int			ret = ENOMEM;
    CFStringRef		key;
    
	if (!store)
		return -1;
	
	if (dict)
		key = SCDynamicStoreKeyCreateNetworkServiceEntity(0, kSCDynamicStoreDomainState, serviceID, dict);
	else 
		key = SCDynamicStoreKeyCreate(0, CFSTR("%@/%@/%@/%@"), kSCDynamicStoreDomainState, kSCCompNetwork, kSCCompService, serviceID);
    if (key) {
        SCDynamicStoreRemoveValue(store, key);
        CFRelease(key);
		ret = 0;
    }

    return ret;
}

/* -----------------------------------------------------------------------------
 unpublish multiple dictionary entries from the cache, given an array of dict names
 ----------------------------------------------------------------------------- */
int unpublish_multiple_dicts(SCDynamicStoreRef store, CFStringRef serviceID, CFArrayRef dictNames, Boolean removeService)
{
    CFStringRef         dictName = NULL;
    int                 i;
    CFStringRef         key;
    CFMutableArrayRef   keys_to_remove;
    int                 numDictNames;
    int                 ret = ENOMEM;
    
	if (!store)
		return -1;
    
    keys_to_remove = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
    
    if (dictNames) {
        numDictNames = CFArrayGetCount(dictNames);
        for (i = 0; i < numDictNames; i++) {
            dictName = CFArrayGetValueAtIndex(dictNames, i);
            if (isA_CFString(dictName)) {
                key = SCDynamicStoreKeyCreateNetworkServiceEntity(0, kSCDynamicStoreDomainState, serviceID, dictName);
                if (key) {
                    CFArrayAppendValue(keys_to_remove, key);
                    CFRelease(key);
                }
            }
        }
    }
    
    if (removeService) {
        key = SCDynamicStoreKeyCreate(0, CFSTR("%@/%@/%@/%@"), kSCDynamicStoreDomainState, kSCCompNetwork, kSCCompService, serviceID);
        if (key) {
            CFArrayAppendValue(keys_to_remove, key);
            CFRelease(key);
        }
    }
    
    SCDynamicStoreSetMultiple(store, NULL, keys_to_remove, NULL);
    
    my_CFRelease(&keys_to_remove);
    
    return ret;
}

/* -----------------------------------------------------------------------------
publish a string entry in the cache, given a dictionary
----------------------------------------------------------------------------- */
int publish_dictstrentry(SCDynamicStoreRef store, CFStringRef serviceID, CFStringRef dict, CFStringRef entry, char *str, int encoding)
{
    
    int			ret = ENOMEM;
    CFStringRef 	ref;
    CFStringRef		key;
    
    key = SCDynamicStoreKeyCreateNetworkServiceEntity(0, kSCDynamicStoreDomainState, serviceID, dict);
    if (key) {
        ref = CFStringCreateWithCString(NULL, str, encoding);
        if (ref) {
            ret = publish_keyentry(store, key, entry, ref);
            CFRelease(ref);
            ret = 0;
        }
        CFRelease(key);
    }
    return ret;
}

/* -----------------------------------------------------------------------------
 return f s CFString contains an IP address
 ----------------------------------------------------------------------------- */
int
cfstring_is_ip(CFStringRef str)
{
	char *buf;
	struct in_addr ip = { 0 };
	CFIndex l;
	int n, result;
	CFRange range;

	if (!isString(str) || (l = CFStringGetLength(str)) == 0)
		return 0;
		
	buf = malloc(l+1);
	if (buf == NULL) {
		SCLog(TRUE, LOG_ERR, CFSTR("Failed to allocate memory"));
		return 0;
	}

	range = CFRangeMake(0, l);
	n = CFStringGetBytes(str, range, kCFStringEncodingMacRoman,
						 0, FALSE, (UInt8 *)buf, l, &l);
	buf[l] = '\0';

	result = inet_aton(buf, &ip);
	free(buf);
	return result;
}

/* -----------------------------------------------------------------------------
 ----------------------------------------------------------------------------- */
CFStringRef
copyPrimaryService (SCDynamicStoreRef store)
{
    CFDictionaryRef	dict;
    CFStringRef		key;
    CFStringRef		primary = NULL;
    
    if ((key = SCDynamicStoreKeyCreateNetworkGlobalEntity(NULL, 
                                                          kSCDynamicStoreDomainState, 
                                                          kSCEntNetIPv4)) == NULL) {
        return NULL;
    }
    
    dict = SCDynamicStoreCopyValue(store, key);
    CFRelease(key);
    if (isA_CFDictionary(dict)) {
        primary = CFDictionaryGetValue(dict,
                                       kSCDynamicStorePropNetPrimaryService);
        
        primary = isA_CFString(primary);
        if (primary)
            CFRetain(primary);
    }
    if (dict != NULL) {
        CFRelease(dict);
    }
    return primary;
}

/* -----------------------------------------------------------------------------
 ----------------------------------------------------------------------------- */
Boolean UpdatePasswordPrefs(CFStringRef serviceID, CFStringRef interfaceType, SCNetworkInterfacePasswordType passwordType, 
								   CFStringRef passwordEncryptionKey, CFStringRef passwordEncryptionValue, CFStringRef logTitle)
{
	SCPreferencesRef		prefs = NULL;
	SCNetworkServiceRef		service = NULL;
	SCNetworkInterfaceRef	interface = NULL;
	CFMutableDictionaryRef	newConfig = NULL;
	CFDictionaryRef			config;
	Boolean					ok, locked = FALSE, success = FALSE;
	
	prefs = SCPreferencesCreate(NULL, CFSTR("UpdatePassword"), NULL);
	if (prefs == NULL) {
		SCLog(TRUE, LOG_ERR, CFSTR("%@: SCPreferencesCreate fails"), logTitle);
        goto done;
	}
	// lock the prefs
	ok = SCPreferencesLock(prefs, TRUE);
	if (!ok) {
		SCLog(TRUE, LOG_ERR, CFSTR("%@: SCPreferencesLock fails"), logTitle);
        goto done;
	}
	
	locked = TRUE;
	
	// get the service
	service = SCNetworkServiceCopy(prefs, serviceID);
	if (service == NULL) {
		SCLog(TRUE, LOG_ERR, CFSTR("%@: SCNetworkServiceCopy fails"), logTitle);
        goto done;
	}
	// get the interface associated with the service
	interface = SCNetworkServiceGetInterface(service);
	if ((interface == NULL) || !CFEqual(SCNetworkInterfaceGetInterfaceType(interface), interfaceType)) {
		SCLog(TRUE, LOG_ERR, CFSTR("%@: interface not %@"), logTitle, interfaceType);
        goto done;
	}
	
	// remove any current password (from the system keychain)
	if (SCNetworkInterfaceCheckPassword(interface, passwordType)) {
        // if password current associated with this interface
        ok = SCNetworkInterfaceRemovePassword(interface,  passwordType);
        if (!ok) {
			SCLog(TRUE, LOG_ERR, CFSTR("%@: SCNetworkInterfaceRemovePassword fails"), logTitle);
        }
	}
	
	// update passworEncryptionKey
	config = SCNetworkInterfaceGetConfiguration(interface);
	
	if (config != NULL) {
        newConfig = CFDictionaryCreateMutableCopy(NULL, 0, config);
	}
	else {
        newConfig = CFDictionaryCreateMutable(0, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
	}
	
	if (newConfig == NULL) {
		SCLog(TRUE, LOG_ERR, CFSTR("%@: cannot allocate new interface configuration"), logTitle);
		goto done;
	}
	
	if (passwordEncryptionValue) {
		CFDictionarySetValue(newConfig, passwordEncryptionKey, passwordEncryptionValue);
	} else {
		CFDictionaryRemoveValue( newConfig, passwordEncryptionKey);
	}
	
	ok = SCNetworkInterfaceSetConfiguration(interface, newConfig);
	if ( !ok ) {
		SCLog(TRUE, LOG_ERR, CFSTR("%@: SCNetworkInterfaceSetConfiguration fails"), logTitle);
		goto done;
	}
	
	// commit & apply the changes
	ok = SCPreferencesCommitChanges(prefs);
	if (!ok) {
		SCLog(TRUE, LOG_ERR, CFSTR("%@: SCPreferencesCommitChanges fails"), logTitle);
		goto done;
	}
	ok = SCPreferencesApplyChanges(prefs);
	if (!ok) {
		SCLog(TRUE, LOG_ERR, CFSTR("%@: SCPreferencesApplyChanges fails"), logTitle);
		goto done;
	}
	
	success = TRUE;
	
	done :
	if (newConfig!= NULL) {
		CFRelease(newConfig);
	}
	if (service != NULL) {
		CFRelease(service);
	}
	if (locked) {
		SCPreferencesUnlock(prefs);
	}
	if (prefs != NULL) {
		CFRelease(prefs);
	}
	return success;
}


/* -----------------------------------------------------------------------------
set the sa_family field of a struct sockaddr, if it exists.
----------------------------------------------------------------------------- */
#define SET_SA_FAMILY(addr, family)		\
    bzero((char *) &(addr), sizeof(addr));	\
    addr.sa_family = (family); 			\
    addr.sa_len = sizeof(addr);

/* -----------------------------------------------------------------------------
Config the interface MTU
----------------------------------------------------------------------------- */
int set_ifmtu(char *ifname, int mtu)
{
    struct ifreq ifr;
	int ip_sockfd;
	
    ip_sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (ip_sockfd < 0) {
		syslog(LOG_INFO, "sifmtu: cannot create ip socket, %s",
	       strerror(errno));
		return 0;
	}

    strlcpy(ifr.ifr_name, ifname, sizeof (ifr.ifr_name));
    ifr.ifr_mtu = mtu;
    ioctl(ip_sockfd, SIOCSIFMTU, (caddr_t) &ifr);

	close(ip_sockfd);
	return 1;
}

/* -----------------------------------------------------------------------------
Config the interface IP addresses and netmask
----------------------------------------------------------------------------- */
int set_ifaddr(char *ifname, u_int32_t o, u_int32_t h, u_int32_t m)
{
    struct ifaliasreq ifra __attribute__ ((aligned (4)));   // Wcast-align fix - force alignment
	int ip_sockfd;
	
    ip_sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (ip_sockfd < 0) {
		syslog(LOG_INFO, "sifaddr: cannot create ip socket, %s",
	       strerror(errno));
		return 0;
	}

    strlcpy(ifra.ifra_name, ifname, sizeof(ifra.ifra_name));
	
    SET_SA_FAMILY(ifra.ifra_addr, AF_INET);
    (ALIGNED_CAST(struct sockaddr_in *) &ifra.ifra_addr)->sin_addr.s_addr = o;
    
	SET_SA_FAMILY(ifra.ifra_broadaddr, AF_INET);
    (ALIGNED_CAST(struct sockaddr_in *) &ifra.ifra_broadaddr)->sin_addr.s_addr = h;
    
	if (m != 0) {
		SET_SA_FAMILY(ifra.ifra_mask, AF_INET);
		(ALIGNED_CAST(struct sockaddr_in *) &ifra.ifra_mask)->sin_addr.s_addr = m;
    } 
	else
		bzero(&ifra.ifra_mask, sizeof(ifra.ifra_mask));
    
    if (ioctl(ip_sockfd, SIOCAIFADDR, (caddr_t) &ifra) < 0) {
		if (errno != EEXIST) {
			//error("Couldn't set interface address: %m");
			close(ip_sockfd);
			return 0;
		}
		//warning("Couldn't set interface address: Address %I already exists", o);
    }

	close(ip_sockfd);
	return 1;
}

/* -----------------------------------------------------------------------------
Clear the interface IP addresses, and delete routes
 * through the interface if possible
 ----------------------------------------------------------------------------- */
int clear_ifaddr(char *ifname, u_int32_t o, u_int32_t h)
{
	struct ifreq ifr __attribute__ ((aligned (4)));       // Wcast-align fix - force alignment
	int ip_sockfd;
	
	ip_sockfd = socket(AF_INET, SOCK_DGRAM, 0);
	if (ip_sockfd < 0) {
		syslog(LOG_INFO, "cifaddr: cannot create ip socket, %s",
		   strerror(errno));
		return 0;
	}

	strlcpy(ifr.ifr_name, ifname, sizeof(ifr.ifr_name));
	SET_SA_FAMILY(ifr.ifr_ifru.ifru_addr, AF_INET);
	(ALIGNED_CAST(struct sockaddr_in *) &ifr.ifr_ifru.ifru_addr)->sin_addr.s_addr = o;
	if (ioctl(ip_sockfd, SIOCDIFADDR, (caddr_t) &ifr) < 0) {
		close(ip_sockfd);
		return 0;
	}

	close(ip_sockfd);
	return 1;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
void
in6_len2mask(struct in6_addr *mask, int len)
{
	int i;

	bzero(mask, sizeof(*mask));
	for (i = 0; i < len / 8; i++)
		mask->s6_addr[i] = 0xff;
	if (len % 8)
		mask->s6_addr[i] = (0xff00 >> (len % 8)) & 0xff;
}

/* -----------------------------------------------------------------------------
mask address according to the mask
----------------------------------------------------------------------------- */
void
in6_maskaddr(struct in6_addr *addr, struct in6_addr *mask)
{
	int i;

	for (i = 0; i < sizeof(struct in6_addr); i++)
		addr->s6_addr[i] &= mask->s6_addr[i];
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
void 
in6_addr2net(struct in6_addr *addr, int prefix, struct in6_addr *net) {

    struct in6_addr	mask;
	int i;

	in6_len2mask(&mask, prefix);

	for (i = 0; i < sizeof(mask.s6_addr); i++)	
		(*net).s6_addr[i] = (*addr).s6_addr[i] & (mask).s6_addr[i];

}

/* -----------------------------------------------------------------------------
Config the interface IPv6 addresses
ll_addr must be a 64 bits address.
----------------------------------------------------------------------------- */
int set_ifaddr6 (char *ifname, struct in6_addr *addr, int prefix)
{
	int s;
	struct in6_aliasreq addreq6;
	struct in6_addr		mask;

	s = socket(AF_INET6, SOCK_DGRAM, 0);
	if (s < 0) {
		syslog(LOG_ERR, "set_ifaddr6: can't create IPv6 socket, %s",
			   strerror(errno));
		return 0;
	}

	memset(&addreq6, 0, sizeof(addreq6));
	strlcpy(addreq6.ifra_name, ifname, sizeof(addreq6.ifra_name));

	/* my addr */
	addreq6.ifra_addr.sin6_family = AF_INET6;
	addreq6.ifra_addr.sin6_len = sizeof(struct sockaddr_in6);
	memcpy(&addreq6.ifra_addr.sin6_addr, addr, sizeof(struct in6_addr));

	/* prefix mask: 128bit */
	addreq6.ifra_prefixmask.sin6_family = AF_INET6;
	addreq6.ifra_prefixmask.sin6_len = sizeof(struct sockaddr_in6);
	in6_len2mask(&mask, prefix);
    memcpy(&addreq6.ifra_prefixmask.sin6_addr, &mask, sizeof(struct in6_addr));

	/* address lifetime (infty) */
	addreq6.ifra_lifetime.ia6t_pltime = ND6_INFINITE_LIFETIME;
	addreq6.ifra_lifetime.ia6t_vltime = ND6_INFINITE_LIFETIME;

	if (IN6_IS_ADDR_LINKLOCAL(addr)) {
		if (ioctl(s, SIOCLL_START, &addreq6) < 0) {
			syslog(LOG_ERR, "set_ifaddr6: can't set link-local IPv6 address, %s",
				   strerror(errno));
			close(s);
			return 0;
		}
	} else {
		if (ioctl(s, SIOCAIFADDR_IN6, &addreq6) < 0) {
			syslog(LOG_ERR, "set_ifaddr6: can't set IPv6 address, %s",
				   strerror(errno));
			close(s);
			return 0;
		}
	}

	close(s);
	return 1;
}

/* -----------------------------------------------------------------------------
Clear the interface IPv6 addresses
 ----------------------------------------------------------------------------- */
int clear_ifaddr6 (char *ifname, struct in6_addr *addr)
{
    int s;
    struct in6_ifreq ifreq6;

   s = socket(AF_INET6, SOCK_DGRAM, 0);
    if (s < 0) {
        syslog(LOG_ERR, "set_ifaddr6: can't create IPv6 socket, %s",
	       strerror(errno));
        return 0;
    }

    memset(&ifreq6, 0, sizeof(ifreq6));
    strlcpy(ifreq6.ifr_name, ifname, sizeof(ifreq6.ifr_name));

    /* my addr */
    ifreq6.ifr_ifru.ifru_addr.sin6_family = AF_INET6;
    ifreq6.ifr_ifru.ifru_addr.sin6_len = sizeof(struct sockaddr_in6);
    memcpy(&ifreq6.ifr_ifru.ifru_addr.sin6_addr, addr, sizeof(struct in6_addr));

    if (ioctl(s, SIOCDIFADDR_IN6, &ifreq6) < 0) {
        syslog(LOG_ERR, "set_ifaddr6: can't set IPv6 address, %s",
	       strerror(errno));
        close(s);
        return 0;
    }

    close(s);
    return 1;
}


/* ----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
const char *inet_sockaddr_to_p(struct sockaddr *addr, char *buf, int buflen)
{
	void *p; 
    
    // Wcast-align fixes (void*) OK - inet_ntop has no alignment requirement
	switch (addr->sa_family) {
		case AF_INET:
            p = &((struct sockaddr_in *)(void*)addr)->sin_addr;			
            break;
		case AF_INET6:
            p = &((struct sockaddr_in6 *)(void*)addr)->sin6_addr;
			break;
		default: 
			return NULL;
	}

	return inet_ntop(addr->sa_family, p, buf, buflen);
}

/* ----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
int inet_p_to_sockaddr(char *buf, struct sockaddr *addr, int addrlen)
{
    bzero(addr, addrlen);
	
    // Wcast-align fixes (void*) OK - inet_pton has no alignment requirement
	if (addrlen >= sizeof(struct sockaddr_in)
		&& inet_pton(AF_INET, buf, &((struct sockaddr_in *)(void*)addr)->sin_addr)) {
        addr->sa_len = sizeof(struct sockaddr_in);
        addr->sa_family = AF_INET;
        return 1;
	}
	
	if (addrlen >= sizeof(struct sockaddr_in6) 
		&& inet_pton(AF_INET6, buf, &((struct sockaddr_in6 *)(void*)addr)->sin6_addr)) {
        addr->sa_len = sizeof(struct sockaddr_in6);
        addr->sa_family = AF_INET6;
        return 1;
	}

	return 0;
}


/* ----------------------------------------------------------------------------
return the default interface name and gateway for a given protocol
----------------------------------------------------------------------------- */
Boolean copyGateway(SCDynamicStoreRef store, u_int8_t family, char *ifname, int ifnamesize, struct sockaddr *gateway, int gatewaysize)
{
	CFDictionaryRef dict;
	CFStringRef key, string;
	Boolean found_interface = FALSE;
	Boolean found_router = FALSE;

 	if (ifname) 
		ifname[0] = 0;
	if (gateway)
		bzero(gateway, gatewaysize);

	if (family != AF_INET && family != AF_INET6)
		return FALSE;

	key = SCDynamicStoreKeyCreateNetworkGlobalEntity(NULL, kSCDynamicStoreDomainState, 
		(family == AF_INET) ? kSCEntNetIPv4 : kSCEntNetIPv6);
    if (key) {
      dict = SCDynamicStoreCopyValue(store, key);
		CFRelease(key);
        if (dict) {
		
			if ((string = CFDictionaryGetValue(dict, kSCDynamicStorePropNetPrimaryInterface))) {
				found_interface = TRUE;
				if (ifname) 
					CFStringGetCString(string, ifname, ifnamesize, kCFStringEncodingUTF8);
			}
			if ((string = CFDictionaryGetValue(dict, (family == AF_INET) ? kSCPropNetIPv4Router : kSCPropNetIPv6Router))) {
				char routeraddress[256];
				routeraddress[0] = 0;
				CFStringGetCString(string, (char*)routeraddress, sizeof(routeraddress), kCFStringEncodingUTF8);
				if (routeraddress[0]) {
					struct sockaddr_storage addr;
					if (inet_p_to_sockaddr(routeraddress, (struct sockaddr *)&addr, sizeof(addr))) {
						found_router = TRUE;
						if (gateway && gatewaysize >= addr.ss_len)
							bcopy(&addr, gateway, addr.ss_len);
					}
				}
			}
			CFRelease(dict);
		}
	}
	return (found_interface && found_router);
}

/* ----------------------------------------------------------------------------
return TRUE if there is a default interface and gateway for a given protocol 
----------------------------------------------------------------------------- */
Boolean hasGateway(SCDynamicStoreRef store, u_int8_t family)
{
	return copyGateway(store, family, 0, 0, 0, 0);

}

/* ----------------------------------------------------------------------------
 Create a "NULL Service" primary IPv6 dictionary for the dynamic store. This
 prevents any other service from becoming primary on IPv6.
 ----------------------------------------------------------------------------- */
#ifndef kIsNULL
#define kIsNULL		CFSTR("IsNULL") /* CFBoolean */
#endif
CFDictionaryRef create_ipv6_dummy_primary(char *if_name)
{
	CFMutableArrayRef		array;
	CFMutableDictionaryRef	ipv6_dict;
	int						isprimary = 1;
	CFNumberRef				num;
	CFStringRef				str;
	
	/* create the IPv6 dictionnary */
	if ((ipv6_dict = CFDictionaryCreateMutable(0, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks)) == 0)
		return NULL;
	
	if ((array = CFArrayCreateMutable(0, 1, &kCFTypeArrayCallBacks))) {
		CFArrayAppendValue(array, CFSTR("::1"));
		CFDictionarySetValue(ipv6_dict, kSCPropNetIPv6Addresses, array);
		CFRelease(array);
	}
	
	CFDictionarySetValue(ipv6_dict, kSCPropNetIPv6Router, CFSTR("::1"));
	
	num = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &isprimary);
	if (num) {
		CFDictionarySetValue(ipv6_dict, kSCPropNetOverridePrimary, num);
		CFRelease(num);
	}
	
	CFDictionarySetValue(ipv6_dict, kIsNULL, kCFBooleanTrue);
	
	if (if_name) {
		if ((str = CFStringCreateWithFormat(NULL, NULL, CFSTR("%s"), if_name))) {
			CFDictionarySetValue(ipv6_dict, kSCPropInterfaceName, str);
			CFRelease(str);
		}
	}
	
	return ipv6_dict;
}

/* ----------------------------------------------------------------------------
get dictionary for ip addresses to publish later to configd
use new state information model
----------------------------------------------------------------------------- */
CFDictionaryRef create_stateaddr(SCDynamicStoreRef store, CFStringRef serviceID, char *if_name, u_int32_t server, u_int32_t o,
			u_int32_t h, u_int32_t m, int isprimary, CFArrayRef includedRoutes, CFArrayRef excludedRoutes)
{
    struct in_addr		addr;
    CFMutableArrayRef		array;
    CFMutableDictionaryRef	ipv4_dict;
    CFStringRef			str;
    CFNumberRef		num;
	SCNetworkServiceRef netservRef;
	
    /* create the IPV4 dictionnary */
    if ((ipv4_dict = CFDictionaryCreateMutable(0, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks)) == 0)
        return NULL;

	/* set the ip address src and dest arrays */
    if ((array = CFArrayCreateMutable(0, 1, &kCFTypeArrayCallBacks))) {
        addr.s_addr = o;
        if ((str = CFStringCreateWithFormat(0, 0, CFSTR(IP_FORMAT), IP_LIST(&addr.s_addr)))) {
            CFArrayAppendValue(array, str);
            CFRelease(str);
            CFDictionarySetValue(ipv4_dict, kSCPropNetIPv4Addresses, array); 
        }
        CFRelease(array);
    }
	
    /* set the router */
    addr.s_addr = h;
    if ((str = CFStringCreateWithFormat(NULL, NULL, CFSTR(IP_FORMAT), IP_LIST(&addr.s_addr)))) {
        CFDictionarySetValue(ipv4_dict, kSCPropNetIPv4Router, str);
        CFRelease(str);
    }
	
	num = CFNumberCreate(NULL, kCFNumberIntType, &isprimary);
	if (num) {
        CFDictionarySetValue(ipv4_dict, kSCPropNetOverridePrimary, num);
		CFRelease(num);
	}
	
	if (if_name) {
		if ((str = CFStringCreateWithFormat(NULL, NULL, CFSTR("%s"), if_name))) {
			CFDictionarySetValue(ipv4_dict, kSCPropInterfaceName, str);
			CFRelease(str);
		}
	}
	
    /* set the server */
    addr.s_addr = server;
    if ((str = CFStringCreateWithFormat(NULL, NULL, CFSTR(IP_FORMAT), IP_LIST(&addr.s_addr)))) {
		CFDictionarySetValue(ipv4_dict, CFSTR("ServerAddress"), str);
        CFRelease(str);
    }
	
#if 0
    /* add the network signature */
    if (network_signature) {
		if (str = CFStringCreateWithFormat(NULL, NULL, CFSTR("%s"), network_signature)) {
			CFDictionarySetValue(ipv4_dict, CFSTR("NetworkSignature"), str);
			CFRelease(str);
		}
	}
#endif
	
	
	/* rank service, to prevent it from becoming primary */
	if (!isprimary) {
		netservRef = _SCNetworkServiceCopyActive(store, serviceID);
		if (netservRef) {
			SCNetworkServiceSetPrimaryRank(netservRef, kSCNetworkServicePrimaryRankLast);
			CFRelease(netservRef);
		}
	}
	
	if (includedRoutes) {
		CFDictionarySetValue(ipv4_dict, kSCPropNetIPv4AdditionalRoutes, includedRoutes);
	}
	
	if (excludedRoutes) {
		CFDictionarySetValue(ipv4_dict, kSCPropNetIPv4ExcludedRoutes, excludedRoutes);
	}

    return ipv4_dict;
}

/* -----------------------------------------------------------------------------
 get dns information
 ----------------------------------------------------------------------------- */
CFDictionaryRef create_dns(SCDynamicStoreRef store, CFStringRef serviceID, CFArrayRef dns, CFStringRef domain, CFArrayRef supp_domains, Boolean neverSearchDomains)
{    
    CFMutableDictionaryRef	dict = NULL;
    CFStringRef			key = NULL;
    CFPropertyListRef		ref;
	
    if (store == NULL)
        return NULL;
	
    key = SCDynamicStoreKeyCreateNetworkServiceEntity(0, kSCDynamicStoreDomainState, serviceID, kSCEntNetDNS);
    if (!key) 
        goto end;
	
    if ((ref = SCDynamicStoreCopyValue(store, key))) {
        dict = CFDictionaryCreateMutableCopy(0, 0, ref);
        CFRelease(ref);
    } else
        dict = CFDictionaryCreateMutable(0, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
	
    if (!dict || (CFGetTypeID(dict) != CFDictionaryGetTypeID()))
        goto end;
	
    CFDictionarySetValue(dict, kSCPropNetDNSServerAddresses, dns);
	
    if (domain)
		CFDictionarySetValue(dict, kSCPropNetDNSDomainName, domain);
	
    if (supp_domains)
		CFDictionarySetValue(dict, kSCPropNetDNSSupplementalMatchDomains, supp_domains);
    
#ifndef kSCPropNetDNSSupplementalMatchDomainsNoSearch
#define kSCPropNetDNSSupplementalMatchDomainsNoSearch CFSTR("SupplementalMatchDomainsNoSearch")
#endif
    
    if (neverSearchDomains) {
        AddNumber(dict, kSCPropNetDNSSupplementalMatchDomainsNoSearch, 1);
    }
	
	/* warn lookupd of upcoming change */
	notify_post("com.apple.system.dns.delay");

end:
    my_CFRelease(&key);
    return dict;
	
}

/* -----------------------------------------------------------------------------
 ----------------------------------------------------------------------------- */
Boolean equal_address(struct sockaddr *addr1, struct sockaddr *addr2)
{
				
	if (addr1->sa_family != addr2->sa_family)
		return FALSE;
	
    /* Wcast-align fixes - don't use assignement or standard compare for ptrs of unknown alignment */
	if (addr1->sa_family == AF_INET) {
        return (!bcmp(&((struct sockaddr_in *)(void*)addr1)->sin_addr.s_addr, 
                      &((struct sockaddr_in *)(void*)addr2)->sin_addr.s_addr, 
                      sizeof(struct in_addr)));     
	}
	
	if (addr1->sa_family == AF_INET6) {
		return (!bcmp(&((struct sockaddr_in6 *)(void*)addr1)->sin6_addr, 
                      &((struct sockaddr_in6 *)(void*)addr2)->sin6_addr, 
                      sizeof(struct in6_addr)));
	}
	
	return FALSE;
}

/* -----------------------------------------------------------------------------
    add/remove a route via a gateway
----------------------------------------------------------------------------- */
int
route_gateway(int cmd, struct sockaddr *dest, struct sockaddr *mask, struct sockaddr *gateway, int use_gway_flag, int use_blackhole_flag)
{
    int 			len;
    int 			rtm_seq = 0;

    struct rtmsg_in4 {
	struct rt_msghdr	hdr;
	struct sockaddr_in	dst;
	struct sockaddr_in	gway;
	struct sockaddr_in	mask;
    };
    struct rtmsg_in6 {
	struct rt_msghdr	hdr;
	struct sockaddr_in6	dst;
	struct sockaddr_in6	gway;
	struct sockaddr_in6	mask;
    };

    int 			sockfd = -1;
    struct rtmsg_in6 rtmsg; // use rtmsg_in6 since it is the bigger one;
	    
	if (dest == NULL || (dest->sa_family != AF_INET
		&& dest->sa_family != AF_INET6))
		return -1;

    if ((sockfd = socket(PF_ROUTE, SOCK_RAW, PF_ROUTE)) < 0) {
	syslog(LOG_INFO, "route_gateway: open routing socket failed, %s",
	       strerror(errno));
	return (-1);
    }

    memset(&rtmsg, 0, sizeof(rtmsg));

	// fill in header, which is common to IPv4 and IPv6
    rtmsg.hdr.rtm_type = cmd;
    rtmsg.hdr.rtm_flags = RTF_UP | RTF_STATIC;
    if (use_gway_flag)
        rtmsg.hdr.rtm_flags |= RTF_GATEWAY;
    if (use_blackhole_flag)
        rtmsg.hdr.rtm_flags |= RTF_BLACKHOLE;
    rtmsg.hdr.rtm_version = RTM_VERSION;
    rtmsg.hdr.rtm_seq = ++rtm_seq;
    rtmsg.hdr.rtm_addrs = RTA_DST | RTA_NETMASK | RTA_GATEWAY;

	// then fill in address specific portion
	if (dest->sa_family == AF_INET) {
		struct rtmsg_in4 *rtmsg4 = (struct rtmsg_in4 *)&rtmsg; 

		bcopy(dest, &rtmsg4->dst, sizeof(rtmsg4->dst));
		if (gateway)
			bcopy(gateway, &rtmsg4->gway, sizeof(rtmsg4->gway));
		if (mask)
			bcopy(mask, &rtmsg4->mask, sizeof(rtmsg4->mask));

		len = sizeof(struct rtmsg_in4);
	}
	else {
		struct rtmsg_in6 *rtmsg6 = (struct rtmsg_in6 *)&rtmsg;
		
		bcopy(dest, &rtmsg6->dst, sizeof(rtmsg6->dst));
		if (gateway)
			bcopy(gateway, &rtmsg6->gway, sizeof(rtmsg6->gway));
		if (mask)
			bcopy(mask, &rtmsg6->mask, sizeof(rtmsg6->mask));

		len = sizeof(struct rtmsg_in6);
	}

	rtmsg.hdr.rtm_msglen = len;
    if (write(sockfd, &rtmsg, len) < 0) {
		syslog((cmd == RTM_DELETE)? LOG_DEBUG : LOG_ERR, "route_gateway: write routing socket failed, %s", strerror(errno));

#if 0
		/* print routing message for debugging */
		char buf[256];
		syslog(LOG_ERR, "-------");
		struct rtmsg_in4 *rtmsg4 = (struct rtmsg_in4 *)&rtmsg; 
		inet_sockaddr_to_p(dest->sa_family == AF_INET ? (struct sockaddr *)&rtmsg4->dst : (struct sockaddr *)&rtmsg.dst, buf, sizeof(buf));
		syslog(LOG_ERR, "route_gateway: rtmsg.dst = %s", buf);
		inet_sockaddr_to_p(dest->sa_family == AF_INET ? (struct sockaddr *)&rtmsg4->gway : (struct sockaddr *)&rtmsg.gway, buf, sizeof(buf));
		syslog(LOG_ERR, "route_gateway: rtmsg.gway = %s", buf);
		inet_sockaddr_to_p(dest->sa_family == AF_INET ? (struct sockaddr *)&rtmsg4->mask : (struct sockaddr *)&rtmsg.mask, buf, sizeof(buf));
		syslog(LOG_ERR, "route_gateway: rtmsg.mask = %s", buf);
		syslog(LOG_ERR, "-------");
#endif

		close(sockfd);
		return (-1);
    }

    close(sockfd);
    return (0);
}

/* -----------------------------------------------------------------------------
add/remove a host route
----------------------------------------------------------------------------- */
boolean_t
set_host_gateway(int cmd, struct sockaddr *host, struct sockaddr *gateway, char *ifname, int isnet)
{
    int 			len;
    int 			rtm_seq = 0;
    struct rtmsg_in4 {
	struct rt_msghdr	hdr;
	struct sockaddr_in	dst;
	struct sockaddr_in	gway;
	struct sockaddr_in	mask;
	struct sockaddr_dl	link;
    };
    struct rtmsg_in6 {
	struct rt_msghdr	hdr;
	struct sockaddr_in6	dst;
	struct sockaddr_in6	gway;
	struct sockaddr_in6	mask;
	struct sockaddr_dl	link;
    };

    int 			sockfd = -1;
    struct rtmsg_in6 rtmsg; // use rtmsg_in6 since it is the bigger one;
	struct sockaddr_dl *link;
	struct in6_addr     ip6_zeros;
    struct in_addr      ip4_zeros;

	if (host == NULL || (host->sa_family != AF_INET
		&& host->sa_family != AF_INET6))
		return FALSE;
	
    if ((sockfd = socket(PF_ROUTE, SOCK_RAW, PF_ROUTE)) < 0) {
	syslog(LOG_INFO, "host_gateway: open routing socket failed, %s",
	       strerror(errno));
	return (FALSE);
    }

    memset(&rtmsg, 0, sizeof(rtmsg));
    rtmsg.hdr.rtm_type = cmd;
    rtmsg.hdr.rtm_flags = RTF_UP | RTF_STATIC;
    if (isnet)
        rtmsg.hdr.rtm_flags |= RTF_CLONING;
    else 
        rtmsg.hdr.rtm_flags |= RTF_HOST;
	bzero(&ip6_zeros, sizeof(ip6_zeros));
    bzero(&ip4_zeros, sizeof(ip4_zeros));
    // Wcast-align fix - use memcmp for unaligned comparison
    if (gateway && (((gateway->sa_family == AF_INET && memcmp(&((struct sockaddr_in *)(void*)gateway)->sin_addr.s_addr, &ip4_zeros, sizeof(struct in_addr)))) ||
		(gateway->sa_family == AF_INET6 && memcmp(&((struct sockaddr_in6 *)(void*)gateway)->sin6_addr, &ip6_zeros, sizeof(struct in6_addr))))) {
        rtmsg.hdr.rtm_flags |= RTF_GATEWAY;
	}
    
	rtmsg.hdr.rtm_version = RTM_VERSION;
    rtmsg.hdr.rtm_seq = ++rtm_seq;
    rtmsg.hdr.rtm_addrs = RTA_DST | RTA_NETMASK | RTA_GATEWAY;
	
	if (host->sa_family == AF_INET) {
		struct rtmsg_in4 *rtmsg4 = (struct rtmsg_in4 *)&rtmsg; 

		bcopy(host, &rtmsg4->dst, sizeof(rtmsg4->dst));

		if (gateway) 
			bcopy(gateway, &rtmsg4->gway, sizeof(rtmsg4->gway));
		
		rtmsg4->mask.sin_len = sizeof(rtmsg4->mask);
		rtmsg4->mask.sin_family = AF_INET;
		rtmsg4->mask.sin_addr.s_addr = 0xFFFFFFFF;

		len = sizeof(struct rtmsg_in4);
		link = &rtmsg4->link;
	}
	else {
		struct rtmsg_in6 *rtmsg6 = (struct rtmsg_in6 *)&rtmsg;
		
		bcopy(host, &rtmsg6->dst, sizeof(rtmsg6->dst));
		
		if (gateway) 
			bcopy(gateway, &rtmsg6->gway, sizeof(rtmsg6->gway));

		rtmsg6->mask.sin6_len = sizeof(rtmsg6->mask);
		rtmsg6->mask.sin6_family = AF_INET6;
		rtmsg6->mask.sin6_addr.__u6_addr.__u6_addr32[0] = 0xFFFFFFFF;
		rtmsg6->mask.sin6_addr.__u6_addr.__u6_addr32[1] = 0xFFFFFFFF;
		rtmsg6->mask.sin6_addr.__u6_addr.__u6_addr32[2] = 0xFFFFFFFF;
		rtmsg6->mask.sin6_addr.__u6_addr.__u6_addr32[3] = 0xFFFFFFFF;

		len = sizeof(struct rtmsg_in6);
		link = &rtmsg6->link;
	}
	
    if (ifname) {
		link->sdl_len = sizeof(rtmsg.link);
		link->sdl_family = AF_LINK;
		link->sdl_nlen = MIN(strlen(ifname), sizeof(link->sdl_data));
		rtmsg.hdr.rtm_addrs |= RTA_IFP;
		bcopy(ifname, link->sdl_data, link->sdl_nlen);
    }
    else {
		/* no link information */
		len -= sizeof(rtmsg.link);
    }
	
    rtmsg.hdr.rtm_msglen = len;
    if (write(sockfd, &rtmsg, len) < 0) {
		syslog((cmd == RTM_DELETE)? LOG_DEBUG : LOG_ERR, "host_gateway: write routing socket failed, command %d, %s", cmd, strerror(errno));

#if 0
		/* print routing message for debugging */
		char buf[256];
		syslog(LOG_ERR, "********");
		struct rtmsg_in4 *rtmsg4 = (struct rtmsg_in4 *)&rtmsg; 
		syslog(LOG_ERR, "host_gateway: dest->sa_family = %d rtmsg.hdr.rtm_msglen = %d", host->sa_family, rtmsg.hdr.rtm_msglen);
		inet_sockaddr_to_p(host->sa_family == AF_INET ? (struct sockaddr *)&rtmsg4->dst : (struct sockaddr *)&rtmsg.dst, buf, sizeof(buf));
		syslog(LOG_ERR, "host_gateway: rtmsg.dst = %s", buf);
		inet_sockaddr_to_p(host->sa_family == AF_INET ? (struct sockaddr *)&rtmsg4->gway : (struct sockaddr *)&rtmsg.gway, buf, sizeof(buf));
		syslog(LOG_ERR, "host_gateway: rtmsg.gway = %s", buf);
		inet_sockaddr_to_p(host->sa_family == AF_INET ? (struct sockaddr *)&rtmsg4->mask : (struct sockaddr *)&rtmsg.mask, buf, sizeof(buf));
		syslog(LOG_ERR, "host_gateway: rtmsg.mask = %s", buf);
		syslog(LOG_ERR, "********");
#endif
		close(sockfd);
		return (FALSE);
    }

    close(sockfd);
    return (TRUE);
}

/* ----------------------------------------------------------------------------
 get proxies using to publish to configd
 ----------------------------------------------------------------------------- */
CFDictionaryRef create_proxies(SCDynamicStoreRef store, CFStringRef serviceID, int autodetect, CFStringRef server, int port, int bypasslocal, 
		CFStringRef exceptionlist, CFArrayRef supp_domains)
{
	int				val, ret = -1;
    CFStringRef		cfstr = NULL;
    CFArrayRef		cfarray;
    CFNumberRef		cfnum,  cfone = NULL;
    CFMutableDictionaryRef	proxies_dict = NULL;
		
    if ((proxies_dict = CFDictionaryCreateMutable(0, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks)) == 0)
        goto fail;

	val = 1;
	cfone = CFNumberCreate(NULL, kCFNumberIntType, &val);
	if (cfone == NULL)
		goto fail;
	
	if (autodetect) {
		CFDictionarySetValue(proxies_dict, kSCPropNetProxiesProxyAutoDiscoveryEnable, cfone);
	}
	else if (server) {
		
		CFDictionarySetValue(proxies_dict, kSCPropNetProxiesFTPEnable, cfone);
		CFDictionarySetValue(proxies_dict, kSCPropNetProxiesHTTPEnable, cfone);
		CFDictionarySetValue(proxies_dict, kSCPropNetProxiesHTTPSEnable, cfone);

		cfnum = CFNumberCreate(NULL, kCFNumberIntType, &port);
		if (cfnum == NULL)
			goto fail;
		
		CFDictionarySetValue(proxies_dict, kSCPropNetProxiesFTPPort, cfnum);
		CFDictionarySetValue(proxies_dict, kSCPropNetProxiesHTTPPort, cfnum);
		CFDictionarySetValue(proxies_dict, kSCPropNetProxiesHTTPSPort, cfnum);
		CFRelease(cfnum);
		
		CFDictionarySetValue(proxies_dict, kSCPropNetProxiesFTPProxy, server);
		CFDictionarySetValue(proxies_dict, kSCPropNetProxiesHTTPProxy, server);
		CFDictionarySetValue(proxies_dict, kSCPropNetProxiesHTTPSProxy, server);

		cfnum = CFNumberCreate(NULL, kCFNumberIntType, &bypasslocal);
		if (cfnum == NULL)
			goto fail;
		CFDictionarySetValue(proxies_dict, kSCPropNetProxiesExcludeSimpleHostnames, cfnum);
		CFRelease(cfnum);
		
		if (exceptionlist) {
			cfarray = CFStringCreateArrayBySeparatingStrings(NULL, exceptionlist, CFSTR(";"));
			if (cfarray) {
				CFDictionarySetValue(proxies_dict, kSCPropNetProxiesExceptionsList, cfarray);
				CFRelease(cfarray);
			}
		}
	}

#ifndef kSCPropNetProxiesSupplementalMatchDomains			
#define kSCPropNetProxiesSupplementalMatchDomains kSCPropNetDNSSupplementalMatchDomains
#endif

    if (supp_domains)
		CFDictionarySetValue(proxies_dict, kSCPropNetProxiesSupplementalMatchDomains, supp_domains);
		
	ret = 0;	
		
fail:
	
    my_CFRelease(&cfone);
	my_CFRelease(&cfstr);
    return proxies_dict;
}

/* -----------------------------------------------------------------------------
 Create the new tun interface, and return the socket
 ----------------------------------------------------------------------------- */
int create_tun_interface(char *name, int name_max_len, int *index, int flags, int ext_stats)
{

	struct ctl_info kernctl_info;
	struct sockaddr_ctl kernctl_addr;
	u_int32_t optlen;
	int tunsock = -1;

	tunsock = socket(PF_SYSTEM, SOCK_DGRAM, SYSPROTO_CONTROL);
	if (tunsock == -1) {
		SCLog(TRUE, LOG_ERR, CFSTR("create_tun_interface: cannot create kernel control socket (errno = %d)"), errno);
		goto fail;
	}
		
	bzero(&kernctl_info, sizeof(kernctl_info));
    strlcpy(kernctl_info.ctl_name, UTUN_CONTROL_NAME, sizeof(kernctl_info.ctl_name));
	if (ioctl(tunsock, CTLIOCGINFO, &kernctl_info)) {
		SCLog(TRUE, LOG_ERR, CFSTR("create_tun_interface: ioctl failed on kernel control socket (errno = %d)"), errno);
		goto fail;
	}
	
	bzero(&kernctl_addr, sizeof(kernctl_addr)); // sets the sc_unit field to 0
	kernctl_addr.sc_len = sizeof(kernctl_addr);
	kernctl_addr.sc_family = AF_SYSTEM;
	kernctl_addr.ss_sysaddr = AF_SYS_CONTROL;
	kernctl_addr.sc_id = kernctl_info.ctl_id;
	kernctl_addr.sc_unit = 0; // we will get the unit number from getpeername
	if (connect(tunsock, (struct sockaddr *)&kernctl_addr, sizeof(kernctl_addr))) {
		SCLog(TRUE, LOG_ERR, CFSTR("create_tun_interface: connect failed on kernel control socket (errno = %d)"), errno);
		goto fail;
	}

	optlen = name_max_len;
	if (getsockopt(tunsock, SYSPROTO_CONTROL, UTUN_OPT_IFNAME, name, &optlen)) {
		SCLog(TRUE, LOG_ERR, CFSTR("create_tun_interface: getsockopt ifname failed on kernel control socket (errno = %d)"), errno);
		goto fail;	
	}

	*index = if_nametoindex(name);

	if (flags) {
		int optflags = 0;
		optlen = sizeof(u_int32_t);
		if (getsockopt(tunsock, SYSPROTO_CONTROL, UTUN_OPT_FLAGS, &optflags, &optlen)) {
			SCLog(TRUE, LOG_ERR, CFSTR("create_tun_interface: getsockopt flags failed on kernel control socket (errno = %d)"), errno);
			goto fail;	
		}
		 
		optflags |= (UTUN_FLAGS_NO_INPUT + UTUN_FLAGS_NO_OUTPUT);
		optlen = sizeof(u_int32_t);
		if (setsockopt(tunsock, SYSPROTO_CONTROL, UTUN_OPT_FLAGS, &optflags, optlen)) {
			SCLog(TRUE, LOG_ERR, CFSTR("create_tun_interface: setsockopt flags failed on kernel control socket (errno = %d)"), errno);
			goto fail;	
		}
	}
	
	if (ext_stats) {
		int optval = 1;
		if (setsockopt(tunsock, SYSPROTO_CONTROL, UTUN_OPT_EXT_IFDATA_STATS, &optval, sizeof(optval))) {
			SCLog(TRUE, LOG_ERR, CFSTR("create_tun_interface: setsockopt externat stats failed on kernel control socket (errno = %d)"), errno);
			goto fail;	
		}
	}

	return tunsock;
	
fail:
	my_close(tunsock);
	return -1;
	
}

/* -----------------------------------------------------------------------------
 Set the delegate interface for the tun interface
 ----------------------------------------------------------------------------- */
int set_tun_delegate(int tunsock, char *delegate_ifname)
{
    int result = 0;
    
    if ((result = setsockopt(tunsock, SYSPROTO_CONTROL, UTUN_OPT_SET_DELEGATE_INTERFACE, delegate_ifname, strlen(delegate_ifname))))
        SCLog(TRUE, LOG_ERR, CFSTR("set_tun_delegate: setsockopt delegate interface failed on kernel control socket (errno = %s)"), strerror(errno));

    return result;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
int event_create_socket(void * ctxt, int *eventfd, CFSocketRef *eventref, CFSocketCallBack callout, Boolean anysubclass)
{
    CFRunLoopSourceRef	rls;
    CFSocketContext	context = { 0, ctxt, NULL, NULL, NULL };
    struct kev_request	kev_req;
        
	*eventfd = socket(PF_SYSTEM, SOCK_RAW, SYSPROTO_EVENT);
	if (*eventfd < 0) {
		SCLog(TRUE, LOG_ERR, CFSTR("event_create_socket cannot create event socket (errno = %d) "), errno);
		goto fail;
	}

	kev_req.vendor_code = KEV_VENDOR_APPLE;
	kev_req.kev_class = KEV_NETWORK_CLASS;
	kev_req.kev_subclass = anysubclass ? KEV_ANY_SUBCLASS : KEV_INET_SUBCLASS;
	ioctl(*eventfd, SIOCSKEVFILT, &kev_req);
        
    if ((*eventref = CFSocketCreateWithNative(NULL, *eventfd, 
                    kCFSocketReadCallBack, callout, &context)) == 0) {
        goto fail;
    }
    if ((rls = CFSocketCreateRunLoopSource(NULL, *eventref, 0)) == 0)
        goto fail;

    CFRunLoopAddSource(CFRunLoopGetCurrent(), rls, kCFRunLoopDefaultMode);
    CFRelease(rls);

    return 0;
    
fail:
	if (*eventref) {
		CFSocketInvalidate(*eventref);
		CFRelease(*eventref);
	}
	else 
		if (*eventfd >= 0) {
			close(*eventfd);
	}
	*eventref = 0;
	*eventfd = -1;

    return -1;
}

/* -----------------------------------------------------------------------------
 ----------------------------------------------------------------------------- */
typedef struct exec_callback_args {
	CFRunLoopRef            rl;
	CFRunLoopSourceRef      rls;
	CFRunLoopSourceContext  rlc;
	pid_t                   pid;
	int                     status;
	struct rusage	        rusage;
	SCDPluginExecCallBack   callback;
	void                   *callbackContext;
} exec_callback_args_t;

/* -----------------------------------------------------------------------------
 ----------------------------------------------------------------------------- */
static 
void exec_callback(pid_t pid, int status, struct rusage *rusage, void *context)
{
	if (isA_CFData(context)) {
		exec_callback_args_t *args = ALIGNED_CAST(__typeof__(args))CFDataGetMutableBytePtr((CFMutableDataRef)context);
		args->pid = pid;
		args->status = status;
		bcopy(rusage, &args->rusage, sizeof(args->rusage));
		// args->context already contains the service
		CFRunLoopSourceSignal(args->rls);
		CFRunLoopWakeUp(args->rl);
	}
}

static void
SCNCPluginExecCallbackRunLoopSource (void *info)
{
	if (isA_CFData(info)) {
		exec_callback_args_t *args = ALIGNED_CAST(__typeof__(args))CFDataGetMutableBytePtr((CFMutableDataRef)info);
		if (args->callback) {
			args->callback(args->pid, args->status, &args->rusage, args->callbackContext);
		}
		CFRunLoopSourceInvalidate(args->rls);
		CFRelease(args->rls);
		CFRelease(args->rl);
		CFRelease((CFMutableDataRef)info); // release (was allocated in SCNCPluginExecCallbackRunLoopSourceInit)
	}
}

CFMutableDataRef
SCNCPluginExecCallbackRunLoopSourceInit (CFRunLoopRef           runloop,
										 SCDPluginExecCallBack  callback,
										 void	               *callbackContext)
{
	CFMutableDataRef      dataRef; // to be used as SCNCPluginExecCallbackRunLoopSource's info
	UInt8                *dataPtr;
	exec_callback_args_t  args;

	// create dataref and fill it with runloop args
	dataRef = CFDataCreateMutable(NULL, sizeof(args));
	if (dataRef == NULL) {
		return NULL;
	}
	CFDataSetLength(dataRef, sizeof(args));
	dataPtr = CFDataGetMutableBytePtr(dataRef);

	bzero(&args, sizeof(args));
	args.rlc.info = dataRef; // use as SCNCPluginExecCallbackRunLoopSource's info
	args.rlc.perform = SCNCPluginExecCallbackRunLoopSource;
	args.rls = CFRunLoopSourceCreate(NULL, 0, &args.rlc);
	if (!args.rls){
		CFRelease(dataRef);
		return NULL;
	}
	args.callback = callback;
	args.callbackContext = callbackContext;
	if (!runloop) {
		args.rl = CFRunLoopGetCurrent();
	} else {
		args.rl = runloop;
	}
	CFRetain(args.rl);
	CFRunLoopAddSource(args.rl, args.rls, kCFRunLoopDefaultMode);
	bcopy(&args, dataPtr, sizeof(args));
	return dataRef; // to be used as exec_callback's context
}

pid_t
SCNCPluginExecCommand (CFRunLoopRef           runloop,
					   SCDPluginExecCallBack  callback,
					   void        	         *callbackContext,
					   uid_t                  uid,
					   gid_t                  gid,
					   const char            *path,
					   char * const           argv[])
{
	pid_t            rc;
	CFMutableDataRef exec_callback_context;

	exec_callback_context = SCNCPluginExecCallbackRunLoopSourceInit(runloop, callback, callbackContext);
	if (!exec_callback_context){
		SCLog(TRUE, LOG_ERR, CFSTR("SCNC: failed to initialize plugin exec_callback's runloop source"));
		return -1;
	}

	rc = _SCDPluginExecCommand(exec_callback, 
							   exec_callback_context, 
							   uid, 
							   gid, 
							   path, 
							   argv);
	return rc;
}

pid_t
SCNCPluginExecCommand2 (CFRunLoopRef           runloop,
						SCDPluginExecCallBack  callback,
						void                  *callbackContext,
						uid_t                  uid,
						gid_t                  gid,
						const char            *path,
						char * const           argv[],
						SCDPluginExecSetup     setup,
						void                  *setupContext)
{
	pid_t            rc;
	CFMutableDataRef exec_callback_context;

	exec_callback_context = SCNCPluginExecCallbackRunLoopSourceInit(runloop, callback, callbackContext);
	if (!exec_callback_context){
		SCLog(TRUE, LOG_ERR, CFSTR("SCNC: failed to initialize plugin exec_callback's runloop source"));
		return -1;
	}

	rc = _SCDPluginExecCommand2(exec_callback,
								exec_callback_context,
								uid,
								gid, 
								path, 
								argv, 
								setup, 
								setupContext);
	return rc;
}

#define SBSLAUNCHER_NAME "sbslauncher"
#define MAX_SBSLAUNCHER_ARGS 16

/* Variable arguments are of type char*, and the last argument must be NULL */
pid_t
SCNCExecSBSLauncherCommandWithArguments (char *command,
										 SCDPluginExecSetup setup,
										 SCDPluginExecCallBack callback,
										 void *callbackContext,
										 ...)
{
	va_list			arguments;
	CFStringRef 	resourceDir = NULL;
	CFURLRef 		resourceURL = NULL, absoluteURL = NULL;
	char			thepath[MAXPATHLEN];
	pid_t			pid = 0;
	char 			*cmdarg[MAX_SBSLAUNCHER_ARGS];
    
	resourceURL = CFBundleCopyResourcesDirectoryURL(gBundleRef);
	
	if (resourceURL == NULL)
		goto done;
	
	absoluteURL = CFURLCopyAbsoluteURL(resourceURL);
	if (absoluteURL == NULL)
		goto done;
	
	resourceDir = CFURLCopyPath(absoluteURL);
	if (resourceDir == NULL)
		goto done;
    
	if (!CFStringGetCString(resourceDir, thepath, sizeof(thepath), kCFStringEncodingMacRoman))
		goto done;
	
	strlcat(thepath, SBSLAUNCHER_NAME, sizeof(thepath));
	
	cmdarg[0] = SBSLAUNCHER_NAME;
	cmdarg[1] = command;
	
	va_start(arguments, callbackContext);
	int i = 2;
	
	char *arg_i = va_arg(arguments, char *);
	while (arg_i != NULL && i < (MAX_SBSLAUNCHER_ARGS - 1)) {
		cmdarg[i++] = arg_i;
		arg_i = va_arg(arguments, char *);
	}
	cmdarg[i] = NULL;
	va_end(arguments);
	
	if (setup) {
		pid = SCNCPluginExecCommand2(NULL, callback, callbackContext, 0, 0, thepath, cmdarg, setup, callbackContext);
	} else {
		pid = SCNCPluginExecCommand(NULL, callback, callbackContext, 0, 0, thepath, cmdarg);
	}

done:
	if (resourceDir)
		CFRelease(resourceDir);
	if (absoluteURL)
		CFRelease(absoluteURL);
	if (resourceURL)
		CFRelease(resourceURL);
    
	return pid;
}

void
extractEnvironmentVariablesApplierFunction (const void *key, const void *value, void *context)
{
	struct service *serv = (struct service *)context;
	CFRange range;

	if (isA_CFString(key)) {
		char *key_buf = (char *)serv->envKeys[serv->envCount];
		char *value_buf = (char *)serv->envValues[serv->envCount];

		key_buf[0] = '\0';
		range.location = 0;
		range.length = CFStringGetLength((CFStringRef)key);
		if (range.length <= 0 ||
			range.length >= sizeof(*serv->envKeys) ||
			CFStringGetBytes((CFStringRef)key, range, kCFStringEncodingUTF8, 0, false, (UInt8 *)key_buf, sizeof(*serv->envKeys), NULL) <= 0) {
			SCLog(TRUE, LOG_ERR, CFSTR("invalid EnvironmentVariables key %@, value %@"), key, value);
			return;
		}
		key_buf[range.length] = '\0';
		serv->envCount++;

		value_buf[0] = '\0';
		if (isA_CFString(value)) {
			range.location = 0;
			range.length = CFStringGetLength((CFStringRef)value);
			if (range.length <= 0 ||
				range.length >= sizeof(*serv->envValues) ||
				CFStringGetBytes((CFStringRef)value, range, kCFStringEncodingUTF8, 0, false, (UInt8 *)value_buf, sizeof(*serv->envValues), NULL) <= 0) {
				SCLog(TRUE, LOG_ERR, CFSTR("invalid EnvironmentVariables key %@, value %@"), key, value);
				return;
			}
			value_buf[range.length] = '\0';
		} else if (isA_CFNumber(value)) {
			int64_t number = 0;
			if (CFNumberGetValue((CFNumberRef)value, kCFNumberSInt64Type, &number)) {
				snprintf(value_buf, sizeof(*serv->envValues), "%lld", number);
			} else {
				SCLog(TRUE, LOG_ERR, CFSTR("invalid EnvironmentVariables key %@, value %@"), key, value);
				return;
			}
		} else if (isA_CFBoolean(value)) {
			snprintf(value_buf, sizeof(*serv->envValues), "%s", CFBooleanGetValue((CFBooleanRef)value) ? "Yes" : "No");
		} else {
			SCLog(TRUE, LOG_ERR, CFSTR("invalid EnvironmentVariables key %@, value %@"), key, value);
			return;
		}
	} else {
		SCLog(TRUE, LOG_ERR, CFSTR("invalid EnvironmentVariables key"));
	}
}

CFDictionaryRef
collectEnvironmentVariables (SCDynamicStoreRef storeRef, CFStringRef serviceID)
{
	if (!storeRef) {
		SCLog(TRUE, LOG_ERR, CFSTR("invalid DynamicStore passed to %s"), __FUNCTION__);
		return NULL;
	}

	if (!serviceID) {
		SCLog(TRUE, LOG_ERR, CFSTR("invalid serviceID passed to %s"), __FUNCTION__);
		return NULL;
	}

	return copyEntity(storeRef, kSCDynamicStoreDomainSetup, serviceID, CFSTR("EnvironmentVariables"));
}

void
extractEnvironmentVariables (CFDictionaryRef envVarDict, struct service *serv)
{
	if (!envVarDict) {
		return;
	} else if (isA_CFDictionary(envVarDict) &&
			   CFDictionaryGetCount(envVarDict) > 0) {
		int count = CFDictionaryGetCount(envVarDict);

		if (serv->envKeys) {
			free(serv->envKeys);
			serv->envKeys = NULL;
		}
		if (serv->envValues) {
			free(serv->envValues);
			serv->envValues = NULL;
		}

		serv->envCount = 0;
		serv->envKeys = (char *)malloc(count * sizeof(envKeyValue_t));
		serv->envValues = (char *)malloc(count * sizeof(envKeyValue_t));
		if (!serv->envKeys || !serv->envValues) {
			SCLog(TRUE, LOG_ERR, CFSTR("Failed to allocate for environment variables"));
			return;
		}

		CFDictionaryApplyFunction(envVarDict, extractEnvironmentVariablesApplierFunction, serv);
	} else {
		SCLog(TRUE, LOG_ERR, CFSTR("empty or invalid EnvironmentVariables dictionary"));
	}
}

/* 
 * Don't call SCLog() in this function as it's unsafe in a post-fork handler that requires async-signal safe.
 */
void
applyEnvironmentVariables (struct service *serv)
{
	int i;

	for (i = 0; i < serv->envCount; i++) {
		if (serv->envKeys[i]) {
			unsetenv(serv->envKeys[i]);
			if (serv->envValues[i])
			    setenv(serv->envKeys[i], serv->envValues[i], TRUE);
		}
	}

	serv->envCount = 0;
	if (serv->envKeys) {
		free(serv->envKeys);
		serv->envKeys = NULL;
	}
	if (serv->envValues) {
		free(serv->envValues);
		serv->envValues = NULL;
	}

}

const char *
scnc_get_reason_str(int scnc_reason)
{
	switch (scnc_reason) {
		case SCNC_STOP_CTRL_STOP:
			return scnc_ctrl_stopped;
		case SCNC_STOP_SYS_SLEEP:
			return scnc_sys_sleep;
		case SCNC_STOP_USER_LOGOUT:
			return scnc_usr_logout;
		case SCNC_STOP_USER_SWITCH:
			return scnc_usr_switch;
		case SCNC_STOP_SOCK_DISCONNECT:
		case SCNC_STOP_SOCK_DISCONNECT_NO_CLIENT:
			return scnc_sock_disco;
		case SCNC_STOP_PLUGIN_CHANGE:
			return scnc_plugin_chg;
		case SCNC_STOP_APP_REMOVED:
			return scnc_app_rem;
		case SCNC_STOP_USER_REQ:
		case SCNC_STOP_USER_REQ_NO_CLIENT:
			return scnc_usr_req;
		case SCNC_STOP_SERV_DISPOSE:
			return scnc_serv_disp;
		case SCNC_STOP_TERM_ALL:
			return scnc_term_all;
	}
	return CONSTSTR(NULL);
}

const char *
ppp_error_to_string (u_int32_t native_ppp_error)
{
    switch (native_ppp_error) {
        case EXIT_FATAL_ERROR:
            return ppp_fatal;
        case EXIT_OPTION_ERROR:
            return ppp_option;
        case EXIT_NOT_ROOT:
            return ppp_not_root;
        case EXIT_NO_KERNEL_SUPPORT:
            return ppp_no_kern;
        case EXIT_USER_REQUEST:
            return ppp_user_req;
        case EXIT_LOCK_FAILED:
            return ppp_lock_fail;
        case EXIT_OPEN_FAILED:
            return ppp_open_fail;
        case EXIT_CONNECT_FAILED:
            return ppp_conn_fail;
        case EXIT_PTYCMD_FAILED:
            return ppp_pty_fail;
        case EXIT_NEGOTIATION_FAILED:
            return ppp_nego_fail;
        case EXIT_PEER_AUTH_FAILED:
            return ppp_peer_auth_fail;
        case EXIT_IDLE_TIMEOUT:
            return ppp_idle_tmo;
        case EXIT_CONNECT_TIME:
            return ppp_sess_tmo;
        case EXIT_CALLBACK:
            return ppp_callback;
        case EXIT_PEER_DEAD:
            return ppp_peer_dead;
        case EXIT_HANGUP:
            return ppp_disco_by_dev;
        case EXIT_LOOPBACK:
            return ppp_loopback;
        case EXIT_INIT_FAILED:
            return ppp_init_fail;
        case EXIT_AUTH_TOPEER_FAILED:
            return ppp_auth_fail;
        case EXIT_TERMINAL_FAILED:
            return ppp_term_fail;
        case EXIT_DEVICE_ERROR:
            return ppp_dev_err;
        case EXIT_PEER_NOT_AUTHORIZED:
            return ppp_peer_unauth;
        case EXIT_CNID_AUTH_FAILED:
            return ppp_cnid_auth_fail;
        case EXIT_PEER_UNREACHABLE:
            return ppp_peer_unreach;
    }
	
    return CONSTSTR(NULL);
}

const char *
ppp_dev_error_to_string (u_int16_t subtype, u_int32_t native_dev_error)
{
    // override with a more specific error
    if (native_dev_error) {
        switch (subtype) {
            case PPP_TYPE_L2TP:
                switch (native_dev_error) {
                    case EXIT_L2TP_NOSERVER:
                        return ppp_dev_no_srvr;
                    case EXIT_L2TP_NOANSWER:
                        return ppp_dev_no_ans;
                    case EXIT_L2TP_PROTOCOLERROR:
                        return ppp_dev_prot_err;
                    case EXIT_L2TP_NETWORKCHANGED:
                        return ppp_dev_net_chg;
                    case EXIT_L2TP_NOSHAREDSECRET:
                        return ppp_dev_psk;
                    case EXIT_L2TP_NOCERTIFICATE:
                        return ppp_dev_cert;
                }
                break;
				
            case PPP_TYPE_PPTP:
                switch (native_dev_error) {
                    case EXIT_PPTP_NOSERVER:
                        return ppp_dev_no_srvr;
                    case EXIT_PPTP_NOANSWER:
                        return ppp_dev_no_ans;
                    case EXIT_PPTP_PROTOCOLERROR:
                        return ppp_dev_prot_err;
                    case EXIT_PPTP_NETWORKCHANGED:
                        return ppp_dev_net_chg;
                }
                break;
				
            case PPP_TYPE_SERIAL:
                switch (native_dev_error) {
                    case EXIT_PPPSERIAL_NOCARRIER:
                        return ppp_dev_no_car;
                    case EXIT_PPPSERIAL_NONUMBER:
                        return ppp_dev_no_num;
                    case EXIT_PPPSERIAL_BADSCRIPT:
                        return ppp_dev_bad_script;
                    case EXIT_PPPSERIAL_BUSY:
                        return ppp_dev_busy;
                    case EXIT_PPPSERIAL_NODIALTONE:
                        return ppp_dev_no_dial;
                    case EXIT_PPPSERIAL_ERROR:
                        return ppp_dev_modem_err;
                    case EXIT_PPPSERIAL_NOANSWER:
                        return ppp_dev_no_ans;
                    case EXIT_PPPSERIAL_HANGUP:
                        return ppp_dev_hang;
                }
                break;
                
            case PPP_TYPE_PPPoE:
                switch (native_dev_error) {
                    case EXIT_PPPoE_NOSERVER:
                        return ppp_dev_no_srvr;
                    case EXIT_PPPoE_NOSERVICE:
                        return ppp_dev_no_srvc;
                    case EXIT_PPPoE_NOAC:
                        return ppp_dev_no_ac;
                    case EXIT_PPPoE_NOACSERVICE:
                        return ppp_dev_no_ac_srvc;
                    case EXIT_PPPoE_CONNREFUSED:
                        return ppp_dev_conn_refuse;
                }
                break;
        }
    }
    
    return CONSTSTR(NULL);
}

const char *
ipsec_error_to_string (int status)
{
    switch (status) {
        case IPSEC_GENERIC_ERROR:
            return ipsec_gen_err;
        case IPSEC_NOSERVERADDRESS_ERROR:
            return ipsec_no_srvr_addr;
        case IPSEC_NOSHAREDSECRET_ERROR:
            return ipsec_no_psk;
        case IPSEC_NOCERTIFICATE_ERROR:
            return ipsec_no_cert;
        case IPSEC_RESOLVEADDRESS_ERROR:
            return ipsec_dns_err;
        case IPSEC_NOLOCALNETWORK_ERROR:
            return ipsec_no_local;
        case IPSEC_CONFIGURATION_ERROR:
            return ipsec_cfg_err;
        case IPSEC_RACOONCONTROL_ERROR:
            return ipsec_ctrl_err;
        case IPSEC_CONNECTION_ERROR:
            return ipsec_conn_err;
        case IPSEC_NEGOTIATION_ERROR:
            return ipsec_nego_err;
        case IPSEC_SHAREDSECRET_ERROR:
            return ipsec_psk_err;
        case IPSEC_SERVER_CERTIFICATE_ERROR:
            return ipsec_srvr_cert_err;
        case IPSEC_CLIENT_CERTIFICATE_ERROR:
            return ipsec_cli_cert_err;
        case IPSEC_XAUTH_ERROR:
            return ipsec_xauth_err;
        case IPSEC_NETWORKCHANGE_ERROR:
            return ipsec_net_chg;
        case IPSEC_PEERDISCONNECT_ERROR:
            return ipsec_peer_disco;
        case IPSEC_PEERDEADETECTION_ERROR:
            return ipsec_peer_dead;
        case IPSEC_EDGE_ACTIVATION_ERROR:
            return ipsec_edge_err;
        case IPSEC_IDLETIMEOUT_ERROR:
            return ipsec_idle_tmo;
        case IPSEC_CLIENT_CERTIFICATE_PREMATURE:
            return ipsec_cli_cert_pre;
        case IPSEC_CLIENT_CERTIFICATE_EXPIRED:
            return ipsec_cli_cert_exp;
        case IPSEC_SERVER_CERTIFICATE_PREMATURE:
            return ipsec_srvr_cert_pre;
        case IPSEC_SERVER_CERTIFICATE_EXPIRED:
            return ipsec_srvr_cert_exp;
        case IPSEC_SERVER_CERTIFICATE_INVALID_ID:
            return ipsec_srvr_cert_id;
    }

    return CONSTSTR(NULL);
}

const char *
vpn_error_to_string (u_int32_t status)
{
    switch (status) {
		case VPN_GENERIC_ERROR:
			return vpn_gen_err;
		case VPN_NOSERVERADDRESS_ERROR:
			return vpn_no_srvr_addr;
		case VPN_NOCERTIFICATE_ERROR:
			return vpn_no_cert;
		case VPN_RESOLVEADDRESS_ERROR:
			return vpn_dns_err;
		case VPN_NOLOCALNETWORK_ERROR:
			return vpn_no_local;
		case VPN_CONFIGURATION_ERROR:
			return vpn_cfg_err;
		case VPN_CONTROL_ERROR:
			return vpn_ctrl_err;
		case VPN_CONNECTION_ERROR:
			return vpn_conn_err;
		case VPN_NETWORKCHANGE_ERROR:
			return vpn_net_chg;
		case VPN_PEERDISCONNECT_ERROR:
			return vpn_peer_disco;
		case VPN_PEERDEADETECTION_ERROR:
			return vpn_peer_dead;
		case VPN_PEERNOTRESPONDING_ERROR:
			return vpn_peer_unresp;
		case VPN_NEGOTIATION_ERROR:
			return vpn_nego_err;
		case VPN_XAUTH_ERROR:
			return vpn_xauth_err;
		case VPN_EDGE_ACTIVATION_ERROR:
			return vpn_edge_err;
		case VPN_IDLETIMEOUT_ERROR:
			return vpn_idle_tmo;
		case VPN_ADDRESSINVALID_ERROR:
			return vpn_addr_invalid;
		case VPN_APPREQUIRED_ERROR:
			return vpn_ap_req;
		case VPN_CLIENT_CERTIFICATE_PREMATURE:
			return vpn_cli_cert_pre;
		case VPN_CLIENT_CERTIFICATE_EXPIRED:
			return vpn_cli_cert_exp;
		case VPN_SERVER_CERTIFICATE_PREMATURE:
			return vpn_srvr_cert_pre;
		case VPN_SERVER_CERTIFICATE_EXPIRED:
			return vpn_srvr_cert_exp;
		case VPN_SERVER_CERTIFICATE_INVALID_ID:
			return vpn_srvr_cert_id;
		case VPN_PLUGIN_UPDATE_REQUIRED:
			return vpn_plugin_upd;
		case VPN_PLUGIN_DISABLED:
			return vpn_plugin_dis;
	}
	
    return CONSTSTR(NULL);
}

/* -----------------------------------------------------------------------------
 cleanup dynamic store for serv->serviceID.
 ----------------------------------------------------------------------------- */
static void
removekeys( void *key, const void *value, void *context)
{
    Boolean ret;
    
    ret = SCDynamicStoreRemoveValue((SCDynamicStoreRef)context, (CFStringRef)key);
    if (!ret)
        SCLog(TRUE, LOG_ERR, CFSTR("PPP Controller: removekeys SCDynamicStoreRemoveValue fails to remove key %@."), key);
}

void
cleanup_dynamicstore(void *serv)
{
    CFDictionaryRef     entities = NULL;
    CFMutableArrayRef   patterns = NULL;
    CFStringRef         pattern = NULL;
    
    /* clean up dynamic store */
    pattern = SCDynamicStoreKeyCreateNetworkServiceEntity(NULL, kSCDynamicStoreDomainState, ((struct service*)serv)->serviceID, kSCCompAnyRegex);
    if (pattern == NULL)
        return;
    patterns = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
    if (patterns == NULL)
        goto fail;
    CFArrayAppendValue(patterns, pattern);
    entities = SCDynamicStoreCopyMultiple(gDynamicStore, NULL, patterns);
    if (entities)
        CFDictionaryApplyFunction(entities, (CFDictionaryApplierFunction)removekeys, (void*)gDynamicStore);

fail:
    my_CFRelease((void *)&pattern);
    my_CFRelease((void *)&patterns);
    my_CFRelease((void *)&entities);
}

/* -----------------------------------------------------------------------------
 Pass in the optional exceptionServiceID to find the primary interface excluding
 the specified service; if the exceptionServiceID is primary, find the next active
 service interface based on NWI order.
 
 If NULL is passed for exceptionServiceID, find the true primary interface.
 ----------------------------------------------------------------------------- */
CFStringRef copy_primary_interface_name(CFStringRef exceptionServiceID)
{
    CFStringRef		interfaceName = NULL, string = NULL, key = NULL, serviceID = NULL;
    CFDictionaryRef dict = NULL;
    Boolean			mustSearchNWIOrder = FALSE;
	
    key = SCDynamicStoreKeyCreateNetworkGlobalEntity(NULL, kSCDynamicStoreDomainState, kSCEntNetIPv4);
    if (key) {
        dict = SCDynamicStoreCopyValue(gDynamicStore, key);
        CFRelease(key);
        if (isDictionary(dict)) {
            string = CFDictionaryGetValue(dict, kSCDynamicStorePropNetPrimaryInterface);
            if (isString(string)) {
                serviceID = CFDictionaryGetValue(dict, kSCDynamicStorePropNetPrimaryService);
                if (serviceID && my_CFEqual(serviceID, exceptionServiceID)) {
                    mustSearchNWIOrder = TRUE;
                } else {
                    // The interface is not the exception, so allow
                    interfaceName = CFStringCreateCopy(NULL, string);
                }
            }
            CFRelease(dict);
        }
    }
    
    if (interfaceName == NULL && mustSearchNWIOrder && exceptionServiceID != NULL) {
        CFStringRef exceptionInterfaceName = copy_interface_name(exceptionServiceID);
        nwi_state_t nwi_state = nwi_state_copy();
        if (exceptionInterfaceName && nwi_state) {
            for (nwi_ifstate_t interface = nwi_state_get_first_ifstate(nwi_state, AF_INET); interface != NULL; interface = nwi_ifstate_get_next(interface, AF_INET)) {
                char *nwi_interface_name = nwi_ifstate_get_ifname(interface);
                CFStringRef nwiInterfaceName = CFStringCreateWithCString(kCFAllocatorDefault, nwi_interface_name, kCFStringEncodingASCII);
                if (nwiInterfaceName && !my_CFEqual(nwiInterfaceName, exceptionInterfaceName)) {
                    nwi_ifstate_flags flags = nwi_ifstate_get_flags(interface);
                    if ((flags & NWI_IFSTATE_FLAGS_HAS_IPV4) && (flags & NWI_IFSTATE_FLAGS_HAS_DNS)) {
                        interfaceName = my_CFRetain(nwiInterfaceName);
                    }
                }
                my_CFRelease(&nwiInterfaceName);
                
                // If we found the desired interface, exit now
                if (interfaceName != NULL) {
                    break;
                }
            }
        }
        my_CFRelease(&exceptionInterfaceName);
        nwi_state_release(nwi_state);
    }
    
    return interfaceName;
}

/* -----------------------------------------------------------------------------
 copy the string separatedy by / at index
 ----------------------------------------------------------------------------- */
static
CFStringRef copy_str_at_index(CFStringRef key, int index)
{
    
    CFArrayRef	components;
    CFStringRef foundstr = NULL;
    
    components = CFStringCreateArrayBySeparatingStrings(NULL, key, CFSTR("/"));
    if (CFArrayGetCount(components) == 5) {
        if ((foundstr = CFArrayGetValueAtIndex(components, index))){
            CFRetain(foundstr);
        }
    }
    CFRelease(components);
    return foundstr;
}

/* -----------------------------------------------------------------------------
 ----------------------------------------------------------------------------- */
CFStringRef copy_service_id_for_interface(CFStringRef interfaceName)
{
    CFDictionaryRef     dict = NULL;
    CFStringRef         pattern = NULL;
    CFMutableArrayRef   patterns = NULL;
    CFStringRef         *keys = NULL;
    CFDictionaryRef     *values = NULL;
    CFIndex             count = 0;
    CFIndex             i = 0;
    CFStringRef         serviceID = NULL;
    
    if (!isString(interfaceName)) {
        goto done;
    }
    
    patterns = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks);
    pattern = SCDynamicStoreKeyCreateNetworkServiceEntity(kCFAllocatorDefault,
                                                          kSCDynamicStoreDomainState,
                                                          kSCCompAnyRegex,
                                                          kSCEntNetIPv4);
    
    if (patterns == NULL || pattern == NULL)
        goto done;
    CFArrayAppendValue(patterns, pattern);
    
    dict = SCDynamicStoreCopyMultiple(gDynamicStore, NULL, patterns);
    count = CFDictionaryGetCount(dict);
    
    keys = calloc(count, sizeof(CFStringRef));
    values = calloc(count, sizeof(CFDictionaryRef));
    if (keys == NULL || values == NULL)
        goto done;
    CFDictionaryGetKeysAndValues(dict, (const void**)keys, (const void**)values);
    
    for (i=0; i < count; i++) {
        CFDictionaryRef ipv4Dict = NULL;
        CFStringRef     ipv4Key = NULL;
        
        ipv4Key  = keys[i];
        ipv4Dict = values[i];
        
        if (!isString(ipv4Key) || !isDictionary(ipv4Dict)) {
            continue;
        }
        
        /* Match interface name here */
        if (my_CFEqual(CFDictionaryGetValue(ipv4Dict, kSCPropInterfaceName), interfaceName)) {
            if ((CFStringHasPrefix(ipv4Key, kSCDynamicStoreDomainState)) && (CFStringHasSuffix(ipv4Key, kSCEntNetIPv4))) {
                /* Copy Service ID */
                serviceID = copy_str_at_index(ipv4Key, 3);
            }
            break;
        }
    }
    
done:
    my_CFRelease(&pattern);
    my_CFRelease(&patterns);
    my_CFRelease(&dict);
    if (keys) {
        free(keys);
    }
    if (values) {
        free(values);
    }
    
    return serviceID;
}

/* -----------------------------------------------------------------------------
 ----------------------------------------------------------------------------- */
CFStringRef copy_interface_type(CFStringRef serviceID)
{
	CFDictionaryRef interface_dict = NULL;
	CFStringRef interface_key = NULL;
	CFStringRef hardware = NULL;
	CFStringRef interface_type = NULL;
	
	if (!isString(serviceID)) {
		goto done;
	}
	
	interface_key = SCDynamicStoreKeyCreateNetworkServiceEntity(kCFAllocatorDefault,
                                                                kSCDynamicStoreDomainSetup,
                                                                serviceID,
                                                                kSCEntNetInterface);
	
	interface_dict = SCDynamicStoreCopyValue(gDynamicStore, interface_key);
	if (!isDictionary(interface_dict)) {
		goto done;
	}
	
	hardware = CFDictionaryGetValue(interface_dict, kSCPropNetInterfaceHardware);
    
	if (isString(hardware)) {
        if (my_CFEqual(hardware, kSCEntNetAirPort)) {
            interface_type = CFRetain(kSCValNetVPNOnDemandRuleInterfaceTypeMatchWiFi);
        } else if (my_CFEqual(hardware, kSCEntNetEthernet)) {
            interface_type = CFRetain(kSCValNetVPNOnDemandRuleInterfaceTypeMatchEthernet);
        }
#if TARGET_OS_IPHONE
        else if (my_CFEqual(hardware, kSCEntNetCommCenter)) {
            interface_type = CFRetain(kSCValNetVPNOnDemandRuleInterfaceTypeMatchCellular);
        }
#endif
	}
	
done:
    my_CFRelease(&interface_key);
    my_CFRelease(&interface_dict);
	
	return interface_type;
}

Boolean primary_interface_is_cellular(Boolean *hasPrimaryInterface)
{
    Boolean isCellular = FALSE;
#if TARGET_OS_IPHONE
	Boolean foundPrimaryInterface = FALSE;
	nwi_state_t state = nwi_state_copy();
	if (state != NULL) {
		int families[] = { AF_INET, AF_INET6 };
		for (uint32_t i = 0; !foundPrimaryInterface && (i < sizeof(families) / sizeof(families[0])); i++) {
			nwi_ifstate_t ifstate;
			for (ifstate = nwi_state_get_first_ifstate(state, families[i]);
				 ifstate != NULL;
				 ifstate = nwi_ifstate_get_next(ifstate, families[i]))
			{
				if (nwi_ifstate_get_vpn_server(ifstate) != NULL) {
					// Skip VPN interfaces
					continue;
				}

				foundPrimaryInterface = TRUE;
				isCellular = nwi_ifstate_get_reachability_flags(ifstate) & kSCNetworkReachabilityFlagsIsWWAN;
				break;
			}
		}
		nwi_state_release(state);
	}

    if (hasPrimaryInterface) {
        *hasPrimaryInterface = foundPrimaryInterface;
    }
#endif
	return isCellular;
}

Boolean interface_is_cellular(const char *interface_name)
{
	Boolean isCellular = FALSE;
	if (interface_name == NULL) {
		return isCellular;
	}

#if TARGET_OS_IPHONE
	nwi_state_t state = nwi_state_copy();

	if (state != NULL) {
		nwi_ifstate_t ifstate = nwi_state_get_ifstate(state, interface_name);
		if (ifstate != NULL) {
			isCellular = ((nwi_ifstate_get_reachability_flags(ifstate) & kSCNetworkReachabilityFlagsIsWWAN) != 0);
		}
		nwi_state_release(state);
	}
#endif

	return isCellular;
}

/* -----------------------------------------------------------------------------
 ----------------------------------------------------------------------------- */
CFDictionaryRef copy_dns_dict(CFStringRef serviceID)
{
    CFStringRef     dnsKey = NULL;
    CFDictionaryRef dnsDict = NULL;
    
    if (!isString(serviceID)) {
		goto done;
	}
    
    dnsKey = SCDynamicStoreKeyCreateNetworkServiceEntity(kCFAllocatorDefault,
                                                         kSCDynamicStoreDomainState,
                                                         serviceID,
                                                         kSCEntNetDNS);
    if (dnsKey == NULL) {
        goto done;
    }
    
    dnsDict = SCDynamicStoreCopyValue(gDynamicStore, dnsKey);
    if (dnsDict == NULL) {
        goto done;
    }
	
done:
	my_CFRelease(&dnsKey);
	
	return dnsDict;
}

/* -----------------------------------------------------------------------------
 ----------------------------------------------------------------------------- */
CFStringRef copy_interface_name(CFStringRef serviceID)
{
    CFStringRef		interfaceName = NULL;
    CFStringRef     ipv4Key = NULL;
    CFDictionaryRef ipv4Dict = NULL;
    
    if (!isString(serviceID)) {
        goto done;
    }
    
    ipv4Key = SCDynamicStoreKeyCreateNetworkServiceEntity(kCFAllocatorDefault,
                                                         kSCDynamicStoreDomainState,
                                                         serviceID,
                                                         kSCEntNetIPv4);
    if (ipv4Key == NULL) {
        goto done;
    }
    
    ipv4Dict = SCDynamicStoreCopyValue(gDynamicStore, ipv4Key);
    if (ipv4Dict == NULL) {
        goto done;
    }
    
    interfaceName = CFDictionaryGetValue(ipv4Dict, kSCPropInterfaceName);
    if (interfaceName) {
        interfaceName = CFStringCreateCopy(kCFAllocatorDefault, interfaceName);
    }
    
done:
    my_CFRelease(&ipv4Key);
    my_CFRelease(&ipv4Dict);
    
    return interfaceName;
}

/* -----------------------------------------------------------------------------
 ----------------------------------------------------------------------------- */
CFArrayRef
copy_service_order(void)
{
	CFDictionaryRef	ip_dict = NULL;
	CFStringRef		key;
	CFArrayRef		serviceorder = NULL;

	key = CREATEGLOBALSETUP(kSCEntNetIPv4);
	if (key) {
		ip_dict = (CFDictionaryRef)SCDynamicStoreCopyValue(gDynamicStore, key);
		if (ip_dict) {
			serviceorder = CFDictionaryGetValue(ip_dict, kSCPropNetServiceOrder);        
			if (serviceorder) {
				CFRetain(serviceorder);
			}
			CFRelease(ip_dict);
		}
		CFRelease(key);
	}

	return serviceorder;
}

CFStringRef scnc_copy_remote_server (struct service *serv, Boolean *isHostname)
{
	CFStringRef remote_address_key = NULL;
	
	switch (serv->type) {
		case TYPE_PPP:
			remote_address_key = kSCPropNetPPPCommRemoteAddress;
			break;
		case TYPE_IPSEC:
			remote_address_key = kSCPropNetIPSecRemoteAddress;
			break;
		case TYPE_VPN:
			remote_address_key = kSCPropNetVPNRemoteAddress;
			break;
	}
	
	if (isHostname)
	{
		*isHostname = FALSE;
	}
	
	if (remote_address_key == NULL) {
		return NULL;
	}
	
	CFStringRef remote_address = CFDictionaryGetValue(serv->systemprefs, remote_address_key);
	if (isA_CFString(remote_address) && CFStringGetLength(remote_address) > 0) {
		CFCharacterSetRef slash_set = CFCharacterSetCreateWithCharactersInString(kCFAllocatorDefault, CFSTR("/"));
		CFCharacterSetRef colon_set = CFCharacterSetCreateWithCharactersInString(kCFAllocatorDefault, CFSTR(":"));
		CFRange slash_range;
		CFRange colon_range;
		
		CFRetain(remote_address);
		
		
		if (CFStringFindCharacterFromSet(remote_address,
		                                 slash_set,
		                                 CFRangeMake(0, CFStringGetLength(remote_address)),
		                                 0,
		                                 &slash_range))
		{
			CFURLRef url = CFURLCreateWithString(kCFAllocatorDefault, remote_address, NULL);
			if (url != NULL) {
				/* Check to see if there is a scheme on the URL. If not, add one. The hostname will not parse without a scheme */
				CFRange schemeRange = CFURLGetByteRangeForComponent(url, kCFURLComponentScheme, NULL);
				if (schemeRange.location == kCFNotFound) {
					CFRelease(url);
					CFStringRef address = CFStringCreateWithFormat(kCFAllocatorDefault, 0, CFSTR("https://%@"), remote_address);
					url = CFURLCreateWithString(kCFAllocatorDefault, address, NULL);
					CFRelease(address);
				}
				if (url != NULL) {
					CFRelease(remote_address);
					remote_address = CFURLCopyHostName(url);
					CFRelease(url);
				}
			}
		} else if (CFStringFindCharacterFromSet(remote_address,
		                                        colon_set,
		                                        CFRangeMake(0, CFStringGetLength(remote_address)),
		                                        0,
		                                        &colon_range))
		{
			CFStringRef address = CFStringCreateWithSubstring(kCFAllocatorDefault,
															  remote_address,
															  CFRangeMake(0, colon_range.location));
			CFRelease(remote_address);
			remote_address = address;
		}
		
		CFRelease(slash_set);
		CFRelease(colon_set);
		
		if (remote_address == NULL) {
			return NULL;
		}
		
		if (isHostname)
		{
			char *addr_cstr;
			CFIndex addr_cstr_len;
			struct sockaddr_storage sa_storage;
			memset(&sa_storage, 0, sizeof(sa_storage));
			addr_cstr_len = CFStringGetLength(remote_address); /* Assume that the address is ASCII */
			addr_cstr = CFAllocatorAllocate(kCFAllocatorDefault, addr_cstr_len + 1, 0);
			
			CFStringGetCString(remote_address, addr_cstr, addr_cstr_len, kCFStringEncodingASCII);
			
			if (inet_pton(AF_INET, addr_cstr, &((struct sockaddr_in *)&sa_storage)->sin_addr) == 1 ||
				inet_pton(AF_INET6, addr_cstr, &((struct sockaddr_in6 *)&sa_storage)->sin6_addr) == 1) {
				*isHostname = FALSE;
			} else {
				*isHostname = TRUE;
			}
			
			CFAllocatorDeallocate(kCFAllocatorDefault, addr_cstr);
		}
	}
	return remote_address;
}

void
scnc_log(int level, CFStringRef format, ...)
{
	if (ne_sm_bridge_is_logging_at_level(level)) {
		va_list args;
		va_start(args, format);
		if (!ne_sm_bridge_logv(level, format, args)) {
			SCLoggerVLog(NULL, level, format, args);
		}
		va_end(args);
	}
}
