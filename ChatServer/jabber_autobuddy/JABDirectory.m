//
//  JABDirectory.m
//  ChatServer/jabber_autobuddy
//
//  Created by peralta on 7/18/08.
//  Copyright 2008 Apple. All rights reserved.
//

#import "JABDirectory.h"

#import <OpenDirectory/NSOpenDirectory.h>

@implementation JABDirectory

@synthesize searchScope = _searchScope;
@synthesize directorySession = _directorySession;
@synthesize directoryNode = _directoryNode;

//--------------------------------------------------------------------------------
+ (id) jabDirectory
{
	return [[[self alloc] initWithScope: DIRECTORYSCOPE_UNDEFINED] autorelease];
}

//--------------------------------------------------------------------------------
+ (id) jabDirectoryWithScope: (NSInteger) iScope
{
	return [[[self alloc] initWithScope: iScope] autorelease];
}

//--------------------------------------------------------------------------------
+ (NSString *) getRecordName: (NSObject *) aRec 
{
	// Helper function for abstracting the different record classes 
	// between DSOjbCWrappers and NSOpenDirectory:
	// - For 10.6 and later, aRec is an ODRecord
	// - For 10.5 and earlier, aRec is an NSDictionary

	ODRecord *odRec = (ODRecord *) aRec;
	return [odRec recordName];
}

//--------------------------------------------------------------------------------
+ (NSString *) getAttributeValue: (NSString *) attribKey fromRecord: (NSObject *) aRec 
{
	// Helper function for abstracting the different record classes 
	// between DSOjbCWrappers and NSOpenDirectory:
	// - For 10.6 and later, aRec is an ODRecord
	// - For 10.5 and earlier, aRec is an NSDictionary

	NSArray *attrVals = nil;
	ODRecord *odRec = (ODRecord *) aRec;
	NSError *odErr = nil;
	attrVals = [odRec valuesForAttribute: attribKey error: &odErr];
	if (nil == attrVals) return nil; // attribute not found in record
	return (0 < [attrVals count]) ? [attrVals objectAtIndex: 0] : nil;
}

#pragma mark -
//--------------------------------------------------------------------------------
- (id) initWithScope: (NSInteger) iScope
{
	self = [super init];
	
	[self openNodeWithScope: iScope];
	
	return self;
}

- (void) dealloc
{
	[self closeNode];
	
	[super dealloc];
}

#pragma mark -
//--------------------------------------------------------------------------------
- (void) openLocalNode
{
	[self openNodeWithScope: DIRECTORYSCOPE_LOCAL];
}

//--------------------------------------------------------------------------------
- (void) openSearchNode
{
	[self openNodeWithScope: DIRECTORYSCOPE_SEARCH];
}

//--------------------------------------------------------------------------------
- (void) openNodeWithScope: (NSInteger) iScope
{
	// Open a new directoryNode based on the requested scope (local or search)

	// WARNING: There is only one active directoryNode instance.  Opening a 
	//          new directoryNode will terminate any existing node!
	//
	// NOTE:    Callers should use [SMPServer closeDirectoryNode] to terminate
	//          the active directoryNode as soon as DS interaction is complete
	//          to insure that all DS-allocated resources are released.

	[self closeNode]; // terminate active node (if any)

	self.searchScope = iScope; // define the scope of a new DS node
	// Create a DS node of the selected scope
	do { // not a loop
		
		if (DIRECTORYSCOPE_UNDEFINED == iScope) 
			break; // scope undefined -- nothing to open
		
		// Make a connection to the DS service
		self.directorySession = [ODSession defaultSession];
		if (nil == self.directorySession)
			break; // can't access DS -- abort

		// Create the appropriately scoped directory node
		ODNode *aNode = nil;
		switch (iScope) {
			case DIRECTORYSCOPE_SEARCH: 
				aNode = [ODNode nodeWithSession: self.directorySession
										   type: kODNodeTypeAuthentication
										  error: nil]; 
				break;
			case DIRECTORYSCOPE_LOCAL:
				aNode = [ODNode nodeWithSession: self.directorySession
										   type: kODNodeTypeLocalNodes
										  error: nil]; 
				break;
			default: ; // DIRECTORYSCOPE_UNDEFINED
		} // switch
		self.directoryNode = aNode;
	} while (0); // not a loop
}

