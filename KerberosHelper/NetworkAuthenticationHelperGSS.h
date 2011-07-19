
#import <GSS/gssapi.h>

/*
 * GSS-API Support
 */

gss_cred_id_t
NAHSelectionGetGSSCredential(NAHSelectionRef client);

gss_name_t
NAHSelectionGetGSSAcceptorName(NAHSelectionRef client);

gss_OID
NAHSelectionGetGSSMech(NAHSelectionRef client);

