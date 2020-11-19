//
//  kc_staging.m
//  kext_tools
//
//  Created by Jack Kim-Biggs on 7/16/19.
//

#import <Foundation/Foundation.h>
#import <IOKit/kext/OSKext.h>
#import <IOKit/kext/OSKextPrivate.h>
#import <Bom/Bom.h>
#import <APFS/APFS.h>

#import <stdio.h>
#import <stdbool.h>
#import <string.h>
#import <sysexits.h>
#import <unistd.h>

#define STRICT_SAFETY 0
#import "safecalls.h"
#import "kc_staging.h"
#import "bootcaches.h"
#import "kext_tools_util.h"

void fatalFileErrorHandler(BOMCopier copier, const char *path, int errno_)
{
	(void)copier;
	LOG_ERROR("BOMCopier fatal file error: %d at %s", errno_, path);
}
void fatalErrorHandler(BOMCopier copier, const char *message)
{
	(void)copier;
	LOG_ERROR("BOMCopier fatal error: %s", message);
}
BOMCopierCopyOperation fileErrorHandler(BOMCopier copier, const char *path, int errno_)
{
	(void)copier;
	LOG_ERROR("BOMCopier file error: %d at %s", errno_, path);
	return BOMCopierContinue;
}

#if KCDITTO_STANDALONE_BINARY
static bool g_shouldUnmountPreboot = false;
static int  g_prebootFD = -1;

static void unmountPreboot(void *volpath)
{
	int     ret;
	char    *newargv[4] = {};

	newargv[0] = "/usr/sbin/diskutil";
	newargv[1] = "unmount";
	newargv[2] = volpath;
	newargv[3] = NULL;

	LOG("Unmounting Preboot: \"%s\"", volpath);

	if (g_prebootFD > -1) {
		close(g_prebootFD);
	}

	pid_t p = fork();
	if (p == 0) {
		setuid(geteuid());
		ret = execv("/usr/sbin/diskutil", newargv);
		if (ret == -1) {
			LOG_ERROR("Could not exec %s", "/usr/sbin/diskutil");
		}
		_exit(1);
	}

	do {
		p = wait(&ret);
	} while (p == -1 && errno == EINTR);

	if (p == -1 || ret) {
		LOG_ERROR("%s returned non-0 exit status", "/usr/sbin/diskutil");
		return;
	}

	g_shouldUnmountPreboot = false;
	(void)rmdir(volpath);
}

static bool mountPreboot(char *devpath, char *mntpnt, int mntpntlen)
{
	int     ret;
	char    tmppath[MNAMELEN] = {};
	char    fullmntpath[MNAMELEN] = {};
	char    *newargv[12] = {};

	g_shouldUnmountPreboot = false;

	strlcpy(tmppath, "/var/tmp/preboot.kcditto.XXXXXXX", sizeof(tmppath));
	if (!mkdtemp(tmppath)) {
		LOG_ERROR("Error creating temp preboot mount point %d (%s)", errno, strerror(errno));
		return false;
	}
	realpath(tmppath, fullmntpath);

	newargv[0] = "/sbin/mount";
	newargv[1] = "-t";
	newargv[2] = "apfs";
	newargv[3] = "-o";
	newargv[4] = "perm";
	newargv[5] = "-o";
	newargv[6] = "owners";
	newargv[7] = "-o";
	newargv[8] = "nobrowse";
	newargv[9] = devpath;
	newargv[10] = fullmntpath;
	newargv[11] = NULL;

	LOG("Mounting Preboot: \"%s\"", fullmntpath);

	pid_t p = fork();
	if (p == 0) {
		setuid(geteuid());
		ret = execv("/sbin/mount", newargv);
		if (ret == -1) {
			LOG_ERROR("Could not exec %s", "/sbin/mount");
		}
		_exit(1);
	}

	do {
		p = wait(&ret);
	} while (p == -1 && errno == EINTR);

	if (p == -1 || ret) {
		LOG_ERROR("%s returned non-0 exit status", "/sbin/mount");
		rmdir(fullmntpath);
		return false;
	}

	strlcpy(mntpnt, fullmntpath, mntpntlen);
	g_shouldUnmountPreboot = true;
	return true;
}
#endif // KCDITTO_STANDALONE_BINARY

