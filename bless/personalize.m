//
//  personalize.m
//

#import <string.h>
#import <stdlib.h>
#import <stdbool.h>
#import <unistd.h>
#import <paths.h>
#import <sys/param.h>
#import <sys/mount.h>
#import <sys/stat.h>
#import <CoreFoundation/CoreFoundation.h>
#import <Foundation/Foundation.h>
#import <OSPersonalization/OSPersonalization.h>
#import <APFS/APFS.h>
#import <Bom/Bom.h>
#import <IOKit/storage/IOMedia.h>
#import "bless.h"
#import "bless_private.h"
#import "protos.h"


#define EX_BADNETWORK		129
#define EX_SERVERREFUSED	130
#define EX_OTHERPERSONALIZE	131


static int HandleSpecialVolume(BLContextPtr context, const char *rootPath, const char *volumeDev, int role, bool useGroupUUID);
static int CopyRootToDir(const char *rootPath, const char *volumePath);


int PersonalizeOSVolume(BLContextPtr context, const char *volumePath, const char *prFile, bool suppressACPrompt)
{
	__block int                 ret = 0;
	NSAutoreleasePool			*pool = [[NSAutoreleasePool alloc] init];
	OSPersonalizationController *pc;
	char						prebootMount[MAXPATHLEN];
	NSURL                       *vURL;
	NSURL						*pURL = nil;
	char						*installEnv;
	NSURL                       *rootsURL;
	dispatch_semaphore_t        wait = dispatch_semaphore_create(0);
	NSDictionary                *prOptions;
	NSDictionary                *restoreOptions = nil;
	NSMutableDictionary         *options;
	char                        tmpLoc[MAXPATHLEN];
	__block NSArray<OSPersonalizedManifestRootType> *types;
	const char                  *pRoot;
	struct statfs               sfs;
	bool						mustCleanupRoots = false;
	bool						mustUnmountPreboot = false;
	bool						dummyBool;
	struct stat					sb;
	bool						useGroup = false;
	bool						isDataVol = false;
	bool						isARV = false;

	// OSPersonalization is weak-linked, so check to make sure it's present
	if ([OSPersonalizationController class] == nil) {
		return ENOTSUP;
	}
	pc = [OSPersonalizationController sharedController];
	if (statfs(volumePath, &sfs) < 0) {
		ret = errno;
		goto exit;
	}
	vURL = [NSURL fileURLWithFileSystemRepresentation:volumePath isDirectory:YES relativeToURL:nil];
	if ([pc personalizationRequiredForVolumeAtMountPoint:vURL]) {
		blesscontextprintf(context, kBLLogLevelVerbose, "Personalization required for volume at %s\n", volumePath);
		installEnv = getenv("__OSINSTALL_ENVIRONMENT");
		if (installEnv && (atoi(installEnv) > 0 || strcasecmp(installEnv, "yes") == 0 || strcasecmp(installEnv, "true") == 0)) {
			strlcpy(tmpLoc, "/var/tmp/RecoveryTemp", sizeof tmpLoc);
		} else {
			if (!confstr(_CS_DARWIN_USER_TEMP_DIR, tmpLoc, sizeof tmpLoc)) {
				// We couldn't get our path in /var/folders, so just try /var/tmp.
				strlcpy(tmpLoc, "/var/tmp/", sizeof tmpLoc);
			}
		}
		
		if (strcmp(sfs.f_fstypename, "apfs") == 0) {
			ret = BLEnsureSpecialAPFSVolumeUUIDPath(context, sfs.f_mntfromname,
                                                    APFS_VOL_ROLE_PREBOOT, false,
                                                    prebootMount, sizeof prebootMount,
											        &mustUnmountPreboot);
			if (ret) goto exit;
			if (stat(prebootMount, &sb) < 0 || !S_ISDIR(sb.st_mode)) {
				// Volume UUID didn't work.  Let's try group UUID.
				ret = BLEnsureSpecialAPFSVolumeUUIDPath(context, sfs.f_mntfromname,
														APFS_VOL_ROLE_PREBOOT, true,
														prebootMount, sizeof prebootMount,
														&dummyBool);
				if (ret) goto exit;
				if (dummyBool) mustUnmountPreboot = true;
				useGroup = true;
			}
			pURL = [NSURL fileURLWithFileSystemRepresentation:prebootMount isDirectory:YES relativeToURL:nil];
		}
		if ([pc volumeHasBeenPersonalized:vURL prebootFolder:pURL]) {
			// This is already done.  Let's get out of here.
			blesscontextprintf(context, kBLLogLevelVerbose, "OSP reports volume is already personalized\n");
			ret = 0;
			goto exit;
		}

		rootsURL = [[NSURL fileURLWithFileSystemRepresentation:tmpLoc isDirectory:YES relativeToURL:nil] URLByAppendingPathComponent:@"roots"];
		
		options = [NSMutableDictionary dictionary];
		[options setObject:[NSNumber numberWithBool:YES] forKey:OSPersonalizationOptionUseRunningDeviceIdentity];
		if (suppressACPrompt) [options setObject:[NSNumber numberWithBool:NO] forKey:OSPersonalizationOptionShowUI];
		if (prFile) {
			prOptions = [NSDictionary dictionaryWithContentsOfFile:[NSString stringWithUTF8String:prFile]];
			if (prOptions) {
				restoreOptions = [prOptions objectForKey:@"RestoreOptions"];
				if (!restoreOptions) restoreOptions = prOptions;
			}
		}
		if (restoreOptions) [options addEntriesFromDictionary:restoreOptions];
		if ([pc networkAvailableForPersonalizationWithOptions:options]) {
			[pc personalizeVolumeAtMountPointForInstall:vURL outputDirectory:rootsURL options:options completionHandler:^(NSArray<OSPersonalizedManifestRootType> * _Nonnull manifestTypes, NSError * _Nullable error) {
				 if (error) {
					 if (OSPErrorIsNetworkingRelated(error)) {
						 ret = EX_BADNETWORK;
					 } else if ([error.domain isEqualToString:OSPErrorDomain] && error.code == OSPTATSUDeclinedAuthorizationError) {
						 ret = EX_SERVERREFUSED;
					 } else {
						 ret = EX_OTHERPERSONALIZE;
					 }
				 } else {
					 ret = 0;
				 }
				 if (!ret) types = manifestTypes;
				 dispatch_semaphore_signal(wait);
			 }];
			dispatch_semaphore_wait(wait, DISPATCH_TIME_FOREVER);
			if (ret) goto exit;
			mustCleanupRoots = true;
			if (strcmp(sfs.f_fstypename, "apfs") == 0) {
				ret = BLIsDataRoleForAPFSVolumeDev(context, sfs.f_mntfromname, &isDataVol);
				if (ret) goto exit;
				ret = BLIsVolumeARV(context, sfs.f_mntonname, sfs.f_mntfromname + strlen(_PATH_DEV), &isARV);
				if (ret) goto exit;
			}
			if (!isDataVol && !isARV) {
				ret = CopyRootToDir([[rootsURL URLByAppendingPathComponent:OSPersonalizedManifestRootTypeBoot] fileSystemRepresentation],
									volumePath);
				if (ret) goto exit;
			}
			if (strcmp(sfs.f_fstypename, "apfs") == 0) {
				pRoot = [[rootsURL URLByAppendingPathComponent:OSPersonalizedManifestRootTypePreboot] fileSystemRepresentation];
				if (access(pRoot, R_OK) < 0) goto exit;
				ret = HandleSpecialVolume(context, pRoot, sfs.f_mntfromname, APFS_VOL_ROLE_PREBOOT, useGroup);
				if (ret) goto exit;
				pRoot = [[rootsURL URLByAppendingPathComponent:OSPersonalizedManifestRootTypeRecoveryBoot] fileSystemRepresentation];
				if (access(pRoot, R_OK) < 0) goto exit;
				ret = HandleSpecialVolume(context, pRoot, sfs.f_mntfromname, APFS_VOL_ROLE_RECOVERY, useGroup);
				if (ret) goto exit;
			}
		} else {
			ret = EX_BADNETWORK;
			goto exit;
		}
	} else {
		blesscontextprintf(context, kBLLogLevelVerbose, "Personalization not required for volume at %s\n", volumePath);
	}
	
exit:
	if (mustUnmountPreboot) {
		BLUnmountContainerVolume(context, prebootMount);
	}
	if (!ret && mustCleanupRoots) {
		DeleteFileOrDirectory([rootsURL fileSystemRepresentation]);
	}
	dispatch_release(wait);
	[pool release];
	return ret;
}



static int HandleSpecialVolume(BLContextPtr context, const char *rootPath, const char *volumeDev, int role, bool useGroupUUID)
{
	int				ret;
	char			specialMountPointPath[MAXPATHLEN];
	bool			mustUnmount;

	ret = BLEnsureSpecialAPFSVolumeUUIDPath(context, volumeDev,
                                            role, useGroupUUID,
                                            specialMountPointPath, sizeof specialMountPointPath,
                                            &mustUnmount);
	if (ret) goto exit;
	
	ret = CopyRootToDir(rootPath, specialMountPointPath);
	if (ret) goto exit;

exit:
	if (mustUnmount) BLUnmountContainerVolume(context, specialMountPointPath);
	return ret;
}



static int CopyRootToDir(const char *rootPath, const char *volumePath)
{
	int			ret;
	BOMCopier	copier;
	
	copier = BOMCopierNew();
	if (!copier) return ENOMEM;
	ret = BOMCopierCopy(copier, rootPath, volumePath);
	BOMCopierFree(copier);
	return ret;
}


