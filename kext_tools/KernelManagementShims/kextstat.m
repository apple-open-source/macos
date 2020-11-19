//
//  kextstat.m
//  kextstat
//
//  Created by sso on 7/9/20.
//

#import <Foundation/Foundation.h>
#import "ShimHelpers.h"

#import "kextstat_main.h"

/*\

    Transform

        kextstat

    into

        kmutil showloaded

\*/
void shimKextstatArgsToKMUtilAndRun(KextstatArgs *toolArgs)
{
    initArgShimming();
    addArgument(@"showloaded");

    if (toolArgs->flagNoKernelComponents) {
        addArguments(@[@"--no-kernel-components"]);
    }

    if (toolArgs->flagListOnly) {
        addArguments(@[@"--list-only"]);
    }

    if (toolArgs->flagShowArchitecture) {
        addArguments(@[@"--arch-info"]);
    }

    if (toolArgs->flagSortByLoadAddress) {
        addArguments(@[@"--sort"]);
    }

    if (toolArgs->bundleIDs) {
        NSArray<NSString *> *strings = (__bridge NSArray<NSString *> *)toolArgs->bundleIDs;
        for (int i = 0; i < strings.count; i++) {
            addArguments(@[@"--bundle-identifier", strings[i]]);
        }
    }

    runWithShimmedArguments();
}
