//
//  secd-81-item-match-policy.m
//  sec

/*
 * This is to fool os services to not provide the Keychain manager
 * interface tht doens't work since we don't have unified headers
 * between iOS and OS X. rdar://23405418/
 */
#define __KEYCHAINCORE__ 1

#import <Foundation/Foundation.h>
#import <Security/SecCertificate.h>
#import <Security/SecItem.h>
#import <Security/SecBase.h>
#import <utilities/SecCFWrappers.h>


#import "secd_regressions.h"
#import "SecdTestKeychainUtilities.h"
#import "secd-83-item-match.h"

//Test SSL SMIME2
NSString *secdTestSMIME1BASE64String = @"MIIDUzCCAjugAwIBAgIBATANBgkqhkiG9w0BAQsFADBHMRQwEgYDVQQDDAtUZXN0IFNNSU1FMTELMAkGA1UEBhMCQ1oxIjAgBgkqhkiG9w0BCQEWE3Rlc3RjZXJ0MUBhcHBsZS5jb20wHhcNMTYwNDA3MDYzNDM0WhcNMTcwNDA3MDYzNDM0WjBHMRQwEgYDVQQDDAtUZXN0IFNNSU1FMTELMAkGA1UEBhMCQ1oxIjAgBgkqhkiG9w0BCQEWE3Rlc3RjZXJ0MUBhcHBsZS5jb20wggEiMA0GCSqGSIb3DQEBAQUAA4IBDwAwggEKAoIBAQDHedMsbVecoqyZwoTTE85bm3QImVz45HYFGKq/L6E5dVPamiiMRZ2BX1XnIdthbIRZhg7OU0zk2KiQKyg+bjnk+l3ZW/C6N3sAx4blq/UemOg6XtdI1Iq/HwLw5qJW8hiy9INA2nMJnSXZrnM6Ezsj92l0PQ2+8oOa95sLfi2e9NJZONESY/XNyc6tclaUYHSQR+BAzllKRorrqvpa3T2o6lmcUG+29TsRUHth6OHZLccMoA6ITk8Eq/vlAsaH4BnD7rFfBwcE/GhkkbZrmH1xaFziEIwMx3uvhsQCpspt50Pf9RWcsj3/bD4aIPUNLRkyn49ZE6MhOmMp7p5UCXapAgMBAAGjSjBIMA4GA1UdDwEB/wQEAwIHgDAWBgNVHSUBAf8EDDAKBggrBgEFBQcDBDAeBgNVHREEFzAVgRN0ZXN0Y2VydDFAYXBwbGUuY29tMA0GCSqGSIb3DQEBCwUAA4IBAQAm3pbBBqrzR3tTwZrjLORH72PY3f/4IvhYNadP3A70Smln0xC+BIjxqTPP1YXcvwY2PfIaNkXrxaqM8/mbR5MjiDQ7SZz5jfYTuJP6+Z9Y9nL2S/62DyhVwJcRskrDCArkaXz1b2n6yEWCw4L5uJmwdRruW5D7mF10f2GtNvGlq5Wj7csKjw6Pwh8kgG3lnGqrWaNfiJku49lbAugZr2bryQvzxi3q0Kk0LBlxVt1fMae/H/cO9nwlqe+mOclJ96zRPWnXKNMduDf00OIp131gqAknXQwSJZoI2mvX9deFdaaS21T1COhvPN+dL7D5umX85NGzoLN2svzihAhMkIK+";
//Test SSL SMIME2
NSString *secdTestSMIME2BASE64String = @"MIIDUzCCAjugAwIBAgIBAjANBgkqhkiG9w0BAQsFADBHMRQwEgYDVQQDDAtUZXN0IFNNSU1FMjELMAkGA1UEBhMCQ1oxIjAgBgkqhkiG9w0BCQEWE3Rlc3RjZXJ0MkBhcHBsZS5jb20wHhcNMTYwNDA3MDYzNjUzWhcNMTcwNDA3MDYzNjUzWjBHMRQwEgYDVQQDDAtUZXN0IFNNSU1FMjELMAkGA1UEBhMCQ1oxIjAgBgkqhkiG9w0BCQEWE3Rlc3RjZXJ0MkBhcHBsZS5jb20wggEiMA0GCSqGSIb3DQEBAQUAA4IBDwAwggEKAoIBAQDk4emo4RNYI7WXtdOjFBgxIaLf80zTH0peza4PMJO7fC+KtStC4KpvRWbIhm+CcvJjpT4jpScKuHisMgfPxfEcK7jv8kRU7waAaW3H5o56YBX+Qen+fA18ulrwwzEHO4KBK7kXT9oSLy1m09u17yf46x3NPshrg0JpTh4F1jPtEK7HPZnzoJMx8i69Rd9zfddm+Gs0TPFtqaUQ9wuKeKr/Vda9kaPo98UabpiXx94sy/rtlULiUY8Nh993KUDcGPI2LTmQSeR8EB37AEoeBm8mRR9IGdZNvkDPEXIoilw1HVMsczxCEVV0HOAcFh+HfvVsWGYySentCpOe95r4/CSbAgMBAAGjSjBIMA4GA1UdDwEB/wQEAwIHgDAWBgNVHSUBAf8EDDAKBggrBgEFBQcDBDAeBgNVHREEFzAVgRN0ZXN0Y2VydDJAYXBwbGUuY29tMA0GCSqGSIb3DQEBCwUAA4IBAQAAVic5KfpUoy7Bu8dZ8qc+1RM/wXscK/2906/ZD7cndFSM2X7x+oRB78mtbmHlvfDC5FC5OAlxpY9S5ITtK/R67wi6LwOK3TkoQzevSqAkl8ylTfh3Sp/bwCRpVJo1yFCBdNnq4clEktu2+W6YSczjrDieN6CvhRE+AQ3sYNpxeaRJUS8PtNdPmYNnPdLK9VLB8Fg2WIHTAYV1nvai8Dvj1Fk6ukJGQxLJmXs6qUT8O+VE0CiVI5wspX+XSO/FSXiYLzFsLSRQNdWOO6U0zJlVpa1UoiRZCiVHnkDcYAMKB66hNUvRPtZYAguH7Ipg5bSld6fr8b43Vt2lkDM1iZAK";
//Test SSL client1
NSString *secdTestSSLClient1BASE64String = @"MIIDPTCCAiWgAwIBAgIBAzANBgkqhkiG9w0BAQsFADBMMRkwFwYDVQQDDBBUZXN0IFNTTCBjbGllbnQxMQswCQYDVQQGEwJDWjEiMCAGCSqGSIb3DQEJARYTdGVzdGNlcnQzQGFwcGxlLmNvbTAeFw0xNjA0MDcwNjM5NDBaFw0xNzA0MDcwNjM5NDBaMEwxGTAXBgNVBAMMEFRlc3QgU1NMIGNsaWVudDExCzAJBgNVBAYTAkNaMSIwIAYJKoZIhvcNAQkBFhN0ZXN0Y2VydDNAYXBwbGUuY29tMIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEAvKF0N/zPBmahsQwcSrCqiUavXVLj5DEVricqEmOPd77Zr3CVf4B/zDEFqyLh2cg15WBlYV0hG8TDa49yOlJ8JZ8d58n/0A0p6Qq27/S1qc7DvpwVqSwwUl5S3DXSrVBxnJKz20Q3f2yNnHv3hJvp7dMI4C+rQK/uSy2syJalTSVUXMLjQegfv3EVBVTL8omb4Myo7HbjYhEUw/faM9NXwUNs0foR2cRvpkqJPi7r6gM/a+4T2SlSst0U+ByPgaFy2QJPPuzQSEa7vA5fmsMDSQ7BHnla1YVjloWh4LHdB32BGmjIwyHYfn5hqKl37V+oASqLeWVD1cK/2dG6nl2AmQIDAQABoyowKDAOBgNVHQ8BAf8EBAMCB4AwFgYDVR0lAQH/BAwwCgYIKwYBBQUHAwIwDQYJKoZIhvcNAQELBQADggEBAEIs0r+XYOl2Tt1dcRVgTXoFT8w4xAJ7JeErhBqEbhPnQ4i/7GGnd20wL3keCLeLvIJm+KOI+OMjLft276MGn6VEN33o8GKt53m0Mz/mS36xdmAdxBu0t1dh2bnBRYh+asD2gz3AFnlUDOC5DqRZ5yz0FwK9UPpN6KqBZc0YbzS/p/cc3UkzywdAX01RTzmxqGgrl5CPruIECRSieykl7fgmqitlLnYms/QvaS963cFvIM6XJdXltbED6GkVc39ab/lXFYfkSC4Wx2LGeP5NEBqeEfLpKHFOuNVo6dks8OGXXfW14jhCRaLJi6P3kh9jfTXAvW8Fw0mGoNciTLPKTK4=";
//Test SSL client2
NSString *secdTestSSLClient2BASE64String = @"MIIDPTCCAiWgAwIBAgIBBDANBgkqhkiG9w0BAQsFADBMMRkwFwYDVQQDDBBUZXN0IFNTTCBjbGllbnQyMQswCQYDVQQGEwJDWjEiMCAGCSqGSIb3DQEJARYTdGVzdGNlcnQ0QGFwcGxlLmNvbTAeFw0xNjA0MDcwNjQxMDFaFw0xNzA0MDcwNjQxMDFaMEwxGTAXBgNVBAMMEFRlc3QgU1NMIGNsaWVudDIxCzAJBgNVBAYTAkNaMSIwIAYJKoZIhvcNAQkBFhN0ZXN0Y2VydDRAYXBwbGUuY29tMIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEAneZGoEDMw7ZdqT5jVA+omajFDixAd9HB5kaw1kUxd6MuFTn95WhLyI84oa8FvmbxGXZ2zBxtZF3hrm2xHPJB0bwpCfzR0GlaoZKbgUzvRx8x+i/+tEkbFnlOlmU1ZW7cGWyu6bI60rfPZ5VwEe6/JgFaKTRdOACxzIaYDJhS2S2LSzgrom27k424LbCWtfwaqEuiPtF3hGmuCAfrdxJ9Pr/qBcKCjkbWdrtIYhsEJzN8oLwdLduy8jM+fiAXwW1skEgIQa7Bo4zJRIsT6fG0U/0HeSESxGfghN3dBCaWGM7yB1Cahrk8pfalhs5IFmFhNy/+UtpBYhO1DGbr4ijfHQIDAQABoyowKDAOBgNVHQ8BAf8EBAMCB4AwFgYDVR0lAQH/BAwwCgYIKwYBBQUHAwIwDQYJKoZIhvcNAQELBQADggEBAFrDD3o6uG8lU1NxLa+i0zSihFDv0b3y516AScUAf6jpuW0Wfhpqb/U1Gk91Zo2ZBFtQ5WniLf4wGZRFeeUOv6PTHdkBz7PtYa/nzyXOlUBdTuZ0w52e2+mEJfCpwhR7MVBvs3QmtXpg7H/lSePbbun/tl8fYRMzcycLWH9Yrz+TluNdS9/Qa3SRgXo4JUKGl8F/nvXohuz2JyGLk43l3Cq9z68IKv7c8SA448E9d+MwwsYc2YcHf9aZAwu81aP5c3XOSaWvP3uzATxb/w6tIhs7O8iHgyq7mRy2iQqACDxpcILKGEu4advaBPmB14kSz/HUS4VahafayXxhCQPqYtc=";
//secdtest1.apple.com
NSString *secdTestSSLServer1BASE64String = @"MIIDYzCCAkugAwIBAgIBBTANBgkqhkiG9w0BAQsFADBPMRwwGgYDVQQDDBNzZWNkdGVzdDEuYXBwbGUuY29tMQswCQYDVQQGEwJDWjEiMCAGCSqGSIb3DQEJARYTdGVzdGNlcnQ1QGFwcGxlLmNvbTAeFw0xNjA0MDcwNjQ3MThaFw0xNzA0MDcwNjQ3MThaME8xHDAaBgNVBAMME3NlY2R0ZXN0MS5hcHBsZS5jb20xCzAJBgNVBAYTAkNaMSIwIAYJKoZIhvcNAQkBFhN0ZXN0Y2VydDVAYXBwbGUuY29tMIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEAxWOhlJjFJ+Fyp0KRCb4Dx20TMf+/gYaNwN9/i0+YNi3wb5mAepO7gN8VoZhYf+fWO6eUGzxFwggCK5b2plZ5dW/3su5B/K5onYxE7wWPsHGfw/rolpkp84fgj2aSUBQnOuwhFvot1dmFUeh55SaIv56sLw5aIbW/xWP6Mhc8kpf8ji5xpFA5JZxmbBZi4iG4E4395DD3lXE1jN4B3aY6gknnA6BYvngvxH/2whitKTDKCqsnWPxGqbJ5kg+0julkgYVEPlfdus/MNTB/c6llKiqIkwNuzPPaHq9VRNnPctEljVJcch7ZwqbluTY+AwRGXuY0RpJ9S6+uEPaQbuDX3QIDAQABo0owSDAOBgNVHQ8BAf8EBAMCB4AwFgYDVR0lAQH/BAwwCgYIKwYBBQUHAwEwHgYDVR0RBBcwFYITc2VjZHRlc3QxLmFwcGxlLmNvbTANBgkqhkiG9w0BAQsFAAOCAQEAfJqxsLI03nm9YSVeOqcyOB+Cj41WmNCgaSvftGmPtmz4QLbEYTYDqrv2zTvTtmS5Z1ugy6XeA4sUx9j9oKJq9+FRkfxQTDOHz7e8dqUvF0ToLPRo0dlGv55FsbVgOM8vKCeqra12FWvSUHXhUn7tC+lQDDiDs5st4NkVRgRsCYHUIfYtYBWABd5Z6kWAR33qysxbxx4cHLGb/1CCfsPF+/IQYS1sF9u+q/Jvbe6Oylyb1qxoe4dcsub4AYgS+yHItcQeZksItwYWLdAQUat7r6bbMLp+EN2DJRzrIQl+Kf3nzSKRBW6HTJR19+6D/pum0q8A0A3chsMW34rvMS469w==";
//secdtest2.apple.com
NSString *secdTestSSLServer2BASE64String = @"MIIDYzCCAkugAwIBAgIBBjANBgkqhkiG9w0BAQsFADBPMRwwGgYDVQQDDBNzZWNkdGVzdDIuYXBwbGUuY29tMQswCQYDVQQGEwJDWjEiMCAGCSqGSIb3DQEJARYTdGVzdGNlcnQ2QGFwcGxlLmNvbTAeFw0xNjA0MDcwNjQ1NTFaFw0xNzA0MDcwNjQ1NTFaME8xHDAaBgNVBAMME3NlY2R0ZXN0Mi5hcHBsZS5jb20xCzAJBgNVBAYTAkNaMSIwIAYJKoZIhvcNAQkBFhN0ZXN0Y2VydDZAYXBwbGUuY29tMIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEAmI95AUYia7UATWdqAUHQGge1vK11u5CXuQm0rpvKMTwzSD/HkEX6jFnCoJ4J+3FUpeY70ZhWfnwiumGmMB6RfI3S2jIWvyrMgIRkiPhiLh5ZDmV6K/w2MVzOEiPRKPVcxgLJm8CF2/EdJnCMmJG8pvWyTwahW43WT7oAj5KdnqSCtysEZ5pOKR+U4S89x0mUuGXc6K3xVWSfM0Az2tepWc11dtuLWPSe5vCU3JuZzFfXsUqgHInWnBNjPfQrgI9LE5EIqslA5dAZLLlr4+OLCmENi6qwQ6GIvzR9S30gAk4Mo/H+RUeqqimkMD8JUL9D72FQdqcC1cFgsADbxslLgwIDAQABo0owSDAOBgNVHQ8BAf8EBAMCB4AwFgYDVR0lAQH/BAwwCgYIKwYBBQUHAwEwHgYDVR0RBBcwFYITc2VjZHRlc3QyLmFwcGxlLmNvbTANBgkqhkiG9w0BAQsFAAOCAQEAC3BDk+HtpO+FKmgx0BfNz4gXR3NjDoe0Ms2toC2YAzR33Z/VycBjLbQ0gfQHyGHYQPYzGgurYPypyzUqKJJVFClmj/VXaNANiFhegHPXRKPyFuw5wbxKE2tgCq3sJ+x4RbDoyGXZz+7bfvWbjynpgiXWWx1V1ABop1UByiYTWp7zVDLTEzYfVGkisr0sV3qoMrKxYBgjUJjXM6p5DeIFr8HaB6lSSqSlCek3oMBfgjEIurpU3LhcGeOn2ItFS8F3wj1YqLvxzgzn3LfPjUOENXI+Fy8lPgibiEeqAcT7//NwleuNQfYL5eGVzuAxcNG9b1NeDkG5t1RQgUL5JwP8bg==";

