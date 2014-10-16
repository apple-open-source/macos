//
//  PSCertData.h
//  CertificateTool
//
//  Created by local on 1/24/13.
//  Copyright (c) 2013 James Murphy. All rights reserved.
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
