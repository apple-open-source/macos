// This is a preprocessed file to define commands that we provide in Security part of the Sec module.

#include <SecurityTool/security_tool_commands.h>

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
                 "[-v][-a|-D|-u attr=value,...|[-q][-g] attr=value,...] [-d password | -f datafile] [attr=value,...]\n"
                 "-q Query for item matching (default)\n"
                 "-g Get password data\n"
                 "-a Add item to keychain\n"
                 "-u Update item in keychain (require query to match)\n"
                 "-D Delete item from keychain\n"
                 "Add, query, update or delete items from the keychain.  Extra attr=value pairs after options always apply to the query\n"
                 "class=[genp|inet|cert|keys] is required for the query",
                 "Manipulate keychain items.")

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
                 "If no keychains are specified the default search list is used.",
                 "Delete one or more internet password items.")

SECURITY_COMMAND("delete-generic-password", keychain_delete_generic_password,
                 "[-a accountName] [-s serviceName] [keychain...]\n"
                 "    -a Match on \"accountName\" when searching.\n"
                 "    -g Display the password for the item found.\n"
                 "    -s Match on \"serviceName\" when searching.\n"
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

