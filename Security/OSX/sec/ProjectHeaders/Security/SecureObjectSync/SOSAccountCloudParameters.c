//
//  AccountCloudParameters.c
//  sec
//

#include "SOSAccountPriv.h"
#include <Security/SecureObjectSync/SOSTransportKeyParameter.h>
//
// Cloud Paramters encode/decode
//

static size_t der_sizeof_cloud_parameters(SecKeyRef publicKey, CFDataRef paramters, CFErrorRef* error)
{
    size_t public_key_size = der_sizeof_public_bytes(publicKey, error);
    size_t parameters_size = der_sizeof_data_or_null(paramters, error);
    
    return ccder_sizeof(CCDER_CONSTRUCTED_SEQUENCE, public_key_size + parameters_size);
}

static uint8_t* der_encode_cloud_parameters(SecKeyRef publicKey, CFDataRef paramters, CFErrorRef* error,
                                            const uint8_t* der, uint8_t* der_end)
{
    uint8_t* original_der_end = der_end;
    
    return ccder_encode_constructed_tl(CCDER_CONSTRUCTED_SEQUENCE, original_der_end, der,
                                       der_encode_public_bytes(publicKey, error, der,
                                                               der_encode_data_or_null(paramters, error, der, der_end)));
}

const uint8_t* der_decode_cloud_parameters(CFAllocatorRef allocator,
                                                  CFIndex algorithmID, SecKeyRef* publicKey,
                                                  CFDataRef *parameters,
                                                  CFErrorRef* error,
                                                  const uint8_t* der, const uint8_t* der_end)
{
    const uint8_t *sequence_end;
    der = ccder_decode_sequence_tl(&sequence_end, der, der_end);
    der = der_decode_public_bytes(allocator, algorithmID, publicKey, error, der, sequence_end);
    der = der_decode_data_or_null(allocator, parameters, error, der, sequence_end);
    
    return der;
}


bool SOSAccountPublishCloudParameters(SOSAccountRef account, CFErrorRef* error){
    bool success = false;
    CFIndex cloud_der_len = der_sizeof_cloud_parameters(
                                                        account->user_public,
                                                        account->user_key_parameters,
                                                        error);
    CFMutableDataRef cloudParameters =
    CFDataCreateMutableWithScratch(kCFAllocatorDefault, cloud_der_len);
    
    if (der_encode_cloud_parameters(account->user_public, account->user_key_parameters, error,
                                    CFDataGetMutableBytePtr(cloudParameters),
                                    CFDataGetMutablePastEndPtr(cloudParameters)) != NULL) {

        CFErrorRef changeError = NULL;
        if (SOSTransportKeyParameterPublishCloudParameters(account->key_transport, cloudParameters, error)) {
            success = true;
        } else {
            SOSCreateErrorWithFormat(kSOSErrorSendFailure, changeError, error, NULL,
                                     CFSTR("update parameters key failed [%@]"), cloudParameters);
        }
        CFReleaseSafe(changeError);
    } else {
        SOSCreateError(kSOSErrorEncodeFailure, CFSTR("Encoding parameters failed"), NULL, error);
    }
    
    CFReleaseNull(cloudParameters);
    
    return success;
}

bool SOSAccountRetrieveCloudParameters(SOSAccountRef account, SecKeyRef *newKey,
                                       CFDataRef derparms,
                                       CFDataRef *newParameters, CFErrorRef* error) {
    const uint8_t *parse_end = der_decode_cloud_parameters(kCFAllocatorDefault, kSecECDSAAlgorithmID,
                                                           newKey, newParameters, error,
                                                           CFDataGetBytePtr(derparms), CFDataGetPastEndPtr(derparms));
    
    if (parse_end == CFDataGetPastEndPtr(derparms)) return true;
    return false;
}

