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
 *  provide helper function for ipsec operations.
 *
----------------------------------------------------------------------------- */


/* -----------------------------------------------------------------------------
  Includes
----------------------------------------------------------------------------- */

#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/sysctl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <net/route.h>
#include <net/if_dl.h>
#include <netinet6/ipsec.h>
#include <SystemConfiguration/SystemConfiguration.h>
#include <SystemConfiguration/SCValidation.h>
#include <sys/param.h>
#include <syslog.h>
#include <netinet/in_var.h>
#include <sys/kern_event.h>

#include "libpfkey.h"
#include "cf_utils.h"
#include "ipsec_utils.h"
#include "RASSchemaDefinitions.h"
#include "vpnoptions.h"


/* -----------------------------------------------------------------------------
 Definitions
----------------------------------------------------------------------------- */

/* a few macros to simplify the code */
#define WRITE(t)  fprintf(file, "%s%s", TAB_LEVEL[level], t)
#define TWRITE(t)  fprintf(file, "%s", t)
#define FAIL(e) { *errstr = e; goto fail; } 	
#define RACOON_CONFIG_PATH	"/var/run/racoon"

/* -----------------------------------------------------------------------------
    globals
----------------------------------------------------------------------------- */

/* indention level for the racoon file */ 
char *TAB_LEVEL[] = {
	"",				/* level 0 */
	"   ",			/* level 1 */
	"      ",		/* level 2 */
	"         "		/* level 3 */
};

/* -----------------------------------------------------------------------------
    Function Prototypes
----------------------------------------------------------------------------- */

static int racoon_configure(CFDictionaryRef ipsec_dict, char **errstr, int apply);
static int configure_sainfo(int level, FILE *file, CFDictionaryRef ipsec_dict, CFDictionaryRef policy, char **errstr);
static int configure_remote(int level, FILE *file, CFDictionaryRef ipsec_dict, char **errstr);
static int configure_proposal(int level, FILE *file, CFDictionaryRef ipsec_dict, CFDictionaryRef proposal_dict, char **errstr);

static void closeall();
static int racoon_pid();
static int racoon_is_started(char *filename);
static int racoon_restart();
static int makepath( char *path);

/* -----------------------------------------------------------------------------
 Create directories and intermediate directories as required.
 ----------------------------------------------------------------------------- */
static int makepath( char *path)
{
	char	*c;
	char	*thepath;
	int		slen=0;
	int		done = 0;
	mode_t	oldmask, newmask;
	struct stat sb;
	int		error=0;
	
	oldmask = umask(0);
	newmask = S_IRWXU | S_IRGRP | S_IROTH | S_IXGRP | S_IXOTH; 

	slen = strlen(path);
	if  ( !(thepath =  malloc( slen+1) ))
		return -1;
	strlcpy( thepath, path, slen+1);
	c = thepath;
	if ( *c == '/' )
		c++;		
	for(  ; !done; c++){
		if ( (*c == '/') || ( *c == '\0' )){
			if ( *c == '\0' )
				done = 1;
			else 
				*c = '\0';
			if ( mkdir( thepath, newmask) ){
				if ( errno == EEXIST || errno == EISDIR){
					if ( stat(thepath, &sb) < 0){
						error = -1;
						break;
					}
				} else {
					error = -1;
					break;
				}
			}
			*c = '/';
		}
	}
	free(thepath);
	umask(oldmask);
	return error;
}

