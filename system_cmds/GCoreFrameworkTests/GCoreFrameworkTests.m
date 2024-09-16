/*
 * Copyright (c) 2021 Apple Inc.  All rights reserved.
 */
//

#import <XCTest/XCTest.h>
#import <GCoreFramework/GCore.h>
#import <Foundation/Foundation.h>
#import <Foundation/NSTask.h>
#import <err.h>
#include "sys/sysctl.h"

#define LEAKS_UTILITY @"/usr/bin/leaks"
#define DEFAULT_COREDUMP_FILENAME "/tmp/coredump_gtest_XXXXXXXXXX"
#define GCORE_DEBUG_LEVEL 1
#define SHARED_FILE_DESCRIPTORS_PATH "/dev/fd/"
@interface GCoreFrameworkTests : XCTestCase

@end
@implementation GCoreFrameworkTests
NSTask *gcoreTask;
mach_port_t corpse_port;
mach_port_t child_task_port;
NSString *temp_filename;
NSTask *child_task;

- (void) dealloc
{
    NSLog(@"dealloc");
    if (corpse_port != 0) {
        NSLog(@"dealloc, release port");
        corpse_port = 0;
    }
    [super dealloc];
}
- (void)setUp
{
    NSLog(@"setUp");
    temp_filename = NULL;
    
    corpse_port = 0;
    child_task_port = 0;
    [self create_child_task];
    NSLog(@"Corpses used by system before execution %d",[self get_num_used_corpses]);
    
}

- (void)tearDown
{
    // Put teardown code here. This method is called after the invocation of each test method in the class.
    NSLog(@"tearDown");
    NSLog(@"Corpses used by system after execution %d",[self get_num_used_corpses]);
    if (temp_filename!=NULL) {
        NSError *error = nil;
        [[NSFileManager defaultManager] removeItemAtPath:temp_filename error:&error];
    }
}

-(int) get_num_used_corpses  {
    size_t output_size;
    int total_corpse_count;
    int ret;
    
    output_size = sizeof(total_corpse_count);
    
    ret = sysctlbyname("kern.total_corpses_count", &total_corpse_count, &output_size, NULL, 0);
    if (ret != 0) {
        printf("sysctlbyname kern.total_corpses_count returned error: %d\n", ret);
        return -1;
    }
    return total_corpse_count;
}

-(bool) create_child_task {
    child_task = [[NSTask alloc] init];
    NSArray *args = [[NSArray alloc] init];
    NSURL *executable_path = [NSURL URLWithString:@"file:///usr/local/bin/dd" ];
    
    //    NSArray *args = @[@"localhost"];
    //    NSURL *executable_path = [NSURL URLWithString:@"file:///sbin/ping" ];
    NSMutableDictionary * env=[[[NSProcessInfo processInfo] environment] mutableCopy];
    env[@"MallocSecureAllocator"]=@"0";
    [child_task setArguments:args] ;
    [child_task setExecutableURL: executable_path];
    [child_task setEnvironment: env];
    return true;
}
/**
 * Terminate any eventual running task;
 */
-(bool) terminate_child_task {
    if (child_task!=NULL) {
        if ([child_task isRunning]) {
            [child_task terminate];
        }
    }
    return true;
}
// Call to the leaks utility and check if the return value is 0, meaning
// the coredump makes sense to it.
-(bool) validate_coredump : (NSString *) coredump
{
    @autoreleasepool {
        NSError *error = NULL;
        NSTask *task = [NSTask launchedTaskWithExecutableURL:[NSURL fileURLWithPath:LEAKS_UTILITY] arguments:@[ coredump ] error:&error terminationHandler:^(NSTask *_Nonnull terminatedTask) {
            int status = terminatedTask.terminationStatus;
            XCTAssert(status < 2,"Leaks tool execution status %d",status);
        }];
        [task waitUntilExit];
        return true;
    }
}

