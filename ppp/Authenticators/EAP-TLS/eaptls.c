/*
 * Copyright (c) 2004 Apple Computer, Inc. All rights reserved.
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

#include "eaptls.h"
#include "eaptls_ui.h"

/*---------------------------------------------------------------------------
** Internal routines 
**---------------------------------------------------------------------------
*/

static CFBundleRef 	bundle = 0;		/* our bundle ref */
static int			initialized_UI = 0;	/* is UI ready */
static char			eaptls_unique[17];
static eaptls_ui_ctx ui_ctx;

static void (*log_debug) __P((char *, ...)) = 0;
static void (*log_error) __P((char *, ...)) = 0;

static EAPClientModuleRef  eapRef = NULL;
static EAPClientPluginData eapData;	
static CFMutableDictionaryRef eapProperties = NULL;
static CFDictionaryRef eapOptions = NULL;
static struct EAP_Packet	*eapSavePacket = NULL;
static int eap_in_ui = 0;


extern EAPClientPluginFuncRef
eaptls_introspect(EAPClientPluginFuncName name);


extern CFDictionaryRef		userOptions;	/* user options from pppd */


/* ------------------------------------------------------------------------------------
get the EAP dictionary from the options
------------------------------------------------------------------------------------ */ 
static void get_options ()
{	
	if (eapOptions)
		return;

	if (userOptions) {
		eapOptions = CFDictionaryGetValue(userOptions, CFSTR("EAP"));
		if (eapOptions)
			CFRetain(eapOptions);
	}
	
	// no option, use empty dictionary
	if (!eapOptions)
		eapOptions = CFDictionaryCreate(0, 0, 0, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks); 

}

/* ------------------------------------------------------------------------------------
------------------------------------------------------------------------------------ */ 
static int load_plugin ()
{	
    EAPClientModuleStatus status;
	
	if (eapRef)
		return EAP_NO_ERROR;
	
	status = EAPClientModuleAddBuiltinModule(eaptls_introspect);
	if (status != kEAPClientModuleStatusOK) {
		syslog(LOG_INFO, "EAP-TLS: EAPClientAddBuiltinModule(eaptls) failed %d\n", status);
		return EAP_ERROR_GENERIC;
	}
	
	eapRef = EAPClientModuleLookup(kEAPTypeTLS);
	if (eapRef == NULL) {
		syslog(LOG_INFO, "EAP-TLS: EAPClientModuleLookup(eaptls) failed\n");
		return EAP_ERROR_GENERIC;
	}
		
	return EAP_NO_ERROR;
}

/* ------------------------------------------------------------------------------------
------------------------------------------------------------------------------------ */ 
int Identity (char *identity, int maxlen)
{	
    CFStringRef			identRef = NULL;
	int					error;
	int					ret = EAP_ERROR_GENERIC;

	error = load_plugin();
	if (error)
		return error;
	
	get_options();
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
int Init (struct EAP_Input *eap_in, void **context)
{	
	int error;
    EAPClientModuleStatus status;
	int ret = EAP_ERROR_GENERIC;
    int fd;
	
	error = load_plugin();
	if (error)
		return error;

	bundle = (CFBundleRef)eap_in->data;    
	CFRetain(bundle);
		
	log_debug = eap_in->log_debug;
    log_error = eap_in->log_error;

	get_options();

	bzero(&eapData, sizeof(eapData));
	
    /* remaining fields are read-only: */
	*((bool *)&eapData.log_enabled) = 1;
	*((uint32_t *)&eapData.log_level) = LOG_ERR;
	*((uint32_t *)&eapData.mtu) = eap_in->mtu;
	*((uint32_t *)&eapData.generation) = 0;/* changed when user updates */

	fd = open("/dev/random", O_RDONLY);
    if (fd < 0) {
		if (log_error) 
			(log_error)("EAP-TLS: Cannot open /dev/random\n");
		goto failed;
	}
	
	read(fd, eaptls_unique, sizeof(eaptls_unique) - 1);
	eaptls_unique[sizeof(eaptls_unique)-1] = 0;
	close(fd);

    eapData.unique_id = eaptls_unique;  /* used for TLS session resumption */
	*((uint32_t *)&eapData.unique_id_length) = strlen(eapData.unique_id);

    eapData.username = (u_char*)eap_in->identity;
	*((uint32_t *)&eapData.username_length) = strlen((char*)eapData.username);

    eapData.password = 0; 	/* may be NULL */
	*((uint32_t *)&eapData.password_length) = 0;

	if (eapOptions)
		eapProperties = CFDictionaryCreateMutableCopy(0, 0, eapOptions);
	else 
		eapProperties = CFDictionaryCreateMutable(0, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks); 
	if (eapProperties == NULL) {
		if (log_error) 
			(log_error)("EAP-TLS: Cannot allocate memory\n");
		goto failed;
	}

	*((CFDictionaryRef *)&eapData.properties) = eapProperties;
	
	//CFDictionarySetValue(prop_dict, kEAPClientPropTLSVerifyServerCertificate, kCFBooleanFalse);
	
	status = EAPClientModulePluginInit(eapRef, &eapData, NULL, &error);
	if (status != kEAPClientStatusOK) {
		if (log_error)
			(log_error)("EAP-TLS: EAPClientPluginInit(eaptls) failed, error %d\n", status);
		goto failed;
	}

	eapSavePacket = NULL;
	eap_in_ui = 0;
    if (eaptls_ui_load(bundle, log_debug, log_error) == 0)
        initialized_UI = 1;

    return EAP_NO_ERROR;

failed:

    return ret;
}

/* ------------------------------------------------------------------------------------
------------------------------------------------------------------------------------ */ 
int Dispose(void *context)
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
	
	if (initialized_UI) {
		eaptls_ui_dispose();
	}
	
    return EAP_NO_ERROR;
}

