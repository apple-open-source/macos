//
//  PortStatusController.h
//  USBProber
//
//  Created by Russvogel on 10/14/10.
//  Copyright 2010 Apple. All rights reserved.
//

#import <Cocoa/Cocoa.h>
#import "PortStatusGatherer.h"
#import "OutlineViewNode.h"
#import "OutlineViewAdditions.h"


@interface PortStatusController : NSObject <PortStatusGathererListener>
{

    IBOutlet id PortStatusOutputOV;

	IBOutlet NSButton *fResfreshButton;
	IBOutlet NSButton *fResfreshAutomatically;

	NSTimer *refreshTimer;

    OutlineViewNode *       _rootNode;
	PortStatusGatherer *    _infoGatherer;
}

- (IBAction)Refresh:(id)sender;
- (IBAction)SaveOutput:(id)sender;
- (IBAction)refreshAutomaticallyAction:(id)sender;

@end