//--------------------------------------------------------------------------------
- (void) closeNode
{
	self.directoryNode = nil;
	self.directorySession = nil;
	self.searchScope = DIRECTORYSCOPE_UNDEFINED;
}

//--------------------------------------------------------------------------------
- (BOOL) isNodeOpen
{
	return (nil != self.directoryNode);
}

#pragma mark -
//--------------------------------------------------------------------------------
- (NSString *) findAttribute: (NSString *) attrName inRecord: (NSString *) recName ofType: (NSString *) recType
{
	// Search the current scope for a attribute in the specified record.
	
	NSString *attribText = nil;
	
	do { // not a loop
		
		// Read legacy config data from OpenDirectory
		if (![self isNodeOpen]) 
			break; // no open node -- abort

		NSError *queryErr = nil;
		ODQuery *aQuery = [ODQuery queryWithNode: self.directoryNode 
		                          forRecordTypes: recType
		                               attribute: attrName
		                               matchType: kODMatchAny 
		                             queryValues: nil // (id) inQueryValueOrList - what's this?
		                        returnAttributes: attrName
		                          maximumResults: 0
		                                   error: &queryErr];

		NSArray *recList = [aQuery resultsAllowingPartial: NO error: &queryErr];
		attribText = [recList objectAtIndex: 0];

	} while (0); // not a loop
	
	return attribText;
}

//--------------------------------------------------------------------------------
- (NSArray *) findAllRecordsOfType: (NSString *) recType
{
	// Search the current scope for all records matching the specified type.
	
	NSArray *recList = nil;

	do { // not a loop

		if (![self isNodeOpen]) 
			break; // no open node -- abort

		NSError *queryErr = nil;
		ODQuery *aQuery = [ODQuery queryWithNode: self.directoryNode 
		                          forRecordTypes: recType
		                               attribute: nil
		                               matchType: kODMatchAny 
		                             queryValues: nil // (id) inQueryValueOrList - what's this?
		                        returnAttributes: nil
		                          maximumResults: 0
		                                   error: &queryErr];
		
		recList = [aQuery resultsAllowingPartial: NO error: &queryErr];
		
	} while (0); // not a loop
		
	return recList;
}

//--------------------------------------------------------------------------------
- (NSString *) findGroupNameForGeneratedUID: (NSString *) groupGuid
{
	// Given a group's GeneratedUID, return the group's Real Name (or record name if a Real Name
	// is not available)

	NSString *groupName = nil;
	do { // not a loop

		if (![self isNodeOpen]) {
			break; // no open node -- abort
		}
		NSError *queryErr = nil;
		ODQuery *aQuery = [ODQuery queryWithNode: self.directoryNode
                                  forRecordTypes: kODRecordTypeGroups
                                       attribute: kODAttributeTypeGUID
                                       matchType: kODMatchEqualTo
                                     queryValues: groupGuid
								returnAttributes: nil
								  maximumResults: 0
                                           error: &queryErr];

		NSArray *recList = [aQuery resultsAllowingPartial: NO error: &queryErr];
		for (ODRecord *aRec in recList) {
			NSError *anErr = nil;
			NSArray *attribVals = [aRec valuesForAttribute: kODAttributeTypeFullName error: &anErr];
			// If the group  has a full name defined, use it
			if (nil != attribVals) {
				groupName = [attribVals objectAtIndex: 0];
			} else {
				// Otherwise, fall back to the group's record name
				attribVals = [aRec valuesForAttribute: kODAttributeTypeRecordName error: &anErr];
				if (nil != attribVals)
					groupName = [attribVals objectAtIndex: 0];
			}
			break; // done
		}

	} while (0); // not a loop

	return groupName;
}


@end
