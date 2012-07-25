//
//  Copyright (c) 2012 Apple. All rights reserved.
//

#import "GKAppDelegate.h"
#import "Authority.h"

#import <Security/SecAssessment.h>
#import <notify.h>
#import <CoreServices/CoreServices.h>

@implementation GKAppDelegate

@synthesize window = _window;
@synthesize authority = _authority;
@synthesize tableView = _tableView;


- (NSUInteger)countOfAuthority
{
    return [self.authority count];
}

- (id)objectInAuthorityAtIndex:(NSUInteger)index
{
    return [self.authority objectAtIndex:index];
}
- (void)insertObject:(id)anObject inAuthorityAtIndex:(NSUInteger)index
{
    [self.authority insertObject:anObject atIndex:index];
}
- (void)removeObjectFromAuthorityAtIndex:(NSUInteger)index
{
    [self.authority removeObjectAtIndex:index];
}

- (void)deleteAuthority:(NSTableView *)tableView atIndexes:(NSIndexSet *)indexes
{
    [[[self.arrayController arrangedObjects] objectsAtIndexes:indexes] enumerateObjectsUsingBlock:^(Authority *auth, NSUInteger idx, BOOL *stop) {

	NSDictionary *query = @{
	    (__bridge id)kSecAssessmentContextKeyUpdate : (__bridge id)kSecAssessmentUpdateOperationRemove,
	    (__bridge id)kSecAssessmentUpdateKeyAuthorization : self.authData,
	};
	
	if (SecAssessmentUpdate((__bridge CFTypeRef)auth.identity, 0, (__bridge CFDictionaryRef)query, NULL))
	    [self.arrayController removeObject:auth];
	else
	    NSBeep();
    }];
}

- (BOOL)doTargetQuery:(CFTypeRef)target withAttributes:(NSDictionary *)attributes
{
    NSMutableDictionary *query = [NSMutableDictionary dictionaryWithDictionary:attributes];
    
    [query setObject:self.authData forKey:(__bridge id)kSecAssessmentUpdateKeyAuthorization];
    
    BOOL ret = SecAssessmentUpdate(target, 0, (__bridge CFDictionaryRef)query, NULL);
    if (ret)
	[self updateAuthorities];
    return ret;
}

- (BOOL)addTargetURL:(NSURL *)url
{
    NSDictionary *query = @{
	(__bridge id)kSecAssessmentContextKeyUpdate : (__bridge id)kSecAssessmentUpdateOperationAdd,
    };
    return [self doTargetQuery:(__bridge CFTypeRef)url withAttributes:query];
}

- (IBAction)addApplication:(NSButtonCell *)sender{

    NSOpenPanel *panel = [NSOpenPanel openPanel];
    
    [panel setAllowedFileTypes:@[ (__bridge id)kUTTypeApplication ]];
    
    [panel beginSheetModalForWindow:self.window completionHandler:^void(NSInteger result) {
	NSLog(@"complete");
	if (result != NSFileHandlingPanelOKButton)
	    return;
	
	[self addTargetURL:[panel URL]];
    }];
}

- (NSDragOperation)tableView:(NSTableView*)pTableView
                validateDrop:(id )info 
                 proposedRow:(NSInteger)row
       proposedDropOperation:(NSTableViewDropOperation)op
{
    return NSDragOperationEvery;
}


- (BOOL)tableView:(NSTableView *)pTableView
       acceptDrop:(id )info
	      row:(NSInteger)pRow
    dropOperation:(NSTableViewDropOperation)operation
{
    NSPasteboard* pboard = [info draggingPasteboard];

    if ([[pboard types] containsObject:NSURLPboardType]) {
	if ([self addTargetURL:[NSURL URLFromPasteboard:pboard]]) {
	    return YES;
	} else {
	    NSBeep();
	    return NO;
	}
    }
    
    return NO;
}


- (void)awakeFromNib {
    self.authority = [NSMutableArray array];
    self.sortDescriptors = @[[[NSSortDescriptor alloc] initWithKey:@"remarks" ascending:YES]];
}

- (void)updateAuthorities
{
    NSDictionary *query = @{
	(__bridge id)kSecAssessmentContextKeyUpdate : (__bridge id)kSecAssessmentUpdateOperationFind,
	(__bridge id)kSecAssessmentUpdateKeyAuthorization : self.authData,
    };
    
    NSDictionary *result = (__bridge NSDictionary *)SecAssessmentCopyUpdate(NULL, 0, (__bridge CFDictionaryRef)query, NULL);
    if (result == NULL)
	return;
    
    NSArray *array = [result objectForKey:(__bridge id)kSecAssessmentUpdateKeyFound];
    NSMutableDictionary *dict = [NSMutableDictionary dictionary];

    [array enumerateObjectsUsingBlock:^(NSDictionary *obj, NSUInteger idx, BOOL *stop) {
	[dict setObject:obj forKey:[obj objectForKey:(__bridge id)kSecAssessmentRuleKeyID]];
    }];

    [[self.arrayController arrangedObjects] enumerateObjectsUsingBlock:^(Authority *obj, NSUInteger idx, BOOL *stop) {
	if ([dict objectForKey:obj.identity])
	    [dict removeObjectForKey:obj.identity];
    }];

    [dict enumerateKeysAndObjectsUsingBlock:^(NSNumber *identity, NSDictionary *obj, BOOL *stop) {
	Authority *auth = [[Authority alloc] initWithAssessment:obj];
	[self insertObject:auth inAuthorityAtIndex:0];
    }];
}


- (void)applicationDidFinishLaunching:(NSNotification *)aNotification
{
    AuthorizationCreate(NULL, NULL, kAuthorizationFlagDefaults, &self->_authRef);
    AuthorizationExternalForm extForm;
    if (AuthorizationMakeExternalForm(self.authRef, &extForm) == noErr) {
	self.authData = [NSData dataWithBytes:&extForm length:sizeof(extForm)];
    }

    [self updateAuthorities];
    
    int token;
    notify_register_dispatch(kNotifySecAssessmentUpdate, &token, dispatch_get_main_queue(), ^void(int foo){
	[self updateAuthorities];
    });

    [self.tableView registerForDraggedTypes:@[NSFilenamesPboardType]];

}

- (BOOL)applicationShouldTerminateAfterLastWindowClosed: (NSApplication *)theApplication
{
    return YES;
}

@end