static bool checkMounted(char *devpath, char *volpath, int vollen)
{
	struct statfs *mnts;
	int mntsize;
	int i;

	// Check if the given volume is mounted.
	mntsize = getmntinfo(&mnts, MNT_NOWAIT);
	if (!mntsize || !mnts) {
		return false;
	}
	for (i = 0; i < mntsize; i++) {
		if (strcmp(mnts[i].f_mntfromname, devpath) == 0) {
			break;
		}
	}

	if (i < mntsize) {
		if (volpath && vollen > 0) {
			strlcpy(volpath, mnts[i].f_mntonname, vollen);
		}
		return true;
	} else {
		if (volpath) {
			*volpath = 0;
		}
		return false;
	}
}

static bool findVolumeGroupUUID(const char *volPath, uuid_string_t *volgroup_uuid_str)
{
	bool result = false;
	struct statfs sfs = {};
	uuid_t group_uuid = {};
	OSStatus err;

	if (statfs(volPath, &sfs) < 0) {
		goto finish;
	}

	/* Find the volume group UUID */
	err = APFSGetVolumeGroupID(sfs.f_mntfromname, group_uuid);
	if (err) {
		LOG_ERROR("Could not find volume group UUID for volume device path %s, error %d",
		          volPath, err);
		goto finish;
	}

	uuid_unparse_upper(group_uuid, *volgroup_uuid_str);
	result = true;

finish:
	return result;
}


/*
 * Given the volume path, find the associated Preboot Volume. Return true if the
 * volume is mounted, and populate prebootMount with the path on success.
 */
static bool findPrebootVolume(const char *volpath, char *prebootMount, int prebootLen)
{

	CFMutableArrayRef candidateVolumes      = NULL;
	CFIndex           candidateVolumesCount = 0;
	bool              result                = false;
	struct statfs     sfs                   = {};
	char              prebootDisk[128]      = {};

	if (statfs(volpath, &sfs) < 0) {
		goto finish;
	}

	/* Find the data volume(s?), and iterate over them to see if our candidate is in there */
	if ((result = APFSVolumeRoleFind(sfs.f_mntfromname,
	                                 APFS_VOL_ROLE_PREBOOT,
	                                 &candidateVolumes)) != kIOReturnSuccess) {
		LOG_ERROR("Could not find preboot volume for volume device path %s, error %d",
		          volpath, result);
		goto finish;
	}

	candidateVolumesCount = CFArrayGetCount(candidateVolumes);
	if (candidateVolumesCount == 0) {
		LOG_ERROR("Found 0 Preboot volumes.");
		goto finish;
	}

	// there should be only 1 preboot volume
	CFStringRef volume = CFArrayGetValueAtIndex(candidateVolumes, 0);
	CFStringGetCString(volume, prebootDisk, sizeof(prebootDisk), kCFStringEncodingUTF8);
	LOG("Preboot disk: %s", prebootDisk);
	result = checkMounted(prebootDisk, prebootMount, prebootLen);
#if KCDITTO_STANDALONE_BINARY
	if (!result) {
		result = mountPreboot(prebootDisk, prebootMount, prebootLen);
	}
	if (result) {
		g_prebootFD = open(prebootMount, O_RDONLY | O_EVTONLY, 0);
	}
#endif

finish:
	SAFE_RELEASE(candidateVolumes);
	return result;
}


/*
 * filenames look like:
 * BootKernelExtensions.kc
 * BootKernelExtensions.kc.development
 * BootKernelExtensions.kc.development.j132ap.im4m
 * BootKernelExtensions.kc.development.j137ap.im4m
 * BootKernelExtensions.kc.development.x589iclydev.im4m
 * BootKernelExtensions.kc.j132ap.im4m
 * BootKernelExtensions.kc.j137ap.im4m
 *
 * So we treat each kc name / suffix pairing as a "prefix" to copy other stuff over, to make sure
 * that we get the .im4m images and per-device immutable caches in case we're building internally.
 * We'll only try to copy all the files if we manage to stat a file with that prefix.
 */

