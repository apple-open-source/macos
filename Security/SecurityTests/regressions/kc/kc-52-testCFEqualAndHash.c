#include <stdlib.h>
#include <CoreFoundation/CoreFoundation.h>
#include <Security/Security.h>

#include "testmore.h"
#include "testenv.h"

/* ==========================================================================
	This test is to ensure we do not regress the fix for radar
	<rdar://problem/9583502> Security CF runtime objects do not implement CF's Hash function
   ========================================================================== */

static void tests(void)
{
	CFDictionaryRef query = CFDictionaryCreate(NULL, (const void **)&kSecClass, (const void **)&kSecClassCertificate, 1, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    ok_status(NULL != query, "Dictionary Creation"); // 1
    
    CFTypeRef result = NULL;
    OSStatus err = SecItemCopyMatching(query, &result);
    ok_status(noErr == err && NULL != result, "SecItemCopyMatching"); // 2

    CFRelease(query);
    
    
    SecCertificateRef cert = (SecCertificateRef)result;
    
    
    CFDataRef cert_data = SecCertificateCopyData(cert);
    ok_status(NULL != cert_data, "SecCertificateCopyData"); // 3   
    
    SecCertificateRef certs[5];
    certs[0] = cert;
    
    for (int iCnt = 1; iCnt < 5; iCnt++)
    {
        cert = NULL;
        cert = SecCertificateCreateWithData(NULL, cert_data);
        ok_status(NULL != cert_data, "SecCertificateCreateWithData"); // 4 5 6 7
        certs[iCnt] = cert;
    }
    
    CFSetRef aSet = CFSetCreate(NULL, (const void **)certs, 4, &kCFTypeSetCallBacks);
    ok_status(NULL != aSet, "CFSetCreate"); // 8

    
    CFIndex count = CFSetGetCount(aSet);
    ok_status(count == 1, "CFSetGetCount"); // 9
    
    
    for (int iCnt = 0; iCnt < 5; iCnt++)
    {
        cert = certs[iCnt];
        if (NULL != cert)
        {
            CFRelease(cert);
            certs[iCnt] = NULL;
        }
    }

}

int main(int argc, char *const *argv)
{
	plan_tests(9);
	if (!tests_begin(argc, argv))
		BAIL_OUT("tests_begin failed");

	tests();

	//ok_leaks("leaks");

	return 0;
}