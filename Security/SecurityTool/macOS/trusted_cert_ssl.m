/*
 * Copyright (c) 2019 Apple Inc. All Rights Reserved.
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
 *
 * verify_ssl.m
 */


#import "trusted_cert_ssl.h"
#include <Foundation/Foundation.h>
#include <CoreServices/CoreServices.h>

#pragma clang diagnostic ignored "-Wdeprecated-declarations"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if TARGET_OS_MAC
#include <Security/oidscert.h>
#include <Security/oidsattr.h>
#include <Security/oidsalg.h>
#include <Security/x509defs.h>
#include <Security/cssmapi.h>
#include <Security/cssmapple.h>
#endif

#include <Security/certextensions.h>
#include <Security/SecKeychain.h>
#include <Security/SecKeychainItem.h>
#include <Security/SecImportExport.h>
#include <Security/SecIdentity.h>
#include <Security/SecIdentitySearch.h>
#include <Security/SecKey.h>
#include <Security/SecCertificate.h>
#include <Security/SecTrust.h>

#include <CFNetwork/CFNetwork.h>
#include <SecurityFoundation/SFCertificateData.h>

typedef CFTypeRef CFURLResponseRef;
CFDictionaryRef _CFURLResponseGetSSLCertificateContext(CFURLResponseRef);

@interface NSURLResponse (URLResponseInternals)
- (CFURLResponseRef)_CFURLResponse;
@end

@interface NSHTTPURLResponse (HTTPURLResponseInternals)
- (NSArray *)_peerCertificateChain;
@end

@interface NSURLResponse (CertificateUtilities)
- (SecTrustRef)peerTrust;
- (NSArray *)peerCertificates;
@end

@interface CertDownloader : NSObject <NSURLDownloadDelegate>
{
    @private
    NSURL *_url;
    NSURLRequest *_urlReq;
    NSURLResponse *_urlRsp;
    NSURLDownload *_urlDL;
    SecTrustRef _trust;
    BOOL _finished;
}

- (id)initWithURLString:(const char *)urlstr;
- (void)dealloc;
- (BOOL)finished;
- (void)waitForDownloadToFinish;

- (SecTrustRef)trustReference;

- (void)downloadDidFinish:(NSURLDownload *)download;
- (void)download:(NSURLDownload *)download didFailWithError:(NSError *)error;

+ (BOOL)isNetworkURL:(const char *)urlstr;

@end

static int _verbose = 0;

#define ANSI_RED     "\x1b[31m"
#define ANSI_GREEN   "\x1b[32m"
#define ANSI_YELLOW  "\x1b[33m"
#define ANSI_BLUE    "\x1b[34m"
#define ANSI_MAGENTA "\x1b[35m"
#define ANSI_CYAN    "\x1b[36m"
#define ANSI_RESET   "\x1b[0m"


@implementation CertDownloader

- (id)initWithURLString:(const char *)urlstr
{
    _finished = NO;
    _url = [[NSURL alloc] initWithString:[NSString stringWithFormat:@"%s", urlstr]];
    _urlReq = [[NSURLRequest alloc] initWithURL:_url];
    _urlRsp = nil;
    _trust = NULL;

    _urlDL = [[NSURLDownload alloc] initWithRequest:_urlReq delegate:self];

    fprintf(stdout, "Opening connection to %s\n", urlstr);
    fflush(stdout);

    return self;
}

- (void)dealloc
{
    _urlDL = nil;
    _urlReq = nil;
    _urlRsp = nil;
    _url = nil;
    _trust = NULL;
}

- (BOOL)finished
{
    return _finished;
}

- (void)waitForDownloadToFinish
{
    // cycle the run loop while we're waiting for the _finished flag to be set
    NSDate *cycleTime;
    while (_finished == NO) {
        cycleTime = [NSDate dateWithTimeIntervalSinceNow:0.1];
        [[NSRunLoop currentRunLoop] runMode: NSDefaultRunLoopMode beforeDate:cycleTime];
    }
}

