#pragma once

#import <DirectoryService/DirServicesTypes.h>

@class PathManager;

@interface DSCLPlugInHelper : NSObject
{
@private
	BOOL			fInteractive;
	PathManager*	fEngine;
}
-(BOOL) isInteractive;
-(void) currDirRef:(tDirReference*) dirRef nodeRef:(tDirNodeReference*) nodeRef recType:(NSString**) recType recName:(NSString**) recName;

// PathManager stack-related access
-(void)			backupStack;
-(void)			restoreStack;
-(void) 		cd:(NSString*) path;
-(NSString*) 	cwdAsDisplayString;
-(BOOL)			rootIsDirService;		// to differentiate whether paths should be "/LDAPv3/127.0.0.1/Users" or just "/Users"

// For debugging only
-(NSString*) getStackDescription;
@end