void addTestCertificates(void) {
    NSData *certDerData = [[NSData alloc] initWithBase64EncodedString:secdTestSMIME1BASE64String options:NSDataBase64DecodingIgnoreUnknownCharacters];
    SecCertificateRef certRef = SecCertificateCreateWithData(kCFAllocatorDefault, (__bridge CFDataRef)certDerData);
    ok_status(SecItemAdd((__bridge CFDictionaryRef) @{ (id)kSecValueRef : (__bridge id) certRef }, NULL), "Add tet certificate");
    CFRelease(certRef);

    certDerData = [[NSData alloc] initWithBase64EncodedString:secdTestSMIME2BASE64String options:NSDataBase64DecodingIgnoreUnknownCharacters];
    certRef = SecCertificateCreateWithData(kCFAllocatorDefault, (__bridge CFDataRef)certDerData);
    ok_status(SecItemAdd((__bridge CFDictionaryRef) @{ (id)kSecValueRef : (__bridge id) certRef }, NULL), "Add tet certificate");
    CFRelease(certRef);

    certDerData = [[NSData alloc] initWithBase64EncodedString:secdTestSSLClient1BASE64String options:NSDataBase64DecodingIgnoreUnknownCharacters];
    certRef = SecCertificateCreateWithData(kCFAllocatorDefault, (__bridge CFDataRef)certDerData);
    ok_status(SecItemAdd((__bridge CFDictionaryRef) @{ (id)kSecValueRef : (__bridge id) certRef }, NULL), "Add tet certificate");
    CFRelease(certRef);

    certDerData = [[NSData alloc] initWithBase64EncodedString:secdTestSSLClient2BASE64String options:NSDataBase64DecodingIgnoreUnknownCharacters];
    certRef = SecCertificateCreateWithData(kCFAllocatorDefault, (__bridge CFDataRef)certDerData);
    ok_status(SecItemAdd((__bridge CFDictionaryRef) @{ (id)kSecValueRef : (__bridge id) certRef }, NULL), "Add tet certificate");
    CFRelease(certRef);

    certDerData = [[NSData alloc] initWithBase64EncodedString:secdTestSSLServer1BASE64String options:NSDataBase64DecodingIgnoreUnknownCharacters];
    certRef = SecCertificateCreateWithData(kCFAllocatorDefault, (__bridge CFDataRef)certDerData);
    ok_status(SecItemAdd((__bridge CFDictionaryRef) @{ (id)kSecValueRef : (__bridge id) certRef }, NULL), "Add tet certificate");
    CFRelease(certRef);

    certDerData = [[NSData alloc] initWithBase64EncodedString:secdTestSSLServer2BASE64String options:NSDataBase64DecodingIgnoreUnknownCharacters];
    certRef = SecCertificateCreateWithData(kCFAllocatorDefault, (__bridge CFDataRef)certDerData);
    ok_status(SecItemAdd((__bridge CFDictionaryRef) @{ (id)kSecValueRef : (__bridge id) certRef }, NULL), "Add tet certificate");
    CFRelease(certRef);
}

