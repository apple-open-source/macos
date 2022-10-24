#import <err.h>
#import <os/variant_private.h>
#import <TargetConditionals.h>

#if TARGET_OS_OSX

#import <Cocoa/Cocoa.h>

int main(int argc, const char * argv[]) {
    if (!os_variant_allows_internal_security_policies("com.apple.security")) {
        errx(1, "app unavailable");
    }
    return NSApplicationMain(argc, argv);
}

#else

#import <UIKit/UIKit.h>
#import "AppDelegate.h"

int main(int argc, char * argv[]) {
    @autoreleasepool {
        if (!os_variant_allows_internal_security_policies("com.apple.security")) {
            errx(1, "app unavailable");
        }
        return UIApplicationMain(argc, argv, nil, NSStringFromClass([AppDelegate class]));
    }
}

#endif