static int copyKCWithCopier(BOMCopier copier, NSString *srcDir, NSString *filename, NSString *dstPath)
{
	int result = EX_SOFTWARE;

	NSError             *pathError   = nil;
	NSString            *srcPath     = [srcDir stringByAppendingPathComponent:filename];
	NSPredicate         *dirFilter   = [NSPredicate predicateWithFormat:@"lastPathComponent BEGINSWITH %@", filename];
	NSFileManager       *fm          = [NSFileManager defaultManager];
	NSArray<NSString *> *dirContents = [fm contentsOfDirectoryAtPath:srcDir error:&pathError];
	NSArray<NSString *> *matches     = [dirContents filteredArrayUsingPredicate:dirFilter];

	if (pathError) {
		LOG_ERROR("Encountered error while inspecting path: %s", [pathError.description UTF8String]);
		result = EX_OK; // fail silently and log the error
		goto finish;
	}

	if (![fm fileExistsAtPath:srcPath isDirectory:false]) {
		result = EX_OK; // silently fail, since we didn't find anything
		goto finish;
	}
	for (NSString *match in matches) {
		NSString   *fullPath       = [srcDir stringByAppendingPathComponent:match];
		const char *srcPathCString = [fullPath UTF8String];
		const char *dstPathCString = [dstPath UTF8String];
		if (!srcPathCString || !dstPathCString) {
			LOG_ERROR("Error considering KC copy: srcPath = %s, dstPath = %s",
					  srcPathCString ? srcPathCString : "(null)",
					  dstPathCString ? dstPathCString : "(null)");
			goto finish;
		}
		LOG("Copying: %s -> %s", srcPathCString, dstPathCString);
		if ((result = BOMCopierCopy(copier, srcPathCString, dstPathCString)) != EX_OK) {
			LOG_ERROR("Error copying KC: srcPath = %s, dstPath = %s, error %d",
					  srcPathCString ? srcPathCString : "(null)",
					  dstPathCString ? dstPathCString : "(null)",
					  result);
			goto finish;
		}
	}
	result = EX_OK;
finish:
	return result;
}


static int copyKCs(NSString *srcDir, NSString *dstDir, NSArray<NSString *> *kc_names)
{
	int result = EX_SOFTWARE;

	BOMCopier copier = BOMCopierNew();
	if (!copier) {
		LOG_ERROR("Error creating BOMCopier");
		goto finish;
	}
	BOMCopierSetFatalErrorHandler(copier, fatalErrorHandler);
	BOMCopierSetFatalFileErrorHandler(copier, fatalFileErrorHandler);
	BOMCopierSetFileErrorHandler(copier, fileErrorHandler);

#if KCDITTO_STANDALONE_BINARY
	if (g_prebootFD > -1) {
		const char *dstDirCString = [dstDir UTF8String];
		if (sdeepmkdir(g_prebootFD, dstDirCString, kCacheDirMode) != 0) {
			LOG_ERROR("error creating / accessing '%s': %d (%s)", dstDirCString, errno, strerror(errno));
			goto finish;
		}
	}
#endif

	for (NSString *kc in kc_names) {
		if ((result = copyKCWithCopier(copier, srcDir, kc, dstDir)) != EX_OK) {
			goto finish;
		}
	}

	BOMCopierFree(copier);
	result = EX_OK;
finish:
	return result;
}


