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
#include <sys/param.h>

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
static int racoon_start(CFBundleRef bundle, char *filename);
static int racoon_restart(int launch_if_needed);


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
	uint8_t * outp = outputData;

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
check if racoon is started with the same configuration file
returns:
    0: racoon is correctly started
    1: racoon is already started with our configuration
    -1: racoon is already started but with a different configuration
    -2: failed to start racoon
----------------------------------------------------------------------------- */
int 
racoon_start(CFBundleRef bundle, char *filename)
{
    int   	pid, err, tries;
    CFURLRef	url;
    char 	name[MAXPATHLEN]; 

    name[0] = 0;
    
    /* if a bundle and a name are passed, start racoon with the specific file */
    if (bundle) {
        if (url = CFBundleCopyBundleURL(bundle)) {
            CFURLGetFileSystemRepresentation(url, 0, name, MAXPATHLEN - 1);
            CFRelease(url);
            strcat(name, "/");
            if (url = CFBundleCopyResourcesDirectoryURL(bundle)) {
                CFURLGetFileSystemRepresentation(url, 0, name + strlen(name), 
                        MAXPATHLEN - strlen(name) - strlen(filename) - 1);
                CFRelease(url);
                strcat(name, "/");
                strcat(name, filename);
            }
        }
    
        if (name[0] == 0)
            return -2;
    }
    
    /* check first is racoon is started */
    err = racoon_is_started(name);
    if (err != 0)
        return err;            

    pid = fork();
    if (pid < 0)
        return -2;

    if (pid == 0) {

        closeall();
        
        // need to exec a tool, with complete parameters list
        if (name[0])
            execle("/usr/sbin/racoon", "racoon", "-f", name, (char *)0, (char *)0);
        else
            execle("/usr/sbin/racoon", "racoon", (char *)0, (char *)0);
            
        // child exits
        exit(0);
        /* NOTREACHED */
    }

    // parent wait for child's completion, that occurs immediatly since racoon daemonize itself
    while (waitpid(pid, &err, 0) < 0) {
        if (errno == EINTR)
            continue;
        return -2;
    }
        
    // give some time to racoon
    sleep(3);
    
    // wait for racoon pid
    tries = 5; // give 5 seconds to racoon to write its pid.
    while (!racoon_is_started(name) && tries) {
        sleep(1);
        tries--;
    }

    if (tries == 0)
        return -1;
        
    //err = (err >> 8) & 0xFF;
    return 0;
}

