//
//  SyncViewController.m
//  Keychain
//
//  Created by john on 10/22/12.
//
//

#import "SyncViewController.h"
#import "MyKeychain.h"

#import <CKBridge/SOSCloudKeychainClient.h>
#import <SecureObjectSync/SOSCloudCircle.h>
#import <SecureObjectSync/SOSCloudCircleInternal.h>
#import <notify.h>
#import <dispatch/dispatch.h>

#import <CoreFoundation/CoreFoundation.h>
#import <CoreFoundation/CFUserNotification.h>

#import <QuartzCore/QuartzCore.h>
#import <Regressions/SOSTestDataSource.h>
#import <securityd/SOSCloudCircleServer.h>
#import <CKBridge/SOSCloudKeychainConstants.h>
#import "PeerListCell.h"
#import <utilities/SecCFRelease.h>

__unused static const uint64_t maxTimeToWaitInSeconds = 30ull * NSEC_PER_SEC;

@interface SyncViewController ()
@end

@implementation SyncViewController

- (void)viewDidLoad
{
    [super viewDidLoad];
	// Do any additional setup after loading the view, typically from a nib.
    [self setStatus:@"Idle…"];
    [self updateSyncingEnabledSwitch];

    notify_register_dispatch(kSOSCCCircleChangedNotification, &notificationToken,
        dispatch_get_main_queue(),
        ^(int tokenx __unused) {
            notificationCount++;
            [self setStatus:@"Got circle changed notification."];
            [self flashChangeLight];
            [self updateSyncingEnabledSwitch];
            [self updateMemberCounts];
            [_peerList reloadData];
            [_applicantList reloadData];
        });
    [_acceptButton setEnabled:NO];

    [self updateStatusCircleColor];

//    _stateChangedC.color = [UIColor redColor];
//    [_stateChangedC setNeedsDisplay];
}

- (void)didReceiveMemoryWarning
{
    [super didReceiveMemoryWarning];
    // Dispose of any resources that can be recreated.
}

- (void)updateSyncingEnabledSwitch
{
    // Set the visual state of switch based on membership in circle
    CFErrorRef error = NULL;
    SOSCCStatus ccstatus = SOSCCThisDeviceIsInCircle(&error);
    BOOL switchIsOn = (ccstatus == kSOSCCInCircle || ccstatus == kSOSCCRequestPending);
    [_syncingEnabled setOn:switchIsOn animated:NO];

    CFStringRef circleStatusStr = SOSCCGetStatusDescription(ccstatus);
    [_circleStatus setText:CFBridgingRelease(circleStatusStr)];
    [_updateCount setText:[NSString stringWithFormat:@"%d", notificationCount]];
    
    // TODO: Maybe update spinny for pending?!?

    NSLog(@"ccstatus: %@ (%d), error: %@", SOSCCGetStatusDescription(ccstatus), ccstatus, error);
}

- (void)updateStatusCircleColor
{
    switch (SOSCCThisDeviceIsInCircle(NULL))
    {
        case kSOSCCInCircle:
            _stateChangedC.color = [UIColor greenColor];
            break;
        case kSOSCCRequestPending:
            _stateChangedC.color = [UIColor yellowColor];
            break;
        default:
            _stateChangedC.color = [UIColor redColor];
            break;
    }
    [_stateChangedC setNeedsDisplay];
}

- (void)updateMemberCounts
{
    CFArrayRef foundApplicants = SOSCCCopyApplicantPeerInfo(NULL);
    CFIndex applicantCount = foundApplicants ? CFArrayGetCount(foundApplicants) : -1;
    [_applicantCount setText:[NSString stringWithFormat:@"%ld", (long)applicantCount]];

    CFArrayRef foundPeers = SOSCCCopyPeerPeerInfo(NULL);
    CFIndex peerCount = foundPeers ? CFArrayGetCount(foundPeers) : -1;
    [_peerCount setText:[NSString stringWithFormat:@"%ld", (long)peerCount]];

    [_acceptButton setEnabled:(applicantCount > 0)? YES: NO];
    
    [self updateStatusCircleColor];

    CFReleaseSafe(foundApplicants);
    CFReleaseSafe(foundPeers);
}

