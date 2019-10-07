// This is a preprocessed file to define commands that we provide in Security part of the Sec module.

#include "SecurityTool/sharedTool/security_tool_commands.h"

#if TARGET_OS_IPHONE
#define USE_SECURITY_ITEM "By default the synchronizable keys is not searched/update/deleted, use \"security item\" for that.\n"
#else
#define USE_SECURITY_ITEM
#endif


SECURITY_COMMAND("add-internet-password", keychain_add_internet_password,
                 "[-a accountName] [-d securityDomain] [-p path] [-P port] [-r protocol] [-s serverName] [-t authenticationType] [-w passwordData] [keychain]\n"
                 "    -a Use \"accountName\".\n"
                 "    -d Use \"securityDomain\".\n"
                 "    -p Use \"path\".\n"
                 "    -P Use \"port\".\n"
                 "    -r Use \"protocol\".\n"
                 "    -s Use \"serverName\".\n"
                 "    -t Use \"authenticationType\".\n"
                 "    -w Use passwordData.\n"
                 "If no keychains is specified the password is added to the default keychain.",
                 "Add an internet password item.")

SECURITY_COMMAND("item", keychain_item,
                 "[-v][-a|-D|-u attr=value,...|[-q][-j][-g] attr=value,...] [-d password | -f datafile] [attr=value,...]\n"
                 "-q Query for item matching (default). Note: as default query skips items with ACL, you have to define 'u_AuthUI=u_AuthUIA' if you want to query items with ACL\n"
                 "-g Get password data\n"
                 "-a Add item to keychain\n"
                 "-u Update item in keychain (require query to match)\n"
                 "-D Delete item from keychain\n"
                 "-j When printing results, print JSON\n"
                 "Add, query, update or delete items from the keychain.  Extra attr=value pairs after options always apply to the query\n"
                 "class=[genp|inet|cert|keys] is required for the query\n"
                 "To search the synchronizable items (not searched by default) use sync=1 as an attr=value pair.\n"
                 "Security Access Control object can be passed as attribute accc with following syntax:\n"
                 "accc=\"<access class>[;operation[:constraint type(constraint parameters)]...]\""
                 "\nExample:\naccc=\"ak;od(cpo(DeviceOwnerAuthentication));odel(true);oe(true)\""
                 "\naccc=\"ak;od(cpo(DeviceOwnerAuthentication));odel(true);oe(true);prp(true)\""
                 "\naccc=\"ak;od(cup(true)pkofn(1)cbio(pbioc(<>)pbioh(<>)));odel(true);oe(true)\""
                 "SAC object for deleting item added by default\n",
                 "Manipulate keychain items.")

SECURITY_COMMAND("policy-dryrun", policy_dryrun,
                 "",
                 "Try to evaluate policy old/new.")

SECURITY_COMMAND("keychain-item-digest", keychain_item_digest,
                 "itemClass keychainAccessGroup\n"
                 "Dump items reported by _SecItemDigest command\n",
                 "Show keychain item digest.")

SECURITY_COMMAND_IOS("add-certificates", keychain_add_certificates,
                 "[-k keychain] file...\n"
                 "If no keychains is specified the certificates are added to the default keychain.\n"
                 "\tadd-certificates -t file...\n"
                 "Add the specified certificates to the users TrustSettings.sqlite3 database.",
                 "Add certificates to the keychain.")

SECURITY_COMMAND_IOS("show-certificates", keychain_show_certificates,
                 "[-p][-s][-t] file...\n"
                 "[-k][-p][-s][-v][-t][-f][-q attr=value,...] [attr=value,...]\n"
                 "    -k Show all certificates in keychain.\n"
                 "    -q Query for certificates matching (implies -k)\n"
                 "    -p Output cert in PEM format.\n"
                 "    -f Show fingerprint (SHA1 digest of octects inside the public key bit string.)\n"
                 "    -s Show subject.\n"
                 "    -v Show entire certificate in text form.\n"
                 "    -t Evaluate trust.",
                 "Display certificates in human readable form.")

