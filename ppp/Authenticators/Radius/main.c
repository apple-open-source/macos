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
 *
 *  Theory of operation :
 *
 *  plugin to add support for authentication against a radius server.
 *
----------------------------------------------------------------------------- */


/* -----------------------------------------------------------------------------
  Includes
----------------------------------------------------------------------------- */

#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <CoreFoundation/CFBundle.h>
#include <CoreFoundation/CoreFoundation.h>
#include <SystemConfiguration/SCSchemaDefinitions.h>
#include <Security/SecKeychain.h>
#include <Security/SecKeychainSearch.h>
#include <Security/SecKeychainItem.h>
#include <CommonCrypto/CommonDigest.h>

#define APPLE 1

#include "../../Helpers/pppd/pppd.h"
#include "../../Helpers/pppd/fsm.h"
#include "../../Helpers/pppd/lcp.h"
#include "../../Helpers/pppd/chap-new.h"
#include "../../Helpers/pppd/chap_ms.h"
#include "../../Family/ppp_comp.h"
#include "../../Helpers/vpnd/RASSchemaDefinitions.h"

#include "radius.h"

#include "radlib.h"
#include "radlib_vs.h"

/* -----------------------------------------------------------------------------
 Definitions
----------------------------------------------------------------------------- */




/* -----------------------------------------------------------------------------
 Forward declarations
----------------------------------------------------------------------------- */

static int radius_papchap_check();
static int radius_pap_auth(char *user, char *passwd, char **msgp,
		struct wordlist **paddrs, struct wordlist **popts);
static int radius_chap_auth(char *name, char *ourname, int id,
			struct chap_digest_type *digest,
			unsigned char *challenge, unsigned char *response,
			unsigned char *message, int message_space);
static int radius_chap_auth_unknown(char *name, char *ourname, int code, int id,
			struct chap_digest_type *digest,
			unsigned char *challenge, unsigned char *pkt, int pkt_len,
			unsigned char *message, int message_space);
static int radius_ip_allowed_address(u_int32_t addr);
static void radius_ip_choose(u_int32_t *addr);
static void radius_ip_up(void *arg, uintptr_t p);
static void radius_ip_down(void *arg, uintptr_t p);
static void radius_system_inited(void *param, uintptr_t code);
static int read_keychainsecret(char *service, char *account, char **password);

/* ------------------------------------------------------------------------------------
 pppd variables
------------------------------------------------------------------------------------ */ 

extern CFDictionaryRef	systemOptions;	/* system options dictionary */

/* -----------------------------------------------------------------------------
 Globals
----------------------------------------------------------------------------- */

extern u_char mppe_send_key[MPPE_MAX_KEY_LEN];
extern u_char mppe_recv_key[MPPE_MAX_KEY_LEN];
extern int mppe_keys_set;		/* Have the MPPE keys been set? */

static CFBundleRef 	bundle = 0;		/* our bundle ref */
static CFDictionaryRef radiusDict = NULL;	/* options dictionary */

/* option variables */

// all the settings are adjustable for each authentication server
#define DEFAULT_TIMEOUT		10		// 10 seconds timeout
#define DEFAULT_RETRIES		4		// try 4 times each server

// primary authentication server
int 	pri_auth_port = 0;			// by default, let the radius library decide
int 	pri_auth_timeout = DEFAULT_TIMEOUT;		// default timeout
int 	pri_auth_retries = DEFAULT_RETRIES;		// default retries
char	*pri_auth_server = NULL;	// by default, let the library use the system files
char	*pri_auth_secret = NULL;	// by default, let the library use the system files

// secondary authentication server
int 	sec_auth_port = 0;			// if null, use primary port
int 	sec_auth_timeout = 0;		// if null, use primary timeout
int 	sec_auth_retries = 0;		// if null, use primary retries
char	*sec_auth_server = NULL;	// if null, use primary server
char	*sec_auth_secret = NULL;	// if null, use primary secret

char	*nas_identifier = NULL;		// NAS Identifier to include in Radius packets
char	*nas_ip_address = NULL;		// NAS IP address to include in Radius packets
int		nas_port_type = RAD_VIRTUAL; // default is virtual
int		tunnel_type = 0;			// not specified

static bool force_mschapv2_retry = 0;		// Force the mschap v2 retry flag
static bool use_eap_proxy = 1;				// turn on radius eap proxy
static bool use_pap = 1;					// turn on radius pap
static bool use_mschap = 1;					// turn on radius mschap

