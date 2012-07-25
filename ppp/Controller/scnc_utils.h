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


#ifndef __SCNC_UTILS__
#define __SCNC_UTILS__

#include <SystemConfiguration/SCDPlugin.h>

int getStringFromEntity(SCDynamicStoreRef store, CFStringRef domain, CFStringRef serviceID, 
        CFStringRef entity, CFStringRef property, u_char *str, u_int16_t maxlen);
CFStringRef copyCFStringFromEntity(SCDynamicStoreRef store, CFStringRef domain, CFStringRef serviceID, 
        CFStringRef entity, CFStringRef property);
int getNumberFromEntity(SCDynamicStoreRef store, CFStringRef domain, CFStringRef serviceID, 
        CFStringRef entity, CFStringRef property, u_int32_t *outval);
int getAddressFromEntity(SCDynamicStoreRef store, CFStringRef domain, CFStringRef serviceID, 
        CFStringRef entity, CFStringRef property, u_int32_t *outval);
int getNumber(CFDictionaryRef service, CFStringRef property, u_int32_t *outval);
int getString(CFDictionaryRef service, CFStringRef property, u_char *str, u_int16_t maxlen);
CFDictionaryRef copyService(SCDynamicStoreRef store, CFStringRef domain, CFStringRef serviceID);
CFDictionaryRef copyEntity(SCDynamicStoreRef store, CFStringRef domain, CFStringRef serviceID, CFStringRef entity);
int existEntity(SCDynamicStoreRef store, CFStringRef domain, CFStringRef serviceID, CFStringRef entity);

u_int32_t CFStringAddrToLong(CFStringRef string);
void AddNumber(CFMutableDictionaryRef dict, CFStringRef property, u_int32_t number);
void AddNumber64(CFMutableDictionaryRef dict, CFStringRef property, u_int64_t number);
void AddString(CFMutableDictionaryRef dict, CFStringRef property, char *string); 
void AddNumberFromState(SCDynamicStoreRef store, CFStringRef serviceID, CFStringRef entity, CFStringRef property, CFMutableDictionaryRef dict); 
void AddStringFromState(SCDynamicStoreRef store, CFStringRef serviceID, CFStringRef entity, CFStringRef property, CFMutableDictionaryRef dict); 

Boolean isString (CFTypeRef obj);
Boolean isData (CFTypeRef obj);
Boolean isArray (CFTypeRef obj);

Boolean my_CFEqual(CFTypeRef obj1, CFTypeRef obj2);
void my_CFRelease(void *t);
CFTypeRef my_CFRetain(CFTypeRef obj);
void my_close(int fd);

int GetStrFromDict (CFDictionaryRef dict, CFStringRef property, char *outstr, int maxlen, char *defaultval);

void *my_Allocate(int size);
void my_Deallocate(void * addr, int size);

CFDataRef Serialize(CFPropertyListRef obj, void **data, u_int32_t *dataLen);
CFPropertyListRef Unserialize(void *data, u_int32_t dataLen);

CFStringRef parse_component(CFStringRef key, CFStringRef prefix);

int publish_dictnumentry(SCDynamicStoreRef store, CFStringRef serviceID, CFStringRef dict, CFStringRef entry, int val);
int unpublish_dictentry(SCDynamicStoreRef store, CFStringRef serviceID, CFStringRef dict, CFStringRef entry);
int publish_dictstrentry(SCDynamicStoreRef store, CFStringRef serviceID, CFStringRef dict, CFStringRef entry, char *str, int encoding);
int unpublish_dict(SCDynamicStoreRef store, CFStringRef serviceID, CFStringRef dict);

Boolean UpdatePasswordPrefs(CFStringRef serviceID, CFStringRef interfaceType, SCNetworkInterfacePasswordType passwordType, 
								   CFStringRef passwordEncryptionKey, CFStringRef passwordEncryptionValue, CFStringRef logTitle);

int cfstring_is_ip(CFStringRef str);

CFStringRef copyPrimaryService (SCDynamicStoreRef store);
Boolean copyGateway(SCDynamicStoreRef store, u_int8_t family, char *ifname, int ifnamesize, struct sockaddr *gateway, int gatewaysize);
Boolean hasGateway(SCDynamicStoreRef store, u_int8_t family);
const char *inet_sockaddr_to_p(struct sockaddr *addr, char *buf, int buflen);
int inet_p_to_sockaddr(char *buf, struct sockaddr *addr, int addrlen);
Boolean equal_address(struct sockaddr *addr1, struct sockaddr *addr2);

int set_ifmtu(char *ifname, int mtu);
int set_ifaddr(char *ifname, u_int32_t o, u_int32_t h, u_int32_t m);
int clear_ifaddr(char *ifname, u_int32_t o, u_int32_t h);
int set_ifaddr6 (char *ifname, struct in6_addr *addr, int prefix);
void in6_addr2net(struct in6_addr *addr, int prefix, struct in6_addr *net);
int clear_ifaddr6 (char *ifname, struct in6_addr *addr);
int publish_stateaddr(SCDynamicStoreRef store, CFStringRef serviceID, char *if_name, u_int32_t server, u_int32_t o, 
					  u_int32_t h, u_int32_t m, int isprimary);
