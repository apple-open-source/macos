//
//  HIDTalkd.m
//  HIDTalkd
//
//  Created by Josh Kergan on 8/20/19.
//

#import <objc/runtime.h>
#import <dlfcn.h>
#import <Foundation/Foundation.h>
#import <getopt.h>
#import <iostream>
#import <os/log.h>

#import "HIDTalk/HIDTalkDaemon.h"

void showHelp(int __unused argc, const char **argv);
void showHelpAndExit(int argc, const char **argv);

static struct option longopts[] = {
    // RemoteXPC toggle
    {"bridged", no_argument, NULL, 'r'},
    {0, 0, 0, 0}};

static const char* HIDTALK_DYLIB_PATH = "/AppleInternal/Library/Frameworks/HIDTalk.framework/HIDTalk";

void showHelpAndExit(int argc, const char **argv) {
    showHelp(argc, argv);
    exit(EXIT_FAILURE);
}

void showHelp(int __unused argc, const char **argv) {
    std::cerr << argv[0] << " [--bridged|-r]" << std::endl;
    std::cerr << "Options:\n";
    std::cerr << "--bridged | -r: Use RemoteXPC framework for service "
                 "registration and message sending. Used for communicating "
                 "to/from bridgeOS to macOS." << std::endl;
}

int main(int argc, const char *argv[]) {
    @autoreleasepool {
        int curOpt;
        bool remoteXPC = false;
        while ((curOpt = getopt_long(argc, (char *const *)argv, "r", longopts,
                                     NULL)) != -1) {
            switch (curOpt) {
            case 'r':
                remoteXPC = true;
                break;
            default:
                showHelpAndExit(argc, argv);
                assert(false && "Did not exit after showing help text with "
                                "invalid parameter");
            }
        }
        os_log_error(OS_LOG_DEFAULT, "Started");

        auto hid_dylib = dlopen(HIDTALK_DYLIB_PATH, RTLD_NOW | RTLD_GLOBAL);
        if (!hid_dylib) {
            os_log_error(OS_LOG_DEFAULT, "Failed to load HIDTalk framework. Please verify that it is installed like %s\n\nDLERROR: %s", HIDTALK_DYLIB_PATH, dlerror());
            exit(EXIT_FAILURE);
        }
        Class HIDTalkService_class = objc_getClass("HIDTalkService");
        if (!HIDTalkService_class) {
            os_log_error(OS_LOG_DEFAULT, "Failed to load HIDTalkService class from HIDTalk framework. DLERROR: %s", dlerror());
            exit(EXIT_FAILURE);
        }

        NSObject<HIDTalkService>* hidDeviceService = [[HIDTalkService_class alloc] init];
        NSObject<HIDTalkService>* hidClientService = [[HIDTalkService_class alloc] init];
        [hidDeviceService setDispatchQueue:dispatch_get_main_queue()];
        [hidClientService setDispatchQueue:dispatch_get_main_queue()];
        [hidDeviceService registerService:@kHIDDeviceRemoteService usingRemoteXPC:remoteXPC];
        [hidDeviceService activateService];

        [hidClientService registerService:@kHIDClientRemoteService usingRemoteXPC:remoteXPC];
        [hidClientService activateService];
        os_log_info(OS_LOG_DEFAULT, "Starting hidtalkd service");
        dispatch_main();
    }

    return 0;
}