+ (void)requestToJoinCircle
{
    // Set the visual state of switch based on membership in circle
    bool bx = true;
    CFErrorRef error = NULL;
    SOSCCStatus ccstatus = SOSCCThisDeviceIsInCircle(&error);

    switch (ccstatus) {
        case kSOSCCCircleAbsent:
            bx = SOSCCResetToOffering(&error);
            break;
        case kSOSCCNotInCircle:
            bx = SOSCCRequestToJoinCircle(&error);
            break;
        default:
            NSLog(@"Request to join circle with bad status:  %@ (%d)", SOSCCGetStatusDescription(ccstatus), ccstatus);
            break;
    }
    if (!bx)
        NSLog(@"requestToJoinCircle Error: %@", error);
}

- (IBAction)acceptAllApplicants:(id)sender
{
    CFArrayRef applicants = SOSCCCopyApplicantPeerInfo(NULL);
    if (applicants) {
        SOSCCAcceptApplicants(applicants, NULL);
        CFRelease(applicants);
    }
}

- (IBAction)handleEnableSyncing:(id)sender
{
    dispatch_queue_t workq = dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0);
    if ([sender isOn])  // i.e. we are trying to turn on syncing
    {
        dispatch_async(workq, ^ {
            NSLog(@"Keychain syncing is being turned ON");
            [[self class] requestToJoinCircle];
        });
    }
    else
    {
        dispatch_async(workq, ^ {
            NSLog(@"Keychain syncing is being turned OFF");
            CFErrorRef error = NULL;
            bool bx = SOSCCRemoveThisDeviceFromCircle(&error);
            if (!bx)
                NSLog(@"SOSCCRemoveThisDeviceFromCircle: %@", error);
        });
    }
}

- (void)flashChangeLight
{
    CABasicAnimation *theAnimation = NULL;

    theAnimation=[CABasicAnimation animationWithKeyPath:@"opacity"];
    theAnimation.duration=0.75;
    theAnimation.repeatCount=5;    //HUGE_VALF;
    theAnimation.autoreverses=YES;
    theAnimation.fromValue=[NSNumber numberWithFloat:1.0];
    theAnimation.toValue=[NSNumber numberWithFloat:0.0];
    [_stateChangedC.layer addAnimation:theAnimation forKey:@"animateOpacity"];
}

- (void)setStatus:(NSString *)message
{
    NSLog(@"%@", message);
    _statusMessage.text = message;
}

//
// MARK: Table view handling
//
- (UITableViewCell *)tableView:(UITableView *)tableView cellForRowAtIndexPath:(NSIndexPath *)indexPath
{
    PeerListCell *cell = [tableView dequeueReusableCellWithIdentifier:@"PeerTableCell" forIndexPath:(NSIndexPath *)indexPath];
    if (cell == nil)
    {
        NSLog(@"cellForRowAtIndexPath : cell was nil");
        cell = [[PeerListCell alloc] initWithStyle:UITableViewCellStyleDefault reuseIdentifier:@"PeerTableCell"];
    }

    NSArray *list = NULL;
    NSArray *hilighted = NULL;
    if (tableView == _peerList) {
        list = (__bridge_transfer NSArray*)SOSCCCopyPeerPeerInfo(NULL);
        hilighted = (__bridge_transfer NSArray*)SOSCCCopyConcurringPeerPeerInfo(NULL);
    } else {
        list = (__bridge_transfer NSArray*)SOSCCCopyApplicantPeerInfo(NULL);
    }

    if (list) {
        cell.peerCircle.text = @"A";
        SOSPeerInfoRef pi = (__bridge SOSPeerInfoRef) list[[indexPath row]];
        if (pi) {
            cell.peerName.text = (__bridge NSString*) SOSPeerInfoGetPeerName(pi);
            if ([hilighted containsObject: (__bridge id)pi])
                cell.peerName.textColor = [UIColor greenColor];
        }
    } else {
        cell.peerName.text = @"Null List";
    }
    
    return cell;
}

- (NSInteger)tableView:(UITableView *)tableView numberOfRowsInSection:(NSInteger)section
{
    NSArray* list = nil;
    
    if (tableView == _peerList) {
        list = (__bridge_transfer NSArray*) SOSCCCopyPeerPeerInfo(NULL);
    } else {
        list = (__bridge_transfer NSArray*) SOSCCCopyApplicantPeerInfo(NULL);
    }
    
    return [list count];
}

@end

