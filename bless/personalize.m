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


static int HandleSpecialVolume(BLContextPtr context, const char *rootPath, const char *volumeDev, int role);
static int EnsureSpecialVolumeUUIDPath(BLContextPtr context, const char *volumeDev, int role, char *mountPoint, int mountLen,
									   bool *didMount);
static int CopyRootToDir(const char *rootPath, const char *volumePath);
static int GetRoledVolumeBSDForContainerBSD(const char *containerBSD, char *volumeBSD, int volumeLen, int role);


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
			ret = EnsureSpecialVolumeUUIDPath(context, sfs.f_mntfromname, APFS_VOL_ROLE_PREBOOT, prebootMount, sizeof prebootMount,
											  &mustUnmountPreboot);
			if (ret) goto exit;
			pURL = [NSURL fileURLWithFileSystemRepresentation:prebootMount isDirectory:YES relativeToURL:nil];
		}
		if ([pc volumeHasBeenPersonalized:vURL prebootFolder:pURL]) {
			// This is already done.  Let's get out of here.
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
			ret = CopyRootToDir([[rootsURL URLByAppendingPathComponent:OSPersonalizedManifestRootTypeBoot] fileSystemRepresentation],
								volumePath);
			if (ret) goto exit;
			if (strcmp(sfs.f_fstypename, "apfs") == 0) {
				pRoot = [[rootsURL URLByAppendingPathComponent:OSPersonalizedManifestRootTypePreboot] fileSystemRepresentation];
				if (access(pRoot, R_OK) < 0) goto exit;
				ret = HandleSpecialVolume(context, pRoot, sfs.f_mntfromname, APFS_VOL_ROLE_PREBOOT);
				if (ret) goto exit;
				pRoot = [[rootsURL URLByAppendingPathComponent:OSPersonalizedManifestRootTypeRecoveryBoot] fileSystemRepresentation];
				if (access(pRoot, R_OK) < 0) goto exit;
				ret = HandleSpecialVolume(context, pRoot, sfs.f_mntfromname, APFS_VOL_ROLE_RECOVERY);
				if (ret) goto exit;
			}
		} else {
			ret = EX_BADNETWORK;
			goto exit;
		}
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



static int HandleSpecialVolume(BLContextPtr context, const char *rootPath, const char *volumeDev, int role)
{
	int				ret;
	char			specialMountPointPath[MAXPATHLEN];
	bool			mustUnmount;

	ret = EnsureSpecialVolumeUUIDPath(context, volumeDev, role, specialMountPointPath, sizeof specialMountPointPath, &mustUnmount);
	if (ret) goto exit;
	
	ret = CopyRootToDir(rootPath, specialMountPointPath);
	if (ret) goto exit;

exit:
	if (mustUnmount) BLUnmountContainerVolume(context, specialMountPointPath);
	return ret;
}




static int EnsureSpecialVolumeUUIDPath(BLContextPtr context, const char *volumeDev, int role, char *mountPoint, int mountLen,
									   bool *didMount)
{
	int				ret;
	char			specialDevPath[64];
	struct statfs	*mnts;
	int				mntsize;
	int				i;
	io_service_t	service = IO_OBJECT_NULL;
	CFStringRef		uuid = NULL;
	int				len;

	strlcpy(specialDevPath, _PATH_DEV, sizeof specialDevPath);
	ret = GetRoledVolumeBSDForContainerBSD(volumeDev, specialDevPath + strlen(_PATH_DEV),
										   sizeof specialDevPath - strlen(_PATH_DEV), role);
	if (ret) goto exit;
	
	// Check if the given volume is mounted.
	mntsize = getmntinfo(&mnts, MNT_NOWAIT);
	if (!mntsize) {
		ret = 5;
		goto exit;
	}
	for (i = 0; i < mntsize; i++) {
		if (strcmp(mnts[i].f_mntfromname, specialDevPath) == 0) break;
	}
	if (i < mntsize) {
		strlcpy(mountPoint, mnts[i].f_mntonname, mountLen);
		*didMount = false;
	} else {
		// The preboot volume isn't mounted right now.  We'll have to mount it.
		ret = BLMountContainerVolume(context, specialDevPath + strlen(_PATH_DEV), mountPoint, mountLen, false);
		if (ret) goto exit;
		*didMount = true;
	}
	ret = BLGetIOServiceForDeviceName(context, volumeDev + strlen(_PATH_DEV), &service);
	if (ret) goto exit;
	uuid = IORegistryEntryCreateCFProperty(service, CFSTR(kIOMediaUUIDKey), kCFAllocatorDefault, 0);
	if (!uuid) {
		ret = EINVAL;
		goto exit;
	}
	len = strlen(mountPoint);
	if (mountLen <= len + 1) {
		ret = EINVAL;
		goto exit;
	}
	mountPoint[len] = '/';
	CFStringGetCString(uuid, mountPoint + len + 1, mountLen - len - 1, kCFStringEncodingUTF8);

exit:
	if (service) IOObjectRelease(service);
	if (uuid) CFRelease(uuid);
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




static int GetRoledVolumeBSDForContainerBSD(const char *containerBSD, char *volumeBSD, int volumeLen, int role)
{
	int                 ret;
	CFMutableArrayRef   volumes = NULL;
	CFStringRef         volume;
	
	*volumeBSD = '\0';
	ret = APFSVolumeRoleFind(containerBSD, role, &volumes);
	if (ret == unix_err(ENOATTR) || !volumes || CFArrayGetCount(volumes) == 0) {
		ret = 0;
	} else if (!ret) {
		volume = CFArrayGetValueAtIndex(volumes, 0);
		CFStringGetCString(volume, volumeBSD, volumeLen, kCFStringEncodingUTF8);
		memmove(volumeBSD, volumeBSD + strlen(_PATH_DEV), strlen(volumeBSD) - strlen(_PATH_DEV) + 1);
	}
	if (volumes) CFRelease(volumes);
	return ret;
}
