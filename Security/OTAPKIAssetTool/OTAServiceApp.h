//
//  OTAServiceApp.h
//  Security
//
//  Created by local on 2/11/13.
//
//

#import <Foundation/Foundation.h>
#import <dispatch/dispatch.h>



@interface OTAServiceApp : NSObject
{
	NSArray*		_file_list;
	NSString*		_manifest_file_name;
	NSString*		_asset_version_file_name;
	NSNumber*		_current_asset_version;
	NSNumber*		_next_asset_version;
	NSString*		_current_asset_directory;
	NSString*		_assets_directory;
	NSFileManager*	_fileManager;
    uid_t           _uid;			/* user uid */
	gid_t           _gid;
	CFTimeInterval  _asset_query_retry_interval;
	bool			_verbose;
}

@property (readonly) NSArray* file_list;
@property (readonly) NSString* manifest_file_name;
@property (readonly) NSString* asset_version_file_name;
@property (readonly) NSNumber* current_asset_version;
@property (readonly) NSNumber* next_asset_version;
@property (readonly) NSString* current_asset_directory;
@property (readonly) NSString* assets_directory;
@property (readonly) NSFileManager* fileManager;
@property (readonly) uid_t uid;
@property (readonly) gid_t gid;


- (id)init:(int)argc withArguments:(const char**)argv;

- (void)checkInWithActivity;

@end
