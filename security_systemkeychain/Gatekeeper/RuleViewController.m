//
//  Copyright (c) 2012 Apple. All rights reserved.
//

#import "RuleViewController.h"
#import <Security/SecAssessment.h>

@implementation RuleViewController

- (IBAction)disableRuleButton:(NSButton *)sender
{
    id value = nil;
    if ([sender state]) {
	value = (__bridge id)kSecAssessmentUpdateOperationEnable;
    } else {
	value = (__bridge id)kSecAssessmentUpdateOperationDisable;
    }

    NSDictionary *query = @{
	(__bridge id)kSecAssessmentContextKeyUpdate : value,
    };


    [[NSApp delegate] doTargetQuery:(__bridge CFTypeRef)self.objectValue.identity withAttributes:query];
}

@end
