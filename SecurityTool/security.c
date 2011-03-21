/*
 * Copyright (c) 2003-2010 Apple Inc. All Rights Reserved.
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
 *
 * security.c
 */

#include "security.h"

#include "leaks.h"
#include "readline.h"

#include "cmsutil.h"
#include "db_commands.h"
#include "keychain_add.h"
#include "keychain_create.h"
#include "keychain_delete.h"
#include "keychain_list.h"
#include "keychain_lock.h"
#include "keychain_set_settings.h"
#include "keychain_show_info.h"
#include "keychain_unlock.h"
#include "keychain_recode.h"
#include "key_create.h"
#include "keychain_find.h"
#include "keychain_import.h"
#include "keychain_export.h"
#include "identity_find.h"
#include "identity_prefs.h"
#include "mds_install.h"
#include "trusted_cert_add.h"
#include "trusted_cert_dump.h"
#include "user_trust_enable.h"
#include "trust_settings_impexp.h"
#include "verify_cert.h"
#include "authz.h"
#include "display_error_code.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <CoreFoundation/CFRunLoop.h>
#include <Security/SecBasePriv.h>
#include <security_asn1/secerr.h>

/* Maximum length of an input line in interactive mode. */
#define MAX_LINE_LEN 4096
/* Maximum number of arguments on an input line in interactive mode. */
#define MAX_ARGS 32

/* Entry in commands array for a command. */
typedef struct command
{
	const char *c_name;    /* name of the command. */
	command_func c_func;   /* function to execute the command. */
	const char *c_usage;   /* usage sting for command. */
	const char *c_help;    /* help string for (or description of) command. */
} command;

/* The default prompt. */
const char *prompt_string = "security> ";

/* The name of this program. */
const char *prog_name;


/* Forward declarations of static functions. */
static int help(int argc, char * const *argv);

/*
 * The command array itself.
 * Add commands here at will.
 * Matching is done on a prefix basis.  The first command in the array
 * gets matched first.
 */
