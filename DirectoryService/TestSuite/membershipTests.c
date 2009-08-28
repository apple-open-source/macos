/*
 * Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
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

#include <dispatch/dispatch.h>
#include <OpenDirectory/OpenDirectory.h>
#include <membership.h>
#include <membershipPriv.h>
#include <sysexits.h>
#include <servers/bootstrap.h>
#include "DirServicesConstPriv.h"

static int printResult( CFErrorRef *inError, const char *message, bool inPass )
{
	int errorCode = EX_USAGE;
	
	if ( inPass == true )
	{
		CFStringRef cfString = CFStringCreateWithFormat( kCFAllocatorDefault, NULL, CFSTR("%s - PASS"), message );
		if ( cfString != NULL ) {
			CFShow( cfString );
			CFRelease( cfString );
		}
	}
	
	if ( (inError != NULL && (*inError) != NULL) || inPass == false )
	{
		if ( inPass == true )
		{
			CFStringRef cfError = CFErrorCopyDescription( *inError );
			CFStringRef cfString = CFStringCreateWithFormat( kCFAllocatorDefault, NULL, CFSTR("%s - FAIL (%@)"), message, cfError );
			if ( cfString != NULL ) {
				CFShow( cfString );
				CFRelease( cfString );
			}
			
			CFRelease( cfError );
			
			// this is temporary until ODFramework has some kind of ranges for error types
			CFIndex errorCode = CFErrorGetCode( *inError );
			if ( errorCode >= kODErrorCredentialsInvalid && errorCode < kODErrorCredentialsInvalid+999 ) {
				errorCode = EX_NOPERM;
			}
		}
		else
		{
			CFStringRef cfString = CFStringCreateWithFormat( kCFAllocatorDefault, NULL, CFSTR("%s - FAIL (no error returned)"), message );
			if ( cfString != NULL ) {
				CFShow( cfString );
				CFRelease( cfString );
			}
		}
		
		if ( inError != NULL && (*inError) != NULL ) {
			// we null the pointer
			CFRelease( *inError );
			*inError = NULL;
		}
	}
	
	
	return errorCode;
}

extern mach_port_t _mbr_port;
extern int _ds_running(void);

int main( int argc, char *argv[] )
{
	struct passwd			*entry;
	int						ii;
	CFErrorRef				error;
	CFMutableDictionaryRef	cfAttribs = CFDictionaryCreateMutable( kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks );
	
	if ( geteuid() != 0 ) {
		printf( "Please run as root\n" );
		exit( EX_USAGE );
	}
	
	// hack to redirect to the debug daemon
	if ( getenv("DS_DEBUG_MODE") ) {
		_ds_running();
		bootstrap_look_up( bootstrap_port, kDSStdMachMembershipPortName"Debug", &_mbr_port );
	}

	ODNodeRef nodeRef = ODNodeCreateWithNodeType( kCFAllocatorDefault, kODSessionDefault, kODNodeTypeLocalNodes, &error );
	assert( nodeRef != NULL );
	
	ODRecordRef everyoneGroup = ODNodeCopyRecord( nodeRef, kODRecordTypeGroups, CFSTR("everyone"), NULL, NULL );
	assert( everyoneGroup != NULL );
	
	ODRecordRef (^createRecord)(ODRecordType, CFStringRef, int32_t, int32_t) = ^(ODRecordType recType, CFStringRef namePrefix, int32_t theID, int32_t pgid) {
		CFMutableDictionaryRef cfAttribs = CFDictionaryCreateMutable( kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, 
																	  &kCFTypeDictionaryValueCallBacks );
		CFArrayRef cfTempAttrib;

		if ( pgid != 0 ) {
			CFStringRef cfGID = CFStringCreateWithFormat( kCFAllocatorDefault, NULL, CFSTR("%d"), pgid );
			cfTempAttrib = CFArrayCreate( kCFAllocatorDefault, (CFTypeRef *) &cfGID, 1, &kCFTypeArrayCallBacks );
			CFRelease( cfGID );
			
			CFDictionarySetValue( cfAttribs, kODAttributeTypePrimaryGroupID, cfTempAttrib );
			CFRelease( cfTempAttrib );
		}
		
		CFStringRef cfID = CFStringCreateWithFormat( kCFAllocatorDefault, NULL, CFSTR("%d"), theID );
		cfTempAttrib = CFArrayCreate( kCFAllocatorDefault, (CFTypeRef *) &cfID, 1, &kCFTypeArrayCallBacks );
		CFRelease( cfID );
		
		CFStringRef cfIDType = (CFEqual(recType, kODRecordTypeGroups) ? kODAttributeTypePrimaryGroupID : kODAttributeTypeUniqueID);
		
		CFDictionarySetValue( cfAttribs, cfIDType, cfTempAttrib );
		CFRelease( cfTempAttrib );
		
		CFStringRef groupName = CFStringCreateWithFormat( kCFAllocatorDefault, NULL, CFSTR("%@_%u"), namePrefix, theID );
		ODRecordRef tempGroup = ODNodeCopyRecord( nodeRef, recType, groupName, NULL, NULL );
		CFErrorRef localError = NULL;
		
		if ( tempGroup != NULL ) {
			ODRecordDelete( tempGroup, NULL );
			CFRelease( tempGroup );
		}
		
		tempGroup = ODNodeCreateRecord( nodeRef, recType, groupName, cfAttribs, &localError );
		if ( tempGroup == NULL ) {
			printResult( &localError, "Error creating a record", false );
			exit( -1 );
		}
		
		CFRelease( groupName );
		
		return tempGroup;
	};
	
	int32_t (^createGroups)(ODRecordRef *, size_t, CFStringRef, int32_t) = ^(ODRecordRef *array, size_t count, CFStringRef recName, int32_t startID ) {
		
		dispatch_apply( count,
					    dispatch_get_concurrent_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT), 
						^(size_t index) {
							array[index] = createRecord( kODRecordTypeGroups, recName, startID+index, 0 );
						} );
		
		return (int32_t) (startID + count);
	};

	void (^deleteGroups)(ODRecordRef *, size_t) = ^(ODRecordRef *array, size_t count ) {
		dispatch_apply( count,
						dispatch_get_concurrent_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT), 
					    ^(size_t index) {
							ODRecordDelete( array[index], NULL );
						} );
	};
	
	void (^syncGroups)(ODRecordRef *, size_t) = ^(ODRecordRef *array, size_t count ) {
		dispatch_apply( count,
					    dispatch_get_concurrent_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT), 
					    ^(size_t index) {
							ODRecordSynchronize( array[index], NULL );
					    } );
	};
	
	int32_t nextID = getpid() << 16 & 0x0fffffff;
	ODRecordRef testgroupNest[4];
	ODRecordRef testcircularGroup[2];
	ODRecordRef testcyclicMesh[3][5];
	
	// create groups
	nextID = createGroups( testgroupNest, sizeof(testgroupNest) / sizeof(ODRecordType), CFSTR("testgroupNest"), nextID );
	nextID = createGroups( testcircularGroup, sizeof(testcircularGroup) / sizeof(ODRecordType), CFSTR("testcircularGroup"), nextID );
	nextID = createGroups( &testcyclicMesh[0][0], 5, CFSTR("testcyclicMesh1"), nextID );
	nextID = createGroups( &testcyclicMesh[1][0], 5, CFSTR("testcyclicMesh2"), nextID );
	nextID = createGroups( &testcyclicMesh[2][0], 5, CFSTR("testcyclicMesh3"), nextID );
	
	ODRecordRef testnestedEveryone = createRecord( kODRecordTypeGroups, CFSTR("testnestedEveryone"), nextID++, 0 );
	ODRecordRef testmemberGroup = createRecord( kODRecordTypeGroups, CFSTR("testmemberGroup"), nextID++, 0 );
	
	// must be the last group created because the GID is kept for PGID
	ODRecordRef testPrimaryGID = createRecord( kODRecordTypeGroups, CFSTR("testPrimaryGID"), nextID, 0 );

	// create nested group setup -- Nest4 -> Nest3 -> Nest2 -> Nest1
	printResult( &error, "Adding testgroupNest3 to testgroupNest4", ODRecordAddMember(testgroupNest[3], testgroupNest[2], &error) );
	printResult( &error, "Adding testgroupNest2 to testgroupNest3", ODRecordAddMember(testgroupNest[2], testgroupNest[1], &error) );
	printResult( &error, "Adding testgroupNest1 to testgroupNest2", ODRecordAddMember(testgroupNest[1], testgroupNest[0], &error) );

	// create a circular group
	printResult( &error, "Adding testcircularGroup2 to testcircularGroup1", ODRecordAddMember(testcircularGroup[0], testcircularGroup[1], &error) );
	printResult( &error, "Adding testcircularGroup1 to testcircularGroup2", ODRecordAddMember(testcircularGroup[1], testcircularGroup[0], &error) );

	// nest everyone in a group
	printResult( &error, "Adding testnestedEveryone to everyoneGroup", ODRecordAddMember(testnestedEveryone, everyoneGroup, &error) );
	printf( "\n" );

	// create users
	int32_t nextUID = nextID;
	ODRecordRef testUser = createRecord( kODRecordTypeUsers, CFSTR("testUser"), nextUID, nextID );
	ODRecordRef testUserNested = createRecord( kODRecordTypeUsers, CFSTR("testNested"), ++nextUID, nextID );
	ODRecordRef testUserCircular = createRecord( kODRecordTypeUsers, CFSTR("testCircular"), ++nextUID, nextID );
	ODRecordRef testUserConflictID = createRecord( kODRecordTypeUsers, CFSTR("testUserConflictID"), ++nextUID, nextID );
	ODRecordRef testUserConflictID2 = createRecord( kODRecordTypeUsers, CFSTR("testUserConflictID2"), nextUID, nextID );
	ODRecordRef testUserCyclicMesh = createRecord( kODRecordTypeUsers, CFSTR("testUserCyclicMesh"), ++nextUID, nextID );
	
	printResult( &error, "Adding testUser to testmemberGroup", ODRecordAddMember(testmemberGroup, testUser, &error) );
	printResult( &error, "Adding testUserNested to testgroupNest2", ODRecordAddMember(testgroupNest[1], testUserNested, &error) );
	printResult( &error, "Adding testUserCircular to testcircularGroup1", ODRecordAddMember(testcircularGroup[0], testUserCircular, &error) );
	
	ODRecordAddMember( testcyclicMesh[2][0], testcyclicMesh[0][4], NULL );
	ODRecordAddMember( testcyclicMesh[2][4], testcyclicMesh[1][0], NULL );

	ODRecordAddMember( testcyclicMesh[1][3], testcyclicMesh[0][2], NULL );
	
	ODRecordAddMember( testcyclicMesh[0][1], testcyclicMesh[1][4], NULL );
	ODRecordAddMember( testcyclicMesh[0][4], testcyclicMesh[1][2], NULL );

	int yy, zz;
	for ( yy = 0; yy < 3; yy++ )
	{
		ODRecordAddMember( testcyclicMesh[yy][0], testUserCyclicMesh, NULL );
		ODRecordSynchronize( testcyclicMesh[yy][0], NULL );
		for ( zz = 0; zz < 4; zz++ )
		{
			ODRecordAddMember( testcyclicMesh[yy][zz+1], testcyclicMesh[yy][zz], NULL );
			ODRecordSynchronize( testcyclicMesh[yy][zz+1], NULL );
		}
	}
	
	printf( "\n" );
	
	// we have to synchronize the group changes otherwise DS won't necessarily detect the change
	syncGroups( testgroupNest, sizeof(testgroupNest) / sizeof(ODRecordRef) );
	syncGroups( testcircularGroup, sizeof(testcircularGroup) / sizeof(ODRecordRef) );
	ODRecordSynchronize( testnestedEveryone, NULL );
	ODRecordSynchronize( testmemberGroup, NULL );
	ODRecordSynchronize( testPrimaryGID, NULL );
	
	// now test memberships
	// ODRecordContainsMember - uses membership APIs
	printResult( &error, "testUser (is member of 'everyone')", ODRecordContainsMember(everyoneGroup, testUser, &error) );
	printResult( &error, "testUser (is member of 'testPrimaryGID')", ODRecordContainsMember(testPrimaryGID, testUser, &error) );
	printResult( &error, "testUser (is member of 'testmemberGroup')", ODRecordContainsMember(testmemberGroup, testUser, &error) );
	printResult( &error, "testUser (is member of 'testnestedEveryone')", ODRecordContainsMember(testnestedEveryone, testUser, &error) );
	printResult( &error, "testUser (is NOT member of 'testmemberNest1')", ODRecordContainsMember(testgroupNest[0], testUser, &error) == false );
	printf( "\n" );

	printResult( &error, "testUserNested (is member of 'everyone')", ODRecordContainsMember(everyoneGroup, testUserNested, &error) );
	printResult( &error, "testUserNested (is member of 'testPrimaryGID')", ODRecordContainsMember(testPrimaryGID, testUserNested, &error) );
	printResult( &error, "testUserNested (is NOT member of 'testmemberGroup')", ODRecordContainsMember(testmemberGroup, testUserNested, &error) == false );
	printResult( &error, "testUserNested (is member of 'testnestedEveryone')", ODRecordContainsMember(testnestedEveryone, testUserNested, &error) );
	printResult( &error, "testUserNested (is NOT member of 'testmemberNest1')", ODRecordContainsMember(testgroupNest[0], testUserNested, &error) == false );
	printResult( &error, "testUserNested (is member of 'testmemberNest2')", ODRecordContainsMember(testgroupNest[1], testUserNested, &error) );
	printResult( &error, "testUserNested (is member of 'testmemberNest3')", ODRecordContainsMember(testgroupNest[2], testUserNested, &error) );
	printResult( &error, "testUserNested (is member of 'testmemberNest4')", ODRecordContainsMember(testgroupNest[3], testUserNested, &error) );
	printf( "\n" );

	printResult( &error, "testUserCircular (is member of 'everyone')", ODRecordContainsMember(everyoneGroup, testUserCircular, &error) );
	printResult( &error, "testUserCircular (is member of 'testPrimaryGID')", ODRecordContainsMember(testPrimaryGID, testUserCircular, &error) );
	printResult( &error, "testUserCircular (is NOT member of 'testmemberGroup')", ODRecordContainsMember(testmemberGroup, testUserCircular, &error) == false );
	printResult( &error, "testUserCircular (is member of 'testnestedEveryone')", ODRecordContainsMember(testnestedEveryone, testUserCircular, &error) );
	printResult( &error, "testUserCircular (is member of 'testcircularGroup1')", ODRecordContainsMember(testcircularGroup[0], testUserCircular, &error) );
	printResult( &error, "testUserCircular (is member of 'testcircularGroup2')", ODRecordContainsMember(testcircularGroup[1], testUserCircular, &error) );
	printf( "\n" );

	printResult( &error, "testUserConflictID (is member of 'everyone')", ODRecordContainsMember(everyoneGroup, testUserConflictID, &error) );
	printResult( &error, "testUserConflictID (is member of 'testPrimaryGID')", ODRecordContainsMember(testPrimaryGID, testUserConflictID, &error) );
	printResult( &error, "testUserConflictID (is NOT member of 'testmemberGroup')", ODRecordContainsMember(testmemberGroup, testUserConflictID, &error) == false );
	printResult( &error, "testUserConflictID (is member of 'testnestedEveryone')", ODRecordContainsMember(testnestedEveryone, testUserConflictID, &error) );
	printf( "\n" );

	for ( yy = 0; yy < 3; yy++ )
	{
		for ( zz = 0; zz < 5; zz++ )
		{
			printResult( &error, "testcyclicMesh (is member of each nested group)", ODRecordContainsMember(testcyclicMesh[yy][zz], testUserCyclicMesh, &error) );
		}
	}	
	
	printResult( &error, "testUserConflictID2 (is member of 'everyone')", ODRecordContainsMember(everyoneGroup, testUserConflictID2, &error) );
	printResult( &error, "testUserConflictID2 (is member of 'testPrimaryGID')", ODRecordContainsMember(testPrimaryGID, testUserConflictID2, &error) );
	printResult( &error, "testUserConflictID2 (is NOT member of 'testmemberGroup')", ODRecordContainsMember(testmemberGroup, testUserConflictID2, &error) == false );
	printResult( &error, "testUserConflictID2 (is member of 'testnestedEveryone')", ODRecordContainsMember(testnestedEveryone, testUserConflictID2, &error) );
	printf( "\n" );
	
	printf( "Check system.log or DirectoryService.error.log for misconfiguration errors for conflictusers\n\n" );
	
	// TODO: more tests
	// conflict group/user (UUID)
	// conflict group/user (name) -- more difficult because we have name conflict guards in dslocal
	// groups with only UUID memberships
	// groups with only name memberships
	
	// test SID logic
	uuid_t uu;
	char username[256];
	char groupname[256];
	
	CFStringRef cfUserName = ODRecordGetRecordName( testUser );
	if ( CFStringGetCString(cfUserName, username, sizeof(username), kCFStringEncodingUTF8) == true ) {
		int rc = mbr_user_name_to_uuid( username, uu );
		printResult( NULL, "mbr_user_name_to_uuid (testUser to UUID)", rc == 0 );
		
		if ( rc == 0 ) {
			nt_sid_t origsid;
			nt_sid_t sid1, sid2, sid3;
			int id_type;
			
			rc = mbr_uuid_to_sid_type( uu, &origsid, &id_type );
			printResult( NULL, "mbr_uuid_to_sid_type (testUser to SID)", rc == 0 );
			printResult( NULL, "sid type is SID_TYPE_USER", id_type == SID_TYPE_USER );
			
			printResult( &error, "\nSetting SMBPrimaryGroupSID on testUser", ODRecordSetValue(testUser, kODAttributeTypeSMBPrimaryGroupSID, CFSTR("S-1-5-21-234"), &error) );
			ODRecordSynchronize( testUser, NULL );
			
			rc = mbr_uuid_to_sid_type( uu, &sid1, &id_type );
			printResult( NULL, "mbr_uuid_to_sid_type (testUser to SID)", rc == 0 );
			if ( rc == 0 ) {
				printResult( NULL, "SIDs are same", memcmp(&sid1, &origsid, sizeof(nt_sid_t)) == 0 );
			}
			
			ODRecordRemoveValue( testUser, kODAttributeTypeSMBPrimaryGroupSID, CFSTR("S-1-5-21-234"), NULL );
			printResult( &error, "\nSetting SMBGroupRID on testUser", ODRecordSetValue(testUser, kODAttributeTypeSMBGroupRID, CFSTR("234"), &error) );
			ODRecordSynchronize( testUser, NULL );
			
			rc = mbr_uuid_to_sid_type( uu, &sid2, &id_type );
			printResult( NULL, "mbr_uuid_to_sid_type (testUser to SID)", rc == 0 );
			if ( rc == 0 ) {
				printResult( NULL, "SIDs are same", memcmp(&sid2, &origsid, sizeof(nt_sid_t)) == 0 );
			}
			
			printResult( &error, "\nSetting SMBRID", ODRecordSetValue(testUser, kODAttributeTypeSMBRID, CFSTR("235"), &error) );
			ODRecordSynchronize( testUser, NULL );
			rc = mbr_uuid_to_sid_type( uu, &sid3, &id_type );
			if ( rc == 0 ) {
				printResult( NULL, "SIDs are different", memcmp(&sid3, &origsid, sizeof(nt_sid_t)) != 0 );
			}
		}
	}
	
	CFStringRef cfGroupName = ODRecordGetRecordName( testPrimaryGID );
	if ( CFStringGetCString(cfGroupName, groupname, sizeof(groupname), kCFStringEncodingUTF8) == true ) {
		int rc = mbr_group_name_to_uuid( groupname, uu );
		printResult( NULL, "\nmbr_group_name_to_uuid (testPrimaryGID to UUID)", rc == 0 );
		
		if ( rc == 0 ) {
			nt_sid_t origsid;
			nt_sid_t sid1, sid2, sid3;
			int id_type;
			
			rc = mbr_uuid_to_sid_type( uu, &origsid, &id_type );
			printResult( NULL, "mbr_uuid_to_sid_type (testPrimaryGID to SID)", rc == 0 );
			printResult( NULL, "sid type is SID_TYPE_GROUP", id_type == SID_TYPE_GROUP );
			
			printResult( &error, "\nSetting SMBPrimaryGroupSID on testPrimaryGID", ODRecordSetValue(testPrimaryGID, kODAttributeTypeSMBPrimaryGroupSID, CFSTR("S-1-5-21-234"), &error) );
			ODRecordSynchronize( testPrimaryGID, NULL );
			
			rc = mbr_uuid_to_sid_type( uu, &sid1, &id_type );
			printResult( NULL, "mbr_uuid_to_sid_type (testPrimaryGID to SID)", rc == 0 );
			if ( rc == 0 ) {
				printResult( NULL, "SIDs are different", memcmp(&sid1, &origsid, sizeof(nt_sid_t)) != 0 );
			}
			
			ODRecordRemoveValue( testPrimaryGID, kODAttributeTypeSMBPrimaryGroupSID, CFSTR("S-1-5-21-234"), NULL );
			printResult( &error, "\nSetting SMBGroupRID on testPrimaryGID", ODRecordSetValue(testPrimaryGID, kODAttributeTypeSMBGroupRID, CFSTR("234"), &error) );
			ODRecordSynchronize( testPrimaryGID, NULL );
			
			rc = mbr_uuid_to_sid_type( uu, &sid2, &id_type );
			printResult( NULL, "mbr_uuid_to_sid_type (testPrimaryGID to SID)", rc == 0 );
			if ( rc == 0 ) {
				printResult( NULL, "SIDs are different", memcmp(&sid2, &sid1, sizeof(nt_sid_t)) != 0 );
			}
			
			ODRecordRemoveValue( testPrimaryGID, kODAttributeTypeSMBGroupRID, CFSTR("234"), NULL );
			printResult( &error, "\nSetting SMBRID", ODRecordSetValue(testPrimaryGID, kODAttributeTypeSMBRID, CFSTR("235"), &error) );
			ODRecordSynchronize( testPrimaryGID, NULL );
			rc = mbr_uuid_to_sid_type( uu, &sid3, &id_type );
			if ( rc == 0 ) {
				printResult( NULL, "SIDs are different", memcmp(&sid3, &sid2, sizeof(nt_sid_t)) != 0 );
			}
		}
	}
	
	ODRecordDelete( testUser, NULL );
	ODRecordDelete( testUserNested, NULL );
	ODRecordDelete( testUserCircular, NULL );
	ODRecordDelete( testUserCyclicMesh, NULL );

	deleteGroups( testgroupNest, sizeof(testgroupNest) / sizeof(ODRecordRef) );
	deleteGroups( testcircularGroup, sizeof(testcircularGroup) / sizeof(ODRecordRef) );
	ODRecordDelete( testnestedEveryone, NULL );
	ODRecordDelete( testmemberGroup, NULL );
	ODRecordDelete( testPrimaryGID, NULL );

	ODRecordDelete( testUserConflictID, NULL );
	ODRecordDelete( testUserConflictID2, NULL );
	
	for ( yy = 0; yy < 3; yy++ )
	{
		for ( zz = 0; zz < 5; zz++ )
		{
			ODRecordDelete( testcyclicMesh[yy][zz], NULL );
		}
	}
}
