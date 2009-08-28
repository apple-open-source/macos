//
//  JABDirectory.h
//  ChatServer/jabber_autobuddy
//
//  Created by peralta on 7/18/08.
//  Copyright 2008 Apple. All rights reserved.
//

#import <Cocoa/Cocoa.h>

#import <DirectoryService/DirectoryService.h>

// constants for directoryScope values
enum {
	DIRECTORYSCOPE_UNDEFINED = -1,
	DIRECTORYSCOPE_LOCAL     =  0,
	DIRECTORYSCOPE_SEARCH    =  1
};

@class ODSession;
@class ODNode;

@interface JABDirectory : NSObject {

	NSInteger _searchScope;       // scope control for DS searches

	ODSession *_directorySession; // Directory Service master context
	ODNode *_directoryNode;       // active DS node

}
@property(assign, readwrite) NSInteger searchScope;
@property(retain, readwrite) ODSession *directorySession;
@property(retain, readwrite) ODNode *directoryNode;

+ (id) jabDirectory;
+ (id) jabDirectoryWithScope: (NSInteger) iScope;

+ (NSString *) getRecordName: (NSObject *) aRec;
+ (NSString *) getAttributeValue: (NSString *) attribKey fromRecord: (NSObject *) aRec;

- (id) initWithScope: (int) iScope;
- (void) dealloc;

// Directory Service access utilities
- (void) openLocalNode;
- (void) openSearchNode;
- (void) openNodeWithScope: (NSInteger) iScope;
- (void) closeNode;
- (BOOL) isNodeOpen;

- (NSString *) findAttribute: (NSString *) attrName inRecord: (NSString *) recName ofType: (NSString *) recType;
- (NSArray *) findAllRecordsOfType: (NSString *) recType;

@end
