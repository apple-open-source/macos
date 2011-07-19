/*
 *  main.mm
 *  CommonCrypto
 *
 *  Created by James Murphy on 10/28/10.
 *  Copyright 2010 __MyCompanyName__. All rights reserved.
 *
 */

#import <Foundation/Foundation.h>
#import "CommonCryptoUnitTests.h"
#import "DigestTest.h"
#import "CommonDigest.h"
#import "CommonCryptor.h"
#import "CommonHMAC.h"
#import "RandomNumberService.h"
#import "EncryptionTest.h"
#import "HMACTest.h"
#import "PBKDFTest.h"
#import "SymmetricWrapTest.h"
#import "CommonRandomSPI.h"

int main (int argc, const char * argv[]) 
{
    NSAutoreleasePool * pool = [[NSAutoreleasePool alloc] init];
	
	NSArray* digestTests = [CCDigestTestObject setupDigestTests:nil];
	
	for (CCDigestTestObject* testObject in digestTests)
	{
		[testObject runTest];
	}
	
	NSArray* encryptionTests = [CCEncryptionTest setupEncryptionTests:nil];
	
	for (CCEncryptionTest* aTest in encryptionTests)
	{
		[aTest runTest];
	}
	
	NSArray* hmacTests = [CCHMACTestObject setupHMACTests:nil];
	
	for (CCHMACTestObject* aTest in hmacTests)
	{
		[aTest runTest];
	}
	
	NSArray* pbkdfTests = [CCDerviationTestObject setupPBKDFTests:nil];
	
	for (CCDerviationTestObject* aTest in pbkdfTests)
	{
		[aTest runTest];
	}
	
	NSArray* SymmetricWrapTests = [CCSymmetricalWrapTest setupSymmWrapTests:nil];
	
	for (CCSymmetricalWrapTest* aTest in SymmetricWrapTests)
	{
		[aTest runTest];
	}
	
	uint8_t bytes[1024];
    uint8_t previous[1024];
    int i;
    
    bzero(previous, 1024);
    for(i = 0; i < 1024; i++) 
	{
        int retval =  CCRandomCopyBytes(kCCRandomDefault, bytes, 1024);
        if(retval) 
		{
            printf("CCRandomCopyBytes: Failed");
        }
        
        if(memcmp(previous, bytes, 1024) == 0) 
		{
            printf("CCRandomCopyBytes: Failed");
        }
        
        memcpy(previous, bytes, 1024);
    }
	
	[pool drain];
	return 0;
}
	
	
	
	

