
#import <KerberosHelper/NetworkAuthenticationHelper.h>
#import <GSS/gssapi.h>

/*
 * GSS-API Support
 */

gss_cred_id_t
NAHSelectionGetGSSCredential(NAHSelectionRef client, CFErrorRef *error);

gss_name_t
NAHSelectionGetGSSAcceptorName(NAHSelectionRef selection, CFErrorRef *error);

gss_OID
NAHSelectionGetGSSMech(NAHSelectionRef client);

/*
 * Turn the AuthenticationInfo dict into something useful
 */

gss_cred_id_t
NAHAuthenticationInfoCopyClientCredential(CFDictionaryRef authInfo, CFErrorRef *error);

gss_name_t
NAHAuthenticationInfoCopyServerName(CFDictionaryRef authInfo, CFErrorRef *error);

gss_OID
NAHAuthenticationInfoGetGSSMechanism(CFDictionaryRef authInfo, CFErrorRef *error);

