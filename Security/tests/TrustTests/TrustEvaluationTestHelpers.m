//
//  TrustEvaluationTestHelpers.m
//  Security
//
//

#include <AssertMacros.h>
#import <Foundation/Foundation.h>
#import <Security/Security.h>

#include <utilities/SecInternalReleasePriv.h>
#include <utilities/SecCFRelease.h>
#include <utilities/SecCFWrappers.h>
#include <Security/SecCertificate.h>
#include <Security/SecCertificatePriv.h>
#include <Security/SecPolicyPriv.h>
#include <Security/SecTrust.h>
#include <Security/SecTrustPriv.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <string.h>

#include "TrustEvaluationTestHelpers.h"

// Want a class for running trust evaluations
// Want a dictionary-driven test with callback

@interface TestTrustEvaluation ()
@property NSString *directory;
@property NSMutableArray *certificates;
@property NSMutableArray *policies;
@property BOOL enableTestCertificates;
@end

@implementation TestTrustEvaluation
@synthesize ocspResponses = _ocspResponses;
@synthesize presentedSCTs = _presentedSCTs;
@synthesize trustedCTLogs = _trustedCTLogs;


- (instancetype)initWithCertificates:(NSArray *)certs policies:(NSArray *)policies {
    if (self = [super init]) {
        if (errSecSuccess != SecTrustCreateWithCertificates((__bridge CFArrayRef)certs, (__bridge CFArrayRef)policies, &_trust)) {
            return NULL;
        }
    }
    return self;
}

- (void)addAnchor:(SecCertificateRef)certificate
{
    CFArrayRef anchors = NULL;
    SecTrustCopyCustomAnchorCertificates(_trust, &anchors);

    NSMutableArray *newAnchors = [NSMutableArray array];
    if (anchors) {
        [newAnchors addObjectsFromArray:CFBridgingRelease(anchors)];
    }
    [newAnchors addObject:(__bridge id)certificate];
    (void)SecTrustSetAnchorCertificates(_trust, (__bridge CFArrayRef)newAnchors);
}

- (void)setAnchors:(NSArray *)anchorArray {
    (void)SecTrustSetAnchorCertificates(_trust, (__bridge CFArrayRef)anchorArray);
}

- (NSArray *)anchors  {
    CFArrayRef anchors = NULL;
    SecTrustCopyCustomAnchorCertificates(_trust, &anchors);
    return CFBridgingRelease(anchors);
}

- (void)setOcspResponses:(NSArray *)ocspResponsesArray {
    if (ocspResponsesArray != _ocspResponses) {
        _ocspResponses = nil;
        _ocspResponses = ocspResponsesArray;
        (void)SecTrustSetOCSPResponse(_trust, (__bridge CFArrayRef)ocspResponsesArray);
    }
}

- (void)setPresentedSCTs:(NSArray *)presentedSCTsArray {
    if (presentedSCTsArray != _presentedSCTs) {
        _presentedSCTs = nil;
        _presentedSCTs = presentedSCTsArray;
        (void)SecTrustSetSignedCertificateTimestamps(_trust, (__bridge CFArrayRef)presentedSCTsArray);
    }
}

- (void)setTrustedCTLogs:(NSArray *)trustedCTLogsArray {
    if (trustedCTLogsArray != _trustedCTLogs) {
        _trustedCTLogs = nil;
        _trustedCTLogs = trustedCTLogsArray;
        (void)SecTrustSetTrustedLogs(_trust, (__bridge CFArrayRef)trustedCTLogsArray);
    }
}

- (void)setVerifyDate:(NSDate *)aVerifyDate {
    if (aVerifyDate != _verifyDate) {
        _verifyDate = nil;
        _verifyDate = aVerifyDate;
        (void)SecTrustSetVerifyDate(_trust, (__bridge CFDateRef)aVerifyDate);
    }
}

- (void)setNeedsEvaluation {
    SecTrustSetNeedsEvaluation(_trust);
}

- (bool)evaluate:(out NSError * _Nullable __autoreleasing *)outError {
    CFErrorRef localError = nil;
    _trustResult = kSecTrustResultInvalid;
    _resultDictionary = nil;
    bool result = SecTrustEvaluateWithError(_trust, &localError);
    if (outError && localError) {
        *outError = CFBridgingRelease(localError);
    }
    (void)SecTrustGetTrustResult(_trust, &_trustResult);
    _resultDictionary = CFBridgingRelease(SecTrustCopyResult(_trust));
    return result;
}

