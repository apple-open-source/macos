//
//  Copyright (c) 2012 Apple. All rights reserved.
//

#import <Foundation/Foundation.h>

@interface Authority : NSObject

@property (strong) NSNumber *identity;

@property (strong) NSString *label;
@property (strong) NSNumber *disabled;
@property (strong) NSString *remarks;
@property (strong) NSString *codeRequirement;
@property (strong) NSData *bookmark;

@property (readonly) NSImage *icon;
@property (readonly) NSString *description;

- (Authority *)initWithAssessment:(NSDictionary *)assessment;
- (void)updateWithAssessment:(NSDictionary *)assessment;

//id
//type
//requirement
//allow
//disabled
//expires
//label
//flags
//ctime
//mtime
//bookmark
//icon
//user

@end