SECURITY_COMMAND("find-internet-password", keychain_find_internet_password,
                 "[-a accountName] [-d securityDomain] [-g] [-p path] [-P port] [-r protocol] [-s serverName] [-t authenticationType] [keychain...]\n"
                 "    -a Match on \"accountName\" when searching.\n"
                 "    -d Match on \"securityDomain\" when searching.\n"
                 "    -g Display the password for the item found.\n"
                 "    -p Match on \"path\" when searching.\n"
                 "    -P Match on \"port\" when searching.\n"
                 "    -r Match on \"protocol\" when searching.\n"
                 "    -s Match on \"serverName\" when searching.\n"
                 "    -t Match on \"authenticationType\" when searching.\n"
                 USE_SECURITY_ITEM
                 "If no keychains are specified the default search list is used.",
                 "Find an internet password item.")

SECURITY_COMMAND("find-generic-password", keychain_find_generic_password,
                 "[-a accountName] [-s serviceName] [keychain...]\n"
                 "    -a Match on \"accountName\" when searching.\n"
                 "    -g Display the password for the item found.\n"
                 "    -s Match on \"serviceName\" when searching.\n"
                 "If no keychains are specified the default search list is used.",
                 "Find a generic password item.")

SECURITY_COMMAND("delete-internet-password", keychain_delete_internet_password,
                 "[-a accountName] [-d securityDomain] [-g] [-p path] [-P port] [-r protocol] [-s serverName] [-t authenticationType] [keychain...]\n"
                 "    -a Match on \"accountName\" when searching.\n"
                 "    -d Match on \"securityDomain\" when searching.\n"
                 "    -g Display the password for the item found.\n"
                 "    -p Match on \"path\" when searching.\n"
                 "    -P Match on \"port\" when searching.\n"
                 "    -r Match on \"protocol\" when searching.\n"
                 "    -s Match on \"serverName\" when searching.\n"
                 "    -t Match on \"authenticationType\" when searching.\n"
                 USE_SECURITY_ITEM
                 "If no keychains are specified the default search list is used.",
                 "Delete one or more internet password items.")

SECURITY_COMMAND("delete-generic-password", keychain_delete_generic_password,
                 "[-a accountName] [-s serviceName] [keychain...]\n"
                 "    -a Match on \"accountName\" when searching.\n"
                 "    -g Display the password for the item found.\n"
                 "    -s Match on \"serviceName\" when searching.\n"
                 USE_SECURITY_ITEM
                 "If no keychains are specified the default search list is used.",
                 "Delete one or more generic password items.")

SECURITY_COMMAND_IOS("keychain-export", keychain_export,
                 "-k <keybag> [-p password ] <plist>\n"
                 "    <keybag>   keybag file name. (Can be created with keystorectl)\n"
                 "    <password> backup password (optional)\n"
                 "    <plist>    backup plist file\n",
                 "Export keychain to a plist file.")

SECURITY_COMMAND_IOS("keychain-import", keychain_import,
                 "-k <keybag> [-p <password> ] <plist>\n"
                 "    <keybag>   keybag file name. (Can be created with keystorectl)\n"
                 "    <password> backup password (optional)\n"
                 "    <plist>    backup plist file\n",
                 "Import keychain from a plist file.")

SECURITY_COMMAND_IOS("pkcs12", pkcs12_util,
                 "[options] -p <password> file\n"
                 "  -d           delete identity\n",
                 "Manipulate pkcs12 blobs.")

SECURITY_COMMAND_IOS("scep", command_scep,
                 "[options] <url>\n"
                 "   -b keysize      Keysize in bits.\n"
                 "   -u usage        Key usage bitmask in decimal (Digital Signature = 1, Key Encipherment = 4).\n"
                 "   -c challenge    Challenge password.\n"
                 "   -n name         Service instance name (required for MS SCEP).\n"
                 "   -v              Verbose.\n"
                 "   -x              Turn cert validation off.\n"
                 "   -s subject      Subject to request (O=Apple,CN=iPhone).\n"
                 "   -h subjaltname  SubjectAlternateName (foo.com).\n"
                 "   -o capabilities Override capabilities GetCACaps returns (POSTPKIOperation,SHA-1,DES3)\n",
                 "Certify a public key using a SCEP server")

SECURITY_COMMAND_IOS("codesign", codesign_util,
                 "[options] <file>\n",
                 "Verify code signature blob in binary.")

