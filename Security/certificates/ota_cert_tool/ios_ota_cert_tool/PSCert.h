//
//  PSCert.h
//  ios_ota_cert_tool
//
//  Created by James Murphy on 12/11/12.
//  Copyright (c) 2012 James Murphy. All rights reserved.
//

#import <Foundation/Foundation.h>


@interface PSCert : NSObject
{
@private
    NSString*             _cert_hash;
}

@property (readonly) NSString* cert_hash;

- (id)initWithCertFilePath:(NSString *)filePath;

@end
