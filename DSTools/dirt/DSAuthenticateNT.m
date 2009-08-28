/*
 * Copyright (c) 2000 - 2003 Apple Computer, Inc. All rights reserved.
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

/*!
 * @header DSAuthenticateNT
 */


#import "DSAuthenticateNT.h"
#import <openssl/rand.h>
#import <DirectoryServiceCore/SMBAuth.h>

extern BOOL doVerbose;

void
print_as_hex( void *bin, int len )
{
	int idx;
	
	for ( idx = 0; idx < len; idx++ )
		printf( "%.2X ", ((unsigned char *)bin)[idx] );

	printf("\n");
}

@implementation DSAuthenticateNT

- (tDirStatus)authenticateInNode:(tDirNodeReference)userNode username:(NSString*)inUsername
						password:(NSString*)inPassword
{
    NSAutoreleasePool      *pool			= [[NSAutoreleasePool alloc] init];
    tDataBufferPtr			step			= NULL;
    tDataBufferPtr			stepResponse	= NULL;
    tDataNodePtr			authMethod		= NULL;
    UInt32					length			= 0;
    UInt32					current			= 0;
    tDirStatus				status			= 0;
	unsigned char			hash[21]		= {0};
	unsigned char			p24[24]			= {0};
	unsigned char			challenge[8]	= {0};

    [self allocateDataBuffer:&step withNumberOfBlocks:1 shouldReallocate: NO];
    [self allocateDataBuffer:&stepResponse withNumberOfBlocks:4 shouldReallocate: NO];
    authMethod = dsDataNodeAllocateString(_DirRef, kDSStdAuthSMB_NT_Key);

	// Prepare data derived from password to pass to dsDoDirNodeAuth()
	RAND_bytes(challenge, 8);
    if(doVerbose)
	{
		printf("---> challenge generated(): ");
		print_as_hex( challenge, 8 );
		fflush(stdout);
	}
	
	CalculateSMBNTHash([inPassword UTF8String], hash);
	bzero( hash+16, 5);
    if(doVerbose)
	{
		printf("---> hash generated(): ");
		print_as_hex( hash, 16 );
		fflush(stdout);
	}
	
	CalculateP24( hash, challenge, p24 );	// pick a random challenge, can be anything.
    if(doVerbose)
	{
		printf("---> p24 generated(): ");
		print_as_hex( p24, 24 );
		fflush(stdout);
	}
		
    length = strlen( [inUsername UTF8String] );
    memcpy( &(step->fBufferData[current]), &length, sizeof(length));
    current += sizeof(length);
    memcpy( &(step->fBufferData[current]), [inUsername UTF8String], length );
    current +=length;

    length = 8;
    memcpy( &(step->fBufferData[current]), &length, sizeof(length));
    current += sizeof(length);
	memcpy( &(step->fBufferData[current]), challenge, length);
    current +=length;

	length = 24;
    memcpy( &(step->fBufferData[current]), &length, sizeof(length));
    current += sizeof(length);
	memcpy( &(step->fBufferData[current]), p24, length);
	
    step->fBufferLength = current + length;

    if(doVerbose)
	{
		printf("---> dsDoDirNodeAuth() ......................... ");
		fflush(stdout);
	}
    status = dsDoDirNodeAuth(userNode, authMethod, 1, step, stepResponse, NULL);
    if(doVerbose)
	{
		[_dsStat printOutErrorMessage:"Status" withStatus:status];
	}

    printf("Username: %s\nPassword: %s\n", [inUsername cString], [inPassword cString]);
    if(status == eDSNoErr)
		printf("Good");
    else
		[_dsStat printOutErrorMessage:"Error" withStatus:status];

    // Clean up allocated memory
    //dsCloseDirNode(userNode); ///  don't close since not opened here
    [self deallocateDataNode:authMethod];
    [self deallocateDataBuffer:step];
    [self deallocateDataBuffer:stepResponse];
    [pool release];
    return (status);
}

@end