const command commands[] =
{
	{ "help", help,
	  "[command ...]",
	  "Show all commands, or show usage for a command." },
    
	{ "list-keychains", keychain_list,
	  "[-d user|system|common|dynamic] [-s [keychain...]]\n"
	  "    -d  Use the specified preference domain\n"
	  "    -s  Set the search list to the specified keychains\n"
	  "With no parameters, display the search list.",
	  "Display or manipulate the keychain search list." },

	{ "default-keychain", keychain_default,
	  "[-d user|system|common|dynamic] [-s [keychain]]\n"
	  "    -d  Use the specified preference domain\n"
	  "    -s  Set the default keychain to the specified keychain\n"
	  "With no parameters, display the default keychain.",
	  "Display or set the default keychain." },

	{ "login-keychain", keychain_login,
	  "[-d user|system|common|dynamic] [-s [keychain]]\n"
	  "    -d  Use the specified preference domain\n"
	  "    -s  Set the login keychain to the specified keychain\n"
	  "With no parameters, display the login keychain.",
	  "Display or set the login keychain." },

	{ "create-keychain", keychain_create,
	  "[-P] [-p password] [keychains...]\n"
	  "    -p  Use \"password\" as the password for the keychains being created\n"
	  "    -P  Prompt the user for a password using the SecurityAgent",
	  "Create keychains and add them to the search list." },

	{ "delete-keychain", keychain_delete,
	  "[keychains...]",
	  "Delete keychains and remove them from the search list." },

	{ "lock-keychain", keychain_lock,
	  "[-a | keychain]\n"
	  "    -a  Lock all keychains",
	  "Lock the specified keychain."},

	{ "unlock-keychain", keychain_unlock,
	  "[-u] [-p password] [keychain]\n"
	  "    -p  Use \"password\" as the password to unlock the keychain\n"
	  "    -u  Do not use the password",
	  "Unlock the specified keychain."},

	{ "set-keychain-settings", keychain_set_settings,
	  "[-lu] [-t timeout] [keychain]\n"
	  "    -l  Lock keychain when the system sleeps\n"
	  "    -u  Lock keychain after timeout interval\n"
	  "    -t  Timeout in seconds (omitting this option specifies \"no timeout\")\n",
	  "Set settings for a keychain."},

	{ "set-keychain-password", keychain_set_password,
	  "[-o oldPassword] [-p newPassword] [keychain]\n"
	  "    -o  Old keychain password (if not provided, will prompt)\n"
	  "    -p  New keychain password (if not provided, will prompt)\n",
	  "Set password for a keychain."},

	{ "show-keychain-info", keychain_show_info,
	  "[keychain]",
	  "Show the settings for keychain." },

	{ "dump-keychain", keychain_dump,
	  "[-adir] [keychain...]\n"
	  "    -a  Dump access control list of items\n"
	  "    -d  Dump (decrypted) data of items\n"
	  "    -i  Interactive access control list editing mode\n"
	  "    -r  Dump the raw (encrypted) data of items",
	  "Dump the contents of one or more keychains." },

#ifndef NDEBUG
	{ "recode-keychain", keychain_recode,
	  "keychain_to_recode keychain_to_get_secrets_from",
	  "Recode a keychain to use the secrets from another one."},
#endif

	{ "create-keypair", key_create_pair,
	  "[-a alg] [-s size] [-f date] [-t date] [-d days] [-k keychain] [-A|-T appPath] description\n"
	  "    -a  Use alg as the algorithm, can be rsa, dh, dsa or fee (default rsa)\n"
	  "    -s  Specify the keysize in bits (default 512)\n"
	  "    -f  Make a key valid from the specified date\n"
	  "    -t  Make a key valid to the specified date\n"
	  "    -d  Make a key valid for the number of days specified from today\n"
	  "    -k  Use the specified keychain rather than the default\n"
	  "    -A  Allow any application to access this key without warning (insecure, not recommended!)\n"
	  "    -T  Specify an application which may access this key (multiple -T options are allowed)\n"
	  "If no options are provided, ask the user interactively.",
	  "Create an asymmetric key pair." },

	#if 0
	/* this was added in mb's integration of PR-3420772, but this is an unimplemented command */
	{ "create-csr", csr_create,
	  "[-a alg] [-s size] [-f date] [-t date] [-d days] [-k keychain] [-A|-T appPath] description\n"
	  "    -a  Use alg as the algorithm, can be rsa, dh, dsa or fee (default rsa)\n"
	  "    -s  Specify the keysize in bits (default 512)\n"
	  "    -f  Make a key valid from the specified date\n"
	  "    -t  Make a key valid to the specified date\n"
	  "    -d  Make a key valid for the number of days specified from today\n"
	  "    -k  Use the specified keychain rather than the default\n"
	  "    -A  Allow any application to access this key without warning (insecure, not recommended!)\n"
	  "    -T  Specify an application which may access this key (multiple -T options are allowed)\n"
	  "If no options are provided, ask the user interactively.",
	  "Create a certificate signing request." },
	#endif

	{ "add-generic-password", keychain_add_generic_password,
	  "[-a account] [-s service] [-w password] [options...] [-A|-T appPath] [keychain]\n"
	  "    -a  Specify account name (required)\n"
	  "    -c  Specify item creator (optional four-character code)\n"
	  "    -C  Specify item type (optional four-character code)\n"
	  "    -D  Specify kind (default is \"application password\")\n"
	  "    -G  Specify generic attribute (optional)\n"
	  "    -j  Specify comment string (optional)\n"
	  "    -l  Specify label (if omitted, service name is used as default label)\n"
	  "    -s  Specify service name (required)\n"
	  "    -p  Specify password to be added (legacy option, equivalent to -w)\n"
	  "    -w  Specify password to be added\n"
	  "    -A  Allow any application to access this item without warning (insecure, not recommended!)\n"
	  "    -T  Specify an application which may access this item (multiple -T options are allowed)\n"
	  "    -U  Update item if it already exists (if omitted, the item cannot already exist)\n"
	  "\n"
	  "By default, the application which creates an item is trusted to access its data without warning.\n"
	  "You can remove this default access by explicitly specifying an empty app pathname: -T \"\"\n"
	  "If no keychain is specified, the password is added to the default keychain.",
	  "Add a generic password item."},
      
	{ "add-internet-password", keychain_add_internet_password,
	  "[-a account] [-s server] [-w password] [options...] [-A|-T appPath] [keychain]\n"
	  "    -a  Specify account name (required)\n"
	  "    -c  Specify item creator (optional four-character code)\n"
	  "    -C  Specify item type (optional four-character code)\n"
	  "    -d  Specify security domain string (optional)\n"
	  "    -D  Specify kind (default is \"Internet password\")\n"
	  "    -j  Specify comment string (optional)\n"
	  "    -l  Specify label (if omitted, server name is used as default label)\n"
	  "    -p  Specify path string (optional)\n"
	  "    -P  Specify port number (optional)\n"
	  "    -r  Specify protocol (optional four-character SecProtocolType, e.g. \"http\", \"ftp \")\n"
	  "    -s  Specify server name (required)\n"
	  "    -t  Specify authentication type (as a four-character SecAuthenticationType, default is \"dflt\")\n"
  	  "    -w  Specify password to be added\n"
	  "    -A  Allow any application to access this item without warning (insecure, not recommended!)\n"
	  "    -T  Specify an application which may access this item (multiple -T options are allowed)\n"
	  "    -U  Update item if it already exists (if omitted, the item cannot already exist)\n"
	  "\n"
	  "By default, the application which creates an item is trusted to access its data without warning.\n"
	  "You can remove this default access by explicitly specifying an empty app pathname: -T \"\"\n"
	  "If no keychain is specified, the password is added to the default keychain.",
	  "Add an internet password item."},

	{ "add-certificates", keychain_add_certificates,
	  "[-k keychain] file...\n"
	  "If no keychain is specified, the certificates are added to the default keychain.",
	  "Add certificates to a keychain."},

	{ "find-generic-password", keychain_find_generic_password,
	  "[-a account] [-s service] [options...] [-g] [keychain...]\n"
	  "    -a  Match \"account\" string\n"
	  "    -c  Match \"creator\" (four-character code)\n"
	  "    -C  Match \"type\" (four-character code)\n"
	  "    -D  Match \"kind\" string\n"
	  "    -G  Match \"value\" string (generic attribute)\n"
	  "    -j  Match \"comment\" string\n"
	  "    -l  Match \"label\" string\n"
	  "    -s  Match \"service\" string\n"
	  "    -g  Display the password for the item found\n"
	  "If no keychains are specified to search, the default search list is used.",
	  "Find a generic password item."},

	{ "delete-generic-password", keychain_delete_generic_password,
		"[-a account] [-s service] [options...] keychain...]\n"
		"    -a  Match \"account\" string\n"
		"    -c  Match \"creator\" (four-character code)\n"
		"    -C  Match \"type\" (four-character code)\n"
		"    -D  Match \"kind\" string\n"
		"    -G  Match \"value\" string (generic attribute)\n"
		"    -j  Match \"comment\" string\n"
		"    -l  Match \"label\" string\n"
		"    -s  Match \"service\" string\n"
		"If no keychains are specified to search, the default search list is used.",
		"Delete a generic password item."},

	{ "find-internet-password", keychain_find_internet_password,
	  "[-a account] [-s server] [options...] [-g] [keychain...]\n"
	  "    -a  Match \"account\" string\n"
	  "    -c  Match \"creator\" (four-character code)\n"
	  "    -C  Match \"type\" (four-character code)\n"
	  "    -d  Match \"securityDomain\" string\n"
	  "    -D  Match \"kind\" string\n"
	  "    -j  Match \"comment\" string\n"
	  "    -l  Match \"label\" string\n"
	  "    -p  Match \"path\" string\n"
	  "    -P  Match port number\n"
	  "    -r  Match \"protocol\" (four-character code)\n"
	  "    -s  Match \"server\" string\n"
	  "    -t  Match \"authenticationType\" (four-character code)\n"
	  "    -g  Display the password for the item found\n"
	  "If no keychains are specified to search, the default search list is used.",
	  "Find an internet password item."},

	{ "delete-internet-password", keychain_delete_internet_password,
		"[-a account] [-s server] [options...] [keychain...]\n"
		"    -a  Match \"account\" string\n"
		"    -c  Match \"creator\" (four-character code)\n"
		"    -C  Match \"type\" (four-character code)\n"
		"    -d  Match \"securityDomain\" string\n"
		"    -D  Match \"kind\" string\n"
		"    -j  Match \"comment\" string\n"
		"    -l  Match \"label\" string\n"
		"    -p  Match \"path\" string\n"
		"    -P  Match port number\n"
		"    -r  Match \"protocol\" (four-character code)\n"
		"    -s  Match \"server\" string\n"
		"    -t  Match \"authenticationType\" (four-character code)\n"
		"If no keychains are specified to search, the default search list is used.",
		"Delete an internet password item."},

	{ "find-certificate", keychain_find_certificate,
	  "[-a] [-c name] [-e emailAddress] [-m] [-p] [-Z] [keychain...]\n"
	  "    -a  Find all matching certificates, not just the first one\n"
	  "    -c  Match on \"name\" when searching (optional)\n"
	  "    -e  Match on \"emailAddress\" when searching (optional)\n"
	  "    -m  Show the email addresses in the certificate\n"
	  "    -p  Output certificate in pem format\n"
	  "    -Z  Print SHA-1 hash of the certificate\n"
	  "If no keychains are specified to search, the default search list is used.",
	  "Find a certificate item."},

	{ "find-identity", keychain_find_identity,
		"[-p policy] [-s string] [-v] [keychain...]\n"
		"    -p  Specify policy to evaluate (multiple -p options are allowed)\n"
		"        Supported policies: basic, ssl-client, ssl-server, smime, eap,\n"
		"        ipsec, ichat, codesigning, sys-default, sys-kerberos-kdc, macappstore\n"
		"    -s  Specify optional policy-specific string (e.g. DNS hostname for SSL,\n"
		"        or RFC822 email address for S/MIME)\n"
		"    -v  Show valid identities only (default is to show all identities)\n"
		"If no keychains are specified to search, the default search list is used.",
	"Find an identity (certificate + private key)."},
	
	{ "delete-certificate", keychain_delete_certificate,
	  "[-c name] [-Z hash] [-t] [keychain...]\n"
	  "    -c  Specify certificate to delete by its common name\n"
	  "    -Z  Specify certificate to delete by its SHA-1 hash value\n"
	  "    -t  Also delete user trust settings for this certificate\n"
	  "The certificate to be deleted must be uniquely specified either by a\n"
	  "string found in its common name, or by its SHA-1 hash.\n"
	  "If no keychains are specified to search, the default search list is used.",
	  "Delete a certificate from a keychain."},

	{ "set-identity-preference", set_identity_preference,
	  "[-n] [-c identity] [-s service] [-u keyUsage] [-Z hash] [keychain...]\n"
	  "    -n  Specify no identity (clears existing preference for service)\n"
	  "    -c  Specify identity by common name of the certificate\n"
	  "    -s  Specify service (may be a URL, RFC822 email address, DNS host, or\n"
	  "        other name) for which this identity is to be preferred\n"
	  "    -u  Specify key usage (optional) - see man page for values\n"
	  "    -Z  Specify identity by SHA-1 hash of certificate (optional)\n",
	  "Set the preferred identity to use for a service."},
	
	{ "get-identity-preference", get_identity_preference,
		"[-s service] [-u keyUsage] [-p] [-c] [-Z] [keychain...]\n"
		"    -s  Specify service (may be a URL, RFC822 email address, DNS host, or\n"
		"        other name)\n"
		"    -u  Specify key usage (optional) - see man page for values\n"
		"    -p  Output identity certificate in pem format\n"
		"    -c  Print common name of the preferred identity certificate\n"
		"    -Z  Print SHA-1 hash of the preferred identity certificate\n",
	"Get the preferred identity to use for a service."},

	{ "create-db", db_create,
	  "[-ao0] [-g dl|cspdl] [-m mode] [name]\n"
	  "    -a  Turn off autocommit\n"
	  "    -g  Attach to \"guid\" rather than the AppleFileDL\n"
	  "    -m  Set the inital mode of the created db to \"mode\"\n"
	  "    -o  Force using openparams argument\n"
	  "    -0  Force using version 0 openparams\n"
	  "If no name is provided, ask the user interactively.",
	  "Create a db using the DL." },

	{ "export" , keychain_export,
	  "[-k keychain] [-t type] [-f format] [-w] [-p] [-P passphrase] [-o outfile]\n"
	  "    -k  keychain to export items from\n"
	  "    -t  Type = certs|allKeys|pubKeys|privKeys|identities|all  (Default: all)\n"
	  "    -f  Format = openssl|openssh1|openssh2|bsafe|pkcs7|pkcs8|pkcs12|pemseq|x509\n"
	  "        ...default format is pemseq for aggregate, openssl for single\n"
	  "    -w  Private keys are wrapped\n"
	  "    -p  PEM encode the output\n"
	  "    -P  Specify wrapping passphrase immediately (default is secure passphrase via GUI)\n"
	  "    -o  Specify output file (default is stdout)",
	  "Export items from a keychain." },

	{ "import", keychain_import,
	  "inputfile [-k keychain] [-t type] [-f format] [-w] [-P passphrase] [options...]\n"
	  "    -k  Target keychain to import into\n"
	  "    -t  Type = pub|priv|session|cert|agg\n"
	  "    -f  Format = openssl|openssh1|openssh2|bsafe|raw|pkcs7|pkcs8|pkcs12|netscape|pemseq\n"
	  "    -w  Specify that private keys are wrapped and must be unwrapped on import\n"
	  "    -x  Specify that private keys are non-extractable after being imported\n"
	  "    -P  Specify wrapping passphrase immediately (default is secure passphrase via GUI)\n"
	  "    -a  Specify name and value of extended attribute (can be used multiple times)\n"
	  "    -A  Allow any application to access the imported key without warning (insecure, not recommended!)\n"
	  "    -T  Specify an application which may access the imported key (multiple -T options are allowed)\n",
	  "Import items into a keychain." },

	{ "cms", cms_util,
	  "[-C|-D|-E|-S] [<options>]\n"
	  "  -C           create a CMS encrypted message\n"
	  "  -D           decode a CMS message\n"
	  "  -E           create a CMS enveloped message\n"
	  "  -S           create a CMS signed message\n"
	  "\n"
	  "Decoding options:\n"
	  "  -c content   use this detached content file\n"
	  "  -h level     generate email headers with info about CMS message\n"
	  "                 (output level >= 0)\n"
	  "  -n           suppress output of content\n"
	  "\n"
	  "Encoding options:\n"
	  "  -r id,...    create envelope for these recipients,\n"
	  "               where id can be a certificate nickname or email address\n"
	  "  -G           include a signing time attribute\n"
	  "  -H hash      hash = MD2|MD4|MD5|SHA1|SHA256|SHA384|SHA512 (default: SHA1)\n"
	  "  -N nick      use certificate named \"nick\" for signing\n"
	  "  -P           include a SMIMECapabilities attribute\n"
	  "  -T           do not include content in CMS message\n"
	  "  -Y nick      include an EncryptionKeyPreference attribute with certificate\n"
	  "                 (use \"NONE\" to omit)\n"
	  "  -Z hash      find a certificate by subject key ID\n"
	  "\n"
	  "Common options:\n"
	  "  -e envelope  specify envelope file (valid with -D or -E)\n"
	  "  -k keychain  specify keychain to use\n"
	  "  -i infile    use infile as source of data (default: stdin)\n"
	  "  -o outfile   use outfile as destination of data (default: stdout)\n"
	  "  -p password  use password as key db password (default: prompt)\n"
	  "  -s           pass data a single byte at a time to CMS\n"
	  "  -u certusage set type of certificate usage (default: certUsageEmailSigner)\n"
	  "  -v           print debugging information\n"
	  "\n"
	  "Cert usage codes:\n"
	  "                  0 - certUsageSSLClient\n"
	  "                  1 - certUsageSSLServer\n"
	  "                  2 - certUsageSSLServerWithStepUp\n"
	  "                  3 - certUsageSSLCA\n"
	  "                  4 - certUsageEmailSigner\n"
	  "                  5 - certUsageEmailRecipient\n"
	  "                  6 - certUsageObjectSigner\n"
	  "                  7 - certUsageUserCertImport\n"
	  "                  8 - certUsageVerifyCA\n"
	  "                  9 - certUsageProtectedObjectSigner\n"
	  "                 10 - certUsageStatusResponder\n"
	  "                 11 - certUsageAnyCA",
	  "Encode or decode CMS messages." },
    
	{ "install-mds" , mds_install,
	  "",		/* no options */
	  "Install (or re-install) the MDS database." },

	{ "add-trusted-cert" , trusted_cert_add,
	  " [<options>] [certFile]\n"
	  "    -d                  Add to admin cert store; default is user\n"
	  "    -r resultType       resultType = trustRoot|trustAsRoot|deny|unspecified;\n"
	  "                              default is trustRoot\n"
	  "    -p policy           Specify policy constraint (ssl, smime, codeSign, IPSec, iChat,\n"
	  "                              basic, swUpdate, pkgSign, pkinitClient, pkinitServer, eap)\n"
	  "    -a appPath          Specify application constraint\n"
	  "    -s policyString     Specify policy-specific string\n"
	  "    -e allowedError     Specify allowed error (certExpired, hostnameMismatch) or integer\n"
  	  "    -u keyUsage         Specify key usage, an integer\n"
	  "    -k keychain         Specify keychain to which cert is added\n"
	  "    -i settingsFileIn   Input trust settings file; default is user domain\n"
	  "    -o settingsFileOut  Output trust settings file; default is user domain\n"
	  "    -D                  Add default setting instead of per-cert setting\n"
	  "    certFile            Certificate(s)",
	  "Add trusted certificate(s)." },

	{ "remove-trusted-cert" , trusted_cert_remove,
	  " [-d] [-D] [certFile]\n"
	  "    -d                  Remove from admin cert store (default is user)\n"
	  "    -D                  Remove default setting instead of per-cert setting\n"
	  "    certFile            Certificate(s)",
	  "Remove trusted certificate(s)." },

	{ "dump-trust-settings" , trusted_cert_dump,
	  " [-s] [-d]\n"
	  "    -s                  Display trusted system certs (default is user)\n"
	  "    -d                  Display trusted admin certs (default is user)\n",
	  "Display contents of trust settings." },

	{ "user-trust-settings-enable", user_trust_enable,
	  "[-d] [-e]\n"
	  "    -d                  Disable user-level trust Settings\n"
	  "    -e                  Enable user-level trust Settings\n"
	  "With no parameters, show current enable state of user-level trust settings.",
	  "Display or manipulate user-level trust settings." },

	{ "trust-settings-export", trust_settings_export,
	  " [-s] [-d] settings_file\n"
	  "    -s                  Export system trust settings (default is user)\n"
	  "    -d                  Export admin trust settings (default is user)\n",
	  "Export trust settings." },

	{ "trust-settings-import", trust_settings_import,
	  " [-d] settings_file\n"
	  "    -d                  Import admin trust settings (default is user)\n",
	  "Import trust settings." },

	{ "verify-cert" , verify_cert,
	  " [<options>]\n"
	  "    -c certFile         Certificate to verify. Can be specified multiple times, leaf first.\n"
	  "    -r rootCertFile     Root Certificate. Can be specified multiple times.\n"
	  "    -p policy           Verify Policy (basic, ssl, smime, codeSign, IPSec, iChat, swUpdate\n"
	  "                              pkgSign, pkinitClient, pkinitServer, eap, macappstore); default is basic.\n"
	  "    -k keychain         Keychain. Can be called multiple times. Default is default search list.\n"
	  "    -n                  No keychain search list.\n"
	  "    -L                  Local certificates only (do not try to fetch missing CA certs from net).\n"
	  "    -l                  Leaf cert is a CA (normally an error, unless this option is given).\n"
	  "    -e emailAddress     Email address for smime policy.\n"
	  "    -s sslHost          SSL host name for ssl policy.\n"
	  "    -q                  Quiet.\n",
	  "Verify certificate(s)." },

	{ "authorize" , authorize,
	  "[<options>] <right(s)...>\n"
	  "  -u        Allow user interaction.\n"
	  "  -c        Use login name and prompt for password.\n"
	  "  -C login  Use given login name and prompt for password.\n"
	  "  -x        Do NOT share -c/-C explicit credentials\n"
#ifndef NDEBUG
	  "  -E        Don't extend rights.\n"
#endif
	  "  -p        Allow returning partial rights.\n"
	  "  -d        Destroy acquired rights.\n"
	  "  -P        Pre-authorize rights only.\n"
	  "  -l        Operate authorizations in least privileged mode.\n"
	  "  -i        Internalize authref passed on stdin.\n"
	  "  -e        Externalize authref to stdout.\n"
	  "  -w        Wait until stdout is closed (to allow reading authref from pipe).\n"
	  "Extend rights flag is passed per default.",
	  "Perform authorization operations." },

	{ "authorizationdb" , authorizationdb,
	  "read <right-name>\n"
	  "       authorizationdb remove <right-name>\n"
	  "       authorizationdb write <right-name> [allow|deny|<rulename>]\n"
	  "If no rulename is specified, write will read a plist from stdin.",
	  "Make changes to the authorization policy database." },

	{ "execute-with-privileges" , execute_with_privileges,
	  "<program> [args...]\n"
	  "On success, stdin will be read and forwarded to the tool.",
	  "Execute tool with privileges." },

	{ "leaks", leaks,
	  "[-cycles] [-nocontext] [-nostacks] [-exclude symbol]\n"
	  "    -cycles       Use a stricter algorithm (\"man leaks\" for details)\n"
	  "    -nocontext    Withhold hex dumps of the leaked memory\n"
	  "    -nostacks     Don't show stack traces of leaked memory\n"
	  "    -exclude      Ignore leaks called from \"symbol\"\n"
	  "(Set the environment variable MallocStackLogging to get symbolic traces.)",
	  "Run /usr/bin/leaks on this process." },

	{ "error", display_error_code,
	  "<error code(s)...>\n"
	  "Display an error string for the given security-related error code.\n"
	  "The error can be in decimal or hex, e.g. 1234 or 0x1234. Multiple "
	  "errors can be separated by spaces.",
	  "Display a descriptive message for the given error code(s)." },

	{}
};