/* -----------------------------------------------------------------------------
sighup racoon to reload configurations
if racoon was not started, it will be started only if launch is set
----------------------------------------------------------------------------- */
int 
racoon_restart(int launch)
{
	int pid = racoon_pid();

	if (pid) {
		kill(pid, SIGHUP);
		//sleep(1); // no need to wait
	}
	else if (launch)
		racoon_start(0, 0);
    
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
		CFStringRef prop_method = NULL;

		/* get te default/preferred authentication from the ipsec dictionary, if available */
		auth_method = CFDictionaryGetValue(ipsec_dict, kRASPropIPSecProposalAuthenticationMethod);

		if (proposal_dict)
			prop_method = CFDictionaryGetValue(proposal_dict, kRASPropIPSecProposalAuthenticationMethod);

		strcpy(str, "pre_shared_key");
		if (isString(auth_method) || isString(prop_method)) {
				
			if (CFEqual(isString(prop_method) ? prop_method : auth_method, kRASValIPSecProposalAuthenticationMethodSharedSecret))
				strcpy(str, "pre_shared_key");
			else if (CFEqual(isString(prop_method) ? prop_method : auth_method, kRASValIPSecProposalAuthenticationMethodCertificate))
				strcpy(str, "rsasig");
			else 
				FAIL("incorrect authentication method";)
		}

		sprintf(text, "authentication_method %s;\n", str);
		WRITE(text);
	}

	/*
		authentication algorithm is OPTIONAL
	*/
	{
		char	str[256];
		CFStringRef algo;

		strcpy(str, "sha1");
		if (proposal_dict) {
			algo = CFDictionaryGetValue(proposal_dict, kRASPropIPSecProposalHashAlgorithm);
			if (isString(algo)) {
				
				if (CFEqual(algo, kRASValIPSecProposalHashAlgorithmMD5))
					strcpy(str, "md5");
				else if (CFEqual(algo, kRASValIPSecProposalHashAlgorithmSHA1))
					strcpy(str, "sha1");
				else 
					FAIL("incorrect authentication algorithm";)

			}
		}
		sprintf(text, "hash_algorithm %s;\n", str);
		WRITE(text);
	}
	
	/*
		encryption algorithm is OPTIONAL
	*/
	{
		char	str[256];
		CFStringRef crypto;

		strcpy(str, "3des");
		if (proposal_dict) {
			crypto = CFDictionaryGetValue(proposal_dict, kRASPropIPSecProposalEncryptionAlgorithm);
			if (isString(crypto)) {
				
				if (CFEqual(crypto, kRASValIPSecProposalEncryptionAlgorithmDES))
					strcpy(str, "des");
				else if (CFEqual(crypto, kRASValIPSecProposalEncryptionAlgorithm3DES))
					strcpy(str, "3des");
				else if (CFEqual(crypto, kRASValIPSecProposalEncryptionAlgorithmAES))
					strcpy(str, "aes");
				else 
					FAIL("incorrect encryption algorithm";)

			}
		}
		sprintf(text, "encryption_algorithm %s;\n", str);
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
		sprintf(text, "lifetime time %d sec;\n", lval);
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
		sprintf(text, "dh_group %d;\n", lval);
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
		
		strcpy(text, "exchange_mode ");
		nb = 0;
		modes = CFDictionaryGetValue(ipsec_dict, kRASPropIPSecExchangeMode);
		if (isArray(modes)) {		
			
			nb = max(CFArrayGetCount(modes), 3);
			for (i = 0; i < nb; i++) {
			
				mode = CFArrayGetValueAtIndex(modes, i);
				if (!isString(mode))
					continue;
					
				if (i)
					strcat(text, ", ");
					
				if (CFEqual(mode, kRASValIPSecExchangeModeMain))
					strcat(text, "main");
				else if (CFEqual(mode, kRASValIPSecExchangeModeAggressive))
					strcat(text, "aggressive");
				else if (CFEqual(mode, kRASValIPSecExchangeModeBase))
					strcat(text, "base");
				else
					FAIL("incorrect phase 1 exchange mode");
			}
		}
		if (nb == 0) {
			char str[256];
			/* default mode is main except if local identifier is defined */
			if (GetStrFromDict(ipsec_dict, kRASPropIPSecLocalIdentifier, str, sizeof(str), ""))
				strcat(text, "aggressive");
			else 
				strcat(text, "main");
		}
		strcat(text, ";\n");
		WRITE(text);
	}
	
	/*
		get the first authentication method from the proposals
		verify all proposal have the same authentication method
	*/
	{
		CFArrayRef  proposals;
		int 	 i, nb;
		CFDictionaryRef proposal;
		CFStringRef method;
		
		/* get te default/preferred authentication from the ipsec dictionary, if available */
		auth_method = CFDictionaryGetValue(ipsec_dict, kRASPropIPSecProposalAuthenticationMethod);

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
		if (auth_method == NULL)
			auth_method = kRASValIPSecProposalAuthenticationMethodSharedSecret;

		if (!CFEqual(auth_method, kRASValIPSecProposalAuthenticationMethodSharedSecret)
			&& !CFEqual(auth_method, kRASValIPSecProposalAuthenticationMethodCertificate))
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
				strcpy(str1, "fqdn");
			else if (!strcmp(str1, "UserFQDN"))
				strcpy(str1, "user_fqdn");
			else if (!strcmp(str1, "KeyID"))
				strcpy(str1, "keyid_use");
			else if (!strcmp(str1, "Address"))
				strcpy(str1, "address");
			else if (!strcmp(str1, "ASN1DN"))
				strcpy(str1, "asn1dn");
			else 
				strcpy(str1, "");
		}
		
		if (GetStrFromDict(ipsec_dict, kRASPropIPSecLocalIdentifier, str, sizeof(str), "")) {
			sprintf(text, "my_identifier %s \"%s\";\n", str1[0] ? str1 : "fqdn", str);
			WRITE(text);
		}
		else {
			if (CFEqual(auth_method, kRASValIPSecProposalAuthenticationMethodSharedSecret)) {
				/* 
					use local address 
				*/
			}
			else if (CFEqual(auth_method, kRASValIPSecProposalAuthenticationMethodCertificate)) {
				/* 
					use subject name from certificate 
				*/
				sprintf(text, "my_identifier asn1dn;\n");
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
				sprintf(text, "peers_identifier address \"%s\";\n", str);
				WRITE(text);
				
				if (CFEqual(auth_method, kRASValIPSecProposalAuthenticationMethodCertificate))
					cert_verification_option = CERT_VERIFICATION_OPTION_PEERS_ID;					
			}
			else if (CFEqual(string, kRASValIPSecIdentifierVerificationUseRemoteIdentifier)) {
				/* 
					verify using the explicitely specified remote identifier key 
				*/
				if (!GetStrFromDict(ipsec_dict, kRASPropIPSecRemoteIdentifier, str, sizeof(str), ""))
					FAIL("no remote identifier found");
				sprintf(text, "peers_identifier fqdn \"%s\";\n", str);
				WRITE(text);

				if (CFEqual(auth_method, kRASValIPSecProposalAuthenticationMethodCertificate))
					cert_verification_option = CERT_VERIFICATION_OPTION_PEERS_ID;					
			}
			else if (CFEqual(string, kRASValIPSecIdentifierVerificationUseOpenDirectory)) {
				/* 
					verify using open directory (certificates only)
				*/
				if (!CFEqual(auth_method, kRASValIPSecProposalAuthenticationMethodCertificate))
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
			sprintf(text, "verify_identifier %s;\n", (cert_verification_option == CERT_VERIFICATION_OPTION_NONE ? "on" : "off"));
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
		if (CFEqual(auth_method, kRASValIPSecProposalAuthenticationMethodSharedSecret)) {
			
			if (!GetStrFromDict(ipsec_dict, kRASPropIPSecSharedSecret, str, sizeof(str), ""))
				FAIL("no shared secret found");
				
			/* 
				Shared Secrets are stored in the KeyChain, in the plist or in the racoon keys file 
			*/
			strcpy(str1, "use");
			string = CFDictionaryGetValue(ipsec_dict, kRASPropIPSecSharedSecretEncryption);
			if (isString(string)) {
				if (CFEqual(string, kRASValIPSecSharedSecretEncryptionKey))
					strcpy(str1, "key");
				else if (CFEqual(string, kRASValIPSecSharedSecretEncryptionKeychain))
					strcpy(str1, "keychain");
				else
					FAIL("incorrect shared secret encryption found"); 
			}
			sprintf(text, "shared_secret %s \"%s\";\n", str1, str);
			WRITE(text);
		}
		/* 
			Certificates authentication method 
		*/
		else if (CFEqual(auth_method, kRASValIPSecProposalAuthenticationMethodCertificate)) {

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
			sprintf(text, "certificate_verification sec_framework%s;\n", option_str);
			WRITE(text);
		}
	}
	
    /* 
		Nonce size key is OPTIONAL 
	*/
	{
		u_int32_t lval;
		GetIntFromDict(ipsec_dict, kRASPropIPSecNonceSize, &lval, 16);
		sprintf(text, "nonce_size %d;\n", lval);
		WRITE(text);
	}
	
	/*
		Enable/Disable nat traversal multiple user support
	*/
	{
		int	natt_multi_user;
		
		if (GetIntFromDict(ipsec_dict, kRASPropIPSecNattMultipleUsersEnabled, &natt_multi_user, 0)) {
			sprintf(text, "nat_traversal_multi_user %s;\n", natt_multi_user ? "on" : "off");
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

		strcpy(str, "claim");
		behavior = CFDictionaryGetValue(ipsec_dict, kRASPropIPSecProposalsBehavior);
		if (isString(behavior)) {
			
			if (CFEqual(behavior, kRASValIPSecProposalsBehaviorClaim))
				strcpy(str, "claim");
			else if (CFEqual(behavior, kRASValIPSecProposalsBehaviorObey))
				strcpy(str, "obey");
			else if (CFEqual(behavior, kRASValIPSecProposalsBehaviorStrict))
				strcpy(str, "strict");
			else if (CFEqual(behavior, kRASValIPSecProposalsBehaviorExact))
				strcpy(str, "exact");
			else
				FAIL("incorrect proposal behavior");

		}
		sprintf(text, "proposal_check %s;\n", str);
		WRITE(text);
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
		
		strcpy(text, "encryption_algorithm ");
		
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
							strcat(text, ", ");
							
						if (CFEqual(algo, kRASValIPSecPolicyEncryptionAlgorithmDES))
							strcat(text, "des");
						else if (CFEqual(algo, kRASValIPSecPolicyEncryptionAlgorithm3DES))
							strcat(text, "3des");
						else if (CFEqual(algo, kRASValIPSecPolicyEncryptionAlgorithmAES))
							strcat(text, "aes");
						else 
							FAIL("incorrect encryption algorithm");
							
						found = 1;
					}
					
				}
			}
		}
		if (!found)
			strcat(text, "aes");

		strcat(text, ";\n");
		WRITE(text);
	}
	
	/* 
		Authentication algorithms are OPTIONAL
	*/
	{
		CFArrayRef  algos;
		int 	i, nb, found = 0;
		
		strcpy(text, "authentication_algorithm ");
		
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
							strcat(text, ", ");
							
						if (CFEqual(algo, kRASValIPSecPolicyHashAlgorithmSHA1))
							strcat(text, "hmac_sha1");
						else if (CFEqual(algo, kRASValIPSecPolicyHashAlgorithmMD5))
							strcat(text, "hmac_md5");
						else 
							FAIL("incorrect authentication algorithm");
							
						found = 1;
					}
					
				}
			}
		}
		if (!found)
			strcat(text, "hmac_sha1");

		strcat(text, ";\n");
		WRITE(text);
	}
	
	/* 
		Compression algorithms are OPTIONAL
	*/
	{
		CFArrayRef  algos;
		int 	i, nb, found = 0;
				
		strcpy(text, "compression_algorithm ");
		
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
							strcat(text, ", ");
							
						if (CFEqual(algo, kRASValIPSecPolicyCompressionAlgorithmDeflate))
							strcat(text, "deflate");
						else 
							FAIL("incorrect compression algorithm");
							
						found = 1;
					}
				}
			}
		}
		if (!found)
			strcat(text, "deflate");

		strcat(text, ";\n");
		WRITE(text);
	}

	/* 
		lifetime is OPTIONAL
	*/
	{
		u_int32_t lval = 3600;
		if (policy)
			GetIntFromDict(policy, kRASPropIPSecPolicyLifetime, &lval, 3600);
		sprintf(text, "lifetime time %d sec;\n", lval);
		WRITE(text);
	}

	/* 
		PFS Group is OPTIONAL
	*/
	{
		u_int32_t lval = 0;
		if (policy) { 
			if (GetIntFromDict(policy, kRASPropIPSecPolicyPFSGroup, &lval, 0)) {
				sprintf(text, "pfs_group %d;\n", lval);
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
	char	local_address[32], remote_address[32];
	
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
		sprintf(filename, "/etc/racoon/remote/%s.conf", anonymous ? "anonymous" : remote_address);
		/* remove any existing leftover, to create with right permissions */
		remove(filename);
	}
	
	/* 
		turn off group/other bits to create file rw owner only 
	*/
	mask = umask(S_IRWXG|S_IRWXO);
	file = fopen(apply ? filename : "/dev/null" , "w");
	mask = umask(mask);
    if (file == NULL) {
		sprintf(text, "cannot create racoon configuration file (error %d)", errno);
		FAIL(text);
	}

    /*
		write the remote record. this is common to all the proposals and sainfo records
	*/
	
	{
		
		sprintf(text, "remote %s {\n", anonymous ? "anonymous" : remote_address);
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
			FAIL("no policies found");
		
		/* if this is the anonymous configuration, only take the first policy */
		if (anonymous)
			nb = 1;
			
		for (i = 0; i < nb; i++) {
				
			CFDictionaryRef policy;
			CFStringRef policylevel, policymode;
			char	local_network[256], remote_network[256];
			int local_port, remote_port;
			int local_prefix, remote_prefix;
			int protocol, tunnel;

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
				sprintf(text, "sainfo anonymous {\n");
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

					sprintf(text, "sainfo address %s/%d 0 address %s/%d 0 {\n", 
						local_network, local_prefix, 
						remote_network, remote_prefix);
				}
				else {
					
					GetIntFromDict(policy, kRASPropIPSecPolicyLocalPort, &local_port, 0);
					GetIntFromDict(policy, kRASPropIPSecPolicyRemotePort, &remote_port, 0);
					GetIntFromDict(policy, kRASPropIPSecPolicyProtocol, &protocol, 0);

					sprintf(text, "sainfo address %s/32 [%d] %d address %s/32 [%d] %d {\n", 
						local_address, local_port, protocol, 
						remote_address, remote_port, protocol);
				}
			}
			
			WRITE(text);

			if (configure_sainfo(level + 1, file, ipsec_dict, policy, errstr))
				goto fail;
						
			/* end of the record */
			WRITE("}\n\n");
		}
	}
	
    fclose(file);

    /*
	 * signal racoon 
	 */
	
	if (apply) {
		racoon_restart(1);
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
	
	sprintf(filename, "/etc/racoon/remote/%s.conf", remote_address);
	remove(filename);
	
	racoon_restart(0);
    return 0;

fail:
	
	return -1;
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
    int			policylen_in, policylen_out, local_prefix, remote_prefix;
	int			protocol = 0xFF;
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
			sprintf(policystr_out, "out none");
			sprintf(policystr_in, "in none");
		}
		else if (CFEqual(policylevel, kRASValIPSecPolicyLevelRequire)) {
			if (tunnel) {
				sprintf(policystr_out, "out ipsec esp/tunnel/%s-%s/require", src_address, dst_address);
				sprintf(policystr_in, "in ipsec esp/tunnel/%s-%s/require", dst_address, src_address);
			}
			else {
				sprintf(policystr_out, "out ipsec esp/transport//require");
				sprintf(policystr_in, "in ipsec esp/transport//require");
			}
		}
		else if (CFEqual(policylevel, kRASValIPSecPolicyLevelDiscard)) {
			sprintf(policystr_out, "out discard");
			sprintf(policystr_in, "in discard");
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
				FAIL("incorrect remote network");
						
			remote_net.sin_len = sizeof(remote_net);
			remote_net.sin_family = AF_INET;
			remote_net.sin_port = htons(0);
			if (!inet_aton(str, &remote_net.sin_addr))
				FAIL("incorrect remote network");

			GetIntFromDict(policy, kRASPropIPSecPolicyRemotePrefix, &remote_prefix, 24);
		
		}
		else {
			int val;
			
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
    int			policylen_in, policylen_out, local_prefix, remote_prefix;
	int			protocol = 0xFF;
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

		sprintf(policystr_out, "out");
		sprintf(policystr_in, "in");
		
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
			int val;
			
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
						 sizeof(u_long)) :\
						 sizeof(u_long)))

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

    //SCLog(_sc_debug, LOG_DEBUG, CFSTR("rtm_flags = 0x%8.8x"), rtm->rtm_flags);

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
		if (err = ioctl(s, SIOCGIFMTU, (caddr_t) &ifr) < 0)
			;
		close(s);
	}
	return ifr.ifr_mtu;
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
	configuratoion varies slightly depending on client or server mode.
	
