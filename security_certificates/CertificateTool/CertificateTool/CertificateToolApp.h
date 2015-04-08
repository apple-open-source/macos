//
//  CertificateToolApp.h
//  CertificateTool
//
//  Copyright (c) 2012-2014 Apple Inc. All Rights Reserved.
//

#import <Foundation/Foundation.h>
#import "PSCertData.h"

@interface CertificateToolApp : NSObject
{
@private
	NSString*	   		_app_name;

    // file system paths used by this object
	NSString*	   		_root_directory;
	NSString*	   		_revoked_directory;
	NSString*	   		_distrusted_directory;
	NSString*	   		_certs_directory;
    NSString*      		_evroot_config_path;
	NSString*	   		_ev_plist_path;
    NSString*      		_info_plist_path;
	NSString*	   		_top_level_directory;
    //NSString*      		_outputDir;
    NSString*           _version_number_plist_path;
    NSNumber*           _version_number;

    PSCertData*         _certRootsData;

    NSMutableDictionary* _EVRootsData;
	NSMutableArray*		_blacked_listed_keys;
    NSMutableArray*     _gray_listed_keys;
	NSData*				_derData;
}

@property (readonly) NSString* app_name;
@property (readonly) NSString* root_directory;
@property (readonly) NSString* revoked_directory;
@property (readonly) NSString* distrusted_directory;
@property (readonly) NSString* certs_directory;
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