-(void) testGCore_zPort_Corpse
{
    @autoreleasepool {
        NSLog(@"Running tests as %@", NSUserName());
        NSError *child_error = [NSError errorWithDomain:NSPOSIXErrorDomain code:0 userInfo:nil];
        [child_task launchAndReturnError:&child_error];
        pid_t child_pid = [child_task processIdentifier];
        char c_temp_filename[] = DEFAULT_COREDUMP_FILENAME;
        char *c_temp_filename_ptr;
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
        c_temp_filename_ptr = mktemp(c_temp_filename);
#pragma clang diagnostic pop
        
        temp_filename = [NSString stringWithUTF8String:c_temp_filename_ptr];
        temp_filename = [temp_filename stringByAppendingString:@".core"];
        
        
        
        kern_return_t kr = task_for_pid(mach_task_self(), child_pid, &child_task_port);
        XCTAssert(kr == KERN_SUCCESS, "Cannot obtain port for child task, err %d", kr);
        
        task_suspension_token_t suspend_token;
        XCTAssert(task_suspend2(child_task_port, &suspend_token) == KERN_SUCCESS ,"Cannot suspend task");
        kr = task_generate_corpse(child_task_port, &corpse_port);
        
        XCTAssert(kr == KERN_SUCCESS, "Cannot create corpse_port from child task, err %d", kr);
        NSLog(@"Fork'ed PID %d", child_pid);
        NSMutableDictionary *dict_corpse = [[NSMutableDictionary alloc]init];
        dict_corpse[@GCORE_OPTION_OUT_FILENAME]   = temp_filename;
        dict_corpse[@GCORE_OPTION_TASK_PORT]      = [[NSNumber alloc] initWithInt:corpse_port];
        dict_corpse[@GCORE_OPTION_PID]            = [[NSNumber alloc] initWithInt:0];
        dict_corpse[@"debug"]                     = [[NSNumber alloc] initWithInt:GCORE_DEBUG_LEVEL];
        int ret_value = create_gcore_with_options(dict_corpse);
        XCTAssert(ret_value >= 0, "Error creating coredump");
        if (!ret_value) {
            XCTAssert([self validate_coredump :temp_filename], "Cannot validate coredump");
        }

        XCTAssert(task_resume2(suspend_token) == KERN_SUCCESS ,"Cannot resume task");
        [self terminate_child_task];
    }
}
-(void) testGCore_Port_NoCorpse
{
    @autoreleasepool {
        NSLog(@"Running tests as %@", NSUserName());
        NSError *child_error = [NSError errorWithDomain:NSPOSIXErrorDomain code:0 userInfo:nil];
        [child_task launchAndReturnError:&child_error];
        pid_t child_pid = [child_task processIdentifier];
        char c_temp_filename[] = DEFAULT_COREDUMP_FILENAME;
        char *c_temp_filename_ptr;
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
        c_temp_filename_ptr = mktemp(c_temp_filename);
#pragma clang diagnostic pop
        
        temp_filename = [NSString stringWithUTF8String:c_temp_filename_ptr];
        temp_filename = [temp_filename stringByAppendingString:@".core"];
        
        kern_return_t kr = task_for_pid(mach_task_self(), child_pid, &child_task_port);
        XCTAssert(kr == KERN_SUCCESS, "Cannot obtain port for child task, err %d", kr);
        XCTAssert(kr == KERN_SUCCESS, "Cannot create corpse_port from child task, err %d", kr);
        // Now we can kill child task
        NSLog(@"Fork'ed PID %d", child_pid);
        NSMutableDictionary *dict_corpse = [[NSMutableDictionary alloc]init];
        dict_corpse[@GCORE_OPTION_OUT_FILENAME]   = temp_filename;
        dict_corpse[@GCORE_OPTION_TASK_PORT]      = [[NSNumber alloc] initWithInt:child_task_port];
        dict_corpse[@GCORE_OPTION_PID]            = [[NSNumber alloc] initWithInt:0];
        dict_corpse[@"debug"]                     = [[NSNumber alloc] initWithInt:GCORE_DEBUG_LEVEL];
        int ret_value = create_gcore_with_options(dict_corpse);
        XCTAssert(ret_value >= 0, "Error creating coredump");
        if (!ret_value) {
            XCTAssert([self validate_coredump :temp_filename], "Cannot validate coredump");
        }
        [self terminate_child_task];
    }
}


