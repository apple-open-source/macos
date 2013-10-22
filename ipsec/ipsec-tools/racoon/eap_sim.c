/*
 * Copyright (c) 2012 Apple Computer, Inc. All rights reserved.
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
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>
#include <syslog.h>
#include <sys/fcntl.h>
#include <CoreFoundation/CoreFoundation.h>
#include <CoreFoundation/CFBundle.h>
#include <mach/mach.h>
#include <EAP8021X/EAP.h>
#include <EAP8021X/EAPClientModule.h>
#include <EAP8021X/EAPClientProperties.h>
#if !TARGET_OS_EMBEDDED	// This file is not built for Embedded
#include <Security/SecKeychain.h>
#include <Security/SecKeychainSearch.h>
#include <Security/SecKeychainItem.h>
#include <Security/SecIdentity.h>
#endif /* TARGET_OS_EMBEDDED */
#include <SystemConfiguration/SCNetworkConnection.h>
#include "plog.h"
#include "eap.h"
#include "eap_sim.h"

/*---------------------------------------------------------------------------
 ** Internal routines 
 **---------------------------------------------------------------------------
 */

static CFBundleRef 	bundle = 0;		/* our bundle ref */
static char			eapsim_unique[17];

static EAPClientModuleRef  eapRef = NULL;
static EAPClientPluginData eapData;	
static CFMutableDictionaryRef eapProperties = NULL;
static CFDictionaryRef eapOptions = NULL;
static struct EAP_Packet	*eapSavePacket = NULL;

extern EAPClientPluginFuncRef eapsim_introspect(EAPClientPluginFuncName name);

/* ------------------------------------------------------------------------------------
 get the EAP dictionary from the options
 ------------------------------------------------------------------------------------ */ 
static void
EAPSIMGetOptions (void)
{	
	if (eapOptions)
		return;
	
	// no option, use empty dictionary
	if (!eapOptions)
		eapOptions = CFDictionaryCreate(0, 0, 0, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);	
}

/* ------------------------------------------------------------------------------------
 ------------------------------------------------------------------------------------ */ 
static int
EAPSIMLoad (void)
{	
    EAPClientModuleStatus status;
	
	if (eapRef)
		return EAP_NO_ERROR;
	
	status = EAPClientModuleAddBuiltinModule(eapsim_introspect);
	if (status != kEAPClientModuleStatusOK) {
		plog(ASL_LEVEL_INFO, "EAP-SIM: EAPClientAddBuiltinModule(eapsim) failed %d\n", status);
		return EAP_ERROR_GENERIC;
	}
	
	eapRef = EAPClientModuleLookup(kEAPTypeEAPSIM);
	if (eapRef == NULL) {
		plog(ASL_LEVEL_INFO, "EAP-SIM: EAPClientModuleLookup(eapsim) failed\n");
		return EAP_ERROR_GENERIC;
	}
	
	return EAP_NO_ERROR;
}

/* ------------------------------------------------------------------------------------
 ------------------------------------------------------------------------------------ */ 
int EAPSIMIdentity (char *identity, int maxlen)
{	
    CFStringRef			identRef = NULL;
	int					error;
	int					ret = EAP_ERROR_GENERIC;
	
	error = EAPSIMLoad();
	if (error)
		return error;
	
	EAPSIMGetOptions();
	if (eapOptions == NULL)
		return ret;
	
	identRef = EAPClientModulePluginUserName(eapRef, eapOptions);
    if (identRef) {
		if (CFStringGetCString(identRef, identity, maxlen, kCFStringEncodingUTF8))
			ret = EAP_NO_ERROR;
		CFRelease(identRef);
	}
	
	return ret;
}

/* ------------------------------------------------------------------------------------
 Init routine called by the EAP engine when it needs the module.
 Identity of the peer is known at this point.
 mode is 0 for client, 1 for server.
 cookie is the EAP engine context, to pass to subsequent calls to EAP.
 context is EAP module context, that will be passed to subsequent calls to the module
 ------------------------------------------------------------------------------------ */ 
