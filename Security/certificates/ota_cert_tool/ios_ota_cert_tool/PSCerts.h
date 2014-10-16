//
//  PSCerts.h
//  ios_ota_cert_tool
//
//  Created by James Murphy on 12/11/12.
//  Copyright (c) 2012 James Murphy. All rights reserved.
//

#import <Foundation/Foundation.h>

@interface PSCerts : NSObject
{
    NSString* _cert_dir_path;
    NSMutableArray* _certs;

}

@property (readonly) NSArray* certs;

- (id)initWithCertFilePath:(NSString *)filePath forBadCerts:(BOOL)forBad;


@end