// NSURLDownloadDelegate delegate methods

- (void)downloadDidFinish:(NSURLDownload *)download
{
    if (!_urlRsp) {
        fprintf(stdout, "No response received from %s\n", [[_url absoluteString] UTF8String]);
        fflush(stdout);
    }
    _finished = YES;
}

- (void)download:(NSURLDownload *)download didReceiveResponse:(NSURLResponse *)response
{
    fprintf(stdout, "Received response from %s\n", [[_url absoluteString] UTF8String]);
    fflush(stdout);
    _urlRsp = response;
}

- (void)download:(NSURLDownload *)download didFailWithError:(NSError *)error
{
    fprintf(stdout, "Failed connection to %s", [[_url absoluteString] UTF8String]);
    if (!_verbose) {
        fprintf(stdout, " (use -v option to see more information)");
    }
    fprintf(stdout, "\n");
    fflush(stdout);
    SecTrustRef tmp = _trust;
    _trust = (SecTrustRef)CFBridgingRetain([[error userInfo] objectForKey:@"NSURLErrorFailingURLPeerTrustErrorKey"]);
    if (tmp) { CFRelease(tmp); }
    _finished = YES;

    if (_verbose) {
        // dump the userInfo dictionary
        NSLog(@"%@", [error userInfo]);
    }
}

- (SecTrustRef)trustReference
{
    // First, see if we already retained the SecTrustRef, e.g. during a failed connection.
    SecTrustRef trust = _trust;
    if (!trust) {
        // If not, try to obtain the SecTrustRef from the response.
        trust = [_urlRsp peerTrust];
        if (trust) {
            if (_verbose > 1) {
                fprintf(stdout, "Obtained SecTrustRef from the response\n");
            }
            CFRetain(trust);
            _trust = trust;
        }
    }
    if (!trust) {
        // We don't have a SecTrustRef, so build one ourselves for this host.
        NSString *host = [_url host];
        if (_verbose > 1) {
            fprintf(stdout, "Building our own SecTrustRef for \"%s\"\n", host ? [host UTF8String] : "<missing host>");
        }
        SecPolicyRef sslPolicy = SecPolicyCreateSSL(false, (__bridge CFStringRef)host);
        SecPolicyRef revPolicy =  SecPolicyCreateRevocation(kSecRevocationUseAnyAvailableMethod);
        NSArray *policies = [NSArray arrayWithObjects:(__bridge id)sslPolicy, (__bridge id)revPolicy, nil];
        NSArray *certs = [_urlRsp peerCertificates];
        if (certs) {
            (void)SecTrustCreateWithCertificates((__bridge CFArrayRef)certs, (__bridge CFArrayRef)policies, &trust);
            _trust = trust;
        }
    }

    // Ensure that the trust has been evaluated
    SecTrustResultType result = kSecTrustResultInvalid;
    OSStatus status = SecTrustGetTrustResult(trust, &result);
    bool needsEvaluation = (status != errSecSuccess || result == kSecTrustResultInvalid);
    if (needsEvaluation) {
        status = SecTrustEvaluate(trust, &result);
        if (_verbose > 1) {
            fprintf(stdout, "%s trust reference (status = %d, trust result = %d)\n",
                    (needsEvaluation) ? "Evaluated" : "Checked",
                    (int)status, (int)result);
        }
    }
    return trust;
}

+ (BOOL)isNetworkURL:(const char *)urlstr
{
    if (urlstr) {
        NSArray *schemes = @[@"https",@"ldaps"];
        NSURL *url = [NSURL URLWithString:[NSString stringWithUTF8String:urlstr]];
        if (url && [schemes containsObject:[url scheme]]) {
            return YES;
        }
    }
    return NO;
}

@end

@implementation NSURLResponse (CertificateUtilities)