- (void) dealloc {
    CFReleaseNull(_trust);
}

/* MARK: -
 * MARK: Dictionary-driven trust objects
 */

/* INSTRUCTIONS FOR ADDING NEW DICTIONARY-DRIVEN TRUSTS:
 *   1. Add the certificates, as DER-encoded files with the 'cer' extension to a directory included in the test Resources
 *      (e.g. OSX/shared_regressions/si-20-sectrust-policies-data/)
 *        NOTE: If your cert needs to be named with "(i[Pp]hone|i[Pp]ad|i[Pp]od)", you need to make two copies -- one named properly
 *        and another named such that it doesn't match that regex. Use the regex trick below for TARGET_OS_TV to make sure your test
 *        works.
 *   2. The input dictionary must include: (see constants below)
 *          MajorTestName
 *          MinorTestName
 *          Policies
 *          Leaf
 *          ExpectedResult
 *          CertDirectory
 *      It is strongly recommended that all test dictionaries include the Anchors and VerifyDate keys.
 *      Addtional optional keys are defined below.
 */

/* Key Constants for Test Dictionaries */
const NSString *kSecTrustTestMajorTestName  = @"MajorTestName";     /* Required; value: string */
const NSString *kSecTrustTestMinorTestName  = @"MinorTestName";     /* Required; value: string */
const NSString *kSecTrustTestPolicies       = @"Policies";          /* Required; value: dictionary or array of dictionaries */
const NSString *kSecTrustTestLeaf           = @"Leaf";              /* Required; value: string */
const NSString *kSecTrustTestIntermediates  = @"Intermediates";     /* Optional; value: string or array of strings */
const NSString *kSecTrustTestAnchors        = @"Anchors";           /* Recommended; value: string or array of strings */
const NSString *kSecTrustTestVerifyDate     = @"VerifyDate";        /* Recommended; value: date */
const NSString *kSecTrustTestExpectedResult = @"ExpectedResult";    /* Required; value: number */
const NSString *kSecTrustTestChainLength    = @"ChainLength";       /* Optional; value: number */
const NSString *kSecTrustTestEnableTestCerts= @"EnableTestCertificates"; /* Optional; value: string */
const NSString *kSecTrustTestDisableBridgeOS= @"BridgeOSDisable";   /* Optional; value: boolean */
const NSString *kSecTrustTestDirectory      = @"CertDirectory";     /* Required; value: string */

/* Key Constants for Policies Dictionaries */
const NSString *kSecTrustTestPolicyOID      = @"PolicyIdentifier";  /* Required; value: string */
const NSString *kSecTrustTestPolicyProperties = @"Properties";      /* Optional; value: dictionary, see Policy Value Constants, SecPolicy.h */

- (void)setMajorTestName:(NSString *)majorTestName minorTestName:(NSString *)minorTestName {
    self.fullTestName = [[majorTestName stringByAppendingString:@"-"] stringByAppendingString:minorTestName];
}

#if TARGET_OS_TV
/* Mastering removes all files named i[Pp]hone, so dynamically replace any i[Pp]hone's with
 * iPh0ne. We have two copies in the resources directory. */
- (NSString *)replaceiPhoneNamedFiles:(NSString *)filename {
    NSRegularExpression *regularExpression = [NSRegularExpression regularExpressionWithPattern:@"iphone"
                                                                                       options:NSRegularExpressionCaseInsensitive
                                                                                         error:nil];
    NSString *newfilename = [regularExpression stringByReplacingMatchesInString:filename
                                                                        options:0
                                                                          range:NSMakeRange(0, [filename length])
                                                                   withTemplate:@"iPh0ne"];
    return newfilename;
}
#endif

- (bool)addLeafToCertificates:(NSString *)leafName {
    SecCertificateRef cert;
    NSString *path = nil, *filename = nil;
    require_string(leafName, errOut, "%@: failed to get leaf for test");
#if TARGET_OS_TV
    filename = [self replaceiPhoneNamedFiles:leafName];
#else
    filename = leafName;
#endif

    path = [[NSBundle bundleForClass:[self class]]
            pathForResource:filename
            ofType:@"cer"
            inDirectory:self.directory];
    require_string(path, errOut, "failed to get path for leaf");
    cert = SecCertificateCreateWithData(NULL, (CFDataRef)[NSData dataWithContentsOfFile:path]);
    require_string(cert, errOut, "failed to create leaf certificate from path");
    self.certificates = [[NSMutableArray alloc] initWithObjects:(__bridge id)cert, nil];
    CFReleaseNull(cert);
    require_string(self.certificates, errOut, "failed to initialize certificates array");
    return true;

errOut:
    return false;
}