-(void) testGCore_PID_NoCorpse
{
    @autoreleasepool {
        NSError *child_error = [NSError errorWithDomain:NSPOSIXErrorDomain code:0 userInfo:nil];
        [child_task launchAndReturnError:&child_error];
        pid_t pid = [child_task processIdentifier];
        pid_t mypid = getpid();
        NSLog(@"Main process PID %d, child pid = %d", mypid,pid);
        char c_temp_filename[] = DEFAULT_COREDUMP_FILENAME;
        char *c_temp_filename_ptr;
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
        c_temp_filename_ptr = mktemp(c_temp_filename);
#pragma clang diagnostic pop
        
        temp_filename = [NSString stringWithUTF8String:c_temp_filename_ptr];
        temp_filename = [temp_filename stringByAppendingString:@".core"];
        
        
        
        NSMutableDictionary *dict_corpse = [[NSMutableDictionary alloc]init];
        dict_corpse[@GCORE_OPTION_OUT_FILENAME]   = temp_filename;
        dict_corpse[@GCORE_OPTION_PID]            = [[NSNumber alloc] initWithInt:pid];
        dict_corpse[@"debug"]                     = [[NSNumber alloc] initWithInt:GCORE_DEBUG_LEVEL];
        int ret_value = create_gcore_with_options(dict_corpse);
        XCTAssert(ret_value >= 0, "Error creating coredump");
        XCTAssert([self validate_coredump :temp_filename], "Cannot validate coredump");
    }
    [self terminate_child_task];
}

-(void) testGCore_PID_fd_NoCorpse
{
    @autoreleasepool {
        NSError *child_error = [NSError errorWithDomain:NSPOSIXErrorDomain code:0 userInfo:nil];
        [child_task launchAndReturnError:&child_error];
        pid_t pid = [child_task processIdentifier];
        pid_t mypid = getpid();
        NSLog(@"Main process PID %d, child pid = %d", mypid,pid);
        char c_temp_filename[] = DEFAULT_COREDUMP_FILENAME;
        char *c_temp_filename_ptr;
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
        c_temp_filename_ptr = mktemp(c_temp_filename);
#pragma clang diagnostic pop
        
        temp_filename = [NSString stringWithUTF8String:c_temp_filename_ptr];
        temp_filename = [temp_filename stringByAppendingString:@".core"];
        int file_handle = creat([temp_filename UTF8String],0);
        NSMutableDictionary *dict_corpse = [[NSMutableDictionary alloc]init];
        dict_corpse[@GCORE_OPTION_FD]             = [[NSNumber alloc] initWithInt:file_handle];
        dict_corpse[@GCORE_OPTION_PID]            = [[NSNumber alloc] initWithInt:pid];
        dict_corpse[@"debug"]                     = [[NSNumber alloc] initWithInt:GCORE_DEBUG_LEVEL];
        int ret_value = create_gcore_with_options(dict_corpse);
        close(file_handle);

        XCTAssert(ret_value >= 0, "Error creating coredump");
        XCTAssert([self validate_coredump :temp_filename], "Cannot validate coredump");
    }
    [self terminate_child_task];
}

#if 0 /* Not available yet */
-(void) testGCore_PID_dev_fd_NoCorpse
{
    /*@autoreleasepool*/ {
        NSError *child_error = [NSError errorWithDomain:NSPOSIXErrorDomain code:0 userInfo:nil];
        [child_task launchAndReturnError:&child_error];
        pid_t pid = [child_task processIdentifier];
        pid_t mypid = getpid();
        NSLog(@"Main process PID %d, child pid = %d", mypid,pid);
        char c_temp_filename[] = DEFAULT_COREDUMP_FILENAME;
        char *c_temp_filename_ptr;
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
        c_temp_filename_ptr = mktemp(c_temp_filename);
#pragma clang diagnostic pop
        
        temp_filename = [NSString stringWithUTF8String:c_temp_filename_ptr];
        temp_filename = [temp_filename stringByAppendingString:@".core"];
        int file_handle = creat([temp_filename UTF8String],0);
        NSMutableDictionary *dict_corpse = [[NSMutableDictionary alloc]init];
        dict_corpse[@GCORE_OPTION_OUT_FILENAME]   = [@SHARED_FILE_DESCRIPTORS_PATH stringByAppendingFormat:@"%d",file_handle];
        dict_corpse[@GCORE_OPTION_PID]            = [[NSNumber alloc] initWithInt:pid];
        dict_corpse[@"debug"]                     = [[NSNumber alloc] initWithInt:GCORE_DEBUG_LEVEL];
        int ret_value = create_gcore_with_options(dict_corpse);
        close(file_handle);

        XCTAssert(ret_value >= 0, "Error creating coredump");
        XCTAssert([self validate_coredump :temp_filename], "Cannot validate coredump");
    }
    [self terminate_child_task];
}
#endif

@end