struct auth_server **auth_servers = NULL;	// array of authentication servers
int nb_auth_servers = 0;	// number of authentication servers

// reponses from the radius server
//static struct in_addr user_ipaddr;		/* ip address for the connecting user */

int (*old_pap_auth_hook) __P((char *user, char *passwd, char **msgp,
				 struct wordlist **paddrs,
				 struct wordlist **popts));

int (*old_chap_verify_hook) __P((char *name, char *ourname, int id,
			struct chap_digest_type *digest,
			unsigned char *challenge, unsigned char *response,
			unsigned char *message, int message_space));

int (*old_chap_unknown_hook) __P((char *name, char *ourname, int code, int id,
			struct chap_digest_type *digest,
			unsigned char *challenge, unsigned char *pkt, int pkt_len,
			unsigned char *message, int message_space));

/* option descriptors */
option_t radius_options[] = {

    // primary authentication server
    { "radius_primary_auth_server", o_string, &pri_auth_server,
      "Primary radius server" },
    { "radius_primary_auth_secret", o_string, &pri_auth_secret,
      "Primary radius server secret", OPT_HIDE },
    { "radius_primary_auth_port", o_int, &pri_auth_port,
      "Port for server" },
    { "radius_primary_auth_timeout", o_int, &pri_auth_timeout,
      "Timeout to contact server" },
    { "radius_primary_auth_retries", o_int, &pri_auth_retries,
      "Retries to contact server" },

    // secondary authentication server
    { "radius_secondary_auth_server", o_string, &sec_auth_server,
      "Primary radius server" },
    { "radius_secondary_auth_secret", o_string, &sec_auth_secret,
      "Primary radius server secret", OPT_HIDE },
    { "radius_secondary_auth_port", o_int, &sec_auth_port,
      "Port for server" },
    { "radius_secondary_auth_timeout", o_int, &sec_auth_timeout,
      "Timeout to contact server" },
    { "radius_secondary_auth_retries", o_int, &sec_auth_retries,
      "Retries to contact server" },

    // shorter parameters...
    { "radius_primary", o_string, &pri_auth_server,
      "Primary radius server" },
    { "radius_secondary", o_string, &sec_auth_server,
      "Secondary radius server" },
    { "radius_secret", o_string, &pri_auth_secret,
      "Radius server", OPT_HIDE},
    { "radius_port", o_int, &pri_auth_port,
      "Port for server" },
    { "radius_timeout", o_int, &pri_auth_timeout,
      "Timeout to contact server" },
    { "radius_retries", o_int, &pri_auth_retries,
      "Retries to contact server" },
	  
    { "radius_nas_ip_address", o_string, &nas_ip_address,
      "NAS IP address to include in Radius packets" },
    { "radius_nas_identifier", o_string, &nas_identifier,
      "NAS Identifier to include in Radius packets" },
    { "radius_nas_port_type", o_int, &nas_port_type,
      "NAS Port Type in Radius packets" },
    { "radius_tunnel_type", o_int, &tunnel_type,
      "Tunnel Type in Radius packets" },

    { "radius_force_mschapv2_retry", o_bool, &force_mschapv2_retry,
      "Force the mschap v2 retry flag", 1 },
    { "radius_eap_proxy", o_bool, &use_eap_proxy,
      "Turn on eap proxy", 1 },
    { "radius_pap", o_bool, &use_pap,
      "Turn on pap", 1 },
    { "radius_mschap", o_bool, &use_mschap,
      "Turn on mschap", 1 },
    { "radius_no_eap_proxy", o_bool, &use_eap_proxy,
      "Turn off eap proxy", 0 },
    { "radius_no_pap", o_bool, &use_pap,
      "Turn off pap", 0 },
    { "radius_no_mschap", o_bool, &use_mschap,
      "Turn off mschap", 0 },

    { NULL }
};
    