- (bool)addCertsToArray:(id)pathsObj outputArray:(NSMutableArray *)outArray {
    __block SecCertificateRef cert = NULL;
    __block NSString* path = nil, *filename = nil;
    require_string(pathsObj, errOut,
                         "failed to get certificate paths for test");

    if ([pathsObj isKindOfClass:[NSString class]]) {
        /* Only one cert path */
#if TARGET_OS_TV
        filename = [self replaceiPhoneNamedFiles:pathsObj];
#else
        filename = pathsObj;
#endif
        path = [[NSBundle bundleForClass:[self class]]
                pathForResource:filename
                ofType:@"cer"
                inDirectory:self.directory];
        require_string(path, errOut, "failed to get path for cert");
        cert = SecCertificateCreateWithData(NULL, (CFDataRef)[NSData dataWithContentsOfFile:path]);
        require_string(cert, errOut, "failed to create certificate from path");
        [outArray addObject:(__bridge id)cert];
        CFReleaseNull(cert);
    } else if ([pathsObj isKindOfClass:[NSArray class]]) {
        /* Test has more than one intermediate */
        [(NSArray *)pathsObj enumerateObjectsUsingBlock:^(NSString *resource, NSUInteger idx, BOOL *stop) {
#if TARGET_OS_TV
            filename = [self replaceiPhoneNamedFiles:resource];
#else
            filename = resource;
#endif
            path = [[NSBundle bundleForClass:[self class]]
                    pathForResource:filename
                    ofType:@"cer"
                    inDirectory:self.directory];
            require_string(path, blockOut, "failed to get path for cert");
            cert = SecCertificateCreateWithData(NULL, (CFDataRef)[NSData dataWithContentsOfFile:path]);
            require_string(cert, blockOut, "failed to create certificate %ld from path %@");
            [outArray addObject:(__bridge id)cert];

            CFReleaseNull(cert);
            return;

        blockOut:
            CFReleaseNull(cert);
            *stop = YES;
        }];
    } else {
        require_string(false, errOut, "unexpected type for intermediates or anchors value");
    }

    return true;

errOut:
    CFReleaseNull(cert);
    return false;
}

- (bool)addIntermediatesToCertificates:(id)intermediatesObj {
    require_string(intermediatesObj, errOut, "failed to get intermediates for test");

    require_string([self addCertsToArray:intermediatesObj outputArray:self.certificates], errOut,
                   "failed to add intermediates to certificates array");

    if ([intermediatesObj isKindOfClass:[NSString class]]) {
        require_string([self.certificates count] == 2, errOut,
                             "failed to add all intermediates");
    } else if ([intermediatesObj isKindOfClass:[NSArray class]]) {
        require_string([self.certificates count] == [(NSArray *)intermediatesObj count] + 1, errOut,
                            "failed to add all intermediates");
    }

    return true;

errOut:
    return false;
}

- (bool)addThirdPartyPinningPolicyChecks:(CFDictionaryRef)properties
                                  policy:(SecPolicyRef)policy
{
    if (!properties) {
        return true;
    }

    CFStringRef spkiSHA256Options[] = {
        kSecPolicyCheckLeafSPKISHA256,
        kSecPolicyCheckCAspkiSHA256,
    };

    for (size_t i = 0; i < sizeof(spkiSHA256Options)/sizeof(spkiSHA256Options[0]); i++) {
        CFArrayRef spkiSHA256StringArray = CFDictionaryGetValue(properties, spkiSHA256Options[i]);
        // Relevant property is not set.
        if (!spkiSHA256StringArray) {
            continue;
        }
        require_string(isArray(spkiSHA256StringArray), errOut, "SPKISHA256 property is not an array");

        CFMutableArrayRef spkiSHA256DataArray = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks);
        require_string(spkiSHA256DataArray, errOut, "failed to allocate memory for the SPKISHA256 data array");

        for (CFIndex j = 0; j < CFArrayGetCount(spkiSHA256StringArray); j++) {
            CFStringRef spkiSHA256String = CFArrayGetValueAtIndex(spkiSHA256StringArray, j);
            require_string(isString(spkiSHA256String), errOut, "SPKISHA256 property array element is not a string");
            CFDataRef spkiSHA256Data = CreateCFDataFromBase64CFString(spkiSHA256String);
            // 'spkiSHA256Data' is optional because we want to allow empty strings.
            if (spkiSHA256Data) {
                CFArrayAppendValue(spkiSHA256DataArray, spkiSHA256Data);
            }
            CFReleaseNull(spkiSHA256Data);
        }

        SecPolicySetOptionsValue(policy, spkiSHA256Options[i], spkiSHA256DataArray);
        CFReleaseNull(spkiSHA256DataArray);
    }

    return true;

