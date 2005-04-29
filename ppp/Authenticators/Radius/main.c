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
#include <signal.h>
#include <errno.h>
#include <fcntl.h>
#include <syslog.h>
#include <netdb.h>
#include <utmp.h>
#include <pwd.h>
#include <setjmp.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <syslog.h>
#include <sys/ioctl.h>
#include <sys/un.h>
#include <sys/uio.h>                     /* struct iovec */

#include <net/if.h>
#include <CoreFoundation/CFBundle.h>
#include <ApplicationServices/ApplicationServices.h>

#define APPLE 1

#include "../../Helpers/pppd/pppd.h"
#include "../../Helpers/pppd/fsm.h"
#include "../../Helpers/pppd/lcp.h"
#include "../../Helpers/pppd/chap-new.h"
#include "../../Helpers/pppd/chap_ms.h"
#include "../../Helpers/pppd/md5.h"
#include "../../Family/ppp_comp.h"

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
static int radius_ip_allowed_address(u_int32_t addr);
static void radius_ip_choose(u_int32_t *addr);
static void radius_ip_up(void *arg, int p);
static void radius_ip_down(void *arg, int p);
static int radius_decryptmppekey(char *key, u_int8_t *attr_value, size_t attr_len, char *secret, char *authenticator);

/* -----------------------------------------------------------------------------
 PPP globals
----------------------------------------------------------------------------- */

extern u_char mppe_send_key[MPPE_MAX_KEY_LEN];
extern u_char mppe_recv_key[MPPE_MAX_KEY_LEN];
extern int mppe_keys_set;		/* Have the MPPE keys been set? */

static CFBundleRef 	bundle = 0;		/* our bundle ref */

/* option variables */

// all the settings are adjustable for each authentication server

// primary authentication server
static int 	pri_auth_port = 0;		// by default, let the radius library decide
static int 	pri_auth_timeout = 10;		// 10 seconds timeout
static int 	pri_auth_retries = 4;		// try 4 times each server
static u_char	*pri_auth_server = NULL;	// by default, let the library use the system files
static u_char	*pri_auth_secret = NULL;	// by default, let the library use the system files

// secondary authentication server
static int 	sec_auth_port = 0;		// if null, use primary port
static int 	sec_auth_timeout = 0;		// if null, use primary timeout
static int 	sec_auth_retries = 0;		// if null, use primary  retries
static u_char	*sec_auth_server = NULL;	// by default, let the library use the system files
static u_char	*sec_auth_secret = NULL;	// if null, use primary secret

// reponses from the radius server
//static struct in_addr user_ipaddr;		/* ip address for the connecting user */

int (*old_pap_auth_hook) __P((char *user, char *passwd, char **msgp,
				 struct wordlist **paddrs,
				 struct wordlist **popts));