int copyKCsInVolume(char *volRoot)
{
	int result = EX_SOFTWARE;
	struct bootCaches *caches = NULL;
	char prebootPath[MAXPATHLEN] = {};
	uuid_string_t volgroup_uuid = {};
	size_t prebootLen = sizeof(prebootPath);

	NSString *srcKCPathString = nil;
	NSString *dstKCPrebootPath = nil;
	NSString *dstKCPrebootVolGroupPath = nil;
	NSArray<NSString *> *kc_names = @[
		@"BootKernelExtensions.kc",
	];

	NSString *srcPLKPathString = nil;
	NSString *dstPLKPrebootPath = nil;
	NSString *dstPLKPrebootVolGroupPath = nil;
	NSArray<NSString *> *plk_names = @[
		@"immutablekernel",
		@"prelinkedkernel",
	];

	NSFileManager *fileManager = [NSFileManager defaultManager];
	NSURL *prebootSysURL = nil;
	bool   use_sysuuid = false;
	NSURL *prebootVolGroupURL = nil;
	bool   use_volgroupuuid = false;

	if (geteuid() != 0) {
		LOG_ERROR("You must be running as root to manage the kext collection preboot area.");
		result = EX_NOPERM;
		goto finish;
	}

	caches = readBootCaches(volRoot, kBROptsNone);
	if (!caches) {
		LOG_ERROR("Error reading bootcaches for volume %s...", volRoot);
		goto finish;
	}

	if (!caches->fileset_bootkc) {
		LOG_ERROR("Error reading bootcaches for volume %s - no fileset path?", volRoot);
		goto finish;
	}

	LOG("System Volume UUID: %s", caches->fsys_uuid);

	if (!findVolumeGroupUUID(volRoot, &volgroup_uuid)) {
	    LOG_ERROR("Couldn't find volume group UUID for volume %s (%s)", volRoot, caches->fsys_uuid);
	    goto finish;
	}
	LOG("Volume Group UUID: %s", volgroup_uuid);

	if (!findPrebootVolume(volRoot, prebootPath, (int)prebootLen)) {
	    LOG_ERROR("Couldn't find preboot mount (or preboot not mounted) for volume %s (%s)", volRoot, prebootPath);
	    goto finish;
	}

	LOG("Preboot volume: %s", prebootPath);

	prebootSysURL = [NSURL fileURLWithPathComponents:@[
				[NSString stringWithUTF8String:prebootPath],
				[NSString stringWithUTF8String:caches->fsys_uuid]]];
	if ([fileManager fileExistsAtPath:prebootSysURL.path]) {
		use_sysuuid = true;
	}

	prebootVolGroupURL = [NSURL fileURLWithPathComponents:@[
				[NSString stringWithUTF8String:prebootPath],
				[NSString stringWithUTF8String:volgroup_uuid]]];
	if ([fileManager fileExistsAtPath:prebootVolGroupURL.path]) {
		use_volgroupuuid = true;
	}

	// expected path:
	// /System/Volumes/Preboot/{SystemUUID}/boot/System/Library/KernelCollections/BootKernelExtensions.kc
	dstKCPrebootPath = [NSString pathWithComponents:@[
		prebootSysURL.path,
		@"boot",
		[[NSString stringWithUTF8String:caches->fileset_bootkc->rpath] stringByDeletingLastPathComponent],
	]];
	if (!dstKCPrebootPath) {
		LOG_ERROR("Error considering preboot KC path for %s...", volRoot);
		goto finish;
	}

	// expected path:
	// /System/Volumes/Preboot/{VolGroupUUID}/boot/System/Library/KernelCollections/BootKernelExtensions.kc
	dstKCPrebootVolGroupPath = [NSString pathWithComponents:@[
		prebootVolGroupURL.path,
		@"boot",
		[[NSString stringWithUTF8String:caches->fileset_bootkc->rpath] stringByDeletingLastPathComponent],
	]];
	if (!dstKCPrebootVolGroupPath) {
		LOG_ERROR("Error considering preboot KC path for %s...", volRoot);
		goto finish;
	}

	srcKCPathString = [NSString pathWithComponents:@[
		[NSString stringWithUTF8String:volRoot],
		[[NSString stringWithUTF8String:caches->fileset_bootkc->rpath] stringByDeletingLastPathComponent],
	]];
	if (!srcKCPathString) {
		LOG_ERROR("Error considering preboot volume for %s...", volRoot);
		goto finish;
	}

	if (use_sysuuid && (result = copyKCs(srcKCPathString, dstKCPrebootPath, kc_names) != EX_OK)) {
		LOG_ERROR("Error copying KCs (SystemVolumeUUID): %d", result);
		goto finish;
	}
	if (use_volgroupuuid && (result = copyKCs(srcKCPathString, dstKCPrebootVolGroupPath, kc_names) != EX_OK)) {
		LOG_ERROR("Error copying KCs (VolGroupUUID): %d", result);
		goto finish;
	}


	// expected path:
	// /System/Volumes/Preboot/{SystemUUID}/System/Library/PrelinkedKernels/prelinkedkernel
	dstPLKPrebootPath = [NSString pathWithComponents:@[
		prebootSysURL.path,
		[[NSString stringWithUTF8String:caches->readonly_kext_boot_cache_file->rpath] stringByDeletingLastPathComponent],
	]];
	if (!dstPLKPrebootPath) {
		LOG_ERROR("Error considering preboot PLK path for %s...", volRoot);
		goto finish;
	}

	// expected path:
	// /System/Volumes/Preboot/{VolGroupUUID}/System/Library/PrelinkedKernels/prelinkedkernel
	dstPLKPrebootVolGroupPath = [NSString pathWithComponents:@[
		prebootVolGroupURL.path,
		[[NSString stringWithUTF8String:caches->readonly_kext_boot_cache_file->rpath] stringByDeletingLastPathComponent],
	]];
	if (!dstPLKPrebootVolGroupPath) {
		LOG_ERROR("Error considering preboot PLK path for %s...", volRoot);
		goto finish;
	}

	srcPLKPathString = [NSString pathWithComponents:@[
		[NSString stringWithUTF8String:volRoot],
		[[NSString stringWithUTF8String:caches->readonly_kext_boot_cache_file->rpath] stringByDeletingLastPathComponent],
	]];
	if (!srcPLKPathString) {
		LOG_ERROR("Error considering preboot PLK path for %s...", volRoot);
		goto finish;
	}

	if (use_sysuuid && (result = copyKCs(srcPLKPathString, dstPLKPrebootPath, plk_names) != EX_OK)) {
		LOG_ERROR("Error copying prelinked kernels (SystemUUID): %d", result);
		goto finish;
	}
	if (use_volgroupuuid && (result = copyKCs(srcPLKPathString, dstPLKPrebootVolGroupPath, plk_names) != EX_OK)) {
		LOG_ERROR("Error copying prelinked kernels (VolGroupUUID): %d", result);
		goto finish;
	}

	result = EX_OK;

finish:
	free(caches);
#if KCDITTO_STANDALONE_BINARY
	if (g_prebootFD > -1) {
		close(g_prebootFD);
		g_prebootFD = -1;
	}
	if (g_shouldUnmountPreboot) {
		unmountPreboot(prebootPath);
	}
#endif
	return result;
}