int
EAPSIMInit (EAP_Input_t *eap_in, void **context, CFDictionaryRef eapOptions)
{	
	int error;
    EAPClientModuleStatus status;
	int ret = EAP_ERROR_GENERIC;

	error = EAPSIMLoad();
	if (error)
		return error;
	
	bundle = (CFBundleRef)eap_in->data;
    if (bundle)
		CFRetain(bundle);
	
	EAPSIMGetOptions();
	
	bzero(&eapData, sizeof(eapData));
	
    /* remaining fields are read-only: */
	*((bool *)&eapData.log_enabled) = 1;
	*((uint32_t *)&eapData.log_level) = LOG_NOTICE;
	*((uint32_t *)&eapData.mtu) = eap_in->mtu;
	*((uint32_t *)&eapData.generation) = 0;/* changed when user updates */

	arc4random_buf(eapsim_unique, sizeof(eapsim_unique) - 1);
	eapsim_unique[sizeof(eapsim_unique)-1] = 0;
	
    eapData.unique_id = eapsim_unique;  /* used for TLS session resumption??? */
	*((uint32_t *)&eapData.unique_id_length) = strlen(eapData.unique_id);

	if (eapOptions) {
        CFTypeRef value = CFDictionaryGetValue(eapOptions, kEAPPropertiesTypeEAPSIM);
        if (value && CFGetTypeID(value) == CFDictionaryGetTypeID()) {
            eapProperties = CFDictionaryCreateMutableCopy(kCFAllocatorDefault, 0, (CFDictionaryRef)value);
        } else {
            eapProperties = CFDictionaryCreateMutableCopy(kCFAllocatorDefault, 0, eapOptions);
        }
	} else
		eapProperties = CFDictionaryCreateMutable(0, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks); 
	if (eapProperties == NULL) {
		plog(ASL_LEVEL_ERR, "EAP-SIM: Cannot allocate memory\n");
		goto failed;
	}

	*((CFDictionaryRef *)&eapData.properties) = (CFDictionaryRef)eapProperties;

	status = EAPClientModulePluginInit(eapRef, &eapData, NULL, &error);
	if (status != kEAPClientStatusOK) {
		plog(ASL_LEVEL_ERR, "EAP-SIM: EAPClientPluginInit(eapsim) failed, error %d\n", status);
		goto failed;
	}
	
	eapSavePacket = NULL;
	
    return EAP_NO_ERROR;
	
failed:
	
    return ret;
}

/* ------------------------------------------------------------------------------------
 ------------------------------------------------------------------------------------ */ 
int EAPSIMDispose (void *context)
{
	
	EAPClientModulePluginFree(eapRef, &eapData);
	eapRef = 0;
	
	if (bundle) {
		CFRelease(bundle);
		bundle = 0;
	}
	
	if (eapOptions) {
		CFRelease(eapOptions);
		eapOptions = 0;
	}
	
	if (eapProperties) {
		CFRelease(eapProperties);
		eapProperties = 0;
	}
	
	if (eapSavePacket) {
		free(eapSavePacket);
		eapSavePacket = 0;
	}
	
    return EAP_NO_ERROR;
}

/* ------------------------------------------------------------------------------------
 ------------------------------------------------------------------------------------ */ 
int
EAPSIMProcess (void *context, EAP_Input_t *eap_in, EAP_Output_t *eap_out)
{
    struct EAP_Packet *pkt_in = NULL;
    struct EAP_Packet *pkt_out = NULL;
	EAPClientStatus status;
	EAPClientState	state;
	EAPClientDomainSpecificError error;
	int do_process = 0;
	
	// by default, ignore the message
	eap_out->action = EAP_ACTION_NONE;
	eap_out->data = 0;
	eap_out->data_len = 0;
	
	switch (eap_in->notification) {

		case EAP_NOTIFICATION_DATA_FROM_UI:
			plog(ASL_LEVEL_ERR, "unexpected EAP UI event");
			break;

		case EAP_NOTIFICATION_PACKET:
			
			pkt_in = (struct EAP_Packet *)eap_in->data;
			do_process = 1;
			break;
	}
	
	if (do_process) {
		
		state = EAPClientModulePluginProcess(eapRef, &eapData, (EAPPacketRef)pkt_in, (EAPPacketRef*)&pkt_out, &status, &error);
		switch(state) {
			case kEAPClientStateAuthenticating:
				switch (status) {
						
					case kEAPClientStatusOK:
						eap_out->data = pkt_out;
						eap_out->data_len = ntohs(pkt_out->len);
						eap_out->action = EAP_ACTION_SEND;
						break;
						
					case kEAPClientStatusUserInputRequired:
						plog(ASL_LEVEL_ERR, "unsupported EAP UI input");
					default:
						eap_out->action = EAP_ACTION_ACCESS_DENIED;			
				}
				break;
				
			case kEAPClientStateSuccess:
				eap_out->action = EAP_ACTION_ACCESS_GRANTED;
				break;
				
			default:
			case kEAPClientStateFailure:
				eap_out->action = EAP_ACTION_ACCESS_DENIED;
				break;
		}
	}
	
	if (eapSavePacket) {
		free(eapSavePacket);
		eapSavePacket = 0;
	}	
	
    return 0;
}

/* ------------------------------------------------------------------------------------
 ------------------------------------------------------------------------------------ */ 
int
EAPSIMFree (void *context, EAP_Output_t *eap_out)
{
	
	EAPClientModulePluginFreePacket(eapRef, &eapData, eap_out->data);
    return EAP_NO_ERROR;
}

/* ------------------------------------------------------------------------------------
 ------------------------------------------------------------------------------------ */ 
int
EAPSIMGetAttribute (void *context, EAP_Attribute_t *eap_attr)
{
	void *data = NULL;
	int len = 0;
	
	eap_attr->data = 0;
	
    switch (eap_attr->type) {
			
        case EAP_ATTRIBUTE_MPPE_SEND_KEY:
            data = EAPClientModulePluginSessionKey(eapRef, &eapData, &len);
            break;
        case EAP_ATTRIBUTE_MPPE_RECV_KEY:
            data = EAPClientModulePluginServerKey(eapRef, &eapData, &len);
            break;
    }
	
	if (data == NULL)
		return -1;
	
	eap_attr->data = data;
    if (len == 32)
        eap_attr->data_len = 64;
    else
        eap_attr->data_len = len;
    return 0;
}
