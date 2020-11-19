//
//  manifests.c
//
//

#include <string.h>
#include <libgen.h>
#include <dirent.h>
#include <paths.h>
#include <sys/param.h>
#include <sys/mount.h>
#include <APFS/APFSConstants.h>
#include <CoreFoundation/CoreFoundation.h>
#import <Foundation/Foundation.h>
#import <OSPersonalization/OSPersonalization.h>
#include "bless.h"
#include "bless_private.h"
#include "protos.h"


int CopyManifests(BLContextPtr context, const char *destPath, const char *srcPath, const char *srcSystemPath)
{
    int							ret = 0;
    OSPersonalizationController	*pc;
    NSArray<NSString *>			*manifestNames;
    NSString					*manifestPath;
    NSString					*srcDir;
    NSString					*destDir;
	NSString					*extraSrcDir = nil;
    NSString					*srcName;
    NSString					*destName;
    NSString					*newDestName;
    NSString					*newSrcName;
    NSString					*suffix;
    NSString					*newSrcPath;
    NSString					*newDestPath;
	NSString					*newExtraSrcPath;
	NSString					*srcPathToUse;
    NSFileManager				*fmd = [NSFileManager defaultManager];
    NSError						*nserr;
	char						canonicalSrc[MAXPATHLEN];
	char						canonicalDest[MAXPATHLEN];
	char						prebootSrcFolder[MAXPATHLEN] = "";
	char						prebootBSD[64];
	struct statfs				sfs;
	struct statfs				sysSfs;
	CFStringRef					volUUID = NULL;
	CFStringRef					groupUUID = NULL;
	bool						mustUnmountPreboot = false;
	char						prebootMnt[MAXPATHLEN];
	NSString					*prebootMntPt;
	NSString					*prebootPath;
	const char					*partialPath;
	bool						foundUUIDPath = false;
	uint16_t					role;
	bool						isAPFSRegVol = false;
	
	// OSPersonalization is weak-linked, so check to make sure it's present
	if ([OSPersonalizationController class] == nil) {
		return ENOTSUP;
	}
	
	// Canonicalize the paths so we can do mount-point comparisons later
	if (realpath(srcPath, canonicalSrc) == NULL) {
		blesscontextprintf(context, kBLLogLevelError, "Couldn't make canonical path from %s\n", srcPath);
		return EINVAL;
	}
	if (realpath(destPath, canonicalDest) == NULL) {
		blesscontextprintf(context, kBLLogLevelError, "Couldn't make canonical path from %s\n", destPath);
		return EINVAL;
	}
	
	// Are the paths the same?  Then there's nothing we want or need to do.
	if (strcmp(canonicalSrc, canonicalDest) == 0) {
		return 0;
	}

	if (statfs(canonicalSrc, &sfs) < 0) {
		ret = errno;
		blesscontextprintf(context, kBLLogLevelError, "Couldn't get volume information for path %s: %d\n", srcPath, ret);
		return ret;
	}

	if (statfs(srcSystemPath, &sysSfs) < 0) {
		ret = errno;
		blesscontextprintf(context, kBLLogLevelError, "Couldn't get volume information for system src path %s: %d\n", srcSystemPath, ret);
		return ret;
	}
	if (strcmp(sysSfs.f_fstypename, "apfs") == 0) {
		ret = BLRoleForAPFSVolumeDev(context, sysSfs.f_mntfromname, &role);
		if (ret) {
			blesscontextprintf(context, kBLLogLevelError, "Couldn't get role for volume at %s: %d\n", srcPath, ret);
			return ret;
		}
		isAPFSRegVol = role == APFS_VOL_ROLE_NONE || role == APFS_VOL_ROLE_SYSTEM || role == APFS_VOL_ROLE_DATA;
	}
	
	pc = [OSPersonalizationController sharedController];
	srcDir = [[NSString stringWithUTF8String:canonicalSrc] stringByDeletingLastPathComponent];
	destDir = [[NSString stringWithUTF8String:canonicalDest] stringByDeletingLastPathComponent];
	srcName = [[NSString stringWithUTF8String:canonicalSrc] lastPathComponent];
	destName = [[NSString stringWithUTF8String:canonicalDest] lastPathComponent];
    manifestNames = [pc requiredManifestPathsForBootFile:[NSString stringWithUTF8String:canonicalDest]];
    for (manifestPath in manifestNames) {
        newDestName = [manifestPath lastPathComponent];
        if (![newDestName hasPrefix:destName]) {
            blesscontextprintf(context, kBLLogLevelError, "Malformed manifest name \"%s\" for file \"%s\"\n",
                               [newDestName UTF8String], destPath);
            ret = ENOENT;
            break;
        }
        suffix = [newDestName substringFromIndex:[destName length]];
        newSrcName = [srcName stringByAppendingString:suffix];
        newSrcPath = [srcDir stringByAppendingPathComponent:newSrcName];
        newDestPath = [destDir stringByAppendingPathComponent:newDestName];
		srcPathToUse = nil;
		if ([fmd fileExistsAtPath:newSrcPath]) {
			srcPathToUse = newSrcPath;
		} else if (isAPFSRegVol) do {
			if (!extraSrcDir) {
				// Most errors in here should not be fatal.  They just mean that we
				// can't find the manifests in an alternate preboot directory.
				ret = GetPrebootBSDForVolumeBSD(context, sysSfs.f_mntfromname + strlen(_PATH_DEV), prebootBSD, sizeof prebootBSD);
				if (ret) { ret = 0; break; }
				if (strncmp(sfs.f_mntonname, canonicalSrc, strlen(sfs.f_mntonname)) != 0) {
					blesscontextprintf(context, kBLLogLevelVerbose, "Bad path for boot item: %s\n", canonicalSrc);
					break;
				}
				ret = GetVolumeUUIDs(context, sysSfs.f_mntfromname + strlen(_PATH_DEV), &volUUID, &groupUUID);
				if (ret) { ret = 0; break; }
				if (!volUUID) {
					blesscontextprintf(context, kBLLogLevelVerbose, "No UUID for volume at %s\n", srcPath);
					break;
				}
				ret = GetMountForBSD(context, prebootBSD, prebootMnt, sizeof prebootMnt);
				if (ret) { ret = 0; break; }
				if (!prebootMnt[0]) {
					ret = BLMountContainerVolume(context, prebootBSD, prebootMnt, sizeof prebootMnt, true);
					if (ret) {
						ret = 0;
						blesscontextprintf(context, kBLLogLevelVerbose, "Could not mount preboot volume at %s\n", prebootBSD);
						break;
					}
					mustUnmountPreboot = true;
				}
				prebootMntPt = [NSString stringWithUTF8String:prebootMnt];
				prebootPath = [prebootMntPt stringByAppendingPathComponent:(NSString *)volUUID];
				if ([fmd fileExistsAtPath:prebootPath]) {
					foundUUIDPath = true;
				} else if (groupUUID) {
					prebootPath = [prebootMntPt stringByAppendingPathComponent:(NSString *)groupUUID];
					if ([fmd fileExistsAtPath:prebootPath]) {
						foundUUIDPath = true;
					}
				}
				if (!foundUUIDPath) {
					blesscontextprintf(context, kBLLogLevelVerbose, "No UUID path in preboot for volume containing %s\n", srcPath);
					break;
				}
				partialPath = [srcDir fileSystemRepresentation] + strlen(sfs.f_mntonname);
				if (*partialPath == '/') partialPath++;
				snprintf(prebootSrcFolder, sizeof prebootSrcFolder, "%s/%s", [prebootPath fileSystemRepresentation], partialPath);
				extraSrcDir = [NSString stringWithUTF8String:prebootSrcFolder];
			}
			newExtraSrcPath = [extraSrcDir stringByAppendingPathComponent:newSrcName];
			if ([fmd fileExistsAtPath:newExtraSrcPath]) {
				srcPathToUse = newExtraSrcPath;
			}
		} while (0);
		if (!srcPathToUse) {
			// If we can't find this "required" manifest, don't fail out, but we do want
			// to make noise about it.  rdar://problem/61842081
//			blesscontextprintf(context, kBLLogLevelError, "WARNING: Missing required manifest: \"%s\".  Continuing...\n", [newSrcName UTF8String]);
		} else {
			[fmd removeItemAtPath:newDestPath error:NULL];
			if ([fmd copyItemAtPath:srcPathToUse toPath:newDestPath error:&nserr] == NO) {
				blesscontextprintf(context, kBLLogLevelError, "Couldn't copy file \"%s\" - %s\n",
								   [newDestPath UTF8String], [[nserr description] UTF8String]);
				ret = [nserr code];
				break;
			}
		}
    }
	if (volUUID) CFRelease(volUUID);
	if (groupUUID) CFRelease(groupUUID);
	if (mustUnmountPreboot) {
		BLUnmountContainerVolume(context, prebootMnt);
	}
    return ret;
}


