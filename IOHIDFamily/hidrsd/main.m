//
//  main.m
//  hidrsd
//
//  Created by yg on 1/15/18.
//

#import <Foundation/Foundation.h>
#import <RemoteHID/RemoteHID.h>

int main(int argc __unused, const char * argv[] __unused) {
    @autoreleasepool {
        HIDRemoteDeviceAACPServer * aacpServer = [[HIDRemoteDeviceAACPServer alloc] initWithQueue:dispatch_get_main_queue()];
        [aacpServer activate];
        dispatch_main();
    }
    return 0;
}
