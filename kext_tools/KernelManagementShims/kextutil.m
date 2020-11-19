//
//  kextutil.m
//  kextutil
//
//  Created by sso on 3/12/20.
//

#import <Foundation/Foundation.h>
#import "ShimHelpers.h"

#import <KernelManagement/KernelManagement.h>
#import <KernelManagement/KernelManagement_Private.h>

#import "kextutil_main.h"
#import "kext_tools_util.h"

bool isKernelManagementLinked() {
    return NSClassFromString(@"KernelManagementClient") ? true : false;
}

/*\

    Transform a command from kextutil to kmutil. For example,

        kextutil Foo.kext

    should turn into a call to:

        kmutil load --bundle-path Foo.kext

\*/
void shimKextutilArgsToKMUtilAndRun(struct KextutilArgs *toolArgs)
{
    if (!toolArgs->doLoad && !toolArgs->doStartMatching) {
        fprintf(stderr, "kextutil: -n is not a supported kmutil mode\n");
        exit(EX_OK);
    }

    /* TODO: Verify whether kernelURL makes sense. Currently it gets set by default.
     * if (toolArgs->kernelURL) {
     *    fprintf(stderr, "kextutil-to-kmutil does not support "
     *            "-k (-kernel)\n");
     *    exit(EX_OK);
     *}
     */

    initArgShimming();
    addArgument(@"load");

    if (toolArgs->archInfo) {
        addArguments(@[@"--arch", @(toolArgs->archInfo->name)]);
    }

    if (toolArgs->dependencyURLs) {
        /* TODO: support -d (for now, translate -dependency to bundle paths) */
        NSArray<NSURL *> *nsurls = (__bridge NSArray<NSURL *> *)toolArgs->dependencyURLs;
        for (int i = 0; i < nsurls.count; i++) {
            addArguments(@[@"--bundle-path", nsurls[i].path]);
        }
    }

    if (toolArgs->kextIDs) {
        NSArray<NSString *> *strings = (__bridge NSArray<NSString *> *)toolArgs->kextIDs;
        for (int i = 0; i < strings.count; i++) {
            addArguments(@[@"--bundle-identifier", strings[i]]);
        }
    }

    if (toolArgs->kextURLs) {
        NSArray<NSURL *> *nsurls = (__bridge NSArray<NSURL *> *)toolArgs->kextURLs;
        for (int i = 0; i < nsurls.count; i++) {
            addArguments(@[@"--bundle-path", nsurls[i].path]);
        }
    }

    if (toolArgs->personalityNames) {
        NSArray<NSString *> *strings = (__bridge NSArray<NSString *> *)toolArgs->personalityNames;
        for (int i = 0; i < strings.count; i++) {
            addArguments(@[@"--personality-name", strings[i]]);
        }
    }

    if (toolArgs->repositoryURLs) {
        NSArray<NSURL *> *nsurls = (__bridge NSArray<NSURL *> *)toolArgs->repositoryURLs;
        for (int i = 0; i < nsurls.count; i++) {
            addArguments(@[@"--repository", nsurls[i].path]);
        }
    }

    if (toolArgs->skipAuthentication) {
        addArguments(@[@"--no-authentication"]);
    }

    if (toolArgs->doLoad && !toolArgs->doStartMatching) {
        addArguments(@[@"--load-style", @"start-only"]);
    }

    if (!toolArgs->doLoad && toolArgs->doStartMatching) {
        addArguments(@[@"--load-style", @"match-only"]);
    }

    runWithShimmedArguments();
}
