//
//  PSCertData.h
//  CertificateTool
//
//  Copyright (c) 2012-2015 Apple Inc. All Rights Reserved.
//

#import <Foundation/Foundation.h>

@interface PSCertData : NSObject
{
@private
    NSArray*        _certs;
    NSData*         _cert_index_data;
    NSData*         _cert_table;
}

@property (readonly)NSData* cert_index_data;
@property (readonly)NSData* cert_table;

- (id)initWithCertificates:(NSArray *)certs;


@end
