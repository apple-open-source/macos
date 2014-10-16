//
//  PSIOSCertToolApp.h
//  ios_ota_cert_tool
//
//  Created by James Murphy on 12/11/12.
//  Copyright (c) 2012 James Murphy. All rights reserved.
//

#import <Foundation/Foundation.h>

@interface PSIOSCertToolApp : NSObject
{
@private
	NSString*	_app_name;
	NSString*	_root_directory;
	NSString*	_revoked_directory;
	NSString*	_distrusted_directory;
	NSString*	_certs_directory;
	NSString*	_ev_plist_path;
    NSString*   _info_plist_path;
	NSString*	_top_level_directory;
    NSString*   _outputDir;
    
    NSArray*    _roots;
    NSArray*    _revoked;
    NSArray*    _distrusted;
    NSArray*    _certs;

	NSArray*	_plist_name_array;
    
}

@property (readonly) NSString* app_name;
@property (readonly) NSString* root_directory;
@property (readonly) NSString* revoked_directory;
@property (readonly) NSString* distrusted_directory;
@property (readonly) NSString* certs_directory;
@property (readonly) NSString* ev_plist_path;
@property (readonly) NSString* info_plist_path;
@property (readonly) NSString* top_level_directory;
@property (readonly) NSString* output_directory;

- (id)init:(int)argc withArguments:(const char**)argv;

- (BOOL)processCertificates;

- (BOOL)outputPlistsToDirectory;

- (BOOL)validate;

@end
