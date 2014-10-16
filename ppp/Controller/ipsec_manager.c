/*
 * Copyright (c) 2000, 2014 Apple Inc. All rights reserved.
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
#include <net/if_utun.h>
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
#include "ipsec_utils.h"
#include "scnc_utils.h"
#include "scnc_cache.h"
#include "cf_utils.h"
#include "PPPControllerPriv.h"
#include "controller_options.h"

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

#define TIMEOUT_INITIAL_CONTACT	10 /* give 10 second to establish contact with server */
#define TIMEOUT_PHASE1			30 /* give 30 second to complete phase 1 */
#define TIMEOUT_PHASE2			30 /* give 30 second to complete phase 2 (to accomodate larger cfgs on slower devices) */
#define TIMEOUT_PHASE2_PING		1 /* sends ping every second until phase 2 starts */
#define TIMEOUT_ASSERT_IDLE		3 /* allow 3 seconds after assert for traffic to trigger rekeys */

#define MAX_PHASE2_PING			15 /* give up after 15 pings */

#define TIMEOUT_INTERFACE_CHANGE	20 /* give 20 second to address to come back */

#define XAUTH_FIRST_TIME		0x8000
#define XAUTH_NEED_USERNAME		0x0001
#define XAUTH_NEED_PASSWORD		0x0002
#define XAUTH_NEED_PASSCODE		0x0004
#define XAUTH_NEED_ANSWER		0x0008
#define XAUTH_NEED_NEXT_PIN		0x0010
#define XAUTH_NEED_XAUTH_INFO	0x0020
#define XAUTH_MUST_PROMPT		0x0040
#define XAUTH_DID_PROMPT		0x0080

#define DISPLAY_RE_ENROLL_ALERT_INTERVAL	15*60	/* how much should lapse before displaying re-enroll alert again */

#define IPSEC_STATUS_IS_CLIENT_CERTIFICATE_INVALID(s) ((s == IPSEC_CLIENT_CERTIFICATE_PREMATURE) || \
														(s == IPSEC_CLIENT_CERTIFICATE_EXPIRED) ||  \
														(s == IPSEC_CLIENT_CERTIFICATE_ERROR))

struct isakmp_xauth {
	u_int16_t	type;
	CFStringRef str;
};


#if TARGET_OS_EMBEDDED
// extra CFUserNotification keys
static CFStringRef const SBUserNotificationTextAutocapitalizationType = CFSTR("SBUserNotificationTextAutocapitalizationType");
static CFStringRef const SBUserNotificationTextAutocorrectionType = CFSTR("SBUserNotificationTextAutocorrectionType");
static CFStringRef const SBUserNotificationGroupsTextFields = CFSTR("SBUserNotificationGroupsTextFields");
#endif

/* -----------------------------------------------------------------------------
Forward declarations
----------------------------------------------------------------------------- */

static void ipsec_log(int level, CFStringRef format, ...) CF_FORMAT_FUNCTION(2, 3);
static void ipsec_updatephase(struct service *serv, int phase);
static void display_notification(struct service *serv, CFStringRef message, int errnum, int dialog_type);
static void racoon_timer(CFRunLoopTimerRef timer, void *info);
static int racoon_restart(struct service *serv, struct sockaddr_in *address);
static int racoon_resolve(struct service *serv);
static bool mode_config_is_default(struct service *serv);
static void install_mode_config(struct service *serv, Boolean installConfig, Boolean installPolicies);
static void uninstall_mode_config(struct service *serv, Boolean uninstallPolicies);
static int unassert_mode_config(struct service *serv);
static int ask_user_xauth(struct service *serv, char* message);
static boolean_t checkpassword(struct service *serv, int must_prompt);
int readn(int ref, void *data, int len);
int writen(int ref, void *data, int len);

int racoon_send_cmd_start_ph2(int fd, u_int32_t address, CFDictionaryRef ipsec_dict);
int racoon_send_cmd_assert(struct service *serv);
int racoon_send_cmd_xauthinfo(int fd, u_int32_t address, struct isakmp_xauth *isakmp_array, int isakmp_nb);
int racoon_send_cmd_start_dpd(int fd, u_int32_t address) ;

void
ipsec_log(int level, CFStringRef format, ...)
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


/* -----------------------------------------------------------------------------
 ----------------------------------------------------------------------------- */
int ipsec_init_things()
{

    // TO DO: save each configuration and remove them upon configd restart, 
    // in case they are leftover after a crash.
    
#if TARGET_OS_EMBEDDED
    // currently only VPN uses IPSec on embedded
    // Do not flush SAs, so as to not interfere with other configurations
    //IPSecFlushAll();
#endif /* TARGET_OS_EMBEDDED */

    return 0;
}

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
		case VPNCTL_NTYPE_LOCAL_CERT_PREMATURE: return "Certificate premature";
		case VPNCTL_NTYPE_LOCAL_CERT_EXPIRED: return "Certificate expired";
		case VPNCTL_NTYPE_PEER_CERT_PREMATURE: return "Server certificate premature";
		case VPNCTL_NTYPE_PEER_CERT_EXPIRED: return "Server certificate expired";
		case VPNCTL_NTYPE_PEER_CERT_INVALID_SUBJNAME: return "Server certificate subjectName invalid";
		case VPNCTL_NTYPE_PEER_CERT_INVALID_SUBJALTNAME: return "Server certificate subjectAltName invalid";
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
			return IPSEC_NO_ERROR;

		case VPNCTL_NTYPE_LOCAL_CERT_PREMATURE:
			return IPSEC_CLIENT_CERTIFICATE_PREMATURE;
			
		case VPNCTL_NTYPE_LOCAL_CERT_EXPIRED:
			return IPSEC_CLIENT_CERTIFICATE_EXPIRED;

		case VPNCTL_NTYPE_PEER_CERT_PREMATURE:
			return IPSEC_SERVER_CERTIFICATE_PREMATURE;
			
		case VPNCTL_NTYPE_PEER_CERT_EXPIRED:
			return IPSEC_SERVER_CERTIFICATE_EXPIRED;

		case VPNCTL_NTYPE_PEER_CERT_INVALID_SUBJNAME:
		case VPNCTL_NTYPE_PEER_CERT_INVALID_SUBJALTNAME:
			return IPSEC_SERVER_CERTIFICATE_INVALID_ID;

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
		case VPNCTL_CMD_ASSERT: return "VPNCTL_CMD_ASSERT";
		case VPNCTL_CMD_RECONNECT: return "VPNCTL_CMD_RECONNECT";
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
	serv->u.ipsec.routes = NULL;
    return 0;
}

/* -----------------------------------------------------------------------------
an interface is come down, dispose the structure
----------------------------------------------------------------------------- */
int ipsec_dispose_service(struct service *serv)
{

    if (serv->u.ipsec.phase != IPSEC_IDLE)
        return 1;
    free_service_routes(serv);
	my_CFRelease(&serv->systemprefs);
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
					 FLAG_SETUP_ONDEMAND +
					 FLAG_DARKWAKE +
					 FLAG_SETUP_PERSISTCONNECTION +
					 FLAG_SETUP_DISCONNECTONWAKE);
	
	serv->flags |= (    
					FLAG_ALERTERRORS +
					FLAG_ALERTPASSWORDS);
	
	my_CFRelease(&serv->systemprefs);
	if (serv->ne_sm_bridge != NULL) {
		serv->systemprefs = ne_sm_bridge_copy_configuration(serv->ne_sm_bridge);
	} else {
		serv->systemprefs = copyEntity(gDynamicStore, kSCDynamicStoreDomainSetup, serv->serviceID, kSCEntNetIPSec);
	}
	if (serv->systemprefs == NULL) {
		ipsec_log(LOG_ERR, CFSTR("IPSec Controller: Cannot copy IPSec dictionary from setup"));
		ipsec_stop(serv, 0);
		return -1;
	}

	/* Currently, OnDemand imples network detection */
	lval = 0;
	getNumber(serv->systemprefs, kSCPropNetIPSecOnDemandEnabled, &lval);
	if (lval)
		serv->flags |= (FLAG_SETUP_ONDEMAND | FLAG_SETUP_NETWORKDETECTION);
	else if (CFDictionaryGetValue(serv->systemprefs, kSCPropNetVPNOnDemandRules)) {
		if (controller_options_is_useVODDisconnectRulesWhenVODDisabled())
			serv->flags |= FLAG_SETUP_NETWORKDETECTION;
	}
	
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
	
	/* "disconnect on wake" is enabled by default for IPSec */
	lval = 1;
	serv->sleepwaketimeout = 0;
	getNumber(serv->systemprefs, kSCPropNetIPSecDisconnectOnWake, &lval);
	if (lval) {
		serv->flags |= FLAG_SETUP_DISCONNECTONWAKE;
		getNumber(serv->systemprefs, kSCPropNetIPSecDisconnectOnWakeTimer, &serv->sleepwaketimeout);
	}

	lval = 1;
	getNumber(serv->systemprefs, CFSTR("AlertEnable"), &lval);
	if (!lval) serv->flags &= ~(FLAG_ALERTERRORS + FLAG_ALERTPASSWORDS);
	
	/* enable "ConnectionPersist" */
	lval = 0;
	getNumber(serv->systemprefs, CFSTR("ConnectionPersist"), &lval);
	if (lval) serv->flags |= FLAG_SETUP_PERSISTCONNECTION;

#if TARGET_OS_EMBEDDED
	if (CFDictionaryContainsKey(serv->systemprefs, CFSTR("ProfileIdentifier"))) {
		my_CFRelease(&serv->profileIdentifier);
		serv->profileIdentifier = my_CFRetain(CFDictionaryGetValue(serv->systemprefs, CFSTR("ProfileIdentifier")));
	}
