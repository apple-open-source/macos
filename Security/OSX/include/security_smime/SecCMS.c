/*
 * Copyright (c) 2014 Apple Inc. All Rights Reserved.
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


#include <AssertMacros.h>

#include <Security/SecCmsDecoder.h>
#include <Security/SecCmsMessage.h>
#include <Security/SecCmsContentInfo.h>
#include <Security/SecCmsSignedData.h>
#include <Security/SecCertificate.h>
#include <Security/SecCertificatePriv.h>


#include <SecCMS.h>

/*  Designed to match the sec submodule implementation available for iOS */
CFArrayRef SecCMSCertificatesOnlyMessageCopyCertificates(CFDataRef message) {
    SecCmsMessageRef cmsg = NULL;
    SecCmsContentInfoRef cinfo;
    SecCmsSignedDataRef sigd = NULL;
    CFMutableArrayRef certs = NULL;

    CSSM_DATA encoded_message = { CFDataGetLength(message), (uint8_t*)CFDataGetBytePtr(message) };
    require_noerr_quiet(SecCmsMessageDecode(&encoded_message, NULL, NULL, NULL, NULL, NULL, NULL, &cmsg), out);
    /* expected to be a signed data message at the top level */
    require(cinfo = SecCmsMessageContentLevel(cmsg, 0), out);
    require(SecCmsContentInfoGetContentTypeTag(cinfo) == SEC_OID_PKCS7_SIGNED_DATA, out);
    require(sigd = (SecCmsSignedDataRef)SecCmsContentInfoGetContent(cinfo), out);

    /* find out about signers */
    int nsigners = SecCmsSignedDataSignerInfoCount(sigd);
    require(nsigners == 0, out);

    CSSM_DATA_PTR *cert_datas = SecCmsSignedDataGetCertificateList(sigd);
    certs = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks);
    CSSM_DATA_PTR cert_data;
    if (cert_datas) while ((cert_data = *cert_datas) != NULL) {
        SecCertificateRef cert = SecCertificateCreateWithBytes(NULL, cert_data->Data, cert_data->Length);
        if (cert) {
            CFArrayAppendValue(certs, cert);
            CFRelease(cert);
        }
        cert_datas++;
    }

out:
    if (cmsg)
        SecCmsMessageDestroy(cmsg);

    return certs;
}


CFDataRef SecCMSCreateCertificatesOnlyMessageIAP(SecCertificateRef cert)
{
    static const uint8_t header[] = {
        0x30, 0x82, 0x03, 0x6d, 0x06, 0x09, 0x2a, 0x86,
        0x48, 0x86, 0xf7, 0x0d, 0x01, 0x07, 0x02, 0xa0,
        0x82, 0x03, 0x5e, 0x30, 0x82, 0x03, 0x5a, 0x02,
        0x01, 0x01, 0x31, 0x00, 0x30, 0x0b, 0x06, 0x09,
        0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x07,
        0x01, 0xa0, 0x82, 0x03, 0x40
    };

    static const uint8_t trailer[] = {
        0xa1, 0x00, 0x31, 0x00
    };

    CFMutableDataRef message = NULL;
    CFDataRef certdata;
    const uint8_t *certbytes;
    CFIndex certlen;
    uint8_t *messagebytes;
    uint16_t messagelen;

    certdata = SecCertificateCopyData(cert);
    require(certdata, out);

    certbytes = CFDataGetBytePtr(certdata);
    certlen = CFDataGetLength(certdata);
    require(certlen > UINT8_MAX, out);
    require(certlen < UINT16_MAX, out);

    message = CFDataCreateMutable(kCFAllocatorDefault, 0);
    require(message, out);

    CFDataAppendBytes(message, header, sizeof(header));
    CFDataAppendBytes(message, certbytes, certlen);
    CFDataAppendBytes(message, trailer, sizeof(trailer));

    messagebytes = CFDataGetMutableBytePtr(message);
    messagelen = CFDataGetLength(message);

    messagelen -= 4;
    messagebytes[2] = messagelen >> 8;
    messagebytes[3] = messagelen & 0xFF;

    messagelen -= 15;
    messagebytes[17] = messagelen >> 8;
    messagebytes[18] = messagelen & 0xFF;

    messagelen -= 4;
    messagebytes[21] = messagelen >> 8;
    messagebytes[22] = messagelen & 0xFF;

    messagelen -= 26;
    messagebytes[43] = messagelen >> 8;
    messagebytes[44] = messagelen & 0xFF;

out:
    if (certdata != NULL)
    {
        CFRelease(certdata);
    }
    return message;
}



