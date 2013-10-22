//
//  KDAppDelegate.m
//  Keychain
//
//  Created by J Osborne on 2/13/13.
//
//

#import "KDAppDelegate.h"
#import "KDCirclePeer.h"
#import "NSArray+mapWithBlock.h"
#include <notify.h>

#define kSecServerKeychainChangedNotification "com.apple.security.keychainchanged"

@implementation KDAppDelegate

- (void)applicationDidFinishLaunching:(NSNotification *)aNotification
{
    self.stuffNotToLeak = [NSMutableArray new];
    [self.stuffNotToLeak addObject:[[NSNotificationCenter defaultCenter] addObserverForName:kKDSecItemsUpdated object:nil queue:[NSOperationQueue mainQueue] usingBlock:^(NSNotification *note) {
        self.itemTableTitle.title = [NSString stringWithFormat:@"All Items (%ld)", (long)[self.itemDataSource numberOfRowsInTableView:self.itemTable]];
    }]];
    
    [self.syncSpinner setUsesThreadedAnimation:YES];
    [self.syncSpinner startAnimation:nil];
    
    self.itemDataSource = [[KDSecItems alloc] init];
    self.itemTable.dataSource = self.itemDataSource;
    
    int notificationToken;
    uint32_t rc = notify_register_dispatch(kSecServerKeychainChangedNotification, &notificationToken, dispatch_get_main_queue(), ^(int token __unused) {
            NSLog(@"Received %s", kSecServerKeychainChangedNotification);
            [(KDSecItems*)self.itemDataSource loadItems];
            [self.itemTable reloadData];
         });
    NSAssert(rc == 0, @"Can't register for %s", kSecServerKeychainChangedNotification);
	
	self.circle = [KDSecCircle new];
	[self.circle addChangeCallback:^{
		self.circleStatusCell.stringValue = self.circle.status;
        
        [self setCheckbox];
        
		self.peerCountCell.objectValue = @(self.circle.peers.count);
		NSString *peerNames = [[self.circle.peers mapWithBlock:^id(id obj) {
			return ((KDCirclePeer*)obj).name;
		}] componentsJoinedByString:@"\n"];
		[self.peerTextList.textStorage replaceCharactersInRange:NSMakeRange(0, [self.peerTextList.textStorage length]) withString:peerNames];
        
		self.applicantCountCell.objectValue = @(self.circle.applicants.count);
		NSString *applicantNames = [[self.circle.applicants mapWithBlock:^id(id obj) {
			return ((KDCirclePeer*)obj).name;
		}] componentsJoinedByString:@"\n"];
		[self.applicantTextList.textStorage replaceCharactersInRange:NSMakeRange(0, [self.applicantTextList.textStorage length]) withString:applicantNames];
        
        [self.syncSpinner stopAnimation:nil];
	}];
}

-(void)setCheckbox
{
    if (self.circle.isInCircle) {
        [self.enableKeychainSyncing setState:NSOnState];
    } else if (self.circle.isOutOfCircle) {
        [self.enableKeychainSyncing setState:NSOffState];
    } else {
        [self.enableKeychainSyncing setState:NSMixedState];
    }
}

-(IBAction)enableKeychainSyncingClicked:(id)sender
{
    [self.syncSpinner startAnimation:sender];
    if (self.circle.isOutOfCircle) {
        [self.circle enableSync];
    } else {
        [self.circle disableSync];
    }
    [self setCheckbox];
}

@end