#endif

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

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
void ipsec_user_notification_callback(struct service *serv, CFUserNotificationRef userNotification, CFOptionFlags responseFlags)
{
	
	if ((responseFlags & 3) != kCFUserNotificationDefaultResponse) {
		switch (serv->u.ipsec.phase) {
			case IPSEC_IDLE:
				if (IPSEC_STATUS_IS_CLIENT_CERTIFICATE_INVALID(serv->u.ipsec.laststatus)) {
#if TARGET_OS_EMBEDDED
					if (serv->ne_sm_bridge) {
						ne_sm_bridge_start_profile_janitor(serv->ne_sm_bridge, serv->profileIdentifier);
					} else {
						start_profile_janitor(serv);
					}
#endif
				}
				return;
			default:
				// user cancelled
				ipsec_stop(serv, 0);
				return;
				
		}
	}
 
#if TARGET_OS_EMBEDDED
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
#if TARGET_OS_EMBEDDED
	if (serv->u.ipsec.timerref) {
		CFRunLoopTimerSetNextFireDate(serv->u.ipsec.timerref, CFAbsoluteTimeGetCurrent() + TIMEOUT_PHASE1);
	}
	ipsec_updatephase(serv, IPSEC_PHASE1);
#else
	if (serv->u.ipsec.phase == IPSEC_PHASE1AUTH) {
		if (serv->u.ipsec.timerref) {
			CFRunLoopTimerSetNextFireDate(serv->u.ipsec.timerref, CFAbsoluteTimeGetCurrent() + TIMEOUT_PHASE1);
		}
		ipsec_updatephase(serv, IPSEC_PHASE1);
	}
#endif /* TARGET_OS_EMBEDDED */
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

		if (serv->ne_sm_bridge == NULL) {
			ok = UpdatePasswordPrefs(serv->serviceID, serv->typeRef, kSCNetworkInterfacePasswordTypeIPSecXAuth, 
			                         kSCPropNetIPSecXAuthPasswordEncryption, must_prompt ? kSCValNetIPSecXAuthPasswordEncryptionPrompt : NULL,
			                         CFSTR("IPSec Controller"));
		} else {
			ne_sm_bridge_clear_saved_password(serv->ne_sm_bridge, kSCPropNetIPSecXAuthPassword);
			ok = TRUE;
		}

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
#if !TARGET_OS_EMBEDDED
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
#if TARGET_OS_EMBEDDED
			// TO DO:
			// currently, password is given inline in SCNetworkConnectionStart
			// needs t implement keychain support later
#else
			prefs = SCPreferencesCreate(NULL, CFSTR("CopyPassword"), NULL);
			if (prefs == NULL) {
				ipsec_log(LOG_ERR, CFSTR("IPSec Controller: SCPreferencesCreate fails"));
				goto done;
			}
			// get the service
			service = SCNetworkServiceCopy(prefs, serv->serviceID);
			if (service == NULL) {
				ipsec_log(LOG_ERR, CFSTR("IPSec Controller: SCNetworkServiceCopy fails"));
				goto done;
			}
			// get the interface associated with the service
			interface = SCNetworkServiceGetInterface(service);
			if ((interface == NULL) || !CFEqual(SCNetworkInterfaceGetInterfaceType(interface), kSCNetworkInterfaceTypeIPSec)) {
				ipsec_log(LOG_ERR, CFSTR("IPSec Controller: interface not IPSec"));
				goto done;
			}
			CFDataRef passworddata = SCNetworkInterfaceCopyPassword( interface, kSCNetworkInterfacePasswordTypeIPSecXAuth);
			if (passworddata) {
				CFIndex passworddatalen = CFDataGetLength(passworddata);
				if ((decryptedpasswd = CFStringCreateWithBytes(NULL, CFDataGetBytePtr(passworddata), passworddatalen, kCFStringEncodingUTF8, FALSE)))
					ipsec_log(LOG_INFO, CFSTR("IPSec Controller: decrypted password %s"),  decryptedpasswd ? (CFStringGetCStringPtr(decryptedpasswd,kCFStringEncodingMacRoman)) : "NULL");
				else
					ipsec_log(LOG_ERR, CFSTR("IPSec Controller: cannot decrypt password"));
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
	
#if !TARGET_OS_EMBEDDED
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
#if TARGET_OS_EMBEDDED
	int		nbfields = 0;
#endif

    if ((serv->flags & FLAG_ALERTPASSWORDS) == 0)
        return -1;

#if !TARGET_OS_EMBEDDED
    if (serv->flags & FLAG_DARKWAKE)
        return -1;
#endif

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
		
#if TARGET_OS_EMBEDDED
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

#if TARGET_OS_EMBEDDED
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

	cmd_xauth_info = ALIGNED_CAST(struct vpnctl_cmd_xauth_info *)serv->u.ipsec.msg;

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
		
		attr = ALIGNED_CAST(struct isakmp_data *)dataptr;
		type = ntohs(attr->type) & 0x7FFF;
		tlv = (type ==  ntohs(attr->type));
		
		switch (type)
		{
			case XAUTH_TYPE:
				switch (ntohs(attr->lorv)) {
					case XAUTH_TYPE_GENERIC:
						break;
					default:
						ipsec_log(LOG_ERR, CFSTR("IPSec Controller: Received unsupported Xauth Type (value %d)"), ntohs(attr->lorv));
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
				ipsec_log(LOG_ERR, CFSTR("IPSec Controller: Received unsupported Xauth Challenge"));
				goto fail;
				break;
			case XAUTH_DOMAIN:
				ipsec_log(LOG_ERR, CFSTR("IPSec Controller: Ignoring unsupported Xauth Domain"));
				break;
			case XAUTH_STATUS:
				ipsec_log(LOG_ERR, CFSTR("IPSec Controller: Received unsupported Xauth Status"));
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
	
	if (serv->u.ipsec.xauth_flags & XAUTH_FIRST_TIME || serv->u.ipsec.phase == IPSEC_RUNNING) {
		
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
					if (serv->ne_sm_bridge == NULL) {
						password = copy_decrypted_password(serv);
					} else {
						password = ne_sm_bridge_copy_password_from_keychain(serv->ne_sm_bridge, kSCPropNetIPSecXAuthPassword);
					}
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

#if TARGET_OS_EMBEDDED
				if (serv->u.ipsec.timerref) {
					CFRunLoopTimerSetNextFireDate(serv->u.ipsec.timerref, CFAbsoluteTimeGetCurrent() + TIMEOUT_PHASE1);
				}
				ipsec_updatephase(serv, IPSEC_PHASE1);
#else
				if (serv->u.ipsec.phase == IPSEC_PHASE1AUTH) {
					if (serv->u.ipsec.timerref) {
						CFRunLoopTimerSetNextFireDate(serv->u.ipsec.timerref, CFAbsoluteTimeGetCurrent() + TIMEOUT_PHASE1);
					}
					ipsec_updatephase(serv, IPSEC_PHASE1);
				}
#endif /* TARGET_OS_EMBEDDED */
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
	struct vpnctl_status_peer_resp *peer_resp; 
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
	
	ipsec_log(LOG_INFO, CFSTR("IPSec Controller: ===================================================="));
	ipsec_log(LOG_INFO, CFSTR("IPSec Controller: Process Message:"));
	ipsec_log(LOG_INFO, CFSTR("IPSec Controller:	msg_type = 0x%x (%s)"), ntohs(serv->u.ipsec.msghdr.msg_type), ipsec_msgtype_to_str(ntohs(serv->u.ipsec.msghdr.msg_type)));
	ipsec_log(LOG_INFO, CFSTR("IPSec Controller:	flags = 0x%x %s"), ntohs(serv->u.ipsec.msghdr.flags), (ntohs(serv->u.ipsec.msghdr.flags) & VPNCTL_FLAG_MODECFG_USED) ? "MODE CONFIG USED" : "");
	ipsec_log(LOG_INFO, CFSTR("IPSec Controller:	cookie = 0x%x"), ntohl(serv->u.ipsec.msghdr.cookie));
	ipsec_log(LOG_INFO, CFSTR("IPSec Controller:	reserved = 0x%x"), ntohl(serv->u.ipsec.msghdr.reserved));
	ipsec_log(LOG_INFO, CFSTR("IPSec Controller:	result = 0x%x"), ntohs(serv->u.ipsec.msghdr.result));
	ipsec_log(LOG_INFO, CFSTR("IPSec Controller:	len = %d"), ntohs(serv->u.ipsec.msghdr.len));


	switch (ntohs(serv->u.ipsec.msghdr.msg_type)) {
			// BIND/UNBIND/CONNECT share the same structure
			case VPNCTL_CMD_BIND:
			case VPNCTL_CMD_UNBIND:
			case VPNCTL_CMD_CONNECT:
			case VPNCTL_CMD_RECONNECT:
			case VPNCTL_CMD_DISCONNECT:
				cmd_bind = ALIGNED_CAST(struct vpnctl_cmd_bind *)serv->u.ipsec.msg; 
				ipsec_log(LOG_INFO, CFSTR("IPSec Controller:	----------------------------"));
				addr.s_addr = cmd_bind->address;
				ipsec_log(LOG_INFO, CFSTR("IPSec Controller:	address = %s"), inet_ntoa(addr));
				break;

			case VPNCTL_CMD_XAUTH_INFO:
				cmd_xauth_info = ALIGNED_CAST(struct vpnctl_cmd_xauth_info *)serv->u.ipsec.msg;
				ipsec_log(LOG_INFO, CFSTR("IPSec Controller:	----------------------------"));
				addr.s_addr = cmd_xauth_info->address;
				ipsec_log(LOG_INFO, CFSTR("IPSec Controller:	address = %s"), inet_ntoa(addr));
				break;

			case VPNCTL_STATUS_IKE_FAILED:
				failed_status = ALIGNED_CAST(struct vpnctl_status_failed *)serv->u.ipsec.msg;
				ipsec_log(LOG_INFO, CFSTR("IPSec Controller:	----------------------------"));
				addr.s_addr = failed_status->address;
				ipsec_log(LOG_INFO, CFSTR("IPSec Controller:	address = %s"), inet_ntoa(addr));
				ipsec_log(LOG_INFO, CFSTR("IPSec Controller:	ike_code = %d 0x%x (%s)"), ntohs(failed_status->ike_code), ntohs(failed_status->ike_code), ipsec_error_to_str(ntohs(failed_status->ike_code)));
				ipsec_log(LOG_INFO, CFSTR("IPSec Controller:	from = %d"), ntohs(failed_status->from));

				switch (ntohs(failed_status->ike_code)) {
					case VPNCTL_NTYPE_LOAD_BALANCE:		
						addr.s_addr = *ALIGNED_CAST(u_int32_t*)failed_status->data;
						ipsec_log(LOG_INFO, CFSTR("IPSec Controller:	redirect address = %s"), inet_ntoa(addr));
						break;
				}

				break;

			case VPNCTL_STATUS_PH1_START_US:
				break;

			case VPNCTL_STATUS_PH1_START_PEER:
				break;

			case VPNCTL_STATUS_PH1_ESTABLISHED:
				phase_change_status = ALIGNED_CAST(struct vpnctl_status_phase_change *)serv->u.ipsec.msg;
				addr.s_addr = phase_change_status->address;
				ipsec_log(LOG_INFO, CFSTR("IPSec Controller:	address = %s"), inet_ntoa(addr));
				if (ntohs(phase_change_status->hdr.flags) & VPNCTL_FLAG_MODECFG_USED) {
					char *modecfg_data = (char*)serv->u.ipsec.msg + sizeof(struct vpnctl_status_phase_change) + sizeof(struct vpnctl_modecfg_params);
					int modecfg_data_len = ntohs(serv->u.ipsec.msghdr.len) - ((sizeof(struct vpnctl_status_phase_change) + sizeof(struct vpnctl_modecfg_params)) -  sizeof(struct vpnctl_hdr));
					struct vpnctl_modecfg_params *modecfg = ALIGNED_CAST(struct vpnctl_modecfg_params *)(void*)(serv->u.ipsec.msg + sizeof(struct vpnctl_status_phase_change));
					addr.s_addr = modecfg->outer_local_addr;
					ipsec_log(LOG_INFO, CFSTR("IPSec Controller:	outer_local_addr = %s"), inet_ntoa(addr));
					ipsec_log(LOG_INFO, CFSTR("IPSec Controller:	outer_remote_port = %d"), ntohs(modecfg->outer_remote_port));
					ipsec_log(LOG_INFO, CFSTR("IPSec Controller:	outer_local_port = %d"), ntohs(modecfg->outer_local_port));
					ipsec_log(LOG_INFO, CFSTR("IPSec Controller:	ifname = %s"), modecfg->ifname);

					int tlen = modecfg_data_len;
					char	*dataptr = modecfg_data;
					
					while (tlen > 0)
					{
						int tlv;
						u_int16_t type;
                        struct isakmp_data attr;
						
                        memcpy(&attr, dataptr, sizeof(attr));       // Wcast-align fix - memcpy for unaligned access
						type = ntohs(attr.type) & 0x7FFF;
						tlv = (type ==  ntohs(attr.type));

						ipsec_log(LOG_INFO, CFSTR("IPSec Controller:	ModeConfig Attribute Type = %d (%s)"), type, ipsec_modecfgtype_to_str(type));
						if (tlv) {
							ipsec_log(LOG_INFO, CFSTR("IPSec Controller:	ModeConfig Attribute Length = %d Value = ..."), ntohs(attr.lorv));

							tlen -= ntohs(attr.lorv);
							dataptr += ntohs(attr.lorv);
						}
						else {
							ipsec_log(LOG_INFO, CFSTR("IPSec Controller:	ModeConfig Attribute Value = %d"), ntohs(attr.lorv));
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
#if !TARGET_OS_EMBEDDED
			case VPNCTL_STATUS_NEED_REAUTHINFO:
#endif /* !TARGET_OS_EMBEDDED */
				cmd_xauth_info = ALIGNED_CAST(struct vpnctl_cmd_xauth_info *)serv->u.ipsec.msg;
				ipsec_log(LOG_INFO, CFSTR("IPSec Controller:	----------------------------"));
				addr.s_addr = cmd_xauth_info->address;
				ipsec_log(LOG_INFO, CFSTR("IPSec Controller:	address = %s"), inet_ntoa(addr));
				
				char *xauth_data = (char*)serv->u.ipsec.msg + sizeof(struct vpnctl_cmd_xauth_info); 
				int xauth_data_len = ntohs(serv->u.ipsec.msghdr.len) - (sizeof(struct vpnctl_cmd_xauth_info) -  sizeof(struct vpnctl_hdr));

				int tlen = xauth_data_len;
				char	*dataptr = xauth_data;
				
				while (tlen > 0)
				{
					int tlv;
					u_int16_t type;
					struct isakmp_data attr;
                    
                    memcpy(&attr, dataptr, sizeof(attr));   // Wcast-align fix - memcpy for unaligned access
					type = ntohs(attr.type) & 0x7FFF;
					tlv = (type ==  ntohs(attr.type));

					ipsec_log(LOG_INFO, CFSTR("IPSec Controller:	XAuth Attribute Type = %d (%s)"), type, ipsec_xauthtype_to_str(type));
					if (tlv) {
						if (type == XAUTH_MESSAGE) {
							char *message = malloc(ntohs(attr.lorv) + 1);
							if (message) {
								bcopy(dataptr + sizeof(u_int32_t), message, ntohs(attr.lorv));
								message[ntohs(attr.lorv)] = 0;
								ipsec_log(LOG_INFO, CFSTR("IPSec Controller:	XAuth Attribute Value = %s"), message);
								free(message);
							}
						}
						else 
							ipsec_log(LOG_INFO, CFSTR("IPSec Controller:	XAuth Attribute Length = %d Value = ..."), ntohs(attr.lorv));

						tlen -= ntohs(attr.lorv);
						dataptr += ntohs(attr.lorv);
					}
					else {
						ipsec_log(LOG_INFO, CFSTR("IPSec Controller:	XAuth Attribute Value = %d"), ntohs(attr.lorv));
					}
					
					tlen -= sizeof(u_int32_t);
					dataptr += sizeof(u_int32_t);
						
				}
				break;

			case VPNCTL_STATUS_PEER_RESP:
				peer_resp = ALIGNED_CAST(__typeof__(peer_resp))serv->u.ipsec.msg;
				ipsec_log(LOG_INFO, CFSTR("IPSec Controller:	----------------------------"));
				addr.s_addr = peer_resp->address;
				ipsec_log(LOG_INFO, CFSTR("IPSec Controller:	response from address = %s"), inet_ntoa(addr));
				ipsec_log(LOG_INFO, CFSTR("IPSec Controller:	ike_code = %d"), ntohs(peer_resp->ike_code));
				break;

			default:
				/* ignore other messages */
				break;
	}

	ipsec_log(LOG_INFO, CFSTR("IPSec Controller: ===================================================="));

}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
static void process_racoon_msg(struct service *serv)
{
	struct vpnctl_status_phase_change *phase_change_status;
	struct vpnctl_status_failed *failed_status;
	struct vpnctl_status_peer_resp *peer_resp; 
	struct sockaddr_in	redirect_addr;
	struct in_addr peer_addr;
	
	
	if (gSCNCVerbose)
		print_racoon_msg(serv);
	
	switch (ntohs(serv->u.ipsec.msghdr.msg_type)) {
			case VPNCTL_STATUS_IKE_FAILED:
				ipsec_log(LOG_ERR, CFSTR("IPSec Controller: IKE FAILED. phase %d, assert %d"), serv->u.ipsec.phase, serv->u.ipsec.asserted);
				failed_status = ALIGNED_CAST(struct vpnctl_status_failed *)serv->u.ipsec.msg;

				switch (ntohs(failed_status->ike_code)) {

					case VPNCTL_NTYPE_LOAD_BALANCE:		
						bzero(&redirect_addr, sizeof(redirect_addr));
						redirect_addr.sin_len = sizeof(redirect_addr);
						redirect_addr.sin_family = AF_INET;
						redirect_addr.sin_port = htons(0);
						redirect_addr.sin_addr.s_addr = *ALIGNED_CAST(u_int32_t*)failed_status->data;
						ipsec_log(LOG_INFO, CFSTR("IPSec Controller: connection redirected to server '%s'..."), inet_ntoa(redirect_addr.sin_addr));
						racoon_restart(serv, &redirect_addr);
						break;

					default:	
						ipsec_log(LOG_INFO, CFSTR("IPSec Controller: connection failed <IKE Error %d (0x%x) %s>"), ntohs(failed_status->ike_code), ntohs(failed_status->ike_code), ipsec_error_to_str(ntohs(failed_status->ike_code)));
						serv->u.ipsec.laststatus = ipsec_error_to_status(serv, ntohs(failed_status->from), ntohs(failed_status->ike_code));
						
						/* after phase 2, an authenticaion error is because the peer is disconnecting us */
						if ((serv->u.ipsec.laststatus == IPSEC_XAUTH_ERROR) && (serv->u.ipsec.phase >= IPSEC_PHASE2))
							serv->u.ipsec.laststatus = IPSEC_PEERDISCONNECT_ERROR;
						ipsec_stop(serv, 0);
						break;
				}

				break;

			case VPNCTL_STATUS_PH1_START_US:
				ipsec_log(LOG_INFO, CFSTR("IPSec Controller: PH1 STARTUS. phase %d, assert %d"), serv->u.ipsec.phase, serv->u.ipsec.asserted);
				if (serv->u.ipsec.phase == IPSEC_INITIALIZE) {
					ipsec_updatephase(serv, IPSEC_CONTACT);
				} else if (IPSEC_IS_ASSERTED_IDLE(serv->u.ipsec) ||
						   IPSEC_IS_ASSERTED_INITIALIZE(serv->u.ipsec)) {
					if (IPSEC_IS_ASSERTED_IDLE(serv->u.ipsec) && serv->u.ipsec.timerref) {
						CFRunLoopTimerSetNextFireDate(serv->u.ipsec.timerref, CFAbsoluteTimeGetCurrent() + TIMEOUT_INITIAL_CONTACT);
					}
					IPSEC_ASSERT_CONTACT(serv->u.ipsec);
				}
				break;

			case VPNCTL_STATUS_PH1_START_PEER:
				ipsec_log(LOG_INFO, CFSTR("IPSec Controller: PH1 STARTPEER. phase %d, assert %d"), serv->u.ipsec.phase, serv->u.ipsec.asserted);
				if (serv->u.ipsec.phase != IPSEC_CONTACT && !IPSEC_IS_ASSERTED_CONTACT(serv->u.ipsec))
					break;
				if (serv->u.ipsec.timerref) {
					CFRunLoopTimerSetNextFireDate(serv->u.ipsec.timerref, CFAbsoluteTimeGetCurrent() + TIMEOUT_PHASE1);
				}
				if (serv->u.ipsec.phase == IPSEC_CONTACT) {
					ipsec_updatephase(serv, IPSEC_PHASE1);
				} else if (IPSEC_IS_ASSERTED_CONTACT(serv->u.ipsec)) {
					IPSEC_ASSERT_PHASE1(serv->u.ipsec);
				}
				break;

			case VPNCTL_STATUS_NEED_AUTHINFO:
				ipsec_log(LOG_INFO, CFSTR("IPSec Controller: AUTHINFO. phase %d, assert %d"), serv->u.ipsec.phase, serv->u.ipsec.asserted);
				if (serv->u.ipsec.phase != IPSEC_PHASE1 && !IPSEC_IS_ASSERTED_PHASE1(serv->u.ipsec))
					break;
				if (serv->u.ipsec.phase == IPSEC_PHASE1) {
					ipsec_updatephase(serv, IPSEC_PHASE1AUTH);
				} else if (IPSEC_IS_ASSERTED_PHASE1(serv->u.ipsec)) {
					// disconnect if we have to prompt user
					if (serv->u.ipsec.xauth_flags & XAUTH_MUST_PROMPT) {
						ipsec_log(LOG_ERR, CFSTR("IPSec Controller: session asserting but XAuth dialog required, so connection aborted"));
						ipsec_stop(serv, 0);
						break;
					}
				}
				if (serv->u.ipsec.timerref) {
					CFRunLoopTimerSetNextFireDate(serv->u.ipsec.timerref, FAR_FUTURE);
				}
				IPSECLOGASLMSG("IPSec requesting Extended Authentication.\n");

				if (process_xauth_need_info(serv)) {
					ipsec_log(LOG_ERR, CFSTR("IPSec Controller: XAuth authentication failed"));
					ipsec_stop(serv, 0);
				}
				break;

			case VPNCTL_STATUS_PH1_ESTABLISHED:
				ipsec_log(LOG_INFO, CFSTR("IPSec Controller: PH1 ESTABLISHED. phase %d, assert %d"), serv->u.ipsec.phase, serv->u.ipsec.asserted);
				if (serv->u.ipsec.phase != IPSEC_PHASE1 && !IPSEC_IS_ASSERTED_PHASE1(serv->u.ipsec))
					break;
				if (serv->u.ipsec.phase == IPSEC_PHASE1) {
					phase_change_status = ALIGNED_CAST(struct vpnctl_status_phase_change *)serv->u.ipsec.msg;
					bool requestedInstall = false;
					if (serv->ne_sm_bridge != NULL) {
						if (serv->u.ipsec.modecfg_msg != NULL) {
							my_Deallocate(serv->u.ipsec.modecfg_msg, serv->u.ipsec.modecfg_msglen);
							serv->u.ipsec.modecfg_msg = NULL;
						}
						serv->u.ipsec.modecfg_msglen = serv->u.ipsec.msgtotallen + 1;
						serv->u.ipsec.modecfg_msg = my_Allocate(serv->u.ipsec.modecfg_msglen);
						memcpy(serv->u.ipsec.modecfg_msg, serv->u.ipsec.msg, serv->u.ipsec.modecfg_msglen);
						requestedInstall = ne_sm_bridge_request_install(serv->ne_sm_bridge, mode_config_is_default(serv));
						install_mode_config(serv, FALSE, TRUE);
					}
					if (!requestedInstall) {
						if (ntohs(phase_change_status->hdr.flags) & VPNCTL_FLAG_MODECFG_USED) {
							install_mode_config(serv, TRUE, TRUE);
						}
					}
					serv->u.ipsec.ping_count = MAX_PHASE2_PING;
					if (serv->u.ipsec.timerref) {
						CFRunLoopTimerSetNextFireDate(serv->u.ipsec.timerref, CFAbsoluteTimeGetCurrent() + TIMEOUT_PHASE2_PING);
					}
					serv->connecttime = mach_absolute_time() * gTimeScaleSeconds;
					serv->connectionslepttime = 0;
				} else if (IPSEC_IS_ASSERTED_PHASE1(serv->u.ipsec)) {
					if (unassert_mode_config(serv)) {
						ipsec_log(LOG_ERR, CFSTR("IPSec Controller: unassert failed"));
						ipsec_stop(serv, 0);
						break;
					}
				}
				IPSECLOGASLMSG("IPSec Phase1 established.\n");
				break;

			case VPNCTL_STATUS_PH2_START:
				ipsec_log(LOG_INFO, CFSTR("IPSec Controller: PH2 START. phase %d, assert %d"), serv->u.ipsec.phase, serv->u.ipsec.asserted);
				if (serv->u.ipsec.phase != IPSEC_PHASE1 && !IPSEC_IS_ASSERTED_PHASE1(serv->u.ipsec))
					break;
				if (serv->u.ipsec.phase == IPSEC_PHASE1) {
					if (serv->u.ipsec.timerref) {
						CFRunLoopTimerSetNextFireDate(serv->u.ipsec.timerref, CFAbsoluteTimeGetCurrent() + TIMEOUT_PHASE2);
					}
					ipsec_updatephase(serv, IPSEC_PHASE2);
				} else if (IPSEC_IS_ASSERTED_PHASE1(serv->u.ipsec)) {
					IPSEC_ASSERT_PHASE2(serv->u.ipsec);
				}
				break;

			case VPNCTL_STATUS_PH2_ESTABLISHED:
				ipsec_log(LOG_INFO, CFSTR("IPSec Controller: PH2 ESTABLISHED. phase %d, assert %d"), serv->u.ipsec.phase, serv->u.ipsec.asserted);
				if (serv->u.ipsec.phase != IPSEC_PHASE2 && !IPSEC_IS_ASSERTED_PHASE2(serv->u.ipsec))
					break;
				if (serv->u.ipsec.timerref) {
					CFRunLoopRemoveTimer(CFRunLoopGetCurrent(), serv->u.ipsec.timerref, kCFRunLoopCommonModes);
				}
				my_CFRelease(&serv->u.ipsec.timerref);
				if (serv->u.ipsec.phase == IPSEC_PHASE2) {
					if (serv->u.ipsec.banner && !(serv->flags & FLAG_ONDEMAND)) {
						display_notification(serv, serv->u.ipsec.banner, 0, dialog_has_disconnect_type);
						my_CFRelease(&serv->u.ipsec.banner);
					}
					SESSIONTRACERESTABLISHED(serv);
				} else if (IPSEC_IS_ASSERTED_PHASE2(serv->u.ipsec)) {
					IPSEC_UNASSERT(serv->u.ipsec);
				}
				ipsec_updatephase(serv, IPSEC_RUNNING);
				serv->was_running = 1;
				IPSECLOGASLMSG("IPSec Phase2 established.\n");
				break;

#if !TARGET_OS_EMBEDDED
			case VPNCTL_STATUS_NEED_REAUTHINFO:
				ipsec_log(LOG_INFO, CFSTR("IPSec Controller: REAUTHINFO. phase %d, assert %d"), serv->u.ipsec.phase, serv->u.ipsec.asserted);
				if (serv->u.ipsec.phase != IPSEC_RUNNING && !IPSEC_IS_ASSERTED_PHASE1(serv->u.ipsec))
					break;

				IPSECLOGASLMSG("IPSec requesting Extended Authentication.\n");

				// disconnect if we have to prompt user
				if (serv->u.ipsec.xauth_flags & XAUTH_MUST_PROMPT) {
					ipsec_log(LOG_ERR, CFSTR("IPSec Controller: XAuth reauthentication dialog required, so connection aborted"));
					ipsec_stop(serv, 0);
				} else if (process_xauth_need_info(serv)) {
					ipsec_log(LOG_ERR, CFSTR("IPSec Controller: XAuth reauthentication failed"));
					ipsec_stop(serv, 0);
				}
				break;
#endif /* !TARGET_OS_EMBEDDED */

			case VPNCTL_STATUS_PEER_RESP:
				ipsec_log(LOG_INFO, CFSTR("IPSec Controller: PEER RESP. phase %d, assert %d"), serv->u.ipsec.phase, serv->u.ipsec.asserted);
				peer_resp = ALIGNED_CAST(__typeof__(peer_resp))serv->u.ipsec.msg;
				ipsec_log(LOG_INFO, CFSTR("IPSec Controller:	----------------------------"));
				peer_addr.s_addr = peer_resp->address;
				ipsec_log(LOG_INFO, CFSTR("IPSec Controller:	response from address = %s"), inet_ntoa(peer_addr));
				ipsec_log(LOG_INFO, CFSTR("IPSec Controller:	ike_code = %d"), ntohs(peer_resp->ike_code));
				if (!serv->u.ipsec.awaiting_peer_resp) {
					ipsec_log(LOG_ERR, CFSTR("IPSec Controller: unsolicited peer response notification"));					
				}
				serv->u.ipsec.awaiting_peer_resp = 0;
				break;

			default:
				/* ignore other messages */
				break;
	}
}

int ipsec_install(struct service *serv)
{
	install_mode_config(serv, TRUE, FALSE);
	return 0;
}

int ipsec_uninstall(struct service *serv)
{
	uninstall_mode_config(serv, FALSE);
	return 0;
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
/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
static int
racoon_trigger_phase2(char *ifname, struct in_addr *ping)
{
	struct icmp *icp;
	int cc, i, j, nbping;    
	struct sockaddr_in whereto;	/* who to ping */
	uint8_t data[256] __attribute__ ((aligned (4)));
	int s, ifindex;
	
	s = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
	if (s < 0)
		return -1;
	
	ifindex = if_nametoindex(ifname);
	setsockopt(s, IPPROTO_IP, IP_BOUND_IF, &ifindex, sizeof(ifindex)); 
	
	whereto.sin_family = AF_INET;	/* Internet address family */
	whereto.sin_port = 0;		/* Source port */
	whereto.sin_addr.s_addr = ping->s_addr;	/* Dest. address */
	
	icp = ALIGNED_CAST(struct icmp *)data;
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

static Boolean add_ipv4_route (CFMutableArrayRef routesArray, uint32_t address, uint32_t netmask, uint32_t gateway, Boolean isExcludedRoute, char * delegateInterfaceName)
{
	CFStringRef string = NULL;
	struct in_addr addr;
	
	if (!isA_CFArray(routesArray))
	{
		return FALSE;
	}
	
	CFMutableDictionaryRef newRouteDictionary = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
	if (!isA_CFDictionary(newRouteDictionary))
	{
		return FALSE;
	}
	
	addr.s_addr = address;
	if ((string = CFStringCreateWithFormat(NULL, NULL, CFSTR(IP_FORMAT), IP_LIST(&addr.s_addr)))) {
		CFDictionarySetValue(newRouteDictionary, kSCPropNetIPv4RouteDestinationAddress, string);
		my_CFRelease(&string);
	}
	
	if (netmask) {
		addr.s_addr = netmask;
		if ((string = CFStringCreateWithFormat(NULL, NULL, CFSTR(IP_FORMAT), IP_LIST(&addr.s_addr)))) {
			CFDictionarySetValue(newRouteDictionary, kSCPropNetIPv4RouteSubnetMask, string);
			my_CFRelease(&string);
		}
	}
	
	if (gateway) {
		addr.s_addr = gateway;
		if ((string = CFStringCreateWithFormat(NULL, NULL, CFSTR(IP_FORMAT), IP_LIST(&addr.s_addr)))) {
			CFDictionarySetValue(newRouteDictionary, kSCPropNetIPv4RouteGatewayAddress, string);
			my_CFRelease(&string);
		}
	}
	
	if (isExcludedRoute && delegateInterfaceName) {
		if ((string = CFStringCreateWithFormat(NULL, NULL, CFSTR("%s"), delegateInterfaceName))) {
			CFDictionarySetValue(newRouteDictionary, kSCPropNetIPv4RouteInterfaceName, string);
			CFRelease(string);
		}
	}
	
	CFArrayAppendValue(routesArray, newRouteDictionary);
	my_CFRelease(&newRouteDictionary);
	
	return TRUE;
}


static CFArrayRef create_ipv4_route_array(struct service *serv, CFDictionaryRef ipsec_dict, struct in_addr gateway)
{
	CFMutableArrayRef routesArray = NULL;
	int	i, nb;
	CFArrayRef policies = NULL;
	struct sockaddr_in remote_net;
	char str[32];
	u_int32_t remote_prefix;
	
	routesArray = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks);
	
	policies = CFDictionaryGetValue(ipsec_dict, kRASPropIPSecPolicies);
	if (!isA_CFArray(policies) || (nb = CFArrayGetCount(policies)) == 0) {
		goto done;
	}
	
	for (i = 0; i < nb; i++) {
		CFDictionaryRef policy;
		CFStringRef policymode, policydirection, policylevel;
		
		policy = CFArrayGetValueAtIndex(policies, i);
		if (!isDictionary(policy)) {
			continue;
		}
		
		/* build policies in and out */
		
		policymode = CFDictionaryGetValue(policy, kRASPropIPSecPolicyMode);
		if (!isA_CFString(policymode) || !CFEqual(policymode, kRASValIPSecPolicyModeTunnel)) {
			continue;
		}
		
		/* if policy direction is not specified, in/out is assumed */
		policydirection = CFDictionaryGetValue(policy, kRASPropIPSecPolicyDirection);
		if (!isA_CFString(policydirection) || (!CFEqual(policydirection, kRASValIPSecPolicyDirectionOut) && !CFEqual(policydirection, kRASValIPSecPolicyDirectionInOut))) {
			continue;
		}
		
		policylevel = CFDictionaryGetValue(policy, kRASPropIPSecPolicyLevel);
		if (!isA_CFString(policylevel) || CFEqual(policylevel, kRASValIPSecPolicyLevelNone)) {
			continue; // no need for routes for 'none' policies
		}
		
		if (!CFEqual(policylevel, kRASValIPSecPolicyLevelRequire) && !CFEqual(policylevel, kRASValIPSecPolicyLevelDiscard)  && !CFEqual(policylevel, kRASValIPSecPolicyLevelUnique)) {
			continue;
		}
		
		/* get remote network */
		if (!GetStrNetFromDict(policy, kRASPropIPSecPolicyRemoteAddress, str, sizeof(str))) {
			continue;
		}
		remote_net.sin_len = sizeof(remote_net);
		remote_net.sin_family = AF_INET;
		remote_net.sin_port = htons(0);
		if (!inet_aton(str, &remote_net.sin_addr)) {
			continue;
		}
		
		uint32_t mask = 0;
		GetIntFromDict(policy, kRASPropIPSecPolicyRemotePrefix, &remote_prefix, 24);
		for (mask = 0; remote_prefix; remote_prefix--) {
			mask = (mask >> 1) | 0x80000000;
		}
		mask = htonl(mask);
		
		add_ipv4_route(routesArray, remote_net.sin_addr.s_addr, mask, gateway.s_addr, FALSE, NULL);
	}
	
done:
	return routesArray;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
static void uninstall_mode_config(struct service *serv, Boolean uninstallPolicies)
{
    CFMutableArrayRef dicts_to_remove;
	int		error = 0;
	char	*errorstr;
	
	if (serv->u.ipsec.modecfg_installed) {
		dicts_to_remove = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
		CFArrayAppendValue(dicts_to_remove, kSCEntNetIPv4);
		if (serv->u.ipsec.dummy_ipv6_installed) {
			CFArrayAppendValue(dicts_to_remove, kSCEntNetIPv6);
			serv->u.ipsec.dummy_ipv6_installed = 0;
		}
		CFArrayAppendValue(dicts_to_remove, kSCEntNetDNS);
		/* Unpublish all-at-once */
		unpublish_multiple_dicts(gDynamicStore, serv->serviceID, dicts_to_remove, TRUE);
		my_CFRelease(&dicts_to_remove);

		if (serv->u.ipsec.modecfg_defaultroute) {
			// don't need that since we unplished the whole dictionary
			// unpublish_dictentry(gDynamicStore, serv->serviceID, kSCEntNetIPv4, kSCPropNetOverridePrimary);
			serv->u.ipsec.modecfg_defaultroute = 0;						
		}

		my_CFRelease(&serv->u.ipsec.banner);

		serv->u.ipsec.modecfg_installed = 0;
	}
	if (serv->u.ipsec.modecfg_policies_installed && uninstallPolicies) {
		if (serv->u.ipsec.modecfg_policies) {
			error = IPSecRemovePolicies(serv->u.ipsec.modecfg_policies, -1,  &errorstr);
			if (error)
			ipsec_log(LOG_ERR, CFSTR("IPSec Controller: Cannot remove mode config policies, error '%s'"), errorstr);
			my_CFRelease(&serv->u.ipsec.modecfg_policies);
		}

		IPSecRemoveSecurityAssociations((struct sockaddr *)&serv->u.ipsec.our_address, (struct sockaddr *)&serv->u.ipsec.peer_address);

		clear_ifaddr(serv->if_name, serv->u.ipsec.inner_local_addr, 0xFFFFFFFF);
		my_close(serv->u.ipsec.kernctl_sock);
		serv->u.ipsec.kernctl_sock = -1;

		free_service_routes(serv);
		serv->u.ipsec.modecfg_policies_installed = 0;
	}
}

static void format_routes_for_cache (struct service *serv, struct in_addr *included_address_gateway, int isdefault)
{
	CFMutableDataRef	includedRouteAddressData = CFDataCreateMutable(kCFAllocatorDefault, 0);
	CFMutableDataRef	includedRouteMaskData = CFDataCreateMutable(kCFAllocatorDefault, 0);
	CFMutableDataRef	excludedRouteAddressData = CFDataCreateMutable(kCFAllocatorDefault, 0);
	CFMutableDataRef	excludedRouteMaskData = CFDataCreateMutable(kCFAllocatorDefault, 0);
	service_route_t *route = NULL;
	
	if (includedRouteAddressData == NULL ||
		includedRouteMaskData == NULL ||
		excludedRouteAddressData == NULL ||
		excludedRouteMaskData == NULL) {
		goto done;
	}
	
	for (route = serv->u.ipsec.routes; route != NULL; route = route->next) {
		if (route->gtwy_address.s_addr == included_address_gateway->s_addr) {
			CFDataAppendBytes(includedRouteAddressData, (uint8_t*)&route->dest_address, sizeof(struct in_addr));
			CFDataAppendBytes(includedRouteMaskData, (uint8_t*)&route->dest_mask, sizeof(struct in_addr));
		} else {
			CFDataAppendBytes(excludedRouteAddressData, (uint8_t*)&route->dest_address, sizeof(struct in_addr));
			CFDataAppendBytes(excludedRouteMaskData, (uint8_t*)&route->dest_mask, sizeof(struct in_addr));
		}
	}

	/* Send info to cache */
	CFMutableDictionaryRef routesDict = CFDictionaryCreateMutable(kCFAllocatorDefault, 1, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
	if (routesDict) {
		CFMutableDictionaryRef ipv4RoutesDict = CFDictionaryCreateMutable(kCFAllocatorDefault, 2, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
		if (ipv4RoutesDict) {
			if (CFDataGetLength(includedRouteAddressData)) {
				CFMutableDictionaryRef includedRoutesDict = CFDictionaryCreateMutable(kCFAllocatorDefault, 2, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
				if (includedRoutesDict) {
					CFDictionaryAddValue(includedRoutesDict, kSCNetworkConnectionNetworkInfoAddresses, includedRouteAddressData);
					CFDictionaryAddValue(includedRoutesDict, kSCNetworkConnectionNetworkInfoMasks, includedRouteMaskData);
					CFDictionaryAddValue(ipv4RoutesDict, kSCNetworkConnectionNetworkInfoIncludedRoutes, includedRoutesDict);
					CFRelease(includedRoutesDict);
				}
			}
			
			if (CFDataGetLength(excludedRouteAddressData)) {
				CFMutableDictionaryRef excludedRoutesDict = CFDictionaryCreateMutable(kCFAllocatorDefault, 2, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
				if (excludedRoutesDict) {
					CFDictionaryAddValue(excludedRoutesDict, kSCNetworkConnectionNetworkInfoAddresses, excludedRouteAddressData);
					CFDictionaryAddValue(excludedRoutesDict, kSCNetworkConnectionNetworkInfoMasks, excludedRouteMaskData);
					CFDictionaryAddValue(ipv4RoutesDict, kSCNetworkConnectionNetworkInfoExcludedRoutes, excludedRoutesDict);
					CFRelease(excludedRoutesDict);
				}
			}
			
			CFDictionaryAddValue(routesDict, kSCNetworkConnectionNetworkInfoIPv4, ipv4RoutesDict);
			CFRelease(ipv4RoutesDict);
		}
		scnc_cache_routing_table(serv, routesDict, FALSE, (isdefault != 0));
		CFRelease(routesDict);
	}
	
done:
	my_CFRelease(&includedRouteAddressData);
	my_CFRelease(&includedRouteMaskData);
	my_CFRelease(&excludedRouteAddressData);
	my_CFRelease(&excludedRouteMaskData);
}

static bool mode_config_is_default(struct service *serv)
{
	bool isdefault = true;
	struct isakmp_data attr;
	char *modecfg_data = (char*)serv->u.ipsec.msg + sizeof(struct vpnctl_status_phase_change) + sizeof(struct vpnctl_modecfg_params);
	int modecfg_data_len = ntohs(serv->u.ipsec.msghdr.len) - ((sizeof(struct vpnctl_status_phase_change) + sizeof(struct vpnctl_modecfg_params)) -  sizeof(struct vpnctl_hdr));
	int tlen = modecfg_data_len;
	char *dataptr = modecfg_data;
	
	while (tlen > 0)
	{
		int tlv;
		u_int16_t type;
		
		memcpy(&attr, dataptr, sizeof(attr));        // Wcast-align fix - memcpy for unaligned access
		type = ntohs(attr.type) & 0x7FFF;
		tlv = (type ==  ntohs(attr.type));
		
		
		if (type == UNITY_SPLIT_INCLUDE) {
			isdefault = false;
			break;
		}
		
		if (tlv) {
			tlen -= ntohs(attr.lorv);
			dataptr += ntohs(attr.lorv);
		}
		
		tlen -= sizeof(u_int32_t);
		dataptr += sizeof(u_int32_t);
	}
	
	return isdefault;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
static void install_mode_config(struct service *serv, Boolean installConfig, Boolean installPolicies)
{
	struct in_addr addr, mask, local_addr, remote_addr;
	char	*data;
    CFDictionaryRef dict = NULL;
    CFMutableArrayRef dicts_to_publish = NULL, dict_names_to_publish = NULL;
	int		error, isdefault = 1, len, data_len;
	char	*errorstr;
	u_int32_t dns, prefix, local_prefix, remote_prefix; 
	u_int32_t	internal_ip4_address, internal_ip4_netmask;
	CFStringRef	strRef, domain_name = NULL;
	CFMutableArrayRef split_dns_array = NULL, dns_array = NULL;
	int must_prompt;
	int unity_splitdns_name_i = 0, unity_split_include_i = 0, unity_local_lan_i = 0, unity_browser_i = 0;
	
	serv->u.ipsec.ping_addr.s_addr = 0;
	
	u_int8_t *modecfg_msg = serv->u.ipsec.modecfg_msg;
	u_int32_t modecfg_msglen = serv->u.ipsec.modecfg_msglen;
	struct vpnctl_hdr *ctl_hdr = (struct vpnctl_hdr *)modecfg_msg;
	if (modecfg_msg == NULL) {
		modecfg_msg = serv->u.ipsec.msg;
		modecfg_msglen = serv->u.ipsec.msglen;
		ctl_hdr = &serv->u.ipsec.msghdr;
	}
	
	if (modecfg_msg == NULL) {
		return;
	}
	
	struct vpnctl_status_phase_change *phase_change_status = ALIGNED_CAST(struct vpnctl_status_phase_change *)modecfg_msg;
	struct vpnctl_modecfg_params *modecfg = ALIGNED_CAST(struct vpnctl_modecfg_params *)(modecfg_msg + sizeof(struct vpnctl_status_phase_change));

	char *modecfg_data = (char*)modecfg_msg + sizeof(struct vpnctl_status_phase_change) + sizeof(struct vpnctl_modecfg_params);
	int modecfg_data_len = ntohs(ctl_hdr->len) - ((sizeof(struct vpnctl_status_phase_change) + sizeof(struct vpnctl_modecfg_params)) -  sizeof(struct vpnctl_hdr));

	CFMutableArrayRef policies_array;
	CFMutableDictionaryRef policies, policy;
		
	internal_ip4_address = htonl(0);
	internal_ip4_netmask = htonl(0xFFFFFFFF);
	
	/* first pass, get mandatory parameters */

	int tlen = modecfg_data_len;
	char	*dataptr = modecfg_data;
    struct isakmp_data attr;

	IPSECLOGASLMSG("IPSec Network Configuration started.\n");

	while (tlen > 0)
	{
		int tlv;
		u_int16_t type;
		
        memcpy(&attr, dataptr, sizeof(attr));   // Wcast-align fix - memcpy for unbaligned access		
		type = ntohs(attr.type) & 0x7FFF;
		tlv = (type ==  ntohs(attr.type));
		
		switch (type)
		{
			case INTERNAL_IP4_ADDRESS:
                // Wcast-align fix - memcpy for unaligned access
                memcpy(&internal_ip4_address, dataptr + sizeof(u_int32_t), sizeof(internal_ip4_address));   // network byte order
				addr.s_addr = internal_ip4_address;
				IPSECLOGASLMSG("IPSec Network Configuration: INTERNAL-IP4-ADDRESS = %s.\n",
							   inet_ntoa(addr));
				break;

			case INTERNAL_IP4_NETMASK:
                // Wcast-align fix - memcpy for unaligned access
                memcpy(&internal_ip4_netmask, dataptr + sizeof(u_int32_t), sizeof(internal_ip4_netmask));   // network byte order
				addr.s_addr = internal_ip4_netmask;
				IPSECLOGASLMSG("IPSec Network Configuration: INTERNAL-IP4-MASK = %s.\n",
							   inet_ntoa(addr));
				break;

			default:
				break;
		}
		
		if (tlv) {
			tlen -= ntohs(attr.lorv);
			dataptr += ntohs(attr.lorv);
		}
		
		tlen -= sizeof(u_int32_t);
		dataptr += sizeof(u_int32_t);
			
	}
	
	if (internal_ip4_address == htonl(0)) {
		// ip address is missing
		ipsec_log(LOG_ERR, CFSTR("IPSec Controller: Internal IP Address missing from Mode Config packet "));
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
		
		memcpy(&attr, dataptr, sizeof(attr));        // Wcast-align fix - memcpy for unaligned access
		type = ntohs(attr.type) & 0x7FFF;
		tlv = (type ==  ntohs(attr.type));
		
		switch (type)
		{
			case INTERNAL_IP4_DNS:
				if (!dns_array)
					dns_array = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);

				if (!dns_array)
					break;
				
                memcpy(&dns, dataptr + sizeof(u_int32_t), sizeof(u_int32_t));      // Wcast-align fix - memcpy for unaligned access
				strRef = CFStringCreateWithFormat(NULL, NULL, CFSTR(IP_FORMAT), IP_LIST(&dns));
				if (!strRef)
					break;

				CFArrayAppendValue(dns_array, strRef);
				IPSECLOGASLMSG("IPSec Network Configuration: INTERNAL-IP4-DNS = %s.\n",
							   CFStringGetCStringPtr(strRef, kCFStringEncodingMacRoman));
				CFRelease(strRef);
				break;


			default:
				break;
		}


		if (tlv) {
			tlen -= ntohs(attr.lorv);
			dataptr += ntohs(attr.lorv);
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
		IPSECLOGASLMSG("IPSec Network Configuration: DEFAULT-ROUTE = local-address %s/%d.\n",
					   inet_ntoa(addr), prefix);

		serv->u.ipsec.ping_addr.s_addr = internal_ip4_address; // will ping our own address, it will hit the (internal_ip4_address --> any) policy
		
		CFDictionarySetValue(policy, kRASPropIPSecPolicyRemoteAddress, CFSTR("0.0.0.0"));
		AddNumber(policy, kRASPropIPSecPolicyRemotePrefix, 0);

		update_service_route(serv, internal_ip4_address, 0xFFFFFFFF, 0, 0, 0, 0, 0);
		
		CFArrayAppendValue(policies_array, policy);
		CFRelease(policy);
	}

	
	CFDictionarySetValue(policies, kRASPropIPSecPolicies, policies_array);
	CFRelease(policies_array);

	if (installPolicies) {
		ipsec_log(LOG_DEBUG, CFSTR("IPSec Controller: Mode Config Policies %@"), policies);
			
		if ((error = IPSecInstallPolicies(policies, -1, &errorstr)) < 0) {
			ipsec_log(LOG_ERR, CFSTR("IPSec Controller: IPSecInstallPolicies failed '%s'"), errorstr);
			CFRelease(policies);
			goto fail;
		}
		serv->u.ipsec.modecfg_policies = (CFMutableDictionaryRef)my_CFRetain(policies);
		serv->u.ipsec.modecfg_defaultroute = isdefault;

		serv->u.ipsec.inner_local_addr = internal_ip4_address;
		serv->u.ipsec.inner_local_mask = internal_ip4_netmask;

		/* create the virtual interface */
		serv->u.ipsec.kernctl_sock = create_tun_interface(serv->if_name, sizeof(serv->if_name), &serv->if_index, UTUN_FLAGS_NO_INPUT + UTUN_FLAGS_NO_OUTPUT, 0);
		if (serv->u.ipsec.kernctl_sock == -1) {
			ipsec_log(LOG_ERR, CFSTR("IPSec Controller: cannot create tunnel interface"));
			goto fail;
		}

		if (set_tun_delegate(serv->u.ipsec.kernctl_sock, serv->u.ipsec.lower_interface)) {
			ipsec_log(LOG_ERR, CFSTR("IPSec Controller: cannot set delegate interface for tunnel interface"));
			goto fail;
		}
		
		set_ifmtu(serv->if_name, 1280); 
		set_ifaddr(serv->if_name, internal_ip4_address, internal_ip4_address,internal_ip4_netmask);

		if ((error = racoon_send_cmd_start_ph2(serv->u.ipsec.controlfd, serv->u.ipsec.peer_address.sin_addr.s_addr, policies)) != 0) {
			ipsec_log(LOG_ERR, CFSTR("IPSec Controller: racoon_send_cmd_start_ph2 failed '%s'"), errorstr);
			goto fail;
		}
		racoon_trigger_phase2(serv->if_name, &serv->u.ipsec.ping_addr);
	}

	if (installConfig) {
		SCNetworkReachabilityRef	ref;
		SCNetworkConnectionFlags	flags;
		bool 			is_peer_local;
		struct sockaddr_in ip_zeros;
		
		bzero(&ip_zeros, sizeof(ip_zeros));
		ip_zeros.sin_len = sizeof(ip_zeros);
		ip_zeros.sin_family = AF_INET;
		
		/* check if is peer on our local subnet */
		ref = SCNetworkReachabilityCreateWithAddress(NULL, (struct sockaddr *)&serv->u.ipsec.peer_address);
		is_peer_local = SCNetworkReachabilityGetFlags(ref, &flags) && (flags & kSCNetworkFlagsIsDirect);
		
#if TARGET_OS_EMBEDDED
		if (serv->u.ipsec.lower_interface[0]) {
			// Mark interface as cellular based on NWI
			serv->u.ipsec.lower_interface_cellular = interface_is_cellular(serv->u.ipsec.lower_interface);
			ipsec_log(LOG_INFO, CFSTR("IPSec Controller: lower interface (%s) is%s cellular"), serv->u.ipsec.lower_interface, serv->u.ipsec.lower_interface_cellular ? "" : " not");
		} else {
			/* Mark interface as cellular based on reachability */
			serv->u.ipsec.lower_interface_cellular = SCNetworkReachabilityGetFlags(ref, &flags) && (flags & kSCNetworkReachabilityFlagsIsWWAN);
		}
#endif
		
		CFRelease(ref);
		
		CFArrayRef includedRoutes = NULL;
		CFMutableArrayRef excludedRoutes = NULL;
		
		if (is_peer_local 
			|| (serv->u.ipsec.lower_gateway.sin_addr.s_addr == 0)) {
			
			if (serv->u.ipsec.lower_interface[0]) {
				/* subnet route */
				excludedRoutes = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks);
				add_ipv4_route(excludedRoutes, serv->u.ipsec.peer_address.sin_addr.s_addr, INADDR_BROADCAST, 0, TRUE, serv->u.ipsec.lower_interface);
			}
		}
		else {
			/* host route */
			excludedRoutes = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks);
			add_ipv4_route(excludedRoutes, serv->u.ipsec.peer_address.sin_addr.s_addr, INADDR_BROADCAST, serv->u.ipsec.lower_gateway.sin_addr.s_addr, TRUE, NULL);
		}
		
		if (!isdefault) {
			addr.s_addr = internal_ip4_address;
			if ((includedRoutes = create_ipv4_route_array(serv, policies, addr)) == NULL) {
				ipsec_log(LOG_ERR, CFSTR("IPSec Controller: create_ipv4_route_array failed"));
			}
		}
		
		if (!serv->ne_sm_bridge) {
			format_routes_for_cache(serv, &addr, isdefault);
		}
		
		dicts_to_publish = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
		dict_names_to_publish = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
		
		dict = create_stateaddr(gDynamicStore, serv->serviceID, serv->if_name, serv->u.ipsec.peer_address.sin_addr.s_addr, internal_ip4_address,internal_ip4_address, internal_ip4_netmask, isdefault, includedRoutes, excludedRoutes);
		my_CFRelease(&includedRoutes);
		my_CFRelease(&excludedRoutes);
		if (dict) {
			CFArrayAppendValue(dict_names_to_publish, kSCEntNetIPv4);
			CFArrayAppendValue(dicts_to_publish, dict);
			CFRelease(dict);
		}
		
		if (isdefault) {
			dict = create_ipv6_dummy_primary(serv->if_name);
			if (dict) {
				serv->u.ipsec.dummy_ipv6_installed = 1;
				CFArrayAppendValue(dict_names_to_publish, kSCEntNetIPv6);
				CFArrayAppendValue(dicts_to_publish, dict);
				CFRelease(dict);
			}
		}

		if (dns_array) {
			
			// add split dns array if only domain name was provided by the server
			if (!split_dns_array && domain_name) {
				split_dns_array = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
				if (split_dns_array) {
					CFArrayAppendValue(split_dns_array, domain_name);
				}
			}
			
			dict = create_dns(gDynamicStore, serv->serviceID, dns_array, domain_name, split_dns_array, FALSE);
			if (dict) {
				CFArrayAppendValue(dict_names_to_publish, kSCEntNetDNS);
				CFArrayAppendValue(dicts_to_publish, dict);
				CFRelease(dict);
			}
		}


		if (serv->ne_sm_bridge != NULL) {
			ne_sm_bridge_filter_state_dictionaries(serv->ne_sm_bridge, dict_names_to_publish, dicts_to_publish);
		}

		/* Publish all-at-once */
		publish_multiple_dicts(gDynamicStore, serv->serviceID,dict_names_to_publish, dicts_to_publish);
		ipsec_log(LOG_DEBUG, CFSTR("IPSec Controller: Published dictionaries to dynamic store."));
	}
	my_CFRelease(&split_dns_array);
	my_CFRelease(&domain_name);
	my_CFRelease(&dns_array);
	my_CFRelease(&dicts_to_publish);
	my_CFRelease(&dict_names_to_publish);
	my_CFRelease(&policies);

	if (installConfig) {
		serv->u.ipsec.modecfg_installed = 1;
	}
	if (installPolicies) {
		serv->u.ipsec.modecfg_policies_installed = 1;
	}

	IPSECLOGASLMSG("IPSec Network Configuration established.\n");

	return;
	
fail:
	my_CFRelease(&split_dns_array);
	my_CFRelease(&domain_name);
	my_CFRelease(&dns_array);
	my_CFRelease(&serv->u.ipsec.banner);
	my_CFRelease(&dicts_to_publish);
	my_CFRelease(&dict_names_to_publish);
	my_CFRelease(&policies);

	if (serv->u.ipsec.modecfg_policies) {
		error = IPSecRemovePolicies(serv->u.ipsec.modecfg_policies, -1,  &errorstr);
		my_CFRelease(&serv->u.ipsec.modecfg_policies);

	}
	if (serv->u.ipsec.modecfg_defaultroute) {
		// don't need that since we unpublished the whole dictionary
		// unpublish_dictentry(gDynamicStore, serv->serviceID, kSCEntNetIPv4, kSCPropNetOverridePrimary);
		serv->u.ipsec.modecfg_defaultroute = 0;						
	}

	my_close(serv->u.ipsec.kernctl_sock);
	serv->u.ipsec.kernctl_sock = -1;	
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
static int unassert_mode_config(struct service *serv)
{
	if (serv->u.ipsec.modecfg_installed) {
		/* we already have a config, so if phase1 is asserted-established stop timer and let racoon take care of the IPSec SAs rekeys */
		if (IPSEC_IS_ASSERTED_PHASE1(serv->u.ipsec)) {
			if (serv->u.ipsec.timerref) {
				CFRunLoopRemoveTimer(CFRunLoopGetCurrent(), serv->u.ipsec.timerref, kCFRunLoopCommonModes);
			}
			my_CFRelease(&serv->u.ipsec.timerref);
		}
		return 0;
	}
	return -1;
}

/* -----------------------------------------------------------------------------
 ----------------------------------------------------------------------------- */
int racoon_send_cmd_reconnect(int fd, u_int32_t address) 
{
	u_int8_t					data[256] __attribute__ ((aligned (4))); 		// Wcast-align fix - force alignment
	struct vpnctl_cmd_connect	*cmd_reconnect = ALIGNED_CAST(struct vpnctl_cmd_connect *)data;
	
	bzero(cmd_reconnect, sizeof(struct vpnctl_cmd_connect));
	cmd_reconnect->hdr.len = htons(sizeof(*cmd_reconnect) - sizeof(cmd_reconnect->hdr));
	cmd_reconnect->hdr.msg_type = htons(VPNCTL_CMD_RECONNECT);
	cmd_reconnect->address = address;
	ipsec_log(LOG_INFO, CFSTR("IPSec Controller: sending RECONNECT to racoon control socket"));
	write(fd, cmd_reconnect, sizeof(*cmd_reconnect));
	return 0;
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
	
	ipsec_log(LOG_INFO, CFSTR("IPSec Controller: racoon_timer expired"));
	
	/* if no contact, try next server */
	if (serv->u.ipsec.phase == IPSEC_INITIALIZE || serv->u.ipsec.phase == IPSEC_CONTACT) {

		if (serv->u.ipsec.resolvedAddress &&
			(serv->u.ipsec.next_address < CFArrayGetCount(serv->u.ipsec.resolvedAddress))) {

			dataref = CFArrayGetValueAtIndex(serv->u.ipsec.resolvedAddress, serv->u.ipsec.next_address);
			serv->u.ipsec.next_address++;
	
			bzero(&address, sizeof(address));
			range.location = 0;
			range.length = sizeof(address);
			CFDataGetBytes(dataref, range, (UInt8 *)&address); 

			ipsec_log(LOG_INFO, CFSTR("IPSec Controller: racoon_timer call racoon_restart"));
			racoon_restart(serv, &address); 
			return;
		}
	}

	/* need to resend ping ? */ 
	if (serv->u.ipsec.phase == IPSEC_PHASE1 || IPSEC_IS_ASSERTED_PHASE1(serv->u.ipsec)) {

		if (serv->u.ipsec.ping_count > 0) {
			serv->u.ipsec.ping_count--;
			//printf("timer_resend ping, addr = 0x%s\n", inet_ntoa(serv->u.ipsec.ping_addr));
			racoon_trigger_phase2(serv->if_name, &serv->u.ipsec.ping_addr);
			if (serv->u.ipsec.timerref) {
				CFRunLoopTimerSetNextFireDate(serv->u.ipsec.timerref, CFAbsoluteTimeGetCurrent() + TIMEOUT_PHASE2_PING);
			}
			return;
		}
	}
	
	/* need to continue assert ? */ 
	if (IPSEC_IS_ASSERTED_IDLE(serv->u.ipsec)) {
		racoon_send_cmd_reconnect(serv->u.ipsec.controlfd, serv->u.ipsec.peer_address.sin_addr.s_addr);
		IPSEC_ASSERT_INITIALIZE(serv->u.ipsec);
		if (serv->u.ipsec.timerref) {
			CFRunLoopTimerSetNextFireDate(serv->u.ipsec.timerref, CFAbsoluteTimeGetCurrent() + TIMEOUT_INITIAL_CONTACT);
		}
		return;
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
#if !TARGET_OS_EMBEDDED
    // increase the timeout if we're waiting for a wireless interface
    if (IFM_TYPE(interface_media) == IFM_IEEE80211) {
        scaled_interface_timeout = (TIMEOUT_INTERFACE_CHANGE << 2);
    }
#endif /* !iPhone */
    ipsec_log(LOG_INFO, CFSTR("getting interface (media %x) timeout for ipsec: %d secs"), interface_media, scaled_interface_timeout);
    return scaled_interface_timeout;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
static 
void event_timer(CFRunLoopTimerRef timer, void *info)
{
	struct service *serv = info;
	
	ipsec_log(LOG_INFO, CFSTR("IPSec Controller: Network change event timer expired"));
	
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
//	ipsec_log(LOG_INFO, CFSTR("IPSec Controller: racoon_callback, failed to read header, closing"));
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
					ipsec_log(LOG_INFO, CFSTR("IPSec Controller: racoon_callback, header ="));
					ipsec_log(LOG_INFO, CFSTR("IPSec Controller: racoon_callback,   msg_type = 0x%x"), serv->u.ipsec.msghdr.msg_type);
					ipsec_log(LOG_INFO, CFSTR("IPSec Controller: racoon_callback,   flags = 0x%x"), serv->u.ipsec.msghdr.flags);
					ipsec_log(LOG_INFO, CFSTR("IPSec Controller: racoon_callback,   cookie = 0x%x"), serv->u.ipsec.msghdr.cookie);
					ipsec_log(LOG_INFO, CFSTR("IPSec Controller: racoon_callback,   reserved = 0x%x"), serv->u.ipsec.msghdr.reserved);
					ipsec_log(LOG_INFO, CFSTR("IPSec Controller: racoon_callback,   result = 0x%x"), serv->u.ipsec.msghdr.result);
					ipsec_log(LOG_INFO, CFSTR("IPSec Controller: racoon_callback,   len = %d"), serv->u.ipsec.msghdr.len);
#endif
              }
        }
    }

    /* first read the data part of the message */
    if (serv->u.ipsec.msglen >= sizeof(struct vpnctl_hdr)) {
//	ipsec_log(LOG_INFO, CFSTR("IPSec Controller: racoon_callback, len to read = %d"), serv->u.ipsec.msgtotallen - serv->u.ipsec.msglen);

        n = readn(s, &serv->u.ipsec.msg[serv->u.ipsec.msglen], serv->u.ipsec.msgtotallen - serv->u.ipsec.msglen);
        switch (n) {
            case -1:
 //	ipsec_log(LOG_INFO, CFSTR("IPSec Controller: racoon_callback, failed to read payload, closing"));
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
			ipsec_log(LOG_INFO, CFSTR("IPSec Controller: connection closed by client, call ipsec_stop"));
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

static int
IPSecCheckVPNInterfaceOrServiceUnrecoverable (SCDynamicStoreRef dynamicStoreRef,
											  const char            *location,
											  struct kern_event_msg *ev_msg,
											  char                  *interface_buf)
{
	//SCDynamicStoreRef dynamicStoreRef = (SCDynamicStoreRef)dynamicStore;
	
	// return 1, if this is a delete event, and;
	// TODO: add support for IPv6 <rdar://problem/5920237>
	// walk Setup:/Network/Service/* and check if there are service entries referencing this interface. e.g. Setup:/Network/Service/44DB8790-0177-4F17-8D4E-37F9413D1D87/Interface:DeviceName == interface, other_serv_found = 1
	// Setup:/Network/Interface/"interface"/AirPort:'PowerEnable' == 0 || Setup:/Network/Interface/"interface"/IPv4 is missing, interf_down = 1
	if (!dynamicStoreRef)
		syslog(LOG_DEBUG, "%s: invalid SCDynamicStore reference", location);
	
	if (dynamicStoreRef &&
	    (ev_msg->event_code == KEV_INET_ADDR_DELETED || ev_msg->event_code == KEV_INET_CHANGED_ADDR)) {
		CFStringRef       interf_key;
		CFMutableArrayRef interf_keys;
		CFStringRef       pattern;
		CFMutableArrayRef patterns;
		CFDictionaryRef   dict = NULL;
		CFIndex           i;
		const void *      keys_q[128];
		const void **     keys = keys_q;
		const void *      values_q[128];
		const void **     values = values_q;
		CFIndex           n;
		CFStringRef       vpn_if;
		int               other_serv_found = 0, interf_down = 0;
		
		vpn_if = CFStringCreateWithCStringNoCopy(NULL,
												 interface_buf,
												 kCFStringEncodingASCII,
												 kCFAllocatorNull);
		if (!vpn_if) {
			// if we could not initialize interface CFString
			syslog(LOG_NOTICE, "%s: failed to initialize interface CFString", location);
			goto done;
		}
		
		interf_keys = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
		patterns = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
		// get Setup:/Network/Interface/<vpn_if>/Airport
		interf_key = SCDynamicStoreKeyCreateNetworkInterfaceEntity(NULL,
																   kSCDynamicStoreDomainSetup,
																   vpn_if,
																   kSCEntNetAirPort);
		CFArrayAppendValue(interf_keys, interf_key);
		CFRelease(interf_key);
		// get State:/Network/Interface/<vpn_if>/Airport
		interf_key = SCDynamicStoreKeyCreateNetworkInterfaceEntity(NULL,
																   kSCDynamicStoreDomainState,
																   vpn_if,
																   kSCEntNetAirPort);
		CFArrayAppendValue(interf_keys, interf_key);
		CFRelease(interf_key);
		// get Setup:/Network/Service/*/Interface
		pattern = SCDynamicStoreKeyCreateNetworkServiceEntity(NULL,
															  kSCDynamicStoreDomainSetup,
															  kSCCompAnyRegex,
															  kSCEntNetInterface);
		CFArrayAppendValue(patterns, pattern);
		CFRelease(pattern);
		// get Setup:/Network/Service/*/IPv4
		pattern = SCDynamicStoreKeyCreateNetworkServiceEntity(NULL,
															  kSCDynamicStoreDomainSetup,
															  kSCCompAnyRegex,
															  kSCEntNetIPv4);
		CFArrayAppendValue(patterns, pattern);
		CFRelease(pattern);
		dict = SCDynamicStoreCopyMultiple(dynamicStoreRef, interf_keys, patterns);
		CFRelease(interf_keys);
		CFRelease(patterns);
		
		if (!dict) {
			// if we could not access the SCDynamicStore
			syslog(LOG_NOTICE, "%s: failed to initialize SCDynamicStore dictionary", location);
			CFRelease(vpn_if);
			goto done;
		}
		// look for the service which matches the provided prefixes
		n = CFDictionaryGetCount(dict);
		if (n <= 0) {
			syslog(LOG_NOTICE, "%s: empty SCDynamicStore dictionary", location);
			CFRelease(vpn_if);
			goto done;
		}
		if (n > (CFIndex)(sizeof(keys_q) / sizeof(CFTypeRef))) {
			keys   = CFAllocatorAllocate(NULL, n * sizeof(CFTypeRef), 0);
			values = CFAllocatorAllocate(NULL, n * sizeof(CFTypeRef), 0);
		}
		CFDictionaryGetKeysAndValues(dict, keys, values);
		for (i=0; i < n; i++) {
			CFStringRef     s_key  = (CFStringRef)keys[i];
			CFDictionaryRef s_dict = (CFDictionaryRef)values[i];
			CFStringRef     s_if;
			
			if (!isA_CFString(s_key) || !isA_CFDictionary(s_dict)) {
				continue;
			}
			
			if (CFStringHasSuffix(s_key, kSCEntNetInterface)) {
				// is a Service Interface entity
				s_if = CFDictionaryGetValue(s_dict, kSCPropNetInterfaceDeviceName);
				if (isA_CFString(s_if) && CFEqual(vpn_if, s_if)) {
					CFArrayRef        components;
					CFStringRef       serviceIDRef = NULL, serviceKey = NULL;
					CFPropertyListRef serviceRef = NULL;
					
					other_serv_found = 1;
					// extract service ID
					components = CFStringCreateArrayBySeparatingStrings(NULL, s_key, CFSTR("/"));
					if (CFArrayGetCount(components) > 3) {
						serviceIDRef = CFArrayGetValueAtIndex(components, 3);
						//if (new key) Setup:/Network/Service/service_id/IPv4 is missing, then interf_down = 1
						serviceKey = SCDynamicStoreKeyCreateNetworkServiceEntity(0, kSCDynamicStoreDomainSetup, serviceIDRef, kSCEntNetIPv4);
						if (!serviceKey ||
						    !(serviceRef = CFDictionaryGetValue(dict, serviceKey))) {
							syslog(LOG_NOTICE, "%s: detected disabled IPv4 Config", location);
							interf_down = 1;
						}
						if (serviceKey) CFRelease(serviceKey);
					}
					if (components) CFRelease(components);
					if (interf_down) break;
				}
				continue;
			} else if (CFStringHasSuffix(s_key, kSCEntNetAirPort)) {
				// Interface/<vpn_if>/Airport entity
				if (CFStringHasPrefix(s_key, kSCDynamicStoreDomainSetup)) {
					CFBooleanRef powerEnable = CFDictionaryGetValue(s_dict, SC_AIRPORT_POWERENABLED_KEY);
					if (isA_CFBoolean(powerEnable) &&
					    CFEqual(powerEnable, kCFBooleanFalse)) {
						syslog(LOG_NOTICE, "%s: detected AirPort, PowerEnable == FALSE", location);
						interf_down = 1;
						break;
					}
				} else if (CFStringHasPrefix(s_key, kSCDynamicStoreDomainState)) {
					UInt16      temp;
					CFNumberRef airStatus = CFDictionaryGetValue(s_dict, SC_AIRPORT_POWERSTATUS_KEY);
					if (isA_CFNumber(airStatus) &&
					    CFNumberGetValue(airStatus, kCFNumberShortType, &temp)) {
						if (temp ==0) {
							syslog(LOG_NOTICE, "%s: detected AirPort, PowerStatus == 0", location);
						}
					}
				}
				continue;
			}
		}
		if (vpn_if) CFRelease(vpn_if);
		if (keys != keys_q) {
			CFAllocatorDeallocate(NULL, keys);
			CFAllocatorDeallocate(NULL, values);
		}
		done :
		if (dict) CFRelease(dict);
		
		return (other_serv_found == 0 || interf_down == 1);             
	}
	return 0;
}

static int
IPSecCheckVPNInterfaceAddressChange (int                    transport_down,
                                     struct kern_event_msg *ev_msg,
                                     char                  *interface_buf,
                                     struct in_addr        *our_address,
                                     struct service        *serv)
{
    struct kev_in_data *inetdata;
	
    /* if transport is still down: ignore deletes, and check if the underlying interface's address has changed (ignore link-local addresses) */
    if (transport_down &&
        (ev_msg->event_code == KEV_INET_NEW_ADDR || ev_msg->event_code == KEV_INET_CHANGED_ADDR)) {
 		inetdata = (struct kev_in_data *) &ev_msg->event_data[0];
#if 0
        syslog(LOG_NOTICE, "%s: checking for interface address change. underlying %s, old-addr %x, new-addr %x\n",
               __FUNCTION__, interface_buf, our_address->s_addr, inetdata->ia_addr.s_addr);
#endif
        /* check if address changed */
        if (our_address->s_addr != inetdata->ia_addr.s_addr &&
            !IN_LINKLOCAL(ntohl(inetdata->ia_addr.s_addr))) {
            return 1;
        }
        // check to see if network has changed (despite the address)
        if (DID_VPN_LOCATIONCHANGE(serv)) {
            syslog(LOG_NOTICE, "%s: the underlying interface has changed networks\n",
                   __FUNCTION__);
            return 1;
        }
    }
    
    return 0;
}

static int
IPSecCheckVPNInterfaceAddressAlternate (int                    transport_down,
                                        struct kern_event_msg *ev_msg,
                                        char                  *interface_buf)
{
    struct kev_in_data *inetdata;
    
    /* if transport is still down: ignore deletes, and check if any interface address has alternative address */
    if (transport_down &&
        (ev_msg->event_code == KEV_INET_NEW_ADDR || ev_msg->event_code == KEV_INET_CHANGED_ADDR)) {
 		inetdata = (struct kev_in_data *) &ev_msg->event_data[0];
#if 0
        syslog(LOG_NOTICE, "%s: checking for alternate interface. underlying %s, new-addr %x\n",
               __FUNCTION__, interface_buf, inetdata->ia_addr.s_addr);
#endif
        /* check if address changed */
        if (!IN_LINKLOCAL(ntohl(inetdata->ia_addr.s_addr))) {
            return 1;
        }
    }
    
    return 0;
}

void
ipsec_network_event(struct service *serv, struct kern_event_msg *ev_msg)
{
	CFRunLoopTimerContext context = { 0, serv, NULL, NULL, NULL };
	int found;
	struct ifaddrs *ifap = NULL;
    char ev_if[32];
	struct kev_in_data *inetdata;
    
	inetdata = (struct kev_in_data *) &ev_msg->event_data[0];
	IPSecLogVPNInterfaceAddressEvent(__FUNCTION__, ev_msg, serv->u.ipsec.timeout_lower_interface_change, serv->u.ipsec.lower_interface, &serv->u.ipsec.our_address.sin_addr);

	// If the configuration is On Demand, switch from Cell to WiFi
	if (serv->ne_sm_bridge != NULL &&
		serv->flags & FLAG_SETUP_ONDEMAND &&
		serv->u.ipsec.lower_interface_cellular) {
		Boolean hasPrimaryInterface = FALSE;
		Boolean isCellular = primary_interface_is_cellular(&hasPrimaryInterface);
		if (hasPrimaryInterface && !isCellular) {
			ipsec_log(LOG_INFO, CFSTR("IPSec Controller: Disconnecting tunnel over cellular in favor of better interface"));
			serv->u.ipsec.laststatus = IPSEC_NETWORKCHANGE_ERROR;
			ipsec_stop(serv, 0);
			return;
		}
	}

	switch (ev_msg->event_code) {
		case KEV_INET_NEW_ADDR:
		case KEV_INET_CHANGED_ADDR:
		case KEV_INET_ADDR_DELETED:
			snprintf(ev_if, sizeof(ev_if), "%s%d", inetdata->link_data.if_name, inetdata->link_data.if_unit);
			// check if changes occured on the interface we are using
			if (!strncmp(ev_if, serv->u.ipsec.lower_interface, sizeof(serv->u.ipsec.lower_interface))) {
				if (inetdata->link_data.if_family == APPLE_IF_FAM_PPP) {
					// disconnect immediately
					ipsec_log(LOG_INFO, CFSTR("IPSec Controller: Network changed on underlying PPP interface"));
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
									&& (ALIGNED_CAST(struct sockaddr_in *)ifa->ifa_addr)->sin_addr.s_addr == serv->u.ipsec.our_address.sin_addr.s_addr);
						}
						freeifaddrs(ifap);
					}

					if (found) {
						// no meaningful change, or address came back. Cancel timer if it was on.
						if (serv->u.ipsec.interface_timerref) {
							ipsec_updatephase(serv, IPSEC_RUNNING);
							ipsec_log(LOG_ERR, CFSTR("IPSec Controller: Network changed, address came back on underlying interface, cancel timer"));
							CFRunLoopRemoveTimer(CFRunLoopGetCurrent(), serv->u.ipsec.interface_timerref, kCFRunLoopCommonModes);
							my_CFRelease(&serv->u.ipsec.interface_timerref);

							// check to see if network has changed (despite the address)
							if (DISCONNECT_VPN_IFLOCATIONCHANGED(serv)) {
								ipsec_log(LOG_ERR, CFSTR("IPSec Controller: the underlying interface %s network changed."),
										serv->u.ipsec.lower_interface);
								serv->u.ipsec.laststatus = IPSEC_NETWORKCHANGE_ERROR;	
								ipsec_stop(serv, 0);
								break;			
							}
							if (serv->flags & FLAG_ONDEMAND) {
								racoon_send_cmd_start_dpd(serv->u.ipsec.controlfd, serv->u.ipsec.peer_address.sin_addr.s_addr);
								serv->u.ipsec.awaiting_peer_resp = 1;
							} else {
								ipsec_log(LOG_INFO, CFSTR("IPSec Controller: asserting connection"));
								racoon_send_cmd_assert(serv);
							}
						}
					}
					else {
						// quick exit if there has been an unrecoverable change in interface/service
						if (IPSecCheckVPNInterfaceOrServiceUnrecoverable(gDynamicStore,
									__FUNCTION__,
									ev_msg,
									serv->u.ipsec.lower_interface))
						{
							ipsec_log(LOG_ERR, CFSTR("IPSec Controller: the underlying interface/service has changed unrecoverably."));
							serv->u.ipsec.laststatus = IPSEC_NETWORKCHANGE_ERROR;   
							ipsec_stop(serv, 0);
							break;
						}

						// no address, arm timer if not there
						if (!serv->u.ipsec.interface_timerref) {
							ipsec_log(LOG_ERR, CFSTR("IPSec Controller: Network changed, address disappeared on underlying interface, install timer %d secs"), serv->u.ipsec.timeout_lower_interface_change);
							serv->u.ipsec.interface_timerref = CFRunLoopTimerCreate(NULL, CFAbsoluteTimeGetCurrent() + serv->u.ipsec.timeout_lower_interface_change, FAR_FUTURE, 0, 0, event_timer, &context);
							if (!serv->u.ipsec.interface_timerref) {
								ipsec_log(LOG_ERR, CFSTR("IPSec Controller: Network changed, cannot create RunLoop timer"));
								// disconnect immediately
								serv->u.ipsec.laststatus = IPSEC_NETWORKCHANGE_ERROR;	
								ipsec_stop(serv, 0);
								break;
							}
							ipsec_updatephase(serv, IPSEC_WAITING);
							CFRunLoopAddTimer(CFRunLoopGetCurrent(), serv->u.ipsec.interface_timerref, kCFRunLoopCommonModes);
							if (serv->u.ipsec.port_mapping_timerref) {
								CFRunLoopRemoveTimer(CFRunLoopGetCurrent(), serv->u.ipsec.port_mapping_timerref, kCFRunLoopCommonModes);
								my_CFRelease(&serv->u.ipsec.port_mapping_timerref);
							}
							(void)DISCONNECT_VPN_IFLOCATIONCHANGED(serv);
						} else {
							// transport is still down: check if there was a valid address change
							if (IPSecCheckVPNInterfaceAddressChange(serv->u.ipsec.phase == IPSEC_WAITING,
										/* && serv->u.ipsec.interface_timerref, */
										ev_msg,
										serv->u.ipsec.lower_interface,
										&serv->u.ipsec.our_address.sin_addr,
										serv))
							{
								// disconnect immediately
								ipsec_log(LOG_ERR, CFSTR("IPSec Controller: the underlying interface %s address changed."),
										serv->u.ipsec.lower_interface);
								serv->u.ipsec.laststatus = IPSEC_NETWORKCHANGE_ERROR;	
								ipsec_stop(serv, 0);
							} else {
								(void)DISCONNECT_VPN_IFLOCATIONCHANGED(serv);
							}
						}
					}
				}
			} else {
				if (IPSecCheckVPNInterfaceAddressAlternate((serv->u.ipsec.phase == IPSEC_WAITING && serv->u.ipsec.interface_timerref), ev_msg, serv->u.ipsec.lower_interface))
				{
					ipsec_log(LOG_ERR, CFSTR("IPSec Controller: an alternative interface %s was detected while the underlying interface %s was down."), ev_if, serv->u.ipsec.lower_interface);
					serv->u.ipsec.laststatus = IPSEC_NETWORKCHANGE_ERROR;	
					ipsec_stop(serv, 0);
				}
			}
			break;
	}
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
static 
void event_callback(CFSocketRef inref, CFSocketCallBackType type,
                     CFDataRef address, const void *data, void *info)
{
	int s = CFSocketGetNative(inref);
	struct service *serv = info;
	char buf[256] __attribute__ ((aligned(4))); 		// Wcast-align fix - force alignment
	struct kern_event_msg *ev_msg;

	if (recv(s, &buf, sizeof(buf), 0) != -1) {
		ev_msg = ALIGNED_CAST(struct kern_event_msg *) &buf;
		ipsec_network_event(serv, ev_msg);
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
		ipsec_log(LOG_ERR, CFSTR("IPSec Controller: cannot create racoon control socket (errno = %d) "), errno);
		goto fail;
	}

	bzero(&sun, sizeof(sun));
	sun.sun_family = AF_LOCAL;
	strncpy(sun.sun_path, "/var/run/vpncontrol.sock", sizeof(sun.sun_path));

	if (connect(serv->u.ipsec.controlfd, (struct sockaddr *)&sun, sizeof(sun)) < 0) {
		ipsec_log(LOG_ERR, CFSTR("IPSec Controller: cannot connect racoon control socket (errno = %d)"), errno);
		goto fail;
	}

    if ((flags = fcntl(serv->u.ipsec.controlfd, F_GETFL)) == -1
	|| fcntl(serv->u.ipsec.controlfd, F_SETFL, flags | O_NONBLOCK) == -1) {
        ipsec_log(LOG_ERR, CFSTR("IPSec Controller: Couldn't set client socket in non-blocking mode, errno = %d"), errno);
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
static
void dns_start_query_cf_callback(CFMachPortRef port, void *msg, CFIndex size, void *info)
{
	struct service	*serv = (struct service *)info;

	if (port != serv->u.ipsec.dnsPort) {
		// we've received a callback on the async DNS port but since the
		// associated CFMachPort doesn't match than the request must have
		// already been cancelled.
		return;
	}

	CFMachPortInvalidate(serv->u.ipsec.dnsPort);
	CFRelease(serv->u.ipsec.dnsPort);
	serv->u.ipsec.dnsPort = NULL;

	getaddrinfo_async_handle_reply(msg);
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
			if (newAddress != NULL) {
				if (!CFArrayContainsValue(addresses, range, newAddress)) {
					CFArrayAppendValue(addresses, newAddress);
					range.length++;
				}
				CFRelease(newAddress);
			}
		}

		/* save the resolved address[es] */
		my_CFRelease(&serv->u.ipsec.resolvedAddress);
		serv->u.ipsec.resolvedAddress      = addresses;
		serv->u.ipsec.resolvedAddressError = NETDB_SUCCESS;
		
		ipsec_log(LOG_DEBUG, CFSTR("IPSec Controller: dns reply: resolvedAddress %@"), serv->u.ipsec.resolvedAddress);

		if (!CFArrayGetCount(serv->u.ipsec.resolvedAddress)) {
			ipsec_log(LOG_INFO, CFSTR("IPSec Controller: dns reply: no IPv4 address in reply"));
			goto fail;
		}
        
		/* get the first address and start racoon */
		dataref = CFArrayGetValueAtIndex(serv->u.ipsec.resolvedAddress, 0);
		if (!dataref || CFDataGetLength(dataref) < sizeof(address)) {
			ipsec_log(LOG_INFO, CFSTR("IPSec Controller: dns reply: failed to get elem %d from addr array"),
			      serv->u.ipsec.next_address);
			goto fail;
		}
		bzero(&address, sizeof(address));
		range.location = 0;
		range.length = sizeof(address);
		CFDataGetBytes(dataref, range, (UInt8 *)&address); 
		serv->u.ipsec.next_address = 1;

		racoon_restart(serv, &address);
		goto done;
	}
	else {
		
		ipsec_log(LOG_INFO, CFSTR("IPSec Controller: dns reply: getaddrinfo() failed: %s"), gai_strerror(status));
		goto fail;
	}
    
fail:
	ipsec_log(LOG_INFO, CFSTR("IPSec Controller: dns reply: Stopping service"));
	serv->u.ipsec.laststatus = IPSEC_RESOLVEADDRESS_ERROR;
	ipsec_stop(serv, 0);
	/* save the error associated with the attempt to resolve the name */
	serv->u.ipsec.resolvedAddress      = CFRetain(kCFNull);
	serv->u.ipsec.resolvedAddressError = status;

done:
	ipsec_log(LOG_DEBUG, CFSTR("IPSec Controller: dns reply: done"));
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

	serv->u.ipsec.dnsPort = _SC_CFMachPortCreateWithPort("PPPController/DNS", port, dns_start_query_cf_callback, &context);
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

	cmd_xauth_info = ALIGNED_CAST(struct vpnctl_cmd_xauth_info *)data;
	bzero(cmd_xauth_info, sizeof(struct vpnctl_cmd_xauth_info));
	cmd_xauth_info->hdr.msg_type = htons(VPNCTL_CMD_XAUTH_INFO);
	cmd_xauth_info->hdr.len = htons(totlen - sizeof(struct vpnctl_hdr));
	cmd_xauth_info->address = address;

	p = data + sizeof(struct vpnctl_cmd_xauth_info);

	for (i = 0; i < isakmp_nb; i++) {
		struct isakmp_data d;
        
		CFIndex olen, len = (isakmp_array[i].str ? CFStringGetLength(isakmp_array[i].str) : 0);
		CFRange range;
		range.location = 0;
		range.length = len;
		
		d.type = htons(isakmp_array[i].type);
		d.lorv = htons(len);
		memcpy(p, &d, sizeof(struct isakmp_data));      // Wcast-align fix - memcpy for unaligned access
		p += sizeof(u_int32_t);		// skip isakmp_data
		if (len) {
			CFStringGetBytes(isakmp_array[i].str, range, kCFStringEncodingUTF8, 0, false, p, len, &olen);		
			p += len;
		}
	}

	write(fd, data, totlen);

	IPSECLOGASLMSG("IPSec sending Extended Authentication.\n");

	free(data);
	return 0;
}


/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
int racoon_send_cmd_connect(int fd, u_int32_t address) 
{
	u_int8_t					data[256] __attribute__ ((aligned (4))); 		// Wcast-align fix - force alignment
	struct vpnctl_cmd_connect	*cmd_connect = ALIGNED_CAST(struct vpnctl_cmd_connect *)data;

	bzero(cmd_connect, sizeof(struct vpnctl_cmd_connect));
	cmd_connect->hdr.len = htons(sizeof(struct vpnctl_cmd_connect) - sizeof(struct vpnctl_hdr));
	cmd_connect->hdr.msg_type = htons(VPNCTL_CMD_CONNECT);
	cmd_connect->address = address;
	ipsec_log(LOG_INFO, CFSTR("IPSec Controller: sending CONNECT to racoon control socket"));
	write(fd, cmd_connect, sizeof(struct vpnctl_cmd_connect));
	IPSECLOGASLMSG("IPSec Phase1 starting.\n"); // connect command triggers phase1
	return 0;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
int racoon_send_cmd_disconnect(int fd, u_int32_t address) 
{
	u_int8_t					data[256] __attribute__ ((aligned (4))); 		// Wcast-align fix - force alignment
	struct vpnctl_cmd_connect	*cmd_connect = ALIGNED_CAST(struct vpnctl_cmd_connect *)data;

	bzero(cmd_connect, sizeof(struct vpnctl_cmd_connect));
	cmd_connect->hdr.len = htons(sizeof(struct vpnctl_cmd_connect) - sizeof(struct vpnctl_hdr));
	cmd_connect->hdr.msg_type = htons(VPNCTL_CMD_DISCONNECT);
	cmd_connect->address = address;
	ipsec_log(LOG_INFO, CFSTR("IPSec Controller: sending DISCONNECT to racoon control socket, address 0x%x"), ntohl(address));
	write(fd, cmd_connect, sizeof(struct vpnctl_cmd_connect));
	return 0;
}

/* -----------------------------------------------------------------------------
 ----------------------------------------------------------------------------- */
int racoon_send_cmd_start_dpd(int fd, u_int32_t address) 
{
	u_int8_t					data[256] __attribute__ ((aligned (4))); 		// Wcast-align fix - force alignment
	struct vpnctl_cmd_start_dpd	*cmd_start_dpd = ALIGNED_CAST(struct vpnctl_cmd_start_dpd *)data;
	
	bzero(cmd_start_dpd, sizeof(struct vpnctl_cmd_start_dpd));
	cmd_start_dpd->hdr.len = htons(sizeof(struct vpnctl_cmd_start_dpd) - sizeof(struct vpnctl_hdr));
	cmd_start_dpd->hdr.msg_type = htons(VPNCTL_CMD_START_DPD);
	cmd_start_dpd->address = address;
	ipsec_log(LOG_INFO, CFSTR("IPSec Controller: sending START_DPD to racoon control socket"));
	write(fd, cmd_start_dpd, sizeof(struct vpnctl_cmd_start_dpd));
	return 0;
}


/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
int racoon_send_cmd_bind(int fd, u_int32_t address, char *version) 
{
	u_int8_t					data[256] __attribute__ ((aligned (4))); 		// Wcast-align fix - force alignment
	struct vpnctl_cmd_bind	*cmd_bind = ALIGNED_CAST(struct vpnctl_cmd_bind *)data;
	int vers_len = 0;
	
	if (version)
		vers_len = strlen(version);
		
	bzero(cmd_bind, sizeof(struct vpnctl_cmd_bind));
	cmd_bind->hdr.len = htons(sizeof(struct vpnctl_cmd_bind) - sizeof(struct vpnctl_hdr) + vers_len);
	cmd_bind->hdr.msg_type = htons(VPNCTL_CMD_BIND);
	cmd_bind->address = address;
	cmd_bind->vers_len = htons(vers_len);
	ipsec_log(LOG_INFO, CFSTR("IPSec Controller: sending BIND to racoon control socket"));
	write(fd, cmd_bind, sizeof(struct vpnctl_cmd_bind));
	if (vers_len)
		write(fd, version, vers_len);
	return 0;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
int racoon_send_cmd_unbind(int fd, u_int32_t address) 
{
	u_int8_t					data[256] __attribute__ ((aligned (4))); 		// Wcast-align fix - force alignment
	struct vpnctl_cmd_unbind	*cmd_unbind = ALIGNED_CAST(struct vpnctl_cmd_unbind *)data;

	bzero(cmd_unbind, sizeof(struct vpnctl_cmd_unbind));
	cmd_unbind->hdr.len = htons(sizeof(struct vpnctl_cmd_unbind) - sizeof(struct vpnctl_hdr));
	cmd_unbind->hdr.msg_type = htons(VPNCTL_CMD_UNBIND);
	cmd_unbind->address = address;
	ipsec_log(LOG_INFO, CFSTR("IPSec Controller: sending UNBIND to racoon control socket"));
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
			sa = ALIGNED_CAST(struct vpnctl_sa_selector *)p;
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
			sa = ALIGNED_CAST(struct vpnctl_sa_selector *)p;
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
	
	algo = ALIGNED_CAST(struct vpnctl_algo *)p;
	algo->algo_class = htons(algclass_ipsec_enc);
	algo->algo = htons(algtype_aes);
	algo->key_len = htons(256);
	
	p += sizeof(struct vpnctl_algo);
	algo = ALIGNED_CAST(struct vpnctl_algo *)p;
	algo->algo_class = htons(algclass_ipsec_enc);
	algo->algo = htons(algtype_aes);
	algo->key_len = htons(0);

	p += sizeof(struct vpnctl_algo);
	algo = ALIGNED_CAST(struct vpnctl_algo *)p;
	algo->algo_class = htons(algclass_ipsec_enc);
	algo->algo = htons(algtype_3des);
	algo->key_len = htons(0);
	
	p += sizeof(struct vpnctl_algo);
	algo = ALIGNED_CAST(struct vpnctl_algo *)p;
	algo->algo_class = htons(algclass_ipsec_auth);
	algo->algo = htons(algtype_hmac_sha1);
	algo->key_len = htons(0);

	p += sizeof(struct vpnctl_algo);
	algo = ALIGNED_CAST(struct vpnctl_algo *)p;
	algo->algo_class = htons(algclass_ipsec_auth);
	algo->algo = htons(algtype_hmac_md5);
	algo->key_len = htons(0);

	p += sizeof(struct vpnctl_algo);
	algo = ALIGNED_CAST(struct vpnctl_algo *)p;
	algo->algo_class = htons(algclass_ipsec_comp);
	algo->algo = htons(algtype_deflate);
	algo->key_len = htons(0);
	
	msglen = sizeof(struct vpnctl_cmd_start_ph2) +
					(selector_count * sizeof(struct vpnctl_sa_selector)) +
					(NB_ALGOS * sizeof(struct vpnctl_algo));
	cmd_start_ph2->hdr.len = htons(msglen - sizeof(struct vpnctl_hdr));

	ipsec_log(LOG_INFO, CFSTR("IPSec Controller: sending START_PH2 to racoon control socket"));
	write(fd, cmd_start_ph2, msglen);
	IPSECLOGASLMSG("IPSec Phase2 starting.\n");

	free(cmd_start_ph2);
	return 0;
	
fail:
	ipsec_log(LOG_ERR, CFSTR("IPSec Controller: failed to start phase2 - '%s'"), errstr);
	if (cmd_start_ph2)
		free(cmd_start_ph2);
	return -1;
}

/* -----------------------------------------------------------------------------
 ----------------------------------------------------------------------------- */
int racoon_send_cmd_assert (struct service *serv) 
{
	struct vpnctl_cmd_assert msg;
	CFRunLoopTimerContext    context = { 0, serv, NULL, NULL, NULL };

	bzero(&msg, sizeof(msg));
	msg.hdr.msg_type = htons(VPNCTL_CMD_ASSERT);
	msg.src_address = serv->u.ipsec.our_address.sin_addr.s_addr;
	msg.dst_address = serv->u.ipsec.peer_address.sin_addr.s_addr;
	msg.hdr.len = htons(sizeof(msg) - sizeof(msg.hdr));;

	write(serv->u.ipsec.controlfd, &msg, sizeof(msg));
	serv->u.ipsec.ping_count = 0;

	IPSEC_ASSERT_IDLE(serv->u.ipsec);
	if (!serv->u.ipsec.timerref) {
		serv->u.ipsec.timerref = CFRunLoopTimerCreate(NULL, CFAbsoluteTimeGetCurrent() + TIMEOUT_ASSERT_IDLE, FAR_FUTURE, 0, 0, racoon_timer, &context);
		if (!serv->u.ipsec.timerref) {
			ipsec_log(LOG_ERR, CFSTR("IPSec Controller: assert cannot create RunLoop timer"));
			goto fail;
		}
		CFRunLoopAddTimer(CFRunLoopGetCurrent(), serv->u.ipsec.timerref, kCFRunLoopCommonModes);
	} else {
		CFRunLoopTimerSetNextFireDate(serv->u.ipsec.timerref, CFAbsoluteTimeGetCurrent() + TIMEOUT_ASSERT_IDLE);
	}
	ipsec_log(LOG_INFO, CFSTR("IPSec Controller: wait for %d secs before forcing SAs to rekey"), TIMEOUT_ASSERT_IDLE);
	return 0;

fail:
	if (serv->u.ipsec.laststatus == IPSEC_NO_ERROR)
		serv->u.ipsec.laststatus = IPSEC_GENERIC_ERROR;
	ipsec_log(LOG_ERR, CFSTR("IPSec Controller: ASSERT failed"));
	ipsec_stop(serv, 0);
	return -1;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */

static 
int racoon_restart(struct service *serv, struct sockaddr_in *address)
{
	char			*errorstr;
	int				error, need_kick = 0, found;
	CFStringRef		string;
    CFRunLoopTimerContext	context = { 0, serv, NULL, NULL, NULL };
	CFDictionaryRef userdict = 0;
	Boolean			using_temp_xauth_name = FALSE;
	CFTypeRef		value;

	ipsec_log(LOG_INFO, CFSTR("IPSec Controller: racoon_restart..."));

	/* unconfigure ipsec first */
	if (serv->u.ipsec.policies_installed) {
		error = IPSecRemovePolicies(serv->u.ipsec.config, -1, &errorstr);
		if (error)
			ipsec_log(LOG_ERR, CFSTR("IPSec Controller: Cannot remove policies, error '%s'"), errorstr);
		
		IPSecRemoveSecurityAssociations((struct sockaddr *)&serv->u.ipsec.our_address, (struct sockaddr *)&serv->u.ipsec.peer_address);
		serv->u.ipsec.policies_installed = 0;
	}
	if (serv->ne_sm_bridge != NULL) {
		ne_sm_bridge_request_uninstall(serv->ne_sm_bridge);
	}
	uninstall_mode_config(serv, TRUE);

	if (serv->u.ipsec.config_applied) {
		// just remove the file, as will kill kick racoon with the config of the load balancing adress
		// this is to avoid useless sighup of racoon
		error = IPSecRemoveConfigurationFile(serv->u.ipsec.config, &errorstr);
		if (error)
			ipsec_log(LOG_ERR, CFSTR("IPSec Controller: Cannot remove configuration, error '%s'"), errorstr);
		serv->u.ipsec.config_applied = 0;
		need_kick = 1;
	}

	serv->u.ipsec.ping_count = 0;

	CLEAR_VPN_PORTMAPPING(serv);

	/* then try new address */
	bcopy(address, &serv->u.ipsec.peer_address, sizeof(serv->u.ipsec.peer_address));
	
	bool useBoundInterface = FALSE;
	if (serv->connectopts) {
		if (GetStrFromDict(serv->connectopts, CFSTR(NESessionStartOptionOutgoingInterface), serv->u.ipsec.lower_interface, sizeof(serv->u.ipsec.lower_interface), "")) {
			useBoundInterface = TRUE;
		}
	}
	
	if (get_src_address((struct sockaddr *)&serv->u.ipsec.our_address, (struct sockaddr *)&serv->u.ipsec.peer_address, useBoundInterface ? serv->u.ipsec.lower_interface : NULL, NULL)) {
		serv->u.ipsec.laststatus = IPSEC_NOLOCALNETWORK_ERROR;
		ipsec_log(LOG_ERR, CFSTR("IPSec Controller: cannot get our local address..."));
		goto fail;
	}
	
	if (!useBoundInterface) {
		copyGateway(gDynamicStore, AF_INET,
			serv->u.ipsec.lower_interface, sizeof(serv->u.ipsec.lower_interface), 
			(struct sockaddr *)&serv->u.ipsec.lower_gateway, sizeof(serv->u.ipsec.lower_gateway));
	}
	
#if !TARGET_OS_EMBEDDED
    serv->u.ipsec.lower_interface_media = get_if_media(serv->u.ipsec.lower_interface);
    serv->u.ipsec.timeout_lower_interface_change = get_interface_timeout(serv->u.ipsec.lower_interface_media);
#else
	serv->u.ipsec.lower_interface_media = 0;
    serv->u.ipsec.timeout_lower_interface_change = get_interface_timeout(0);
#endif /* !iPhone */

	// check to see if interface is captive: if so, bail if the interface is not ready.
	if (check_interface_captive_and_not_ready(gDynamicStore, serv->u.ipsec.lower_interface)) {
		// TODO: perhaps we should wait for a few seconds?
		goto fail;
	}
	
	TRACK_VPN_LOCATION(serv);
	
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

		serv->u.ipsec.config = IPSecCreateCiscoDefaultConfiguration(&serv->u.ipsec.our_address, 
			&serv->u.ipsec.peer_address, cfstring_is_ip(remote_address) ? NULL : remote_address, auth_method, 
			1, 0, verify_id);
		if (!serv->u.ipsec.config) {
			ipsec_log(LOG_ERR, CFSTR("IPSec Controller: cannot create IPSec dictionary..."));
			goto fail;
		}

		/* merge the system config prefs into the default dictionary */
		CFDictionaryApplyFunction(serv->systemprefs, merge_ipsec_dict, serv->u.ipsec.config);

		if (useBoundInterface) {
			CFDictionarySetValue(serv->u.ipsec.config, kRASPropIPSecForceLocalAddress, kCFBooleanTrue);
		}
		
		// by default, want to configure idle time out when connecting OnDemand
		if (serv->flags & FLAG_ONDEMAND) {
			CFNumberRef num;
			int val;

			val = 1;
			if ((num = CFNumberCreate(NULL, kCFNumberIntType, &val))) {
				CFDictionarySetValue(serv->u.ipsec.config, kRASPropIPSecDisconnectOnIdle, num);
				CFRelease(num);
			}
			val = 120; // 2 minutes by default
			if ((num = CFNumberCreate(NULL, kCFNumberIntType, &val))) {
				CFDictionarySetValue(serv->u.ipsec.config, kRASPropIPSecDisconnectOnIdleTimer, num);
				CFRelease(num);
			}
		}

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
				CFDictionarySetValue(serv->u.ipsec.config, kRASPropIPSecRemoteIdentifier, remote_address);
				CFDictionarySetValue(serv->u.ipsec.config, kRASPropIPSecIdentifierVerification, kRASValIPSecIdentifierVerificationUseRemoteIdentifier);
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
				if (serv->connectopts && userdict != NULL) {
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
				ipsec_log(LOG_ERR, CFSTR("IPSec Controller: incorrect shared secret found."));
				goto fail;
			}
			
		}
	}
	else {
		string = CFStringCreateWithCString(0, addr2ascii(AF_INET, &serv->u.ipsec.our_address.sin_addr, sizeof(serv->u.ipsec.our_address.sin_addr), 0), kCFStringEncodingASCII);
		if (string) {
			CFDictionarySetValue(serv->u.ipsec.config, kRASPropIPSecLocalAddress, string);
			CFRelease(string);
		}
		string = CFStringCreateWithCString(0, addr2ascii(AF_INET, &serv->u.ipsec.peer_address.sin_addr, sizeof(serv->u.ipsec.peer_address.sin_addr), 0), kCFStringEncodingASCII);
		if (string) {
			CFDictionarySetValue(serv->u.ipsec.config, kRASPropIPSecRemoteAddress, string);
			CFRelease(string);
		}
		if (useBoundInterface) {
			CFDictionarySetValue(serv->u.ipsec.config, kRASPropIPSecForceLocalAddress, kCFBooleanTrue);
		}
	}
	
	ipsec_log(LOG_DEBUG, CFSTR("IPSec Controller: Complete IPsec dictionary %@"), serv->u.ipsec.config);

	/* Temporarily add in XAuthName if missing when creating the racoon conf file */
	if (!CFDictionaryContainsKey(serv->u.ipsec.config, kRASPropIPSecXAuthName)) {
		CFDictionarySetValue(serv->u.ipsec.config, kRASPropIPSecXAuthName, CFSTR(" "));
		using_temp_xauth_name = TRUE;
	}
	error = IPSecApplyConfiguration(serv->u.ipsec.config, &errorstr);
	if (using_temp_xauth_name) {
		CFDictionaryRemoveValue(serv->u.ipsec.config, kRASPropIPSecXAuthName);
	}
	if (error) {
		ipsec_log(LOG_ERR, CFSTR("IPSec Controller: Cannot apply configuration, error '%s'"), errorstr);
		serv->u.ipsec.laststatus = IPSEC_CONFIGURATION_ERROR;
		goto fail;
	}

	serv->u.ipsec.config_applied = 1;
	need_kick = 0;

	/* Install policies and trigger racoon */
	if (IPSecCountPolicies(serv->u.ipsec.config)) {
		error = IPSecInstallPolicies(serv->u.ipsec.config, -1, &errorstr);
		if (error) {
			ipsec_log(LOG_ERR, CFSTR("IPSec Controller: Cannot install policies, error '%s'"), errorstr);
			serv->u.ipsec.laststatus = IPSEC_CONFIGURATION_ERROR;
			goto fail;
		}

		serv->u.ipsec.policies_installed = 1;
	}
	
	/* open and connect to the racoon control socket */
	if (serv->u.ipsec.controlfd == -1) {
		if (racoon_create_socket(serv) < 0) {
			ipsec_log(LOG_ERR, CFSTR("IPSec Controller: cannot create racoon control socket"));
			serv->u.ipsec.laststatus = IPSEC_RACOONCONTROL_ERROR;
			goto fail;
		}
	}
	else {
		racoon_send_cmd_unbind(serv->u.ipsec.controlfd, htonl(0xFFFFFFFF));
	}

	SET_VPN_PORTMAPPING(serv);

	racoon_send_cmd_bind(serv->u.ipsec.controlfd, serv->u.ipsec.peer_address.sin_addr.s_addr, gIPSecAppVersion);
	racoon_send_cmd_connect(serv->u.ipsec.controlfd, serv->u.ipsec.peer_address.sin_addr.s_addr);
	
	if (!serv->u.ipsec.timerref) {
		serv->u.ipsec.timerref = CFRunLoopTimerCreate(NULL, CFAbsoluteTimeGetCurrent() + TIMEOUT_INITIAL_CONTACT, FAR_FUTURE, 0, 0, racoon_timer, &context);
		if (!serv->u.ipsec.timerref) {
			ipsec_log(LOG_ERR, CFSTR("IPSec Controller: cannot create RunLoop timer"));
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
	ipsec_log(LOG_ERR, CFSTR("IPSec Controller: restart failed"));
	if (need_kick)
		IPSecKickConfiguration();
	ipsec_stop(serv, 0);
    return serv->u.ipsec.laststatus;
}


#if TARGET_OS_EMBEDDED
/* -----------------------------------------------------------------------------
 ----------------------------------------------------------------------------- */
void ipsec_cellular_event(struct service *serv, int event)
{
	switch (event) {
		case CELLULAR_BRINGUP_SUCCESS_EVENT:
			racoon_resolve(serv);	
			break;
		case CELLULAR_BRINGUP_FATAL_FAILURE_EVENT:
		case CELLULAR_BRINGUP_NETWORK_FAILURE_EVENT:
			serv->u.ipsec.laststatus = IPSEC_EDGE_ACTIVATION_ERROR;	
			ipsec_stop(serv, 0);
			break;
	}
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
	
	if (serv->initialized == FALSE) {
		serv->u.ipsec.laststatus = IPSEC_CONFIGURATION_ERROR;
		display_notification(serv, NULL, serv->u.ipsec.laststatus, dialog_default_type);
		return serv->u.ipsec.laststatus;
	}

	ipsec_log(LOG_INFO, CFSTR("IPSec Controller: ipsec_start, ondemand flag = %d"), onDemand);
	
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
	
	/* check for pending cert invalid alert */
	if (serv->userNotificationRef) {
		if ( onDemand ){
			if (IPSEC_STATUS_IS_CLIENT_CERTIFICATE_INVALID(serv->u.ipsec.laststatus)){
				ipsec_log(LOG_ERR, CFSTR("IPSec Controller: ipsec_start fails cert validity, returns error %d "), serv->u.ipsec.laststatus);
				return serv->u.ipsec.laststatus;
			}
		}
		CFUserNotificationCancel(serv->userNotificationRef);
		CFRunLoopRemoveSource(CFRunLoopGetCurrent(), serv->userNotificationRLS, kCFRunLoopDefaultMode);
		my_CFRelease(&serv->userNotificationRef);
		my_CFRelease(&serv->userNotificationRLS);			
	}

	ipsec_updatephase(serv, IPSEC_INITIALIZE);
	IPSEC_UNASSERT(serv->u.ipsec);
	serv->was_running = 0;
	service_started(serv);
    serv->u.ipsec.laststatus = IPSEC_NO_ERROR;
	serv->u.ipsec.resolvedAddress      = 0;
	serv->u.ipsec.resolvedAddressError = NETDB_SUCCESS;
	serv->u.ipsec.next_address = 0;
	serv->u.ipsec.ping_count = 0;
	serv->u.ipsec.awaiting_peer_resp = 0;
	
	serv->connectopts = options;
    my_CFRetain(serv->connectopts);
	
    serv->uid = uid;
    serv->gid = gid;

    scnc_bootstrap_retain(serv, bootstrap);
    
    if (onDemand)
        serv->flags |= FLAG_ONDEMAND;
	else
		serv->flags &= ~FLAG_ONDEMAND;
	serv->flags &= ~FLAG_USECERTIFICATE;
	
	ipsec_log(LOG_DEBUG, CFSTR("IPSec Controller: IPSec System Prefs %@"), serv->systemprefs);
    
#if DEBUG /* connectopts can contain cleartext passwords */
	ipsec_log(LOG_DEBUG, CFSTR("IPSec Controller: IPSec User Options %@"), serv->connectopts);
#endif
	
	/* build the peer address */
	if (!GetStrFromDict (serv->systemprefs, kRASPropIPSecRemoteAddress, remoteaddress, sizeof(remoteaddress), "")) {
		serv->u.ipsec.laststatus = IPSEC_NOSERVERADDRESS_ERROR;	
		ipsec_log(LOG_ERR, CFSTR("IPSec Controller: cannot find RemoteAddress ..."));
		goto fail;
	}
	if (!racoon_validate_cfg_str(remoteaddress)) {
		serv->u.ipsec.laststatus = IPSEC_NOSERVERADDRESS_ERROR;
		ipsec_log(LOG_ERR, CFSTR("IPSec Controller: invalid RemoteAddress ..."));
		goto fail;
	}

	IPSECLOGASLMSG("IPSec connecting to server %s\n", remoteaddress);

	{		
		serv->u.ipsec.xauth_flags = XAUTH_FIRST_TIME;
		// force prompt if necessary
		CFStringRef encryption = CFDictionaryGetValue(serv->systemprefs, kSCPropNetIPSecXAuthPasswordEncryption);
		if (isString(encryption) 
			&& CFStringCompare(encryption, kSCValNetIPSecXAuthPasswordEncryptionPrompt, 0) == kCFCompareEqualTo) {
			serv->u.ipsec.xauth_flags |= XAUTH_MUST_PROMPT;
		}
	}
	
	if (serv->ne_sm_bridge == NULL) {
		/* open the kernel event socket */
		if (serv->u.ipsec.eventfd == -1) {
			if (event_create_socket(serv, &serv->u.ipsec.eventfd, &serv->u.ipsec.eventref, event_callback, FALSE) < 0) {
				ipsec_log(LOG_ERR, CFSTR("IPSec Controller: cannot create event socket"));
				goto fail;
			}
		}
	}
	
#if TARGET_OS_EMBEDDED
	{
		/* first, bring up Cellular */
		int need_cellular = FALSE;
		SCNetworkReachabilityRef ref = NULL;
		SCNetworkConnectionFlags	flags = 0;
		struct sockaddr_in peer_address;	/* the other side IP address */

		// we don't want a synchronous dns request to happen. 
		// checking for default route is enough
		bzero(&peer_address, sizeof(peer_address));
		peer_address.sin_len = sizeof(peer_address);
		peer_address.sin_family = AF_INET;
		ref = SCNetworkReachabilityCreateWithAddress(NULL, (struct sockaddr *)&peer_address);
		//ref = SCNetworkReachabilityCreateWithName(NULL, remoteaddress);
		if (ref) {
			
			if (SCNetworkReachabilityGetFlags(ref, &flags)) {
				if ((flags & kSCNetworkReachabilityFlagsReachable) &&
					(flags & kSCNetworkReachabilityFlagsConnectionRequired) &&
					(flags & kSCNetworkReachabilityFlagsIsWWAN)) {
					need_cellular = TRUE;
				}

				ipsec_log(LOG_INFO, CFSTR("IPSec Controller: ipsec_start reachability flags = 0x%x, need_cellular = %d"), flags, need_cellular);
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
		
		if (need_cellular) {
			if (!bringup_cellular(serv)) {
				// cannot bring cellular up
				serv->u.ipsec.laststatus = IPSEC_EDGE_ACTIVATION_ERROR;	
				goto fail;
			}

			// cellular connection in progress
			return 0;
		}
	}
#endif

	return racoon_resolve(serv);
	
fail:
	if (serv->u.ipsec.laststatus == IPSEC_NO_ERROR)
		serv->u.ipsec.laststatus = IPSEC_GENERIC_ERROR;	
	ipsec_log(LOG_ERR, CFSTR("IPSec Controller: ipsec_start failed"));
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
	publish_dictnumentry(gDynamicStore, serv->serviceID, kSCEntNetIPSec, kSCPropNetIPSecStatus, phase);
}

/* -----------------------------------------------------------------------------
 detects disconnects caused by recoverable errors and flags the connection for 
 auto reconnect (i.e. persistence) and avoid UI dialog
 ----------------------------------------------------------------------------- */
static void
ipsec_check_for_disconnect_by_recoverable_error (struct service *serv, u_int32_t *flags)
{
#if !TARGET_OS_EMBEDDED
	if (serv->was_running &&
		!serv->persist_connect &&
		(serv->flags & (FLAG_FREE | FLAG_ONTRAFFIC | FLAG_ONDEMAND | FLAG_CONNECT | FLAG_SETUP_PERSISTCONNECTION)) == FLAG_SETUP_PERSISTCONNECTION &&
		(serv->connecttime && serv->establishtime) &&
		serv->u.ipsec.laststatus && serv->u.ipsec.laststatus != IPSEC_PEERDISCONNECT_ERROR) {
		ipsec_log(LOG_ERR, CFSTR("IPSec Controller: %d disconnected with status %d. Will try reconnect shortly."),
			  serv->u.ipsec.phase, serv->u.ipsec.laststatus);
		// prevent error dialog from popping up during this disconnect
		*flags = (serv->flags | FLAG_ALERTERRORS);
		serv->flags &= ~FLAG_ALERTERRORS;
		serv->persist_connect = 1;
	}
#endif
}

static int
ipsec_persist_connection (struct service *serv, u_int32_t flags)
{
#if !TARGET_OS_EMBEDDED
	u_int32_t laststatus = 0;

	if (serv->persist_connect) {
		ipsec_updatephase(serv, IPSEC_INITIALIZE);
		IPSEC_UNASSERT(serv->u.ipsec);
		serv->flags |= flags;
		laststatus = serv->u.ipsec.laststatus;
		serv->u.ipsec.laststatus = IPSEC_NO_ERROR;
		serv->u.ipsec.next_address = 1;
		serv->u.ipsec.ping_count = 0;
		serv->u.ipsec.awaiting_peer_resp = 0;
		if (serv->u.ipsec.msg) {
			my_Deallocate(serv->u.ipsec.msg, serv->u.ipsec.msgtotallen + 1);
			serv->u.ipsec.msg = 0;
		}
		serv->u.ipsec.msglen = 0;
		serv->u.ipsec.msgtotallen = 0;
		if (serv->u.ipsec.modecfg_msg != NULL) {
			my_Deallocate(serv->u.ipsec.modecfg_msg, serv->u.ipsec.modecfg_msglen);
			serv->u.ipsec.modecfg_msg = NULL;
		}
		serv->u.ipsec.modecfg_msglen = 0;
		if (serv->u.ipsec.timerref) {
			CFRunLoopRemoveTimer(CFRunLoopGetCurrent(), serv->u.ipsec.timerref, kCFRunLoopCommonModes);
			my_CFRelease(&serv->u.ipsec.timerref);
		}
		if (serv->u.ipsec.interface_timerref) {
			CFRunLoopRemoveTimer(CFRunLoopGetCurrent(), serv->u.ipsec.interface_timerref, kCFRunLoopCommonModes);
			my_CFRelease(&serv->u.ipsec.interface_timerref);
		}
		if (serv->u.ipsec.port_mapping_timerref) {
			CFRunLoopRemoveTimer(CFRunLoopGetCurrent(), serv->u.ipsec.port_mapping_timerref, kCFRunLoopCommonModes);
			my_CFRelease(&serv->u.ipsec.port_mapping_timerref);
		}
		my_CFRelease(&serv->u.ipsec.port_mapping_timerrun);

		ipsec_log(LOG_NOTICE, CFSTR("IPSec Controller: reconnecting"));
		my_CFRelease(&serv->connection_nid);
		my_CFRelease(&serv->connection_nap);
		racoon_restart(serv, &serv->u.ipsec.peer_address);
		serv->persist_connect = 0;
		return TRUE;
	}
#endif
	return FALSE;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
int ipsec_stop(struct service *serv, int signal)
{
	char			*errorstr;
	int				error;
	u_int32_t		flags = 0;

	ipsec_log(LOG_INFO, CFSTR("IPSec Controller: ipsec_stop"));

    SESSIONTRACERSTOP(serv);
    STOP_TRACKING_VPN_LOCATION(serv);

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
			/* special-case disconnect transitions? */
            ipsec_check_for_disconnect_by_recoverable_error(serv, &flags);
            return 0;

        case IPSEC_WAITING:
        case IPSEC_RUNNING:
			/* special-case disconnect transitions? */
            ipsec_check_for_disconnect_by_recoverable_error(serv, &flags);
        default:
            ipsec_updatephase(serv, IPSEC_TERMINATE);
    }

	IPSECLOGASLMSG("IPSec disconnecting from server %s\n", inet_ntoa(serv->u.ipsec.peer_address.sin_addr));	

	if (serv->u.ipsec.dnsPort) {
		mach_port_t mp = CFMachPortGetPort(serv->u.ipsec.dnsPort);
		CFMachPortInvalidate(serv->u.ipsec.dnsPort);
		CFRelease(serv->u.ipsec.dnsPort);
		serv->u.ipsec.dnsPort = NULL;
		getaddrinfo_async_cancel(mp);
	}

	if (serv->u.ipsec.controlfd != -1) {
		racoon_send_cmd_disconnect(serv->u.ipsec.controlfd, serv->u.ipsec.peer_address.sin_addr.s_addr);
	}	

	if (ipsec_persist_connection(serv, flags) == TRUE) {
		return 0;
	}

	if (serv->u.ipsec.policies_installed) {
		error = IPSecRemovePolicies(serv->u.ipsec.config, -1, &errorstr);
		if (error)
			ipsec_log(LOG_ERR, CFSTR("IPSec Controller: Cannot remove policies, error '%s'"), errorstr);
		
		IPSecRemoveSecurityAssociations((struct sockaddr *)&serv->u.ipsec.our_address, (struct sockaddr *)&serv->u.ipsec.peer_address);
		
		serv->u.ipsec.policies_installed = 0;
	}
	if (serv->ne_sm_bridge != NULL) {
		ne_sm_bridge_request_uninstall(serv->ne_sm_bridge);
	}
	uninstall_mode_config(serv, TRUE);

	if (serv->u.ipsec.config_applied) {
		error = IPSecRemoveConfiguration(serv->u.ipsec.config, &errorstr);
		if (error)
			ipsec_log(LOG_ERR, CFSTR("IPSec Controller: Cannot remove configuration, error '%s'"), errorstr);
		serv->u.ipsec.config_applied = 0;
	}
	
    if (serv->u.ipsec.msg) {
        my_Deallocate(serv->u.ipsec.msg, serv->u.ipsec.msgtotallen + 1);
        serv->u.ipsec.msg = 0;
    }
    serv->u.ipsec.msglen = 0;
    serv->u.ipsec.msgtotallen = 0;
	
	if (serv->u.ipsec.modecfg_msg != NULL) {
		my_Deallocate(serv->u.ipsec.modecfg_msg, serv->u.ipsec.modecfg_msglen);
		serv->u.ipsec.modecfg_msg = NULL;
	}
	serv->u.ipsec.modecfg_msglen = 0;
	
    serv->uid = 0;
    serv->gid = 0;
    serv->bootstrap = 0;
	
	serv->u.ipsec.ping_count = 0;

	serv->u.ipsec.lower_interface[0] = 0;
	serv->u.ipsec.lower_interface_cellular = FALSE;

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

	scnc_bootstrap_dealloc(serv);
	scnc_ausession_dealloc(serv);
     
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

#if TARGET_OS_EMBEDDED
	if (gNattKeepAliveInterval != -1) {
		int newval = gNattKeepAliveInterval;
		sysctlbyname("net.key.natt_keepalive_interval", 0, 0, &newval, sizeof(newval));	
		gNattKeepAliveInterval = -1;
	}
#endif

	if (serv->u.ipsec.port_mapping_timerref) {
		CFRunLoopRemoveTimer(CFRunLoopGetCurrent(), serv->u.ipsec.port_mapping_timerref, kCFRunLoopCommonModes);
		my_CFRelease(&serv->u.ipsec.port_mapping_timerref);
	}
	my_CFRelease(&serv->u.ipsec.port_mapping_timerrun);

	if (IPSEC_STATUS_IS_CLIENT_CERTIFICATE_INVALID(serv->u.ipsec.laststatus)) {
#if TARGET_OS_EMBEDDED
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
    	cleanup_dynamicstore((void *)serv);
	IPSEC_UNASSERT(serv->u.ipsec);
	CLEAR_VPN_PORTMAPPING(serv);
	serv->persist_connect = 0;
	serv->was_running = 0;
	service_ended(serv);

	if (serv->ne_sm_bridge != NULL) {
		allow_dispose(serv);
	}
    
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
		ipsec_log(LOG_INFO, CFSTR("IPSec Controller: ipsec_getstatus = %s"), 
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
int ipsec_copyextendedstatus(struct service *serv, CFDictionaryRef *statusdict)
{
    CFMutableDictionaryRef dict = NULL;
	CFMutableDictionaryRef ipsecdict = NULL;
    char *addrstr;
	int error = 0;

	*statusdict = NULL;
	
    if ((dict = CFDictionaryCreateMutable(NULL, 0, &kCFCopyStringDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks)) == 0) {
		error = ENOMEM;
        goto fail;
	}

    /* create and add IPSec dictionary */
    if ((ipsecdict = CFDictionaryCreateMutable(NULL, 0, &kCFCopyStringDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks)) == 0) {
		error = ENOMEM;
        goto fail;
	}
    
    AddNumber(ipsecdict, kSCPropNetIPSecStatus, serv->u.ipsec.phase);
    
    if (serv->u.ipsec.phase != IPSEC_IDLE	
		&& (addrstr = inet_ntoa(serv->u.ipsec.peer_address.sin_addr))) {
		AddString(ipsecdict, kRASPropIPSecRemoteAddress, addrstr);
	}
	
    switch (serv->u.ipsec.phase) {
        case IPSEC_RUNNING:
			if (serv->ne_sm_bridge != NULL) {
				AddNumber64(ipsecdict, kSCPropNetIPSecConnectTime, ne_sm_bridge_get_connect_time(serv->ne_sm_bridge));
			} else {
				AddNumber(ipsecdict, kSCPropNetIPSecConnectTime, serv->connecttime);
			}
            break;
            
        default:
			AddNumber(ipsecdict, CFSTR("LastCause"), serv->u.ipsec.laststatus);
    }
    CFDictionaryAddValue(dict, kSCEntNetIPSec, ipsecdict);
	
    /* create and add IPv4 dictionary */
    if (serv->u.ipsec.phase == IPSEC_RUNNING) {
        CFMutableDictionaryRef ipv4dict = (CFMutableDictionaryRef)copyEntity(gDynamicStore, kSCDynamicStoreDomainState, serv->serviceID, kSCEntNetIPv4);
        if (ipv4dict) {
            CFDictionaryAddValue(dict, kSCEntNetIPv4, ipv4dict);
            CFRelease(ipv4dict);
        }
    }

    AddNumber(dict, kSCNetworkConnectionStatus, ipsec_getstatus(serv));
    
    ipsec_log(LOG_DEBUG, CFSTR("IPSec Controller: Copy Extended Status %@"), dict);

	*statusdict = CFRetain(dict);
	
fail:
    my_CFRelease(&ipsecdict);
    my_CFRelease(&dict);
    return error;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
int ipsec_getconnectdata(struct service *serv, CFDictionaryRef *options, int all)
{
    CFDictionaryRef		opts;
    CFMutableDictionaryRef	mdict = NULL;
    CFDictionaryRef	dict;
	int err = 0;

	*options = NULL;
        
    /* return saved data */
    opts = serv->connectopts;

    if (opts == 0) {
        // no data
        return 0;
    }
    
	if (!all) {
		/* special code to remove secret information */

		mdict = CFDictionaryCreateMutableCopy(0, 0, opts);
		if (mdict == 0) {
			// no data
			return 0;
		}
		
		dict = CFDictionaryGetValue(mdict, kSCEntNetIPSec);
		if (dict && (CFGetTypeID(dict) == CFDictionaryGetTypeID())) {
			CFMutableDictionaryRef mdict1 = CFDictionaryCreateMutableCopy(0, 0, dict);
			if (mdict1) {
				CFDictionaryRemoveValue(mdict1, kSCPropNetIPSecSharedSecret);
				CFDictionarySetValue(mdict, kSCEntNetIPSec, mdict1);
				CFRelease(mdict1);
			}
		}
		*options = CFRetain(mdict);
	} else {
		*options = CFRetain(opts);
	}

    my_CFRelease(&mdict);
    return err;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
int ipsec_copystatistics(struct service *serv, CFDictionaryRef *statsdict)
{
    CFMutableDictionaryRef dict = NULL;
	CFMutableDictionaryRef ipsecdict = NULL;
	int error = 0;

	*statsdict = NULL;
	
	if (serv->u.ipsec.phase != IPSEC_RUNNING)
			return EINVAL;
			
    if ((dict = CFDictionaryCreateMutable(NULL, 0, &kCFCopyStringDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks)) == 0) {
		error = ENOMEM;
		goto fail;
	}

    /* create and add IPSec dictionary */
    if ((ipsecdict = CFDictionaryCreateMutable(NULL, 0, &kCFCopyStringDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks)) == 0) {
		error = ENOMEM;
		goto fail;
	}
    
	AddNumber(ipsecdict, kSCNetworkConnectionBytesIn, 0);
	AddNumber(ipsecdict, kSCNetworkConnectionBytesOut, 0);
	AddNumber(ipsecdict, kSCNetworkConnectionPacketsIn, 0);
	AddNumber(ipsecdict, kSCNetworkConnectionPacketsOut, 0);
	AddNumber(ipsecdict, kSCNetworkConnectionErrorsIn, 0);
	AddNumber(ipsecdict, kSCNetworkConnectionErrorsOut, 0);

    CFDictionaryAddValue(dict, kSCEntNetIPSec, ipsecdict);

	*statsdict = CFRetain(dict);

fail:
    my_CFRelease(&ipsecdict);
    my_CFRelease(&dict);
    return error;
}

void ipsec_device_lock(struct service *serv)
{
}

void ipsec_device_unlock(struct service *serv)
{
	serv->u.ipsec.has_displayed_reenroll_alert = FALSE;
}

/* -----------------------------------------------------------------------------
user has looged out
need to check the disconnect on logout flag for the ipsec interfaces
----------------------------------------------------------------------------- */
void ipsec_log_out(struct service *serv)
{
	if (serv->u.ipsec.phase != IPSEC_IDLE
		&& (serv->flags & FLAG_SETUP_DISCONNECTONLOGOUT))
		scnc_stop(serv, 0, SIGTERM, SCNC_STOP_USER_LOGOUT);
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
				scnc_stop(serv, 0, SIGTERM, SCNC_STOP_USER_SWITCH);
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
	if (serv->u.ipsec.phase != IPSEC_IDLE && (serv->flags & FLAG_SETUP_DISCONNECTONSLEEP) && !checking) { 
		scnc_stop(serv, 0, SIGTERM, SCNC_STOP_SYS_SLEEP);
	}
        
	return 0;
}

/* -----------------------------------------------------------------------------
system is waking up
----------------------------------------------------------------------------- */
void ipsec_wake_up(struct service	*serv)
{
	if (serv->u.ipsec.phase == IPSEC_RUNNING ||
		serv->u.ipsec.phase == IPSEC_WAITING) {
		if (DISCONNECT_VPN_IFOVERSLEPT(__FUNCTION__, serv, (char *)serv->if_name)) {
			return;
		} else if (DISCONNECT_VPN_IFLOCATIONCHANGED(serv)) {
			return;
		}
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
			case IPSEC_NETWORKCHANGE_ERROR: /* IPSec Error 15 */
				return;
#if TARGET_OS_EMBEDDED
			/* Errors 16, 17, 19 are not displayed on embedded os */
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

	/* Are we trying to display the re-enrolling alert */
	if ((dialog_type == dialog_cert_fixme_type) && (serv->flags & FLAG_ONDEMAND)){
		/* check the last time we displayed this */
		if (serv->u.ipsec.has_displayed_reenroll_alert){
			goto done;
		}
	}
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
			serv->u.ipsec.has_displayed_reenroll_alert = TRUE;
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

