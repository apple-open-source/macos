//
//  Copyright (c) 2012 Apple. All rights reserved.
//

#import "Authority.h"

#import <Security/SecAssessment.h>

@implementation Authority


- (Authority *)initWithAssessment:(NSDictionary *)assessment
{
    [self updateWithAssessment:assessment];
    return self;
}

- (void)updateWithAssessment:(NSDictionary *)assessment
{
    self.identity = [assessment objectForKey:(__bridge id)kSecAssessmentRuleKeyID];
    self.remarks = [assessment objectForKey:(__bridge id)kSecAssessmentRuleKeyRemarks];
    self.disabled = [assessment objectForKey:(__bridge id)kSecAssessmentRuleKeyDisabled];
    self.codeRequirement = [assessment objectForKey:(__bridge id)kSecAssessmentRuleKeyRequirement];
    self.bookmark = [assessment objectForKey:(__bridge id)kSecAssessmentRuleKeyBookmark];
}

- (NSString *)description
{
    if (self.remarks)
	return self.remarks;
    return @"description here";
}

- (NSImage *)icon
{
    if (self.bookmark == NULL)
	return NULL;
    
    NSURL *url = [NSURL URLByResolvingBookmarkData:self.bookmark options:0 relativeToURL:NULL bookmarkDataIsStale:NULL error:NULL];
 
    NSDictionary *icons = [url resourceValuesForKeys:@[ NSURLEffectiveIconKey, NSURLCustomIconKey ] error:NULL];
    
    NSImage *image = [icons objectForKey: NSURLCustomIconKey];
    if (image)
	return image;

    return [icons objectForKey: NSURLEffectiveIconKey];
}

@end