/* ------------------------------------------------------------------------------------
------------------------------------------------------------------------------------ */ 
int Process(void *context, struct EAP_Input *eap_in, struct EAP_Output *eap_out) 
{
    struct EAP_Packet *pkt_in = NULL;
    struct EAP_Packet *pkt_out = NULL;
	EAPClientStatus status;
	EAPClientState	state;
	EAPClientDomainSpecificError error;
	eaptls_ui_ctx *ui_ctx_in;
	int do_process = 0;
	CFDictionaryRef	publish_prop;
	
	// by default, ignore the message
	eap_out->action = EAP_ACTION_NONE;
	eap_out->data = 0;
	eap_out->data_len = 0;

	switch (eap_in->notification) {
			
		case EAP_NOTIFICATION_DATA_FROM_UI:

			eap_in_ui = 0;
			
			ui_ctx_in = (eaptls_ui_ctx *)eap_in->data;
			switch (ui_ctx_in->response) {
				case RESPONSE_OK:
					
					// add the required property to the eap config dictionary
					publish_prop = EAPClientModulePluginPublishProperties(eapRef, &eapData);
					if (publish_prop) {
						 CFArrayRef     chain;
						 chain = CFDictionaryGetValue(publish_prop, kEAPClientPropTLSServerCertificateChain);
						 if (chain) {
							CFDictionarySetValue(eapProperties, kEAPClientPropTLSUserTrustProceedCertificateChain, chain);
						 }
						 CFRelease(publish_prop);
					}
					pkt_in = eapSavePacket;
					do_process = 1;
					break;
				case RESPONSE_CANCEL:
					eap_out->action = EAP_ACTION_CANCEL;
					break;
				case RESPONSE_ERROR:
					eap_out->action = EAP_ACTION_ACCESS_DENIED;
					break;
			}
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
						
						/* save last packet receive, will be process after user input obtained */
						if (eapSavePacket)
							free(eapSavePacket);
						eapSavePacket = malloc(pkt_in->len);
						if (!eapSavePacket) {								
							if (log_error)
								(log_error)("EAP-TLS: no memory to save packet\n");
							eap_out->action = EAP_ACTION_ACCESS_DENIED;			
						}
						bcopy(pkt_in, eapSavePacket, pkt_in->len);

						// waiting for UI notification, ignore the request
						if (eap_in_ui)
							break;
		
						eap_in_ui = 1;
						eap_out->action = EAP_ACTION_INVOKE_UI;
						bzero(&ui_ctx, sizeof(ui_ctx));
						ui_ctx.request = REQUEST_TRUST_EVAL;
						eap_out->data = &ui_ctx;
						eap_out->data_len = sizeof(ui_ctx);
						break;
						
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

	if (eapSavePacket && !eap_in_ui) {
		free(eapSavePacket);
		eapSavePacket = 0;
	}	

    return 0;
}

/* ------------------------------------------------------------------------------------
------------------------------------------------------------------------------------ */ 
int Free(void *context, struct EAP_Output *eap_out)
{

	EAPClientModulePluginFreePacket(eapRef, &eapData, eap_out->data);
    return EAP_NO_ERROR;
}

/* ------------------------------------------------------------------------------------
------------------------------------------------------------------------------------ */ 
int GetAttribute(void *context, struct EAP_Attribute *eap_attr) 
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
	eap_attr->data_len = len;
    return 0;
}

/* ------------------------------------------------------------------------------------
------------------------------------------------------------------------------------ */ 
int InteractiveUI(void *data_in, int data_in_len,
                    void **data_out, int *data_out_len)
{

    eaptls_ui_ctx	*ctx = (eaptls_ui_ctx *)data_in;
	CFDictionaryRef	publish_prop;
	
    ctx->response = RESPONSE_OK;
	
    if (!initialized_UI)
        return -1;
    
    switch (ctx->request)
    {
        case REQUEST_TRUST_EVAL:

			publish_prop = EAPClientModulePluginPublishProperties(eapRef, &eapData);
			if (publish_prop == NULL) {
				ctx->response = RESPONSE_ERROR;			
				break;
			}

            eaptls_ui_trusteval(publish_prop, data_in, data_in_len, data_out, data_out_len);
			
			CFRelease(publish_prop);

            break;
        default:
            break;
    }
    return EAP_NO_ERROR;
}