SECURITY_COMMAND_IOS("enroll-secure-profile", command_spc,
                 "[options] <file>\n",
                 "Enroll in secure profile service.")

SECURITY_COMMAND_IOS("keys-need-update", keychain_roll_keys,
                     "[options]\n"
                     "   -f   attempt an update.\n",
                     "Rotate keys.")

SECURITY_COMMAND("log", log_control,
                 "[options] [scope_list]\n"
                 "   -l              list current settings.\n"
                 "   -s scope_list   set log scopes to scope_list.\n"
                 "   -c scope_list   set log scopes to scope_list for all devices in circle.\n",
                 "control logging settings")

SECURITY_COMMAND_IOS("verify-cert", verify_cert,
                 "[options]\n"
                 "   -c certFile     Certificate to verify. Can be specified multiple times.\n"
                 "   -r rootCertFile Root Certificate. Can be specified multiple times.\n"
                 "   -p policy       Verify policy (basic, ssl, smime, eap, IPSec, appleID,\n"
                 "                   codeSign, timestamp, revocation).\n"
                 "   -C              Set client policy to true. Default is server policy. (ssl, IPSec, eap)\n"
                 "   -d date         Set date and time to use when verifying certificate,\n"
                 "                   provided in the form of YYYY-MM-DD-hh:mm:ss (time optional) in GMT.\n"
                 "                   e.g: 2016-04-25-15:59:59 for April 25, 2016 at 3:59:59 pm in GMT\n"
                 "   -L              Local certs only.\n"
                 "   -n name         Name to be verified. (ssl, IPSec, smime)\n"
                 "   -q              Quiet.\n"
                 "   -R revOption    Perform revocation checking with one of the following options:\n"
                 "                       ocsp     Check revocation status using OCSP method.\n"
                 "                       require  Require a positive response for successful verification.\n"
                 "                       offline  Consult cached responses only (no network requests).\n"
                 "                   Can be specified multiple times; e.g. to check revocation via OCSP\n"
                 "                   and require a positive response, use \"-R ocsp -R require\".\n",
                 "Verify certificate(s).")

SECURITY_COMMAND_IOS("trust-store", trust_store_show_certificates,
                 "[-p][-f][-s][-v][-t][-k]\n"
                 "    -p Output cert in PEM format.\n"
                 "    -f Show fingerprint (SHA1 digest certificate.)\n"
                 "    -s Show subject.\n"
                 "    -v Show entire certificate in text form.\n"
                 "    -t Show trust settings for certificates.\n"
                 "    -k Show keyid (SHA1 digest of public key)",
                 "Display user trust store certificates and trust settings.")

SECURITY_COMMAND("check-trust-update", check_trust_update,
                 "[-s][-e]\n"
                 "    -s Check for Supplementals (Pinning DB and Trusted CT Logs) update\n"
                 "    -e Check for SecExperiment update\n",
                 "Check for data updates for trust and return current version.")

SECURITY_COMMAND("add-ct-exceptions", add_ct_exceptions,
                 "[options]\n"
                 "   -d domain  Domain to add. Can be specified multiple times.\n"
                 "   -c cert    Cert to add. Can be specified multiple times.\n"
                 "   -p plist   plist with exceptions to set (resetting existing).\n"
                 "                 Overrides -d and -c\n"
                 "                 For detailed specification, see SecTrustSettingsPriv.h.\n"
                 "   -r which   Reset exceptions for \"domain\", \"cert\", or \"all\".\n"
                 "                 Overrides -d, -c, and -p\n",
                 "Set exceptions for Certificate Transparency enforcement")

SECURITY_COMMAND("show-ct-exceptions", show_ct_exceptions,
                 "[options]\n"
                 "   -a             Output all combined CT exceptions.\n"
                 "   -i identifier  Output CT exceptions for specified identifier.\n"
                 "                      Default is exceptions for this tool. Overridden by -a.\n"
                 "   -d             Output domain exceptions. Default is both domains and certs.\n"
                 "   -c             Output certificate exceptions (as SPKI hash).\n"
                 "                      Default is both domains and certs.\n",
                 "Display exceptions for Certificate Transparency enforcement in json.")
