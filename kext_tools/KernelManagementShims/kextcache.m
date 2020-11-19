//
//  kextcache.m
//  kextcache
//
//  Created by jkb on 3/11/20.
//

#import <Foundation/Foundation.h>
#import "../kextcache_main.h"
#import "ShimHelpers.h"

static bool isInstallerActive(void)
{
    return (getenv("__OSINSTALL_ENVIRONMENT") != NULL) && (get_bootarg("-rootdmg-ramdisk") != NULL);
}

// This function will always result in a call to exit(). If calling this
// function spawns kmutil, then it will call exit() with kmutil's exit status.
void shimKextcacheArgsToKMUtilAndRun(KextcacheArgs *toolArgs)
{
    int exitCode = EX_OK; // what we'll exit with if we don't spawn kmutil
    initArgShimming();

    OSKextRequiredFlags filterAllFlags = toolArgs->requiredFlagsAll;
    OSKextRequiredFlags bothFlags = filterAllFlags | toolArgs->requiredFlagsRepositoriesOnly;

    if (toolArgs->updateOpts & kBRUEarlyBoot) {
        // To be used for an early boot task for updating the auxKC.
        OSKextLog(/* kext */ NULL,
            kOSKextLogWarningLevel | kOSKextLogGeneralFlag,
            "Ignoring kextcache invocation from early boot");
        goto cancel;
    }

    if (toolArgs->isInstaller || isInstallerActive()) {
        // Anything involving the installer should do nothing
        OSKextLog(/* kext */ NULL,
            kOSKextLogWarningLevel | kOSKextLogGeneralFlag,
            "Ignoring kextcache invocation from the Installer");
        goto cancel;
    }


    if (toolArgs->clearStaging || toolArgs->pruneStaging) {
        addArguments(@[
            @"clear-staging"
        ]);
    }
    // Gather a kmutil shim invocation, then print out the command, but don't
    // execute it
    else if (toolArgs->prelinkedKernelPath) {
        addArguments(@[
            @"create",
            @"-n",
            @"boot",
            @"--boot-path",
            [NSString stringWithUTF8String:toolArgs->prelinkedKernelPath]
        ]);

        OSKextRequiredFlags checkFlags[] = {
            kOSKextOSBundleRequiredLocalRootFlag,
            kOSKextOSBundleRequiredNetworkRootFlag,
            kOSKextOSBundleRequiredSafeBootFlag
        };

        const NSString* checkKeys[] = {
            @(kOSBundleRequiredLocalRoot),
            @(kOSBundleRequiredNetworkRoot),
            @(kOSBundleRequiredSafeBoot)
        };
        _Static_assert(N_ELEMS(checkFlags) == N_ELEMS(checkKeys),
                "Length of arrays should be equal");

        for (unsigned long i = 0; i < N_ELEMS(checkFlags); i++) {
            if (bothFlags & checkFlags[i]) {
                // -fF behaves similarly to -lL / -nN / -sS in kextcache, except it accepts a filter predicate
                // instead for filtering by Info.plist key, which is a bit more flexible.
                addArgument(filterAllFlags & checkFlags[i] ? @"-F" : @"-f");
                addArgument([NSString stringWithFormat:@"'%@' == '%@'", @(kOSBundleRequiredKey), checkKeys[i]]);
            }
        }

        if (toolArgs->kernelPath) {
            NSString *nsKernelPath = [NSString stringWithUTF8String:toolArgs->kernelPath];
            addArguments(@[@"--kernel", nsKernelPath]);

            if (nsKernelPath.pathExtension.length > 0) {
                addArguments(@[@"--variant-suffix", nsKernelPath.pathExtension]);
            }
        }

        for (NSURL *repositoryURL in (__bridge NSArray<NSURL *> *)toolArgs->repositoryURLs) {
            addArguments(@[@"--repository", repositoryURL.path]);
        }

        for (NSURL *bundleURL in (__bridge NSArray<NSURL *> *)toolArgs->namedKextURLs) {
            addArguments(@[@"--bundle-path", bundleURL.path]);
        }

        OSKextLogCFString(/* kext */ NULL,
            kOSKextLogWarningLevel | kOSKextLogGeneralFlag,
            CFSTR("It looks like you're trying to create a prelinked kernel. Kextcache is deprecated and no longer suitable for use. "
                "You can build a kext collection containing the same kexts by invoking: %@"),
                createStringFromShimmedArguments());

        exitCode = EX_OK;
        goto cancel;
    }
    // Otherwise, if this is an attempt to update a volume, then the outcome depends on if we're installing a root,
    // and if the destination is currently writable.
    else if (toolArgs->updateVolumeURL) {
        NSString *updateVolumePath = ((__bridge NSURL *)toolArgs->updateVolumeURL).path;
        addArguments(@[
            @"install",
            @"--volume-root",
            updateVolumePath
        ]);

        if (toolArgs->dstRootUpdate) {
            // this is the darwinup invocation: `kextcache -Du /`
            addArgument(@"--update-all");
            addArgument(@"--update-preboot");
        } else {
            addArgument(@"--check-rebuild");
        }
    }
    // If we're being asked to update the personality cache, then politely decline.
    else if (toolArgs->updateSystemCaches) {
        OSKextLogCFString(/* kext */ NULL,
            kOSKextLogWarningLevel | kOSKextLogGeneralFlag,
            CFSTR("It looks like you're trying to update the system caches. As of macOS 11, the personality cache is "
		  "no longer in use for keeping kext matching information up-to-date. For more information, see "
		  "`man kmutil`."));
        exitCode = EX_OK;
        goto cancel;
    }
    // All other commands should also fail nicely with a pointer to kmutil(8).
    else {
        OSKextLogCFString(/* kext */ NULL,
            kOSKextLogWarningLevel | kOSKextLogGeneralFlag,
            CFSTR("This option is no longer supported as of macOS 11. For supported modern kext and kernel management "
		   "commands, see `man kmutil`."));
        exitCode = EX_OK;
        goto cancel;
    }
    runWithShimmedArguments();
cancel:
    exit(exitCode);
}
