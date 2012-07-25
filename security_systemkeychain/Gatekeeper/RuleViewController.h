//
//  Copyright (c) 2012 Apple. All rights reserved.
//

#import <Cocoa/Cocoa.h>
#import "Authority.h"
#import "GKAppDelegate.h"

@interface RuleViewController : NSTableCellView

@property (strong) Authority *objectValue;

-  (IBAction) disableRuleButton:(NSButton *)sender;

@end
