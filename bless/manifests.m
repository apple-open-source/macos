//
//  manifests.c
//
//

#include <string.h>
#include <libgen.h>
#include <dirent.h>
#include <sys/param.h>
#include <CoreFoundation/CoreFoundation.h>
#import <Foundation/Foundation.h>
#import <OSPersonalization/OSPersonalization.h>
#include "bless.h"
#include "protos.h"


int CopyManifests(BLContextPtr context, const char *destPath, const char *srcPath)
{
    int                            ret = 0;
    OSPersonalizationController    *pc = [OSPersonalizationController sharedController];
    NSArray<NSString *>            *manifestNames;
    NSString                       *manifestPath;
    NSString                       *srcDir;
    NSString                       *destDir;
    NSString                       *srcName;
    NSString                       *destName;
    NSString                       *newDestName;
    NSString                       *newSrcName;
    NSString                       *suffix;
    NSString                       *newSrcPath;
    NSString                       *newDestPath;
    NSFileManager                  *fmd = [NSFileManager defaultManager];
    NSError                        *nserr;
    
    srcDir = [[NSString stringWithUTF8String:srcPath] stringByDeletingLastPathComponent];
    destDir = [[NSString stringWithUTF8String:destPath] stringByDeletingLastPathComponent];
    srcName = [[NSString stringWithUTF8String:srcPath] lastPathComponent];
    destName = [[NSString stringWithUTF8String:destPath] lastPathComponent];
    manifestNames = [pc requiredManifestPathsForBootFile:[NSString stringWithUTF8String:destPath]];
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
        [fmd removeItemAtPath:newDestPath error:NULL];
        if ([fmd fileExistsAtPath:newSrcPath]) {
            if ([fmd copyItemAtPath:newSrcPath toPath:newDestPath error:&nserr] == NO) {
                blesscontextprintf(context, kBLLogLevelError, "Couldn't copy file \"%s\" - %s\n",
                                   [newDestPath UTF8String], [[nserr description] UTF8String]);
                ret = [nserr code];
                break;
            }
        }
    }
    return ret;
}


