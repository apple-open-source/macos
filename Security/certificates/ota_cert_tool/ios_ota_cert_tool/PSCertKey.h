//
//  PSCertKey.h
//  ios_ota_cert_tool
//
//  Created by James Murphy on 12/12/12.
//  Copyright (c) 2012 James Murphy. All rights reserved.
//

#import <Foundation/Foundation.h>

@interface PSCertKey : NSObject
{
@private
    NSString*             _key_hash;
}

@property (readonly) NSString* key_hash;

- (id)initWithCertFilePath:(NSString *)filePath;

@end
