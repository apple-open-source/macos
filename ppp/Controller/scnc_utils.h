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


#ifndef __PPP_UTILS__
#define __PPP_UTILS__


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
void AddNumber(CFMutableDictionaryRef dict, CFStringRef property, u_int32_t nunmber);
void AddString(CFMutableDictionaryRef dict, CFStringRef property, char *string); 
void AddNumberFromState(SCDynamicStoreRef store, CFStringRef serviceID, CFStringRef entity, CFStringRef property, CFMutableDictionaryRef dict); 
void AddStringFromState(SCDynamicStoreRef store, CFStringRef serviceID, CFStringRef entity, CFStringRef property, CFMutableDictionaryRef dict); 

Boolean isString (CFTypeRef obj);
Boolean isData (CFTypeRef obj);

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


#endif