- (SecTrustRef)peerTrust
{
    SecTrustRef trust = NULL;

    // Obtain the underlying SecTrustRef used by CFNetwork for the connection.
    CFDictionaryRef certificateContext = _CFURLResponseGetSSLCertificateContext([self _CFURLResponse]);
    if (_verbose && !certificateContext) {
        fprintf(stdout, "Unable to get SSL certificate context!\n");
        fflush(stdout);
    } else {
        trust = (SecTrustRef) CFDictionaryGetValue(certificateContext, kCFStreamPropertySSLPeerTrust);
    }
    if (_verbose && !trust) {
        fprintf(stdout, "Unable to get kCFStreamPropertySSLPeerTrust!\n");
        fflush(stdout);
    }
    return trust;
}

- (NSArray *)peerCertificates
{
    NSArray *certificateChain = nil;
    if ([self isKindOfClass:[NSHTTPURLResponse class]]) {
        certificateChain = [(NSHTTPURLResponse *)self _peerCertificateChain];
    }
    if (_verbose && certificateChain) {
        fprintf(stdout, "Peer certificates: ");
        fflush(stdout);
        CFShow((__bridge CFArrayRef)certificateChain);
    }
    return certificateChain;
}

@end


static NSString *errorStringForKey(NSString *key)
{
    NSString *errstr = nil;
    // %%% note: these dictionary keys currently do not have exported constants
    if (![key length] || [key isEqualToString:@"StatusCodes"]) {
        return errstr; // skip empty and legacy numeric errors
    } else if ([key isEqualToString:@"SSLHostname"]) {
        errstr = @"Host name not found in Subject Alternative Name extension";
    } else if ([key isEqualToString:@"TemporalValidity"]) {
        errstr = @"Certificate has expired, or is not yet valid (check date)";
    } else if ([key isEqualToString:@"KeySize"]) {
        errstr = @"Certificate uses a key size which is considered too weak";
    } else if ([key isEqualToString:@"SignatureHashAlgorithms"]) {
        errstr = @"Certificate uses a signing algorithm which is considered too weak";
    } else if ([key isEqualToString:@"KeyUsage"]) {
        errstr = @"The Key Usage extension does not permit this use for the certificate";
    } else if ([key isEqualToString:@"ExtendedKeyUsage"]) {
        errstr = @"The Extended Key Usage extension does not permit this use for the certificate";
    } else if ([key isEqualToString:@"Revocation"]) {
        errstr = @"Certificate has been revoked and cannot be used";
    } else if ([key isEqualToString:@"BlackListedLeaf"]) {
        errstr = @"Certificate has been blocked and cannot be used";
    } else if ([key isEqualToString:@"AnchorTrusted"]) {
        errstr = @"The root of the certificate chain is not trusted";
    } else if ([key isEqualToString:@"MissingIntermediate"]) {
        errstr = @"Unable to find next certificate in the chain";
    } else if ([key isEqualToString:@"NonEmptySubject"]) {
        errstr = @"Certificate has no subject, and SAN is missing or not marked critical";
    } else if ([key isEqualToString:@"BasicCertificateProcessing"]) {
        errstr = @"Certificate not standards compliant (RFC 5280|CABF Baseline Requirements)";
    } else if ([key isEqualToString:@"NameConstraints"]) {
        errstr = @"Certificate violates name constraints placed on issuing CA";
    } else if ([key isEqualToString:@"PolicyConstraints"]) {
        errstr = @"Certificate violates policy constraints placed on issuing CA";
    } else if ([key isEqualToString:@"CTRequired"]) {
        errstr = @"Certificate Transparency validation is required but missing";
    } else if ([key isEqualToString:@"ValidityPeriodMaximums"]) {
        errstr = @"Certificate exceeds maximum allowable validity period (normally 825 days)";
    } else if ([key isEqualToString:@"ServerAuthEKU"]) {
        errstr = @"The Extended Key Usage extension does not permit server authentication";
    } else if ([key isEqualToString:@"UnparseableExtension"]) {
        errstr = @"Unable to parse a standard extension (corrupt or invalid format detected)";
    }

    if (errstr) {
        errstr = [NSString stringWithFormat:@"%@ [%@]", errstr, key];
    } else {
        errstr = [NSString stringWithFormat:@"[%@]", key];
    }
    return errstr;
}

