//
//  CertificateToolApp.h
//  CertificateTool
//
//  Copyright (c) 2012-2014,2024 Apple Inc. All Rights Reserved.
//

#import <Foundation/Foundation.h>
#import "PSCertData.h"

@interface CertificateToolApp : NSObject
@property (readonly) NSString* app_name;
@property (readonly) NSString* root_directory;
@property (readonly) NSString* custom_directory;
@property (readonly) NSString* platform_directory;
@property (readonly) NSString* test_root_directory;
@property (readonly) NSString* test_platform_directory;
@property (readonly) NSString* revoked_directory;
@property (readonly) NSString* distrusted_directory;
@property (readonly) NSString* allowlist_directory;
@property (readonly) NSString* certs_directory;
@property (readonly) NSString* constraints_config_path;
@property (readonly) NSString* evroot_config_path;
@property (readonly) NSString* ev_plist_path;
@property (readonly) NSString* info_plist_path;
@property (readonly) NSString* top_level_directory;
@property (readonly) NSString* output_directory;
@property (readonly) NSString* version_number_plist_path;
@property (readonly) NSNumber* version_number;

- (id)init:(int)argc withArguments:(const char**)argv;

- (BOOL)processCertificates;

- (BOOL)outputPlistsToDirectory;

- (BOOL)createManifest;


@end
