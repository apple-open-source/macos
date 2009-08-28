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

#include <fcntl.h>
#include <unistd.h>
#include <netdb.h>
#include <netdb_async.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include "sys/syslog.h"
#include <paths.h>
#include <net/if.h>
#include <net/if_media.h>
#include <net/ndrv.h>
#include <net/if_dl.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include <arpa/inet.h>
#include <net/route.h>
#include <mach/mach_time.h>
#include <mach/mach.h>
#include <mach/message.h>
#include <mach/boolean.h>
#include <mach/mach_error.h>
#include <servers/bootstrap.h>
#include <notify.h>
#include <sys/kern_control.h>
#include <sys/sys_domain.h>
#include <sys/kern_event.h>
#include <netinet/in_var.h>
#include <ifaddrs.h>
#include <sys/sysctl.h>

#include <CoreFoundation/CFUserNotification.h>
#include <SystemConfiguration/SystemConfiguration.h>
#include <SystemConfiguration/SCDPlugin.h>
#include <SystemConfiguration/SCPrivate.h>      // for SCLog()
#include <mach/task_special_ports.h>
#include "pppcontroller_types.h"
#include "pppcontroller.h"
#include <SystemConfiguration/SCValidation.h>
#include <SystemConfiguration/SCPreferences.h>
#include <sys/un.h>

#include "scnc_client.h"
#include "scnc_main.h"
#include "ipsec_manager.h"
#include "scnc_utils.h"
#include "cf_utils.h"
#include "PPPControllerPriv.h"

//#include <IPSec/IPSec.h>
//#include <IPSec/IPSecSchemaDefinitions.h>

#include "../Helpers/vpnd/RASSchemaDefinitions.h"
#include "../Helpers/vpnd/ipsec_utils.h"

#include "sessionTracer.h"



/* -----------------------------------------------------------------------------
globals
----------------------------------------------------------------------------- */

enum {
    do_nothing = 0,
    do_process,
    do_close,
    do_error
};

enum {
	dialog_default_type = 0,
	dialog_has_disconnect_type = 1,
	dialog_cert_fixme_type = 2	
};

#define FAR_FUTURE          (60.0 * 60.0 * 24.0 * 365.0 * 1000.0)
#define TIMEOUT_INITIAL_CONTACT	10 /* give 10 second to establish contact with server */
#define TIMEOUT_PHASE1			30 /* give 30 second to complete phase 1 */
#define TIMEOUT_PHASE2			15 /* give 15 second to complete phase 2 */
#define TIMEOUT_PHASE2_PING		1 /* sends ping every second until phase 2 starts */

#define MAX_PHASE2_PING			15 /* give up after 15 pings */

#define TIMEOUT_INTERFACE_CHANGE	20 /* give 20 second to address to come back */
#ifdef TARGET_EMBEDDED_OS
#define TIMEOUT_EDGE	2 /* give 2 second after edge is ready to propagate all network dns notification. */
#endif

#define XAUTH_FIRST_TIME		0x8000
#define XAUTH_NEED_USERNAME		0x0001
#define XAUTH_NEED_PASSWORD		0x0002
#define XAUTH_NEED_PASSCODE		0x0004
#define XAUTH_NEED_ANSWER		0x0008
#define XAUTH_NEED_NEXT_PIN		0x0010
#define XAUTH_NEED_XAUTH_INFO	0x0020
#define XAUTH_MUST_PROMPT		0x0040
#define XAUTH_DID_PROMPT		0x0080

struct isakmp_xauth {
	u_int16_t	type;
	CFStringRef str;
};


#ifdef TARGET_EMBEDDED_OS
// extra CFUserNotification keys
static CFStringRef const SBUserNotificationTextAutocapitalizationType = CFSTR("SBUserNotificationTextAutocapitalizationType");
static CFStringRef const SBUserNotificationTextAutocorrectionType = CFSTR("SBUserNotificationTextAutocorrectionType");
static CFStringRef const SBUserNotificationGroupsTextFields = CFSTR("SBUserNotificationGroupsTextFields");
#endif

/* -----------------------------------------------------------------------------
Forward declarations
----------------------------------------------------------------------------- */

static void ipsec_updatephase(struct service *serv, int phase);
static void display_notification(struct service *serv, CFStringRef message, int errnum, int dialog_type);
static void racoon_timer(CFRunLoopTimerRef timer, void *info);
static int racoon_restart(struct service *serv, struct sockaddr_in *address);
static int racoon_resolve(struct service *serv);
static void install_mode_config(struct service *serv);
static void uninstall_mode_config(struct service *serv);
static boolean_t set_host_gateway(int cmd, struct in_addr host, struct in_addr gateway, char *ifname, int isnet);
//static int route_gateway(int cmd, struct in_addr dest, struct in_addr mask, struct in_addr gateway, int use_gway_flag);
static int publish_dns(struct service *serv, CFArrayRef dns, CFStringRef domain, CFArrayRef supp_domains);
static int ask_user_xauth(struct service *serv, char* message);
static boolean_t checkpassword(struct service *serv, int must_prompt);
int readn(int ref, void *data, int len);
int writen(int ref, void *data, int len);

int racoon_send_cmd_start_ph2(int fd, u_int32_t address, CFDictionaryRef ipsec_dict);
int racoon_send_cmd_xauthinfo(int fd, u_int32_t address, struct isakmp_xauth *isakmp_array, int isakmp_nb);
int racoon_send_cmd_start_dpd(int fd, u_int32_t address) ;


/* ----------------------------------------------------------------------------- 
get the ipsec string corresponding to the ike error
----------------------------------------------------------------------------- */
char *ipsec_error_to_str(int ike_code)
{
	switch (ike_code) {
		case VPNCTL_NTYPE_INVALID_PAYLOAD_TYPE: return "Invalid payload type";
		case VPNCTL_NTYPE_DOI_NOT_SUPPORTED: return "DOI not supported";
		case VPNCTL_NTYPE_SITUATION_NOT_SUPPORTED: return "Situation not supported";
		case VPNCTL_NTYPE_INVALID_COOKIE: return "Invalid cookie";
		case VPNCTL_NTYPE_INVALID_MAJOR_VERSION: return "Invalid major version";
		case VPNCTL_NTYPE_INVALID_MINOR_VERSION: return "Invalid minor version";
		case VPNCTL_NTYPE_INVALID_EXCHANGE_TYPE: return "Invalid exchange type";
		case VPNCTL_NTYPE_INVALID_FLAGS: return "Invalid flags";
		case VPNCTL_NTYPE_INVALID_MESSAGE_ID: return "Invalid message id";
		case VPNCTL_NTYPE_INVALID_PROTOCOL_ID: return "Invalid protocol id";
		case VPNCTL_NTYPE_INVALID_SPI: return "Invalid SPI";
		case VPNCTL_NTYPE_INVALID_TRANSFORM_ID: return "Invalid transform id";
		case VPNCTL_NTYPE_ATTRIBUTES_NOT_SUPPORTED: return "Attributes not supported";
		case VPNCTL_NTYPE_NO_PROPOSAL_CHOSEN: return "No proposal chosen";
		case VPNCTL_NTYPE_BAD_PROPOSAL_SYNTAX: return "Bad proposal syntax";
		case VPNCTL_NTYPE_PAYLOAD_MALFORMED: return "Payload malformed";
		case VPNCTL_NTYPE_INVALID_KEY_INFORMATION: return "Invalid key information";
		case VPNCTL_NTYPE_INVALID_ID_INFORMATION: return "Invalid id information";
		case VPNCTL_NTYPE_INVALID_CERT_ENCODING: return "Invalid cert encoding";
		case VPNCTL_NTYPE_INVALID_CERTIFICATE: return "Invalid certificate";
		case VPNCTL_NTYPE_BAD_CERT_REQUEST_SYNTAX: return "Bad cert request syntax";
		case VPNCTL_NTYPE_INVALID_CERT_AUTHORITY: return "Invalid cert authority";
		case VPNCTL_NTYPE_INVALID_HASH_INFORMATION: return "Invalid hash information";
		case VPNCTL_NTYPE_AUTHENTICATION_FAILED: return "Authentication Failed";
		case VPNCTL_NTYPE_INVALID_SIGNATURE: return "Invalid signature";
		case VPNCTL_NTYPE_ADDRESS_NOTIFICATION: return "Address notification";
		case VPNCTL_NTYPE_NOTIFY_SA_LIFETIME: return "Notify SA lifetime";
		case VPNCTL_NTYPE_CERTIFICATE_UNAVAILABLE: return "Certificate unavailable";
		case VPNCTL_NTYPE_UNSUPPORTED_EXCHANGE_TYPE: return "Unsupported exchange type";
		case VPNCTL_NTYPE_UNEQUAL_PAYLOAD_LENGTHS: return "Unequal payload lengths";
		case VPNCTL_NTYPE_LOAD_BALANCE: return "Load balance";
		case VPNCTL_NTYPE_PEER_DEAD: return "Dead Peer";
		case VPNCTL_NTYPE_PH1_DELETE: return "Phase 1 Delete";
		case VPNCTL_NTYPE_IDLE_TIMEOUT: return "Idle Timeout";
		case VPNCTL_NTYPE_INTERNAL_ERROR: return "Internal error";
	}
	return "Unknown error";
}

/* ----------------------------------------------------------------------------- 
get the ipsec generic error corresponding to the ike error
----------------------------------------------------------------------------- */
u_int32_t ipsec_error_to_status(struct service *serv, int from, int ike_code)
{
	switch (ike_code) {
		case VPNCTL_NTYPE_INVALID_PAYLOAD_TYPE:
		case VPNCTL_NTYPE_DOI_NOT_SUPPORTED:
		case VPNCTL_NTYPE_SITUATION_NOT_SUPPORTED:
		case VPNCTL_NTYPE_INVALID_COOKIE:
		case VPNCTL_NTYPE_INVALID_MAJOR_VERSION:
		case VPNCTL_NTYPE_INVALID_MINOR_VERSION:
		case VPNCTL_NTYPE_INVALID_EXCHANGE_TYPE:
		case VPNCTL_NTYPE_INVALID_FLAGS:
		case VPNCTL_NTYPE_INVALID_MESSAGE_ID:
		case VPNCTL_NTYPE_INVALID_PROTOCOL_ID:
		case VPNCTL_NTYPE_INVALID_SPI:
		case VPNCTL_NTYPE_INVALID_TRANSFORM_ID:
		case VPNCTL_NTYPE_ATTRIBUTES_NOT_SUPPORTED:
		case VPNCTL_NTYPE_NO_PROPOSAL_CHOSEN:
		case VPNCTL_NTYPE_BAD_PROPOSAL_SYNTAX:
		case VPNCTL_NTYPE_PAYLOAD_MALFORMED:
		case VPNCTL_NTYPE_INVALID_KEY_INFORMATION:
		case VPNCTL_NTYPE_INVALID_ID_INFORMATION:
		case VPNCTL_NTYPE_INVALID_CERT_ENCODING:
		case VPNCTL_NTYPE_BAD_CERT_REQUEST_SYNTAX:
		case VPNCTL_NTYPE_UNSUPPORTED_EXCHANGE_TYPE:
		case VPNCTL_NTYPE_UNEQUAL_PAYLOAD_LENGTHS:
		case VPNCTL_NTYPE_INTERNAL_ERROR:
			return IPSEC_NEGOTIATION_ERROR;
			
		case VPNCTL_NTYPE_INVALID_HASH_INFORMATION: 
			return IPSEC_SHAREDSECRET_ERROR;

		case VPNCTL_NTYPE_CERTIFICATE_UNAVAILABLE:
			return IPSEC_NOCERTIFICATE_ERROR;
			
		case VPNCTL_NTYPE_INVALID_SIGNATURE:
		case VPNCTL_NTYPE_INVALID_CERTIFICATE:
		case VPNCTL_NTYPE_INVALID_CERT_AUTHORITY:
			return (from == FROM_REMOTE ? IPSEC_CLIENT_CERTIFICATE_ERROR : IPSEC_SERVER_CERTIFICATE_ERROR);

		case VPNCTL_NTYPE_AUTHENTICATION_FAILED:
			return IPSEC_XAUTH_ERROR;

		case VPNCTL_NTYPE_ADDRESS_NOTIFICATION:
		case VPNCTL_NTYPE_NOTIFY_SA_LIFETIME:
		case VPNCTL_NTYPE_LOAD_BALANCE:
		case VPNCTL_NTYPE_PEER_DEAD:
		case VPNCTL_NTYPE_IDLE_TIMEOUT:
			return IPSEC_NO_ERROR;

		case VPNCTL_NTYPE_PH1_DELETE:
			return ((serv->flags & FLAG_USECERTIFICATE) && (from == FROM_REMOTE) && (serv->u.ipsec.phase == IPSEC_PHASE1)) ?
				IPSEC_CLIENT_CERTIFICATE_ERROR : IPSEC_NO_ERROR;
			
	}
	return IPSEC_GENERIC_ERROR;
}

/* ----------------------------------------------------------------------------- 
get the ipsec string corresponding to the message type
----------------------------------------------------------------------------- */
char *ipsec_msgtype_to_str(int msgtype)
{
	switch (msgtype) {
		case VPNCTL_CMD_BIND: return "VPNCTL_CMD_BIND";
		case VPNCTL_CMD_UNBIND: return "VPNCTL_CMD_UNBIND";
		case VPNCTL_CMD_REDIRECT: return "VPNCTL_CMD_REDIRECT";
		case VPNCTL_CMD_PING: return "VPNCTL_CMD_PING";
		case VPNCTL_CMD_CONNECT: return "VPNCTL_CMD_CONNECT";
		case VPNCTL_CMD_DISCONNECT: return "VPNCTL_CMD_DISCONNECT";
		case VPNCTL_CMD_START_PH2: return "VPNCTL_CMD_START_PH2";
		case VPNCTL_CMD_XAUTH_INFO: return "VPNCTL_CMD_XAUTH_INFO";
		case VPNCTL_STATUS_IKE_FAILED: return "VPNCTL_STATUS_IKE_FAILED";
		case VPNCTL_STATUS_PH1_START_US: return "VPNCTL_STATUS_PH1_START_US";
		case VPNCTL_STATUS_PH1_START_PEER: return "VPNCTL_STATUS_PH1_START_PEER";
		case VPNCTL_STATUS_PH1_ESTABLISHED: return "VPNCTL_STATUS_PH1_ESTABLISHED";
		case VPNCTL_STATUS_PH2_START: return "VPNCTL_STATUS_PH2_START";
		case VPNCTL_STATUS_PH2_ESTABLISHED: return "VPNCTL_STATUS_PH2_ESTABLISHED";
		case VPNCTL_STATUS_NEED_AUTHINFO: return "VPNCTL_STATUS_NEED_AUTHINFO";
		case VPNCTL_STATUS_NEED_REAUTHINFO: return "VPNCTL_STATUS_NEED_REAUTHINFO";
	}
	return "Unknown message type";
}

/* ----------------------------------------------------------------------------- 
get the ipsec string corresponding to the message type
----------------------------------------------------------------------------- */
char *ipsec_xauthtype_to_str(int msgtype)
{
	switch (msgtype) {
		case XAUTH_TYPE: return "XAUTH_TYPE";
		case XAUTH_USER_NAME: return "XAUTH_USER_NAME";
		case XAUTH_USER_PASSWORD: return "XAUTH_USER_PASSWORD";
		case XAUTH_PASSCODE: return "XAUTH_PASSCODE";
		case XAUTH_MESSAGE: return "XAUTH_MESSAGE";
		case XAUTH_CHALLENGE: return "XAUTH_CHALLENGE";
		case XAUTH_DOMAIN: return "XAUTH_DOMAIN";
		case XAUTH_STATUS: return "XAUTH_STATUS";
		case XAUTH_NEXT_PIN: return "XAUTH_NEXT_PIN";
		case XAUTH_ANSWER: return "XAUTH_ANSWER";
		case XAUTH_VENDOR: return "XAUTH_VENDOR";
	}
	return "XAUTH_TYPE unknown type";
}

/* ----------------------------------------------------------------------------- 
get the ipsec string corresponding to the message type
----------------------------------------------------------------------------- */
char *ipsec_modecfgtype_to_str(int msgtype)
{
	switch (msgtype) {

		case INTERNAL_IP4_ADDRESS: return "INTERNAL_IP4_ADDRESS";
		case INTERNAL_IP4_NETMASK: return "INTERNAL_IP4_NETMASK";
		case INTERNAL_IP4_DNS: return "INTERNAL_IP4_DNS";
		case INTERNAL_IP4_NBNS: return "INTERNAL_IP4_NBNS";
		case INTERNAL_ADDRESS_EXPIRY: return "INTERNAL_ADDRESS_EXPIRY";
		case INTERNAL_IP4_DHCP: return "INTERNAL_IP4_DHCP";
		case APPLICATION_VERSION: return "APPLICATION_VERSION";
		case INTERNAL_IP6_ADDRESS: return "INTERNAL_IP6_ADDRESS";
		case INTERNAL_IP6_NETMASK: return "INTERNAL_IP6_NETMASK";
		case INTERNAL_IP6_DNS: return "INTERNAL_IP6_DNS";
		case INTERNAL_IP6_NBNS: return "INTERNAL_IP6_NBNS";
		case INTERNAL_IP6_DHCP: return "INTERNAL_IP6_DHCP";
		case INTERNAL_IP4_SUBNET: return "INTERNAL_IP4_SUBNET";
		case SUPPORTED_ATTRIBUTES: return "SUPPORTED_ATTRIBUTES";
		case INTERNAL_IP6_SUBNET: return "INTERNAL_IP6_SUBNET";
		
	}
	return "MODECFG_TYPE unknown type";
}


/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
u_int16_t ipsec_subtype(CFStringRef subtypeRef) 
{

	return 0;
}

/* -----------------------------------------------------------------------------
an interface structure needs to be created
----------------------------------------------------------------------------- */
int ipsec_new_service(struct service *serv)
{

    serv->u.ipsec.phase = IPSEC_IDLE;
    serv->u.ipsec.controlfd = -1;
	serv->u.ipsec.kernctl_sock = -1;
    serv->u.ipsec.eventfd = -1;
    return 0;
}

/* -----------------------------------------------------------------------------
an interface is come down, dispose the structure
----------------------------------------------------------------------------- */
int ipsec_dispose_service(struct service *serv)
{

    if (serv->u.ipsec.phase != IPSEC_IDLE)
        return 1;
    
	my_CFRelease(&serv->systemprefs);
#ifdef TARGET_EMBEDDED_OS
	my_CFRelease(&serv->profileIdentifier);
#endif
	
    return 0;
}