void printErrorDetails(SecTrustRef trust)
{
    CFDictionaryRef result = SecTrustCopyResult(trust);
    CFArrayRef properties = SecTrustCopyProperties(trust);
    NSArray *props = (__bridge NSArray *)properties;
    NSArray *details = [(__bridge NSDictionary *)result objectForKey:@"TrustResultDetails"];
    if (!props || !details) {
        if (result) { CFRelease(result); }
        if (properties) { CFRelease(properties); }
        return;
    }
    // Preflight to see if there are any errors to display
    CFIndex errorCount = 0, chainLength = [details count];
    for (CFIndex chainIndex = 0; chainIndex < chainLength; chainIndex++) {
        NSDictionary *certDetails = (NSDictionary *)[details objectAtIndex:chainIndex];
        errorCount += [certDetails count];
    }
    if (!errorCount) {
        if (result) { CFRelease(result); }
        if (properties) { CFRelease(properties); }
        return;
    }

    // Display per-certificate errors
    fprintf(stdout, "---\nCertificate errors\n");
    for (CFIndex chainIndex = 0; chainIndex < chainLength; chainIndex++) {
        NSDictionary *certProps = (NSDictionary *)[props objectAtIndex:chainIndex];
        NSString *certTitle = (NSString *)[certProps objectForKey:(__bridge NSString*)kSecPropertyTypeTitle];
        fprintf(stdout, " %ld: %s\n", (long)chainIndex, [certTitle UTF8String]);
        NSDictionary *certDetails = (NSDictionary *)[details objectAtIndex:chainIndex];
        NSEnumerator *keyEnumerator = [certDetails keyEnumerator];
        NSString *key;
        while ((key = (NSString*)[keyEnumerator nextObject])) {
            NSString *str = errorStringForKey(key);
            if (!str) { continue; }
            fprintf(stdout, ANSI_RED "    %s" ANSI_RESET "\n", [str UTF8String]);
        }
    }
    fflush(stdout);

    CFRelease(result);
    CFRelease(properties);
}

void printExtendedResults(SecTrustRef trust)
{
    CFDictionaryRef trustResults = SecTrustCopyResult(trust);
    if (!trustResults) { return; }
    fprintf(stdout, "---\n");

    NSDictionary *results = (__bridge NSDictionary *)trustResults;
    NSString *orgName = [results objectForKey:(NSString *)kSecTrustOrganizationName];
    CFBooleanRef isEV = (__bridge CFBooleanRef)[results objectForKey:(NSString *)kSecTrustExtendedValidation];
    if (isEV == kCFBooleanTrue) {
        fprintf(stdout, "Extended Validation (EV) confirmed for \"" ANSI_GREEN "%s" ANSI_RESET "\"\n", [orgName UTF8String]);
    } else {
        fprintf(stdout, "No extended validation result found\n");
    }
    CFBooleanRef isCT = (__bridge CFBooleanRef)[results objectForKey:(NSString *)kSecTrustCertificateTransparency];
    CFBooleanRef isCTW = (__bridge CFBooleanRef)[results objectForKey:(NSString *)kSecTrustCertificateTransparencyWhiteList];
    if (isCT == kCFBooleanTrue) {
        fprintf(stdout, "Certificate Transparency (CT) status: " ANSI_GREEN "verified" ANSI_RESET "\n");
    } else if (isCTW == kCFBooleanTrue) {
        fprintf(stdout, "Certificate Transparency requirement waived for approved EV certificate\n");
    } else {
        fprintf(stdout, "Certificate Transparency (CT) status: " ANSI_RED "not verified" ANSI_RESET "\n");
        fprintf(stdout, "Unable to find at least 2 signed certificate timestamps (SCTs) from approved logs\n");
    }
    fflush(stdout);
    CFRelease(trustResults);
}