/* Global variables. */
int do_quiet = 0;
int do_verbose = 0;

/* Return 1 if name matches command. */
static int
match_command(const char *command, const char *name)
{
	return !strncmp(command, name, strlen(name));
}

/* The help command. */
static int
help(int argc, char * const *argv)
{
	const command *c;

	if (argc > 1)
	{
		char * const *arg;
		for (arg = argv + 1; *arg; ++arg)
		{
			int found = 0;

			for (c = commands; c->c_name; ++c)
			{
				if (match_command(c->c_name, *arg))
				{
					found = 1;
					break;
				}
			}

			if (found)
				printf("Usage: %s %s\n", c->c_name, c->c_usage);
			else
			{
				sec_error("%s: no such command: %s", argv[0], *arg);
				return 1;
			}
		}
	}
	else
	{
		for (c = commands; c->c_name; ++c)
			printf("    %-17s %s\n", c->c_name, c->c_help);
	}

	return 0;
}

/* States for split_line parser. */
typedef enum
{
	SKIP_WS,
	READ_ARG,
	READ_ARG_ESCAPED,
	QUOTED_ARG,
	QUOTED_ARG_ESCAPED
} parse_state;

/* Split a line into multiple arguments and return them in *pargc and *pargv. */
static void
split_line(char *line, int *pargc, char * const **pargv)
{
	static char *argvec[MAX_ARGS + 1];
	int argc = 0;
	char *ptr = line;
	char *dst = line;
	parse_state state = SKIP_WS;
	int quote_ch = 0;

	for (ptr = line; *ptr; ++ptr)
	{
		if (state == SKIP_WS)
		{
			if (isspace(*ptr))
				continue;

			if (*ptr == '"' || *ptr == '\'')
			{
				quote_ch = *ptr;
				state = QUOTED_ARG;
				argvec[argc] = dst;
				continue; /* Skip the quote. */
			}
			else
			{
				state = READ_ARG;
				argvec[argc] = dst;
			}
		}

		if (state == READ_ARG)
		{
			if (*ptr == '\\')
			{
				state = READ_ARG_ESCAPED;
				continue;
			}
			else if (isspace(*ptr))
			{
				/* 0 terminate each arg. */
				*dst++ = '\0';
				argc++;
				state = SKIP_WS;
				if (argc >= MAX_ARGS)
					break;
			}
			else
				*dst++ = *ptr;
		}

		if (state == QUOTED_ARG)
		{
			if (*ptr == '\\')
			{
				state = QUOTED_ARG_ESCAPED;
				continue;
			}
			if (*ptr == quote_ch)
			{
				/* 0 terminate each arg. */
				*dst++ = '\0';
				argc++;
				state = SKIP_WS;
				if (argc >= MAX_ARGS)
					break;
			}
			else
				*dst++ = *ptr;
		}

		if (state == READ_ARG_ESCAPED)
		{
			*dst++ = *ptr;
			state = READ_ARG;
		}

		if (state == QUOTED_ARG_ESCAPED)
		{
			*dst++ = *ptr;
			state = QUOTED_ARG;
		}
	}

	if (state != SKIP_WS)
	{
		/* Terminate last arg. */
		*dst++ = '\0';
		argc++;
	}

	/* Teminate arg vector. */
	argvec[argc] = NULL;

	*pargv = argvec;
	*pargc = argc;
}