int publish_proxies(SCDynamicStoreRef store, CFStringRef serviceID, int autodetect, CFStringRef server, int port, int bypasslocal, 
					CFStringRef exceptionlist, CFArrayRef supp_domains);
boolean_t set_host_gateway(int cmd, struct sockaddr *host, struct sockaddr *gateway, char *ifname, int isnet);
int route_gateway(int cmd, struct sockaddr *dest, struct sockaddr *mask, struct sockaddr *gateway, int use_gway_flag, int use_blackhole_flag);

int publish_dns(SCDynamicStoreRef store, CFStringRef serviceID, CFArrayRef dns, CFStringRef domain, CFArrayRef supp_domains);
void in6_len2mask(struct in6_addr *mask, int len);
void in6_maskaddr(struct in6_addr *addr, struct in6_addr *mask);

#define IP_FORMAT	"%d.%d.%d.%d"
#define IP_CH(ip)	((u_char *)(ip))
#define IP_LIST(ip)	IP_CH(ip)[0],IP_CH(ip)[1],IP_CH(ip)[2],IP_CH(ip)[3]


#define CREATESERVICESETUP(a)	SCDynamicStoreKeyCreateNetworkServiceEntity(0, \
                    kSCDynamicStoreDomainSetup, kSCCompAnyRegex, a)
#define CREATESERVICESTATE(a)	SCDynamicStoreKeyCreateNetworkServiceEntity(0, \
                    kSCDynamicStoreDomainState, kSCCompAnyRegex, a)
#define CREATEPREFIXSETUP()	SCDynamicStoreKeyCreate(0, CFSTR("%@/%@/%@/"), \
                    kSCDynamicStoreDomainSetup, kSCCompNetwork, kSCCompService)
#define CREATEPREFIXSTATE()	SCDynamicStoreKeyCreate(0, CFSTR("%@/%@/%@/"), \
                    kSCDynamicStoreDomainState, kSCCompNetwork, kSCCompService)
#define CREATEGLOBALSETUP(a)	SCDynamicStoreKeyCreateNetworkGlobalEntity(0, \
                    kSCDynamicStoreDomainSetup, a)
#define CREATEGLOBALSTATE(a)	SCDynamicStoreKeyCreateNetworkGlobalEntity(0, \
                    kSCDynamicStoreDomainState, a)


int create_tun_interface(char *name, int name_max_len, int *index, int flags, int ext_stats);
int setup_bootstrap_port();
int event_create_socket(void *ctxt, int *eventfd, CFSocketRef *eventref, CFSocketCallBack callout, Boolean anysubclass);

pid_t
SCNCPluginExecCommand (CFRunLoopRef runloop, SCDPluginExecCallBack callback, void *context, uid_t uid, gid_t gid,
					   const char *path, char * const argv[]);
pid_t
SCNCPluginExecCommand2 (CFRunLoopRef runloop, SCDPluginExecCallBack callback, void *context, uid_t uid, gid_t gid,
						const char *path,char * const argv[], SCDPluginExecSetup setup, void *setupContext);

CFDictionaryRef
collectEnvironmentVariables (SCDynamicStoreRef storeRef, CFStringRef serviceID);

void
applyEnvironmentVariables (CFDictionaryRef envVarDict);

/* Wcast-align fix - cast away alignment warning when buffer is aligned */
#define ALIGNED_CAST(type)	(type)(void *) 

// scnc_stop reasons
enum {
	SCNC_STOP_NONE = 0,
	SCNC_STOP_CTRL_STOP,
	SCNC_STOP_SYS_SLEEP,
	SCNC_STOP_USER_LOGOUT,
	SCNC_STOP_USER_SWITCH,
	SCNC_STOP_SOCK_DISCONNECT,
	SCNC_STOP_SOCK_DISCONNECT_NO_CLIENT,
	SCNC_STOP_PLUGIN_CHANGE,
	SCNC_STOP_APP_REMOVED,
	SCNC_STOP_USER_REQ,
	SCNC_STOP_USER_REQ_NO_CLIENT,
	SCNC_STOP_TERM_ALL,
	SCNC_STOP_SERV_DISPOSE,
};

const char *
scnc_get_reason_str(int scnc_reason);
const char *
ppp_error_to_string (u_int32_t native_ppp_error);
const char *
ppp_dev_error_to_string (u_int16_t subtype, u_int32_t native_dev_error);
const char *
ipsec_error_to_string (int status);
const char *
vpn_error_to_string (u_int32_t status);
void
cleanup_dynamicstore(void *serv);

#endif
