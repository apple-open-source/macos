//
//  AppDelegate.m
//  DesktopSample
//
//  Created by Love Hörnquist Åstrand on 2011-11-13.
//  Copyright (c) 2011 __MyCompanyName__. All rights reserved.
//

#import "AppDelegate.h"
#import <GSS/GSSItem.h>

@implementation AppDelegate

@synthesize window = _window;
@synthesize tableview = _tableview;
@synthesize credentials = _credentials;
@synthesize arrayController = _arrayController;

- (void)applicationDidFinishLaunching:(NSNotification *)aNotification
{
	[self refreshCredentials:nil];
}

- (IBAction)refreshCredentials:(id)sender
{
	_credentials = [[NSMutableArray alloc] init];
	
	CFMutableDictionaryRef attrs = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
	
	CFDictionaryAddValue(attrs, kGSSAttrClass, kGSSAttrClassKerberos);
	
	CFErrorRef error = NULL;
	
	CFArrayRef items = GSSItemCopyMatching(attrs, &error);
	if (items) {
		CFIndex n, count = CFArrayGetCount(items);
		for (n = 0; n < count; n++) {
			CFTypeRef item = CFArrayGetValueAtIndex(items, n);
			NSLog(@"item %d = %@", (int)n, item);
			
			NSDictionary *i;
			
			i = [(__bridge NSDictionary *)item mutableCopy];
			[i setValue:@"expire1" forKey:@"kGSSAttrTransientExpire"];
			NSLog(@"%@ %@", i, [i className]);
			[_credentials addObject:i];
		}
		CFRelease(items);
	}
	CFRelease(attrs);
	
	[_credentials addObject:@{ @"kGSSAttrNameDisplay" : @"foo", @"kGSSAttrTransientExpire" : @"expire"}];
	
	NSLog(@"%@", _credentials);
	
	[_arrayController setContent:_credentials];
	
	NSLog(@"item %@", [_arrayController valueForKeyPath:@"arrangedObjects.kGSSAttrNameDisplay"]);
	NSLog(@"item %@", [_arrayController valueForKeyPath:@"arrangedObjects.kGSSAttrTransientExpire"]);
	
	[_tableview reloadData];
	
	
}
@end