/* Print a (hopefully) useful usage message. */
static int
usage(void)
{
	printf(
		"Usage: %s [-h] [-i] [-l] [-p prompt] [-q] [-v] [command] [opt ...]\n"
		"    -i    Run in interactive mode.\n"
		"    -l    Run /usr/bin/leaks -nocontext before exiting.\n"
		"    -p    Set the prompt to \"prompt\" (implies -i).\n"
		"    -q    Be less verbose.\n"
		"    -v    Be more verbose about what's going on.\n"
		"%s commands are:\n", prog_name, prog_name);
	help(0, NULL);
	return 2;
}

/* Execute a single command. */ 
static int
execute_command(int argc, char * const *argv)
{
	const command *c;
	int found = 0;

	/* Nothing to do. */
	if (argc == 0)
		return 0;

	for (c = commands; c->c_name; ++c)
	{
		if (match_command(c->c_name, argv[0]))
		{
			found = 1;
			break;
		}
	}

	if (found)
	{
		int result;

		/* Reset getopt for command proc. */
		optind = 1;
		optreset = 1;

		if (do_verbose)
		{
			int ix;

			fprintf(stderr, "%s", c->c_name);
			for (ix = 1; ix < argc; ++ix)
				fprintf(stderr, " \"%s\"", argv[ix]);
			fprintf(stderr, "\n");
		}

		result = c->c_func(argc, argv);
		if (result == 2)
			fprintf(stderr, "Usage: %s %s\n        %s\n", c->c_name, c->c_usage, c->c_help);

		return result;
	}
	else
	{
		sec_error("unknown command \"%s\"", argv[0]);
		return 1;
	}
}