int (*old_chap_verify_hook) __P((char *name, char *ourname, int id,
			struct chap_digest_type *digest,
			unsigned char *challenge, unsigned char *response,
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

    old_pap_auth_hook = pap_auth_hook;
    pap_auth_hook = radius_pap_auth;

    old_chap_verify_hook = chap_verify_hook;
    chap_verify_hook = radius_chap_auth;

    ip_choose_hook = radius_ip_choose;
    allowed_address_hook = radius_ip_allowed_address;

    add_notifier(&ip_up_notify, radius_ip_up, 0);
    add_notifier(&ip_down_notify, radius_ip_down, 0);

    // add the radius specific options
    add_options(radius_options);

    return 0;
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
type :  
    0 = PAP, clear text
    5 = CHAP_DIGEST_MD5
    0x80 = CHAP_MICROSOFT
    0x81 = CHAP_MICROSOFT_V2
----------------------------------------------------------------------------- */
static 
int radius_authenticate_user(char *user, char *passwd, int type, 
    char *challenge, int chal_len, int chal_id, 
    void *remotemd, int remotemd_len, unsigned char *message, int message_space)
{
    struct rad_handle *h;
    int err, ret = 0, attr_type;
	void *attr_value;
	size_t attr_len;
	u_int32_t attr_vendor;
    char buf[MS_CHAP_RESPONSE_LEN + 1];
    char auth[MD4_SIGNATURE_SIZE + 1];

    // do some option checking in case not all the options are supplied
    if (sec_auth_server) {
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
    
    h = rad_auth_open();
    if (h == NULL) 
        novm("Radius : can't open context.\n");	// will die...
     
    if (pri_auth_server) {
        err = rad_add_server(h, pri_auth_server, pri_auth_port, pri_auth_secret, pri_auth_timeout, pri_auth_retries);
        if (err != 0) {
            error("Radius : Can't use primary server '%s'\n", pri_auth_server);
            goto done;
        }
    }

    if (sec_auth_server) {
        err = rad_add_server(h, sec_auth_server, sec_auth_port, sec_auth_secret, sec_auth_timeout, sec_auth_retries);
        if (err != 0) {
            error("Radius : Can't use secondary server '%s'\n", sec_auth_server);
            // continue with primary server only
        }
    }
    
    rad_create_request(h, RAD_ACCESS_REQUEST);
    rad_put_string(h, RAD_USER_NAME, user);

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
            rad_put_vendor_attr(h, RAD_VENDOR_MICROSOFT, RAD_MICROSOFT_MS_CHAP_CHALLENGE,
                        challenge, chal_len);
            /* 
                MSCHAP and MSCHAPv2 use the same structure length
                fields UseNT and Flags are at the same place
                MSCHAP : first put id and UseNT, then LANManResp and NTResp
                MSCHAPv2 : put id and Flags, then PeerChallenge, Reserved and NTResp
            */
            buf[0] = chal_id;
            buf[1] = ((MS_ChapResponse *) remotemd)->UseNT;
            memcpy(&buf[2], remotemd, MS_CHAP_RESPONSE_LEN - 1);
            rad_put_vendor_attr(h, RAD_VENDOR_MICROSOFT, 						
                        type == CHAP_MICROSOFT ? RAD_MICROSOFT_MS_CHAP_RESPONSE : RAD_MICROSOFT_MS_CHAP2_RESPONSE,
                        buf, MS_CHAP_RESPONSE_LEN + 1);
            break;
    }

#if 0
    if (remoteaddress)
	rad_put_string(h, RAD_CALLING_STATION_ID, remoteaddress);
#endif
    rad_put_int(h, RAD_SERVICE_TYPE, RAD_FRAMED);
    rad_put_int(h, RAD_FRAMED_PROTOCOL, RAD_PPP);
    
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

					while ((attr_type = rad_get_attr(h, &attr_value, &attr_len)) > 0 ) {

						switch (attr_type) {
						
							case RAD_VENDOR_SPECIFIC: 
							
								attr_type = rad_get_vendor_attr(&attr_vendor, &attr_value,  &attr_len);
								switch (attr_type) {
									case RAD_MICROSOFT_MS_MPPE_SEND_KEY:
										rad_request_authenticator(h, auth, sizeof(auth));
										radius_decryptmppekey(mppe_send_key, attr_value, attr_len, rad_server_secret(h), auth);
										mppe_keys_set = 1;

										break;
									case RAD_MICROSOFT_MS_MPPE_RECV_KEY:
										rad_request_authenticator(h, auth, sizeof(auth));
										radius_decryptmppekey(mppe_recv_key, attr_value, attr_len, rad_server_secret(h), auth);
										mppe_keys_set = 1;
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
                    break;
            }

            break;
        case RAD_ACCESS_CHALLENGE: 
            error("Radius : Received Access Challenge\n");
            break;
        default: 
            error("Radius : Authentication error %d. %s.\n", err, rad_strerror(h));
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
   
    return radius_authenticate_user(user, passwd, 0, 0, 0, 0, 0, 0, 0, 0);
    
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
        response, response_len, message, message_space);
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
void radius_ip_up(void *arg, int p)
{
    // need to implement accounting

}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
static
void radius_ip_down(void *arg, int p)
{
    // need to implement accounting
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
static int
radius_decryptmppekey(char *key, u_int8_t *attr_value, size_t attr_len, char *secret, char *authenticator)
{
    int i;
    u_char  plain[32];
    u_char  buf[16];
    MD5_CTX ctx;
	
    memcpy(plain, attr_value + 2, sizeof(plain)); /* key string */

    MD5Init(&ctx);
    MD5Update(&ctx, secret, strlen(secret));
	MD5Update(&ctx, authenticator, strlen(authenticator));
    MD5Update(&ctx, attr_value, 2); /* salt */
    MD5Final(buf, &ctx);

    for (i = 0; i < 16; i++)
		plain[i] ^= buf[i];

    MD5Init(&ctx);
    MD5Update(&ctx, secret, strlen(secret));
    MD5Update(&ctx, attr_value + 2, 16); /* key string */
    MD5Final(buf, &ctx);

    for (i = 0; i < 16; i++)
		plain[16 + i] ^= buf[i];

	memcpy(key, plain + 1, 16);
    return 0;
}