int evaluate_ssl(const char *urlstr, int verbose, SecTrustRef * CF_RETURNS_RETAINED trustRef)
{
    @autoreleasepool {
        _verbose = verbose;
        if (trustRef) {
            *trustRef = NULL;
        }
        if (![CertDownloader isNetworkURL:urlstr]) {
            return 2;
        }
        CertDownloader *context = [[CertDownloader alloc] initWithURLString:urlstr];
        [context waitForDownloadToFinish];
        SecTrustRef trust = [context trustReference];
        if (trustRef && trust) {
            CFRetain(trust);
            *trustRef = trust;
        }
    }
    return 0;
}

static bool isHex(unichar c)
{
    return ((c >= 0x30 && c <= 0x39) || /* 0..9 */
            (c >= 0x41 && c <= 0x46) || /* A..F */
            (c >= 0x61 && c <= 0x66));  /* a..f */
}

static bool isEOL(unichar c)
{
    return (c == 0x0D || c == 0x0A);
}

CF_RETURNS_RETAINED CFStringRef CopyCertificateTextRepresentation(SecCertificateRef certificate)
{
    if (!certificate) {
        return NULL;
    }
    @autoreleasepool {
        NSData *certData = [[[SFCertificateData alloc] initWithCertificate:certificate] tabDelimitedTextData];
        NSString *certStr = [[NSString alloc] initWithData:certData encoding:NSUnicodeStringEncoding];
        NSMutableString *outStr = [NSMutableString stringWithCapacity:0];
        [outStr appendString:certStr];

        // process the output for readability by changing tabs to spaces
        CFIndex index, count = [outStr length];
        for (index = 1; index < count; index++) {
            unichar c = [outStr characterAtIndex:index];
            unichar p = [outStr characterAtIndex:index-1];
            if (isEOL(p)) { // start of line
                while (c == 0x09) { // convert tabs to spaces until non-tab found
                    [outStr replaceCharactersInRange:NSMakeRange(index, 1) withString:@" "];
                    c = [outStr characterAtIndex:++index];
                }
            } else if (c == 0x09) { // tab found between label and value
                if (p == 0x20) { // continue the run of spaces
                    [outStr replaceCharactersInRange:NSMakeRange(index, 1) withString:@" "];
                } else { // insert colon delimiter and space
                    [outStr replaceCharactersInRange:NSMakeRange(index, 1) withString:@": "];
                    count++; // we inserted an extra character
                }
            }
        }
        // remove spaces in hexadecimal data representations for compactness
        count = [outStr length];
        index = 0;
        while (++index < count) {
            unichar c = [outStr characterAtIndex:index];
            unichar p = [outStr characterAtIndex:index-1];
            // possible start of hex data run occurs after colon delimiter
            if (p == 0x3A && c == 0x20) {
                CFIndex start = index;
                CFIndex len = 0;
                while ((start+len+3 < count)) {
                    // scan for repeating three-character pattern
                    unichar first = [outStr characterAtIndex:start+len+0];
                    if (first != 0x20) {
                        break;
                    }
                    unichar second = [outStr characterAtIndex:start+len+1];
                    unichar third = [outStr characterAtIndex:start+len+2];
                    unichar last = [outStr characterAtIndex:start+len+3];
                    if (isHex(second) && isHex(third)) {
                        len += 3;
                    } else if (isEOL(second) && isHex(third) && isHex(last)) {
                        len += 4; // pattern continues on next line
                    } else {
                        break;
                    }
                }
                if (len > 0) {
                    // skip over the first space after the colon, which we want to keep
                    for (CFIndex idx = start+1; idx < count && idx < start+len; idx++) {
                        c = [outStr characterAtIndex:idx];
                        if (c == 0x20 || isEOL(c)) {
                            [outStr deleteCharactersInRange:NSMakeRange(idx,1)];
                            count--; // we removed a character from the total length
                            len--; // our substring also has one less character
                        }
                    }
                }
            }
        }
        return CFBridgingRetain(outStr);
    }
}