/* -----------------------------------------------------------------------------
The base-64 encoding packs three 8-bit bytes into four 7-bit ASCII
characters.  If the number of bytes in the original data isn't divisable
by three, "=" characters are used to pad the encoded data.  The complete
set of characters used in base-64 are:
     'A'..'Z' => 00..25
     'a'..'z' => 26..51
     '0'..'9' => 52..61
     '+'      => 62
     '/'      => 63
     '='      => pad

Parameters:
inputData: the data to convert.
file: the file itself.
outputData: the converted output buffer. 
	the caller needs to make sure the buffer is large enough.
----------------------------------------------------------------------------- */
static int EncodeDataUsingBase64(CFDataRef inputData, char *outputData, int maxOutputLen) {
    static const char __CFPLDataEncodeTable[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    
    const uint8_t *bytes = CFDataGetBytePtr(inputData);
    CFIndex length = CFDataGetLength(inputData);
    CFIndex i, pos;
    const uint8_t *p;
	uint8_t * outp = (uint8_t *)outputData;

	if (maxOutputLen < (length + (length / 3) + (length % 3) + 1))
		return 0;
	
    pos = 0;		// position within buf

    for (i = 0, p = bytes; i < length; i++, p++) {
        /* 3 bytes are encoded as 4 */
        switch (i % 3) {
            case 0:
                outp[pos++] = __CFPLDataEncodeTable [ ((p[0] >> 2) & 0x3f)];
                break;
            case 1:
                outp[pos++] = __CFPLDataEncodeTable [ ((((p[-1] << 8) | p[0]) >> 4) & 0x3f)];
                break;
            case 2:
                outp[pos++] = __CFPLDataEncodeTable [ ((((p[-1] << 8) | p[0]) >> 6) & 0x3f)];
                outp[pos++] = __CFPLDataEncodeTable [ (p[0] & 0x3f)];
                break;
        }
    }
        
    switch (i % 3) {
	case 0:
            break;
	case 1:
            outp[pos++] = __CFPLDataEncodeTable [ ((p[-1] << 4) & 0x30)];
            outp[pos++] = '=';
            outp[pos++] = '=';
            break;
	case 2:
            outp[pos++] =  __CFPLDataEncodeTable [ ((p[-1] << 2) & 0x3c)];
            outp[pos++] = '=';
            break;
    }
    
	outp[pos] = 0;
	return pos;
}

/* -----------------------------------------------------------------------------
simple max function
----------------------------------------------------------------------------- */
static inline u_int
max(u_int a, u_int b)
{
	return (a > b ? a : b);
}

/* -----------------------------------------------------------------------------
close all file descriptors, usefule in fork/exec operations
----------------------------------------------------------------------------- */
void 
closeall()
{
    int i;

    for (i = getdtablesize() - 1; i >= 0; i--) close(i);
    open("/dev/null", O_RDWR, 0);
    dup(0);
    dup(0);
    return;
}

/* -----------------------------------------------------------------------------
return the pid of racoon process
----------------------------------------------------------------------------- */
int 
racoon_pid()
{
    int   	pid = 0, err, name[4];
    FILE 	*f;
    size_t	namelen, infolen;
    struct kinfo_proc	info;
    
    f = fopen("/var/run/racoon.pid", "r");
    if (f) {
        fscanf(f, "%d", &pid);
        fclose(f);

        /* 
            check the pid is valid, 
            verify if process is running and is racoon
        */
        name[0] = CTL_KERN;
        name[1] = KERN_PROC;
        name[2] = KERN_PROC_PID;
        name[3] = pid;
        namelen = 4;

        bzero(&info, sizeof(info));
        infolen = sizeof(info);
        
        err = sysctl(name, namelen, &info, &infolen, 0, 0);

        if (err == 0 && !strcmp("racoon", info.kp_proc.p_comm)) {
            /* process exist and is called racoon */
            return pid;
        }
    }

    return 0;
}

/* -----------------------------------------------------------------------------
check if racoon is started with the same configuration file
returns:
    0: racoon is not started
    1: racoon is already started with our configuration
    -1: racoon is already started but with a different configuration
----------------------------------------------------------------------------- */
int 
racoon_is_started(char *filename)
{
    return (racoon_pid() != 0);
}

/* -----------------------------------------------------------------------------
sigusr1 racoon to reload configurations
if racoon was not started, it will be started only if launch is set
----------------------------------------------------------------------------- */
int 
racoon_restart()
{
	int pid = racoon_pid();

	if (pid) {
#ifdef TARGET_EMBEDDED_OS
		kill(pid, SIGHUP);
#else
		kill(pid, SIGUSR1);
#endif
		//sleep(1); // no need to wait
	}
    
    return 0;
}

/* -----------------------------------------------------------------------------
Terminate racoon process.
this is not a good idea...
----------------------------------------------------------------------------- */
static int 
racoon_stop()
{
    int   	pid = racoon_pid();

    if (pid)
        kill(pid, SIGTERM);
    return 0;
}

/* -----------------------------------------------------------------------------
Configure one phase 1 proposal of IPSec
An IPSec configuration can contain several proposals, 
and this function will be called for each of them

Parameters:
level: indentation level of the racoon file (make the generated file prettier).
file: the file itself.
ipsec_dict: dictionary containing the IPSec configuration.
errstr: error string returned in case of configuration error.

Return code:
0 if successful, -1 otherwise.
----------------------------------------------------------------------------- */
int 
configure_proposal(int level, FILE *file, CFDictionaryRef ipsec_dict, CFDictionaryRef proposal_dict, char **errstr)
{
    char 	text[MAXPATHLEN];
	
	/*
		authentication method is OPTIONAL
	*/
	{
		char	str[256];
		CFStringRef auth_method = NULL;
		//CFStringRef prop_method = NULL;
		u_int32_t xauth_enabled = 0;
		
		/* get the default/preferred authentication from the ipsec dictionary, if available */
		auth_method = CFDictionaryGetValue(ipsec_dict, kRASPropIPSecAuthenticationMethod);
		GetIntFromDict(ipsec_dict, kRASPropIPSecXAuthEnabled, &xauth_enabled, 0);

#if 0
		if (proposal_dict)
			prop_method = CFDictionaryGetValue(proposal_dict, kRASPropIPSecProposalAuthenticationMethod);

		strlcpy(str, "pre_shared_key", sizeof(str));
		if (isString(auth_method) || isString(prop_method)) {
				
			if (CFEqual(isString(prop_method) ? prop_method : auth_method, kRASValIPSecProposalAuthenticationMethodSharedSecret))
				strlcpy(str, "pre_shared_key", sizeof(str));
			else if (CFEqual(isString(prop_method) ? prop_method : auth_method, kRASValIPSecProposalAuthenticationMethodCertificate))
				strlcpy(str, "rsasig", sizeof(str));
			else if (CFEqual(isString(prop_method) ? prop_method : auth_method, kRASValIPSecProposalAuthenticationMethodXauthSharedSecretClient))
				strlcpy(str, "xauth_psk_client", sizeof(str));
			else if (CFEqual(isString(prop_method) ? prop_method : auth_method, kRASValIPSecProposalAuthenticationMethodXauthCertificateClient))
				strlcpy(str, "xauth_rsa_client", sizeof(str));
			else if (CFEqual(isString(prop_method) ? prop_method : auth_method, kRASValIPSecProposalAuthenticationMethodHybridCertificateClient))
				strlcpy(str, "hybrid_rsa_client", sizeof(str));
			else 
				FAIL("incorrect authentication method";)
		}
#endif
		strlcpy(str, "pre_shared_key", sizeof(str));
		if (isString(auth_method)) {
				
			if (CFEqual(auth_method, kRASValIPSecAuthenticationMethodSharedSecret))
				strlcpy(str, xauth_enabled ? "xauth_psk_client" : "pre_shared_key", sizeof(str));
			else if (CFEqual(auth_method, kRASValIPSecAuthenticationMethodCertificate))
				strlcpy(str, xauth_enabled ? "xauth_rsa_client" : "rsasig", sizeof(str));
			else if (CFEqual(auth_method, kRASValIPSecAuthenticationMethodHybrid))
				strlcpy(str, "hybrid_rsa_client", sizeof(str));
			else 
				FAIL("incorrect authentication method";)
		}

		snprintf(text, sizeof(text), "authentication_method %s;\n", str);
		WRITE(text);
	}

	/*
		authentication algorithm is OPTIONAL
	*/
	{
		char	str[256];
		CFStringRef algo;

		strlcpy(str, "sha1", sizeof(str));
		if (proposal_dict) {
			algo = CFDictionaryGetValue(proposal_dict, kRASPropIPSecProposalHashAlgorithm);
			if (isString(algo)) {
				
				if (CFEqual(algo, kRASValIPSecProposalHashAlgorithmMD5))
					strlcpy(str, "md5", sizeof(str));
				else if (CFEqual(algo, kRASValIPSecProposalHashAlgorithmSHA1))
					strlcpy(str, "sha1", sizeof(str));
				else 
					FAIL("incorrect authentication algorithm";)

			}
		}
		snprintf(text, sizeof(text), "hash_algorithm %s;\n", str);
		WRITE(text);
	}
	
	/*
		encryption algorithm is OPTIONAL
	*/
	{
		char	str[256];
		CFStringRef crypto;

		strlcpy(str, "3des", sizeof(str));
		if (proposal_dict) {
			crypto = CFDictionaryGetValue(proposal_dict, kRASPropIPSecProposalEncryptionAlgorithm);
			if (isString(crypto)) {
				
				if (CFEqual(crypto, kRASValIPSecProposalEncryptionAlgorithmDES))
					strlcpy(str, "des", sizeof(str));
				else if (CFEqual(crypto, kRASValIPSecProposalEncryptionAlgorithm3DES))
					strlcpy(str, "3des", sizeof(str));
				else if (CFEqual(crypto, kRASValIPSecProposalEncryptionAlgorithmAES))
					strlcpy(str, "aes", sizeof(str));
				else if (CFEqual(crypto, kRASValIPSecProposalEncryptionAlgorithmAES256))
					strlcpy(str, "aes 256", sizeof(str));
				else 
					FAIL("incorrect encryption algorithm";)

			}
		}
		snprintf(text, sizeof(text), "encryption_algorithm %s;\n", str);
		WRITE(text);
	}
	
	/* 
		Lifetime is OPTIONAL
	*/
	{
		u_int32_t lval = 3600;
		if (proposal_dict) {
			GetIntFromDict(proposal_dict, kRASPropIPSecProposalLifetime, &lval, 3600);
		}
		snprintf(text, sizeof(text), "lifetime time %d sec;\n", lval);
		WRITE(text);
	}
	
	/* 
		DH Group is OPTIONAL
	*/
	{
		u_int32_t lval = 2;
		if (proposal_dict) {
			GetIntFromDict(proposal_dict, kRASPropIPSecProposalDHGroup, &lval, 2);
		}
		snprintf(text, sizeof(text), "dh_group %d;\n", lval);
		WRITE(text);
	}
	
	return 0;

fail:
	return -1;
}

/* -----------------------------------------------------------------------------
Configure the phase 1 of IPSec.
The phase 1 contains things like addresses, authentication, and proposals array.

Parameters:
level: indentation level of the racoon file (make the generated file prettier).
file: the file itself.
filename: the name of the file (NULL if no actual file is to be written)
ipsec_dict: dictionary containing the IPSec configuration.
errstr: error string returned in case of configuration error.

Return code:
0 if successful, -1 otherwise.
----------------------------------------------------------------------------- */
#define CERT_VERIFICATION_OPTION_NONE		0
#define CERT_VERIFICATION_OPTION_OPEN_DIR	1
#define CERT_VERIFICATION_OPTION_PEERS_ID	2

int 
configure_remote(int level, FILE *file, CFDictionaryRef ipsec_dict, char **errstr)
{
    char 	text[MAXPATHLEN];
	CFStringRef auth_method = NULL;
	int		cert_verification_option = CERT_VERIFICATION_OPTION_NONE;
	char	*option_str;
	
	
	/* 
		ipsec domain of interpretion and situation 
	*/
    WRITE("doi ipsec_doi;\n");
	WRITE("situation identity_only;\n");

	/* 
		exchange mode is OPTIONAL, default will be main mode 
	*/
	{
		int i, nb;
		CFArrayRef  modes;
		CFStringRef mode;
		
		strlcpy(text, "exchange_mode ", sizeof(text));
		nb = 0;
		modes = CFDictionaryGetValue(ipsec_dict, kRASPropIPSecExchangeMode);
		if (isArray(modes)) {		
			
			nb = max(CFArrayGetCount(modes), 3);
			for (i = 0; i < nb; i++) {
			
				mode = CFArrayGetValueAtIndex(modes, i);
				if (!isString(mode))
					continue;
					
				if (i)
					strlcat(text, ", ", sizeof(text));
					
				if (CFEqual(mode, kRASValIPSecExchangeModeMain))
					strlcat(text, "main", sizeof(text));
				else if (CFEqual(mode, kRASValIPSecExchangeModeAggressive))
					strlcat(text, "aggressive", sizeof(text));
				else if (CFEqual(mode, kRASValIPSecExchangeModeBase))
					strlcat(text, "base", sizeof(text));
				else
					FAIL("incorrect phase 1 exchange mode");
			}
		}
		if (nb == 0) {
			char str[256];
			/* default mode is main except if local identifier is defined */
			if (GetStrFromDict(ipsec_dict, kRASPropIPSecLocalIdentifier, str, sizeof(str), ""))
				strlcat(text, "aggressive", sizeof(text));
			else 
				strlcat(text, "main", sizeof(text));
		}
		strlcat(text, ";\n", sizeof(text));
		WRITE(text);
	}
	
	/*
		get the first authentication method from the proposals
		verify all proposal have the same authentication method
	*/
	{
		
		/* get the default/preferred authentication from the ipsec dictionary, if available */
		auth_method = CFDictionaryGetValue(ipsec_dict, kRASPropIPSecAuthenticationMethod);

#if 0
		CFArrayRef  proposals;
		int 	 i, nb;
		CFDictionaryRef proposal;
		CFStringRef method;

		proposals = CFDictionaryGetValue(ipsec_dict, kRASPropIPSecProposals);
		if (isArray(proposals)) {
			
			nb = CFArrayGetCount(proposals);
			for (i = 0; i < nb; i++) {
				
				proposal = CFArrayGetValueAtIndex(proposals, i);
				if (isDictionary(proposal)) {
					
					method = CFDictionaryGetValue(proposal, kRASPropIPSecProposalAuthenticationMethod);
					if (isString(method)) {
						
						if (auth_method == NULL)
							auth_method = method;
						else if (!CFEqual(auth_method, method))
							FAIL("inconsistent authentication methods");
					}
				}
			}
		}
#endif
		if (auth_method == NULL)
			auth_method = kRASValIPSecAuthenticationMethodSharedSecret;

		if (!CFEqual(auth_method, kRASValIPSecAuthenticationMethodSharedSecret)
			&& !CFEqual(auth_method, kRASValIPSecAuthenticationMethodCertificate)
			&& !CFEqual(auth_method, kRASValIPSecAuthenticationMethodHybrid))
			FAIL("incorrect authentication method found");
	}

	/* 
		local identifier is OPTIONAL 
		if local identifier is not specified, we will configure IKE differently depending
		on the authentication method used
	*/
	{
		char	str[256];
		char	str1[256];

		if (GetStrFromDict(ipsec_dict, CFSTR("LocalIdentifierType"), str1, sizeof(str1), "")) {
			if (!strcmp(str1, "FQDN"))
				strlcpy(str1, "fqdn", sizeof(str1));
			else if (!strcmp(str1, "UserFQDN"))
				strlcpy(str1, "user_fqdn", sizeof(str1));
			else if (!strcmp(str1, "KeyID"))
				strlcpy(str1, "keyid_use", sizeof(str1));
			else if (!strcmp(str1, "Address"))
				strlcpy(str1, "address", sizeof(str1));
			else if (!strcmp(str1, "ASN1DN"))
				strlcpy(str1, "asn1dn", sizeof(str1));
			else 
				strlcpy(str1, "", sizeof(str1));
		}
		
		if (GetStrFromDict(ipsec_dict, kRASPropIPSecLocalIdentifier, str, sizeof(str), "")) {
			snprintf(text, sizeof(text), "my_identifier %s \"%s\";\n", str1[0] ? str1 : "fqdn", str);
			WRITE(text);
		}
		else {
			if (CFEqual(auth_method, kRASValIPSecAuthenticationMethodSharedSecret)
				|| CFEqual(auth_method, kRASValIPSecAuthenticationMethodHybrid)) {
				/* 
					use local address 
				*/
			}
			else if (CFEqual(auth_method, kRASValIPSecAuthenticationMethodCertificate)) {
				/* 
					use subject name from certificate 
				*/
				snprintf(text, sizeof(text), "my_identifier asn1dn;\n");
				WRITE(text);
			}
		}
	}
	
	/*
		remote identifier verification key is OPTIONAL
		by default we will use the remote address
	*/
	{
		CFStringRef string;
		char	str[256];

		string = CFDictionaryGetValue(ipsec_dict, kRASPropIPSecIdentifierVerification);
		if (!isString(string))
			string = kRASValIPSecIdentifierVerificationGenerateFromRemoteAddress;
		
		if (CFEqual(string, kRASValIPSecIdentifierVerificationNone)) {
			/* 
				no verification, use for test purpose
			*/
			WRITE("verify_identifier off;\n");
		}
		else {
			
			if (CFEqual(string, kRASValIPSecIdentifierVerificationGenerateFromRemoteAddress)) {
				/* 
					verify using the remote address 
				*/
				if (!GetStrAddrFromDict(ipsec_dict, kRASPropIPSecRemoteAddress, str, sizeof(str)))
					FAIL("no remote address found");
				snprintf(text, sizeof(text), "peers_identifier address \"%s\";\n", str);
				WRITE(text);
				
				if (CFEqual(auth_method, kRASValIPSecAuthenticationMethodCertificate))
					cert_verification_option = CERT_VERIFICATION_OPTION_PEERS_ID;					
			}
			else if (CFEqual(string, kRASValIPSecIdentifierVerificationUseRemoteIdentifier)) {
				/* 
					verify using the explicitely specified remote identifier key 
				*/
				if (!GetStrFromDict(ipsec_dict, kRASPropIPSecRemoteIdentifier, str, sizeof(str), ""))
					FAIL("no remote identifier found");
				snprintf(text, sizeof(text), "peers_identifier fqdn \"%s\";\n", str);
				WRITE(text);

				if (CFEqual(auth_method, kRASValIPSecAuthenticationMethodCertificate))
					cert_verification_option = CERT_VERIFICATION_OPTION_PEERS_ID;					
			}
			else if (CFEqual(string, kRASValIPSecIdentifierVerificationUseOpenDirectory)) {
				/* 
					verify using open directory (certificates only)
				*/
				if (!CFEqual(auth_method, kRASValIPSecAuthenticationMethodCertificate))
					FAIL("open directory can only be used with certificate authentication");
				cert_verification_option = CERT_VERIFICATION_OPTION_OPEN_DIR;
			}
			else
				FAIL("incorrect verification method");
			
			/* 
				verification is required.
				if certificates are not used - verify peers identifier against the Id payload in the isakmp packet.
				otherwise, verification will be done using the Id in the peers certificate.
			*/
			snprintf(text, sizeof(text), "verify_identifier %s;\n", (cert_verification_option == CERT_VERIFICATION_OPTION_NONE ? "on" : "off"));
			WRITE(text);			
		}
	}
	
	/* 
		Authentication method parameters
	*/
	{
		char	str[256], str1[256];
		CFStringRef	string;
		
		/* 
			Shared Secret authentication method 
		*/
		if (CFEqual(auth_method, kRASValIPSecAuthenticationMethodSharedSecret)
			|| CFEqual(auth_method, kRASValIPSecAuthenticationMethodHybrid)) {
			
			if (!GetStrFromDict(ipsec_dict, kRASPropIPSecSharedSecret, str, sizeof(str), ""))
				FAIL("no shared secret found");
				
			/* 
				Shared Secrets are stored in the KeyChain, in the plist or in the racoon keys file 
			*/
			strlcpy(str1, "use", sizeof(str1));
			string = CFDictionaryGetValue(ipsec_dict, kRASPropIPSecSharedSecretEncryption);
			if (isString(string)) {
				if (CFEqual(string, kRASValIPSecSharedSecretEncryptionKey))
					strlcpy(str1, "key", sizeof(str1));
				else if (CFEqual(string, kRASValIPSecSharedSecretEncryptionKeychain))
					strlcpy(str1, "keychain", sizeof(str1));
				else
					FAIL("incorrect shared secret encryption found"); 
			}
			snprintf(text, sizeof(text), "shared_secret %s \"%s\";\n", str1, str);
			WRITE(text);
		}
		/* 
			Certificates authentication method 
		*/
		else if (CFEqual(auth_method, kRASValIPSecAuthenticationMethodCertificate)) {

			CFDataRef local_cert;

			/* 
				certificate come from the keychain 
			*/
			local_cert  = CFDictionaryGetValue(ipsec_dict, kRASPropIPSecLocalCertificate);
			if (isData(local_cert)) {
				WRITE("certificate_type x509 in_keychain \"");
				fwrite(text, 1, EncodeDataUsingBase64(local_cert, text, sizeof(text)), file);
				TWRITE("\";\n");
			}
			else 
				WRITE("certificate_type x509 in_keychain;\n"); 
				
			WRITE("verify_cert on;\n");
			if (cert_verification_option == CERT_VERIFICATION_OPTION_OPEN_DIR)
					option_str = " use_open_dir";
			else if (cert_verification_option == CERT_VERIFICATION_OPTION_PEERS_ID)
					option_str = " use_peers_identifier";
			else
					option_str = ""; 
			snprintf(text, sizeof(text), "certificate_verification sec_framework%s;\n", option_str);
			WRITE(text);
		}
	}
	
    /* 
		Nonce size key is OPTIONAL 
	*/
	{
		u_int32_t lval;
		GetIntFromDict(ipsec_dict, kRASPropIPSecNonceSize, &lval, 16);
		snprintf(text, sizeof(text), "nonce_size %d;\n", lval);
		WRITE(text);
	}
	
	/*
	 Enable/Disable nat traversal multiple user support
	 */
	{
		u_int32_t	natt_multi_user;
		
		if (GetIntFromDict(ipsec_dict, kRASPropIPSecNattMultipleUsersEnabled, &natt_multi_user, 0)) {
			snprintf(text, sizeof(text), "nat_traversal_multi_user %s;\n", natt_multi_user ? "on" : "off");
			WRITE(text);
		}
	}

	/*
	 Enable/Disable nat traversal keepalive
	 */
	{
		u_int32_t	natt_keepalive;
		
		if (GetIntFromDict(ipsec_dict, kRASPropIPSecNattKeepAliveEnabled, &natt_keepalive, 1)) {
			snprintf(text, sizeof(text), "nat_traversal_keepalive %s;\n", natt_keepalive ? "on" : "off");
			WRITE(text);
		}
	}
	
	/*
	 Enable/Disable Dead Peer Detection
	 */
	{
		int	dpd_enabled, dpd_delay, dpd_retry, dpd_maxfail, blackhole_enabled;
		
		if (GetIntFromDict(ipsec_dict, kRASPropIPSecDeadPeerDetectionEnabled, (u_int32_t *)&dpd_enabled, 0)) {
			GetIntFromDict(ipsec_dict, kRASPropIPSecDeadPeerDetectionDelay, (u_int32_t *)&dpd_delay, 30);
			GetIntFromDict(ipsec_dict, kRASPropIPSecDeadPeerDetectionRetry, (u_int32_t *)&dpd_retry, 5);
			GetIntFromDict(ipsec_dict, kRASPropIPSecDeadPeerDetectionMaxFail, (u_int32_t *)&dpd_maxfail, 5);
			GetIntFromDict(ipsec_dict, kRASPropIPSecBlackHoleDetectionEnabled, (u_int32_t *)&blackhole_enabled, 1);
			snprintf(text, sizeof(text), "dpd_delay %d;\n", dpd_delay);
			WRITE(text);
			snprintf(text, sizeof(text), "dpd_retry %d;\n", dpd_retry);
			WRITE(text);
			snprintf(text, sizeof(text), "dpd_maxfail %d;\n", dpd_maxfail);
			WRITE(text);
			snprintf(text, sizeof(text), "dpd_algorithm %s;\n", blackhole_enabled ? "dpd_blackhole_detect" : "dpd_inbound_detect");
			WRITE(text);
		}
	}

	/*
	 Inactivity timeout
	 */
	{
		int	disconnect_on_idle, disconnect_on_idle_timer;
		
		if (GetIntFromDict(ipsec_dict, kRASPropIPSecDisconnectOnIdle, (u_int32_t *)&disconnect_on_idle, 0)) {
			// 2 minutes default
			GetIntFromDict(ipsec_dict, kRASPropIPSecDisconnectOnIdleTimer, (u_int32_t *)&disconnect_on_idle_timer, 120);
			// only count outgoing traffic -- direction outbound
			snprintf(text, sizeof(text), "disconnect_on_idle idle_timeout %d idle_direction idle_outbound;\n", disconnect_on_idle_timer);
			WRITE(text);
		}
	}
	
    /* 
		other keys 
	*/
    WRITE("initial_contact on;\n");
    WRITE("support_mip6 on;\n");
	
	/* 
		proposal behavior key is OPTIONAL
	 	by default we impose our settings 
	*/
	{
		CFStringRef behavior;
		char	str[256];

		strlcpy(str, "claim", sizeof(str));
		behavior = CFDictionaryGetValue(ipsec_dict, kRASPropIPSecProposalsBehavior);
		if (isString(behavior)) {
			
			if (CFEqual(behavior, kRASValIPSecProposalsBehaviorClaim))
				strlcpy(str, "claim", sizeof(str));
			else if (CFEqual(behavior, kRASValIPSecProposalsBehaviorObey))
				strlcpy(str, "obey", sizeof(str));
			else if (CFEqual(behavior, kRASValIPSecProposalsBehaviorStrict))
				strlcpy(str, "strict", sizeof(str));
			else if (CFEqual(behavior, kRASValIPSecProposalsBehaviorExact))
				strlcpy(str, "exact", sizeof(str));
			else
				FAIL("incorrect proposal behavior");

		}
		snprintf(text, sizeof(text), "proposal_check %s;\n", str);
		WRITE(text);
	}

	/* 
		XAUTH User Name is OPTIONAL
	*/
	{
		char	str[256];

		if (GetStrFromDict(ipsec_dict, kRASPropIPSecXAuthName, str, sizeof(str), "")) {
			snprintf(text, sizeof(text), "xauth_login \"%s\";\n", str);
			WRITE(text);
		}
	}

	/* 
		ModeConfig is OPTIONAL
	*/
	{
		u_int32_t	modeconfig;
		
		if (GetIntFromDict(ipsec_dict, kRASPropIPSecModeConfigEnabled, &modeconfig, 0)) {
			snprintf(text, sizeof(text), "mode_cfg %s;\n", modeconfig ? "on" : "off");
			WRITE(text);
		}
	}
    
	/*
		proposal records are OPTIONAL 
	*/
	{
		int 	i = 0, nb = 0;
		CFArrayRef  proposals;
		CFDictionaryRef proposal = NULL;

		proposals = CFDictionaryGetValue(ipsec_dict, kRASPropIPSecProposals);
		if (isArray(proposals))
			nb = CFArrayGetCount(proposals);
			
		do {
			
			if (nb) {
				proposal = CFArrayGetValueAtIndex(proposals, i);
				if (!isDictionary(proposal))
					FAIL("incorrect phase 1 proposal");
			}
			
			WRITE("\n");
			WRITE("proposal {\n");

			if (configure_proposal(level + 1, file, ipsec_dict, proposal, errstr))
				goto fail;
			
			WRITE("}\n");
	
		
		} while (++i < nb);
	}
	
	return 0;
	
fail:

	return -1;
}

/* -----------------------------------------------------------------------------
Configure one phase 2 proposal of IPSec
An IPSec configuration can contain several proposals, 
and this function will be called for each of them

Parameters:
level: indentation level of the racoon file (make the generated file prettier).
file: the file itself.
ipsec_dict: dictionary containing the IPSec configuration.
policy: dictionary that uses this proposal, from the Policies array of ipsec_dict.
errstr: error string returned in case of configuration error.

Return code:
0 if successful, -1 otherwise.
----------------------------------------------------------------------------- */
int 
configure_sainfo(int level, FILE *file, CFDictionaryRef ipsec_dict, CFDictionaryRef policy, char **errstr)
{
    char 	text[MAXPATHLEN];

	/*
		Encryption algorithms are OPTIONAL
	*/
	{
		CFArrayRef  algos;
		int 	i, nb, found = 0;
		
		strlcpy(text, "encryption_algorithm ", sizeof(text));
		
		if (policy) {
			algos = CFDictionaryGetValue(policy, kRASPropIPSecPolicyEncryptionAlgorithm);
			if (isArray(algos)) {		
				
				nb = CFArrayGetCount(algos);
				if (nb) {
					
					for (i = 0; i < nb; i++) {
					
						CFStringRef algo;
						
						algo = CFArrayGetValueAtIndex(algos, i);
						if (!isString(algo))
							continue;
							
						if (found)
							strlcat(text, ", ", sizeof(text));
							
						if (CFEqual(algo, kRASValIPSecPolicyEncryptionAlgorithmDES))
							strlcat(text, "des", sizeof(text));
						else if (CFEqual(algo, kRASValIPSecPolicyEncryptionAlgorithm3DES))
							strlcat(text, "3des", sizeof(text));
						else if (CFEqual(algo, kRASValIPSecPolicyEncryptionAlgorithmAES))
							strlcat(text, "aes", sizeof(text));
						else 
							FAIL("incorrect encryption algorithm");
							
						found = 1;
					}
					
				}
			}
		}
		if (!found)
			strlcat(text, "aes", sizeof(text));

		strlcat(text, ";\n", sizeof(text));
		WRITE(text);
	}
	
	/* 
		Authentication algorithms are OPTIONAL
	*/
	{
		CFArrayRef  algos;
		int 	i, nb, found = 0;
		
		strlcpy(text, "authentication_algorithm ", sizeof(text));
		
		if (policy) {
			algos = CFDictionaryGetValue(policy, kRASPropIPSecPolicyHashAlgorithm);
			if (isArray(algos)) {		
				
				nb = CFArrayGetCount(algos);
				if (nb) {
					
					for (i = 0; i < nb; i++) {
					
						CFStringRef algo;
						
						algo = CFArrayGetValueAtIndex(algos, i);
						if (!isString(algo))
							continue;
							
						if (found)
							strlcat(text, ", ", sizeof(text));
							
						if (CFEqual(algo, kRASValIPSecPolicyHashAlgorithmSHA1))
							strlcat(text, "hmac_sha1", sizeof(text));
						else if (CFEqual(algo, kRASValIPSecPolicyHashAlgorithmMD5))
							strlcat(text, "hmac_md5", sizeof(text));
						else 
							FAIL("incorrect authentication algorithm");
							
						found = 1;
					}
					
				}
			}
		}
		if (!found)
			strlcat(text, "hmac_sha1", sizeof(text));

		strlcat(text, ";\n", sizeof(text));
		WRITE(text);
	}
	
	/* 
		Compression algorithms are OPTIONAL
	*/
	{
		CFArrayRef  algos;
		int 	i, nb, found = 0;
				
		strlcpy(text, "compression_algorithm ", sizeof(text));
		
		if (policy) {
			algos = CFDictionaryGetValue(policy, kRASPropIPSecPolicyCompressionAlgorithm);
			if (isArray(algos)) {		
				
				nb = CFArrayGetCount(algos);
				if (nb) {

					for (i = 0; i < nb; i++) {
					
						CFStringRef algo;
						
						algo = CFArrayGetValueAtIndex(algos, i);
						if (!isString(algo))
							continue;
							
						if (found)
							strlcat(text, ", ", sizeof(text));
							
						if (CFEqual(algo, kRASValIPSecPolicyCompressionAlgorithmDeflate))
							strlcat(text, "deflate", sizeof(text));
						else 
							FAIL("incorrect compression algorithm");
							
						found = 1;
					}
				}
			}
		}
		if (!found)
			strlcat(text, "deflate", sizeof(text));

		strlcat(text, ";\n", sizeof(text));
		WRITE(text);
	}

	/* 
		lifetime is OPTIONAL
	*/
	{
		u_int32_t lval = 3600;
		if (policy)
			GetIntFromDict(policy, kRASPropIPSecPolicyLifetime, &lval, 3600);
		snprintf(text, sizeof(text), "lifetime time %d sec;\n", lval);
		WRITE(text);
	}

	/* 
		PFS Group is OPTIONAL
	*/
	{
		u_int32_t lval = 0;
		if (policy) { 
			if (GetIntFromDict(policy, kRASPropIPSecPolicyPFSGroup, &lval, 0) && lval) {
				snprintf(text, sizeof(text), "pfs_group %d;\n", lval);
				WRITE(text);
			}
		}
	}
    
	return 0;

fail: 
	
	return -1;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
int 
IPSecApplyConfiguration(CFDictionaryRef ipsec_dict, char **errstr)
{
	return racoon_configure(ipsec_dict, errstr, 1);
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
int 
IPSecValidateConfiguration(CFDictionaryRef ipsec_dict, char **errstr)
{
	return racoon_configure(ipsec_dict, errstr, 0);
}

/* -----------------------------------------------------------------------------
Configure IKE. 
It will perform all necessary validation of the ipsec configuration, generate 
a configuration file (if apply is set) for racoon.
This function is very sequential. It will configure all components of an IKE configuration.
This will not install kernel policies in the kernel and must be done separately.

Parameters:
ipsec_dict: dictionary containing the IPSec configuration.
errstr: error string returned in case of configuration error.
apply: if 1, apply the configuration to racoon, 
		otherwise, just validate the configuration.

Return code:
0 if successful, -1 otherwise.
----------------------------------------------------------------------------- */
int 
racoon_configure(CFDictionaryRef ipsec_dict, char **errstr, int apply)
{
    int 	level = 0, anonymous;
	mode_t	mask;
	FILE	*file = 0;
	char	filename[256], text[256];
	char	text2[256];
	char	local_address[32], remote_address[32];
	struct stat	sb;
	
	filename[0] = 0;

	if (!isDictionary(ipsec_dict))
		FAIL("IPSec dictionary not present");

	/* 
		local address is REQUIRED 
		verify it is defined, not needed in racoon configuration
	*/
	if (!GetStrAddrFromDict(ipsec_dict, kRASPropIPSecLocalAddress, local_address, sizeof(local_address)))
		FAIL("incorrect local address found");

	/* 
		remote address is REQUIRED 
	*/
	if (!GetStrAddrFromDict(ipsec_dict, kRASPropIPSecRemoteAddress, remote_address, sizeof(remote_address)))
		FAIL("incorrect remote address found");

	anonymous = inet_addr(remote_address) == 0;
    /*
		create the configuration file 
		make the file only readable by root, so we can stick the shared secret inside 
	*/
	if (apply) {
		snprintf(filename, sizeof(filename), RACOON_CONFIG_PATH "/%s.conf", anonymous ? "anonymous" : remote_address);
		/* remove any existing leftover, to create with right permissions */
		remove(filename);
		if (stat(RACOON_CONFIG_PATH, &sb) != 0 && errno == ENOENT) {
			/* Create the path */ 
			if ( makepath( RACOON_CONFIG_PATH ) ){
				snprintf(text, sizeof(text), "cannot create racoon configuration file path (error %d)", errno);
				FAIL(text);
			}
		}
	}
	
	/* 
		turn off group/other bits to create file rw owner only 
	*/
	mask = umask(S_IRWXG|S_IRWXO);
	file = fopen(apply ? filename : "/dev/null" , "w");
	mask = umask(mask);
    if (file == NULL) {
		snprintf(text, sizeof(text), "cannot create racoon configuration file (error %d)", errno);
		FAIL(text);
	}

    /*
		write the remote record. this is common to all the proposals and sainfo records
	*/
	
	/* XXX hack iphone... remove immediately*/
	if (CFDictionaryGetValue(ipsec_dict, CFSTR("UseAnonymousPolicy")))
		anonymous = 1;

	{
		
		snprintf(text, sizeof(text), "remote %s {\n", anonymous ? "anonymous" : remote_address);
		WRITE(text);
		
		/*
			configure now all the remote directives
		*/
		if (configure_remote(level + 1, file, ipsec_dict, errstr))
			goto fail;
				
		/* end of the remote record */
		WRITE("}\n\n");
	}
	
    /*
	 * write the sainfo records 
	 */
	
	{
		CFArrayRef policies;
		int i, nb;

		policies = CFDictionaryGetValue(ipsec_dict, kRASPropIPSecPolicies);
		if (!isArray(policies)
			|| (nb = CFArrayGetCount(policies)) == 0)
			//FAIL("no policies found");
			goto no_policy; // do not fail anymore
		
		/* if this is the anonymous configuration, only take the first policy */
		if (anonymous)
			nb = 1;
			
		for (i = 0; i < nb; i++) {
				
			CFDictionaryRef policy;
			CFStringRef policylevel, policymode;
			char	local_network[256], remote_network[256];
			u_int32_t local_port, remote_port;
			u_int32_t local_prefix, remote_prefix;
			u_int32_t protocol, tunnel;

			policy = CFArrayGetValueAtIndex(policies, i);
			if (!isDictionary(policy))
				FAIL("incorrect policy found");
				
			/* if policy leve is not specified, None is assumed */
			policylevel = CFDictionaryGetValue(policy, kRASPropIPSecPolicyLevel);
			if (!isString(policylevel) || CFEqual(policylevel, kRASValIPSecPolicyLevelNone))
				continue;
			else if (CFEqual(policylevel, kRASValIPSecPolicyLevelDiscard))
				continue;
			else if (!CFEqual(policylevel, kRASValIPSecPolicyLevelRequire))
				FAIL("incorrect policy level found");
			
			if (anonymous) {
				snprintf(text, sizeof(text), "sainfo anonymous {\n");
			}
			else {
				/*
					IPSec is required for this policy 
					write the sainfo record
					local and remote network are REQUIRED
				*/
				tunnel = 1;
				policymode = CFDictionaryGetValue(policy, kRASPropIPSecPolicyMode);
				if (isString(policymode)) {
				
					if (CFEqual(policymode, kRASValIPSecPolicyModeTunnel))
							tunnel = 1;
					else if (CFEqual(policymode, kRASValIPSecPolicyModeTransport))
							tunnel = 0;
					else
						FAIL("incorrect policy type found");
				}

				if (tunnel) {
					if (!GetStrNetFromDict(policy, kRASPropIPSecPolicyLocalAddress, local_network, sizeof(local_network)))  
						FAIL("incorrect policy local network");
						
					if (!GetStrNetFromDict(policy, kRASPropIPSecPolicyRemoteAddress, remote_network, sizeof(remote_network)))
						FAIL("incorrect policy remote network");

					GetIntFromDict(policy, kRASPropIPSecPolicyLocalPrefix, &local_prefix, 24);
					if (local_prefix == 0)
						FAIL("incorrect policy local prefix");

					GetIntFromDict(policy, kRASPropIPSecPolicyRemotePrefix, &remote_prefix, 24);
					if (remote_prefix == 0)
						FAIL("incorrect policy remote prefix");

					snprintf(text, sizeof(text), "sainfo address %s/%d 0 address %s/%d 0 {\n", 
						local_network, local_prefix, 
						remote_network, remote_prefix);
					snprintf(text2, sizeof(text2), "sainfo address %s/%d 0 address %s/%d 0 {\n", 
							 remote_network, remote_prefix,
							 local_network, local_prefix);
					
				}
				else {
					
					GetIntFromDict(policy, kRASPropIPSecPolicyLocalPort, &local_port, 0);
					GetIntFromDict(policy, kRASPropIPSecPolicyRemotePort, &remote_port, 0);
					GetIntFromDict(policy, kRASPropIPSecPolicyProtocol, &protocol, 0);

					snprintf(text, sizeof(text), "sainfo address %s/32 [%d] %d address %s/32 [%d] %d {\n", 
						local_address, local_port, protocol, 
						remote_address, remote_port, protocol);
					snprintf(text2, sizeof(text2), "sainfo address %s/32 [%d] %d address %s/32 [%d] %d {\n", 
							 remote_address, remote_port, protocol,
							 local_address, local_port, protocol);
					
				}
			}
			
			WRITE(text);

			if (configure_sainfo(level + 1, file, ipsec_dict, policy, errstr))
				goto fail;
						
			/* end of the record */
			WRITE("}\n\n");

			if ( !anonymous ){
				/* write bidirectional info */
				WRITE(text2);

				if (configure_sainfo(level + 1, file, ipsec_dict, policy, errstr))
					goto fail;

				/* end of the record */
				WRITE("}\n\n");
			}

		}
	
		no_policy:
			;

	}
	
    fclose(file);

    /*
	 * signal racoon 
	 */

	if (apply) {
		racoon_restart();
	}
	
    return 0;

fail:

    if (file)
		fclose(file);
    if (filename[0])
		remove(filename);
	return -1;
}


/* -----------------------------------------------------------------------------
Unconfigure IPSec. 
Remove an configuration from IPSec, remove the racoon file and restart racoon..
This will not remove kernel policies in the kernel and must be done separately.

Parameters:
ipsec_dict: dictionary containing the IPSec configuration.
errstr: error string returned in case of configuration error.

Return code:
0 if successful, -1 otherwise.
----------------------------------------------------------------------------- */
int 
IPSecRemoveConfiguration(CFDictionaryRef ipsec_dict, char **errstr) 
{
    int 	anonymous;
	char	filename[256];
	char	remote_address[32];
	
	filename[0] = 0;

	if (!isDictionary(ipsec_dict))
		FAIL("IPSec dictionary not present");

	if (!GetStrAddrFromDict(ipsec_dict, kRASPropIPSecRemoteAddress, remote_address, sizeof(remote_address)))
		FAIL("incorrect remote address found");

	anonymous = inet_addr(remote_address) == 0;

	// don't remove anymous entry ?
	if (anonymous)
		return 0;
	
	snprintf(filename, sizeof(filename), RACOON_CONFIG_PATH "/%s.conf", remote_address);
	remove(filename);
	
	racoon_restart();
    return 0;

fail:
	
	return -1;
}

/* -----------------------------------------------------------------------------
Unconfigure IPSec. 
Remove an configuration from IPSec, remove the racoon file does not restart racoon.
This will not remove kernel policies in the kernel and must be done separately.

Parameters:
ipsec_dict: dictionary containing the IPSec configuration.
errstr: error string returned in case of configuration error.

Return code:
0 if successful, -1 otherwise.
----------------------------------------------------------------------------- */
int 
IPSecRemoveConfigurationFile(CFDictionaryRef ipsec_dict, char **errstr) 
{
    int 	anonymous;
	char	filename[256];
	char	remote_address[32];
	
	filename[0] = 0;

	if (!isDictionary(ipsec_dict))
		FAIL("IPSec dictionary not present");

	if (!GetStrAddrFromDict(ipsec_dict, kRASPropIPSecRemoteAddress, remote_address, sizeof(remote_address)))
		FAIL("incorrect remote address found");

	anonymous = inet_addr(remote_address) == 0;

	// don't remove anymous entry ?
	if (anonymous)
		return 0;
	
	snprintf(filename, sizeof(filename), RACOON_CONFIG_PATH "/%s.conf", remote_address);
	remove(filename);
	
    return 0;

fail:
	
	return -1;
}

/* -----------------------------------------------------------------------------
Kick IPSec racoon when changes have been made before to the configuration files. 

Parameters:

Return code:
0 if successful, -1 otherwise.
----------------------------------------------------------------------------- */
int 
IPSecKickConfiguration() 
{
	
	racoon_restart();
    return 0;
}

/* -----------------------------------------------------------------------------
Count the number of IPSec kernel policies in the configuration. 

Parameters:
ipsec_dict: dictionary containing the IPSec configuration.

Return code:
number of policies in the configuration.
----------------------------------------------------------------------------- */
int 
IPSecCountPolicies(CFDictionaryRef ipsec_dict) 
{
	CFArrayRef  policies;

	policies = CFDictionaryGetValue(ipsec_dict, kRASPropIPSecPolicies);
	if (isArray(policies))
		return CFArrayGetCount(policies);		
		
    return 0;
}

/* -----------------------------------------------------------------------------
Install IPSec kernel policies. 
This will not configure IKE and must be done separately.

Parameters:
ipsec_dict: dictionary containing the IPSec configuration.
index: -1, install all policies defined in the configuration
	otherwise, install only the policy at the specified index.
errstr: error string returned in case of configuration error.

Return code:
0 if successful, -1 otherwise.
----------------------------------------------------------------------------- */
int 
IPSecInstallPolicies(CFDictionaryRef ipsec_dict, CFIndex index, char ** errstr) 
{
    int			s = -1, err, seq = 0, i, nb;
    char		policystr_in[64], policystr_out[64], src_address[32], dst_address[32], str[32];
    caddr_t		policy_in = 0, policy_out = 0;
    u_int32_t	policylen_in, policylen_out, local_prefix, remote_prefix;
	u_int32_t	protocol = 0xFF;
	CFArrayRef  policies;
	struct sockaddr_in  local_net;
	struct sockaddr_in  remote_net;
	CFIndex	start, end;

    s = pfkey_open();
    if (s < 0) 
		FAIL("cannot open a pfkey socket");
    
	if (!GetStrAddrFromDict(ipsec_dict, kRASPropIPSecLocalAddress, src_address, sizeof(src_address)))
		FAIL("incorrect local address");

	if (!GetStrAddrFromDict(ipsec_dict, kRASPropIPSecRemoteAddress, dst_address, sizeof(dst_address)))
		FAIL("incorrect remote address");
	
	policies = CFDictionaryGetValue(ipsec_dict, kRASPropIPSecPolicies);
	if (!isArray(policies)
		|| (nb = CFArrayGetCount(policies)) == 0
		|| index > nb)
		FAIL("no policies found");
		
	if (index == -1) {
		start = 0;
		end = nb;
	}
	else {
		start = index;
		end = index + 1;
	}
	for (i = start; i < end; i++) {
	
		int		tunnel, in, out;
		CFDictionaryRef policy;
		CFStringRef policymode, policydirection, policylevel;

		policy = CFArrayGetValueAtIndex(policies, i);
		if (!isDictionary(policy))
			FAIL("incorrect policy found");
	
		/* build policies in and out */

		tunnel = 1;
		policymode = CFDictionaryGetValue(policy, kRASPropIPSecPolicyMode);
		if (isString(policymode)) {
		
			if (CFEqual(policymode, kRASValIPSecPolicyModeTunnel))
				tunnel = 1;
			else if (CFEqual(policymode, kRASValIPSecPolicyModeTransport))
				tunnel = 0;
			else
				FAIL("incorrect policy type found");
		}

		/* if policy direction is not specified, in/out is assumed */
		in = out = 1;
		policydirection = CFDictionaryGetValue(policy, kRASPropIPSecPolicyDirection);
		if (isString(policydirection)) {
		
			if (CFEqual(policydirection, kRASValIPSecPolicyDirectionIn))
				out = 0;
			else if (CFEqual(policydirection, kRASValIPSecPolicyDirectionOut))
				in = 0;
			else if (!CFEqual(policydirection, kRASValIPSecPolicyDirectionInOut))
				FAIL("incorrect policy direction found");
		}

		policylevel = CFDictionaryGetValue(policy, kRASPropIPSecPolicyLevel);
		if (!isString(policylevel) || CFEqual(policylevel, kRASValIPSecPolicyLevelNone)) {
			snprintf(policystr_out, sizeof(policystr_out), "out none");
			snprintf(policystr_in, sizeof(policystr_in), "in none");
		}
		else if (CFEqual(policylevel, kRASValIPSecPolicyLevelUnique)) {
			if (tunnel) {
				snprintf(policystr_out, sizeof(policystr_out), "out ipsec esp/tunnel/%s-%s/unique", src_address, dst_address);
				snprintf(policystr_in, sizeof(policystr_in), "in ipsec esp/tunnel/%s-%s/unique", dst_address, src_address);
			}
			else {
				snprintf(policystr_out, sizeof(policystr_out), "out ipsec esp/transport//unique");
				snprintf(policystr_in, sizeof(policystr_in), "in ipsec esp/transport//unique");
			}
		}
		else if (CFEqual(policylevel, kRASValIPSecPolicyLevelRequire)) {
			if (tunnel) {
				snprintf(policystr_out, sizeof(policystr_out), "out ipsec esp/tunnel/%s-%s/require", src_address, dst_address);
				snprintf(policystr_in, sizeof(policystr_in), "in ipsec esp/tunnel/%s-%s/require", dst_address, src_address);
			}
			else {
				snprintf(policystr_out, sizeof(policystr_out), "out ipsec esp/transport//require");
				snprintf(policystr_in, sizeof(policystr_in), "in ipsec esp/transport//require");
			}
		}
		else if (CFEqual(policylevel, kRASValIPSecPolicyLevelDiscard)) {
			snprintf(policystr_out, sizeof(policystr_out), "out discard");
			snprintf(policystr_in, sizeof(policystr_in), "in discard");
		}
		else 
			FAIL("incorrect policy level");
		
		policy_in = ipsec_set_policy(policystr_in, strlen(policystr_in));
		if (policy_in == 0)
			FAIL("cannot set policy in");

		policy_out = ipsec_set_policy(policystr_out, strlen(policystr_out));
		if (policy_out == 0)
			FAIL("cannot set policy out");

		policylen_in = ((struct sadb_x_policy *)policy_in)->sadb_x_policy_len << 3;
		policylen_out = ((struct sadb_x_policy *)policy_out)->sadb_x_policy_len << 3;


		if (tunnel) {
			/* get local and remote networks */
		
			if (!GetStrNetFromDict(policy, kRASPropIPSecPolicyLocalAddress, str, sizeof(str)))
				FAIL("incorrect local network");
						
			local_net.sin_len = sizeof(local_net);
			local_net.sin_family = AF_INET;
			local_net.sin_port = htons(0);
			if (!inet_aton(str, &local_net.sin_addr))
				FAIL("incorrect local network");

			GetIntFromDict(policy, kRASPropIPSecPolicyLocalPrefix, &local_prefix, 24);

			if (!GetStrNetFromDict(policy, kRASPropIPSecPolicyRemoteAddress, str, sizeof(str)))
				FAIL("incorrect remote network0");
						
			remote_net.sin_len = sizeof(remote_net);
			remote_net.sin_family = AF_INET;
			remote_net.sin_port = htons(0);
			if (!inet_aton(str, &remote_net.sin_addr))
				FAIL("incorrect remote network1");

			GetIntFromDict(policy, kRASPropIPSecPolicyRemotePrefix, &remote_prefix, 24);
		
		}
		else {
			u_int32_t val;
			
			local_net.sin_len = sizeof(local_net);
			local_net.sin_family = AF_INET;
			GetIntFromDict(policy, kRASPropIPSecPolicyLocalPort, &val, 0);
			local_net.sin_port = htons(val);
			if (!inet_aton(src_address, &local_net.sin_addr))
				FAIL("incorrect local address");

			local_prefix = local_net.sin_addr.s_addr ? 32 : 0;

			remote_net.sin_len = sizeof(remote_net);
			remote_net.sin_family = AF_INET;
			GetIntFromDict(policy, kRASPropIPSecPolicyRemotePort, &val, 0);
			remote_net.sin_port = htons(val);
			if (!inet_aton(dst_address, &remote_net.sin_addr))
				FAIL("incorrect remote address");

			remote_prefix = remote_net.sin_addr.s_addr ? 32 : 0;
			
			GetIntFromDict(policy, kRASPropIPSecPolicyProtocol, &protocol, 0);

		}
		
		/* configure kernel policies */
		
		if (out) {
			err = pfkey_send_spdadd(s, (struct sockaddr *)&local_net, local_prefix, (struct sockaddr *)&remote_net, remote_prefix, protocol, policy_out, policylen_out, seq++);
			if (err < 0)
				FAIL("cannot add policy out");
		}
		
		if (in) {
			err = pfkey_send_spdadd(s, (struct sockaddr *)&remote_net, remote_prefix, (struct sockaddr *)&local_net, local_prefix, protocol, policy_in, policylen_in, seq++);
			if (err < 0)
				FAIL("cannot add policy in");
		}
			
		free(policy_in);
		free(policy_out);
		policy_in = 0;
		policy_out = 0;
	}

	pfkey_close(s);
	return 0;

fail:
	if (policy_in)
		free(policy_in);
	if (policy_out)
		free(policy_out);
	if (s != -1)
		pfkey_close(s);
    return -1;
}


/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
static int 
install_remove_routes(int cmd, CFDictionaryRef ipsec_dict, CFIndex index, char ** errstr, struct in_addr gateway) 
{
    int			s = -1, i, nb;
    char		src_address[32], dst_address[32], str[32];
    u_int32_t	remote_prefix;
	CFArrayRef  policies;
	struct sockaddr_in  local_net;
	struct sockaddr_in  remote_net;
	CFIndex	start, end;
    int 			len;
    int 			rtm_seq = 0;

    struct {
	struct rt_msghdr	hdr;
	struct sockaddr_in	dst;
        struct sockaddr_in	gway;
        struct sockaddr_in	mask;
    } rtmsg;

    s = socket(PF_ROUTE, SOCK_RAW, AF_INET);
    if (s < 0) 
		FAIL("cannot open a routing socket");
    
	if (!GetStrAddrFromDict(ipsec_dict, kRASPropIPSecLocalAddress, src_address, sizeof(src_address)))
		FAIL("incorrect local address");

	if (!GetStrAddrFromDict(ipsec_dict, kRASPropIPSecRemoteAddress, dst_address, sizeof(dst_address)))
		FAIL("incorrect remote address");
	
	policies = CFDictionaryGetValue(ipsec_dict, kRASPropIPSecPolicies);
	if (!isArray(policies)
		|| (nb = CFArrayGetCount(policies)) == 0
		|| index > nb)
		FAIL("no policies found");
		
	if (index == -1) {
		start = 0;
		end = nb;
	}
	else {
		start = index;
		end = index + 1;
	}
	for (i = start; i < end; i++) {
	
		int		tunnel, in, out;
		CFDictionaryRef policy;
		CFStringRef policymode, policydirection, policylevel;

		policy = CFArrayGetValueAtIndex(policies, i);
		if (!isDictionary(policy))
			FAIL("incorrect policy found");
	
		/* build policies in and out */

		tunnel = 1;
		policymode = CFDictionaryGetValue(policy, kRASPropIPSecPolicyMode);
		if (isString(policymode)) {
		
			if (CFEqual(policymode, kRASValIPSecPolicyModeTunnel))
				tunnel = 1;
			else if (CFEqual(policymode, kRASValIPSecPolicyModeTransport))
				tunnel = 0;
			else
				FAIL("incorrect policy type found");
		}

		if (tunnel == 0)
			continue;	// no need for routes for 'tranport' policies

		/* if policy direction is not specified, in/out is assumed */
		in = out = 1;
		policydirection = CFDictionaryGetValue(policy, kRASPropIPSecPolicyDirection);
		if (isString(policydirection)) {
		
			if (CFEqual(policydirection, kRASValIPSecPolicyDirectionIn))
				out = 0;
			else if (CFEqual(policydirection, kRASValIPSecPolicyDirectionOut))
				in = 0;
			else if (!CFEqual(policydirection, kRASValIPSecPolicyDirectionInOut))
				FAIL("incorrect policy direction found");
		}

		if (out == 0)
			continue;	// no need for routes for 'in' policies
			
		policylevel = CFDictionaryGetValue(policy, kRASPropIPSecPolicyLevel);
		if (!isString(policylevel) || CFEqual(policylevel, kRASValIPSecPolicyLevelNone))
			continue; // no need for routes for 'none' policies

		if (!CFEqual(policylevel, kRASValIPSecPolicyLevelRequire) && !CFEqual(policylevel, kRASValIPSecPolicyLevelDiscard)  && !CFEqual(policylevel, kRASValIPSecPolicyLevelUnique))
			FAIL("incorrect policy level");
	
		/* get local and remote networks */
	
		if (!GetStrNetFromDict(policy, kRASPropIPSecPolicyLocalAddress, str, sizeof(str)))
			FAIL("incorrect local network");
					
		local_net.sin_len = sizeof(local_net);
		local_net.sin_family = AF_INET;
		local_net.sin_port = htons(0);
		if (!inet_aton(str, &local_net.sin_addr))
			FAIL("incorrect local network");

		if (!GetStrNetFromDict(policy, kRASPropIPSecPolicyRemoteAddress, str, sizeof(str)))
			FAIL("incorrect remote network0");
					
		remote_net.sin_len = sizeof(remote_net);
		remote_net.sin_family = AF_INET;
		remote_net.sin_port = htons(0);
		if (!inet_aton(str, &remote_net.sin_addr))
			FAIL("incorrect remote network1");


		memset(&rtmsg, 0, sizeof(rtmsg));
		rtmsg.hdr.rtm_type = cmd;
		rtmsg.hdr.rtm_flags = RTF_UP | RTF_STATIC;
		rtmsg.hdr.rtm_flags |= RTF_GATEWAY;
		rtmsg.hdr.rtm_version = RTM_VERSION;
		rtmsg.hdr.rtm_seq = ++rtm_seq;
		rtmsg.hdr.rtm_addrs = RTA_DST | RTA_NETMASK | RTA_GATEWAY;
		rtmsg.dst.sin_len = sizeof(rtmsg.dst);
		rtmsg.dst.sin_family = AF_INET;
		rtmsg.dst.sin_addr = remote_net.sin_addr;
		rtmsg.gway.sin_len = sizeof(rtmsg.gway);
		rtmsg.gway.sin_family = AF_INET;
		rtmsg.gway.sin_addr = gateway;
		rtmsg.mask.sin_len = sizeof(rtmsg.mask);
		rtmsg.mask.sin_family = AF_INET;
		GetIntFromDict(policy, kRASPropIPSecPolicyRemotePrefix, &remote_prefix, 24);
		for (rtmsg.mask.sin_addr.s_addr = 0; remote_prefix; remote_prefix--)
			rtmsg.mask.sin_addr.s_addr = (rtmsg.mask.sin_addr.s_addr>>1)|0x80000000;
		rtmsg.mask.sin_addr.s_addr = htonl(rtmsg.mask.sin_addr.s_addr);
		len = sizeof(rtmsg);
		rtmsg.hdr.rtm_msglen = len;
		if (write(s, &rtmsg, len) < 0)
			printf("cannot write on routing socket %d (0x%x --> 0x%x)\n", errno, htonl(remote_net.sin_addr.s_addr), htonl(gateway.s_addr));			;//FAIL("cannot write on routing socket", errno);
		
	}

	close(s);
	return 0;

fail:
	if (s != -1)
		close(s);
    return -1;
}

/* -----------------------------------------------------------------------------
Install IPSec Routes. 
This will not configure IKE or intall policies,  and must be done separately.

Parameters:
ipsec_dict: dictionary containing the IPSec configuration.
index: -1, install all policies defined in the configuration
	otherwise, install only the policy at the specified index.
errstr: error string returned in case of configuration error.
gateway: gateway for the routes

Return code:
0 if successful, -1 otherwise.
----------------------------------------------------------------------------- */
int 
IPSecInstallRoutes(CFDictionaryRef ipsec_dict, CFIndex index, char ** errstr, struct in_addr gateway) 
{
	return install_remove_routes(RTM_ADD, ipsec_dict, index, errstr, gateway);
}

/* -----------------------------------------------------------------------------
Remove IPSec Routes. 
This will not unconfigure IKE or remove policies, and must be done separately.

Parameters:
ipsec_dict: dictionary containing the IPSec configuration.
index: -1, install all policies defined in the configuration
	otherwise, install only the policy at the specified index.
errstr: error string returned in case of configuration error.
gateway: gateway for the routes

Return code:
0 if successful, -1 otherwise.
----------------------------------------------------------------------------- */
int 
IPSecRemoveRoutes(CFDictionaryRef ipsec_dict, CFIndex index, char ** errstr, struct in_addr gateway) 
{
	return install_remove_routes(RTM_DELETE, ipsec_dict, index, errstr, gateway);
}

/* -----------------------------------------------------------------------------
Remove IPSec kernel policies. 
This will not unconfigure IKE and must be done separately.

Parameters:
ipsec_dict: dictionary containing the IPSec configuration.
index: -1, remove all policies defined in the configuration
	otherwise, remove only the policy at the specified index.
errstr: error string returned in case of configuration error.

Return code:
0 if successful, -1 otherwise.
----------------------------------------------------------------------------- */
int 
IPSecRemovePolicies(CFDictionaryRef ipsec_dict, CFIndex index, char ** errstr) 
{
    int			s = -1, err, seq = 0, nb, i;
    char		policystr_in[64], policystr_out[64], str[32];
    caddr_t		policy_in = 0, policy_out = 0;
    u_int32_t	policylen_in, policylen_out, local_prefix, remote_prefix;
	u_int32_t	protocol = 0xFF;
	CFArrayRef  policies;
	struct sockaddr_in  local_net;
	struct sockaddr_in  remote_net;
    char		src_address[32], dst_address[32];
	CFIndex start, end;

    s = pfkey_open();
    if (s < 0)
		FAIL("cannot open a pfkey socket");

	if (!GetStrAddrFromDict(ipsec_dict, kRASPropIPSecLocalAddress, src_address, sizeof(src_address)))
		FAIL("incorrect local address");

	if (!GetStrAddrFromDict(ipsec_dict, kRASPropIPSecRemoteAddress, dst_address, sizeof(dst_address)))
		FAIL("incorrect remote address");
	
	policies = CFDictionaryGetValue(ipsec_dict, kRASPropIPSecPolicies);
	if (!isArray(policies)
		|| (nb = CFArrayGetCount(policies)) == 0
		|| index > nb)
		FAIL("no policies found");
		
	if (index == -1) {
		start = 0;
		end = nb;
	}
	else {
		start = index;
		end = index + 1;
	}
	for (i = start; i < end; i++) {
		
		CFDictionaryRef policy;
		int		tunnel, in, out;
		CFStringRef policymode, policydirection;

		policy = CFArrayGetValueAtIndex(policies, i);
		if (!isDictionary(policy))
			FAIL("incorrect policy found");
	
		/* build policies in and out */

		tunnel = 1;
		policymode = CFDictionaryGetValue(policy, kRASPropIPSecPolicyMode);
		if (isString(policymode)) {
		
			if (CFEqual(policymode, kRASValIPSecPolicyModeTunnel))
				tunnel = 1;
			else if (CFEqual(policymode, kRASValIPSecPolicyModeTransport))
				tunnel = 0;
			else
				FAIL("incorrect policy type found");
		}

		/* if policy direction is not specified, in/out is assumed */
		in = out = 1;
		policydirection = CFDictionaryGetValue(policy, kRASPropIPSecPolicyDirection);
		if (isString(policydirection)) {
		
			if (CFEqual(policydirection, kRASValIPSecPolicyDirectionIn))
				out = 0;
			else if (CFEqual(policydirection, kRASValIPSecPolicyDirectionOut))
				in = 0;
			else if (!CFEqual(policydirection, kRASValIPSecPolicyDirectionInOut))
				FAIL("incorrect policy direction found");
		}

		snprintf(policystr_out, sizeof(policystr_out), "out");
		snprintf(policystr_in, sizeof(policystr_in), "in");
		
		policy_in = ipsec_set_policy(policystr_in, strlen(policystr_in));
		if (policy_in == 0)
			FAIL("cannot set policy in");

		policy_out = ipsec_set_policy(policystr_out, strlen(policystr_out));
		if (policy_out == 0)
			FAIL("cannot set policy out");

		policylen_in = ((struct sadb_x_policy *)policy_in)->sadb_x_policy_len << 3;
		policylen_out = ((struct sadb_x_policy *)policy_out)->sadb_x_policy_len << 3;
		

		if (tunnel) {
			/* get local and remote networks */
		
			if (!GetStrNetFromDict(policy, kRASPropIPSecPolicyLocalAddress, str, sizeof(str)))
				FAIL("incorrect local network");
						
			local_net.sin_len = sizeof(local_net);
			local_net.sin_family = AF_INET;
			local_net.sin_port = htons(0);
			if (!inet_aton(str, &local_net.sin_addr))
				FAIL("incorrect local network");

			GetIntFromDict(policy, kRASPropIPSecPolicyLocalPrefix, &local_prefix, 24);

			if (!GetStrNetFromDict(policy, kRASPropIPSecPolicyRemoteAddress, str, sizeof(str)))
				FAIL("incorrect remote network");
						
			remote_net.sin_len = sizeof(remote_net);
			remote_net.sin_family = AF_INET;
			remote_net.sin_port = htons(0);
			if (!inet_aton(str, &remote_net.sin_addr))
				FAIL("incorrect remote network");

			GetIntFromDict(policy, kRASPropIPSecPolicyRemotePrefix, &remote_prefix, 24);
		
		}
		else {
			u_int32_t val;
			
			local_net.sin_len = sizeof(local_net);
			local_net.sin_family = AF_INET;
			GetIntFromDict(policy, kRASPropIPSecPolicyLocalPort, &val, 0);
			local_net.sin_port = htons(val);
			if (!inet_aton(src_address, &local_net.sin_addr))
				FAIL("incorrect local address");

			local_prefix = local_net.sin_addr.s_addr ? 32 : 0;

			remote_net.sin_len = sizeof(remote_net);
			remote_net.sin_family = AF_INET;
			GetIntFromDict(policy, kRASPropIPSecPolicyRemotePort, &val, 0);
			remote_net.sin_port = htons(val);
			if (!inet_aton(dst_address, &remote_net.sin_addr))
				FAIL("incorrect remote address");

			remote_prefix = remote_net.sin_addr.s_addr ? 32 : 0;
			
			GetIntFromDict(policy, kRASPropIPSecPolicyProtocol, &protocol, 0);
		}
		
		/* unconfigure kernel policies */
		
		if (out) {
			err = pfkey_send_spddelete(s, (struct sockaddr *)&local_net, local_prefix, (struct sockaddr *)&remote_net, remote_prefix, protocol, policy_out, policylen_out, seq++);
			if (err < 0)
				FAIL("cannot delete policy out");
		}
		
		if (in) {
			err = pfkey_send_spddelete(s, (struct sockaddr *)&remote_net, remote_prefix, (struct sockaddr *)&local_net, local_prefix, protocol, policy_in, policylen_in, seq++);
			if (err < 0)
				FAIL("cannot delete policy in");
		}
			
		free(policy_in);
		free(policy_out);
		policy_in = 0;
		policy_out = 0;

	}

	pfkey_close(s);
	return 0;

fail:
	if (policy_in)
		free(policy_in);
	if (policy_out)
		free(policy_out);
	if (s != -1)
		pfkey_close(s);
    return -1;
}


/* -----------------------------------------------------------------------------
Remove security associations.
This will not unconfigure IKE or remove policies and must be done separately.

Parameters:
src: source address of the security association to remove.
dst: destination address of the security association to remove.

Return code:
0 if successful, pf_key error otherwise.
----------------------------------------------------------------------------- */
int 
IPSecRemoveSecurityAssociations(struct sockaddr *src, struct sockaddr *dst) 
{
    int 	s, err;

    s = pfkey_open();
    if (s < 0)
        return -1;
        
    err = pfkey_send_delete_all(s, SADB_SATYPE_ESP, IPSEC_MODE_ANY, src, dst);
    if (err < 0)
        goto end;

    err = pfkey_send_delete_all(s, SADB_SATYPE_ESP, IPSEC_MODE_ANY, dst, src);
    if (err < 0)
        goto end;
    
end:
    pfkey_close(s);
    return err;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
#define ROUNDUP(a, size) \
	(((a) & ((size)-1)) ? (1 + ((a) | ((size)-1))) : (a))

#define NEXT_SA(ap) (ap) = (struct sockaddr *) \
	((caddr_t)(ap) + ((ap)->sa_len ? ROUNDUP((ap)->sa_len,\
						 sizeof(u_int32_t)) :\
						 sizeof(u_int32_t)))

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
static void
get_rtaddrs(int addrs, struct sockaddr *sa, struct sockaddr **rti_info)
{
    int             i;

    for (i = 0; i < RTAX_MAX; i++) {
        if (addrs & (1 << i)) {
            rti_info[i] = sa;
            NEXT_SA(sa);
            addrs ^= (1 << i);
        } else
            rti_info[i] = NULL;
    }
}


#define BUFLEN (sizeof(struct rt_msghdr) + 512)	/* 8 * sizeof(struct sockaddr_in6) = 192 */

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
void
sockaddr_to_string(const struct sockaddr *address, char *buf, size_t bufLen)
{
    bzero(buf, bufLen);
    switch (address->sa_family) {
        case AF_INET :
            (void)inet_ntop(((struct sockaddr_in *)address)->sin_family,
                            &((struct sockaddr_in *)address)->sin_addr,
                            buf,
                            bufLen);
            break;
        case AF_INET6 : {
            (void)inet_ntop(((struct sockaddr_in6 *)address)->sin6_family,
                            &((struct sockaddr_in6 *)address)->sin6_addr,
                            buf,
                            bufLen);
            if (((struct sockaddr_in6 *)address)->sin6_scope_id) {
                int	n;

                n = strlen(buf);
                if ((n+IF_NAMESIZE+1) <= bufLen) {
                    buf[n++] = '%';
                    if_indextoname(((struct sockaddr_in6 *)address)->sin6_scope_id, &buf[n]);
                }
            }
            break;
        }
        case AF_LINK :
            if (((struct sockaddr_dl *)address)->sdl_len < bufLen) {
                bufLen = ((struct sockaddr_dl *)address)->sdl_len;
            } else {
                bufLen = bufLen - 1;
            }

            bcopy(((struct sockaddr_dl *)address)->sdl_data, buf, bufLen);
            break;
        default :
            snprintf(buf, bufLen, "unexpected address family %d", address->sa_family);
            break;
    }
}

/* -----------------------------------------------------------------------------
For a given destination address, get the source address and interface
that will be used to send traffic.

Parameters:
src: source address we want to know.
dst: destination address we will talk to.
if_name: interface that will be used.

Return code:
0 if successful, -1 otherwise.
----------------------------------------------------------------------------- */
int
get_src_address(struct sockaddr *src, const struct sockaddr *dst, char *if_name)
{
    char		buf[BUFLEN];
    struct rt_msghdr	*rtm;
    pid_t		pid = getpid();
    int			rsock = -1, seq = 0, n;
    struct sockaddr	*rti_info[RTAX_MAX], *sa;
    struct sockaddr_dl	*sdl;

    rsock = socket(PF_ROUTE, SOCK_RAW, 0);
    if (rsock == -1)
        return -1;

    bzero(&buf, sizeof(buf));

    rtm = (struct rt_msghdr *)&buf;
    rtm->rtm_msglen  = sizeof(struct rt_msghdr);
    rtm->rtm_version = RTM_VERSION;
    rtm->rtm_type    = RTM_GET;
    rtm->rtm_flags   = RTF_STATIC|RTF_UP|RTF_HOST|RTF_GATEWAY;
    rtm->rtm_addrs   = RTA_DST|RTA_IFP; /* Both destination and device */
    rtm->rtm_pid     = pid;
    rtm->rtm_seq     = ++seq;

    sa = (struct sockaddr *) (rtm + 1);
    bcopy(dst, sa, dst->sa_len);
    rtm->rtm_msglen += sa->sa_len;

    sdl = (struct sockaddr_dl *) ((void *)sa + sa->sa_len);
    sdl->sdl_family = AF_LINK;
    sdl->sdl_len = sizeof (struct sockaddr_dl);
    rtm->rtm_msglen += sdl->sdl_len;

    do {
        n = write(rsock, &buf, rtm->rtm_msglen);
        if (n == -1 && errno != EINTR) {
            close(rsock);
            return -1;
        }
    } while (n == -1); 

    /* Type, seq, pid identify our response.
        Routing sockets are broadcasters on input. */
    do {
        do {
            n = read(rsock, (void *)&buf, sizeof(buf));
            if (n == -1 && errno != EINTR) {
                close(rsock);
                return -1;
            }
        } while (n == -1); 
    } while (rtm->rtm_type != RTM_GET 
            || rtm->rtm_seq != seq
            || rtm->rtm_pid != pid);

    get_rtaddrs(rtm->rtm_addrs, sa, rti_info);

#if 0
{ /* DEBUG */
    int 	i;
    char	buf[200];

    //SCLog(gSCNCDebug, LOG_DEBUG, CFSTR("rtm_flags = 0x%8.8x"), rtm->rtm_flags);

    for (i=0; i<RTAX_MAX; i++) {
        if (rti_info[i] != NULL) {
                sockaddr_to_string(rti_info[i], buf, sizeof(buf));
                printf("%d: %s\n", i, buf);
        }
    }
} /* DEBUG */
#endif
    
    bcopy(rti_info[5], src, rti_info[5]->sa_len);
    if (if_name)
        strncpy(if_name, ((struct sockaddr_dl *)rti_info[4])->sdl_data, IF_NAMESIZE);

    close(rsock);
    return 0;
}

/* -----------------------------------------------------------------------------
Get the mtu of an interface.

Parameters:
if_name: interface we want information about.

Return code:
mtu for the interface.
----------------------------------------------------------------------------- */
u_int32_t 
get_if_mtu(char *if_name)
{
	struct ifreq ifr;
	int s, err;

    ifr.ifr_mtu = 1500;

    s = socket(AF_INET, SOCK_DGRAM, 0);
    if (s >= 0) {
		strlcpy(ifr.ifr_name, if_name, sizeof (ifr.ifr_name));
		if ((err = ioctl(s, SIOCGIFMTU, (caddr_t) &ifr)) < 0)
			;
		close(s);
	}
	return ifr.ifr_mtu;
}

/* -----------------------------------------------------------------------------
 Get the media of an interface.
 
 Parameters:
 if_name: interface we want information about.
 
 Return code:
 media for the interface.
 ----------------------------------------------------------------------------- */
u_int32_t 
get_if_media(char *if_name)
{
	struct ifmediareq ifr;
	int s, err;
    
    bzero(&ifr, sizeof(ifr));

    s = socket(AF_INET, SOCK_DGRAM, 0);
    if (s >= 0) {
		strlcpy(ifr.ifm_name, if_name, sizeof (ifr.ifm_name));
		if ((err = ioctl(s, SIOCGIFMEDIA, (caddr_t) &ifr)) < 0) {
//			syslog(LOG_ERR, "can't get interface '%s' media, err %d", if_name, err);
        }
		close(s);
	}
	return ifr.ifm_current;
}

/* -----------------------------------------------------------------------------
Get the baudrate of an interface.

Parameters:
if_name: interface we want information about.

Return code:
baudrate for the interface.
----------------------------------------------------------------------------- */
u_int32_t 
get_if_baudrate(char *if_name)
{
    char *                  buf     = NULL;
    size_t                  buf_len = 0;
    struct if_msghdr *      ifm;
    unsigned int            if_index;
    u_int32_t               baudrate = 0;
    int                     mib[6];

    /* get the interface index */

    if_index = if_nametoindex(if_name);
    if (if_index == 0) {
        goto done;      // if unknown interface
    }

    /* get information for the specified device */

    mib[0] = CTL_NET;
    mib[1] = PF_ROUTE;
    mib[2] = 0;
    mib[3] = AF_LINK;
    mib[4] = NET_RT_IFLIST;
    mib[5] = if_index;      /* ask for exactly one interface */

    if (sysctl(mib, 6, NULL, &buf_len, NULL, 0) < 0) {
        goto done;
    }
    buf = malloc(buf_len);
    if (sysctl(mib, 6, buf, &buf_len, NULL, 0) < 0) {
        goto done;
    }

    /* get the baudrate for the interface */

    ifm = (struct if_msghdr *)buf;
    switch (ifm->ifm_type) {
        case RTM_IFINFO : {
            baudrate = ifm->ifm_data.ifi_baudrate;
            break;
        }
    }

done :

    if (buf != NULL)
        free(buf);

    return baudrate;
}

/* ----------------------------------------------------------------------------- 
Set the preference for security association, between "Prefer old" or "Prefer new".
The kernel is compiled to prefer old associations, which means that if a second
association with the same addresses is created while the first one is not expired, 
then the second one will not be used for outgoing traffic.
When we change to prefer the new one, the old one is not used anymore for outgoing 
traffic.
KAME/BSD prefers the old security association.
This does not work for VPN servers where we need to always use the most recent 
association as a client can connect, then disconnect without the server noticing, 
and reconnect again, creating a new association but with the old one still around.

Parameters:
oldval: old value returned, to restore later if need.
newval: new value to set. 0 to prefer old, 1 to prefer new. 

Return code:
errno
----------------------------------------------------------------------------- */
int IPSecSetSecurityAssociationsPreference(int *oldval, int newval)
{
    size_t len = sizeof(int); 
    
    if (newval != 0 && newval != 1)
        return 0;	// ignore the command
    
    return sysctlbyname("net.key.prefered_oldsa", oldval, &len, &newval, sizeof(int));
}


/* ----------------------------------------------------------------------------- 
Create a default configuration for the L2TP protocol.
L2PT has a defined set of parameters for IPSec, and this function will create a 
dictionary with all necessary keys.
The call will need to complete or adjust the configuration with specific keys
like authentications keys, or with more specific parameters.

Parameters:
src: source address of the configuration.
dst: destination address of the configuration.
authenticationMethod: SharedSecret or Certificate. 
isClient: 1 if client, 0 if server.
	configuration varies slightly depending on client or server mode.
	
Return code:
the configuration dictionary
----------------------------------------------------------------------------- */
CFMutableDictionaryRef 
IPSecCreateL2TPDefaultConfiguration(struct sockaddr *src, struct sockaddr *dst, char *dst_hostName, CFStringRef authenticationMethod, 
		int isClient, int natt_multiple_users, CFStringRef identifierVerification) 
{
	CFStringRef				src_string, dst_string, hostname_string = NULL;
	CFMutableDictionaryRef	ipsec_dict, policy0, policy1 = NULL;
	CFMutableArrayRef		policy_array, encryption_array, hash_array;
#if 0
	CFMutableDictionaryRef	proposal_dict;
	CFMutableArrayRef		proposal_array;
#endif
	CFNumberRef				src_port_num, dst_port_num, dst_port1_num, proto_num, natt_multiuser_mode = NULL;
	int						zero = 0, one = 1, udpproto = IPPROTO_UDP, val;
	struct sockaddr_in		*our_address = (struct sockaddr_in *)src;
	struct sockaddr_in		*peer_address = (struct sockaddr_in *)dst;
		
	/* create the main ipsec dictionary */
	ipsec_dict = CFDictionaryCreateMutable(0, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);

	if (dst_hostName)
		hostname_string = CFStringCreateWithCString(0, dst_hostName, kCFStringEncodingASCII);
	src_string = CFStringCreateWithCString(0, addr2ascii(AF_INET, &our_address->sin_addr, sizeof(our_address->sin_addr), 0), kCFStringEncodingASCII);
	dst_string = CFStringCreateWithCString(0, addr2ascii(AF_INET, &peer_address->sin_addr, sizeof(peer_address->sin_addr), 0), kCFStringEncodingASCII);
	val = ntohs(our_address->sin_port); /* because there is no uint16 type */
	src_port_num = CFNumberCreate(0, kCFNumberIntType, &val);
	val = ntohs(peer_address->sin_port); /* because there is no uint16 type */
	dst_port_num = CFNumberCreate(0, kCFNumberIntType, &val);
	dst_port1_num = CFNumberCreate(0, kCFNumberIntType, &zero);
	proto_num = CFNumberCreate(0, kCFNumberIntType, &udpproto);
	if (!isClient)
		natt_multiuser_mode = CFNumberCreate(0, kCFNumberIntType, natt_multiple_users ? &one : &zero);
	

	CFDictionarySetValue(ipsec_dict, kRASPropIPSecAuthenticationMethod, authenticationMethod);
	CFDictionarySetValue(ipsec_dict, kRASPropIPSecLocalAddress, src_string);
	CFDictionarySetValue(ipsec_dict, kRASPropIPSecRemoteAddress, dst_string);
	CFDictionarySetValue(ipsec_dict, kRASPropIPSecProposalsBehavior, isClient ? kRASValIPSecProposalsBehaviorObey : kRASValIPSecProposalsBehaviorClaim);
	if (isClient && CFEqual(authenticationMethod, kRASValIPSecAuthenticationMethodCertificate)) {
		if (identifierVerification) {
			CFDictionarySetValue(ipsec_dict, kRASPropIPSecIdentifierVerification, identifierVerification);
		}
		else {
			if (dst_hostName) {
				CFDictionarySetValue(ipsec_dict, kRASPropIPSecRemoteIdentifier, hostname_string);
				CFDictionarySetValue(ipsec_dict, kRASPropIPSecIdentifierVerification, kRASValIPSecIdentifierVerificationUseRemoteIdentifier);
			} else
				CFDictionarySetValue(ipsec_dict, kRASPropIPSecIdentifierVerification, kRASValIPSecIdentifierVerificationGenerateFromRemoteAddress);
		}
	} else /*server or no certificate */
		CFDictionarySetValue(ipsec_dict, kRASPropIPSecIdentifierVerification, kRASValIPSecIdentifierVerificationNone);
		
	/* if server - set natt multiple user mode */
	if (!isClient)
		CFDictionarySetValue(ipsec_dict, kRASPropIPSecNattMultipleUsersEnabled, natt_multiuser_mode);
	
	/* create the phase 1 proposals */
#if 0
	proposal_dict = CFDictionaryCreateMutable(0, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
	CFDictionarySetValue(proposal_dict, kRASPropIPSecAuthenticationMethod, authenticationMethod);

	proposal_array = CFArrayCreateMutable(0, 0, &kCFTypeArrayCallBacks);
	CFArraySetValueAtIndex(proposal_array, 0, proposal_dict);
	CFRelease(proposal_dict);

	CFDictionarySetValue(ipsec_dict, kRASPropIPSecProposals, proposal_array);
	CFRelease(proposal_array);
#endif
	
	/* create the policies */
	policy0 = CFDictionaryCreateMutable(0, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
	CFDictionarySetValue(policy0, kRASPropIPSecPolicyLocalPort, src_port_num);
	CFDictionarySetValue(policy0, kRASPropIPSecPolicyRemotePort, dst_port_num);
	CFDictionarySetValue(policy0, kRASPropIPSecPolicyProtocol, proto_num);
	CFDictionarySetValue(policy0, kRASPropIPSecPolicyMode, kRASValIPSecPolicyModeTransport);
	CFDictionarySetValue(policy0, kRASPropIPSecPolicyLevel, kRASValIPSecPolicyLevelRequire);
	encryption_array = CFArrayCreateMutable(0, 0, &kCFTypeArrayCallBacks);
	CFArraySetValueAtIndex(encryption_array, 0, kRASValIPSecPolicyEncryptionAlgorithmAES);
	CFArraySetValueAtIndex(encryption_array, 1, kRASValIPSecPolicyEncryptionAlgorithm3DES);
	CFDictionarySetValue(policy0, kRASPropIPSecPolicyEncryptionAlgorithm, encryption_array);
	CFRelease(encryption_array);
	hash_array = CFArrayCreateMutable(0, 0, &kCFTypeArrayCallBacks);
	CFArraySetValueAtIndex(hash_array, 0, kRASValIPSecPolicyHashAlgorithmSHA1);
	CFArraySetValueAtIndex(hash_array, 1, kRASValIPSecPolicyHashAlgorithmMD5);
	CFDictionarySetValue(policy0, kRASPropIPSecPolicyHashAlgorithm, hash_array);
	CFRelease(hash_array);

	if (isClient) {
		policy1 = CFDictionaryCreateMutableCopy(0, 0, policy0);
		CFDictionarySetValue(policy1, kRASPropIPSecPolicyRemotePort, dst_port1_num);
		CFDictionarySetValue(policy1, kRASPropIPSecPolicyDirection, kRASValIPSecPolicyDirectionIn);
	}
		
	policy_array = CFArrayCreateMutable(0, 0, &kCFTypeArrayCallBacks);
	CFArraySetValueAtIndex(policy_array, 0, policy0);
	if (isClient)
		CFArraySetValueAtIndex(policy_array, 1, policy1);

	CFRelease(policy0);
	if (isClient)
		CFRelease(policy1);
	
	CFDictionarySetValue(ipsec_dict, kRASPropIPSecPolicies, policy_array);
	CFRelease(policy_array);
	
	CFRelease(src_string);
	CFRelease(dst_string);
	CFRelease(src_port_num);
	CFRelease(dst_port_num);
	CFRelease(proto_num);
	if (!isClient)
		CFRelease(natt_multiuser_mode);
	if (hostname_string)
		CFRelease(hostname_string);

	return ipsec_dict;
}

/* ----------------------------------------------------------------------------- 
Create a default configuration for the Cisco Server connection.
L2PT has a defined set of parameters for IPSec, and this function will create a 
dictionary with all necessary keys.
The call will need to complete or adjust the configuration with specific keys
like authentications keys, or with more specific parameters.

Parameters:
src: source address of the configuration.
dst: destination address of the configuration.
authenticationMethod: SharedSecret or Certificate. 
isClient: 1 if client, 0 if server.
	configuration varies slightly depending on client or server mode.
	
Return code:
the configuration dictionary
----------------------------------------------------------------------------- */
CFMutableDictionaryRef 
IPSecCreateCiscoDefaultConfiguration(struct sockaddr *src, struct sockaddr *dst, CFStringRef dst_hostName, CFStringRef authenticationMethod, 
		int isClient, int natt_multiple_users, CFStringRef identifierVerification) 
{
	CFStringRef				src_string, dst_string;
	CFMutableDictionaryRef	ipsec_dict, proposal_dict;
	CFMutableArrayRef		proposal_array;
#if 0
	CFMutableDictionaryRef	policy0, policy1 = NULL;
	CFMutableArrayRef		policy_array,  encryption_array, hash_array;
#endif
	CFNumberRef				src_port_num, dst_port_num, dst_port1_num, proto_num, natt_multiuser_mode = NULL, cfone = NULL, cfzero = NULL, dhgroup, lifetime, dpd_delay;
	int						zero = 0, one = 1, anyproto = -1, val, nb;
	struct sockaddr_in		*our_address = (struct sockaddr_in *)src;
	struct sockaddr_in		*peer_address = (struct sockaddr_in *)dst;

	// remove as soon as we add dh_group 5 to the loop
#define ADD_DHGROUP5 1
#ifdef ADD_DHGROUP5
	int		add_group5 = 0, val5 = 5;
	CFNumberRef	dhgroup5 = NULL;
#endif
	
	/* create the main ipsec dictionary */
	ipsec_dict = CFDictionaryCreateMutable(0, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);

	src_string = CFStringCreateWithCString(0, addr2ascii(AF_INET, &our_address->sin_addr, sizeof(our_address->sin_addr), 0), kCFStringEncodingASCII);
	dst_string = CFStringCreateWithCString(0, addr2ascii(AF_INET, &peer_address->sin_addr, sizeof(peer_address->sin_addr), 0), kCFStringEncodingASCII);
	val = ntohs(our_address->sin_port); /* because there is no uint16 type */
	src_port_num = CFNumberCreate(0, kCFNumberIntType, &val);
	val = ntohs(peer_address->sin_port); /* because there is no uint16 type */
	dst_port_num = CFNumberCreate(0, kCFNumberIntType, &val);
	dst_port1_num = CFNumberCreate(0, kCFNumberIntType, &zero);
	proto_num = CFNumberCreate(0, kCFNumberIntType, &anyproto);
	if (!isClient)
		natt_multiuser_mode = CFNumberCreate(0, kCFNumberIntType, natt_multiple_users ? &one : &zero);
	
	cfzero = CFNumberCreate(0, kCFNumberIntType, &zero);
	cfone = CFNumberCreate(0, kCFNumberIntType, &one);
	CFDictionarySetValue(ipsec_dict, kRASPropIPSecModeConfigEnabled, cfone);
	CFDictionarySetValue(ipsec_dict, kRASPropIPSecXAuthEnabled, cfone);
	
	CFDictionarySetValue(ipsec_dict, kRASPropIPSecLocalAddress, src_string);
	CFDictionarySetValue(ipsec_dict, kRASPropIPSecRemoteAddress, dst_string);
	CFDictionarySetValue(ipsec_dict, kRASPropIPSecProposalsBehavior, isClient ? kRASValIPSecProposalsBehaviorObey : kRASValIPSecProposalsBehaviorClaim);
	if (isClient && CFEqual(authenticationMethod, kRASValIPSecAuthenticationMethodCertificate)) {
#ifdef ADD_DHGROUP5
		add_group5 = 1;
#endif
		if (identifierVerification) {
			CFDictionarySetValue(ipsec_dict, kRASPropIPSecIdentifierVerification, identifierVerification);
		}
		else {
			if (dst_hostName) {
				CFDictionarySetValue(ipsec_dict, kRASPropIPSecRemoteIdentifier, dst_hostName);
				CFDictionarySetValue(ipsec_dict, kRASPropIPSecIdentifierVerification, kRASValIPSecIdentifierVerificationUseRemoteIdentifier);
			} else
				CFDictionarySetValue(ipsec_dict, kRASPropIPSecIdentifierVerification, kRASValIPSecIdentifierVerificationGenerateFromRemoteAddress);
		}
	} else /*server or no certificate */
		CFDictionarySetValue(ipsec_dict, kRASPropIPSecIdentifierVerification, kRASValIPSecIdentifierVerificationNone);
		
	/* if server - set natt multiple user mode */
	if (!isClient)
		CFDictionarySetValue(ipsec_dict, kRASPropIPSecNattMultipleUsersEnabled, natt_multiuser_mode);

	/* use dead peer detection blackhole detection */
	CFDictionarySetValue(ipsec_dict, kRASPropIPSecDeadPeerDetectionEnabled, cfone);
	CFDictionarySetValue(ipsec_dict, kRASPropIPSecBlackHoleDetectionEnabled, cfone);
	val = 20; // 20 seconds dpd with blackhole detection  
	dpd_delay = CFNumberCreate(0, kCFNumberIntType, &val);
	if (dpd_delay) {
		CFDictionarySetValue(ipsec_dict, kRASPropIPSecDeadPeerDetectionDelay, dpd_delay);
		CFRelease(dpd_delay);
	}

	/* By default, disable keep alive and let DPD play that role 
	 20 seconds default keepalive kills battery. */
	//	CFDictionarySetValue(ipsec_dict, kRASPropIPSecNattKeepAliveEnabled, cfzero); 
	
	/* create the phase 1 proposals */
#if 1
	//Create all combination of proposals
	proposal_array = CFArrayCreateMutable(0, 0, &kCFTypeArrayCallBacks);

	val = 3600; 
	lifetime = CFNumberCreate(0, kCFNumberIntType, &val);

	nb = 0;
	
	/* should support group 1, 2, and 5 for main monde and only group 2 for aggressive mode
		supporting so many proposals could require to turn on ike fragmentation and needs more testing */
	//int i, dh_group[] = { 1, 2, 5, 0 };
	int i, dh_group[] = { 2, 0 }; 
	
	for (i = 0; val = dh_group[i]; i++) {	

		dhgroup = CFNumberCreate(0, kCFNumberIntType, &val);

#ifdef ADD_DHGROUP5
		if (add_group5)
				dhgroup5 = CFNumberCreate(0, kCFNumberIntType, &val5);
#endif
		
		// --- AES256/SHA1
#ifdef ADD_DHGROUP5
		if (add_group5) {
			proposal_dict = CFDictionaryCreateMutable(0, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
			CFDictionarySetValue(proposal_dict, kRASPropIPSecProposalEncryptionAlgorithm, kRASValIPSecProposalEncryptionAlgorithmAES256);
			CFDictionarySetValue(proposal_dict, kRASPropIPSecProposalHashAlgorithm, kRASValIPSecProposalHashAlgorithmSHA1);
			CFDictionarySetValue(proposal_dict, kRASPropIPSecProposalDHGroup, dhgroup5);
			CFDictionarySetValue(proposal_dict, kRASPropIPSecProposalLifetime, lifetime);
			CFArraySetValueAtIndex(proposal_array, nb++, proposal_dict);
			CFRelease(proposal_dict);
		}
#endif
		
		proposal_dict = CFDictionaryCreateMutable(0, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);		
		CFDictionarySetValue(proposal_dict, kRASPropIPSecProposalEncryptionAlgorithm, kRASValIPSecProposalEncryptionAlgorithmAES256);
		CFDictionarySetValue(proposal_dict, kRASPropIPSecProposalHashAlgorithm, kRASValIPSecProposalHashAlgorithmSHA1);
		CFDictionarySetValue(proposal_dict, kRASPropIPSecProposalDHGroup, dhgroup);
		CFDictionarySetValue(proposal_dict, kRASPropIPSecProposalLifetime, lifetime);
		CFArraySetValueAtIndex(proposal_array, nb++, proposal_dict);
		CFRelease(proposal_dict);

		// --- AES/SHA1
		proposal_dict = CFDictionaryCreateMutable(0, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
		CFDictionarySetValue(proposal_dict, kRASPropIPSecProposalEncryptionAlgorithm, kRASValIPSecProposalEncryptionAlgorithmAES);
		CFDictionarySetValue(proposal_dict, kRASPropIPSecProposalHashAlgorithm, kRASValIPSecProposalHashAlgorithmSHA1);
		CFDictionarySetValue(proposal_dict, kRASPropIPSecProposalDHGroup, dhgroup);
		CFDictionarySetValue(proposal_dict, kRASPropIPSecProposalLifetime, lifetime);
		CFArraySetValueAtIndex(proposal_array, nb++, proposal_dict);
		CFRelease(proposal_dict);

		// --- AES256/MD5
#ifdef ADD_DHGROUP5
		if (add_group5) {
			proposal_dict = CFDictionaryCreateMutable(0, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
			CFDictionarySetValue(proposal_dict, kRASPropIPSecProposalEncryptionAlgorithm, kRASValIPSecProposalEncryptionAlgorithmAES256);
			CFDictionarySetValue(proposal_dict, kRASPropIPSecProposalHashAlgorithm, kRASValIPSecProposalHashAlgorithmMD5);
			CFDictionarySetValue(proposal_dict, kRASPropIPSecProposalDHGroup, dhgroup5);
			CFDictionarySetValue(proposal_dict, kRASPropIPSecProposalLifetime, lifetime);
			CFArraySetValueAtIndex(proposal_array, nb++, proposal_dict);
			CFRelease(proposal_dict);
		}
#endif
		
		proposal_dict = CFDictionaryCreateMutable(0, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
		CFDictionarySetValue(proposal_dict, kRASPropIPSecProposalEncryptionAlgorithm, kRASValIPSecProposalEncryptionAlgorithmAES256);
		CFDictionarySetValue(proposal_dict, kRASPropIPSecProposalHashAlgorithm, kRASValIPSecProposalHashAlgorithmMD5);
		CFDictionarySetValue(proposal_dict, kRASPropIPSecProposalDHGroup, dhgroup);
		CFDictionarySetValue(proposal_dict, kRASPropIPSecProposalLifetime, lifetime);
		CFArraySetValueAtIndex(proposal_array, nb++, proposal_dict);
		CFRelease(proposal_dict);

		// --- AES/MD5
		proposal_dict = CFDictionaryCreateMutable(0, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
		CFDictionarySetValue(proposal_dict, kRASPropIPSecProposalEncryptionAlgorithm, kRASValIPSecProposalEncryptionAlgorithmAES);
		CFDictionarySetValue(proposal_dict, kRASPropIPSecProposalHashAlgorithm, kRASValIPSecProposalHashAlgorithmMD5);
		CFDictionarySetValue(proposal_dict, kRASPropIPSecProposalDHGroup, dhgroup);
		CFDictionarySetValue(proposal_dict, kRASPropIPSecProposalLifetime, lifetime);
		CFArraySetValueAtIndex(proposal_array, nb++, proposal_dict);
		CFRelease(proposal_dict);

		// --- 3DES/SHA1
		proposal_dict = CFDictionaryCreateMutable(0, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
		CFDictionarySetValue(proposal_dict, kRASPropIPSecProposalEncryptionAlgorithm, kRASValIPSecProposalEncryptionAlgorithm3DES);
		CFDictionarySetValue(proposal_dict, kRASPropIPSecProposalHashAlgorithm, kRASValIPSecProposalHashAlgorithmSHA1);
		CFDictionarySetValue(proposal_dict, kRASPropIPSecProposalDHGroup, dhgroup);
		CFDictionarySetValue(proposal_dict, kRASPropIPSecProposalLifetime, lifetime);
		CFArraySetValueAtIndex(proposal_array, nb++, proposal_dict);
		CFRelease(proposal_dict);

		// --- 3DES/MD5
		proposal_dict = CFDictionaryCreateMutable(0, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
		CFDictionarySetValue(proposal_dict, kRASPropIPSecProposalEncryptionAlgorithm, kRASValIPSecProposalEncryptionAlgorithm3DES);
		CFDictionarySetValue(proposal_dict, kRASPropIPSecProposalHashAlgorithm, kRASValIPSecProposalHashAlgorithmMD5);
		CFDictionarySetValue(proposal_dict, kRASPropIPSecProposalDHGroup, dhgroup);
		CFDictionarySetValue(proposal_dict, kRASPropIPSecProposalLifetime, lifetime);
		CFArraySetValueAtIndex(proposal_array, nb++, proposal_dict);
		CFRelease(proposal_dict);

		// --- DES/SHA1
		proposal_dict = CFDictionaryCreateMutable(0, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
		CFDictionarySetValue(proposal_dict, kRASPropIPSecProposalEncryptionAlgorithm, kRASValIPSecProposalEncryptionAlgorithmDES);
		CFDictionarySetValue(proposal_dict, kRASPropIPSecProposalHashAlgorithm, kRASValIPSecProposalHashAlgorithmSHA1);
		CFDictionarySetValue(proposal_dict, kRASPropIPSecProposalDHGroup, dhgroup);
		CFDictionarySetValue(proposal_dict, kRASPropIPSecProposalLifetime, lifetime);
		CFArraySetValueAtIndex(proposal_array, nb++, proposal_dict);
		CFRelease(proposal_dict);

		// --- DES/MD5
		proposal_dict = CFDictionaryCreateMutable(0, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
		CFDictionarySetValue(proposal_dict, kRASPropIPSecProposalEncryptionAlgorithm, kRASValIPSecProposalEncryptionAlgorithmDES);
		CFDictionarySetValue(proposal_dict, kRASPropIPSecProposalHashAlgorithm, kRASValIPSecProposalHashAlgorithmMD5);
		CFDictionarySetValue(proposal_dict, kRASPropIPSecProposalDHGroup, dhgroup);
		CFDictionarySetValue(proposal_dict, kRASPropIPSecProposalLifetime, lifetime);
		CFArraySetValueAtIndex(proposal_array, nb++, proposal_dict);
		CFRelease(proposal_dict);

		// ---
		
		CFRelease(dhgroup);
#ifdef ADD_DHGROUP5
		if (add_group5)
			CFRelease(dhgroup5);
#endif
	}
	
	CFRelease(lifetime);

	CFDictionarySetValue(ipsec_dict, kRASPropIPSecProposals, proposal_array);
	CFRelease(proposal_array);

#endif
	
	/* create the policies */
#if 0
	policy0 = CFDictionaryCreateMutable(0, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
	CFDictionarySetValue(policy0, kRASPropIPSecPolicyLocalPort, src_port_num);
	CFDictionarySetValue(policy0, kRASPropIPSecPolicyRemotePort, dst_port_num);
	CFDictionarySetValue(policy0, kRASPropIPSecPolicyProtocol, proto_num);
	CFDictionarySetValue(policy0, kRASPropIPSecPolicyMode, kRASValIPSecPolicyModeTransport);
	CFDictionarySetValue(policy0, kRASPropIPSecPolicyLevel, kRASValIPSecPolicyLevelRequire);
	val = 0; 
	dhgroup = CFNumberCreate(0, kCFNumberIntType, &val);
	CFDictionarySetValue(policy0, kRASPropIPSecPolicyPFSGroup, dhgroup);
	CFRelease(dhgroup);
	val = 3600; 
	lifetime = CFNumberCreate(0, kCFNumberIntType, &val);
	CFDictionarySetValue(policy0, kRASPropIPSecPolicyLifetime, lifetime);
	CFRelease(lifetime);
	encryption_array = CFArrayCreateMutable(0, 0, &kCFTypeArrayCallBacks);
	CFArraySetValueAtIndex(encryption_array, 0, kRASValIPSecPolicyEncryptionAlgorithmAES);
	CFArraySetValueAtIndex(encryption_array, 1, kRASValIPSecPolicyEncryptionAlgorithm3DES);
	CFDictionarySetValue(policy0, kRASPropIPSecPolicyEncryptionAlgorithm, encryption_array);
	CFRelease(encryption_array);
	hash_array = CFArrayCreateMutable(0, 0, &kCFTypeArrayCallBacks);
	CFArraySetValueAtIndex(hash_array, 0, kRASValIPSecPolicyHashAlgorithmSHA1);
	CFArraySetValueAtIndex(hash_array, 1, kRASValIPSecPolicyHashAlgorithmMD5);
	CFDictionarySetValue(policy0, kRASPropIPSecPolicyHashAlgorithm, hash_array);
	CFRelease(hash_array);

	if (isClient) {
		policy1 = CFDictionaryCreateMutableCopy(0, 0, policy0);
		CFDictionarySetValue(policy1, kRASPropIPSecPolicyRemotePort, dst_port1_num);
		CFDictionarySetValue(policy1, kRASPropIPSecPolicyDirection, kRASValIPSecPolicyDirectionIn);
	}
		
	policy_array = CFArrayCreateMutable(0, 0, &kCFTypeArrayCallBacks);
	CFArraySetValueAtIndex(policy_array, 0, policy0);
	if (isClient)
		CFArraySetValueAtIndex(policy_array, 1, policy1);

	CFRelease(policy0);
	if (isClient)
		CFRelease(policy1);
	
	CFDictionarySetValue(ipsec_dict, kRASPropIPSecPolicies, policy_array);
	CFRelease(policy_array);
#endif
#if 0
/* XXX Hack iPhone create sainfo anonymous until we fix racoon. In this specific case, mode must be "transport" */
	policy0 = CFDictionaryCreateMutable(0, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
	/* XXX */
	CFDictionarySetValue(ipsec_dict, CFSTR("UseAnonymousPolicy"), CFSTR("UseAnonymousPolicy"));
	CFDictionarySetValue(policy0, kRASPropIPSecPolicyLocalPort, src_port_num);
	CFDictionarySetValue(policy0, kRASPropIPSecPolicyRemotePort, dst_port_num);
	CFDictionarySetValue(policy0, kRASPropIPSecPolicyProtocol, proto_num);
	CFDictionarySetValue(policy0, kRASPropIPSecPolicyMode, kRASValIPSecPolicyModeTransport);
	CFDictionarySetValue(policy0, kRASPropIPSecPolicyLevel, kRASValIPSecPolicyLevelRequire);
	val = 0; 
	dhgroup = CFNumberCreate(0, kCFNumberIntType, &val);
	CFDictionarySetValue(policy0, kRASPropIPSecPolicyPFSGroup, dhgroup);
	CFRelease(dhgroup);
	val = 3600; 
	lifetime = CFNumberCreate(0, kCFNumberIntType, &val);
	CFDictionarySetValue(policy0, kRASPropIPSecPolicyLifetime, lifetime);
	CFRelease(lifetime);
	encryption_array = CFArrayCreateMutable(0, 0, &kCFTypeArrayCallBacks);
	CFArraySetValueAtIndex(encryption_array, 0, kRASValIPSecPolicyEncryptionAlgorithmAES);
	CFArraySetValueAtIndex(encryption_array, 1, kRASValIPSecPolicyEncryptionAlgorithm3DES);
	CFDictionarySetValue(policy0, kRASPropIPSecPolicyEncryptionAlgorithm, encryption_array);
	CFRelease(encryption_array);
	hash_array = CFArrayCreateMutable(0, 0, &kCFTypeArrayCallBacks);
	CFArraySetValueAtIndex(hash_array, 0, kRASValIPSecPolicyHashAlgorithmSHA1);
	CFArraySetValueAtIndex(hash_array, 1, kRASValIPSecPolicyHashAlgorithmMD5);
	CFDictionarySetValue(policy0, kRASPropIPSecPolicyHashAlgorithm, hash_array);
	CFRelease(hash_array);

	if (isClient) {
		policy1 = CFDictionaryCreateMutableCopy(0, 0, policy0);
		CFDictionarySetValue(policy1, kRASPropIPSecPolicyRemotePort, dst_port1_num);
		CFDictionarySetValue(policy1, kRASPropIPSecPolicyDirection, kRASValIPSecPolicyDirectionIn);
	}
		
	policy_array = CFArrayCreateMutable(0, 0, &kCFTypeArrayCallBacks);
	CFArraySetValueAtIndex(policy_array, 0, policy0);
	if (isClient)
		CFArraySetValueAtIndex(policy_array, 1, policy1);

	CFRelease(policy0);
	if (isClient)
		CFRelease(policy1);
	
	CFDictionarySetValue(ipsec_dict, kRASPropIPSecPolicies, policy_array);
	CFRelease(policy_array);
#endif
	
	CFRelease(src_string);
	CFRelease(dst_string);
	CFRelease(src_port_num);
	CFRelease(dst_port_num);
	CFRelease(proto_num);
	if (cfone)
		CFRelease(cfone);
	if (cfzero)
		CFRelease(cfzero);
	if (!isClient)
		CFRelease(natt_multiuser_mode);

	return ipsec_dict;
}

/* -----------------------------------------------------------------------------
IPSecSelfRepair. 
This is an attempt to have IPSec try to repair itself when things
don't work anymore.
Tipically, kill and restart racoon...

Parameters:

Return code:
0 if successful, -1 otherwise.
----------------------------------------------------------------------------- */
int 
IPSecSelfRepair() 
{
	int err;
	
	racoon_stop();
		
	return 0;
}

static char unknown_if_family_str[16];
static char *
if_family2ascii (u_int32_t if_family)
{
	switch (if_family) {
		case APPLE_IF_FAM_LOOPBACK:
			return("Loopback");
		case APPLE_IF_FAM_ETHERNET:
			return("Ether");
		case APPLE_IF_FAM_SLIP:
			return("SLIP");
		case APPLE_IF_FAM_TUN:
			return("TUN");
		case APPLE_IF_FAM_VLAN:
			return("VLAN");
		case APPLE_IF_FAM_PPP:
			return("PPP");
		case APPLE_IF_FAM_PVC:
			return("PVC");
		case APPLE_IF_FAM_DISC:
			return("DISC");
		case APPLE_IF_FAM_MDECAP:
			return("MDECAP");
		case APPLE_IF_FAM_GIF:
			return("GIF");
		case APPLE_IF_FAM_FAITH:
			return("FAITH");
		case APPLE_IF_FAM_STF:
			return("STF");
		case APPLE_IF_FAM_FIREWIRE:
			return("FireWire");
		case APPLE_IF_FAM_BOND:
			return("Bond");
		default:
			snprintf(unknown_if_family_str, sizeof(unknown_if_family_str), "%d", if_family);
			return(unknown_if_family_str);
	}
}

void
IPSecLogVPNInterfaceAddressEvent (char                  *location,
								  struct kern_event_msg *ev_msg,
								  int                    wait_interface_timeout,
							 	  char                  *interface,
								  struct in_addr        *our_address)
{
	struct in_addr mask;
	char           our_addr_str[INET_ADDRSTRLEN];

	if (!ev_msg) {
		syslog(LOG_NOTICE, "%s: %d secs TIMEOUT waiting for interface to be reconfigured. previous setting (name: %s, address: %s).",
			   location,
			   wait_interface_timeout,
			   interface,
			   addr2ascii(AF_INET, our_address, sizeof(*our_address), our_addr_str));
		return;
	} else {
		struct kev_in_data      *inetdata = (struct kev_in_data *) &ev_msg->event_data[0];
		struct kev_in_collision *inetdata_coll = (struct kev_in_collision *) &ev_msg->event_data[0];
		char                     new_addr_str[INET_ADDRSTRLEN];
		char                     new_mask_str[INET_ADDRSTRLEN];
		char                     dst_addr_str[INET_ADDRSTRLEN];
		
		mask.s_addr = ntohl(inetdata->ia_netmask);
		
		switch (ev_msg->event_code) {
			case KEV_INET_NEW_ADDR:
				syslog(LOG_NOTICE, "%s: Address added. previous interface setting (name: %s, address: %s), current interface setting (name: %s%d, family: %s, address: %s, subnet: %s, destination: %s).",
					   location,
					   interface,
					   addr2ascii(AF_INET, our_address, sizeof(*our_address), our_addr_str),
					   inetdata->link_data.if_name, inetdata->link_data.if_unit,
					   if_family2ascii(inetdata->link_data.if_family),
					   addr2ascii(AF_INET, &inetdata->ia_addr, sizeof(inetdata->ia_addr), new_addr_str),
					   addr2ascii(AF_INET, &mask, sizeof(mask), new_mask_str),
					   addr2ascii(AF_INET, &inetdata->ia_dstaddr, sizeof(inetdata->ia_dstaddr), dst_addr_str));
				break;
			case KEV_INET_CHANGED_ADDR:
				syslog(LOG_NOTICE, "%s: Address changed. previous interface setting (name: %s, address: %s), current interface setting (name: %s%d, family: %s, address: %s, subnet: %s, destination: %s).",
					   location,
					   interface,
					   addr2ascii(AF_INET, our_address, sizeof(*our_address), our_addr_str),
					   inetdata->link_data.if_name, inetdata->link_data.if_unit,
					   if_family2ascii(inetdata->link_data.if_family),
					   addr2ascii(AF_INET, &inetdata->ia_addr, sizeof(inetdata->ia_addr), new_addr_str),
					   addr2ascii(AF_INET, &mask, sizeof(mask), new_mask_str),
					   addr2ascii(AF_INET, &inetdata->ia_dstaddr, sizeof(inetdata->ia_dstaddr), dst_addr_str));
				break;
			case KEV_INET_ADDR_DELETED:
				syslog(LOG_NOTICE, "%s: Address deleted. previous interface setting (name: %s, address: %s), deleted interface setting (name: %s%d, family: %s, address: %s, subnet: %s, destination: %s).",
					   location,
					   interface,
					   addr2ascii(AF_INET, our_address, sizeof(*our_address), our_addr_str),
					   inetdata->link_data.if_name, inetdata->link_data.if_unit,
					   if_family2ascii(inetdata->link_data.if_family),
					   addr2ascii(AF_INET, &inetdata->ia_addr, sizeof(inetdata->ia_addr), new_addr_str),
					   addr2ascii(AF_INET, &mask, sizeof(mask), new_mask_str),
					   addr2ascii(AF_INET, &inetdata->ia_dstaddr, sizeof(inetdata->ia_dstaddr), dst_addr_str));
				break;
			case KEV_INET_ARPCOLLISION:
				syslog(LOG_NOTICE, "%s: ARP collided. previous interface setting (name: %s, address: %s), conflicting interface setting (name: %s%d, family: %s, address: %s, mac: %x:%x:%x:%x:%x:%x).",
					   location,
					   interface,
					   addr2ascii(AF_INET, our_address, sizeof(*our_address), our_addr_str),
					   inetdata_coll->link_data.if_name,
					   inetdata_coll->link_data.if_unit,
					   if_family2ascii(inetdata_coll->link_data.if_family),
					   addr2ascii(AF_INET, &inetdata_coll->ia_ipaddr, sizeof(inetdata_coll->ia_ipaddr), new_addr_str),
					   inetdata_coll->hw_addr[5],inetdata_coll->hw_addr[4],inetdata_coll->hw_addr[3],inetdata_coll->hw_addr[2],inetdata_coll->hw_addr[1],inetdata_coll->hw_addr[0]);
				break;
			default:
				syslog(LOG_NOTICE, "%s: Unknown Address event (%d). previous interface setting (name: %s, address: %s), other interface setting (name: %s%d, family: %s, address: %s, subnet: %s, destination: %s).",
					   location,
					   ev_msg->event_code,
					   interface,
					   addr2ascii(AF_INET, our_address, sizeof(*our_address), our_addr_str),
					   inetdata->link_data.if_name, inetdata->link_data.if_unit,
					   if_family2ascii(inetdata->link_data.if_family),
					   addr2ascii(AF_INET, &inetdata->ia_addr, sizeof(inetdata->ia_addr), new_addr_str),
					   addr2ascii(AF_INET, &mask, sizeof(mask), new_mask_str),
					   addr2ascii(AF_INET, &inetdata->ia_dstaddr, sizeof(inetdata->ia_dstaddr), dst_addr_str));
				break;
		}
	}
}

int
IPSecCheckVPNInterfaceOrServiceUnrecoverable (void                  *dynamicStore,
					      char                  *location,
					      struct kern_event_msg *ev_msg,
					      char                  *interface_buf)
{
	SCDynamicStoreRef dynamicStoreRef = (SCDynamicStoreRef)dynamicStore;

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
			goto done;
		}
		// look for the service which matches the provided prefixes
		n = CFDictionaryGetCount(dict);
		if (n <= 0) {
			syslog(LOG_NOTICE, "%s: empty SCDynamicStore dictionary", location);
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

			//syslog(LOG_NOTICE, "processing key: %s \n", CFStringGetCStringPtr(s_key, kCFStringEncodingMacRoman));

			if (CFStringHasSuffix(s_key, kSCEntNetInterface)) {
				// is a Service Interface entity
				s_if = CFDictionaryGetValue(s_dict, kSCPropNetInterfaceDeviceName);
				if (isA_CFString(s_if) && CFEqual(vpn_if, s_if)) {
					CFArrayRef        components;
					CFStringRef       serviceIDRef = NULL, serviceKey = NULL;
					CFPropertyListRef serviceRef = NULL;

					syslog(LOG_NOTICE, "linked service found: %s \n", CFStringGetCStringPtr(s_key, kCFStringEncodingMacRoman));
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
					CFBooleanRef powerEnable = CFDictionaryGetValue(s_dict, kSCPropNetAirPortPowerEnabled);
					if (isA_CFBoolean(powerEnable) &&
					    CFEqual(powerEnable, kCFBooleanFalse)) {
						syslog(LOG_NOTICE, "%s: detected AirPort, PowerEnable == FALSE", location);
						interf_down = 1;
						break;
					}
				} else if (CFStringHasPrefix(s_key, kSCDynamicStoreDomainState)) {
					UInt16      temp;
					CFNumberRef airStatus = CFDictionaryGetValue(s_dict, CFSTR("Power Status"));
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

int
IPSecCheckVPNInterfaceAddressChange (int                    transport_down,
                                     struct kern_event_msg *ev_msg,
                                     char                  *interface_buf,
                                     struct in_addr        *our_address)
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
    }
    
    return 0;
}

int
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