errOut:
    return false;
}

- (bool)addPolicy:(NSDictionary *)policyDict
{
    SecPolicyRef policy = NULL;
    NSString *policyIdentifier = [(NSDictionary *)policyDict objectForKey:kSecTrustTestPolicyOID];
    NSDictionary *policyProperties = [(NSDictionary *)policyDict objectForKey:kSecTrustTestPolicyProperties];
    require_string(policyIdentifier, errOut, "failed to get policy OID");

    CFDictionaryRef properties = (__bridge CFDictionaryRef)policyProperties;
    policy = SecPolicyCreateWithProperties((__bridge CFStringRef)policyIdentifier,
                                           properties);
    require_string(policy, errOut, "failed to create properties for policy OID");
    require_string([self addThirdPartyPinningPolicyChecks:properties policy:policy], errOut, "failed to parse properties for third-party-pinning policy checks");
    [self.policies addObject:(__bridge id)policy];
    CFReleaseNull(policy);

    return true;
errOut:
    CFReleaseNull(policy);
    return false;
}

- (bool)addPolicies:(id)policiesObj {
    require_string(policiesObj, errOut,
                   "failed to get policies for test");

    self.policies = [[NSMutableArray alloc] init];
    require_string(self.policies, errOut,
                   "failed to initialize policies array");
    if ([policiesObj isKindOfClass:[NSDictionary class]]) {
        /* Test has only one policy */
        require_string([self addPolicy:policiesObj], errOut, "failed to add policy");
    } else if ([policiesObj isKindOfClass:[NSArray class]]) {
        /* Test more than one policy */
        [(NSArray *)policiesObj enumerateObjectsUsingBlock:^(NSDictionary *policyDict, NSUInteger idx, BOOL *stop) {
            if (![self addPolicy:policyDict]) {
                *stop = YES;
            }
        }];

        require_string([(NSArray *)policiesObj count] == [self.policies count], errOut, "failed to add all policies");
    } else {
        require_string(false, errOut, "unexpected type for Policies value");
    }

    return true;

errOut:
    return false;
}

- (bool)setAnchorsFromPlist:(id)anchorsObj {
    NSMutableArray *anchors = [NSMutableArray array];
    require_string(anchorsObj, errOut, "failed to get anchors for test");
    require_string([self addCertsToArray:anchorsObj outputArray:anchors], errOut, "failed to add anchors to anchors array");

    if ([anchorsObj isKindOfClass:[NSString class]]) {
        require_string([anchors count] == 1, errOut, "failed to add all anchors");
    } else if ([anchorsObj isKindOfClass:[NSArray class]]) {
        require_string([anchors count] == [(NSArray *)anchorsObj count], errOut, "failed to add all anchors");
    }

    // set the anchors in the SecTrustRef
    self.anchors = anchors;
    return true;

errOut:
    return false;
}

