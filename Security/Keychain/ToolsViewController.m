//
//  ToolsViewController.m
//  Keychain
//
//  Created by john on 10/22/12.
//
//

#import "ToolsViewController.h"
#import "MyKeychain.h"

#include <CKBridge/SOSCloudKeychainClient.h>
#include <SecureObjectSync/SOSCloudCircle.h>
#include <SecureObjectSync/SOSCloudCircleInternal.h>
#include <notify.h>
#include <dispatch/dispatch.h>
//#include <utilities/SecCFWrappers.h>

#include <CoreFoundation/CoreFoundation.h>
#include <CoreFoundation/CFUserNotification.h>

#import <QuartzCore/QuartzCore.h>
#include <Regressions/SOSTestDataSource.h>
#include <securityd/SOSCloudCircleServer.h>
#include <CKBridge/SOSCloudKeychainConstants.h>
//#import "PeerListCell.h"

static const uint64_t maxTimeToWaitInSeconds = 30ull * NSEC_PER_SEC;

static bool testClearAll(void *sender)
{
    __block bool result = false;
    dispatch_queue_t processQueue = dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0);
    dispatch_semaphore_t waitSemaphore = dispatch_semaphore_create(0);
    dispatch_time_t finishTime = dispatch_time(DISPATCH_TIME_NOW, maxTimeToWaitInSeconds);

    SOSCloudKeychainClearAll(processQueue, ^(CFDictionaryRef returnedValues, CFErrorRef error)
        {
            result = true;
//            secerror("SOSCloudKeychainClearAll returned: %@", error);
            dispatch_async(dispatch_get_main_queue(),
            ^{
                NSLog(@"Updating because of notification");
            });

            dispatch_semaphore_signal(waitSemaphore);
        });
    
	dispatch_semaphore_wait(waitSemaphore, finishTime);
//ARC	dispatch_release(waitSemaphore);
//    secerror("SOSCloudKeychainClearAll exit");
    return result;
}

static void apply_block_1(const void *value, void *context)
{
    return ((__bridge void (^)(const void *value))context)(value);
}

static inline void CFArrayForEach(CFArrayRef array, void (^operation)(const void *value)) {
    CFArrayApplyFunction(array, CFRangeMake(0, CFArrayGetCount(array)), apply_block_1, (__bridge void *)(operation));
}

static void dumpCircleInfo()
{
    CFErrorRef error = NULL;
    CFArrayRef applicantPeerInfos = NULL;
    CFArrayRef peerInfos = NULL;
    int idx;
    
    NSArray *ccmsgs = @[@"Error", @"InCircle", @"NotInCircle", @"RequestPending", @"CircleAbsent"  ];
    
    SOSCCStatus ccstatus = SOSCCThisDeviceIsInCircle(&error);
    NSLog(@"ccstatus: %d, error: %@", ccstatus, error);
    idx = ccstatus-kSOSCCError;
    if (0<=idx && idx<(int)[ccmsgs count])
        NSLog(@"ccstatus: %d (%@)", ccstatus, ccmsgs[idx]);
    
    // Now look at current applicants
    applicantPeerInfos = SOSCCCopyApplicantPeerInfo(&error);
    if (applicantPeerInfos)
    {
        NSLog(@"Applicants: %ld, error: %@", (long)CFArrayGetCount(applicantPeerInfos), error);
        CFArrayForEach(applicantPeerInfos, ^(const void *value) {
            SOSPeerInfoRef peer = (SOSPeerInfoRef)value;
            CFStringRef peerName = SOSPeerInfoGetPeerName(peer);
            NSLog(@"Applicant: %@", peerName);
        });
    }
    else
        NSLog(@"No applicants, error: %@", error);
    
    
    peerInfos = SOSCCCopyPeerPeerInfo(&error);
    if (peerInfos)
    {
        NSLog(@"Peers: %ld, error: %@", (long)CFArrayGetCount(peerInfos), error);
        CFArrayForEach(peerInfos, ^(const void *value) {
            SOSPeerInfoRef peer = (SOSPeerInfoRef)value;
            CFStringRef peerName = SOSPeerInfoGetPeerName(peer);
            NSLog(@"Peer: %@", peerName);
        });
    }
    else
        NSLog(@"No peers, error: %@", error);
}


@interface ToolsViewController ()
@end

@implementation ToolsViewController

- (void)viewDidLoad
{
    [super viewDidLoad];
	// Do any additional setup after loading the view, typically from a nib.
    [self setStatus:@"Idle…"];
}

- (void)didReceiveMemoryWarning
{
    [super didReceiveMemoryWarning];
    // Dispose of any resources that can be recreated.
}

- (void)addPasswordItem:(NSString *)account service:(NSString *)service password:(NSString *) thePassword
{
    [[MyKeychain sharedInstance] setPasswordFull:account service:service password:thePassword];
}

- (IBAction)handleAutoPopulate:(id)sender
{
    [self addPasswordItem:@"12345678" service:@"Evernote" password:@"fiord42/sate"];
    [self addPasswordItem:@"acct2433" service:@"SwissBank" password:@"nerd0)sorely"];
    [self addPasswordItem:@"QR49BZQ77" service:@"Wells Fargo" password:@"per1}bargirl"];
    [self addPasswordItem:@"03991993-9291" service:@"Bank of America" password:@"dabs35\angst"];
    [self addPasswordItem:@"followme" service:@"Twitter" password:@"mica86[board"];
    [self addPasswordItem:@"j18373@apple.com" service:@"Mail" password:@"macro13:VIII"];
    [self addPasswordItem:@"j18373" service:@"Facebook" password:@"vow5:karakul"];
    [self addPasswordItem:@"lonely22" service:@"G+Circles" password:@"vclub17'earls"];
    [self addPasswordItem:@"yoyo9182" service:@"Skype" password:@"Andy137#FAQs"];
    [self addPasswordItem:@"terminator3828" service:@"Blizzard" password:@"David95?hive"];
}

- (IBAction)handleClearKeychain:(id)sender
{
    NSLog(@"Clear All Keychain Items");
    [[MyKeychain sharedInstance] clearAllKeychainItems];
}

- (IBAction)handleClearKVS:(id)sender
{
    testClearAll((__bridge void *)(self));
}

- (IBAction)resetToEmpty:(id)sender
{
    SOSCCResetToEmpty(NULL);
}

- (IBAction)handleDumpButton:(id)sender
{
    dumpCircleInfo();
}

- (IBAction)handleSync:(id)sender
{
    //SOSCCSyncWithAllPeers();
}

//bool 
- (void)setStatus:(NSString *)message
{
    NSLog(@"%@", message);
    _statusMessage.text = message;
}

@end