static void test(id returnKeyName) {
    NSDateFormatter *dateFormatter = [[NSDateFormatter alloc] init];
    [dateFormatter setDateFormat:@"yyyy-MM-dd HH:mm:ss zzz"];
    [dateFormatter setLocale:[[NSLocale alloc] initWithLocaleIdentifier:@"us_EN"]];
    NSDate *validDate = [dateFormatter dateFromString: @"2016-04-07 16:00:00 GMT"];
    NSDate *dateBefore = [dateFormatter dateFromString: @"2016-04-06 16:00:00 GMT"];
    NSDate *dateAfter = [dateFormatter dateFromString: @"2017-04-08 16:00:00 GMT"];

    CFTypeRef result = NULL;
    ok_status(SecItemCopyMatching( (__bridge CFDictionaryRef)@{ (id)kSecClass : (id)kSecClassCertificate,
                                                                (id)kSecMatchLimit : (id)kSecMatchLimitAll,
                                                                returnKeyName : @YES }, &result));
    ok(result && CFArrayGetCount(result) == 6);
    CFReleaseNull(result);
#if TARGET_OS_IPHONE
    SecPolicyRef policy = SecPolicyCreateWithProperties(kSecPolicyAppleSMIME, NULL);
#else
    SecPolicyRef policy = SecPolicyCreateWithProperties(kSecPolicyAppleSMIME, (__bridge CFDictionaryRef)@{ (id)kSecPolicyKU_DigitalSignature : @YES });
#endif
    ok_status(SecItemCopyMatching( (__bridge CFDictionaryRef)@{ (id)kSecClass : (id)kSecClassCertificate,
                                                                (id)kSecMatchLimit : (id)kSecMatchLimitAll,
                                                                (id)kSecMatchPolicy : (__bridge id)policy,
                                                                returnKeyName : @YES }, &result));
    ok(result && CFArrayGetCount(result) == 2);
    CFReleaseNull(policy);
    CFReleaseNull(result);

#if TARGET_OS_IPHONE
    policy = SecPolicyCreateWithProperties(kSecPolicyAppleSMIME, (__bridge CFDictionaryRef)@{
#else
    policy = SecPolicyCreateWithProperties(kSecPolicyAppleSMIME, (__bridge CFDictionaryRef)@{ (id)kSecPolicyKU_DigitalSignature : @YES,
#endif
                                                                                              (id)kSecPolicyName : @"testcert1@apple.com" });
    ok_status(SecItemCopyMatching( (__bridge CFDictionaryRef)@{ (id)kSecClass : (id)kSecClassCertificate,
                                                                (id)kSecMatchLimit : (id)kSecMatchLimitAll,
                                                                (id)kSecMatchPolicy : (__bridge id)policy,
                                                                returnKeyName : @YES }, &result));
    ok(result && CFArrayGetCount(result) == 1);
    CFReleaseNull(result);

    ok_status(SecItemCopyMatching( (__bridge CFDictionaryRef)@{ (id)kSecClass : (id)kSecClassCertificate,
                                                                (id)kSecMatchLimit : (id)kSecMatchLimitAll,
                                                                (id)kSecMatchPolicy : (__bridge id)policy,
                                                                (id)kSecMatchValidOnDate : validDate,
                                                                returnKeyName : @YES }, &result));
    ok(result && CFArrayGetCount(result) == 1);
    CFReleaseNull(result);

    is_status(SecItemCopyMatching( (__bridge CFDictionaryRef)@{ (id)kSecClass : (id)kSecClassCertificate,
                                                                (id)kSecMatchLimit : (id)kSecMatchLimitAll,
                                                                (id)kSecMatchPolicy : (__bridge id)policy,
                                                                (id)kSecMatchValidOnDate : dateBefore,
                                                                returnKeyName : @YES }, &result), errSecItemNotFound);
    CFReleaseNull(result);

    is_status(SecItemCopyMatching( (__bridge CFDictionaryRef)@{ (id)kSecClass : (id)kSecClassCertificate,
                                                                (id)kSecMatchLimit : (id)kSecMatchLimitAll,
                                                                (id)kSecMatchPolicy : (__bridge id)policy,
                                                                (id)kSecMatchValidOnDate : dateAfter,
                                                                returnKeyName : @YES }, &result), errSecItemNotFound);
    CFReleaseNull(policy);
    CFReleaseNull(result);
#if TARGET_OS_IPHONE
    policy = SecPolicyCreateWithProperties(kSecPolicyAppleSSL, NULL);
#else
    policy = SecPolicyCreateWithProperties(kSecPolicyAppleSSL, (__bridge CFDictionaryRef)@{ (id)kSecPolicyKU_DigitalSignature : @YES });
#endif
    ok_status(SecItemCopyMatching( (__bridge CFDictionaryRef)@{ (id)kSecClass : (id)kSecClassCertificate,
                                                                (id)kSecMatchLimit : (id)kSecMatchLimitAll,
                                                                (id)kSecMatchPolicy : (__bridge id)policy,
                                                                returnKeyName : @YES }, &result));
    ok(result && CFArrayGetCount(result) == 2);
    CFReleaseNull(policy);
    CFReleaseNull(result);

#if TARGET_OS_IPHONE
    policy = SecPolicyCreateWithProperties(kSecPolicyAppleSSL, (__bridge CFDictionaryRef)@{
#else
    policy = SecPolicyCreateWithProperties(kSecPolicyAppleSSL, (__bridge CFDictionaryRef)@{ (id)kSecPolicyKU_DigitalSignature : @YES,
#endif
                                                                                              (id)kSecPolicyName : @"secdtest1.apple.com" });
    ok_status(SecItemCopyMatching( (__bridge CFDictionaryRef)@{ (id)kSecClass : (id)kSecClassCertificate,
                                                                (id)kSecMatchLimit : (id)kSecMatchLimitAll,
                                                                (id)kSecMatchPolicy : (__bridge id)policy,
                                                                returnKeyName : @YES }, &result));
    ok(result && CFArrayGetCount(result) == 1);
    CFReleaseNull(result);

    ok_status(SecItemCopyMatching( (__bridge CFDictionaryRef)@{ (id)kSecClass : (id)kSecClassCertificate,
                                                                (id)kSecMatchLimit : (id)kSecMatchLimitAll,
                                                                (id)kSecMatchPolicy : (__bridge id)policy,
                                                                (id)kSecMatchValidOnDate : validDate,
                                                                returnKeyName : @YES }, &result));
    ok(result && CFArrayGetCount(result) == 1);
    CFReleaseNull(result);

    is_status(SecItemCopyMatching( (__bridge CFDictionaryRef)@{ (id)kSecClass : (id)kSecClassCertificate,
                                                                (id)kSecMatchLimit : (id)kSecMatchLimitAll,
                                                                (id)kSecMatchPolicy : (__bridge id)policy,
                                                                (id)kSecMatchValidOnDate : dateBefore,
                                                                returnKeyName : @YES }, &result), errSecItemNotFound);
    CFReleaseNull(result);

    is_status(SecItemCopyMatching( (__bridge CFDictionaryRef)@{ (id)kSecClass : (id)kSecClassCertificate,
                                                                (id)kSecMatchLimit : (id)kSecMatchLimitAll,
                                                                (id)kSecMatchPolicy : (__bridge id)policy,
                                                                (id)kSecMatchValidOnDate : dateAfter,
                                                                returnKeyName : @YES }, &result), errSecItemNotFound);
    CFReleaseNull(policy);
    CFReleaseNull(result);

#if TARGET_OS_IPHONE
    policy = SecPolicyCreateWithProperties(kSecPolicyAppleSSL, (__bridge CFDictionaryRef)@{
#else
    policy = SecPolicyCreateWithProperties(kSecPolicyAppleSSL, (__bridge CFDictionaryRef)@{ (id)kSecPolicyKU_DigitalSignature : @YES,
#endif
                                                                                            (id)kSecPolicyClient : @YES });
    ok_status(SecItemCopyMatching( (__bridge CFDictionaryRef)@{ (id)kSecClass : (id)kSecClassCertificate,
                                                                (id)kSecMatchLimit : (id)kSecMatchLimitAll,
                                                                (id)kSecMatchPolicy : (__bridge id)policy,
                                                                returnKeyName : @YES }, &result));
    ok(result && CFArrayGetCount(result) == 2);
    CFReleaseNull(result);

    ok_status(SecItemCopyMatching( (__bridge CFDictionaryRef)@{ (id)kSecClass : (id)kSecClassCertificate,
                                                                (id)kSecMatchLimit : (id)kSecMatchLimitAll,
                                                                (id)kSecMatchPolicy : (__bridge id)policy,
                                                                (id)kSecMatchValidOnDate : validDate,
                                                                returnKeyName : @YES }, &result));
    ok(result && CFArrayGetCount(result) == 2);
    CFReleaseNull(result);

    is_status(SecItemCopyMatching( (__bridge CFDictionaryRef)@{ (id)kSecClass : (id)kSecClassCertificate,
                                                                (id)kSecMatchLimit : (id)kSecMatchLimitAll,
                                                                (id)kSecMatchPolicy : (__bridge id)policy,
                                                                (id)kSecMatchValidOnDate : dateBefore,
                                                                returnKeyName : @YES }, &result), errSecItemNotFound);
    CFReleaseNull(result);

    is_status(SecItemCopyMatching( (__bridge CFDictionaryRef)@{ (id)kSecClass : (id)kSecClassCertificate,
                                                                (id)kSecMatchLimit : (id)kSecMatchLimitAll,
                                                                (id)kSecMatchPolicy : (__bridge id)policy,
                                                                (id)kSecMatchValidOnDate : dateAfter,
                                                                returnKeyName : @YES }, &result), errSecItemNotFound);
    CFReleaseNull(policy);
    CFReleaseNull(result);
}

int secd_83_item_match_policy(int argc, char *const *argv)
{
    secd_test_setup_temp_keychain(__FUNCTION__, NULL);
    plan_tests(103);

    @autoreleasepool {
        addTestCertificates();
        NSArray *returnKeyNames = @[(id)kSecReturnAttributes, (id)kSecReturnData, (id)kSecReturnRef, (id)kSecReturnPersistentRef];
        for (id returnKeyName in returnKeyNames)
            test(returnKeyName);
    }

    return 0;
}
