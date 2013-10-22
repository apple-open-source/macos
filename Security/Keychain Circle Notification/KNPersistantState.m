//
//  KNPersistantState.m
//  Security
//
//  Created by J Osborne on 7/28/13.
//
//

#import "KNPersistantState.h"

@implementation KNPersistantState

-(NSURL*)urlForStorage
{
	return [NSURL URLWithString:@"Preferences/com.apple.security.KCN.plist" relativeToURL:[[NSFileManager defaultManager] URLForDirectory:NSLibraryDirectory inDomain:NSUserDomainMask appropriateForURL:nil create:YES error:nil]];
}

+(instancetype)loadFromStorage
{
	KNPersistantState *state = [[KNPersistantState alloc] init];
    if (!state) {
        return state;
    }
    
    id plist = @{@"lastWritten": [NSDate distantPast]};

    NSError *error = nil;
    NSData *stateData = [NSData dataWithContentsOfURL:[state urlForStorage] options:0 error:&error];
    if (!stateData) {
        NSLog(@"Can't read state data (p=%@, err=%@)", [state urlForStorage], error);
    } else {
        NSPropertyListFormat format;
        plist = [NSPropertyListSerialization propertyListWithData:stateData options: NSPropertyListMutableContainersAndLeaves format:&format error:&error];
        
        if (plist == nil) {
            NSLog(@"Can't deserialize %@, e=%@", stateData, error);
        }
    }
    
    state.lastCircleStatus = plist[@"lastCircleStatus"] ? [plist[@"lastCircleStatus"] intValue] : kSOSCCCircleAbsent;
    state.lastWritten = plist[@"lastWritten"];
	state.pendingApplicationReminderInterval = plist[@"pendingApplicationReminderInterval"];
	state.debugLeftReason = plist[@"debugLeftReason"];
	state.pendingApplicationReminder = plist[@"pendingApplicationReminder"];
	state.applcationDate = plist[@"applcationDate"];
	if (!state.applcationDate) {
		state.applcationDate = [NSDate distantPast];
	}
	if (!state.pendingApplicationReminder) {
		state.pendingApplicationReminder = [NSDate distantFuture];
	}
    if (!state.pendingApplicationReminderInterval || [state.pendingApplicationReminderInterval doubleValue] <= 0) {
        state.pendingApplicationReminderInterval = [NSNumber numberWithUnsignedInt:60 * 60 * 24 * 2];
    }
    
    return state;
}

-(void)writeToStorage
{
    NSMutableDictionary *plist = [@{@"lastCircleStatus": [NSNumber numberWithInt:self.lastCircleStatus],
									@"lastWritten": [NSDate date],
									@"applcationDate": self.applcationDate,
									@"pendingApplicationReminder": self.pendingApplicationReminder,
                            } mutableCopy];
	if (self.debugLeftReason) {
		plist[@"debugLeftReason"] = self.debugLeftReason;
	}
	if (self.pendingApplicationReminderInterval) {
		plist[@"pendingApplicationReminderInterval"] = self.pendingApplicationReminderInterval;
	}
    NSLog(@"writeToStorage plist=%@", plist);
	
    NSError *error = nil;
    NSData *stateData = [NSPropertyListSerialization dataWithPropertyList:[plist copy] format:NSPropertyListXMLFormat_v1_0 options:kCFPropertyListImmutable error:&error];
    if (!stateData) {
        NSLog(@"Can't serialize %@: %@", plist, error);
        return;
    }
    if (![stateData writeToURL:[self urlForStorage] options:NSDataWritingAtomic error:&error]) {
        NSLog(@"Can't write to %@, error=%@", [self urlForStorage], error);
    }
}


@end