Return code:
the configuration dictionary
----------------------------------------------------------------------------- */
CFMutableDictionaryRef 
IPSecCreateL2TPDefaultConfiguration(struct sockaddr *src, struct sockaddr *dst, char *dst_hostName, CFStringRef authenticationMethod, 
		int isClient, int natt_multiple_users, CFStringRef identifierVerification) 
{
	CFStringRef				src_string, dst_string, hostname_string = NULL;
	CFMutableDictionaryRef	ipsec_dict, proposal_dict, policy0, policy1 = NULL;
	CFMutableArrayRef		policy_array, proposal_array, encryption_array, hash_array;
	CFNumberRef				src_port_num, dst_port_num, dst_port1_num, proto_num, natt_multiuser_mode;
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
	

	CFDictionarySetValue(ipsec_dict, kRASPropIPSecLocalAddress, src_string);
	CFDictionarySetValue(ipsec_dict, kRASPropIPSecRemoteAddress, dst_string);
	CFDictionarySetValue(ipsec_dict, kRASPropIPSecProposalsBehavior, isClient ? kRASValIPSecProposalsBehaviorObey : kRASValIPSecProposalsBehaviorClaim);
	if (isClient && CFEqual(authenticationMethod, kRASValIPSecProposalAuthenticationMethodCertificate)) {
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
	proposal_dict = CFDictionaryCreateMutable(0, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
	CFDictionarySetValue(proposal_dict, kRASPropIPSecProposalAuthenticationMethod, authenticationMethod);

	proposal_array = CFArrayCreateMutable(0, 0, &kCFTypeArrayCallBacks);
	CFArraySetValueAtIndex(proposal_array, 0, proposal_dict);
	CFRelease(proposal_dict);

	CFDictionarySetValue(ipsec_dict, kRASPropIPSecProposals, proposal_array);
	CFRelease(proposal_array);
	
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
	err = racoon_start(0,0);
	if (err)
		return -1;
	
	return 0;
}