/* -----------------------------------------------------------------------------
changed for this service occured in configd cache
----------------------------------------------------------------------------- */
int ipsec_setup_service(struct service *serv)
{
	
    u_int32_t 		lval;
	
	/* get some general setting flags first */
	serv->flags &= ~(
					 FLAG_SETUP_ONTRAFFIC +
					 FLAG_SETUP_DISCONNECTONLOGOUT +
					 FLAG_SETUP_DISCONNECTONSLEEP +
					 FLAG_SETUP_PREVENTIDLESLEEP +
					 FLAG_SETUP_DISCONNECTONFASTUSERSWITCH +
					 FLAG_SETUP_ONDEMAND);
	
	serv->flags |= (    
					FLAG_ALERTERRORS +
					FLAG_ALERTPASSWORDS);
	
	my_CFRelease(&serv->systemprefs);
    serv->systemprefs = copyEntity(gDynamicStore, kSCDynamicStoreDomainSetup, serv->serviceID, kSCEntNetIPSec);
	if (serv->systemprefs == NULL) {
		SCLog(TRUE, LOG_ERR, CFSTR("IPSec Controller: Cannot copy IPSec dictionary from setup"));
		ipsec_stop(serv, 0);
		return -1;
	}

#ifdef TARGET_EMBEDDED_OS
	my_CFRelease(&serv->profileIdentifier);
	CFDictionaryRef payloadRoot = copyEntity(gDynamicStore, kSCDynamicStoreDomainSetup, serv->serviceID, CFSTR("com.apple.payload/PayloadRoot"));
	if (payloadRoot) {
		serv->profileIdentifier = CFDictionaryGetValue(payloadRoot, CFSTR("PayloadIdentifier"));
		serv->profileIdentifier = isA_CFString(serv->profileIdentifier);
		if (serv->profileIdentifier)
			CFRetain(serv->profileIdentifier);
		CFRelease(payloadRoot);
	}
#endif
	
	lval = 0;
	getNumber(serv->systemprefs, kSCPropNetIPSecOnDemandEnabled, &lval);
	if (lval) serv->flags |= FLAG_SETUP_ONDEMAND;
	
	lval = 0;
	getNumber(serv->systemprefs, CFSTR("DisconnectOnLogout"), &lval);
	if (lval) serv->flags |= FLAG_SETUP_DISCONNECTONLOGOUT;
	
	lval = 0;
	getNumber(serv->systemprefs, CFSTR("DisconnectOnSleep"), &lval);
	if (lval) serv->flags |= FLAG_SETUP_DISCONNECTONSLEEP;
	
	lval = 0;
	getNumber(serv->systemprefs, CFSTR("PreventIdleSleep"), &lval);
	if (lval) serv->flags |= FLAG_SETUP_PREVENTIDLESLEEP;
	
	/* if the DisconnectOnFastUserSwitch key does not exist, use DisconnectOnLogout */
	lval = (serv->flags & FLAG_SETUP_DISCONNECTONLOGOUT);
	getNumber(serv->systemprefs, CFSTR("DisconnectOnFastUserSwitch"), &lval);
	if (lval) serv->flags |= FLAG_SETUP_DISCONNECTONFASTUSERSWITCH;
	
	lval = 1;
	getNumber(serv->systemprefs, CFSTR("AlertEnable"), &lval);
	if (!lval) serv->flags &= ~(FLAG_ALERTERRORS + FLAG_ALERTPASSWORDS);
	
	return 0;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
static void merge_ipsec_dict(const void *key, const void *value, void *context)
{
	/* ignore some of the keys */

	/* remote address was resolved elsewhere */
	if (CFStringCompare(key, kRASPropIPSecRemoteAddress, 0) == kCFCompareEqualTo)
		return;

	/* merge everything else */
	CFDictionarySetValue((CFMutableDictionaryRef)context, key, value);
}


#ifdef TARGET_EMBEDDED_OS
/* -----------------------------------------------------------------------------
 ----------------------------------------------------------------------------- */
int start_profile_janitor(struct service *serv)
{
	CFStringRef 	resourceDir = NULL;
	CFURLRef 		resourceURL = NULL, absoluteURL = NULL;
	char			thepath[MAXPATHLEN], payloadIdentifierStr[256];
	int				ret = 0;
	char 			*cmdarg[3];

	if (!CFStringGetCString(serv->profileIdentifier, payloadIdentifierStr, sizeof(payloadIdentifierStr), kCFStringEncodingUTF8))
		goto done;

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
	
	strlcat(thepath, "sbslauncher", sizeof(thepath));
	
	cmdarg[0] = "sbslauncher";
	cmdarg[1] = payloadIdentifierStr;
	cmdarg[2] = NULL;

	if (_SCDPluginExecCommand(NULL, 0, 0 /*pid*/, 0/*gid*/, thepath, cmdarg) == 0)
		goto done;

	ret = 1;

done:

	if (ret)
		SCLog(TRUE, LOG_NOTICE, CFSTR("IPSec Controller: Started Profile Janitor for profile '%@'"), serv->profileIdentifier);
	else
		SCLog(TRUE, LOG_ERR, CFSTR("IPSec Controller: Failed to start Profile Janitor for profile '%@'"), serv->profileIdentifier);

	if (resourceDir)
		CFRelease(resourceDir);
	if (absoluteURL)
		CFRelease(absoluteURL);
	if (resourceURL)
		CFRelease(resourceURL);

	return ret;
}
#endif

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
void ipsec_user_notification_callback(struct service *serv, CFUserNotificationRef userNotification, CFOptionFlags responseFlags)
{
	
	if ((responseFlags & 3) != kCFUserNotificationDefaultResponse) {
		switch (serv->u.ipsec.phase) {
			case IPSEC_IDLE:
				if (serv->u.ipsec.laststatus == IPSEC_CLIENT_CERTIFICATE_ERROR) {
#ifdef TARGET_EMBEDDED_OS
					start_profile_janitor(serv);
#endif
				}
				return;
			default:
				// user cancelled
				ipsec_stop(serv, 0);
				return;
				
		}
	}
 
#ifdef TARGET_EMBEDDED_OS
	if (serv->u.ipsec.phase != IPSEC_PHASE1AUTH)
		return;
#else
	if (serv->u.ipsec.phase != IPSEC_PHASE1AUTH &&
	    serv->u.ipsec.phase != IPSEC_RUNNING)
		return;
#endif
 
	struct isakmp_xauth		isakmp_array[2]; /* max 2 attributes currently supported simultaneously */
	int						isakmp_nb = 0;

	if (serv->u.ipsec.xauth_flags & XAUTH_NEED_ANSWER) {
		isakmp_array[isakmp_nb].type = XAUTH_ANSWER;
		isakmp_array[isakmp_nb].str = CFUserNotificationGetResponseValue(userNotification, kCFUserNotificationTextFieldValuesKey, isakmp_nb);
		isakmp_nb++;
	}
	else if (serv->u.ipsec.xauth_flags & XAUTH_NEED_NEXT_PIN) {
		isakmp_array[isakmp_nb].type = XAUTH_NEXT_PIN;
		isakmp_array[isakmp_nb].str = CFUserNotificationGetResponseValue(userNotification, kCFUserNotificationTextFieldValuesKey, isakmp_nb);
		isakmp_nb++;
	}
	else {
		if (serv->u.ipsec.xauth_flags & XAUTH_NEED_USERNAME) {
			isakmp_array[isakmp_nb].type = XAUTH_USER_NAME;
			isakmp_array[isakmp_nb].str = CFUserNotificationGetResponseValue(userNotification, kCFUserNotificationTextFieldValuesKey, isakmp_nb);
			isakmp_nb++;
		}
		
		if (serv->u.ipsec.xauth_flags & XAUTH_NEED_PASSCODE) {
			isakmp_array[isakmp_nb].type = XAUTH_PASSCODE;
			isakmp_array[isakmp_nb].str = CFUserNotificationGetResponseValue(userNotification, kCFUserNotificationTextFieldValuesKey, isakmp_nb);
			isakmp_nb++;
		}
		else if (serv->u.ipsec.xauth_flags & XAUTH_NEED_PASSWORD) {
			isakmp_array[isakmp_nb].type = XAUTH_USER_PASSWORD;
			isakmp_array[isakmp_nb].str = CFUserNotificationGetResponseValue(userNotification, kCFUserNotificationTextFieldValuesKey, isakmp_nb);
			isakmp_nb++;
		}
	}
	
	// note: isakmp_nb can be 0. for exmple, sometime the server just pushes a message information, and we just need to acknowledge
#ifdef TARGET_EMBEDDED_OS
	CFRunLoopTimerSetNextFireDate(serv->u.ipsec.timerref, CFAbsoluteTimeGetCurrent() + TIMEOUT_PHASE1);
	ipsec_updatephase(serv, IPSEC_PHASE1);
#else
	if (serv->u.ipsec.phase == IPSEC_PHASE1AUTH) {
        CFRunLoopTimerSetNextFireDate(serv->u.ipsec.timerref, CFAbsoluteTimeGetCurrent() + TIMEOUT_PHASE1);
		ipsec_updatephase(serv, IPSEC_PHASE1);
	}
#endif /* TARGET_EMBEDDED_OS */
	racoon_send_cmd_xauthinfo(serv->u.ipsec.controlfd, serv->u.ipsec.peer_address.sin_addr.s_addr, isakmp_array, isakmp_nb);

}

/* -----------------------------------------------------------------------------
 ----------------------------------------------------------------------------- */
static boolean_t checkpassword(struct service *serv, int must_prompt)
{
	int pref_must_prompt;
	Boolean ok;
	Boolean didrestart = FALSE;
	Boolean needauthinfo = FALSE;
	
	pref_must_prompt = (serv->u.ipsec.xauth_flags & XAUTH_MUST_PROMPT) ? 1 : 0;
	if (must_prompt != pref_must_prompt) {
		needauthinfo = (serv->u.ipsec.xauth_flags & XAUTH_NEED_XAUTH_INFO) ? TRUE : FALSE;
		ok = UpdatePasswordPrefs(serv->serviceID, serv->typeRef, kSCNetworkInterfacePasswordTypeIPSecXAuth, 
								 kSCPropNetIPSecXAuthPasswordEncryption, must_prompt ? kSCValNetIPSecXAuthPasswordEncryptionPrompt : NULL,
								CFSTR("IPSec Controller"));
		if (ok) {
			// update current config
			if (must_prompt) {
				serv->u.ipsec.xauth_flags |= XAUTH_MUST_PROMPT;
				CFDictionarySetValue(serv->u.ipsec.config, kSCPropNetIPSecXAuthPasswordEncryption, kSCValNetIPSecXAuthPasswordEncryptionPrompt);
			} else {
				serv->u.ipsec.xauth_flags &= ~XAUTH_MUST_PROMPT;
				CFDictionaryRemoveValue( serv->u.ipsec.config, kSCPropNetIPSecXAuthPasswordEncryption);
			}
		}
		
		if ( needauthinfo && (serv->u.ipsec.xauth_flags & XAUTH_MUST_PROMPT) 
			&& !(serv->u.ipsec.xauth_flags & XAUTH_DID_PROMPT)) {
			/* policy changed from "ok to save" to "do not save"
			 if the connection was established using a saved password, 
			 then disconnect, and reconnect to make sure the user is prompted */

			racoon_restart(serv, &serv->u.ipsec.peer_address);
			didrestart = TRUE;
		}
	}
	return didrestart;
}

/* -----------------------------------------------------------------------------
 ----------------------------------------------------------------------------- */
static CFStringRef copy_decrypted_password(struct service *serv)
{
#ifndef TARGET_EMBEDDED_OS
	SCNetworkInterfaceRef	interface = NULL;
	SCNetworkServiceRef		service = NULL;
	SCPreferencesRef		prefs = NULL;
#endif
	CFStringRef	decryptedpasswd = NULL;
	CFStringRef passwdencryption = NULL;
	
	passwdencryption = CFDictionaryGetValue(serv->u.ipsec.config, kSCPropNetIPSecXAuthPasswordEncryption);
	passwdencryption = isA_CFString(passwdencryption);
	if (passwdencryption) {
		if (CFStringCompare(passwdencryption, kSCValNetIPSecXAuthPasswordEncryptionKeychain, 0) == kCFCompareEqualTo) {
#ifdef TARGET_EMBEDDED_OS
			// TO DO:
			// currently, password is given inline in SCNetworkConnectionStart
			// needs t implement keychain support later
#else
			prefs = SCPreferencesCreate(NULL, CFSTR("CopyPassword"), NULL);
			if (prefs == NULL) {
				SCLog(TRUE, LOG_ERR, CFSTR("IPSec Controller: SCPreferencesCreate fails"));
				goto done;
			}
			// get the service
			service = SCNetworkServiceCopy(prefs, serv->serviceID);
			if (service == NULL) {
				SCLog(TRUE, LOG_ERR, CFSTR("IPSec Controller: SCNetworkServiceCopy fails"));
				goto done;
			}
			// get the interface associated with the service
			interface = SCNetworkServiceGetInterface(service);
			if ((interface == NULL) || !CFEqual(SCNetworkInterfaceGetInterfaceType(interface), kSCNetworkInterfaceTypeIPSec)) {
				SCLog(TRUE, LOG_ERR, CFSTR("IPSec Controller: interface not IPSec"));
				goto done;
			}
			CFDataRef passworddata = SCNetworkInterfaceCopyPassword( interface, kSCNetworkInterfacePasswordTypeIPSecXAuth);
			if (passworddata) {
				CFIndex passworddatalen = CFDataGetLength(passworddata);
				if ((decryptedpasswd = CFStringCreateWithBytes(NULL, CFDataGetBytePtr(passworddata), passworddatalen, kCFStringEncodingUTF8, FALSE)))
					SCLog(TRUE, LOG_INFO, CFSTR("IPSec Controller: decrypted password %s"),  decryptedpasswd ? (CFStringGetCStringPtr(decryptedpasswd,kCFStringEncodingMacRoman)) : "NULL");
				else
					SCLog(TRUE, LOG_ERR, CFSTR("IPSec Controller: cannot decrypt password"));
				CFRelease(passworddata);
			}
#endif
		}
	}
	else {
		decryptedpasswd = CFDictionaryGetValue(serv->u.ipsec.config, kSCPropNetIPSecXAuthPassword);
		decryptedpasswd = isA_CFString(decryptedpasswd);
		if (decryptedpasswd)
			CFRetain(decryptedpasswd);
	}
	
#ifndef TARGET_EMBEDDED_OS
done:
	if (prefs != NULL) {
        CFRelease(prefs);
	}
	if (service != NULL) {
		CFRelease(service);
	}
#endif					
	return (CFStringRef)decryptedpasswd;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
static
int ask_user_xauth(struct service *serv, char* message) 
{
    CFStringRef 	msg = NULL;
    CFMutableDictionaryRef 	dict = NULL;
    SInt32 			err;
    CFOptionFlags 		flags;
    CFMutableArrayRef 		array;
	CFIndex  secure_field = 0;
	int		ret = 0;
#ifdef TARGET_EMBEDDED_OS
	int		nbfields = 0;
#endif

    if ((serv->flags & FLAG_ALERTPASSWORDS) == 0)
        return -1;

	/* first, remove any pending notification, if any */
	if (serv->userNotificationRef) {
		CFUserNotificationCancel(serv->userNotificationRef);
		CFRunLoopRemoveSource(CFRunLoopGetCurrent(), serv->userNotificationRLS, kCFRunLoopDefaultMode);
		my_CFRelease(&serv->userNotificationRef);
		my_CFRelease(&serv->userNotificationRLS);			
	}

	if (message)
		msg = CFStringCreateWithFormat(0, 0, CFSTR("%s"), message);
	else 
		msg = CFStringCreateWithFormat(0, 0, CFSTR("Enter your user authentication"));

    if (!msg && !CFStringGetLength(msg))
		goto fail;

    dict = CFDictionaryCreateMutable(NULL, 0, &kCFCopyStringDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    if (!dict)
		goto fail;
		
	if (gIconURLRef)
		CFDictionaryAddValue(dict, kCFUserNotificationIconURLKey, gIconURLRef);
	if (gBundleURLRef)
		CFDictionaryAddValue(dict, kCFUserNotificationLocalizationURLKey, gBundleURLRef);

	CFDictionaryAddValue(dict, kCFUserNotificationAlertMessageKey, msg);
	CFDictionaryAddValue(dict, kCFUserNotificationAlertHeaderKey, CFSTR("VPN Connection"));
	CFDictionaryAddValue(dict, kCFUserNotificationAlternateButtonTitleKey, CFSTR("Cancel"));
	
	array = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);  
	if (array) {
		if (serv->u.ipsec.xauth_flags & XAUTH_NEED_ANSWER) {
			CFArrayAppendValue(array, CFSTR("Answer"));
		}
		else if (serv->u.ipsec.xauth_flags & XAUTH_NEED_NEXT_PIN) {
			CFArrayAppendValue(array, CFSTR("Next PIN"));
			secure_field = 1;
		}
		else {
			if (serv->u.ipsec.xauth_flags & XAUTH_NEED_USERNAME) {
				CFArrayAppendValue(array, CFSTR("Account"));
			}		
			
			if (serv->u.ipsec.xauth_flags & XAUTH_NEED_PASSCODE) {
				CFArrayAppendValue(array, CFSTR("Passcode"));
				secure_field = (serv->u.ipsec.xauth_flags & XAUTH_NEED_USERNAME) ? 2 : 1;
			}
			else if (serv->u.ipsec.xauth_flags & XAUTH_NEED_PASSWORD) {
				CFArrayAppendValue(array, CFSTR("Password"));
				secure_field = (serv->u.ipsec.xauth_flags & XAUTH_NEED_USERNAME) ? 2 : 1;
			}
		}
		
#ifdef TARGET_EMBEDDED_OS
		nbfields = CFArrayGetCount(array);
#endif
		CFDictionaryAddValue(dict, kCFUserNotificationTextFieldTitlesKey, array);
		CFRelease(array);
	}
	
	if (serv->u.ipsec.xauth_flags & XAUTH_NEED_USERNAME) {
		CFStringRef username = CFDictionaryGetValue(serv->u.ipsec.config, kRASPropIPSecXAuthName);
		if (isString(username)) {
			array = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);  
			if (array) {
				CFArrayAppendValue(array, username);
				
				// for some reason, CFUsernotification wants to have both values present in the array, in order to display the first one.
				if (serv->u.ipsec.xauth_flags & (XAUTH_NEED_PASSWORD | XAUTH_NEED_PASSCODE)) {
					CFArrayAppendValue(array, CFSTR(""));
				}

				CFDictionaryAddValue(dict, kCFUserNotificationTextFieldValuesKey, array);
				CFRelease(array);
			}
		}
	}

#ifdef TARGET_EMBEDDED_OS
	if (nbfields > 0) {
		CFMutableArrayRef autoCapsTypes = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
		CFMutableArrayRef autoCorrectionTypes = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
		int i, zero = 0, one = 1;
		CFNumberRef zeroRef = CFNumberCreate(NULL, kCFNumberIntType, &zero);
		CFNumberRef oneRef = CFNumberCreate(NULL, kCFNumberIntType, &one);
		
		if (autoCapsTypes && autoCorrectionTypes && zeroRef && oneRef) {
			for(i = 0; i < nbfields; i++) {
				// no auto caps or autocorrection for any of our fields
				CFArrayAppendValue(autoCapsTypes, zeroRef);
				CFArrayAppendValue(autoCorrectionTypes, oneRef);
			}
			CFDictionarySetValue(dict, SBUserNotificationTextAutocapitalizationType, autoCapsTypes);
			CFDictionarySetValue(dict, SBUserNotificationTextAutocorrectionType, autoCorrectionTypes);
		}
		my_CFRelease(&autoCapsTypes);
		my_CFRelease(&autoCorrectionTypes);
		my_CFRelease(&zeroRef);
		my_CFRelease(&oneRef);

		// make CFUN prettier
		CFDictionarySetValue(dict, SBUserNotificationGroupsTextFields, kCFBooleanTrue);
	}
#endif

	flags = 0;
	if (secure_field)
		flags = CFUserNotificationSecureTextField(secure_field - 1);

	serv->userNotificationRef = CFUserNotificationCreate(NULL, 150 /* 2 min 30 sec */, flags, &err, dict);
	if (!serv->userNotificationRef)
		goto fail;

	serv->userNotificationRLS = CFUserNotificationCreateRunLoopSource(NULL, serv->userNotificationRef, 
												user_notification_callback, 0);
	if (!serv->userNotificationRLS) {
		my_CFRelease(&serv->userNotificationRef);
		goto fail;
	}
	CFRunLoopAddSource(CFRunLoopGetCurrent(), serv->userNotificationRLS, kCFRunLoopDefaultMode);
		
done:
    my_CFRelease(&dict);
    my_CFRelease(&msg);
	return ret;

fail:
	ret = -1;
	goto done;
}


/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
static int process_xauth_need_info(struct service *serv)
{
	char *message = NULL;
	struct vpnctl_cmd_xauth_info *cmd_xauth_info;

	cmd_xauth_info = (struct vpnctl_cmd_xauth_info *)serv->u.ipsec.msg;

	char *xauth_data = (char*)serv->u.ipsec.msg + sizeof(struct vpnctl_cmd_xauth_info); 
	int xauth_data_len = ntohs(serv->u.ipsec.msghdr.len) - (sizeof(struct vpnctl_cmd_xauth_info) -  sizeof(struct vpnctl_hdr));

	int tlen = xauth_data_len;
	struct isakmp_data *attr;
	char	*dataptr = xauth_data;
	
	serv->u.ipsec.xauth_flags &= ~(XAUTH_NEED_USERNAME | XAUTH_NEED_PASSWORD | XAUTH_NEED_PASSCODE | XAUTH_NEED_ANSWER);
	serv->u.ipsec.xauth_flags |= XAUTH_NEED_XAUTH_INFO;
	
	while (tlen > 0)
	{
		int tlv;
		u_int16_t type;
		
		attr = (struct isakmp_data *)dataptr;
		type = ntohs(attr->type) & 0x7FFF;
		tlv = (type ==  ntohs(attr->type));
		
		switch (type)
		{
			case XAUTH_TYPE:
				switch (ntohs(attr->lorv)) {
					case XAUTH_TYPE_GENERIC:
						break;
					default:
						SCLog(TRUE, LOG_ERR, CFSTR("IPSec Controller: Received unsupported Xauth Type (value %d)"), ntohs(attr->lorv));
						goto fail;
				}
				break;
			case XAUTH_USER_NAME:
				serv->u.ipsec.xauth_flags |= XAUTH_NEED_USERNAME;
				serv->u.ipsec.xauth_flags &= ~(XAUTH_NEED_NEXT_PIN + XAUTH_NEED_ANSWER);
				break;
			case XAUTH_USER_PASSWORD:
				serv->u.ipsec.xauth_flags |= XAUTH_NEED_PASSWORD;
				serv->u.ipsec.xauth_flags &= ~(XAUTH_NEED_PASSCODE + XAUTH_NEED_NEXT_PIN + XAUTH_NEED_ANSWER);
				break;
			case XAUTH_PASSCODE:
				serv->u.ipsec.xauth_flags |= XAUTH_NEED_PASSCODE;
				serv->u.ipsec.xauth_flags &= ~(XAUTH_NEED_PASSWORD + XAUTH_NEED_NEXT_PIN + XAUTH_NEED_ANSWER);
				break;
			case XAUTH_MESSAGE:
				if (message)	// we've already seen that attribute
					break;
				message = malloc(ntohs(attr->lorv) + 1);
				if (message) {
					bcopy(dataptr + sizeof(u_int32_t), message, ntohs(attr->lorv));
					message[ntohs(attr->lorv)] = 0;
				}
				break;
			case XAUTH_CHALLENGE:
				SCLog(TRUE, LOG_ERR, CFSTR("IPSec Controller: Received unsupported Xauth Challenge"));
				goto fail;
				break;
			case XAUTH_DOMAIN:
				SCLog(TRUE, LOG_ERR, CFSTR("IPSec Controller: Ignoring unsupported Xauth Domain"));
				break;
			case XAUTH_STATUS:
				SCLog(TRUE, LOG_ERR, CFSTR("IPSec Controller: Received unsupported Xauth Status"));
				goto fail;
				break;
			case XAUTH_NEXT_PIN:
				serv->u.ipsec.xauth_flags |= XAUTH_NEED_NEXT_PIN;
				serv->u.ipsec.xauth_flags &= ~(XAUTH_NEED_USERNAME + XAUTH_NEED_PASSWORD + XAUTH_NEED_PASSCODE + XAUTH_NEED_ANSWER);
				break;
			case XAUTH_ANSWER:
				serv->u.ipsec.xauth_flags |= XAUTH_NEED_ANSWER;
				serv->u.ipsec.xauth_flags &= ~(XAUTH_NEED_USERNAME + XAUTH_NEED_PASSWORD + XAUTH_NEED_PASSCODE + XAUTH_NEED_NEXT_PIN);
				break;
			default:
				break;
		}


		if (tlv) {
			tlen -= ntohs(attr->lorv);
			dataptr += ntohs(attr->lorv);
		}
		
		tlen -= sizeof(u_int32_t);
		dataptr += sizeof(u_int32_t);
			
	}
	
	if (serv->u.ipsec.xauth_flags & XAUTH_FIRST_TIME) {
		
		serv->u.ipsec.xauth_flags &= ~(XAUTH_FIRST_TIME);
		
		// first time, use credential info from the prefs if they were available
		// additional times, assume failure and re-prompt
		
		if (!(serv->u.ipsec.xauth_flags & XAUTH_MUST_PROMPT)) {

			CFStringRef username = NULL;
			CFStringRef password = NULL;
			int has_info = 0;
				
			if (serv->u.ipsec.xauth_flags & XAUTH_NEED_USERNAME) {
				username = CFDictionaryGetValue(serv->u.ipsec.config, kRASPropIPSecXAuthName);
				has_info = isString(username) && CFStringGetLength(username);		
			}
			
			if (has_info || !(serv->u.ipsec.xauth_flags & XAUTH_NEED_USERNAME)) {
				if (serv->u.ipsec.xauth_flags & (XAUTH_NEED_PASSWORD | XAUTH_NEED_PASSCODE)) {
					password = copy_decrypted_password(serv);
					has_info = isString(password) && CFStringGetLength(password);
				}
			}

			if (has_info) {

				struct isakmp_xauth		isakmp_array[2]; /* max 2 attributes currently supported simultaneously */
				int						isakmp_nb = 0;

				if (serv->u.ipsec.xauth_flags & XAUTH_NEED_USERNAME) {
					isakmp_array[isakmp_nb].type = XAUTH_USER_NAME;
					isakmp_array[isakmp_nb].str = username;
					isakmp_nb++;
				}
				
				if (serv->u.ipsec.xauth_flags & XAUTH_NEED_PASSCODE) {
					isakmp_array[isakmp_nb].type = XAUTH_PASSCODE;
					isakmp_array[isakmp_nb].str = password;
					isakmp_nb++;
				}
				else if (serv->u.ipsec.xauth_flags & XAUTH_NEED_PASSWORD) {
					isakmp_array[isakmp_nb].type = XAUTH_USER_PASSWORD;
					isakmp_array[isakmp_nb].str = password;
					isakmp_nb++;
				}

#ifdef TARGET_EMBEDDED_OS
				CFRunLoopTimerSetNextFireDate(serv->u.ipsec.timerref, CFAbsoluteTimeGetCurrent() + TIMEOUT_PHASE1);
				ipsec_updatephase(serv, IPSEC_PHASE1);
#else
				if (serv->u.ipsec.phase == IPSEC_PHASE1AUTH) {
					CFRunLoopTimerSetNextFireDate(serv->u.ipsec.timerref, CFAbsoluteTimeGetCurrent() + TIMEOUT_PHASE1);
					ipsec_updatephase(serv, IPSEC_PHASE1);
				}
#endif /* TARGET_EMBEDDED_OS */
				racoon_send_cmd_xauthinfo(serv->u.ipsec.controlfd, serv->u.ipsec.peer_address.sin_addr.s_addr, isakmp_array, isakmp_nb);
				if (password)
					CFRelease(password);
				goto done;
			}
			
			if (password)
				CFRelease(password);
		}
	}
	
	if (ask_user_xauth(serv, message))
		goto fail;	
	
	serv->u.ipsec.xauth_flags |= XAUTH_DID_PROMPT;
	
done:
	if (message)
		free(message);
	return 0;

fail:
	serv->u.ipsec.xauth_flags = 0;
	if (message)
		free(message);
	return 1;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
static void print_racoon_msg(struct service *serv)
{
	struct vpnctl_status_phase_change *phase_change_status;
	struct vpnctl_status_failed *failed_status;
	struct vpnctl_cmd_xauth_info *cmd_xauth_info;
	struct vpnctl_cmd_bind *cmd_bind;
	struct in_addr addr;
	
#if 0
	char *rawdata = serv->u.ipsec.msg; 
	int i;

	printf("Header = 0x");
	for (i= 0; i < sizeof(struct vpnctl_hdr); i++) {
		printf("%02X ", rawdata[i]);
	}
	printf("\nData = 0x");
	for (i= 0; i < ntohs(serv->u.ipsec.msghdr.len); i++) {
		printf("%02X ", rawdata[i + sizeof(struct vpnctl_hdr)]);
	}
	printf("\n");
#endif
	
	SCLog(TRUE, LOG_INFO, CFSTR("IPSec Controller: ===================================================="));
	SCLog(TRUE, LOG_INFO, CFSTR("IPSec Controller: Process Message:"));
	SCLog(TRUE, LOG_INFO, CFSTR("IPSec Controller:	msg_type = 0x%x (%s)"), ntohs(serv->u.ipsec.msghdr.msg_type), ipsec_msgtype_to_str(ntohs(serv->u.ipsec.msghdr.msg_type)));
	SCLog(TRUE, LOG_INFO, CFSTR("IPSec Controller:	flags = 0x%x %s"), ntohs(serv->u.ipsec.msghdr.flags), (ntohs(serv->u.ipsec.msghdr.flags) & VPNCTL_FLAG_MODECFG_USED) ? "MODE CONFIG USED" : "");
	SCLog(TRUE, LOG_INFO, CFSTR("IPSec Controller:	cookie = 0x%x"), ntohl(serv->u.ipsec.msghdr.cookie));
	SCLog(TRUE, LOG_INFO, CFSTR("IPSec Controller:	reserved = 0x%x"), ntohl(serv->u.ipsec.msghdr.reserved));
	SCLog(TRUE, LOG_INFO, CFSTR("IPSec Controller:	result = 0x%x"), ntohs(serv->u.ipsec.msghdr.result));
	SCLog(TRUE, LOG_INFO, CFSTR("IPSec Controller:	len = %d"), ntohs(serv->u.ipsec.msghdr.len));


	switch (ntohs(serv->u.ipsec.msghdr.msg_type)) {
			// BIND/UNBIND/CONNECT share the same structure
			case VPNCTL_CMD_BIND:
			case VPNCTL_CMD_UNBIND:
			case VPNCTL_CMD_CONNECT:
			case VPNCTL_CMD_DISCONNECT:
				cmd_bind = (struct vpnctl_cmd_bind *)serv->u.ipsec.msg;
				SCLog(TRUE, LOG_INFO, CFSTR("IPSec Controller:	----------------------------"));
				addr.s_addr = cmd_bind->address;
				SCLog(TRUE, LOG_INFO, CFSTR("IPSec Controller:	address = %s"), inet_ntoa(addr));
				break;

			case VPNCTL_CMD_XAUTH_INFO:
				cmd_xauth_info = (struct vpnctl_cmd_xauth_info *)serv->u.ipsec.msg;
				SCLog(TRUE, LOG_INFO, CFSTR("IPSec Controller:	----------------------------"));
				addr.s_addr = cmd_xauth_info->address;
				SCLog(TRUE, LOG_INFO, CFSTR("IPSec Controller:	address = %s"), inet_ntoa(addr));
				break;

			case VPNCTL_STATUS_IKE_FAILED:
				failed_status = (struct vpnctl_status_failed *)serv->u.ipsec.msg;
				SCLog(TRUE, LOG_INFO, CFSTR("IPSec Controller:	----------------------------"));
				addr.s_addr = failed_status->address;
				SCLog(TRUE, LOG_INFO, CFSTR("IPSec Controller:	address = %s"), inet_ntoa(addr));
				SCLog(TRUE, LOG_INFO, CFSTR("IPSec Controller:	ike_code = %d 0x%x (%s)"), ntohs(failed_status->ike_code), ntohs(failed_status->ike_code), ipsec_error_to_str(ntohs(failed_status->ike_code)));
				SCLog(TRUE, LOG_INFO, CFSTR("IPSec Controller:	from = %d"), ntohs(failed_status->from));

				switch (ntohs(failed_status->ike_code)) {
					case VPNCTL_NTYPE_LOAD_BALANCE:		
						addr.s_addr = *(u_int32_t*)failed_status->data;
						SCLog(TRUE, LOG_INFO, CFSTR("IPSec Controller:	redirect address = %s"), inet_ntoa(addr));
						break;
				}

				break;

			case VPNCTL_STATUS_PH1_START_US:
				break;

			case VPNCTL_STATUS_PH1_START_PEER:
				break;

			case VPNCTL_STATUS_PH1_ESTABLISHED:
				phase_change_status = (struct vpnctl_status_phase_change *)serv->u.ipsec.msg;
				addr.s_addr = phase_change_status->address;
				SCLog(TRUE, LOG_INFO, CFSTR("IPSec Controller:	address = %s"), inet_ntoa(addr));
				if (ntohs(phase_change_status->hdr.flags) & VPNCTL_FLAG_MODECFG_USED) {
					char *modecfg_data = (char*)serv->u.ipsec.msg + sizeof(struct vpnctl_status_phase_change) + sizeof(struct vpnctl_modecfg_params);
					int modecfg_data_len = ntohs(serv->u.ipsec.msghdr.len) - ((sizeof(struct vpnctl_status_phase_change) + sizeof(struct vpnctl_modecfg_params)) -  sizeof(struct vpnctl_hdr));
					struct vpnctl_modecfg_params *modecfg = (struct vpnctl_modecfg_params *)(serv->u.ipsec.msg + sizeof(struct vpnctl_status_phase_change));
					addr.s_addr = modecfg->outer_local_addr;
					SCLog(TRUE, LOG_INFO, CFSTR("IPSec Controller:	outer_local_addr = %s"), inet_ntoa(addr));
					SCLog(TRUE, LOG_INFO, CFSTR("IPSec Controller:	outer_remote_port = %d"), ntohs(modecfg->outer_remote_port));
					SCLog(TRUE, LOG_INFO, CFSTR("IPSec Controller:	outer_local_port = %d"), ntohs(modecfg->outer_local_port));
					SCLog(TRUE, LOG_INFO, CFSTR("IPSec Controller:	ifname = %s"), modecfg->ifname);

					int tlen = modecfg_data_len;
					struct isakmp_data *attr;
					char	*dataptr = modecfg_data;
					
					while (tlen > 0)
					{
						int tlv;
						u_int16_t type;
						
						attr = (struct isakmp_data *)dataptr;
						type = ntohs(attr->type) & 0x7FFF;
						tlv = (type ==  ntohs(attr->type));

						SCLog(TRUE, LOG_INFO, CFSTR("IPSec Controller:	ModeConfig Attribute Type = %d (%s)"), type, ipsec_modecfgtype_to_str(type));
						if (tlv) {
							SCLog(TRUE, LOG_INFO, CFSTR("IPSec Controller:	ModeConfig Attribute Length = %d Value = ..."), ntohs(attr->lorv));

							tlen -= ntohs(attr->lorv);
							dataptr += ntohs(attr->lorv);
						}
						else {
							SCLog(TRUE, LOG_INFO, CFSTR("IPSec Controller:	ModeConfig Attribute Value = %d"), ntohs(attr->lorv));
						}
						
						tlen -= sizeof(u_int32_t);
						dataptr += sizeof(u_int32_t);
							
					}
				}
				break;

			case VPNCTL_STATUS_PH2_START:
				break;

			case VPNCTL_STATUS_PH2_ESTABLISHED:
				break;

			case VPNCTL_STATUS_NEED_AUTHINFO:
#ifndef TARGET_EMBEDDED_OS
			case VPNCTL_STATUS_NEED_REAUTHINFO:
#endif /* !TARGET_EMBEDDED_OS */
				cmd_xauth_info = (struct vpnctl_cmd_xauth_info *)serv->u.ipsec.msg;
				SCLog(TRUE, LOG_INFO, CFSTR("IPSec Controller:	----------------------------"));
				addr.s_addr = cmd_xauth_info->address;
				SCLog(TRUE, LOG_INFO, CFSTR("IPSec Controller:	address = %s"), inet_ntoa(addr));
				
				char *xauth_data = (char*)serv->u.ipsec.msg + sizeof(struct vpnctl_cmd_xauth_info); 
				int xauth_data_len = ntohs(serv->u.ipsec.msghdr.len) - (sizeof(struct vpnctl_cmd_xauth_info) -  sizeof(struct vpnctl_hdr));

				int tlen = xauth_data_len;
				struct isakmp_data *attr;
				char	*dataptr = xauth_data;
				
				while (tlen > 0)
				{
					int tlv;
					u_int16_t type;
					
					attr = (struct isakmp_data *)dataptr;
					type = ntohs(attr->type) & 0x7FFF;
					tlv = (type ==  ntohs(attr->type));

					SCLog(TRUE, LOG_INFO, CFSTR("IPSec Controller:	XAuth Attribute Type = %d (%s)"), type, ipsec_xauthtype_to_str(type));
					if (tlv) {
						if (type == XAUTH_MESSAGE) {
							char *message = malloc(ntohs(attr->lorv) + 1);
							if (message) {
								bcopy(dataptr + sizeof(u_int32_t), message, ntohs(attr->lorv));
								message[ntohs(attr->lorv)] = 0;
								SCLog(TRUE, LOG_INFO, CFSTR("IPSec Controller:	XAuth Attribute Value = %s"), message);
								free(message);
							}
						}
						else 
							SCLog(TRUE, LOG_INFO, CFSTR("IPSec Controller:	XAuth Attribute Length = %d Value = ..."), ntohs(attr->lorv));

						tlen -= ntohs(attr->lorv);
						dataptr += ntohs(attr->lorv);
					}
					else {
						SCLog(TRUE, LOG_INFO, CFSTR("IPSec Controller:	XAuth Attribute Value = %d"), ntohs(attr->lorv));
					}
					
					tlen -= sizeof(u_int32_t);
					dataptr += sizeof(u_int32_t);
						
				}
				break;

			default:
				/* ignore other messages */
				break;
	}

	SCLog(TRUE, LOG_INFO, CFSTR("IPSec Controller: ===================================================="));

}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
static void process_racoon_msg(struct service *serv)
{
	struct vpnctl_status_phase_change *phase_change_status;
	struct vpnctl_status_failed *failed_status;
	struct sockaddr_in	redirect_addr;
	
	
	if (gSCNCVerbose)
		print_racoon_msg(serv);
	
	switch (ntohs(serv->u.ipsec.msghdr.msg_type)) {
			case VPNCTL_STATUS_IKE_FAILED:
				failed_status = (struct vpnctl_status_failed *)serv->u.ipsec.msg;

				switch (ntohs(failed_status->ike_code)) {

					case VPNCTL_NTYPE_LOAD_BALANCE:		
						bzero(&redirect_addr, sizeof(redirect_addr));
						redirect_addr.sin_len = sizeof(redirect_addr);
						redirect_addr.sin_family = AF_INET;
						redirect_addr.sin_port = htons(0);
						redirect_addr.sin_addr.s_addr = *(u_int32_t*)failed_status->data;
						SCLog(TRUE, LOG_INFO, CFSTR("IPSec Controller: connection redirected to server '%s'..."), inet_ntoa(redirect_addr.sin_addr));
						racoon_restart(serv, &redirect_addr);
						break;

					default:	
						SCLog(TRUE, LOG_INFO, CFSTR("IPSec Controller: connection failed <IKE Error %d (0x%x) %s>"), ntohs(failed_status->ike_code), ntohs(failed_status->ike_code), ipsec_error_to_str(ntohs(failed_status->ike_code)));
						serv->u.ipsec.laststatus = ipsec_error_to_status(serv, ntohs(failed_status->from), ntohs(failed_status->ike_code));
						
						/* after phase 2, an authenticaion error is because the peer is disconnecting us */
						if ((serv->u.ipsec.laststatus == IPSEC_XAUTH_ERROR) && (serv->u.ipsec.phase >= IPSEC_PHASE2))
							serv->u.ipsec.laststatus = IPSEC_PEERDISCONNECT_ERROR;
						ipsec_stop(serv, 0);
						break;
				}

				break;

			case VPNCTL_STATUS_PH1_START_US:
				if (serv->u.ipsec.phase != IPSEC_INITIALIZE)
					break;
				ipsec_updatephase(serv, IPSEC_CONTACT);
				break;

			case VPNCTL_STATUS_PH1_START_PEER:
				if (serv->u.ipsec.phase != IPSEC_CONTACT)
					break;
				/* initial contact established, remove timer */
				CFRunLoopTimerSetNextFireDate(serv->u.ipsec.timerref, CFAbsoluteTimeGetCurrent() + TIMEOUT_PHASE1);
				ipsec_updatephase(serv, IPSEC_PHASE1);
				break;

			case VPNCTL_STATUS_NEED_AUTHINFO:
				if (serv->u.ipsec.phase != IPSEC_PHASE1)
					break;

				ipsec_updatephase(serv, IPSEC_PHASE1AUTH);
				CFRunLoopTimerSetNextFireDate(serv->u.ipsec.timerref, FAR_FUTURE);
				if (process_xauth_need_info(serv)) {
					SCLog(TRUE, LOG_INFO, CFSTR("IPSec Controller: XAuth authentication failed"));
					ipsec_stop(serv, 0);
				}
				break;

			case VPNCTL_STATUS_PH1_ESTABLISHED:
				SCLog(TRUE, LOG_INFO, CFSTR("IPSec Controller:	----------------------------"));
				if (serv->u.ipsec.phase != IPSEC_PHASE1)
					break;
				phase_change_status = (struct vpnctl_status_phase_change *)serv->u.ipsec.msg;
				if (ntohs(phase_change_status->hdr.flags) & VPNCTL_FLAG_MODECFG_USED) {
					install_mode_config(serv);
				}
				serv->u.ipsec.ping_count = MAX_PHASE2_PING;
				CFRunLoopTimerSetNextFireDate(serv->u.ipsec.timerref, CFAbsoluteTimeGetCurrent() + TIMEOUT_PHASE2_PING);
				//CFRunLoopRemoveTimer(CFRunLoopGetCurrent(), serv->u.ipsec.timerref, kCFRunLoopCommonModes);
				serv->connecttime = mach_absolute_time() * gTimeScaleSeconds;
				//ipsec_updatephase(serv, IPSEC_RUNNING);
				//ipsec_updatephase(serv, IPSEC_PHASE1);
				break;

			case VPNCTL_STATUS_PH2_START:
				if (serv->u.ipsec.phase != IPSEC_PHASE1)
					break;
				CFRunLoopTimerSetNextFireDate(serv->u.ipsec.timerref, CFAbsoluteTimeGetCurrent() + TIMEOUT_PHASE2);
				ipsec_updatephase(serv, IPSEC_PHASE2);
				break;

			case VPNCTL_STATUS_PH2_ESTABLISHED:
				if (serv->u.ipsec.phase != IPSEC_PHASE2)
					break;
				CFRunLoopRemoveTimer(CFRunLoopGetCurrent(), serv->u.ipsec.timerref, kCFRunLoopCommonModes);
				if (serv->u.ipsec.banner) {
					display_notification(serv, serv->u.ipsec.banner, 0, dialog_has_disconnect_type);
					my_CFRelease(&serv->u.ipsec.banner);
				}
                SESSIONTRACERESTABLISHED(serv);
				ipsec_updatephase(serv, IPSEC_RUNNING);
			break;

#ifndef TARGET_EMBEDDED_OS
			case VPNCTL_STATUS_NEED_REAUTHINFO:
				if (serv->u.ipsec.phase != IPSEC_RUNNING)
					break;

				if (process_xauth_need_info(serv)) {
					SCLog(TRUE, LOG_INFO, CFSTR("IPSec Controller: XAuth reauthentication failed"));
				}
				break;
#endif /* !TARGET_EMBEDDED_OS */

			default:
				/* ignore other messages */
				break;
	}
}


/* ----------------------------------------------------------------------------
publish ip addresses using configd cache mechanism
use new state information model
----------------------------------------------------------------------------- */
int publish_stateaddr(SCDynamicStoreRef store, CFStringRef serviceID, char *if_name, u_int32_t server, u_int32_t o, u_int32_t h, u_int32_t m, int isprimary)
{
    struct in_addr		addr;
    CFMutableArrayRef		array;
    CFMutableDictionaryRef	ipv4_dict;
    CFStringRef			str;
    CFNumberRef		num;
	SCNetworkServiceRef netservRef;
	
#define IP_FORMAT	"%d.%d.%d.%d"
#define IP_CH(ip)	((u_char *)(ip))
#define IP_LIST(ip)	IP_CH(ip)[0],IP_CH(ip)[1],IP_CH(ip)[2],IP_CH(ip)[3]
	
	
    /* create the IPV4 dictionnary */
    if ((ipv4_dict = CFDictionaryCreateMutable(0, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks)) == 0)
        return 0;
	
	/* set the ip address src and dest arrays */
    if (array = CFArrayCreateMutable(0, 1, &kCFTypeArrayCallBacks)) {
        addr.s_addr = o;
        if (str = CFStringCreateWithFormat(0, 0, CFSTR(IP_FORMAT), IP_LIST(&addr))) {
            CFArrayAppendValue(array, str);
            CFRelease(str);
            CFDictionarySetValue(ipv4_dict, kSCPropNetIPv4Addresses, array); 
        }
        CFRelease(array);
    }
	
    /* set the router */
    addr.s_addr = h;
    if (str = CFStringCreateWithFormat(NULL, NULL, CFSTR(IP_FORMAT), IP_LIST(&addr))) {
        CFDictionarySetValue(ipv4_dict, kSCPropNetIPv4Router, str);
        CFRelease(str);
    }
	
	num = CFNumberCreate(NULL, kCFNumberIntType, &isprimary);
	if (num) {
        CFDictionarySetValue(ipv4_dict, kSCPropNetOverridePrimary, num);
		CFRelease(num);
	}
	
	if (if_name) {
		if (str = CFStringCreateWithFormat(NULL, NULL, CFSTR("%s"), if_name)) {
			CFDictionarySetValue(ipv4_dict, kSCPropInterfaceName, str);
			CFRelease(str);
		}
	}
	
    /* set the server */
    addr.s_addr = server;
    if (str = CFStringCreateWithFormat(NULL, NULL, CFSTR(IP_FORMAT), IP_LIST(&addr))) {
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
	
    /* update the store now */
    if (str = SCDynamicStoreKeyCreateNetworkServiceEntity(0, kSCDynamicStoreDomainState, serviceID, kSCEntNetIPv4)) {
        
        if (SCDynamicStoreSetValue(store, str, ipv4_dict) == 0)
            ;//warning("SCDynamicStoreSetValue IP %s failed: %s\n", ifname, SCErrorString(SCError()));
		
        CFRelease(str);
    }
    
    CFRelease(ipv4_dict);
	
	/* rank service, to prevent it from becoming primary */
	if (!isprimary) {
		netservRef = _SCNetworkServiceCopyActive(store, serviceID);
		if (netservRef) {
			SCNetworkServiceSetPrimaryRank(netservRef, kSCNetworkServicePrimaryRankNever);
			CFRelease(netservRef);
		}
	}
	
    return 1;
}

/* ----------------------------------------------------------------------------
 ----------------------------------------------------------------------------- */
int ipsec_ondemand_add_service_data(struct service *serv, CFMutableDictionaryRef ondemand_dict)
{
	CFArrayRef			array;
	CFStringRef			string;
	
	array = CFDictionaryGetValue(serv->systemprefs, kSCPropNetIPSecOnDemandMatchDomainsAlways);
	if (isArray(array))
		CFDictionarySetValue(ondemand_dict, kSCNetworkConnectionOnDemandMatchDomainsAlways, array);
	array = CFDictionaryGetValue(serv->systemprefs, kSCPropNetIPSecOnDemandMatchDomainsOnRetry);
	if (isArray(array))
		CFDictionarySetValue(ondemand_dict, kSCNetworkConnectionOnDemandMatchDomainsOnRetry, array);
	array = CFDictionaryGetValue(serv->systemprefs, kSCPropNetIPSecOnDemandMatchDomainsNever);
	if (isArray(array))
		CFDictionarySetValue(ondemand_dict, kSCNetworkConnectionOnDemandMatchDomainsNever, array);
	
	string = CFDictionaryGetValue(serv->systemprefs, kRASPropIPSecRemoteAddress);
	if (isString(string))
		CFDictionarySetValue(ondemand_dict, kSCNetworkConnectionOnDemandRemoteAddress, string);
	
	return 0;
}

/* ----------------------------------------------------------------------------
 publish proxies using configd cache mechanism
 ----------------------------------------------------------------------------- */
int publish_proxies(SCDynamicStoreRef store, CFStringRef serviceID, int autodetect, CFStringRef server, int port, int bypasslocal, CFStringRef exceptionlist)
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
	else {
		// MUST have server
		if (server == NULL)
			goto fail;
		
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
		
    /* update the store now */
    cfstr = SCDynamicStoreKeyCreateNetworkServiceEntity(0, kSCDynamicStoreDomainState, serviceID, kSCEntNetProxies);
    if (cfstr == NULL)
		goto fail;

	if (SCDynamicStoreSetValue(store, cfstr, proxies_dict) == 0) {
		//warning("SCDynamicStoreSetValue IP %s failed: %s\n", ifname, SCErrorString(SCError()));
		goto fail;
	}
		
	ret = 0;	
		
fail:
	
    my_CFRelease(&cfone);
	my_CFRelease(&cfstr);
    my_CFRelease(&proxies_dict);
    return ret;
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
int sifmtu(char *ifname, int mtu)
{
    struct ifreq ifr;
	int ip_sockfd;
	
    ip_sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (ip_sockfd < 0) {
		SCLog(TRUE, LOG_ERR, CFSTR("IPSec Controller: cannot create ip socket (errno = %d)"), errno);
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
int sifaddr(char *ifname, u_int32_t o, u_int32_t h, u_int32_t m)
{
    struct ifaliasreq ifra;
	int ip_sockfd;
	
    ip_sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (ip_sockfd < 0) {
		SCLog(TRUE, LOG_ERR, CFSTR("IPSec Controller: cannot create ip socket (errno = %d)"), errno);
		return 0;
	}

    strlcpy(ifra.ifra_name, ifname, sizeof(ifra.ifra_name));
	
    SET_SA_FAMILY(ifra.ifra_addr, AF_INET);
    ((struct sockaddr_in *) &ifra.ifra_addr)->sin_addr.s_addr = o;
    
	SET_SA_FAMILY(ifra.ifra_broadaddr, AF_INET);
    ((struct sockaddr_in *) &ifra.ifra_broadaddr)->sin_addr.s_addr = h;
    
	if (m != 0) {
		SET_SA_FAMILY(ifra.ifra_mask, AF_INET);
		((struct sockaddr_in *) &ifra.ifra_mask)->sin_addr.s_addr = m;
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
int cifaddr(char *ifname, u_int32_t o, u_int32_t h)
{
    //struct ifreq ifr;
    struct ifaliasreq ifra;
	int ip_sockfd;
	
    ip_sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (ip_sockfd < 0) {
		SCLog(TRUE, LOG_ERR, CFSTR("IPSec Controller: cannot create ip socket (errno = %d)"), errno);
		return 0;
	}

    strlcpy(ifra.ifra_name, ifname, sizeof(ifra.ifra_name));
    SET_SA_FAMILY(ifra.ifra_addr, AF_INET);
    ((struct sockaddr_in *) &ifra.ifra_addr)->sin_addr.s_addr = o;
    SET_SA_FAMILY(ifra.ifra_broadaddr, AF_INET);
    ((struct sockaddr_in *) &ifra.ifra_broadaddr)->sin_addr.s_addr = h;
    bzero(&ifra.ifra_mask, sizeof(ifra.ifra_mask));
    if (ioctl(ip_sockfd, SIOCDIFADDR, (caddr_t) &ifra) < 0) {
		if (errno != EADDRNOTAVAIL)
			;//warning("Couldn't delete interface address: %m");
		return 0;
    }
	
	close(ip_sockfd);
    return 1;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
static int
racoon_trigger_phase2(char *ifname, struct in_addr *ping)
{
	struct icmp *icp;
	int cc, i, j, nbping;    
	struct sockaddr_in whereto;	/* who to ping */
	uint8_t data[256];
	int s, ifindex;
	
	s = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
	if (s < 0)
		return -1;
	
	ifindex = if_nametoindex(ifname);
	setsockopt(s, IPPROTO_IP, IP_BOUND_IF, &ifindex, sizeof(ifindex)); 
	
	whereto.sin_family = AF_INET;	/* Internet address family */
	whereto.sin_port = 0;		/* Source port */
	whereto.sin_addr.s_addr = ping->s_addr;	/* Dest. address */
	
	icp = (struct icmp *)data;
	icp->icmp_type = ICMP_ECHO;
	icp->icmp_code = 0;
	icp->icmp_cksum = 0;
	icp->icmp_seq = htons(0);
	icp->icmp_id = 0;			/* ID */
	
	cc = ICMP_MINLEN;
		
	size_t len = sizeof(int); 
	if (sysctlbyname("net.key.blockacq_count", &nbping, &len, 0, 0))
		nbping = 10;

	for (j = 0; j <= nbping; j++) {
		i = sendto(s, data, cc, 0, (struct sockaddr *)&whereto, sizeof(whereto));
		if (i < cc) {
			close(s);
			return -1;
		}
	}
	close(s);
	return 0;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
static void uninstall_mode_config(struct service *serv)
{
	int		error = 0;
	char	*errorstr;
	struct in_addr ip_zeros = { 0 };
	struct in_addr addr;
	
	if (!serv->u.ipsec.modecfg_installed)
		return;
		
	cifaddr(serv->u.ipsec.if_name, serv->u.ipsec.inner_local_addr, 0xFFFFFFFF); 

	unpublish_dict(gDynamicStore, serv->serviceID, kSCEntNetIPv4);
	unpublish_dict(gDynamicStore, serv->serviceID, kSCEntNetDNS);
	unpublish_dict(gDynamicStore, serv->serviceID, NULL);
	
	if (serv->u.ipsec.modecfg_routes_installed) {
		addr.s_addr = serv->u.ipsec.inner_local_addr;
		error = IPSecRemoveRoutes(serv->u.ipsec.modecfg_policies, -1,  &errorstr, addr);
		if (error)
			SCLog(TRUE, LOG_ERR, CFSTR("IPSec Controller: Cannot remove mode routes, error '%s'"), errorstr);
		serv->u.ipsec.modecfg_routes_installed = 0;
	}

	if (serv->u.ipsec.modecfg_policies) {
		error = IPSecRemovePolicies(serv->u.ipsec.modecfg_policies, -1,  &errorstr);
		if (error)
			SCLog(TRUE, LOG_ERR, CFSTR("IPSec Controller: Cannot remove mode config policies, error '%s'"), errorstr);
		my_CFRelease(&serv->u.ipsec.modecfg_policies);

	}
	if (serv->u.ipsec.modecfg_defaultroute) {
		// don't need that since we unplished the whole dictionary
		// unpublish_dictentry(gDynamicStore, serv->serviceID, kSCEntNetIPv4, kSCPropNetOverridePrimary);
		serv->u.ipsec.modecfg_defaultroute = 0;						
	}
	
	IPSecRemoveSecurityAssociations((struct sockaddr *)&serv->u.ipsec.our_address, (struct sockaddr *)&serv->u.ipsec.peer_address);

    if (serv->u.ipsec.modecfg_peer_route_set) {
		set_host_gateway(RTM_DELETE, serv->u.ipsec.peer_address.sin_addr, ip_zeros, 0, serv->u.ipsec.modecfg_peer_route_set == 1 ? 0 : 1);
        serv->u.ipsec.modecfg_peer_route_set = 0;
    }
	
	my_close(serv->u.ipsec.kernctl_sock);
	serv->u.ipsec.kernctl_sock = -1;

	my_CFRelease(&serv->u.ipsec.banner);

	serv->u.ipsec.modecfg_installed = 0;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
static void install_mode_config(struct service *serv)
{
	struct in_addr addr, mask;
	char	*data;
	int		error, isdefault = 1, len, data_len;
	char	*errorstr;
	u_int32_t dns, prefix; 
	struct sockaddr_ctl       kernctl_addr;
	struct ctl_info	kernctl_info;
	u_int32_t	internal_ip4_address, internal_ip4_netmask;
	CFStringRef	strRef, domain_name = NULL;
	CFMutableArrayRef split_dns_array = NULL, dns_array = NULL;
	u_int32_t optflags;
	socklen_t optlen;
	
	serv->u.ipsec.ping_addr.s_addr = 0;

	struct vpnctl_status_phase_change *phase_change_status = (struct vpnctl_status_phase_change *)serv->u.ipsec.msg;
	struct vpnctl_modecfg_params *modecfg = (struct vpnctl_modecfg_params *)(serv->u.ipsec.msg + sizeof(struct vpnctl_status_phase_change));

	char *modecfg_data = (char*)serv->u.ipsec.msg + sizeof(struct vpnctl_status_phase_change) + sizeof(struct vpnctl_modecfg_params);
	int modecfg_data_len = ntohs(serv->u.ipsec.msghdr.len) - ((sizeof(struct vpnctl_status_phase_change) + sizeof(struct vpnctl_modecfg_params)) -  sizeof(struct vpnctl_hdr));

	CFMutableArrayRef policies_array;
	CFMutableDictionaryRef policies, policy;
		
	internal_ip4_address = htonl(0);
	internal_ip4_netmask = htonl(0xFFFFFFFF);
	
	/* first pass, get mandatory parameters */

	int tlen = modecfg_data_len;
	struct isakmp_data *attr;
	char	*dataptr = modecfg_data;
	
	while (tlen > 0)
	{
		int tlv;
		u_int16_t type;
		
		attr = (struct isakmp_data *)dataptr;
		type = ntohs(attr->type) & 0x7FFF;
		tlv = (type ==  ntohs(attr->type));
		
		switch (type)
		{
			case INTERNAL_IP4_ADDRESS:
				internal_ip4_address = *(u_int32_t *)(dataptr + sizeof(u_int32_t));  // network byte order 
				break;

			case INTERNAL_IP4_NETMASK:
				internal_ip4_netmask = *(u_int32_t *)(dataptr + sizeof(u_int32_t));  // network byte order
				break;

			default:
				break;
		}
		
		if (tlv) {
			tlen -= ntohs(attr->lorv);
			dataptr += ntohs(attr->lorv);
		}
		
		tlen -= sizeof(u_int32_t);
		dataptr += sizeof(u_int32_t);
			
	}
	
	if (internal_ip4_address == htonl(0)) {
		// ip address is missing
		SCLog(TRUE, LOG_ERR, CFSTR("IPSec Controller: Internal IP Address missing from Modem Config packet "));
		return;
	}

	policies = CFDictionaryCreateMutable(NULL, 0, &kCFCopyStringDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
	policies_array = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
	
	addr.s_addr = modecfg->outer_local_addr;
	AddString(policies, kRASPropIPSecLocalAddress, inet_ntoa(addr));
	addr.s_addr = phase_change_status->address;
	AddString(policies, kRASPropIPSecRemoteAddress, inet_ntoa(addr));

	/* second pass, get optional parameters */

	tlen = modecfg_data_len;
	dataptr = modecfg_data;

	while (tlen > 0)
	{
		int tlv;
		u_int16_t type;
		
		attr = (struct isakmp_data *)dataptr;
		type = ntohs(attr->type) & 0x7FFF;
		tlv = (type ==  ntohs(attr->type));
		
		switch (type)
		{
			case INTERNAL_IP4_DNS:
				if (!dns_array)
					dns_array = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);

				if (!dns_array)
					break;
				
				dns = *(u_int32_t *)(dataptr + sizeof(u_int32_t));  // network byte order
				strRef = CFStringCreateWithFormat(NULL, NULL, CFSTR(IP_FORMAT), IP_LIST(&dns));
				if (!strRef)
					break;

				CFArrayAppendValue(dns_array, strRef);
				CFRelease(strRef);
				break;


			default:
				break;
		}


		if (tlv) {
			tlen -= ntohs(attr->lorv);
			dataptr += ntohs(attr->lorv);
		}
		
		tlen -= sizeof(u_int32_t);
		dataptr += sizeof(u_int32_t);
			
	}

	
	if (isdefault) {
		// default route
		policy = CFDictionaryCreateMutable(NULL, 0, &kCFCopyStringDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);

		CFDictionarySetValue(policy, kRASPropIPSecPolicyMode, kRASValIPSecPolicyModeTunnel);
		CFDictionarySetValue(policy, kRASPropIPSecPolicyDirection, kRASValIPSecPolicyDirectionInOut);
		CFDictionarySetValue(policy, kRASPropIPSecPolicyLevel, kRASValIPSecPolicyLevelRequire);

		addr.s_addr = internal_ip4_address;
		AddString(policy, kRASPropIPSecPolicyLocalAddress, inet_ntoa(addr));
		mask.s_addr = ntohl(0xFFFFFFFF /* internal_ip4_netmask */); // VPN client should use 255.255.255.255
		for (prefix = 0; mask.s_addr; mask.s_addr<<=1, prefix++);
		AddNumber(policy, kRASPropIPSecPolicyLocalPrefix, prefix);

		serv->u.ipsec.ping_addr.s_addr = internal_ip4_address; // will ping our own address, it will hit the (internal_ip4_address --> any) policy
		
		CFDictionarySetValue(policy, kRASPropIPSecPolicyRemoteAddress, CFSTR("0.0.0.0"));
		AddNumber(policy, kRASPropIPSecPolicyRemotePrefix, 0);
		
		CFArrayAppendValue(policies_array, policy);
		CFRelease(policy);
	}

	
	CFDictionarySetValue(policies, kRASPropIPSecPolicies, policies_array);
	CFRelease(policies_array);
	
	if (gSCNCDebug) {
		CFShow(CFSTR("IPSec Controller: Mode Config Policies"));
		CFShow(policies);
	}
		
	if (error = IPSecInstallPolicies(policies, -1, &errorstr) < 0) {
		SCLog(TRUE, LOG_ERR, CFSTR("IPSec Controller: IPSecInstallPolicies failed '%s'"), errorstr);
		CFRelease(policies);
		goto fail;
	}

	serv->u.ipsec.modecfg_policies = policies;						
	serv->u.ipsec.modecfg_defaultroute = isdefault;						

	serv->u.ipsec.inner_local_addr = internal_ip4_address; 
	serv->u.ipsec.inner_local_mask = internal_ip4_netmask; 

	/* create the virtual interface */
	serv->u.ipsec.kernctl_sock = socket(PF_SYSTEM, SOCK_DGRAM, SYSPROTO_CONTROL);
	if (serv->u.ipsec.kernctl_sock == -1) {
		SCLog(TRUE, LOG_ERR, CFSTR("IPSec Controller: cannot create kernel control socket (errno = %d)"), errno);
		goto fail;
	}


/* user tunnel interface private definitions */
	
#define UTUN_CONTROL_NAME "com.apple.net.utun_control"
#define UTUN_OPT_FLAGS			1
#define UTUN_OPT_IFNAME			2
	
enum {
	UTUN_FLAGS_NO_OUTPUT = 0x1,
	UTUN_FLAGS_NO_INPUT = 0x2,
};
		
	bzero(&kernctl_info, sizeof(kernctl_info));
    strcpy(kernctl_info.ctl_name, UTUN_CONTROL_NAME);
	if (ioctl(serv->u.ipsec.kernctl_sock, CTLIOCGINFO, &kernctl_info)) {
		SCLog(TRUE, LOG_ERR, CFSTR("IPSec Controller: ioctl failed on kernel control socket (errno = %d)"), errno);
		goto fail;
	}
	
	bzero(&kernctl_addr, sizeof(kernctl_addr)); // sets the sc_unit field to 0
	kernctl_addr.sc_len = sizeof(kernctl_addr);
	kernctl_addr.sc_family = AF_SYSTEM;
	kernctl_addr.ss_sysaddr = AF_SYS_CONTROL;
	kernctl_addr.sc_id = kernctl_info.ctl_id;
	kernctl_addr.sc_unit = 0; // we will get the unit numner from getpeername
	if (connect(serv->u.ipsec.kernctl_sock, (struct sockaddr *)&kernctl_addr, sizeof(kernctl_addr))) {
		SCLog(TRUE, LOG_ERR, CFSTR("IPSec Controller: connect failed on kernel control socket (errno = %d)"), errno);
		goto fail;
	}

	optlen = sizeof(serv->u.ipsec.if_name);
	if (getsockopt(serv->u.ipsec.kernctl_sock, SYSPROTO_CONTROL, UTUN_OPT_IFNAME, serv->u.ipsec.if_name, &optlen)) {
		SCLog(TRUE, LOG_ERR, CFSTR("IPSec Controller: getsockopt ifname failed on kernel control socket (errno = %d)"), errno);
		goto fail;	
	}
		
	optflags = 0;
	optlen = sizeof(u_int32_t);
	if (getsockopt(serv->u.ipsec.kernctl_sock, SYSPROTO_CONTROL, UTUN_OPT_FLAGS, &optflags, &optlen)) {
		SCLog(TRUE, LOG_ERR, CFSTR("IPSec Controller: getsockopt flags failed on kernel control socket (errno = %d)"), errno);
		goto fail;	
	}
	 
	optflags |= (UTUN_FLAGS_NO_INPUT + UTUN_FLAGS_NO_OUTPUT);
	optlen = sizeof(u_int32_t);
	if (setsockopt(serv->u.ipsec.kernctl_sock, SYSPROTO_CONTROL, UTUN_OPT_FLAGS, &optflags, optlen)) {
		SCLog(TRUE, LOG_ERR, CFSTR("IPSec Controller: setsockopt flags failed on kernel control socket (errno = %d)"), errno);
		goto fail;	
	}
	
	sifmtu(serv->u.ipsec.if_name, 1280); 
	sifaddr(serv->u.ipsec.if_name, internal_ip4_address, internal_ip4_address,internal_ip4_netmask); 

	SCNetworkReachabilityRef	ref;
	SCNetworkConnectionFlags	flags;
	bool 			is_peer_local;
	struct in_addr ip_zeros = { 0 };
	
	/* check if is peer on our local subnet */
	ref = SCNetworkReachabilityCreateWithAddress(NULL, (struct sockaddr *)&serv->u.ipsec.peer_address);
	is_peer_local = SCNetworkReachabilityGetFlags(ref, &flags) && (flags & kSCNetworkFlagsIsDirect);
	CFRelease(ref);
	
	set_host_gateway(RTM_DELETE, serv->u.ipsec.peer_address.sin_addr, ip_zeros, 0, 0);
	
	if (is_peer_local 
		|| (serv->u.ipsec.lower_gateway.s_addr == 0)) {
		
		if (serv->u.ipsec.lower_interface[0]) {
			/* subnet route */
			struct in_addr		gateway;
            bzero(&gateway, sizeof(gateway));
			set_host_gateway(RTM_ADD, serv->u.ipsec.peer_address.sin_addr, gateway, serv->u.ipsec.lower_interface, 1);
			serv->u.ipsec.modecfg_peer_route_set = 2;
		}
	}
	else {
		/* host route */
		set_host_gateway(RTM_ADD, serv->u.ipsec.peer_address.sin_addr, serv->u.ipsec.lower_gateway, 0, 0);
		serv->u.ipsec.modecfg_peer_route_set = 1;
	}

	if (!isdefault) {
		addr.s_addr = internal_ip4_address;
		if (error = IPSecInstallRoutes(policies, -1, &errorstr, addr) < 0) {
			SCLog(TRUE, LOG_ERR, CFSTR("IPSec Controller: IPSecInstallRoutes failed '%s'"), errorstr);
		}
		serv->u.ipsec.modecfg_routes_installed = 1;
	}
	

//	publish_stateaddr(gDynamicStore, serv->serviceID, modecfg->inner_local_addr,modecfg->inner_local_addr, modecfg->inner_local_mask, 1);
	publish_stateaddr(gDynamicStore, serv->serviceID, serv->u.ipsec.if_name, serv->u.ipsec.peer_address.sin_addr.s_addr, internal_ip4_address,internal_ip4_address, internal_ip4_netmask, isdefault);
	
	
	

	if (error = racoon_send_cmd_start_ph2(serv->u.ipsec.controlfd, serv->u.ipsec.peer_address.sin_addr.s_addr, policies) != 0) {
		SCLog(TRUE, LOG_ERR, CFSTR("IPSec Controller: racoon_send_cmd_start_ph2 failed '%s'"), errorstr);
		goto fail;
	}
	racoon_trigger_phase2(serv->u.ipsec.if_name, &serv->u.ipsec.ping_addr);

	if (dns_array) {
		
		// add split dns array if only domain name was provided by the server
		if (!split_dns_array && domain_name) {
			split_dns_array = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
			if (split_dns_array) {
				CFArrayAppendValue(split_dns_array, domain_name);
			}
		}
		
		publish_dns(serv, dns_array, domain_name, split_dns_array);
	}
	
	my_CFRelease(&split_dns_array);
	my_CFRelease(&domain_name);
	my_CFRelease(&dns_array);
	
	serv->u.ipsec.modecfg_installed = 1;

	return;
	
fail:
	my_CFRelease(&split_dns_array);
	my_CFRelease(&domain_name);
	my_CFRelease(&dns_array);
	my_CFRelease(&serv->u.ipsec.banner);

	if (serv->u.ipsec.modecfg_routes_installed) {
		addr.s_addr = serv->u.ipsec.inner_local_addr;
		error = IPSecRemoveRoutes(serv->u.ipsec.modecfg_policies, -1,  &errorstr, addr);
		serv->u.ipsec.modecfg_routes_installed = 0;
	}

	if (serv->u.ipsec.modecfg_policies) {
		error = IPSecRemovePolicies(serv->u.ipsec.modecfg_policies, -1,  &errorstr);
		my_CFRelease(&serv->u.ipsec.modecfg_policies);

	}
	if (serv->u.ipsec.modecfg_defaultroute) {
		// don't need that since we unplished the whole dictionary
		// unpublish_dictentry(gDynamicStore, serv->serviceID, kSCEntNetIPv4, kSCPropNetOverridePrimary);
		serv->u.ipsec.modecfg_defaultroute = 0;						
	}

	my_close(serv->u.ipsec.kernctl_sock);
	serv->u.ipsec.kernctl_sock = -1;	
}

/* -----------------------------------------------------------------------------
 set dns information
 ----------------------------------------------------------------------------- */
static 
int publish_dns(struct service *serv, CFArrayRef dns, CFStringRef domain, CFArrayRef supp_domains)
{    
    int				ret = 0;
    CFMutableDictionaryRef	dict = NULL;
    CFStringRef			key = NULL;
    CFPropertyListRef		ref;
	
    if (gDynamicStore == NULL)
        return 0;
	
    key = SCDynamicStoreKeyCreateNetworkServiceEntity(0, kSCDynamicStoreDomainState, serv->serviceID, kSCEntNetDNS);
    if (!key) 
        goto end;
	
    if (ref = SCDynamicStoreCopyValue(gDynamicStore, key)) {
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
	
	/* warn lookupd of upcoming change */
	notify_post("com.apple.system.dns.delay");
	
    if (SCDynamicStoreSetValue(gDynamicStore, key, dict))
        ret = 1;
    else
		;// warning("SCDynamicStoreSetValue DNS/WINS %s failed: %s\n", ifname, SCErrorString(SCError()));
	
end:
    if (key)  
        CFRelease(key);
    if (dict)  
        CFRelease(dict);
    return ret;
	
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
static 
void racoon_timer(CFRunLoopTimerRef timer, void *info)
{
	struct service *serv = info;
	struct sockaddr_in address;
	CFRange			range;
	CFDataRef		dataref;
	
	SCLog(gSCNCVerbose, LOG_INFO, CFSTR("IPSec Controller: racoon_timer expired"));
	
	/* if no contact, try next server */
	if (serv->u.ipsec.phase == IPSEC_INITIALIZE || serv->u.ipsec.phase == IPSEC_CONTACT) {

		if (serv->u.ipsec.resolvedAddress &&
			(serv->u.ipsec.next_address < CFArrayGetCount(serv->u.ipsec.resolvedAddress))) {

			dataref = CFArrayGetValueAtIndex(serv->u.ipsec.resolvedAddress, serv->u.ipsec.next_address);
			serv->u.ipsec.next_address++;
	
			//if (gSCNCDebug) {
			//	CFShow(dataref);
			//}
			bzero(&address, sizeof(address));
			range.location = 0;
			range.length = sizeof(address);
			CFDataGetBytes(dataref, range, (UInt8 *)&address); 

			SCLog(gSCNCDebug, LOG_INFO, CFSTR("IPSec Controller: racoon_timer call racoon_restart"));
			racoon_restart(serv, &address); 
			return;
		}
	}

	/* need to resend ping ? */ 
	if (serv->u.ipsec.phase == IPSEC_PHASE1) {

		if (serv->u.ipsec.ping_count > 0) {
			serv->u.ipsec.ping_count--;
			//printf("timer_resend ping, addr = 0x%s\n", inet_ntoa(serv->u.ipsec.ping_addr));
			racoon_trigger_phase2(serv->u.ipsec.if_name, &serv->u.ipsec.ping_addr);
			CFRunLoopTimerSetNextFireDate(serv->u.ipsec.timerref, CFAbsoluteTimeGetCurrent() + TIMEOUT_PHASE2_PING);
			return;
		}
	}
	
	switch (serv->u.ipsec.phase) {
		case IPSEC_INITIALIZE:
			serv->u.ipsec.laststatus = IPSEC_CONFIGURATION_ERROR;
			break;
		case IPSEC_CONTACT:
			serv->u.ipsec.laststatus = IPSEC_CONNECTION_ERROR;
			break;
		default:
			serv->u.ipsec.laststatus = IPSEC_NEGOTIATION_ERROR;
			break;
	}
	ipsec_stop(serv, 0);

}

static u_int32_t get_interface_timeout (u_int32_t interface_media)
{
    u_int32_t scaled_interface_timeout = TIMEOUT_INTERFACE_CHANGE;
#ifndef TARGET_EMBEDDED_OS
    // increase the timeout if we're waiting for a wireless interface
    if (IFM_TYPE(interface_media) == IFM_IEEE80211) {
        scaled_interface_timeout = (TIMEOUT_INTERFACE_CHANGE << 2);
    }
#endif /* !iPhone */
    SCLog(gSCNCVerbose, LOG_INFO, CFSTR("getting interface (media %x) timeout for ipsec: %d secs"), interface_media, scaled_interface_timeout);
    return scaled_interface_timeout;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
static 
void event_timer(CFRunLoopTimerRef timer, void *info)
{
	struct service *serv = info;
	
	SCLog(gSCNCVerbose, LOG_INFO, CFSTR("IPSec Controller: Network change event timer expired"));
	
	IPSecLogVPNInterfaceAddressEvent(__FUNCTION__, NULL, serv->u.ipsec.timeout_lower_interface_change, serv->u.ipsec.lower_interface, &serv->u.ipsec.our_address.sin_addr);

	serv->u.ipsec.laststatus = IPSEC_NETWORKCHANGE_ERROR;	
	ipsec_stop(serv, 0);

}


/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
static 
void racoon_callback(CFSocketRef inref, CFSocketCallBackType type,
                     CFDataRef address, const void *data, void *info)
{
    int 		s = CFSocketGetNative(inref);
    int			action = do_nothing;
    ssize_t		n;
	struct service *serv = info;
	
    /* first read the header part of the message */
    if (serv->u.ipsec.msglen < sizeof(struct vpnctl_hdr)) {
        n = readn(s, &((u_int8_t *)&serv->u.ipsec.msghdr)[serv->u.ipsec.msglen], sizeof(struct vpnctl_hdr) - serv->u.ipsec.msglen);
        switch (n) {
            case -1:
//	SCLog(TRUE, LOG_INFO, CFSTR("IPSec Controller: racoon_callback, failed to read header, closing"));
                action = do_close;
                break;
            default:
                serv->u.ipsec.msglen += n;
                if (serv->u.ipsec.msglen == sizeof(struct vpnctl_hdr)) {
				
                    serv->u.ipsec.msgtotallen = serv->u.ipsec.msglen + ntohs(serv->u.ipsec.msghdr.len);
                    serv->u.ipsec.msg = my_Allocate(serv->u.ipsec.msgtotallen + 1);
                    if (serv->u.ipsec.msg == 0)
                        action = do_error;
                    else {
                        bcopy(&serv->u.ipsec.msghdr, serv->u.ipsec.msg, sizeof(struct vpnctl_hdr));
                        // let's end the message with a null byte
                        serv->u.ipsec.msg[serv->u.ipsec.msgtotallen] = 0;
                    }
#if 0
					SCLog(TRUE, LOG_INFO, CFSTR("IPSec Controller: racoon_callback, header ="));
					SCLog(TRUE, LOG_INFO, CFSTR("IPSec Controller: racoon_callback,   msg_type = 0x%x"), serv->u.ipsec.msghdr.msg_type);
					SCLog(TRUE, LOG_INFO, CFSTR("IPSec Controller: racoon_callback,   flags = 0x%x"), serv->u.ipsec.msghdr.flags);
					SCLog(TRUE, LOG_INFO, CFSTR("IPSec Controller: racoon_callback,   cookie = 0x%x"), serv->u.ipsec.msghdr.cookie);
					SCLog(TRUE, LOG_INFO, CFSTR("IPSec Controller: racoon_callback,   reserved = 0x%x"), serv->u.ipsec.msghdr.reserved);
					SCLog(TRUE, LOG_INFO, CFSTR("IPSec Controller: racoon_callback,   result = 0x%x"), serv->u.ipsec.msghdr.result);
					SCLog(TRUE, LOG_INFO, CFSTR("IPSec Controller: racoon_callback,   len = %d"), serv->u.ipsec.msghdr.len);
#endif
              }
        }
    }

    /* first read the data part of the message */
    if (serv->u.ipsec.msglen >= sizeof(struct vpnctl_hdr)) {
//	SCLog(TRUE, LOG_INFO, CFSTR("IPSec Controller: racoon_callback, len to read = %d"), serv->u.ipsec.msgtotallen - serv->u.ipsec.msglen);

        n = readn(s, &serv->u.ipsec.msg[serv->u.ipsec.msglen], serv->u.ipsec.msgtotallen - serv->u.ipsec.msglen);
        switch (n) {
            case -1:
 //	SCLog(TRUE, LOG_INFO, CFSTR("IPSec Controller: racoon_callback, failed to read payload, closing"));
               action = do_close;
                break;
            default:
                serv->u.ipsec.msglen += n;
                if (serv->u.ipsec.msglen == serv->u.ipsec.msgtotallen) {
                    action = do_process;
                }
        }
    }

    /* perform action */
    switch (action) {
        case do_nothing:
            break;
        case do_error:
        case do_close:
            /* connection closed by client */
			SCLog(gSCNCDebug, LOG_INFO, CFSTR("IPSec Controller: connection closed by client, call ipsec_stop"));
			serv->u.ipsec.laststatus = IPSEC_GENERIC_ERROR;	
			ipsec_stop(serv, 0);
            break;

        case do_process:
            // process client request
            process_racoon_msg(serv);			
            my_Deallocate(serv->u.ipsec.msg, serv->u.ipsec.msgtotallen + 1);
            serv->u.ipsec.msg = 0;
            serv->u.ipsec.msglen = 0;
            serv->u.ipsec.msgtotallen = 0;
            break;
    }
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
static 
void event_callback(CFSocketRef inref, CFSocketCallBackType type,
                     CFDataRef address, const void *data, void *info)
{
    int 		s = CFSocketGetNative(inref);
	struct service *serv = info;
    CFRunLoopTimerContext	context = { 0, serv, NULL, NULL, NULL };
	int found;
    struct ifaddrs *ifap = NULL;

    
	char                 	buf[256], ev_if[32];
	struct kern_event_msg	*ev_msg;
	struct kev_in_data     	*inetdata;

	if (recv(s, &buf, sizeof(buf), 0) != -1) {
		ev_msg = (struct kern_event_msg *) &buf;
		inetdata = (struct kev_in_data *) &ev_msg->event_data[0];
		IPSecLogVPNInterfaceAddressEvent(__FUNCTION__, ev_msg, serv->u.ipsec.timeout_lower_interface_change, serv->u.ipsec.lower_interface, &serv->u.ipsec.our_address.sin_addr);
		switch (ev_msg->event_code) {
			case KEV_INET_NEW_ADDR:
			case KEV_INET_CHANGED_ADDR:
			case KEV_INET_ADDR_DELETED:
				snprintf(ev_if, sizeof(ev_if), "%s%d", inetdata->link_data.if_name, inetdata->link_data.if_unit);
				// check if changes occured on the interface we are using
				if (!strncmp(ev_if, serv->u.ipsec.lower_interface, sizeof(serv->u.ipsec.lower_interface))) {
					if (inetdata->link_data.if_family == APPLE_IF_FAM_PPP) {
						// disconnect immediately
						SCLog(gSCNCDebug, LOG_INFO, CFSTR("IPSec Controller: Network changed on underlying PPP interface"));
						serv->u.ipsec.laststatus = IPSEC_NETWORKCHANGE_ERROR;	
						ipsec_stop(serv, 0);
					}
					else {
						
						/* check if address still exist */
						found = 0;
						if (getifaddrs(&ifap) == 0) {
							struct ifaddrs *ifa;
							for (ifa = ifap; ifa && !found ; ifa = ifa->ifa_next) {
								found = (ifa->ifa_name  
										&& ifa->ifa_addr
										&& !strncmp(ifa->ifa_name, serv->u.ipsec.lower_interface, sizeof(serv->u.ipsec.lower_interface))
										&& ifa->ifa_addr->sa_family == AF_INET
										&& ((struct sockaddr_in *)ifa->ifa_addr)->sin_addr.s_addr == serv->u.ipsec.our_address.sin_addr.s_addr);
							}
							freeifaddrs(ifap);
						}
						
						if (found) {
							// no meaningful change, or address came back. Cancel timer if it was on.
							if (serv->u.ipsec.interface_timerref) {

								/* reinstall server route */
								switch (serv->u.ipsec.modecfg_peer_route_set) {
									case 1:
										/* host route */
										set_host_gateway(RTM_ADD, serv->u.ipsec.peer_address.sin_addr, serv->u.ipsec.lower_gateway, 0, 0);
										break;
									case 2:
										/* subnet route */
										set_host_gateway(RTM_ADD, serv->u.ipsec.peer_address.sin_addr, serv->u.ipsec.lower_gateway, serv->u.ipsec.lower_interface, 1);
										break;
								}
								
								ipsec_updatephase(serv, IPSEC_RUNNING);
								SCLog(TRUE, LOG_ERR, CFSTR("IPSec Controller: Network changed, address came back on underlying interface, cancel timer"));
								CFRunLoopRemoveTimer(CFRunLoopGetCurrent(), serv->u.ipsec.interface_timerref, kCFRunLoopCommonModes);
								my_CFRelease(&serv->u.ipsec.interface_timerref);
								if (serv->u.ipsec.phase == IPSEC_RUNNING) {
									racoon_send_cmd_start_dpd(serv->u.ipsec.controlfd, serv->u.ipsec.peer_address.sin_addr.s_addr);
								}
							}
						}
						else {
							// quick exit if there has been an unrecoverable change in interface/service
							if (IPSecCheckVPNInterfaceOrServiceUnrecoverable(gDynamicStore,
													 __FUNCTION__,
													 ev_msg,
													 serv->u.ipsec.lower_interface)) {
								SCLog(TRUE, LOG_ERR, CFSTR("IPSec Controller: the underlying interface/service has changed unrecoverably\n"));
								serv->u.ipsec.laststatus = IPSEC_NETWORKCHANGE_ERROR;   
								ipsec_stop(serv, 0);
								break;
							}

							// no address, arm timer if not there
							if (!serv->u.ipsec.interface_timerref) {
								SCLog(TRUE, LOG_ERR, CFSTR("IPSec Controller: Network changed, address disappeared on underlying interface, install timer %d secs"), serv->u.ipsec.timeout_lower_interface_change);
								serv->u.ipsec.interface_timerref = CFRunLoopTimerCreate(NULL, CFAbsoluteTimeGetCurrent() + serv->u.ipsec.timeout_lower_interface_change, FAR_FUTURE, 0, 0, event_timer, &context);
								if (!serv->u.ipsec.interface_timerref) {
									SCLog(TRUE, LOG_ERR, CFSTR("IPSec Controller: Network changed, cannot create RunLoop timer"));
									// disconnect immediately
									serv->u.ipsec.laststatus = IPSEC_NETWORKCHANGE_ERROR;	
									ipsec_stop(serv, 0);
									break;
								}
								ipsec_updatephase(serv, IPSEC_WAITING);
								CFRunLoopAddTimer(CFRunLoopGetCurrent(), serv->u.ipsec.interface_timerref, kCFRunLoopCommonModes);
							} else {
							        // transport is still down: check if there was a valid address change
							        if (IPSecCheckVPNInterfaceAddressChange(serv->u.ipsec.phase == IPSEC_WAITING /* && serv->u.ipsec.interface_timerref */,
													ev_msg,
													serv->u.ipsec.lower_interface,
													&serv->u.ipsec.our_address)) {
								        // disconnect immediately
								        SCLog(TRUE, LOG_ERR, CFSTR("IPSec Controller: the underlying interface %s address changed\n"),
									      serv->u.ipsec.lower_interface);
									serv->u.ipsec.laststatus = IPSEC_NETWORKCHANGE_ERROR;	
									ipsec_stop(serv, 0);
								}
							}
						}
					}
				} else {
				        if (IPSecCheckVPNInterfaceAddressAlternate((serv->u.ipsec.phase == IPSEC_WAITING && serv->u.ipsec.interface_timerref),
										   ev_msg,
										   serv->u.ipsec.lower_interface)) {
					        SCLog(TRUE, LOG_ERR, CFSTR("IPSec Controller: an alternative interface %s was detected while the underlying interface %s was down\n"),
						      ev_if, serv->u.ipsec.lower_interface);
						serv->u.ipsec.laststatus = IPSEC_NETWORKCHANGE_ERROR;	
						ipsec_stop(serv, 0);
					}
				}
				break;
		}
	}
	
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
static int racoon_create_socket(struct service *serv)
{
    int			flags;
    CFRunLoopSourceRef	rls;
    CFSocketContext	context = { 0, serv, NULL, NULL, NULL };
	struct sockaddr_un	sun;
        
	serv->u.ipsec.controlfd = socket(PF_LOCAL, SOCK_STREAM, 0);
	if (serv->u.ipsec.controlfd < 0) {
		SCLog(TRUE, LOG_ERR, CFSTR("IPSec Controller: cannot create racoon control socket (errno = %d) "), errno);
		goto fail;
	}

	bzero(&sun, sizeof(sun));
	sun.sun_family = AF_LOCAL;
	strncpy(sun.sun_path, "/var/run/vpncontrol.sock", sizeof(sun.sun_path));

	if (connect(serv->u.ipsec.controlfd, (struct sockaddr *)&sun, sizeof(sun)) < 0) {
		SCLog(TRUE, LOG_ERR, CFSTR("IPSec Controller: cannot connect racoon control socket (errno = %d)"), errno);
		goto fail;
	}

    if ((flags = fcntl(serv->u.ipsec.controlfd, F_GETFL)) == -1
	|| fcntl(serv->u.ipsec.controlfd, F_SETFL, flags | O_NONBLOCK) == -1) {
        SCLog(TRUE, LOG_ERR, CFSTR("IPSec Controller: Couldn't set client socket in non-blocking mode, errno = %d"), errno);
    }
        
    if ((serv->u.ipsec.controlref = CFSocketCreateWithNative(NULL, serv->u.ipsec.controlfd, 
                    kCFSocketReadCallBack, racoon_callback, &context)) == 0) {
        goto fail;
    }
    if ((rls = CFSocketCreateRunLoopSource(NULL, serv->u.ipsec.controlref, 0)) == 0)
        goto fail;

    CFRunLoopAddSource(CFRunLoopGetCurrent(), rls, kCFRunLoopDefaultMode);
    CFRelease(rls);

    return 0;
    
fail:
	if (serv->u.ipsec.controlref) {
		CFSocketInvalidate(serv->u.ipsec.controlref);
		CFRelease(serv->u.ipsec.controlref);
	}
	else 
		if (serv->u.ipsec.controlfd >= 0) {
			close(serv->u.ipsec.controlfd);
	}
	serv->u.ipsec.controlref = 0;
	serv->u.ipsec.controlfd = -1;

    return -1;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
static int event_create_socket(struct service *serv)
{
    CFRunLoopSourceRef	rls;
    CFSocketContext	context = { 0, serv, NULL, NULL, NULL };
    struct kev_request	kev_req;
        
	serv->u.ipsec.eventfd = socket(PF_SYSTEM, SOCK_RAW, SYSPROTO_EVENT);
	if (serv->u.ipsec.eventfd < 0) {
		SCLog(TRUE, LOG_ERR, CFSTR("IPSec Controller: cannot create event socket (errno = %d) "), errno);
		goto fail;
	}

	kev_req.vendor_code = KEV_VENDOR_APPLE;
	kev_req.kev_class = KEV_NETWORK_CLASS;
	kev_req.kev_subclass = KEV_INET_SUBCLASS;
	ioctl(serv->u.ipsec.eventfd, SIOCSKEVFILT, &kev_req);
        
    if ((serv->u.ipsec.eventref = CFSocketCreateWithNative(NULL, serv->u.ipsec.eventfd, 
                    kCFSocketReadCallBack, event_callback, &context)) == 0) {
        goto fail;
    }
    if ((rls = CFSocketCreateRunLoopSource(NULL, serv->u.ipsec.eventref, 0)) == 0)
        goto fail;

    CFRunLoopAddSource(CFRunLoopGetCurrent(), rls, kCFRunLoopDefaultMode);
    CFRelease(rls);

    return 0;
    
fail:
	if (serv->u.ipsec.eventref) {
		CFSocketInvalidate(serv->u.ipsec.eventref);
		CFRelease(serv->u.ipsec.eventref);
	}
	else 
		if (serv->u.ipsec.eventfd >= 0) {
			close(serv->u.ipsec.eventfd);
	}
	serv->u.ipsec.eventref = 0;
	serv->u.ipsec.eventfd = -1;

    return -1;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
static
void dns_start_query_cf_callback(CFMachPortRef port, void *msg, CFIndex size, void *info)
{
	int32_t			status;
	struct service	*serv = (struct service *)info;
	mach_port_t		mp;

	if (port != serv->u.ipsec.dnsPort) {
		// we've received a callback on the async DNS port but since the
		// associated CFMachPort doesn't match than the request must have
		// already been cancelled.
		return;
	}

	mp = CFMachPortGetPort(serv->u.ipsec.dnsPort);
	CFMachPortInvalidate(serv->u.ipsec.dnsPort);
	CFRelease(serv->u.ipsec.dnsPort);
	serv->u.ipsec.dnsPort = NULL;

	status = getaddrinfo_async_handle_reply(msg);
	if ((status == 0) &&
	    (serv->u.ipsec.resolvedAddress == NULL) && (serv->u.ipsec.resolvedAddressError == NETDB_SUCCESS)) {
		CFMachPortContext	context	= { 0, serv, NULL, NULL, NULL };
		CFRunLoopSourceRef	rls;

		// if request has been re-queued
		serv->u.ipsec.dnsPort = CFMachPortCreateWithPort(NULL, mp, dns_start_query_cf_callback, &context, NULL);
		if ( serv->u.ipsec.dnsPort ){
			rls = CFMachPortCreateRunLoopSource(NULL, serv->u.ipsec.dnsPort, 0);
			CFRunLoopAddSource(CFRunLoopGetCurrent(), rls, kCFRunLoopCommonModes);
			CFRelease(rls);
		}
		return;
	}

	return;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
static 
void dns_start_query_callback(int32_t status, struct addrinfo *res, void *context)
{
	struct service *serv		= (struct service *)context;
	struct addrinfo				*resP;
	CFDataRef					dataref;
	struct sockaddr_in			address;

	if ((status == 0) && (res != NULL)) {

		CFMutableArrayRef	addresses;
		CFRange		range; 

		addresses = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
		range.location = 0;
		range.length = 0;
		
		for (resP = res; resP; resP = resP->ai_next) {
			CFDataRef	newAddress;
			 
			/* make sure it will fit... */
			if (resP->ai_addr->sa_len > sizeof(struct sockaddr_in))
				continue; 

			newAddress = CFDataCreate(NULL, (void *)resP->ai_addr, resP->ai_addr->sa_len);
			if (!CFArrayContainsValue(addresses, range, newAddress)) {
				CFArrayAppendValue(addresses, newAddress);
				range.length++;
			}
			CFRelease(newAddress);
		}

		/* save the resolved address[es] */
		serv->u.ipsec.resolvedAddress      = addresses;
		serv->u.ipsec.resolvedAddressError = NETDB_SUCCESS;
		
		if (gSCNCDebug) {
			CFShow(CFSTR("IPSec Controller: resolvedAddress"));
			CFShow(serv->u.ipsec.resolvedAddress);
		}

		/* get the first address and start racoon */
		dataref = CFArrayGetValueAtIndex(serv->u.ipsec.resolvedAddress, serv->u.ipsec.next_address);
		bzero(&address, sizeof(address));
		range.location = 0;
		range.length = sizeof(address);
		CFDataGetBytes(dataref, range, (UInt8 *)&address); 
		serv->u.ipsec.next_address = 1;

		racoon_restart(serv, &address);
	} 
	else {
		
		SCLog(gSCNCVerbose, LOG_INFO, CFSTR("IPSec Controller: getaddrinfo() failed: %s"), gai_strerror(status));
		serv->u.ipsec.laststatus = IPSEC_RESOLVEADDRESS_ERROR;	
		ipsec_stop(serv, 0);
		/* save the error associated with the attempt to resolve the name */
		serv->u.ipsec.resolvedAddress      = CFRetain(kCFNull);
		serv->u.ipsec.resolvedAddressError = status;
	}

	if (res != NULL)	
		freeaddrinfo(res);

}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
static 
int dns_start_query(struct service *serv, char *name) 
{
	int					error;
	struct addrinfo		hints;
	mach_port_t			port;
	CFMachPortContext	context		= { 0, serv, NULL, NULL, NULL };
	CFRunLoopSourceRef	rls;

	gettimeofday(&serv->u.ipsec.dnsQueryStart, NULL);

	bzero(&hints, sizeof(hints));
	hints.ai_flags = AI_ADDRCONFIG;
	hints.ai_family = PF_INET;	/* only IPv4 */
#ifdef	AI_PARALLEL
	hints.ai_flags |= AI_PARALLEL;
#endif	/* AI_PARALLEL */

	error = getaddrinfo_async_start(&port, name, NULL, &hints,
					dns_start_query_callback, (void *)serv);
	if (error != 0) {
		/* save the error associated with the attempt to resolve the name */
		dns_start_query_callback(error, NULL, (void *)serv);
		return -1;
	}

	serv->u.ipsec.dnsPort = CFMachPortCreateWithPort(NULL, port, dns_start_query_cf_callback, &context, NULL);
	if ( serv->u.ipsec.dnsPort ){
		rls = CFMachPortCreateRunLoopSource(NULL, serv->u.ipsec.dnsPort, 0);
		CFRunLoopAddSource(CFRunLoopGetCurrent(), rls, kCFRunLoopCommonModes);
		CFRelease(rls);
	}

	return 0;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
int racoon_send_cmd_xauthinfo(int fd, u_int32_t address, struct isakmp_xauth *isakmp_array, int isakmp_nb) 
{
	u_int8_t						*data, *p; 
	struct vpnctl_cmd_xauth_info	*cmd_xauth_info;
	int								i, totlen = sizeof(struct vpnctl_cmd_xauth_info);

	for (i = 0; i < isakmp_nb; i++) {
		totlen += sizeof(u_int32_t) + (isakmp_array[i].str ? CFStringGetLength(isakmp_array[i].str) : 0);
	}

	data = malloc(totlen);
	if (!data) {
		return -1;
	}

	cmd_xauth_info = (struct vpnctl_cmd_xauth_info *)data;
	bzero(cmd_xauth_info, sizeof(struct vpnctl_cmd_xauth_info));
	cmd_xauth_info->hdr.msg_type = htons(VPNCTL_CMD_XAUTH_INFO);
	cmd_xauth_info->hdr.len = htons(totlen - sizeof(struct vpnctl_hdr));
	cmd_xauth_info->address = address;

	p = data + sizeof(struct vpnctl_cmd_xauth_info);

	for (i = 0; i < isakmp_nb; i++) {
		struct isakmp_data *d = (struct isakmp_data *)p;
		CFIndex olen, len = (isakmp_array[i].str ? CFStringGetLength(isakmp_array[i].str) : 0);
		CFRange range;
		range.location = 0;
		range.length = len;
		
		d->type = htons(isakmp_array[i].type);
		d->lorv = htons(len);
		p += sizeof(u_int32_t);
		if (len) {
			CFStringGetBytes(isakmp_array[i].str, range, kCFStringEncodingUTF8, 0, false, p, len, &olen);		
			p += len;
		}
	}

	write(fd, data, totlen);

	free(data);
	return 0;
}


/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
int racoon_send_cmd_connect(int fd, u_int32_t address) 
{
	u_int8_t					data[256]; 
	struct vpnctl_cmd_connect	*cmd_connect = (struct vpnctl_cmd_connect *)data;

	bzero(cmd_connect, sizeof(struct vpnctl_cmd_connect));
	cmd_connect->hdr.len = htons(sizeof(struct vpnctl_cmd_connect) - sizeof(struct vpnctl_hdr));
	cmd_connect->hdr.msg_type = htons(VPNCTL_CMD_CONNECT);
	cmd_connect->address = address;
	SCLog(gSCNCVerbose, LOG_INFO, CFSTR("IPSec Controller: sending CONNECT to racoon control socket"));
	write(fd, cmd_connect, sizeof(struct vpnctl_cmd_connect));
	return 0;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
int racoon_send_cmd_disconnect(int fd, u_int32_t address) 
{
	u_int8_t					data[256]; 
	struct vpnctl_cmd_connect	*cmd_connect = (struct vpnctl_cmd_connect *)data;

	bzero(cmd_connect, sizeof(struct vpnctl_cmd_connect));
	cmd_connect->hdr.len = htons(sizeof(struct vpnctl_cmd_connect) - sizeof(struct vpnctl_hdr));
	cmd_connect->hdr.msg_type = htons(VPNCTL_CMD_DISCONNECT);
	cmd_connect->address = address;
	SCLog(gSCNCVerbose, LOG_INFO, CFSTR("IPSec Controller: sending DISCONNECT to racoon control socket, address 0x%x"), ntohl(address));
	write(fd, cmd_connect, sizeof(struct vpnctl_cmd_connect));
	return 0;
}

/* -----------------------------------------------------------------------------
 ----------------------------------------------------------------------------- */
int racoon_send_cmd_start_dpd(int fd, u_int32_t address) 
{
	u_int8_t					data[256]; 
	struct vpnctl_cmd_start_dpd	*cmd_start_dpd = (struct vpnctl_cmd_start_dpd *)data;
	
	bzero(cmd_start_dpd, sizeof(struct vpnctl_cmd_start_dpd));
	cmd_start_dpd->hdr.len = htons(sizeof(struct vpnctl_cmd_start_dpd) - sizeof(struct vpnctl_hdr));
	cmd_start_dpd->hdr.msg_type = htons(VPNCTL_CMD_START_DPD);
	cmd_start_dpd->address = address;
	SCLog(gSCNCVerbose, LOG_INFO, CFSTR("IPSec Controller: sending START_DPD to racoon control socket"));
	write(fd, cmd_start_dpd, sizeof(struct vpnctl_cmd_start_dpd));
	return 0;
}


/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
int racoon_send_cmd_bind(int fd, u_int32_t address, char *version) 
{
	u_int8_t					data[256]; 
	struct vpnctl_cmd_bind	*cmd_bind = (struct vpnctl_cmd_bind *)data;
	int vers_len = 0;
	
	if (version)
		vers_len = strlen(version);
		
	bzero(cmd_bind, sizeof(struct vpnctl_cmd_bind));
	cmd_bind->hdr.len = htons(sizeof(struct vpnctl_cmd_bind) - sizeof(struct vpnctl_hdr) + vers_len);
	cmd_bind->hdr.msg_type = htons(VPNCTL_CMD_BIND);
	cmd_bind->address = address;
	cmd_bind->vers_len = htons(vers_len);
	SCLog(gSCNCVerbose, LOG_INFO, CFSTR("IPSec Controller: sending BIND to racoon control socket"));
	write(fd, cmd_bind, sizeof(struct vpnctl_cmd_bind));
	if (vers_len)
		write(fd, version, vers_len);
	return 0;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
int racoon_send_cmd_unbind(int fd, u_int32_t address) 
{
	u_int8_t					data[256]; 
	struct vpnctl_cmd_unbind	*cmd_unbind = (struct vpnctl_cmd_unbind *)data;

	bzero(cmd_unbind, sizeof(struct vpnctl_cmd_unbind));
	cmd_unbind->hdr.len = htons(sizeof(struct vpnctl_cmd_unbind) - sizeof(struct vpnctl_hdr));
	cmd_unbind->hdr.msg_type = htons(VPNCTL_CMD_UNBIND);
	cmd_unbind->address = address;
	SCLog(gSCNCVerbose, LOG_INFO, CFSTR("IPSec Controller: sending UNBIND to racoon control socket"));
	write(fd, cmd_unbind, sizeof(struct vpnctl_cmd_unbind));
	return 0;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
int racoon_send_cmd_start_ph2(int fd, u_int32_t address, CFDictionaryRef ipsec_dict) 
{
	u_int8_t	*p; 
	struct		vpnctl_cmd_start_ph2 *cmd_start_ph2 = NULL;
	struct		vpnctl_algo	*algo;
	int			policy_count;
	u_int16_t	selector_count = 0;
	int			nb, i;
    char		src_address[32], dst_address[32], str[32];
	u_int32_t	remote_address;
	CFArrayRef	policies;
	CFIndex		start, end;
	size_t		bufsiz, msglen;
	char		*errstr = '\0';
	
#define NB_ALGOS 6
	
	policy_count = IPSecCountPolicies(ipsec_dict);
	if (policy_count <= 0) {
		errstr = "cannot create ph2 config - no policies found";
		goto fail;
	}
			
	/* allocate max space needed - adjust len later */
	bufsiz = sizeof(struct vpnctl_cmd_start_ph2) + ((policy_count * sizeof(struct vpnctl_sa_selector)) * 2) + (NB_ALGOS * sizeof(struct vpnctl_algo));
	cmd_start_ph2 = malloc(bufsiz);
	if (cmd_start_ph2 == NULL) {
		errstr = "out of memory";
		goto fail;
	}
	
	bzero(cmd_start_ph2, bufsiz);
	cmd_start_ph2->hdr.msg_type = htons(VPNCTL_CMD_START_PH2);
	cmd_start_ph2->address = address;

	/* XXX let use default for now */
	cmd_start_ph2->lifetime = htonl(3600);
	cmd_start_ph2->selector_count = 0;
	cmd_start_ph2->algo_count = htons(NB_ALGOS);
		    
	if (!GetStrAddrFromDict(ipsec_dict, kRASPropIPSecLocalAddress, src_address, sizeof(src_address))) {
		errstr = "incorrect local address";
		goto fail;
	}
	
	if (!GetStrAddrFromDict(ipsec_dict, kRASPropIPSecRemoteAddress, dst_address, sizeof(dst_address))) {
		errstr = "incorrect remote address";
		goto fail;
	}
	
	if (!inet_aton(dst_address, (struct in_addr *)&remote_address)) {
		errstr = "invalid remote address";
		goto fail;
	}
	
	if (remote_address != address) {
		errstr = "remote address mismatch";
		goto fail;
	}
	
	policies = CFDictionaryGetValue(ipsec_dict, kRASPropIPSecPolicies);
	if (!isArray(policies) || (nb = CFArrayGetCount(policies)) == 0) {
		errstr = "no policy found";
		goto fail;
	}
	
	start = 0;
	end = nb;

	p = ((u_int8_t *)(cmd_start_ph2)) + sizeof(struct vpnctl_cmd_start_ph2);

	for (i = start; i < end; i++) {
		
		int				tunnel, in, out;
		CFDictionaryRef policy;
		CFStringRef		policymode, policydirection, policylevel;
		struct			in_addr		local_net_address, remote_net_address;
		u_int16_t		local_net_port, remote_net_port;
		u_int32_t		local_prefix, remote_prefix, local_mask, remote_mask;
		u_int32_t		protocol;
		struct			vpnctl_sa_selector *sa;

		protocol = 0xFF;
		
		policy = CFArrayGetValueAtIndex(policies, i);
		if (!isDictionary(policy)) {
			errstr = "incorrect policy found";
			goto fail;
		}
		
		/* build policies in and out */
		
		tunnel = 1;
		policymode = CFDictionaryGetValue(policy, kRASPropIPSecPolicyMode);
		if (isString(policymode)) {
			
			if (CFEqual(policymode, kRASValIPSecPolicyModeTunnel))
				tunnel = 1;
			else if (CFEqual(policymode, kRASValIPSecPolicyModeTransport))
				tunnel = 0;
			else {
				errstr = "incorrect policy type found";
				goto fail;
			}
		}
		
		/* if policy direction is not specified, in/out is assumed */
		in = out = 1;
		policydirection = CFDictionaryGetValue(policy, kRASPropIPSecPolicyDirection);
		if (isString(policydirection)) {
			
			if (CFEqual(policydirection, kRASValIPSecPolicyDirectionIn))
				out = 0;
			else if (CFEqual(policydirection, kRASValIPSecPolicyDirectionOut))
				in = 0;
			else if (!CFEqual(policydirection, kRASValIPSecPolicyDirectionInOut)) {
				errstr = "incorrect policy direction found";
				goto fail;
			}
		}
		
		policylevel = CFDictionaryGetValue(policy, kRASPropIPSecPolicyLevel);		
		if (!isString(policylevel) || 
			(!CFEqual(policylevel, kRASValIPSecPolicyLevelUnique) && !CFEqual(policylevel, kRASValIPSecPolicyLevelRequire)))
			continue;
		
		if (tunnel) {
			
			int j;
			
			/* get local and remote networks */
			
			if (!GetStrAddrFromDict(policy, kRASPropIPSecPolicyLocalAddress, str, sizeof(str))) {
				errstr = "incorrect local network";
				goto fail;
			}
			
			local_net_port = htons(0);
			if (!inet_aton(str, &local_net_address)) {
				errstr = "incorrect local network";
				goto fail;
			}
			
			GetIntFromDict(policy, kRASPropIPSecPolicyLocalPrefix, &local_prefix, 32);
			for (j = 0, local_mask = 0; j < local_prefix; j++)
				local_mask = (local_mask >> 1) | 0x80000000;
			
			if (!GetStrAddrFromDict(policy, kRASPropIPSecPolicyRemoteAddress, str, sizeof(str))) {
				errstr = "incorrect remote network";
				goto fail;
			}
			
			remote_net_port = htons(0);
			if (!inet_aton(str, &remote_net_address)) {
				errstr = "incorrect remote network";
				goto fail;
			}
			
			GetIntFromDict(policy, kRASPropIPSecPolicyRemotePrefix, &remote_prefix, 32);
			for (j = 0, remote_mask = 0; j < remote_prefix; j++)
				remote_mask = (remote_mask >> 1) | 0x80000000;

		}
		else {
			u_int32_t val;
			
			GetIntFromDict(policy, kRASPropIPSecPolicyLocalPort, &val, 0);
			local_net_port = htons(val);
			if (!inet_aton(src_address, &local_net_address)) {
				errstr = "incorrect local address";
				goto fail;
			}
			
			local_mask = local_net_address.s_addr ? 0xFFFFFFFF : 0;
			
			GetIntFromDict(policy, kRASPropIPSecPolicyRemotePort, &val, 0);
			remote_net_port = htons(val);
			if (!inet_aton(dst_address, &remote_net_address)) {
				errstr = "incorrect remote address";
				goto fail;
			}
			
			remote_mask = remote_net_address.s_addr ? 0xFFFFFFFF : 0;
			
			GetIntFromDict(policy, kRASPropIPSecPolicyProtocol, &protocol, 0);
			
		}
		
		/* setup phase2 command */
		
		if (out) {
			sa = (struct vpnctl_sa_selector *)p;
			sa->src_tunnel_address = local_net_address.s_addr;
			sa->src_tunnel_mask = htonl(local_mask);
			sa->src_tunnel_port = local_net_port;
			sa->dst_tunnel_address = remote_net_address.s_addr;
			sa->dst_tunnel_mask = htonl(remote_mask);
			sa->dst_tunnel_port = remote_net_port;
			sa->ul_protocol = htons((u_int16_t)protocol);
			selector_count++;
			p += sizeof(struct vpnctl_sa_selector);
		}
				
		if (in) {
			sa = (struct vpnctl_sa_selector *)p;
			sa->dst_tunnel_address = local_net_address.s_addr;
			sa->dst_tunnel_mask = htonl(local_mask);
			sa->dst_tunnel_port = local_net_port;
			sa->src_tunnel_address = remote_net_address.s_addr;
			sa->src_tunnel_mask = htonl(remote_mask);
			sa->src_tunnel_port = remote_net_port;
			sa->ul_protocol = htons((u_int16_t)protocol);
			selector_count++;
			p += sizeof(struct vpnctl_sa_selector);
		}
	}
	cmd_start_ph2->selector_count = htons(selector_count);
	
	algo = (struct vpnctl_algo *)p;
	algo->algo_class = htons(algclass_ipsec_enc);
	algo->algo = htons(algtype_aes);
	algo->key_len = htons(256);
	
	p += sizeof(struct vpnctl_algo);
	algo = (struct vpnctl_algo *)p;
	algo->algo_class = htons(algclass_ipsec_enc);
	algo->algo = htons(algtype_aes);
	algo->key_len = htons(0);

	p += sizeof(struct vpnctl_algo);
	algo = (struct vpnctl_algo *)p;
	algo->algo_class = htons(algclass_ipsec_enc);
	algo->algo = htons(algtype_3des);
	algo->key_len = htons(0);
	
	p += sizeof(struct vpnctl_algo);
	algo = (struct vpnctl_algo *)p;
	algo->algo_class = htons(algclass_ipsec_auth);
	algo->algo = htons(algtype_hmac_sha1);
	algo->key_len = htons(0);

	p += sizeof(struct vpnctl_algo);
	algo = (struct vpnctl_algo *)p;
	algo->algo_class = htons(algclass_ipsec_auth);
	algo->algo = htons(algtype_hmac_md5);
	algo->key_len = htons(0);

	p += sizeof(struct vpnctl_algo);
	algo = (struct vpnctl_algo *)p;
	algo->algo_class = htons(algclass_ipsec_comp);
	algo->algo = htons(algtype_deflate);
	algo->key_len = htons(0);
	
	msglen = sizeof(struct vpnctl_cmd_start_ph2) +
					(selector_count * sizeof(struct vpnctl_sa_selector)) +
					(NB_ALGOS * sizeof(struct vpnctl_algo));
	cmd_start_ph2->hdr.len = htons(msglen - sizeof(struct vpnctl_hdr));

	SCLog(gSCNCVerbose, LOG_INFO, CFSTR("IPSec Controller: sending START_PH2 to racoon control socket"));
	write(fd, cmd_start_ph2, msglen);

	free(cmd_start_ph2);
	return 0;
	
fail:
	SCLog(TRUE, LOG_ERR, CFSTR("IPSec Controller: failed to start phase2 - '%s'"), errstr);
	if (cmd_start_ph2)
		free(cmd_start_ph2);
	return -1;
}


/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
static 
int racoon_restart(struct service *serv, struct sockaddr_in *address)
{
	char			*errorstr;
	int				error, need_kick = 0, found;
	CFStringRef		string, key;
    CFRunLoopTimerContext	context = { 0, serv, NULL, NULL, NULL };
	CFDictionaryRef userdict, dict;
	CFTypeRef		value;

	SCLog(gSCNCVerbose, LOG_INFO, CFSTR("IPSec Controller: racoon_restart..."));

	/* unconfigure ipsec first */
	if (serv->u.ipsec.policies_installed) {
		error = IPSecRemovePolicies(serv->u.ipsec.config, -1, &errorstr);
		if (error)
			SCLog(TRUE, LOG_ERR, CFSTR("IPSec Controller: Cannot remove policies, error '%s'"), errorstr);
		
		IPSecRemoveSecurityAssociations((struct sockaddr *)&serv->u.ipsec.our_address, (struct sockaddr *)&serv->u.ipsec.peer_address);
		serv->u.ipsec.policies_installed = 0;
	}
	uninstall_mode_config(serv);

	if (serv->u.ipsec.config_applied) {
		// just remove the file, as will kill kick racoon with the config of the load balancing adress
		// this is to avoid useless sighup of racoon
		error = IPSecRemoveConfigurationFile(serv->u.ipsec.config, &errorstr);
		if (error)
			SCLog(TRUE, LOG_ERR, CFSTR("IPSec Controller: Cannot remove configuration, error '%s'"), errorstr);
		serv->u.ipsec.config_applied = 0;
		need_kick = 1;
	}

	serv->u.ipsec.ping_count = 0;

	/* then try new address */
	bcopy(address, &serv->u.ipsec.peer_address, sizeof(serv->u.ipsec.peer_address));

	if (get_src_address((struct sockaddr *)&serv->u.ipsec.our_address, (struct sockaddr *)&serv->u.ipsec.peer_address, 0)) {
		serv->u.ipsec.laststatus = IPSEC_NOLOCALNETWORK_ERROR;
		SCLog(TRUE, LOG_ERR, CFSTR("IPSec Controller: cannot get our local address..."));
		goto fail;
	}

    serv->u.ipsec.lower_interface[0] = 0;
	bzero(&serv->u.ipsec.lower_gateway, sizeof(serv->u.ipsec.lower_gateway));
	
	/* XXX this part should be change to grab the real lower interface instead of the primary */
    key = SCDynamicStoreKeyCreateNetworkGlobalEntity(NULL, kSCDynamicStoreDomainState, kSCEntNetIPv4);
    if (key) {
        dict = SCDynamicStoreCopyValue(gDynamicStore, key);
		CFRelease(key);
        if (dict) {
            if (string  = CFDictionaryGetValue(dict, kSCDynamicStorePropNetPrimaryInterface))
                CFStringGetCString(string, serv->u.ipsec.lower_interface, sizeof(serv->u.ipsec.lower_interface), kCFStringEncodingUTF8);
			if (string  = CFDictionaryGetValue(dict, kSCPropNetIPv4Router)) {
				u_int8_t routeraddress[16];
				routeraddress[0] = 0;
				CFStringGetCString(string, (char*)routeraddress, sizeof(routeraddress), kCFStringEncodingUTF8);
				if (routeraddress[0]) 
					inet_aton((char*)routeraddress, &serv->u.ipsec.lower_gateway);
			}
            CFRelease(dict);
        }
    }

#ifndef TARGET_EMBEDDED_OS
    serv->u.ipsec.lower_interface_media = get_if_media(serv->u.ipsec.lower_interface);
    serv->u.ipsec.timeout_lower_interface_change = get_interface_timeout(serv->u.ipsec.lower_interface_media);
#else
    serv->u.ipsec.timeout_lower_interface_change = get_interface_timeout(0);
#endif /* !iPhone */    
	
	/* if retry, then change src/dst address, otherwise create the config */
	if (!serv->u.ipsec.config) {
	
		CFStringRef auth_method;
		auth_method = CFDictionaryGetValue(serv->systemprefs, kRASPropIPSecAuthenticationMethod);
		if (!isString(auth_method) || CFEqual(auth_method, kRASValIPSecAuthenticationMethodSharedSecret)) {
			auth_method = kRASValIPSecAuthenticationMethodSharedSecret;

		}
		else if (CFEqual(auth_method, kRASValIPSecAuthenticationMethodCertificate)) {
			serv->flags |= FLAG_USECERTIFICATE;
#if 0
			/* need to check for share secret at some point in the path */
			certificate = CFDictionaryGetValue(useripsec_dict, kRASPropIPSecLocalCertificate);
			if (!isData(certificate)) {
				devstatus = EXIT_L2TP_NOCERTIFICATE;
				error("L2TP: no user certificate  found.\n");
				goto fail;
			}
#endif
		}
		
		CFStringRef verify_id;
		verify_id = CFDictionaryGetValue(serv->systemprefs, kRASPropIPSecIdentifierVerification);
		if (verify_id && !isString(verify_id)) {
#if 0
			/* need to check correct value */
			error("L2TP: incorrect identifier verification found.\n");
			goto fail;
#endif
		}


		/* now create the default config */
		CFStringRef remote_address = CFDictionaryGetValue(serv->systemprefs, kRASPropIPSecRemoteAddress);

		serv->u.ipsec.config = IPSecCreateCiscoDefaultConfiguration((struct sockaddr *)&serv->u.ipsec.our_address, 
			(struct sockaddr *)&serv->u.ipsec.peer_address, cfstring_is_ip(remote_address) ? NULL : remote_address, auth_method, 
			1, 0, verify_id);
		if (!serv->u.ipsec.config) {
			SCLog(TRUE, LOG_ERR, CFSTR("IPSec Controller: cannot create IPSec dictionary..."));
			goto fail;
		}
		
		// by default, want to configure idle time out when connecting OnDemand
		if (serv->flags & FLAG_ONDEMAND) {
			CFNumberRef num;
			int val;

			val = 1;
			if (num = CFNumberCreate(NULL, kCFNumberIntType, &val)) {
				CFDictionarySetValue(serv->u.ipsec.config, kRASPropIPSecDisconnectOnIdle, num);
				CFRelease(num);
			}
			val = 120; // 2 minutes by default
			if (num = CFNumberCreate(NULL, kCFNumberIntType, &val)) {
				CFDictionarySetValue(serv->u.ipsec.config, kRASPropIPSecDisconnectOnIdleTimer, num);
				CFRelease(num);
			}
		}
			
		/* merge the system config prefs into the default dictionary */
		CFDictionaryApplyFunction(serv->systemprefs, merge_ipsec_dict, serv->u.ipsec.config);

		if (serv->connectopts && (userdict = CFDictionaryGetValue(serv->connectopts, kSCEntNetIPSec))) {
			
			value = CFDictionaryGetValue(userdict, kRASPropIPSecXAuthName);
			if (isString(value))
				CFDictionarySetValue(serv->u.ipsec.config, kRASPropIPSecXAuthName, value);
			
			value = CFDictionaryGetValue(userdict, kRASPropIPSecXAuthPassword);
			if (isString(value))
				CFDictionarySetValue(serv->u.ipsec.config, kRASPropIPSecXAuthPassword, value);
		}


		/* Change to Hybrid authentication if applicable */
		#define IPSecLocalIdentifierHybridSuffix	CFSTR("[hybrid]")
		value = CFDictionaryGetValue(serv->u.ipsec.config, kRASPropIPSecLocalIdentifier);
		if (isString(value) && CFStringHasSuffix(value, IPSecLocalIdentifierHybridSuffix)) {
			CFStringRef value1 = CFStringCreateWithSubstring(NULL, value, CFRangeMake(0, CFStringGetLength(value) - CFStringGetLength(IPSecLocalIdentifierHybridSuffix)));
			if (value1) {
				CFDictionarySetValue(serv->u.ipsec.config, kRASPropIPSecLocalIdentifier, value1);
				CFDictionarySetValue(serv->u.ipsec.config, kRASPropIPSecAuthenticationMethod, kRASValIPSecAuthenticationMethodHybrid);
				CFRelease(value1);
			}
		}

		/* Complete shared secret, with defaut convention "serviceID.SS" if necessary */
		if (CFEqual(auth_method, kRASValIPSecAuthenticationMethodSharedSecret)) {
			
			found = 0;
			/* first check is secret is available in the prefs */
			CFStringRef secret_string = CFDictionaryGetValue(serv->u.ipsec.config, kRASPropIPSecSharedSecret);
			if (isString(secret_string)) {
				/* secret or secret keychain is in the prefs, leave it alone */
				found = 1;
			}
			else {
				/* encryption is not specified, and secret was missing. check connect options */
				if (serv->connectopts) {
					CFStringRef secret_string = CFDictionaryGetValue(userdict, kRASPropIPSecSharedSecret);
					if (isString(secret_string)) {
						found = 1;
						CFDictionarySetValue(serv->u.ipsec.config, kRASPropIPSecSharedSecret, secret_string);
						CFDictionaryRemoveValue(serv->u.ipsec.config, kRASPropIPSecSharedSecretEncryption);
					}
				}
				else {
					/* secret is not in the prefs, check for encryption pref */
					CFStringRef secret_encryption_string = CFDictionaryGetValue(serv->u.ipsec.config, kRASPropIPSecSharedSecretEncryption);
					if (my_CFEqual(secret_encryption_string, kRASValIPSecSharedSecretEncryptionKeychain)) {
						/* encryption is KeyChain. Create a default secret key for the key chain */
						secret_string = CFStringCreateWithFormat(NULL, NULL, CFSTR("%@.SS"), serv->serviceID);
						if (secret_string) {
							found = 1;
							CFDictionarySetValue(serv->u.ipsec.config, kRASPropIPSecSharedSecret, secret_string);
							CFRelease(secret_string);
						}
					}
				}
			} 
			
			if (!found) {
				serv->u.ipsec.laststatus = IPSEC_NOSHAREDSECRET_ERROR;
				SCLog(TRUE, LOG_ERR, CFSTR("IPSec Controller: incorrect shared secret found."));
				goto fail;
			}
			
		}
	}
	else {
		string = CFStringCreateWithCString(0, addr2ascii(AF_INET, &serv->u.ipsec.peer_address.sin_addr, sizeof(serv->u.ipsec.peer_address.sin_addr), 0), kCFStringEncodingASCII);
		if (string) {
			CFDictionarySetValue(serv->u.ipsec.config, kRASPropIPSecLocalAddress, string);
			CFRelease(string);
		}
		string = CFStringCreateWithCString(0, addr2ascii(AF_INET, &serv->u.ipsec.peer_address.sin_addr, sizeof(serv->u.ipsec.peer_address.sin_addr), 0), kCFStringEncodingASCII);
		if (string) {
			CFDictionarySetValue(serv->u.ipsec.config, kRASPropIPSecRemoteAddress, string);
			CFRelease(string);
		}
	}
	
	if (gSCNCDebug) {
		CFShow(CFSTR("IPSec Controller: Complete IPsec dictionary"));
		CFShow(serv->u.ipsec.config);
	}

	error = IPSecApplyConfiguration(serv->u.ipsec.config, &errorstr);
	if (error) {
		SCLog(TRUE, LOG_ERR, CFSTR("IPSec Controller: Cannot apply configuration, error '%s'"), errorstr);
		serv->u.ipsec.laststatus = IPSEC_CONFIGURATION_ERROR;
		goto fail;
	}

	serv->u.ipsec.config_applied = 1;
	need_kick = 0;

	/* Install policies and trigger racoon */
	if (IPSecCountPolicies(serv->u.ipsec.config)) {
		error = IPSecInstallPolicies(serv->u.ipsec.config, -1, &errorstr);
		if (error) {
			SCLog(TRUE, LOG_ERR, CFSTR("IPSec Controller: Cannot install policies, error '%s'"), errorstr);
			serv->u.ipsec.laststatus = IPSEC_CONFIGURATION_ERROR;
			goto fail;
		}

		serv->u.ipsec.policies_installed = 1;
	}
	
	/* open and connect to the racoon control socket */
	if (serv->u.ipsec.controlfd == -1) {
		if (racoon_create_socket(serv) < 0) {
			SCLog(TRUE, LOG_ERR, CFSTR("IPSec Controller: cannot create racoon control socket"));
			serv->u.ipsec.laststatus = IPSEC_RACOONCONTROL_ERROR;
			goto fail;
		}
	}
	else {
		racoon_send_cmd_unbind(serv->u.ipsec.controlfd, htonl(0xFFFFFFFF));
	}
		
	racoon_send_cmd_bind(serv->u.ipsec.controlfd, serv->u.ipsec.peer_address.sin_addr.s_addr, gIPSecAppVersion);
	racoon_send_cmd_connect(serv->u.ipsec.controlfd, serv->u.ipsec.peer_address.sin_addr.s_addr);
	
	if (!serv->u.ipsec.timerref) {
		serv->u.ipsec.timerref = CFRunLoopTimerCreate(NULL, CFAbsoluteTimeGetCurrent() + TIMEOUT_INITIAL_CONTACT, FAR_FUTURE, 0, 0, racoon_timer, &context);
		if (!serv->u.ipsec.timerref) {
			SCLog(TRUE, LOG_ERR, CFSTR("IPSec Controller: cannot create RunLoop timer"));
			goto fail;
		}
		CFRunLoopAddTimer(CFRunLoopGetCurrent(), serv->u.ipsec.timerref, kCFRunLoopCommonModes);
	}
	else {
		CFRunLoopTimerSetNextFireDate(serv->u.ipsec.timerref, CFAbsoluteTimeGetCurrent() + TIMEOUT_INITIAL_CONTACT);
	}
	return 0;

fail:
	if (serv->u.ipsec.laststatus == IPSEC_NO_ERROR)
		serv->u.ipsec.laststatus = IPSEC_GENERIC_ERROR;
	SCLog(TRUE, LOG_ERR, CFSTR("IPSec Controller: restart failed"));
	if (need_kick)
		IPSecKickConfiguration();
	ipsec_stop(serv, 0);
    return serv->u.ipsec.laststatus;
}

#if 0
/* -----------------------------------------------------------------------------
    add/remove a route via a gateway
----------------------------------------------------------------------------- */
static int
route_gateway(int cmd, struct in_addr dest, struct in_addr mask, struct in_addr gateway, int use_gway_flag)
{
    int 			len;
    int 			rtm_seq = 0;

    struct {
	struct rt_msghdr	hdr;
	struct sockaddr_in	dst;
        struct sockaddr_in	gway;
        struct sockaddr_in	mask;
    } rtmsg;
   
    int 			sockfd = -1;
    
    if ((sockfd = socket(PF_ROUTE, SOCK_RAW, AF_INET)) < 0) {
	syslog(LOG_INFO, "host_gateway: open routing socket failed, %s",
	       strerror(errno));
	return (0);
    }

    memset(&rtmsg, 0, sizeof(rtmsg));
    rtmsg.hdr.rtm_type = cmd;
    rtmsg.hdr.rtm_flags = RTF_UP | RTF_STATIC;
    if (use_gway_flag)
        rtmsg.hdr.rtm_flags |= RTF_GATEWAY;
    rtmsg.hdr.rtm_version = RTM_VERSION;
    rtmsg.hdr.rtm_seq = ++rtm_seq;
    rtmsg.hdr.rtm_addrs = RTA_DST | RTA_NETMASK | RTA_GATEWAY;
    rtmsg.dst.sin_len = sizeof(rtmsg.dst);
    rtmsg.dst.sin_family = AF_INET;
    rtmsg.dst.sin_addr = dest;
    rtmsg.gway.sin_len = sizeof(rtmsg.gway);
    rtmsg.gway.sin_family = AF_INET;
    rtmsg.gway.sin_addr = gateway;
    rtmsg.mask.sin_len = sizeof(rtmsg.mask);
    rtmsg.mask.sin_family = AF_INET;
    rtmsg.mask.sin_addr = mask;

    len = sizeof(rtmsg);
    rtmsg.hdr.rtm_msglen = len;
    if (write(sockfd, &rtmsg, len) < 0) {
	syslog(LOG_ERR, "host_gateway: write routing socket failed, %s", strerror(errno));
	close(sockfd);
	return (0);
    }

    close(sockfd);
    return (1);
}
#endif

/* -----------------------------------------------------------------------------
add/remove a host route
----------------------------------------------------------------------------- */
static boolean_t
set_host_gateway(int cmd, struct in_addr host, struct in_addr gateway, char *ifname, int isnet)
{
    int 			len;
    int 			rtm_seq = 0;
    struct {
	struct rt_msghdr	hdr;
	struct sockaddr_in	dst;
	struct sockaddr_in	gway;
	struct sockaddr_in	mask;
	struct sockaddr_dl	link;
    } 				rtmsg;
    int 			sockfd = -1;

    if ((sockfd = socket(PF_ROUTE, SOCK_RAW, AF_INET)) < 0) {
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
    if (gateway.s_addr)
        rtmsg.hdr.rtm_flags |= RTF_GATEWAY;
    rtmsg.hdr.rtm_version = RTM_VERSION;
    rtmsg.hdr.rtm_seq = ++rtm_seq;
    rtmsg.hdr.rtm_addrs = RTA_DST | RTA_NETMASK;
    rtmsg.dst.sin_len = sizeof(rtmsg.dst);
    rtmsg.dst.sin_family = AF_INET;
    rtmsg.dst.sin_addr = host;
    rtmsg.hdr.rtm_addrs |= RTA_GATEWAY;
    rtmsg.gway.sin_len = sizeof(rtmsg.gway);
    rtmsg.gway.sin_family = AF_INET;
    rtmsg.gway.sin_addr = gateway;
    rtmsg.mask.sin_len = sizeof(rtmsg.mask);
    rtmsg.mask.sin_family = AF_INET;
    rtmsg.mask.sin_addr.s_addr = 0xFFFFFFFF;

    len = sizeof(rtmsg);
    if (ifname) {
	rtmsg.link.sdl_len = sizeof(rtmsg.link);
	rtmsg.link.sdl_family = AF_LINK;
	rtmsg.link.sdl_nlen = MIN(strlen(ifname), sizeof(rtmsg.link.sdl_data));
	rtmsg.hdr.rtm_addrs |= RTA_IFP;
	bcopy(ifname, rtmsg.link.sdl_data, rtmsg.link.sdl_nlen);
    }
    else {
	/* no link information */
	len -= sizeof(rtmsg.link);
    }
    rtmsg.hdr.rtm_msglen = len;
    if (write(sockfd, &rtmsg, len) < 0) {
		//SCLog(TRUE, LOG_ERR, CFSTR("IPSec Controller: host_gateway: write routing socket failed, %s"), strerror(errno));
	close(sockfd);
	return (FALSE);
    }

    close(sockfd);
    return (TRUE);
}


#ifdef TARGET_EMBEDDED_OS
/* -----------------------------------------------------------------------------
 Bring up EDGE
 ----------------------------------------------------------------------------- */

/* -----------------------------------------------------------------------------
 ----------------------------------------------------------------------------- */
static 
void edge_timer(CFRunLoopTimerRef timer, void *info)
{
	struct service *serv = info;
	
	my_CFRelease(&serv->u.ipsec.edge_timerref);
	racoon_resolve(serv);	
}


static 
void callbackEDGE(CTServerConnectionRef connection, CFStringRef notification, CFDictionaryRef notificationInfo, void* info) {
	
	struct service	*serv = (struct service *)info;
	SInt32 which;
    CFRunLoopTimerContext	timer_ctxt = { 0, serv, NULL, NULL, NULL };
	
	// You only want to act on notifications for the main PDP context.
	if (CFNumberGetValue((CFNumberRef)CFDictionaryGetValue(notificationInfo, kCTRegistrationDataContextID), kCFNumberSInt32Type, &which) && (which == 0)) {		
		
		if (CFEqual(notification, kCTRegistrationDataStatusChangedNotification)) {
			if (CFEqual(CFDictionaryGetValue(notificationInfo, kCTRegistrationDataActive), kCFBooleanTrue)) {
				// It's good to go.  Tear down everything and run.
				SCLog(gSCNCVerbose, LOG_INFO, CFSTR("IPSec Controller: callbackEDGE activation succeeded"));
				CFRunLoopRemoveSource(CFRunLoopGetCurrent(), serv->u.ipsec.edgeRLS, kCFRunLoopCommonModes);
				my_CFRelease(&serv->u.ipsec.edgeRLS);			
				CFMachPortInvalidate(serv->u.ipsec.edgePort);
				my_CFRelease(&serv->u.ipsec.edgePort);			
				my_CFRelease(&serv->u.ipsec.edgeConnection);

				serv->u.ipsec.edge_timerref = CFRunLoopTimerCreate(NULL, CFAbsoluteTimeGetCurrent() + TIMEOUT_EDGE, FAR_FUTURE, 0, 0, edge_timer, &timer_ctxt);
				if (!serv->u.ipsec.edge_timerref) {
					SCLog(TRUE, LOG_ERR, CFSTR("IPSec Controller: callback EDGE, cannot create RunLoop timer"));
					// disconnect immediately
					serv->u.ipsec.laststatus = IPSEC_EDGE_ACTIVATION_ERROR;	
					ipsec_stop(serv, 0);
					return;
				}
				CFRunLoopAddTimer(CFRunLoopGetCurrent(), serv->u.ipsec.edge_timerref, kCFRunLoopCommonModes);
			}
		}
		else {
			// Failed.  Tear down everything.
			SCLog(gSCNCVerbose, LOG_INFO, CFSTR("IPSec Controller: callbackEDGE activation failed"));
			serv->u.ipsec.laststatus = IPSEC_EDGE_ACTIVATION_ERROR;	
			ipsec_stop(serv, 0);
		}
	}
}

static 
void _ServerConnectionHandleReply(CFMachPortRef port, void *msg, CFIndex size, void *info) {
    
    (void)port;	    /* Unused */
    (void)size;	    /* Unused */
    (void)info;	    /* Unused */
    
    _CTServerConnectionHandleReply(msg);
}

static
int BringUpEDGE(struct service *serv)
{
	_CTServerConnectionContext ctxt = { 0, serv, NULL, NULL, NULL };
	CFMachPortContext mach_ctxt = { 0, serv, NULL, NULL, NULL };
    CFRunLoopTimerContext	timer_ctxt = { 0, serv, NULL, NULL, NULL };
	CTError error = { kCTErrorDomainNoError, 0 };
	Boolean active = FALSE;
	
	serv->u.ipsec.edgeConnection = _CTServerConnectionCreate(kCFAllocatorDefault, callbackEDGE, &ctxt);
	if (!serv->u.ipsec.edgeConnection)
		goto fail;
	
	error = _CTServerConnectionGetPacketContextActive(serv->u.ipsec.edgeConnection, 0, &active);
	
	if (error.error)
		goto fail;
	
	SCLog(gSCNCVerbose, LOG_INFO, CFSTR("IPSec Controller: BringUpEDGE active = %d"), active);

	if (active) {
		my_CFRelease(&serv->u.ipsec.edgeConnection);
		serv->u.ipsec.edge_timerref = CFRunLoopTimerCreate(NULL, CFAbsoluteTimeGetCurrent() + TIMEOUT_EDGE, FAR_FUTURE, 0, 0, edge_timer, &timer_ctxt);
		if (!serv->u.ipsec.edge_timerref) {
			SCLog(TRUE, LOG_ERR, CFSTR("IPSec Controller: BringUpEDGE, cannot create RunLoop timer"));
			// disconnect immediately
			serv->u.ipsec.laststatus = IPSEC_EDGE_ACTIVATION_ERROR;	
			ipsec_stop(serv, 0);
			return 0;
		}
		CFRunLoopAddTimer(CFRunLoopGetCurrent(), serv->u.ipsec.edge_timerref, kCFRunLoopCommonModes);
		return 1; // timer active, pretend connection in progress
	}
	
	error = _CTServerConnectionRegisterForNotification(serv->u.ipsec.edgeConnection, kCTRegistrationDataStatusChangedNotification);
	if (error.error == 0)
		error = _CTServerConnectionRegisterForNotification(serv->u.ipsec.edgeConnection, kCTRegistrationDataActivateFailedNotification);
	if (error.error == 0)
		error = _CTServerConnectionSetPacketContextActive(serv->u.ipsec.edgeConnection, 0, TRUE);		// Zero is the main PDP context.
	
	if (error.error)
		goto fail;
	
	serv->u.ipsec.edgePort = CFMachPortCreateWithPort(NULL, _CTServerConnectionGetPort(serv->u.ipsec.edgeConnection), (CFMachPortCallBack)_ServerConnectionHandleReply, &mach_ctxt, NULL);
	serv->u.ipsec.edgeRLS = CFMachPortCreateRunLoopSource(NULL, serv->u.ipsec.edgePort, 0);
	CFRunLoopAddSource(CFRunLoopGetCurrent(), serv->u.ipsec.edgeRLS, kCFRunLoopCommonModes);
	
	return 1; // connection in progress
	
fail:
	SCLog(TRUE, LOG_ERR, CFSTR("IPSec Controller: BringUpEDGE cannot get edge status, error = %d"), error.error);
	my_CFRelease(&serv->u.ipsec.edgeConnection);
	return 0; // connection failed
}

#endif

/* -----------------------------------------------------------------------------
 ----------------------------------------------------------------------------- */
int racoon_resolve(struct service *serv)
{
	char			remoteaddress[255];

	GetStrFromDict (serv->systemprefs, kRASPropIPSecRemoteAddress, remoteaddress, sizeof(remoteaddress), "");

	serv->u.ipsec.peer_address.sin_len = sizeof(serv->u.ipsec.peer_address);
	serv->u.ipsec.peer_address.sin_family = AF_INET;
	serv->u.ipsec.peer_address.sin_port = htons(0);
	if (!inet_aton(remoteaddress, &serv->u.ipsec.peer_address.sin_addr)) {
		if (dns_start_query(serv, remoteaddress)) {
			serv->u.ipsec.laststatus = IPSEC_RESOLVEADDRESS_ERROR;	
			goto fail;
		}
		return 0;
	}
	
	return racoon_restart(serv, &serv->u.ipsec.peer_address);
	
fail:
	if (serv->u.ipsec.laststatus == IPSEC_NO_ERROR)
		serv->u.ipsec.laststatus = IPSEC_GENERIC_ERROR;	

	ipsec_stop(serv, 0);
    return serv->u.ipsec.laststatus;
}

/* -----------------------------------------------------------------------------
 ----------------------------------------------------------------------------- */
int ipsec_start(struct service *serv, CFDictionaryRef options, uid_t uid, gid_t gid, mach_port_t bootstrap, u_int8_t onTraffic, u_int8_t onDemand)
{
	char			remoteaddress[255];
	
	SCLog(gSCNCVerbose, LOG_INFO, CFSTR("IPSec Controller: ipsec_start"));
	
    switch (serv->u.ipsec.phase) {
        case IPSEC_IDLE:
            break;
			
        default:
			if (my_CFEqual(options, serv->connectopts)) {
				// notify client, so at least then can get the status if they were waiting got it 
				phase_changed(serv, serv->u.ipsec.phase);				
				return 0;
			}
            return EIO;	// not the right time to dial
    }
	
	/* remove any pending notification */
	if (serv->userNotificationRef) {
		CFUserNotificationCancel(serv->userNotificationRef);
		CFRunLoopRemoveSource(CFRunLoopGetCurrent(), serv->userNotificationRLS, kCFRunLoopDefaultMode);
		my_CFRelease(&serv->userNotificationRef);
		my_CFRelease(&serv->userNotificationRLS);			
	}

	ipsec_updatephase(serv, IPSEC_INITIALIZE);
	service_started(serv);
    serv->u.ipsec.laststatus = IPSEC_NO_ERROR;
	serv->u.ipsec.resolvedAddress      = 0;
	serv->u.ipsec.resolvedAddressError = NETDB_SUCCESS;
	serv->u.ipsec.next_address = 0;
	serv->u.ipsec.ping_count = 0;
	
	serv->connectopts = options;
    my_CFRetain(serv->connectopts);
	
    serv->uid = uid;
    serv->gid = gid;
    serv->bootstrap = bootstrap;
    if (onDemand)
        serv->flags |= FLAG_ONDEMAND;
	else
		serv->flags &= ~FLAG_ONDEMAND;
	serv->flags &= ~FLAG_USECERTIFICATE;
	
	if (gSCNCDebug) {
		CFShow(CFSTR("IPSec Controller: IPSec System Prefs"));
		CFShow(serv->systemprefs);
		
		if (serv->connectopts) {
			CFShow(CFSTR("IPSec Controller: IPSec User Options"));
			CFShow(serv->connectopts);
		}
		else CFShow(CFSTR("IPSec Controller: IPSec User Options = none"));
	}
	
	/* build the peer address */
	if (!GetStrFromDict (serv->systemprefs, kRASPropIPSecRemoteAddress, remoteaddress, sizeof(remoteaddress), "")) {
		serv->u.ipsec.laststatus = IPSEC_NOSERVERADDRESS_ERROR;	
		SCLog(TRUE, LOG_ERR, CFSTR("IPSec Controller: cannot find RemoteAddress ..."));
		goto fail;
	}
	
	{		
		serv->u.ipsec.xauth_flags = XAUTH_FIRST_TIME;
		// force prompt if necessary
		CFStringRef encryption = CFDictionaryGetValue(serv->systemprefs, kSCPropNetIPSecXAuthPasswordEncryption);
		if (isString(encryption) 
			&& CFStringCompare(encryption, kSCValNetIPSecXAuthPasswordEncryptionPrompt, 0) == kCFCompareEqualTo) {
			serv->u.ipsec.xauth_flags |= XAUTH_MUST_PROMPT;
		}
	}
	
	/* open the kernel event socket */
	if (serv->u.ipsec.eventfd == -1) {
		if (event_create_socket(serv) < 0) {
			SCLog(TRUE, LOG_ERR, CFSTR("IPSec Controller: cannot create event socket"));
			goto fail;
		}
	}
	
#ifdef TARGET_EMBEDDED_OS
	{
		/* first, bring up EDGE */
		int need_edge = FALSE;
		SCNetworkReachabilityRef ref = NULL;
		SCNetworkConnectionFlags	flags = 0;

		ref = SCNetworkReachabilityCreateWithName(NULL, remoteaddress);
		if (ref) {
			
			if (SCNetworkReachabilityGetFlags(ref, &flags)) {
				if ((flags & kSCNetworkReachabilityFlagsReachable) &&
					(flags & kSCNetworkReachabilityFlagsConnectionRequired) &&
					(flags & kSCNetworkReachabilityFlagsIsWWAN)) {
					need_edge = TRUE;
				}

				SCLog(gSCNCVerbose, LOG_INFO, CFSTR("IPSec Controller: ipsec_start reachability flags = 0x%x, need_edge = %d"), flags, need_edge);
			}
			CFRelease(ref);
		}

		int enabled = 1;
		CFNumberRef numref = CFDictionaryGetValue(serv->systemprefs, kRASPropIPSecNattKeepAliveEnabled);
		numref = isA_CFNumber(numref);
		if (numref)
			CFNumberGetValue(numref, kCFNumberIntType, &enabled);
			
		if (enabled) {
			int timer = (flags & kSCNetworkReachabilityFlagsIsWWAN) ? 0 : 60;
			
			numref = CFDictionaryGetValue(serv->systemprefs, kRASPropIPSecNattKeepAliveTimer);
			numref = isA_CFNumber(numref);
			if (numref)
				CFNumberGetValue(numref, kCFNumberIntType, &timer);

			size_t oldlen = sizeof(gNattKeepAliveInterval); 
			sysctlbyname("net.key.natt_keepalive_interval", &gNattKeepAliveInterval, &oldlen, &timer, sizeof(int));
		}
		
		if (need_edge) {
			if (!BringUpEDGE(serv)) {
				// cannot bring edge up
				goto fail;
			}

			// edge connection in progress
			return 0;
		}
	}
#endif

	return racoon_resolve(serv);
	
fail:
	if (serv->u.ipsec.laststatus == IPSEC_NO_ERROR)
		serv->u.ipsec.laststatus = IPSEC_GENERIC_ERROR;	
	SCLog(TRUE, LOG_ERR, CFSTR("IPSec Controller: ipsec_start failed"));
	ipsec_stop(serv, 0);
    return serv->u.ipsec.laststatus;
}

// XXX
int ipsec_getstatus_hack_notify(struct service *serv)
{
	SCNetworkConnectionStatus	status = PPP_IDLE;

	switch (serv->u.ipsec.phase) {
		case IPSEC_INITIALIZE:
		case IPSEC_CONTACT:
		case IPSEC_PHASE1:
		case IPSEC_PHASE1AUTH:
		case IPSEC_PHASE2:
			status = PPP_ESTABLISH;
			break;
		case IPSEC_TERMINATE:
			status = PPP_TERMINATE;
			break;
		case IPSEC_RUNNING:
			status = PPP_RUNNING;
			break;
		case IPSEC_IDLE:
		default:
			status = PPP_IDLE;
	}
	return status;
}

/* -----------------------------------------------------------------------------
phase change for this config occured
----------------------------------------------------------------------------- */
static 
void ipsec_updatephase(struct service *serv, int phase)
{

    /* check for new state */
    if (phase == serv->u.ipsec.phase)
        return;
    
    serv->u.ipsec.phase = phase;

	phase_changed(serv, phase);
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
int ipsec_stop(struct service *serv, int signal)
{
	char			*errorstr;
	int				error;

	SCLog(gSCNCVerbose, LOG_INFO, CFSTR("IPSec Controller: ipsec_stop"));

    SESSIONTRACERSTOP(serv);

	if (serv->u.ipsec.phase == IPSEC_PHASE1AUTH) {
		/* first, remove any pending authentication dialog */
		if (serv->userNotificationRef) {
			CFUserNotificationCancel(serv->userNotificationRef);
			CFRunLoopRemoveSource(CFRunLoopGetCurrent(), serv->userNotificationRLS, kCFRunLoopDefaultMode);
			my_CFRelease(&serv->userNotificationRef);
			my_CFRelease(&serv->userNotificationRLS);			
		}
	}

   // anticipate next phase
    switch (serv->u.ipsec.phase) {
    
        case IPSEC_IDLE:
        case IPSEC_TERMINATE:
            return 0;
            
        default:
            ipsec_updatephase(serv, IPSEC_TERMINATE);
    }

	if (serv->u.ipsec.controlfd != -1) {
		racoon_send_cmd_disconnect(serv->u.ipsec.controlfd, serv->u.ipsec.peer_address.sin_addr.s_addr);
	}	

	if (serv->u.ipsec.policies_installed) {
		error = IPSecRemovePolicies(serv->u.ipsec.config, -1, &errorstr);
		if (error)
			SCLog(TRUE, LOG_ERR, CFSTR("IPSec Controller: Cannot remove policies, error '%s'"), errorstr);
		
		IPSecRemoveSecurityAssociations((struct sockaddr *)&serv->u.ipsec.our_address, (struct sockaddr *)&serv->u.ipsec.peer_address);
		
		serv->u.ipsec.policies_installed = 0;
	}
	uninstall_mode_config(serv);

	if (serv->u.ipsec.config_applied) {
		error = IPSecRemoveConfiguration(serv->u.ipsec.config, &errorstr);
		if (error)
			SCLog(TRUE, LOG_ERR, CFSTR("IPSec Controller: Cannot remove configuration, error '%s'"), errorstr);
		serv->u.ipsec.config_applied = 0;
	}
	
    if (serv->u.ipsec.msg) {
        my_Deallocate(serv->u.ipsec.msg, serv->u.ipsec.msgtotallen + 1);
        serv->u.ipsec.msg = 0;
    }
    serv->u.ipsec.msglen = 0;
    serv->u.ipsec.msgtotallen = 0;
	
    serv->uid = 0;
    serv->gid = 0;
    serv->bootstrap = 0;
	
	serv->u.ipsec.ping_count = 0;

	my_CFRelease(&serv->u.ipsec.resolvedAddress);
	serv->u.ipsec.resolvedAddress = 0;
	
	if (serv->u.ipsec.controlref) {
		
		CFSocketInvalidate(serv->u.ipsec.controlref);
		my_CFRelease(&serv->u.ipsec.controlref);
	}
	else if (serv->u.ipsec.controlfd) { 
		close(serv->u.ipsec.controlfd);
	}
	serv->u.ipsec.controlfd	= -1;

	if (serv->u.ipsec.eventref) {
		
		CFSocketInvalidate(serv->u.ipsec.eventref);
		my_CFRelease(&serv->u.ipsec.eventref);
	}
	else if (serv->u.ipsec.eventfd) { 
		close(serv->u.ipsec.eventfd);
	}
	serv->u.ipsec.eventfd	= -1;

	my_CFRelease(&serv->connectopts);
	my_CFRelease(&serv->u.ipsec.config);
	
	if (serv->u.ipsec.timerref) {
		CFRunLoopRemoveTimer(CFRunLoopGetCurrent(), serv->u.ipsec.timerref, kCFRunLoopCommonModes);
		my_CFRelease(&serv->u.ipsec.timerref);
	}

	if (serv->u.ipsec.interface_timerref) {
		CFRunLoopRemoveTimer(CFRunLoopGetCurrent(), serv->u.ipsec.interface_timerref, kCFRunLoopCommonModes);
		my_CFRelease(&serv->u.ipsec.interface_timerref);
	}

#ifdef TARGET_EMBEDDED_OS
	if (serv->u.ipsec.edge_timerref) {
		CFRunLoopRemoveTimer(CFRunLoopGetCurrent(), serv->u.ipsec.edge_timerref, kCFRunLoopCommonModes);
		my_CFRelease(&serv->u.ipsec.edge_timerref);
	}
	
	if (serv->u.ipsec.edgeRLS) {
		CFRunLoopRemoveSource(CFRunLoopGetCurrent(), serv->u.ipsec.edgeRLS, kCFRunLoopCommonModes);
		my_CFRelease(&serv->u.ipsec.edgeRLS);
	}
	if (serv->u.ipsec.edgePort) {
		CFMachPortInvalidate(serv->u.ipsec.edgePort);
		my_CFRelease(&serv->u.ipsec.edgePort);			
	}
	my_CFRelease(&serv->u.ipsec.edgeConnection);

	if (gNattKeepAliveInterval != -1) {
		int newval = gNattKeepAliveInterval;
		sysctlbyname("net.key.natt_keepalive_interval", 0, 0, &newval, sizeof(newval));	
		gNattKeepAliveInterval = -1;
	}
#endif
	
	/* 
	 silently fail when connection is on demand,
	 except for unrecoverable certificate errors
	 if needed, kick profile janitor and disable OnDemand
	 */
	if (serv->u.ipsec.laststatus == IPSEC_CLIENT_CERTIFICATE_ERROR) {
		disable_ondemand(serv);

#ifdef TARGET_EMBEDDED_OS
		if (serv->profileIdentifier) {
			CFStringRef msg = CFStringCreateWithFormat(0, 0, CFSTR("IPSec Error %d, Re-enroll"), serv->u.ipsec.laststatus);
			if (msg) {
				display_notification(serv, msg, 0, dialog_cert_fixme_type);
				CFRelease(msg);
			}
		}
		else
#endif
			display_notification(serv, NULL, serv->u.ipsec.laststatus, dialog_default_type);
	}
	else {
		if (!(serv->flags & FLAG_ONDEMAND)
			&& (serv->u.ipsec.laststatus != IPSEC_NO_ERROR))
			display_notification(serv, NULL, serv->u.ipsec.laststatus, dialog_default_type);
	}
	
	ipsec_updatephase(serv, IPSEC_IDLE);
	service_ended(serv);
    
	return 0;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
int ipsec_getstatus(struct service *serv)
{
	SCNetworkConnectionStatus	status = kSCNetworkConnectionDisconnected;

	switch (serv->u.ipsec.phase) {
		case IPSEC_INITIALIZE:
		case IPSEC_CONTACT:
		case IPSEC_PHASE1:
		case IPSEC_PHASE1AUTH:
		case IPSEC_PHASE2:
			status = kSCNetworkConnectionConnecting;
			break;
		case IPSEC_WAITING:
		case IPSEC_TERMINATE:
			status = kSCNetworkConnectionDisconnecting;
			break;
		case IPSEC_RUNNING:
			status = kSCNetworkConnectionConnected;
			break;
		case IPSEC_IDLE:
		default:
			status = kSCNetworkConnectionDisconnected;
	}

	if (gSCNCVerbose) {
		SCLog(gSCNCVerbose, LOG_INFO, CFSTR("IPSec Controller: ipsec_getstatus = %s"), 
			status == kSCNetworkConnectionDisconnected ? "Disconnected" :
			(status == kSCNetworkConnectionConnecting ? "Connecting" :
			(status == kSCNetworkConnectionDisconnecting ? "Disconnecting" :
			(status == kSCNetworkConnectionConnected ? "Connected" :
			("Unknown" )))));
	}
	
	return status;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
int ipsec_copyextendedstatus(struct service *serv, void **reply, u_int16_t *replylen)
{
    CFMutableDictionaryRef	statusdict = 0, dict = 0;
    CFDataRef			dataref = 0;
    void			*dataptr = 0;
    u_int32_t			datalen = 0;
    char			*addrstr;
	
    if ((statusdict = CFDictionaryCreateMutable(NULL, 0, &kCFCopyStringDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks)) == 0)
        goto fail;

    /* create and add dictionary */
    if ((dict = CFDictionaryCreateMutable(NULL, 0, &kCFCopyStringDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks)) == 0)
        goto fail;
    
	/* XXX Hack phone */
	/* We need to stop publishing this fake PPP dictionary, but it seems 
	 that the Settings App key off this dictionary to display the VPN state */
    AddNumber(dict, kSCPropNetIPSecStatus, ipsec_getstatus_hack_notify(serv));
    if (serv->u.ipsec.phase != IPSEC_IDLE	
		&& (addrstr = inet_ntoa(serv->u.ipsec.peer_address.sin_addr))) {
		AddString(dict, kRASPropIPSecRemoteAddress, addrstr);
		AddString(dict, CFSTR("CommRemoteAddress"), addrstr);
	}
    switch (serv->u.ipsec.phase) {
        case IPSEC_RUNNING:
			AddNumber(dict, kSCPropNetIPSecConnectTime, serv->connecttime);
            //AddNumberFromState(gDynamicStore, serv->serviceID, kSCEntNetPPP, kSCPropNetPPPDisconnectTime, dict);
            break;
            
        default:
			AddNumber(dict, CFSTR("LastCause"), serv->u.ipsec.laststatus);
    }
    CFDictionaryAddValue(statusdict, kSCEntNetPPP, dict);
	CFRelease(dict);
    if ((dict = CFDictionaryCreateMutable(NULL, 0, &kCFCopyStringDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks)) == 0)
        goto fail;
	/* XXX End Hack phone */
	
    AddNumber(dict, kSCPropNetIPSecStatus, serv->u.ipsec.phase);
    
    if (serv->u.ipsec.phase != IPSEC_IDLE	
		&& (addrstr = inet_ntoa(serv->u.ipsec.peer_address.sin_addr))) {
		AddString(dict, kRASPropIPSecRemoteAddress, addrstr);
	}
	
    switch (serv->u.ipsec.phase) {
        case IPSEC_RUNNING:
			AddNumber(dict, kSCPropNetIPSecConnectTime, serv->connecttime);
            break;
            
        default:
			AddNumber(dict, CFSTR("LastCause"), serv->u.ipsec.laststatus);
    }
    CFDictionaryAddValue(statusdict, kSCEntNetIPSec, dict);
	
    CFRelease(dict);
	
    /* create and add IPv4 dictionary */
    if (serv->u.ipsec.phase == IPSEC_RUNNING) {
        dict = (CFMutableDictionaryRef)copyEntity(gDynamicStore, kSCDynamicStoreDomainState, serv->serviceID, kSCEntNetIPv4);
        if (dict) {
            CFDictionaryAddValue(statusdict, kSCEntNetIPv4, dict);
            CFRelease(dict);
        }
    }
	
	if (gSCNCDebug) {
		CFShow(CFSTR("IPSec Controller: Copy Extended Status\n"));
		CFShow(statusdict);
	}
	
    /* We are done, now serialize it */
    if ((dataref = Serialize(statusdict, &dataptr, &datalen)) == 0)
        goto fail;
    
    *reply = my_Allocate(datalen);
    if (*reply == 0)
        goto fail;

    bcopy(dataptr, *reply, datalen);    
    CFRelease(statusdict);
    CFRelease(dataref);
    *replylen = datalen;
    return 0;

fail:
    my_CFRelease(&statusdict);
    my_CFRelease(&dict);
    my_CFRelease(&dataref);
    return ENOMEM;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
int ipsec_getconnectdata(struct service *serv, void **reply, u_int16_t *replylen, int all)
{
    CFDataRef			dataref = NULL;
    void			*dataptr = 0;
    u_int32_t			datalen = 0;
    CFDictionaryRef		opts;
    CFMutableDictionaryRef	mdict = NULL, mdict1;
    CFDictionaryRef	dict;
	int err = 0;
        
    /* return saved data */
    opts = serv->connectopts;

    if (opts == 0) {
        // no data
        *replylen = 0;
        return 0;
    }
    
	if (!all) {
		/* special code to remove secret information */

		mdict = CFDictionaryCreateMutableCopy(0, 0, opts);
		if (mdict == 0) {
			// no data
			*replylen = 0;
			return 0;
		}
		
		dict = CFDictionaryGetValue(mdict, kSCEntNetIPSec);
		if (dict && (CFGetTypeID(dict) == CFDictionaryGetTypeID())) {
			mdict1 = CFDictionaryCreateMutableCopy(0, 0, dict);
			if (mdict1) {
				CFDictionaryRemoveValue(mdict1, kSCPropNetIPSecSharedSecret);
				CFDictionarySetValue(mdict, kSCEntNetIPSec, mdict1);
				CFRelease(mdict1);
			}
		}
	}

    if ((dataref = Serialize(all ? opts : mdict, &dataptr, &datalen)) == 0) {
		err = ENOMEM;
        goto end;
    }
    
    *reply = my_Allocate(datalen);
    if (*reply == 0) {
		err = ENOMEM;
        goto end;
    }
    else {
        bcopy(dataptr, *reply, datalen);
        *replylen = datalen;
    }

end:
    my_CFRelease(&mdict);
    my_CFRelease(&dataref);
    return err;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
int ipsec_copystatistics(struct service *serv, void **reply, u_int16_t *replylen)
{
    CFMutableDictionaryRef	statsdict = 0, dict = 0;
    CFDataRef			dataref = 0;
    void				*dataptr = 0;
    u_int32_t			datalen = 0;
	int					error = 0;
	
	if (serv->u.ipsec.phase != IPSEC_RUNNING)
			return EINVAL;
			
    if ((statsdict = CFDictionaryCreateMutable(NULL, 0, &kCFCopyStringDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks)) == 0) {
		error = ENOMEM;
		goto fail;
	}

    /* create and add IPSec dictionary */
    if ((dict = CFDictionaryCreateMutable(NULL, 0, &kCFCopyStringDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks)) == 0) {
		error = ENOMEM;
		goto fail;
	}
    
	AddNumber(dict, kSCNetworkConnectionBytesIn, 0);
	AddNumber(dict, kSCNetworkConnectionBytesOut, 0);
	AddNumber(dict, kSCNetworkConnectionPacketsIn, 0);
	AddNumber(dict, kSCNetworkConnectionPacketsOut, 0);
	AddNumber(dict, kSCNetworkConnectionErrorsIn, 0);
	AddNumber(dict, kSCNetworkConnectionErrorsOut, 0);

    CFDictionaryAddValue(statsdict, kSCEntNetIPSec, dict);
    CFRelease(dict);

    /* We are done, now serialize it */
    if ((dataref = Serialize(statsdict, &dataptr, &datalen)) == 0) {
		error = ENOMEM;
        goto fail;
	}
    
    *reply = my_Allocate(datalen);
    if (*reply == 0) {
		error = ENOMEM;
        goto fail;
	}

    bcopy(dataptr, *reply, datalen);    
	
    CFRelease(statsdict);
    CFRelease(dataref);
    *replylen = datalen;
    return 0;

fail:
    my_CFRelease(&statsdict);
    my_CFRelease(&dict);
    my_CFRelease(&dataref);
    return error;
}

/* -----------------------------------------------------------------------------
user has looged out
need to check the disconnect on logout flag for the ipsec interfaces
----------------------------------------------------------------------------- */
void ipsec_log_out(struct service *serv)
{
	if (serv->u.ipsec.phase != IPSEC_IDLE
		&& (serv->flags & FLAG_SETUP_DISCONNECTONLOGOUT))
		scnc_stop(serv, 0, SIGTERM);
}

/* -----------------------------------------------------------------------------
user has logged in
----------------------------------------------------------------------------- */
void ipsec_log_in(struct service *serv)
{
}

/* -----------------------------------------------------------------------------
user has switched
need to check the disconnect on logout flags for the ppp interfaces
----------------------------------------------------------------------------- */
void ipsec_log_switch(struct service *serv)
{

	switch (serv->u.ipsec.phase) {
		case IPSEC_IDLE:
			break;
			
		default:
			if (serv->flags & FLAG_SETUP_DISCONNECTONFASTUSERSWITCH)
				scnc_stop(serv, 0, SIGTERM);
	}
}

/* -----------------------------------------------------------------------------
ipv4 state has changed
----------------------------------------------------------------------------- */
void ipsec_ipv4_state_changed(struct service *serv)
{
}

/* -----------------------------------------------------------------------------
system is asking permission to sleep
return if sleep is authorized
----------------------------------------------------------------------------- */
int ipsec_can_sleep(struct service	*serv)
{
        
    // I refuse idle sleep if ppp is connected
	if (serv->u.ipsec.phase == IPSEC_RUNNING
		&& (serv->flags & FLAG_SETUP_PREVENTIDLESLEEP))
		return 0;
	
    return 1;
}

/* -----------------------------------------------------------------------------
system is going to sleep
disconnect services and return if a delay is needed
----------------------------------------------------------------------------- */ 
int ipsec_will_sleep(struct service	*serv, int checking)
{
    u_int32_t			delay = 0, alert = 0;
            
	if (serv->u.ipsec.phase != IPSEC_IDLE
		&& (serv->flags & FLAG_SETUP_DISCONNECTONSLEEP)) { 
		delay = 1;
		alert = 2;
		if (!checking)
			scnc_stop(serv, 0, SIGTERM);
	}
        
    return delay + alert;
}

/* -----------------------------------------------------------------------------
system is waking up
----------------------------------------------------------------------------- */
void ipsec_wake_up(struct service	*serv)
{
	if (serv->u.ipsec.phase == IPSEC_RUNNING) {
		racoon_send_cmd_start_dpd(serv->u.ipsec.controlfd, serv->u.ipsec.peer_address.sin_addr.s_addr);
	}
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
static
void display_notification(struct service *serv, CFStringRef message, int errnum, int dialog_type) 
{
    CFStringRef 	msg = NULL;
    CFMutableDictionaryRef 	dict = NULL;
    SInt32 			err;

    if ((serv->flags & FLAG_ALERTERRORS) == 0)
        return;

	if (message)
		msg = message;
	else {
		// filter out the following messages
		switch (errnum) {
			case IPSEC_NO_ERROR: /* IPSec Error 0 */
				return;				
#ifdef TARGET_EMBEDDED_OS
			/* Error 15, 16, 17 exists are not display on embedded os */
			case IPSEC_NETWORKCHANGE_ERROR: /* IPSec Error 15 */
			case IPSEC_PEERDISCONNECT_ERROR: /* IPSec Error 16 */
			case IPSEC_PEERDEADETECTION_ERROR: /* IPSec Error 17 */
			case IPSEC_IDLETIMEOUT_ERROR: /* IPSec Error 19 */
				return;
#endif
		}
		msg = CFStringCreateWithFormat(0, 0, CFSTR("IPSec Error %d"), errnum);
	}

    if (!msg || !CFStringGetLength(msg))
		goto done;

    dict = CFDictionaryCreateMutable(NULL, 0, &kCFCopyStringDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    if (!dict)
		goto done;
		
	if (gIconURLRef)
		CFDictionaryAddValue(dict, kCFUserNotificationIconURLKey, gIconURLRef);
	if (gBundleURLRef)
		CFDictionaryAddValue(dict, kCFUserNotificationLocalizationURLKey, gBundleURLRef);

	CFDictionaryAddValue(dict, kCFUserNotificationAlertMessageKey, msg);
	CFDictionaryAddValue(dict, kCFUserNotificationAlertHeaderKey, CFSTR("VPN Connection"));

	switch (dialog_type) {
		case dialog_has_disconnect_type:
			CFDictionaryAddValue(dict, kCFUserNotificationAlternateButtonTitleKey, CFSTR("Disconnect"));
			break;
		case dialog_cert_fixme_type:
			CFDictionaryAddValue(dict, kCFUserNotificationDefaultButtonTitleKey, CFSTR("Ignore"));
			CFDictionaryAddValue(dict, kCFUserNotificationAlternateButtonTitleKey, CFSTR("Settings"));
			break;
	}
	
	if (serv->userNotificationRef) {
		err = CFUserNotificationUpdate(serv->userNotificationRef, 0, kCFUserNotificationNoteAlertLevel, dict);
	}
	else {
		serv->userNotificationRef = CFUserNotificationCreate(NULL, 0, kCFUserNotificationNoteAlertLevel, &err, dict);
		if (!serv->userNotificationRef)
			goto done;

		serv->userNotificationRLS = CFUserNotificationCreateRunLoopSource(NULL, serv->userNotificationRef, 
													user_notification_callback, 0);
		if (!serv->userNotificationRLS) {
			my_CFRelease(&serv->userNotificationRef);
			goto done;
		}
		CFRunLoopAddSource(CFRunLoopGetCurrent(), serv->userNotificationRLS, kCFRunLoopDefaultMode);
	}
		

done:
    my_CFRelease(&dict);
	if (!message)
		my_CFRelease(&msg);
}

