
#include <dce/rpc.h>

#include <stdio.h>
#include <iconv.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#import "ms-icp.h"

#import <Security/Security.h>


heim_data_t
request_certificate(heim_data_t pkcs10_csr,
		    heim_string_t server_name,
		    heim_string_t ca_name,
		    heim_string_t cert_template,
		    heim_error_t *error)
{
    CERTTRANSBLOB pctbCert;
    CERTTRANSBLOB pctbEncodedCert;
    CERTTRANSBLOB pctbDispositionMessage;

    uint32_t pdwDisposition=0x55667788;
    CERTTRANSBLOB pctbAttribs;
    CERTTRANSBLOB pctbRequest;
    uint32_t pdwRequestId;

    const char * protocol_family = "ncacn_ip_tcp";
    char *partial_string_binding = NULL;
    error_status_t status;
    unsigned32 authn_protocol = rpc_c_authn_gss_mskrb;
    unsigned32 authn_level = rpc_c_protect_level_connect;
    rpc_binding_handle_t binding_handle;

    heim_data_t certificate = NULL;

    char *sn;


    sn = heim_string_copy_utf8(servername);

    asprintf(&partial_string_binding, "%s:%s[]", protocol_family, sn);
    free(sn);
    
    rpc_binding_from_string_binding((unsigned char *)partial_string_binding,
                                    &binding_handle,
                                    &status);
    free(partial_string_binding);

    if (status)
	return NULL;

    asprintf(&sn, "host/%s", servername);

    unsigned_char_t *server_princ_name=malloc(1024);
    sprintf((char *)server_princ_name,"host/%s",servername);
    unsigned32 princstatus;
    

    
    rpc_ep_resolve_binding(binding_handle,
                           ICertPassage_v0_0_c_ifspec,
                           &status);

    rpc_binding_set_auth_info(binding_handle,
                              (unsigned_char_p_t)sn,
                              authn_level,
                              authn_protocol,
                              NULL,
                              rpc_c_authz_name,
                              &status);
    free(fn);
    if (status)
	return NULL;
        
    uint32_t dwFlags = 0xFF;
    int outlength;
    heim_data_t pwszAuthority;

    pwszAuthority = heim_string_copy_utf16(ca_name);
    
    pctbRequest.pb = rk_UNCONST(heim_data_get_bytes(pkcs10));
    pctbRequest.cb = heim_data_get_length(pkcs10);
    
    heim_string_t c_attributes = heim_string_create_with_format("CertificateTemplate:%@", cert_template);

    heim_data_t c_attrs_utf16 = heim_string_copy_utf16(c_attributes);

    pctbAttribs.pb = rk_UNCONST(heim_data_get_bytes(c_attrs_utf16));
    pctbAttribs.cb = heim_data_get_length(c_attrs_utf16);


    uint32_t outstatus = CertServerRequest(binding_handle,
					   dwFlags,
					   (unsigned short *)pwszAuthority,
					   &pdwRequestId,
					   &pdwDisposition,
					   &pctbAttribs,
					   &pctbRequest,
					   &pctbCert,
					   &pctbEncodedCert,
					   &pctbDispositionMessage);
    
    if (outstatus)
	return NULL;

    if (pdwRequestId > 0) {
        if (pctbEncodedCert.cb==0) {
            // printf("Failed.  Check Failed Requests with request ID %i in the CA for the reason why\n",pdwRequestId);
	} else {
	    certificate = heim_data_create(pctbEncodedCert.pb, pctbEncodedCert.cb);
	}
    } else {
	// XXXX
    }

    return certificate;
}
