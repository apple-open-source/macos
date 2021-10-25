//
//  pythonwrapper.m
//  pythonwrapper
//
//  Created by Andy Kaplan on 1/14/21.
//

#import <Foundation/Foundation.h>
#import <libgen.h>
#import "pythonpromptProtocol.h"
#import "pythonprompt.h"

#define PYTHON_UNWRAPPED_BIN "/System/Library/Frameworks/Python.framework/Versions/2.7/bin/unwrapped"
#define PYTHON_NO_EXEC_ARG "noexec"

void triggerPrompt(int pid, NSString *newBinPath, NSString *oldBinPath)
{
    NSXPCConnection *connection;
    id<pythonpromptProtocol> service;
    __block BOOL ok = YES;
    
    connection = [[NSXPCConnection alloc] initWithServiceName:@"com.apple.python.pythonprompt"];
    connection.remoteObjectInterface = [NSXPCInterface interfaceWithProtocol:@protocol(pythonpromptProtocol)];
    [connection resume];
    service = [connection synchronousRemoteObjectProxyWithErrorHandler:^(NSError * _Nonnull error) {
        ok = NO;
    }];
    if (!ok) {
        goto done;
    }
    [service prompt:pid withNewBinPath:newBinPath withOldBinPath:oldBinPath withReply:^(NSString *result) {
    }];
    
done:
    [connection invalidate];
}

int main(int argc, char *const argv[])
{
    @autoreleasepool {
        NSString *binName = [NSString stringWithFormat:@"%s", basename(argv[0])];
        BOOL execPython = YES;
        
        if ([binName isEqualToString:@"pythonwrapper"]) {
            if (argc >= 2 && [[NSString stringWithFormat:@"%s", basename(argv[1])] isEqualToString:@PYTHON_NO_EXEC_ARG]) {
                execPython = NO;
            } else {
                NSLog(@"pythonwrapper is not supposed to be executed directly. Exiting.");
                exit(0);
            }
        }
        
        NSString *newBinPath = [NSString stringWithFormat:@"%@/%@", @PYTHON_UNWRAPPED_BIN, binName];
        NSString *oldBinPath = [NSString stringWithUTF8String:argv[0]];
        NSProcessInfo *info = [NSProcessInfo processInfo];
        
        //   Running the analytics/prompt code in an XPC service allows us to bypass the sandbox
        triggerPrompt(info.processIdentifier, newBinPath, oldBinPath);
            
        if (execPython && execv(newBinPath.UTF8String, argv) == -1) {
            int savedErrno = errno;
            NSLog(@"Could not exec %@ from %@: %s.", newBinPath, oldBinPath, strerror(savedErrno));
            return 1;
        }
    }
    
    return 0;
}
