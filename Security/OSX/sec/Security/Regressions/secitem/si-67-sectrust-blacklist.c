/*
 * Copyright (c) 2011-2014 Apple Inc. All Rights Reserved.
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


#include <CoreFoundation/CoreFoundation.h>
#include <Security/SecCertificate.h>
#include <Security/SecCertificatePriv.h>
#include <Security/SecPolicyPriv.h>
#include <Security/SecTrust.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <string.h>

#include "si-67-sectrust-blacklist/Global Trustee.cer.h"
#include "si-67-sectrust-blacklist/login.yahoo.com.1.cer.h"
#include "si-67-sectrust-blacklist/UTN-USERFirst-Hardware.cer.h"
#include "si-67-sectrust-blacklist/login.yahoo.com.2.cer.h"
#include "si-67-sectrust-blacklist/addons.mozilla.org.cer.h"
#include "si-67-sectrust-blacklist/login.yahoo.com.cer.h"
#include "si-67-sectrust-blacklist/login.live.com.cer.h"
#include "si-67-sectrust-blacklist/mail.google.com.cer.h"
#include "si-67-sectrust-blacklist/login.skype.com.cer.h"
#include "si-67-sectrust-blacklist/www.google.com.cer.h"

#include "shared_regressions.h"

static void validate_one_cert(uint8_t *data, size_t len, int chain_length, SecTrustResultType trust_result)
{
    SecTrustRef trust;
	SecCertificateRef cert;
    SecPolicyRef policy = SecPolicyCreateSSL(false, NULL);
    CFArrayRef certs;

	isnt(cert = SecCertificateCreateWithBytes(NULL, data, len),
		NULL, "create cert");
    certs = CFArrayCreate(NULL, (const void **)&cert, 1, NULL);
    ok_status(SecTrustCreateWithCertificates(certs, policy, &trust),
        "create trust with single cert");
	//CFDateRef date = CFDateCreate(NULL, 1301008576);
    //ok_status(SecTrustSetVerifyDate(trust, date), "set date");
    //CFRelease(date);

	SecTrustResultType trustResult;
    ok_status(SecTrustEvaluate(trust, &trustResult), "evaluate trust");
	is(SecTrustGetCertificateCount(trust), chain_length, "cert count");
    is_status(trustResult, trust_result, "correct trustResult");
    CFRelease(trust);
    CFRelease(policy);
    CFRelease(certs);
    CFRelease(cert);
}

static void tests(void)
{
    validate_one_cert(Global_Trustee_cer, sizeof(Global_Trustee_cer), 2, kSecTrustResultFatalTrustFailure);
    validate_one_cert(login_yahoo_com_1_cer, sizeof(login_yahoo_com_1_cer), 2, kSecTrustResultFatalTrustFailure);
    /* this is the root, which isn't ok for ssl and fails here, but at the
       same time it proves that kSecTrustResultFatalTrustFailure isn't
       returned for policy failures that aren't blacklisting */
    validate_one_cert(login_yahoo_com_2_cer, sizeof(login_yahoo_com_2_cer), 2, kSecTrustResultFatalTrustFailure);
    validate_one_cert(addons_mozilla_org_cer, sizeof(addons_mozilla_org_cer), 2, kSecTrustResultFatalTrustFailure);
    validate_one_cert(login_yahoo_com_cer, sizeof(login_yahoo_com_cer), 2, kSecTrustResultFatalTrustFailure);
    validate_one_cert(login_live_com_cer, sizeof(login_live_com_cer), 2, kSecTrustResultFatalTrustFailure);
    validate_one_cert(mail_google_com_cer, sizeof(mail_google_com_cer), 2, kSecTrustResultFatalTrustFailure);
    validate_one_cert(login_skype_com_cer, sizeof(login_skype_com_cer), 2, kSecTrustResultFatalTrustFailure);
    validate_one_cert(www_google_com_cer, sizeof(www_google_com_cer), 2, kSecTrustResultFatalTrustFailure);
}

static int ping_host(char *host_name){
    
    struct sockaddr_in pin;
    struct hostent *nlp_host;
    int sd;
    int port;
    int retries = 5;
    
    port=80;
    
    while ((nlp_host=gethostbyname(host_name))==0 && retries--){
        printf("Resolve Error! (%s) %d\n", host_name, h_errno);
        sleep(1);
    }

    if(nlp_host==0)
        return 0;
    
    bzero(&pin,sizeof(pin));
    pin.sin_family=AF_INET;
    pin.sin_addr.s_addr=htonl(INADDR_ANY);
    pin.sin_addr.s_addr=((struct in_addr *)(nlp_host->h_addr))->s_addr;
    pin.sin_port=htons(port);
    
    sd=socket(AF_INET,SOCK_STREAM,0);
    
    if (connect(sd,(struct sockaddr*)&pin,sizeof(pin))==-1){
        printf("connect error! (%s) %d\n", host_name, errno);
        close(sd);
        return 0;
    }
    else{
        close(sd);
        return 1;
    }
}
 
int si_67_sectrust_blacklist(int argc, char *const *argv)
{
    char *hosts[] = {
        "EVSecure-ocsp.verisign.com",
        "EVIntl-ocsp.verisign.com",
        "EVIntl-aia.verisign.com",
        "ocsp.comodoca.com",
        "crt.comodoca.com",
    };
    
    unsigned host_cnt = 0;

    plan_tests(45);

    for (host_cnt = 0; host_cnt < sizeof(hosts)/sizeof(hosts[0]); host_cnt ++)
        if(ping_host(hosts[host_cnt]) == 0){
            printf("Accessing specific server (%s) failed, check the network!\n", hosts[host_cnt]);
            return 0;
        }

	tests();

	return 0;
}
