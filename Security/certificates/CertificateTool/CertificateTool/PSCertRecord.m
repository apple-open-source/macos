//
//  PSCertRecord.m
//  CertificateTool
//
//  Created by James Murphy on 12/19/12.
//  Copyright (c) 2012 James Murphy. All rights reserved.
//

#import "PSCertRecord.h"

@interface PSCertRecord (PrivateMethod)

- (BOOL)ceateCertRecord:(NSData *)cert_data withFlags:(NSNumber *)flags

@end

@implementation PSCertRecord


- (id)initWithCertData:(NSData *)cert_data withFlags:(NSNumber *)flags
{
	if ((self = [super init]))
	{
		_cert_record = nil;
		if (![self ceateCertRecord:cert_data withFlags:flags])
		{
			NSLog(@"Could not create the certificate record");
			_cert_record = nil;
		}
	}
	return self;
}

- (BOOL)ceateCertRecord:(NSData *)cert_data withFlags:(NSNumber *)flags
{
	BOOL result = NO;

	if (nil == cert_data)
	{
		return result;
	}

	UInt32 flag_value = 0;
	if (nil != flags)
	{
		flag_value = (UInt32)[flags unsignedIntValue];
	}

	


}

@end