/* -----------------------------------------------------------------------------
plugin entry point, called by pppd
----------------------------------------------------------------------------- */
int start(CFBundleRef ref)
{
   
    bundle = ref;
    CFRetain(bundle);

	// hookup our handlers    
	pap_check_hook = radius_papchap_check;
	chap_check_hook = radius_papchap_check;

    // add the radius specific options
    add_options(radius_options);

	add_notifier(&system_inited_notify, radius_system_inited, 0);
	
    return 0;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
void makestr(CFDictionaryRef dict, const void *key, char **value, char *defaultval, char *errorstr) {

	CFStringRef	strRef;
	int len;
	char *str = 0;
	 
	strRef = CFDictionaryGetValue(dict, key);
	if (strRef) {
		len = CFStringGetLength(strRef) + 1;
		str = malloc(CFStringGetMaximumSizeForEncoding(len, kCFStringEncodingUTF8));
		if (!str)
			novm(errorstr);

		str[0] = 0;
		CFStringGetCString(strRef, str, len, kCFStringEncodingUTF8);
		*value = str;		
	}
	else if (defaultval) {
		len = strlen(defaultval) + 1;
		*value = strdup(defaultval);
		if (*value == NULL)
			novm(errorstr);
	}

}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
void makeint(CFDictionaryRef dict, const void *key, int *value, int defaultval) {

	CFNumberRef	numRef;
	 
	numRef = CFDictionaryGetValue(dict, key);
	if (numRef)
		CFNumberGetValue(numRef, kCFNumberIntType, value);
	else 
		*value = defaultval;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
static 
void radius_system_inited(void *param, uintptr_t code)
{
	CFStringRef	strRef;
	CFArrayRef	serversArray, authArray;
	CFDictionaryRef	serverDict;
	int 	i, installPAP,	installMSCHAP2, installEAP;

	radiusDict = NULL;

	if (systemOptions) {
		radiusDict = CFDictionaryGetValue(systemOptions, kRASEntRadius);
		if (radiusDict)
			CFRetain(radiusDict);
	}

	// if Radius dictionnary is present, use it
	if (radiusDict) {

		// NAS identifier & Identifier
		makestr(radiusDict, kRASPropRadiusNASIdentifier, &nas_identifier, 0, "Radius: Cannot allocate memory for nas identifier.\n");
		makestr(radiusDict, kRASPropRadiusNASIPAddress, &nas_ip_address, 0, "Radius: Cannot allocate memory for nas ip address.\n");

		// NAS Port Type & Tunnel Type
		makeint(radiusDict, kRASPropRadiusNASPortType, &nas_port_type, 0);
		makeint(radiusDict, kRASPropRadiusTunnelType, &tunnel_type, 0);

		// Radius servers
		nb_auth_servers = 0;
		serversArray = CFDictionaryGetValue(radiusDict, kRASPropRadiusServers);
		if (serversArray && CFGetTypeID(serversArray) == CFArrayGetTypeID())
			nb_auth_servers = CFArrayGetCount(serversArray);

		if (nb_auth_servers == 0) {
			error("Radius : No server specified\n");
			return;
		}

		auth_servers = malloc(nb_auth_servers * sizeof(struct auth_server *));
		if (!auth_servers)
			novm("Radus: Cannot allocate memory for radius servers list.\n");
		bzero(auth_servers, nb_auth_servers * sizeof(struct auth_server *));
		
		for (i = 0; i < nb_auth_servers; i++) {
			struct auth_server *server = NULL;

			auth_servers[i] = malloc(sizeof(struct auth_server));
			server = auth_servers[i];
			if (!server)
				novm("Radus: Cannot allocate memory for radius server structure.\n");
			bzero(server, sizeof(struct auth_server));

			serverDict = CFArrayGetValueAtIndex(serversArray, i);
			if (serverDict && CFGetTypeID(serverDict) == CFDictionaryGetTypeID()) {
			
				/* server parameters */
				
				// Server address and Shared Secret
				makestr(serverDict, kRASPropRadiusServerAddress, &server->address, "", "Radius: Cannot allocate memory for server address.\n");
				makestr(serverDict, kRASPropRadiusServerSharedSecret, &server->secret, "", "Radius: Cannot allocate memory for server shared secret.\n");
				strRef = CFDictionaryGetValue(serverDict, kRASPropRadiusServerSharedSecretEncryption);
				if (strRef && 
					(CFGetTypeID(strRef) == CFStringGetTypeID()) &&
					(CFStringCompare(strRef, kRASValRadiusSharedSecretEncryptionKeychain, 0) == kCFCompareEqualTo)) {
					// get actual secret from keychain
					char *secret = server->secret;
					server->secret = 0;
					read_keychainsecret("com.apple.ppp.radius", secret, &(server->secret));
					if (!server->secret) {
						server->secret = malloc(1);
						server->secret[0] = 0;
					}
					free(secret);
				}

				// Server Port, Timeout, Retries
				makeint(serverDict, kRASPropRadiusServerPort, &server->port, 0);
				makeint(serverDict, kRASPropRadiusServerTimeout, &server->timeout, DEFAULT_TIMEOUT);
				makeint(serverDict, kRASPropRadiusServerRetries, &server->retries, DEFAULT_RETRIES);

				// Server authentication methods
				authArray = CFDictionaryGetValue(serverDict, kRASPropRadiusServerAuthProtocol);
				if (!authArray) {
					// Authentication methods not specified. Assume default support for EAP + PAP + MSCHAP2.
					installPAP = installEAP = installMSCHAP2 = 1;
					server->proto = RADIUS_USE_PAP + RADIUS_USE_MSCHAP2 + RADIUS_USE_EAP;
				}
				else if (CFGetTypeID(authArray) == CFArrayGetTypeID()) {
					CFRange range;
					range.location = 0;
					range.length = CFArrayGetCount(authArray);
					
					if (CFArrayContainsValue(authArray, range, kRASValPPPAuthProtocolEAP)) {
						installEAP = 1;
						server->proto |= RADIUS_USE_EAP;
					}
					if (CFArrayContainsValue(authArray, range, kRASValPPPAuthProtocolPAP)) {
						installPAP = 1;
						server->proto |= RADIUS_USE_PAP;
					}
					if (CFArrayContainsValue(authArray, range, kRASValPPPAuthProtocolMSCHAP2)) {
						installMSCHAP2 = 1;
						server->proto |= RADIUS_USE_MSCHAP2;
					}
				}
			}
			
		}

	}
	
	// if no Radius dictionnary is present, check flat file parameters
	if (!radiusDict) {

		if (pri_auth_server == 0) {
			error("Radius : No primary server specified\n");
			return;
		}

		nb_auth_servers = 1;

		// do some option checking in case not all the options are supplied
		if (sec_auth_server) {
			nb_auth_servers++;
			// transfer primary options to secondary server options, if they are not set
			if (sec_auth_secret == NULL)
				sec_auth_secret = pri_auth_secret;
			if (sec_auth_port == 0)
				sec_auth_port = pri_auth_port;
			if (sec_auth_timeout == 0)
				sec_auth_timeout = pri_auth_timeout;
			if (sec_auth_retries == 0)
				sec_auth_retries = pri_auth_retries;
		}
		
		auth_servers = malloc(nb_auth_servers * sizeof(struct auth_server *));
		bzero(auth_servers, nb_auth_servers * sizeof(struct auth_server *));
		for (i = 0; i < nb_auth_servers; i++) {
			auth_servers[i] = malloc(sizeof(struct auth_server));
			bzero(auth_servers[i], sizeof(struct auth_server));
		}

		// build servers array
		auth_servers[0]->address = strdup(pri_auth_server);
		if (!auth_servers[0]->address)
			novm("Radus: Cannot allocate memory for server address.\n");
		if (pri_auth_secret) {
			auth_servers[0]->secret = strdup(pri_auth_secret);
			if (!auth_servers[0]->secret)
				novm("Radus: Cannot allocate memory for server secret.\n");
		}
		auth_servers[0]->port = pri_auth_port;
		auth_servers[0]->timeout = pri_auth_timeout;
		auth_servers[0]->retries = pri_auth_retries;
		auth_servers[0]->proto = (use_pap ? RADIUS_USE_PAP : 0) +
				(use_mschap ? RADIUS_USE_MSCHAP2 : 0) +
				(use_eap_proxy ? RADIUS_USE_EAP : 0);
		installPAP = use_pap;
		installMSCHAP2 = use_mschap;
		installEAP = use_eap_proxy;
		
		if (sec_auth_server) {
			auth_servers[1]->address = strdup(sec_auth_server);
			if (!auth_servers[1]->address)
				novm("Radus: Cannot allocate memory for server address.\n");
			if (sec_auth_secret) {
				auth_servers[1]->secret = strdup(sec_auth_secret);
				if (!auth_servers[1]->secret)
					novm("Radus: Cannot allocate memory for server secret.\n");
			}
			auth_servers[1]->port = sec_auth_port;
			auth_servers[1]->timeout = sec_auth_timeout;
			auth_servers[1]->retries = sec_auth_retries;
			auth_servers[1]->proto = (use_pap ? RADIUS_USE_PAP : 0) +
					(use_mschap ? RADIUS_USE_MSCHAP2 : 0) +
					(use_eap_proxy ? RADIUS_USE_EAP : 0);
		}
	}


	// hookup our handlers    
	if (installPAP || installMSCHAP2) {
	
		if (installPAP) {
			old_pap_auth_hook = pap_auth_hook;
			pap_auth_hook = radius_pap_auth;
		}	
			
		if (installMSCHAP2) {
			old_chap_verify_hook = chap_verify_hook;
			chap_verify_hook = radius_chap_auth;
			old_chap_unknown_hook = chap_unknown_hook;
			chap_unknown_hook = radius_chap_auth_unknown;
		}

		ip_choose_hook = radius_ip_choose;
		allowed_address_hook = radius_ip_allowed_address;

	}

	if (installEAP) {
		if (radius_eap_install() < 0) {
			error("Radius: Can't install EAP-Radius handler");
		}
	}
	
	// TO DO: accounting
	add_notifier(&ip_up_notify, radius_ip_up, 0);
	add_notifier(&ip_down_notify, radius_ip_down, 0);

}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
static 
int radius_papchap_check()
{
    // radius will provide authentication
    // may be we should check for valid radius settings at this point ?
    return 1;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
#if 0
static void
ascii2hex(u_char *text, int len, u_char *hex) 
{
	int i;
	for (i = 0; i < len; i++) {
		if (text[0] >= '0' && text[0] <= '9')
			hex[i] = text[0] - '0';
		else if (text[0] >= 'a' && text[0] <= 'f')
			hex[i] = text[0] - 'a' + 10;
		else if (text[0] >= 'A' && text[0] <= 'F')
			hex[i] = text[0] - 'A' + 10;

		hex[i] <<= 4;

		if (text[1] >= '0' && text[1] <= '9')
			hex[i] |= text[1] - '0';
		else if (text[1] >= 'a' && text[1] <= 'f')
			hex[i] |= text[1] - 'a' + 10;
		else if (text[1] >= 'A' && text[1] <= 'F')
			hex[i] |= text[1] - 'A' + 10;

		text += 2;
	}
}
#endif

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
static void
hex2ascii(u_char *hex, int len, u_char *text) 
{
	int i, v;
	for (i = 0; i < len; i++) {
		v = hex[i] >> 4;
		*text++ = (v >= 0 && v <= 9) ? v + '0' : v - 10 + 'A';

		v = hex[i] & 0xF;
		*text++ = (v >= 0 && v <= 9) ? v + '0' : v - 10 + 'A';
	}
}

/* -----------------------------------------------------------------------------
type :  
    0 = PAP, clear text
    5 = CHAP_DIGEST_MD5
    0x80 = CHAP_MICROSOFT
    0x81 = CHAP_MICROSOFT_V2
----------------------------------------------------------------------------- */
static 
int radius_authenticate_user(char *user, char *passwd, int type, 
    char *challenge, int chal_len, int chal_id, 
    void *remotemd, int remotemd_len, unsigned char *message, int message_space, int changepassword)
{
	struct rad_handle *h = 0;
    int err, ret = 0, attr_type, i;
	void *attr_value;
	size_t attr_len, len;
	u_int32_t attr_vendor;
    char buf[256]; //MS_CHAP_RESPONSE_LEN + 1];
    char auth[MD4_SIGNATURE_SIZE + 1];
	
    h = rad_auth_open();
    if (h == NULL) 
        novm("Radius : can't open context.\n");	// will die...
	
	for (i = 0; i < nb_auth_servers; i++) {
		struct auth_server *server = auth_servers[i]; 
		
		if (server->proto & (RADIUS_USE_PAP | RADIUS_USE_MSCHAP2)) {
			err = rad_add_server(h, server->address, server->port, server->secret, server->timeout, server->retries);
			if (err != 0) {
				error("Radius : Can't use server '%s'\n", server->address);
				if (i == 0)
					goto done;
			}
		}
	}
	
    rad_create_request(h, RAD_ACCESS_REQUEST);
    rad_put_string(h, RAD_USER_NAME, user);

    rad_put_int(h, RAD_SERVICE_TYPE, RAD_FRAMED);
    rad_put_int(h, RAD_FRAMED_PROTOCOL, RAD_PPP);
    rad_put_int(h, RAD_NAS_PORT_TYPE, nas_port_type);
    if (tunnel_type)
		rad_put_int(h, RAD_TUNNEL_TYPE, tunnel_type);

	
	/* Add NAS_IP_ADDRESS and NAS_IDENTIFIER */
	if (nas_ip_address) {
		struct in_addr addr;
		addr.s_addr = 0;
		ascii2addr(AF_INET, nas_ip_address, &addr);
		rad_put_int(h, RAD_NAS_IP_ADDRESS, ntohl(addr.s_addr));
	}
	
    if (nas_identifier)
		rad_put_string(h, RAD_NAS_IDENTIFIER, nas_identifier);

	/* if there is no nas_ip_address or no nas_identifier, use a hostname() as nas_identifier */
	if (nas_identifier == NULL && nas_ip_address == NULL) {
		char hostname[256];
		if (gethostname(hostname, sizeof(hostname)) < 0 )
			strlcpy(hostname, "Apple", sizeof(hostname));
		hostname[255] = 0;
		rad_put_string(h, RAD_NAS_IDENTIFIER, hostname);
	}
	
    switch (type) {
        
        case 0: /* PAP, clear text */
            rad_put_string(h, RAD_USER_PASSWORD, passwd);
            break;
            
        case CHAP_MD5:
        
#define MD5_HASH_SIZE		16

            rad_put_attr(h, RAD_CHAP_CHALLENGE, challenge, chal_len);
            buf[0] = chal_id;
            memcpy(&buf[1], remotemd, MD5_HASH_SIZE);
            rad_put_attr(h, RAD_CHAP_PASSWORD, buf, MD5_HASH_SIZE + 1);
            break;
    			
        case CHAP_MICROSOFT:	   
            break;

        case CHAP_MICROSOFT_V2:
			if (changepassword) {
				rad_put_vendor_attr(h, RAD_VENDOR_MICROSOFT, RAD_MICROSOFT_MS_CHAP_CHALLENGE,
							challenge, chal_len);
				buf[0] = 7; buf[1] = 1; // code = 7, id = 1;
				memcpy(&buf[2], remotemd + 516, 66);
				rad_put_vendor_attr(h, RAD_VENDOR_MICROSOFT, RAD_MICROSOFT_MS_CHAP2_PW, buf, 68);
				buf[0] = 6; buf[1] = 1; buf[2] = 0; buf[3] = 1; // code = 6, id = 1, seq = 1;
				memcpy(&buf[4], remotemd, 243);
				rad_put_vendor_attr(h, RAD_VENDOR_MICROSOFT, RAD_MICROSOFT_MS_CHAP_NT_ENC_PW, buf, 247);
				buf[0] = 6; buf[1] = 1; buf[2] = 0; buf[3] = 2; // code = 6, id = 1, seq = 2
				memcpy(&buf[4], remotemd+243, 243);
				rad_put_vendor_attr(h, RAD_VENDOR_MICROSOFT, RAD_MICROSOFT_MS_CHAP_NT_ENC_PW, buf, 247);
				buf[0] = 6; buf[1] = 1; buf[2] = 0; buf[3] = 3; // code = 6, id = 1, seq = 3
				memcpy(&buf[4], remotemd+486, 30);
				rad_put_vendor_attr(h, RAD_VENDOR_MICROSOFT, RAD_MICROSOFT_MS_CHAP_NT_ENC_PW, buf, 34);
			}
			else {
				rad_put_vendor_attr(h, RAD_VENDOR_MICROSOFT, RAD_MICROSOFT_MS_CHAP_CHALLENGE,
							challenge, chal_len);
				/* 
					MSCHAP and MSCHAPv2 use the same structure length
					fields UseNT and Flags are at the same place
					MSCHAP : first put id and UseNT, then LANManResp and NTResp
					MSCHAPv2 : put id and Flags, then PeerChallenge, Reserved and NTResp
				*/
				buf[0] = chal_id;
				buf[1] = ((MS_ChapResponse *) remotemd)->UseNT[0];
				memcpy(&buf[2], remotemd, MS_CHAP_RESPONSE_LEN - 1);
				rad_put_vendor_attr(h, RAD_VENDOR_MICROSOFT, 						
							type == CHAP_MICROSOFT ? RAD_MICROSOFT_MS_CHAP_RESPONSE : RAD_MICROSOFT_MS_CHAP2_RESPONSE,
							buf, MS_CHAP_RESPONSE_LEN + 1);
			}
            break;
    }

#if 0
    if (remoteaddress)
	rad_put_string(h, RAD_CALLING_STATION_ID, remoteaddress);
#endif
    
    err = rad_send_request(h);
    
    switch (err) {
        case RAD_ACCESS_ACCEPT: 
            /* TO DO: fetch interesting information from the response */
            ret = 1;
            switch (type) {
                
                case 0: /* PAP, clear text */
                    break;
                    
                case CHAP_MD5:
                    slprintf(message, message_space, "Access granted");
                    break;
            
                case CHAP_MICROSOFT:	   
                case CHAP_MICROSOFT_V2:                

					while ((attr_type = rad_get_attr(h, (const void **)&attr_value, &attr_len)) > 0 ) {

						switch (attr_type) {
						
							case RAD_VENDOR_SPECIFIC: 
							
								attr_type = rad_get_vendor_attr(&attr_vendor, (const void **)&attr_value,  &attr_len);
								switch (attr_type) {
									case RAD_MICROSOFT_MS_MPPE_SEND_KEY:
										len = rad_request_authenticator(h, auth, sizeof(auth));
										
										if(len != -1)
										{
											radius_decryptmppekey(mppe_send_key, attr_value, attr_len, (u_char*)rad_server_secret(h), auth, len);
											mppe_keys_set = 1;
										}
										else
											error("Radius: rad-mschapv2-mppe-send-key:  could not get authenticator!\n");										
										break;
										
									case RAD_MICROSOFT_MS_MPPE_RECV_KEY:
										len = rad_request_authenticator(h, auth, sizeof(auth));
										
										if(len != -1)
										{
											radius_decryptmppekey(mppe_recv_key, attr_value, attr_len, (u_char*)rad_server_secret(h), auth, len);
											mppe_keys_set = 1;
										}
										else
											error("Radius: rad-mschapv2-mppe-recv-key:  could not get authenticator!\n");
										break;

									case RAD_MICROSOFT_MS_CHAP2_SUCCESS:
										if ((attr_len - 1) < message_space) {
											memcpy(message, attr_value + 1, attr_len - 1);
											message[attr_len - 1] = 0;
										}
										break;
								}
								
								break;
						}
					}

                    break;
            }
            break;

        case RAD_ACCESS_REJECT: 
            switch (type) {
                
                case 0: /* PAP, clear text */
                    break;
                    
                case CHAP_MD5:
                    slprintf(message, message_space, "Access denied");
                    break;
            
                case CHAP_MICROSOFT:	   
                case CHAP_MICROSOFT_V2:

					while ((attr_type = rad_get_attr(h, (const void **)&attr_value, &attr_len)) > 0 ) {

						switch (attr_type) {
						
							case RAD_VENDOR_SPECIFIC: 
								attr_type = rad_get_vendor_attr(&attr_vendor, (const void **)&attr_value,  &attr_len);

								switch (attr_type) {

									case RAD_MICROSOFT_MS_CHAP_ERROR:
										if ((attr_len - 1) < message_space) {
											memcpy(message, attr_value + 1, attr_len - 1);
											message[attr_len - 1] = 0;
											if (strstr(message, "E=648")) {
												unsigned char *p;
												int len;
												/* stick the challenge in the packet */
												p = strstr(message, " V=");
												if (p == 0) {
													error("Radius: couldn't find Version field in the MS-CHAP-ERROR packet\n");
													ret = 0;
													break;
												}
												len = strlen(message) - (p - message);
												memcpy(p+3+2*MD4_SIGNATURE_SIZE, p, len);
												*p++ = ' ';
												*p++ = 'C';
												*p++ = '=';
												hex2ascii(challenge, MD4_SIGNATURE_SIZE, p);
												message[attr_len - 1 + 3 + 2*MD4_SIGNATURE_SIZE] = 0;
												ret = -1;
											}
											else if (strstr(message, "E=691")) {
												unsigned char *p;
												int len;
												/* look for retry flag and force it if necessary
												  We have an option to force it because Windows Radius IAS set the R=0 flag */
												p = strstr(message, " R=");
												if (p && force_mschapv2_retry) 
													p[3] = '1';
												if (p == 0 || p[3] == '0') {
													ret = 0;
													break;
												}
												/* stick the challenge in the packet */
												p = strstr(message, " V=");
												if (p == 0) {
													error("Radius: couldn't find Version field in the MS-CHAP-ERROR packet\n");
													ret = 0;
													break;
												}
												len = strlen(message) - (p - message);
												memcpy(p+3+2*MD4_SIGNATURE_SIZE, p, len);
												*p++ = ' ';
												*p++ = 'C';
												*p++ = '=';
												hex2ascii(challenge, MD4_SIGNATURE_SIZE, p);
												message[attr_len - 1 + 3 + 2*MD4_SIGNATURE_SIZE] = 0;
												ret = -1;
											}
										}
										break;
								}
								break;
						}
					}
                    break;
            }

            break;
        case RAD_ACCESS_CHALLENGE: 
            error("Radius : Received Access Challenge\n");
            break;
        default: 
            error("Radius : Authentication error %d. %s.\n", err, rad_strerror(h));
			slprintf(message, message_space, "Access denied");

    }
    
done:
    if (h)
        rad_close(h);
    
    return ret;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
static int
radius_pap_auth(char *user, char *passwd, char **msgp,
		struct wordlist **paddrs, struct wordlist **popts)
{
   
    return radius_authenticate_user(user, passwd, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
static int radius_chap_auth(char *user, char *ourname, int id,
			struct chap_digest_type *digest,
			unsigned char *challenge, unsigned char *response,
			unsigned char *message, int message_space)
{
    int challenge_len = *challenge++;
    int response_len = *response++;

    return radius_authenticate_user(user, 0, digest->code,  
        challenge, challenge_len, id, 
        response, response_len, message, message_space, 0);
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
static int radius_chap_auth_unknown(char *user, char *ourname, int code, int id,
			struct chap_digest_type *digest,
			unsigned char *challenge, unsigned char *pkt, int pkt_len,
			unsigned char *message, int message_space)
{
    int challenge_len = *challenge++;
	
	/* only handle MSChapV2 ChangePassword messages */
	if (digest->code == CHAP_MICROSOFT_V2 && code == 7) {
		/* pkt_len should be 582 */
		return radius_authenticate_user(user, 0, digest->code,  
			challenge, challenge_len, id, 
			pkt, pkt_len, message, message_space, 1);
	}
	
	/* ignore packet */
	return -2; 
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
static int
radius_ip_allowed_address(u_int32_t addr)
{

    // any address is OK
    // need to implement address option in radius packet
    return 1;    
}


/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
static void
radius_ip_choose(u_int32_t* addr)
{

    // need to implement address option in radius packet
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
static 
void radius_ip_up(void *arg, uintptr_t p)
{
    // need to implement accounting

}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
static
void radius_ip_down(void *arg, uintptr_t p)
{
    // need to implement accounting
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
int
radius_decryptmppekey(char *key, u_int8_t *attr_value, size_t attr_len, char *secret, char *authenticator, size_t auth_len)
{
    int i;
    u_char  plain[32];
    u_char  buf[16];
    MD5_CTX ctx;
			
    memcpy(plain, attr_value + 2, sizeof(plain)); /* key string */

    MD5_Init(&ctx);
    MD5_Update(&ctx, secret, strlen(secret));
	MD5_Update(&ctx, authenticator, auth_len);
    MD5_Update(&ctx, attr_value, 2); /* salt */
    MD5_Final(buf, &ctx);

    for (i = 0; i < 16; i++)
		plain[i] ^= buf[i];

    MD5_Init(&ctx);
    MD5_Update(&ctx, secret, strlen(secret));
    MD5_Update(&ctx, attr_value + 2, 16); /* key string */
    MD5_Final(buf, &ctx);

    for (i = 0; i < 16; i++)
		plain[16 + i] ^= buf[i];

	memcpy(key, plain + 1, 16);
    return 0;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
static int
read_keychainsecret(char *service, char *account, char **password)

{
	SecKeychainRef keychain = NULL;
	UInt32 password_len	= 0;
	OSStatus status;
	int ret = 0;
	
	status = SecKeychainSetPreferenceDomain(kSecPreferencesDomainSystem);
	if (status != noErr) {
		error("Radius: failed to set system keychain domain");
		goto end;
	}

	status = SecKeychainCopyDomainDefault(kSecPreferencesDomainSystem, &keychain);
	if (status != noErr) {
		error("Radius: failed to get system keychain domain");
		goto end;
	}
	
	status = SecKeychainFindGenericPassword(keychain,
					        service ? strlen(service) : 0, service,
					        account ? strlen(account) : 0, account,
					        &password_len, (void**)password,
					        NULL);
	
	switch (status) {

	    case noErr :
			ret = 1;
			break;

	    case errSecItemNotFound :
			error("Radius: Shared Secret not found in the system keychain");
			break;

	    default :
			error("Radius: failed to get password from system keychain (error %d)", status);
	}

end:

	if (keychain)
		CFRelease(keychain);

	return ret;
}