static void
receive_notifications(void)
{
	/* Run the CFRunloop to get any pending notifications. */
	while (CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0.0, TRUE) == kCFRunLoopRunHandledSource);
}


const char *
sec_errstr(int err)
{
    const char *errString;
    if (IS_SEC_ERROR(err))
        errString = SECErrorString(err);
    else
        errString = cssmErrorString(err);
    return errString;
}

void
sec_error(const char *msg, ...)
{
    va_list args;

    fprintf(stderr, "%s: ", prog_name);

    va_start(args, msg);
    vfprintf(stderr, msg, args);
    va_end(args);

    fprintf(stderr, "\n");
}

void
sec_perror(const char *msg, int err)
{
    sec_error("%s: %s", msg, sec_errstr(err));
}

int
main(int argc, char * const *argv)
{
	int result = 0;
	int do_help = 0;
	int do_interactive = 0;
	int do_leaks = 0;
	int ch;

	/* Remember my name. */
	prog_name = strrchr(argv[0], '/');
	prog_name = prog_name ? prog_name + 1 : argv[0];

	/* Do getopt stuff for global options. */
	optind = 1;
	optreset = 1;
	while ((ch = getopt(argc, argv, "hilp:qv")) != -1)
	{
		switch  (ch)
		{
		case 'h':
			do_help = 1;
			break;
		case 'i':
			do_interactive = 1;
			break;
		case 'l':
			do_leaks = 1;
			break;
		case 'p':
			do_interactive = 1;
			prompt_string = optarg;
			break;
		case 'q':
			do_quiet = 1;
			break;
		case 'v':
			do_verbose = 1;
			break;
		case '?':
		default:
			return usage();
		}
	}

	argc -= optind;
	argv += optind;

	if (do_help)
	{
		/* Munge argc/argv so that argv[0] is something. */
		return help(argc + 1, argv - 1);
	}
	else if (argc > 0)
	{
		receive_notifications();
		result = execute_command(argc, argv);
		receive_notifications();
	}
	else if (do_interactive)
	{
		/* In interactive mode we just read commands and run them until readline returns NULL. */

        /* Only show prompt string if stdin is a tty. */
        int show_prompt = isatty(0);

		for (;;)
		{
			static char buffer[MAX_LINE_LEN];
			char * const *av, *input;
			int ac;

            if (show_prompt)
                fprintf(stderr, "%s", prompt_string);

			input = readline(buffer, MAX_LINE_LEN);
			if (!input)
				break;

			split_line(input, &ac, &av);
			receive_notifications();
			result = execute_command(ac, av);
			receive_notifications();
			if (result == -1)
			{
				result = 0;
				break;
			}

			if (result && ! do_quiet)
			{
				fprintf(stderr, "%s: returned %d\n", av[0], result);
			}
		}
	}
	else
		result = usage();

	if (do_leaks)
	{
		char *const argvec[3] = { "leaks", "-nocontext", NULL };
		leaks(2, argvec);
	}

	return result;
}