- (instancetype _Nullable) initWithTrustDictionary:(NSDictionary *)testDict
{
    if (!(self = [super init])) {
        return self;
    }

    NSString *majorTestName = nil, *minorTestName = nil;
    SecTrustRef trust = NULL;

#if TARGET_OS_BRIDGE
    /* Some of the tests don't work on bridgeOS because there is no Certificates bundle. Skip them. */
    if([testDict[kSecTrustTestDisableBridgeOS] boolValue]) {
        self.bridgeOSDisabled =  YES;
    }
#endif

    /* Test certificates work by default on internal builds. We still need this to
     * determine whether to expect failure for production devices. */
    self.enableTestCertificates = [testDict[kSecTrustTestEnableTestCerts] boolValue];

    /* Test name, for documentation purposes */
    majorTestName = testDict[kSecTrustTestMajorTestName];
    minorTestName = testDict[kSecTrustTestMinorTestName];
    require_string(majorTestName && minorTestName, errOut, "Failed to create test names for test");
    [self setMajorTestName:majorTestName minorTestName:minorTestName];

#if DEBUG
    fprintf(stderr, "BEGIN trust creation for %s", [self.fullTestName cStringUsingEncoding:NSUTF8StringEncoding]);
#endif

    /* Cert directory */
    self.directory = testDict[kSecTrustTestDirectory];
    require_string(self.directory, errOut, "No directory for test!");

    /* Populate the certificates array */
    require_quiet([self addLeafToCertificates:testDict[kSecTrustTestLeaf]], errOut);

    /* Add optional intermediates to certificates array */
    if (testDict[kSecTrustTestIntermediates]) {
        require_quiet([self addIntermediatesToCertificates:testDict[kSecTrustTestIntermediates]], errOut);
    }

    /* Create the policies */
#if !TARGET_OS_BRIDGE
    require_quiet([self addPolicies:testDict[kSecTrustTestPolicies]], errOut);
#else // TARGET_OS_BRIDGE
    if (![self addPolicies:testDict[kSecTrustTestPolicies]]) {
        /* Some policies aren't available on bridgeOS (because there is no Certificates bundle on bridgeOS).
         * If we fail to add the policies for a disabled test, let SecTrustCreate fall back to the Basic policy.
         * We'll skip the evaluation and other tests by honoring bridgeOSDisabled, but we need to return a
         * TestTrustEvaluation object so that the test continues. */
        if (self.bridgeOSDisabled) {
            self.policies = nil;
        } else {
            goto errOut;
        }
    }
#endif // TARGET_OS_BRIDGE


    /* Create the trust object */
    require_noerr_string(SecTrustCreateWithCertificates((__bridge CFArrayRef)self.certificates,
                                                              (__bridge CFArrayRef)self.policies,
                                                              &trust),
                               errOut,
                               "failed to create trust ref");
    self.trust = trust;

    /* Optionally set anchors in trust object */
    if (testDict[kSecTrustTestAnchors]) {
        require_quiet([self setAnchorsFromPlist:testDict[kSecTrustTestAnchors]], errOut);
    }

    /* Set optional date in trust object */
    if (testDict[kSecTrustTestVerifyDate]) {
        self.verifyDate = testDict[kSecTrustTestVerifyDate];
    }

    /* Set expected results */
    self.expectedResult = testDict[kSecTrustTestExpectedResult];
    self.expectedChainLength = testDict[kSecTrustTestChainLength];

#if DEBUG
    fprintf(stderr, "END trust creation for %s", [self.fullTestName cStringUsingEncoding:NSUTF8StringEncoding]);
#endif

    return self;

errOut:
    return nil;
}

- (bool)evaluateForExpectedResults:(out NSError * _Nullable __autoreleasing *)outError
{
#if TARGET_OS_BRIDGE
    // Artificially skip tests for bridgeOS. To prevent test errors these need to be reported as a pass.
    if (self.bridgeOSDisabled) {
        return true;
    }
#endif

    if (!self.expectedResult) {
        if (outError) {
            NSString *errorDescription = [NSString stringWithFormat:@"Test %@: no expected results set",
                                          self.fullTestName];
            *outError = [NSError errorWithDomain:@"TrustTestsError" code:(-1)
                                        userInfo:@{ NSLocalizedFailureReasonErrorKey : errorDescription}];
        }
        return false;
    }

    SecTrustResultType trustResult = kSecTrustResultInvalid;
    if (errSecSuccess != SecTrustGetTrustResult(self.trust, &trustResult)) {
        if (outError) {
            NSString *errorDescription = [NSString stringWithFormat:@"Test %@: Failed to get trust result",
                                          self.fullTestName];
            *outError = [NSError errorWithDomain:@"TrustTestsError" code:(-2)
                                        userInfo:@{ NSLocalizedFailureReasonErrorKey : errorDescription}];
        }
        return false;
    }

    bool result = false;

    /* If we enabled test certificates on a non-internal device, expect a failure instead of success. */
    if (self.enableTestCertificates && !SecIsInternalRelease() && ([self.expectedResult unsignedIntValue] == 4)) {
        if (trustResult == kSecTrustResultRecoverableTrustFailure) {
            result = true;
        }
    } else if (trustResult == [self.expectedResult unsignedIntValue]) {
        result = true;
    }

    if (!result) {
        if (outError) {
            NSString *errorDescription = [NSString stringWithFormat:@"Test %@: Expected result %@ %s does not match actual result %u %s",
                                          self.fullTestName, self.expectedResult,
                                          (self.enableTestCertificates ? "for test cert" : ""),
                                          trustResult,
                                          SecIsInternalRelease() ? "" : "on prod device"];
            *outError = [NSError errorWithDomain:@"TrustTestsError" code:(-3)
                                        userInfo:@{ NSLocalizedFailureReasonErrorKey : errorDescription}];
        }
        return result;
    }

    /* expected chain length is optional, but if we have it, verify */
    if (self.expectedChainLength && (SecTrustGetCertificateCount(self.trust) != [self.expectedChainLength longValue])) {
        if (outError) {
            NSString *errorDescription = [NSString stringWithFormat:@"Test %@: Expected chain length %@ does not match actual chain length %ld",
                                          self.fullTestName, self.expectedChainLength, SecTrustGetCertificateCount(self.trust)];
            *outError = [NSError errorWithDomain:@"TrustTestsError" code:(-4)
                                        userInfo:@{ NSLocalizedFailureReasonErrorKey : errorDescription}];
        }
        return false;
    }
    return true;
}

