/*
 * Copyright (c) 2013-2014 Apple Inc. All Rights Reserved.
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
 */


#import "KDAppDelegate.h"
#import "KDCirclePeer.h"
#import "NSArray+mapWithBlock.h"
#include <notify.h>

#include <Security/SecItemInternal.h>

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

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-variable"
    int notificationToken;
    uint32_t rc = notify_register_dispatch(kSecServerKeychainChangedNotification, &notificationToken, dispatch_get_main_queue(), ^(int token __unused) {
            NSLog(@"Received %s", kSecServerKeychainChangedNotification);
            [(KDSecItems*)self.itemDataSource loadItems];
            [self.itemTable reloadData];
         });
    NSAssert(rc == 0, @"Can't register for %s", kSecServerKeychainChangedNotification);
#pragma clang diagnostic pop
	
	self.circle = [KDSecCircle new];

    __weak typeof(self) weakSelf = self;
	[self.circle addChangeCallback:^{
        __strong typeof(self) strongSelf = weakSelf;
        if(strongSelf) {
            strongSelf.circleStatusCell.stringValue = strongSelf.circle.status;

            [strongSelf setCheckbox];

            strongSelf.peerCountCell.objectValue = @(strongSelf.circle.peers.count);
            NSString *peerNames = [[strongSelf.circle.peers mapWithBlock:^id(id obj) {
                return ((KDCirclePeer*)obj).name;
            }] componentsJoinedByString:@"\n"];
            [strongSelf.peerTextList.textStorage replaceCharactersInRange:NSMakeRange(0, [strongSelf.peerTextList.textStorage length]) withString:peerNames];

            strongSelf.applicantCountCell.objectValue = @(strongSelf.circle.applicants.count);
            NSString *applicantNames = [[strongSelf.circle.applicants mapWithBlock:^id(id obj) {
                return ((KDCirclePeer*)obj).name;
            }] componentsJoinedByString:@"\n"];
            [strongSelf.applicantTextList.textStorage replaceCharactersInRange:NSMakeRange(0, [strongSelf.applicantTextList.textStorage length]) withString:applicantNames];

            [strongSelf.syncSpinner stopAnimation:nil];
        }
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
