//
//  SecABC.m
//  Security
//

#import <SoftLinking/SoftLinking.h>
#import <os/log.h>

#import "SecABC.h"

/*
 * This is using soft linking since we need upward (to workaround BNI build dependencies)
 * and weak linking since SymptomDiagnosticReporter is not available on base and darwinOS.
 */

SOFT_LINK_OPTIONAL_FRAMEWORK(PrivateFrameworks, SymptomDiagnosticReporter);
SOFT_LINK_CLASS(SymptomDiagnosticReporter, SDRDiagnosticReporter);


#if ABC_BUGCAPTURE
#import <SymptomDiagnosticReporter/SDRDiagnosticReporter.h>
#endif

void SecABCTrigger(CFStringRef type,
                   CFStringRef subtype,
                   CFStringRef subtypeContext,
                   CFDictionaryRef payload)
{
    [SecABC triggerAutoBugCaptureWithType:(__bridge NSString *)type
                                  subType:(__bridge NSString *)subtype
                           subtypeContext:(__bridge NSString *)subtypeContext
                                   domain:@"com.apple.security.keychain"
                                   events:nil
                                  payload:(__bridge NSDictionary *)payload
                          detectedProcess:nil];
}


@implementation SecABC

+ (void)triggerAutoBugCaptureWithType:(NSString *)type
                              subType:(NSString *)subType
{
    [self triggerAutoBugCaptureWithType: type
                                subType: subType
                         subtypeContext: nil
                                 domain: @"com.apple.security.keychain"
                                 events: nil
                                payload: nil
                        detectedProcess: nil];
}


+ (void)triggerAutoBugCaptureWithType:(NSString *)type
                              subType:(NSString *)subType
                       subtypeContext:(NSString * _Nullable)subtypeContext
                               domain:(NSString *)domain
                               events:(NSArray * _Nullable)events
                              payload:(NSDictionary * _Nullable)payload
                      detectedProcess:(NSString * _Nullable)process
{
#if ABC_BUGCAPTURE
    os_log_info(OS_LOG_DEFAULT, "TriggerABC for %{public}@/%{public}@/%{public}@",
                type, subType, subtypeContext);

    // no ABC on darwinos
    Class sdrDiagReporter = getSDRDiagnosticReporterClass();
    if (sdrDiagReporter == nil) {
        return;
    }

    SDRDiagnosticReporter *diagnosticReporter = [[sdrDiagReporter alloc] init];
    NSMutableDictionary *signature = [diagnosticReporter signatureWithDomain:domain
                                                                        type:type
                                                                     subType:subType
                                                              subtypeContext:subtypeContext
                                                             detectedProcess:process?:[[NSProcessInfo processInfo] processName]
                                                      triggerThresholdValues:nil];
    if (signature == NULL) {
        os_log_error(OS_LOG_DEFAULT, "TriggerABC signature generation failed");
        return;
    }

    (void)[diagnosticReporter snapshotWithSignature:signature
                                              delay:5.0
                                             events:events
                                            payload:payload
                                            actions:nil
                                              reply:^void(NSDictionary *response)
    {
        os_log_info(OS_LOG_DEFAULT,
                    "Received response from Diagnostic Reporter - %{public}@/%{public}@/%{public}@: %{public}@",
                    type, subType, subtypeContext, response);
    }];
#endif
}

@end
