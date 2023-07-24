#if OCTAGON

#import "OTTapToRadarAdapter.h"
#import "utilities/debugging.h"

#import <TapToRadarKit/TapToRadarKit.h>
#import <os/feature_private.h>

@implementation OTTapToRadarActualAdapter

- (id)init {
    if((self = [super init])) {

    }
    return self;
}

- (void)postHomePodLostTrustTTR {
    if([TapToRadarService class] == nil) {
        secnotice("octagon-ttr", "Trust lost, but TTR service not available");
        return;
    }

    if(!os_feature_enabled(Security, TTRTrustLossOnHomePod)) {
        secnotice("octagon-ttr", "Trust lost, not posting TTR due to feature flag");
        return;
    }

    secnotice("octagon-ttr", "Trust lost, posting TTR");

    RadarDraft* draft = [[RadarDraft alloc] init];
    draft.component = [[RadarComponent alloc] initWithName:@"Security" version:@"iCloud Keychain" identifier:606179];
    draft.isUserInitiated = NO;
    draft.reproducibility = ReproducibilityNotApplicable;
    draft.remoteDeviceClasses = RemoteDeviceClassesiPhone |
    RemoteDeviceClassesiPad |
    RemoteDeviceClassesMac |
    RemoteDeviceClassesAppleWatch |
    RemoteDeviceClassesAppleTV |
    RemoteDeviceClassesHomePod;
    draft.remoteDeviceSelections = RemoteDeviceSelectionsHomeKitHome;
    draft.title = @"Lost CDP trust";

    draft.problemDescription = @"HomePod unexpectedly lost CDP trust (please do not file this radar if you performed Reset Protected Data on another device, or otherwise intended to cause CDP trust loss on this HomePod). To disable this prompt for testing, turn off the Security/TTRTrustLossOnHomePod feature flag on the HomePod.";
    draft.classification = ClassificationOtherBug;

    TapToRadarService* s = [TapToRadarService shared];
    [s createDraft:draft forProcessNamed:@"CDP" withDisplayReason:@"HomePod lost CDP/Manatee access" completionHandler:^(NSError * _Nullable error) {
        if(error == nil) {
            secnotice("octagon", "Created TTR successfully");
        } else {
            secnotice("octagon", "Created TTR with error: %@", error);
        }
    }];
}

@end

#endif // OCTAGON