/*
 * Copy files from the ROSP staging area into the System Volume.
 * This function will be deleted once ROSP has been replaced with AuthAPFS.
 */
int copyDeferredPrelinkedKernels(char *volRoot)
{
	int result = EX_SOFTWARE;
	struct bootCaches *caches = NULL;
	char *tempPath = NULL, *destPath = NULL;
	NSString *tempPathString = nil; NSString *destPathString = nil;

	NSArray<NSString *> *kc_names = @[
		@"immutablekernel",
		@"prelinkedkernel",
	];


	if (geteuid() != 0) {
		LOG_ERROR("You must be running as root to manage the prelinked kernel staging area.");
		result = EX_NOPERM;
		goto finish;
	}

	caches = readBootCaches(volRoot, kBROptsNone);
	if (!caches) {
		LOG_ERROR("Error reading bootcaches for volume %s...", volRoot);
		goto finish;
	}

	if (!caches->readonly_kext_boot_cache_file) {
		LOG_ERROR("Error reading bootcaches for volume %s - no plk path?", volRoot);
		goto finish;
	}
	destPath = caches->readonly_kext_boot_cache_file->rpath;

	if (!caches->kext_boot_cache_file) {
		LOG_ERROR("Error reading bootcaches for volume %s - no plk staging path?", volRoot);
		goto finish;
	}
	tempPath = caches->kext_boot_cache_file->rpath;

	/* Remove the last path component, since it names the prelinkedkernel filename. */
	tempPathString = [NSString pathWithComponents:@[
		[NSString stringWithUTF8String:volRoot],
		[[NSString stringWithUTF8String:tempPath] stringByDeletingLastPathComponent],
	]];
	if (!tempPathString) {
		LOG_ERROR("Error considering prelinked staging area...");
		goto finish;
	}

	destPathString = [NSString pathWithComponents:@[
		[NSString stringWithUTF8String:volRoot],
		[[NSString stringWithUTF8String:destPath] stringByDeletingLastPathComponent],
	]];
	if (!destPathString) {
		LOG_ERROR("Error considering prelinked destination...");
		goto finish;
	}

	if ((result = copyKCs(tempPathString, destPathString, kc_names) != EX_OK)) {
		LOG_ERROR("Error copying kernels...");
		goto finish;
	}

	result = EX_OK;
finish:
	free(caches);
	return result;
}
