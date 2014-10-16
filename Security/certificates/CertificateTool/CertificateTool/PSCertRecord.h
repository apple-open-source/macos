//
//  PSCertRecord.h
//  CertificateTool
//
//  Copyright (c) 2012-2013 Apple Inc. All Rights Reserved.
//

#import <Foundation/Foundation.h>
#import 

@interface PSCertRecord : NSObject
{
@private
	NSData*	_cert_record;
}
@property (readonly) NSData* cert_record;

- (id)initWithCertData:(NSData *)cert_data withFlags:(NSNumber *)flags;

@end