@end

int ping_host(char *host_name)
{
    struct sockaddr_in pin;
    struct hostent *nlp_host;
    struct in_addr addr;
    int sd = 0;
    int port = 80;
    int retries = 5; // we try 5 times, then give up
    char **h_addr_list = NULL;

    while ((nlp_host=gethostbyname(host_name)) == 0 && retries--) {
        printf("Resolve Error! (%s) %d\n", host_name, h_errno);
        sleep(1);
    }
    if (nlp_host == 0) {
        return 0;
    }

    bzero(&pin,sizeof(pin));
    pin.sin_family=AF_INET;
    pin.sin_addr.s_addr=htonl(INADDR_ANY);
    h_addr_list = malloc(nlp_host->h_length * sizeof(char *));
    memcpy(h_addr_list, nlp_host->h_addr_list, nlp_host->h_length * sizeof(char *));
    memcpy(&addr, h_addr_list[0], sizeof(struct in_addr));
    pin.sin_addr.s_addr=addr.s_addr;
    pin.sin_port=htons(port);

    sd=socket(AF_INET,SOCK_STREAM,0);

    if (connect(sd,(struct sockaddr*)&pin,sizeof(pin)) == -1) {
        printf("connect error! (%s) %d\n", host_name, errno);
        close(sd);
        free(h_addr_list);
        return 0;
    }
    close(sd);
    free(h_addr_list);
    return 1;
}

static int current_dir = -1;
static char *home_var = NULL;

NSURL *setUpTmpDir(void) {
    /* Set up TMP directory for trustd's files */
    int ok = 0;
    NSError* error = nil;
    NSString* pid = [NSString stringWithFormat: @"tst-%d", [[NSProcessInfo processInfo] processIdentifier]];
    NSURL* tmpDirURL = [[NSURL fileURLWithPath:NSTemporaryDirectory() isDirectory:YES] URLByAppendingPathComponent:pid];
    ok = (bool)tmpDirURL;

    if (current_dir == -1 && home_var == NULL) {
        ok = ok && [[NSFileManager defaultManager] createDirectoryAtURL:tmpDirURL
                                            withIntermediateDirectories:NO
                                                             attributes:NULL
                                                                  error:&error];

        NSURL* libraryURL = [tmpDirURL URLByAppendingPathComponent:@"Library"];
        NSURL* preferencesURL = [tmpDirURL URLByAppendingPathComponent:@"Preferences"];

        ok =  (ok && (current_dir = open(".", O_RDONLY) >= 0)
               && (chdir([tmpDirURL fileSystemRepresentation]) >= 0)
               && (setenv("HOME", [tmpDirURL fileSystemRepresentation], 1) >= 0)
               && (bool)(home_var = getenv("HOME")));

        ok = ok && [[NSFileManager defaultManager] createDirectoryAtURL:libraryURL
                                            withIntermediateDirectories:NO
                                                             attributes:NULL
                                                                  error:&error];

        ok = ok && [[NSFileManager defaultManager] createDirectoryAtURL:preferencesURL
                                            withIntermediateDirectories:NO
                                                             attributes:NULL
                                                                  error:&error];
    }

    if (ok > 0) {
        return tmpDirURL;
    }

    return nil;
}
