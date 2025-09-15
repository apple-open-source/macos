/*
 * Copyright (c) 2024 Apple Inc.  All rights reserved.
 */

#import <XCTest/XCTest.h>
#import <GCoreFramework/GCore.h>
#import <Foundation/Foundation.h>
#import <Foundation/NSTask.h>
#import <err.h>
#include <sys/sysctl.h>
#include <sys/stat.h>
#include <mach-o/loader.h>

#define LEAKS_UTILITY @"/usr/bin/leaks"
#define DEFAULT_COREDUMP_FILENAME "/tmp/gcore_XXXXXXXXXX"
#define GCORE_DEBUG_LEVEL 1
// #define SHARED_FILE_DESCRIPTORS_PATH "/dev/fd/"

@interface GCoreFrameworkTests : XCTestCase
@end

@implementation GCoreFrameworkTests

mach_port_t corpse_port;
mach_port_t child_task_port;

static int
get_num_used_corpses(void)
{
    unsigned int total_corpse_count = 0;
    size_t output_size = sizeof(total_corpse_count);
    int ret = sysctlbyname("kern.total_corpses_count",
        &total_corpse_count, &output_size, NULL, 0);
    if (ret != 0) {
        fprintf(stderr, "kern.total_corpses_count error: %d/%d\n", ret, errno);
        exit(1);
    }
    return (int)total_corpse_count;
}

- (void)setUp {
    NSLog(@"setUp: %d active corpses", get_num_used_corpses());
}

- (void)tearDown {
    if (corpse_port) {
        mach_port_deallocate(mach_task_self(), corpse_port);
        corpse_port = MACH_PORT_NULL;
    }
    if (child_task_port) {
        mach_port_deallocate(mach_task_self(), child_task_port);
        child_task_port = MACH_PORT_NULL;
    }
    NSLog(@"tearDown: %d active corpses", get_num_used_corpses());
}

-(NSTask *)createChildTask {
    NSTask *task = [[NSTask alloc] init];

    NSURL *exec = [NSURL URLWithString:@"file:///bin/sleep"];
    [task setExecutableURL:exec];
    [task setArguments:@[@"100"]] ;

    NSMutableDictionary *env =
        [[[NSProcessInfo processInfo] environment] mutableCopy];
    env[@"MallocSecureAllocator"] = @"0";
    [task setEnvironment:env];

    NSError *error = nil;
    BOOL launched = [task launchAndReturnError:&error];
    XCTAssert(launched, "Failed to launch: %@\n", error);
    if (launched) {
        NSLog(@"Test running as %@ pid %d, launched %@ as pid %d",
            NSUserName(), getpid(), exec.path, [task processIdentifier]);
        return task;
    }
    return nil;
}

// Minimal check that we wrote the right header to a core file

static bool
is_valid_mach_core_fd(int fd, const char *fnm)
{
    struct stat st;
    if (fstat(fd, &st) == -1) {
        fprintf(stderr, "fstat %s: %s", fnm, strerror(errno));
        return false;
    }
    if (st.st_size == 0) {
        fprintf(stderr, "%s: file is empty\n", fnm);
        return false;
    }
    union {
        struct mach_header_64 mh64;
        struct mach_header mh32;
    } mh;
    if (st.st_size < sizeof(mh)) {
        fprintf(stderr, "%s: file too small (%llu bytes)\n", fnm, st.st_size);
        return false;
    }
    const ssize_t nr = read(fd, &mh, sizeof(mh));
    if (nr == -1) {
        fprintf(stderr, "read %s: %s\n", fnm, strerror(errno));
        return false;
    }
    if (nr != sizeof(mh)) {
        fprintf(stderr, "read %s: %ld bytes?\n", fnm, nr);
        return false;
    }
    if (mh.mh64.magic == MH_MAGIC_64) {
        return mh.mh64.filetype == MH_CORE && mh.mh64.ncmds > 1;
    } else if (mh.mh32.magic == MH_MAGIC) {
        return mh.mh32.filetype == MH_CORE && mh.mh32.ncmds > 1;
    }
    return false;
}

static bool
is_valid_mach_core(const char *fnm)
{
    const int fd = open(fnm, O_RDONLY);
    if (fd == -1) {
        fprintf(stderr, "open %s: %s\n",
                fnm, strerror(errno));
        return false;
    }
    const bool ret = is_valid_mach_core_fd(fd, fnm);
    (void) close(fd);
    return ret;
}

// Answers the question does the file have a
// reasonable mach header at the beginning?

-(void) validateCoreFile:(NSString *)file {
    XCTAssert(is_valid_mach_core([file UTF8String]), "Invalid corefile");
}

// Use the leaks utility and check if the return value is 0, meaning
// the coredump makes sense to it.
// This only works on "full" coredumps at present.

-(void) validateCoreDump:(NSString *)coredump {
    [self validateCoreFile:coredump];
    NSError *error = nil;
    NSTask *task = [NSTask launchedTaskWithExecutableURL:[NSURL fileURLWithPath:LEAKS_UTILITY]
        arguments:@[ coredump ] error:&error terminationHandler:^(NSTask *_Nonnull terminatedTask) {
        int status = terminatedTask.terminationStatus;
        XCTAssert(status < 2, "%@ exit status: %d", LEAKS_UTILITY, status);
    }];
    [task waitUntilExit];
}

-(NSString *)coreFileName
{
    char c_filename[] = DEFAULT_COREDUMP_FILENAME;
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
    char *c_filenamep = mktemp(c_filename);
#pragma clang diagnostic pop

    NSString *name = [NSString stringWithUTF8String:c_filenamep];
    return [name stringByAppendingString:@".core"];
}

/*
 * Get the task port, suspend the process, create a corpse and
 * hand the corpse port to: gcore -o filename -d -v -N -x full 0
 * (Not sure -why- we suspend the process..)
 */
-(void) testGCore_zPort_Corpse
{
    NSTask *task = [self createChildTask];
    pid_t pid = [task processIdentifier];

    kern_return_t kr = task_for_pid(mach_task_self(), pid, &child_task_port);
    XCTAssert(kr == KERN_SUCCESS,
        "Cannot obtain port for child %d, error %d (%s)",
        pid, kr, mach_error_string(kr));
    mach_port_t target = child_task_port;
#if 0
    /*
     * This doesn't seem to work in the XCtest environment, even though
     * gcore (at the time of writing) seems perfectly capable of
     * dumping from a corpse via a simple C standalone.
     */
    task_suspension_token_t suspend_token = MACH_PORT_NULL;
    kr = task_suspend2(child_task_port, &suspend_token);
    XCTAssert(kr == KERN_SUCCESS,
        "Cannot suspend task, error %d (%s)",
        kr, mach_error_string(kr));

    kr = task_generate_corpse(child_task_port, &corpse_port);
    XCTAssert(kr == KERN_SUCCESS,
        "Cannot create corpse port from child task, error %d (%s)",
        kr, mach_error_string(kr));

    kr = task_resume2(suspend_token);
    XCTAssert(kr == KERN_SUCCESS, "Cannot resume task, error %d (%s)",
        kr, mach_error_string(kr));
    target = corpse_port;
#endif

    NSString *fileName = [self coreFileName];

    NSMutableDictionary *dict = [[NSMutableDictionary alloc] init];
    dict[@GCORE_OPTION_OUT_FILENAME] = fileName;
    dict[@GCORE_OPTION_TASK_PORT] = [[NSNumber alloc] initWithInt:target];
    dict[@GCORE_OPTION_PID] = [[NSNumber alloc] initWithInt:0];
    dict[@GCORE_OPTION_DEBUG] = [[NSNumber alloc] initWithInt:GCORE_DEBUG_LEVEL];
    dict[@GCORE_OPTION_VERBOSE] = @"";
    dict[@GCORE_OPTION_ANNOTATIONS] = @"";
    dict[@GCORE_OPTION_CONTENT] = @"full";

    int ret_value = create_gcore_with_options(dict);
    XCTAssert(ret_value == 0, "Cannot create coredump, error %d (%s)",
        ret_value, strerror(ret_value));
    if (ret_value == 0) {
        [self validateCoreDump:fileName];
    }

    [[NSFileManager defaultManager] removeItemAtPath:fileName error:nil];
    [task terminate];
}

/*
 * Get the task port and hand that to gcore for dumping
 * with: gcore -o filename -d -N 0
 */
-(void) testGCore_Port_NoCorpse
{
    NSTask *task = [self createChildTask];
    pid_t pid = [task processIdentifier];

    kern_return_t kr = task_for_pid(mach_task_self(), pid, &child_task_port);
    XCTAssert(kr == KERN_SUCCESS,
        "Cannot get task port for child pid %d, error %d (%s)",
        pid, kr, mach_error_string(kr));
        
    NSString *fileName = [self coreFileName];
        
    NSMutableDictionary *dict = [[NSMutableDictionary alloc] init];
    dict[@GCORE_OPTION_OUT_FILENAME] = fileName;
    dict[@GCORE_OPTION_TASK_PORT] = [[NSNumber alloc] initWithInt:child_task_port];
    dict[@GCORE_OPTION_PID] = [[NSNumber alloc] initWithInt:0];
    dict[@GCORE_OPTION_DEBUG] = [[NSNumber alloc] initWithInt:GCORE_DEBUG_LEVEL];
    dict[@GCORE_OPTION_ANNOTATIONS] = @"";

    int ret_value = create_gcore_with_options(dict);
    XCTAssert(ret_value == 0, "Cannot create coredump, error %d (%s)",
        ret_value, strerror(ret_value));
    if (ret_value == 0) {
        [self validateCoreDump:fileName];
    }

    [[NSFileManager defaultManager] removeItemAtPath:fileName error:nil];
    [task terminate];
}

/*
 * gcore -o filename -d -N -v pid
 */
-(void) testGCore_PID_NoCorpse
{
    NSTask *task = [self createChildTask];
    pid_t pid = [task processIdentifier];

    NSString *fileName = [self coreFileName];

    NSMutableDictionary *dict = [[NSMutableDictionary alloc] init];
    dict[@GCORE_OPTION_OUT_FILENAME] = fileName;
    dict[@GCORE_OPTION_PID] = [[NSNumber alloc] initWithInt:pid];
    dict[@GCORE_OPTION_DEBUG] = [[NSNumber alloc] initWithInt:GCORE_DEBUG_LEVEL];
    dict[@GCORE_OPTION_ANNOTATIONS] = @"";
    dict[@GCORE_OPTION_VERBOSE] = @"";

    int ret_value = create_gcore_with_options(dict);
    XCTAssert(ret_value == 0, "Cannot create coredump, error %d (%s)",
              ret_value, strerror(ret_value));
    if (ret_value == 0) {
        [self validateCoreDump:fileName];
    }

    [[NSFileManager defaultManager] removeItemAtPath:fileName error:nil];
    [task terminate];
}

/*
 * gcore -s -x compact -f outfd pid
 */
-(void) testGCore_PID_fd_NoCorpse
{
    NSTask *task = [self createChildTask];
    pid_t pid = [task processIdentifier];

    NSString *fileName = [self coreFileName];
    int fd = creat([fileName UTF8String], 0600);

    NSMutableDictionary *dict = [[NSMutableDictionary alloc] init];
    dict[@GCORE_OPTION_FD] = [[NSNumber alloc] initWithInt:fd];
    dict[@GCORE_OPTION_PID] = [[NSNumber alloc] initWithInt:pid];
    dict[@GCORE_OPTION_SUSPEND] = @"";
    dict[@GCORE_OPTION_CONTENT] = @"compact";

    int ret_value = create_gcore_with_options(dict);
    close(fd);
    XCTAssert(ret_value == 0, "Cannot create coredump, error %d (%s)",
        ret_value, strerror(ret_value));
    if (ret_value == 0) {
        // Note: minimal core dump check for 'compact' content
        [self validateCoreFile:fileName];
    }

    [[NSFileManager defaultManager] removeItemAtPath:fileName error:nil];
    [task terminate];
}

#if 0
/*
 * /dev/fd/# doesn't work with gcore, currently, because it writes to
 * filename.tmp before renaming it into place so that dump creation
 * appears atomic.
 */
-(void) testGCore_PID_dev_fd_NoCorpse
{
    NSTask *task = [self createChildTask];
    pid_t pid = [task processIdentifier];

    NSString *fileName = [self coreFileName];
    int fd = creat([fileName UTF8String], 0600);

    NSMutableDictionary *dict = [[NSMutableDictionary alloc] init];
    dict[@GCORE_OPTION_OUT_FILENAME] =
        [@SHARED_FILE_DESCRIPTORS_PATH stringByAppendingFormat:@"%d", fd];
    dict[@GCORE_OPTION_PID] = [[NSNumber alloc] initWithInt:pid];
    dict[@GCORE_OPTION_DEBUG] = [[NSNumber alloc] initWithInt:GCORE_DEBUG_LEVEL];

    int ret_value = create_gcore_with_options(dict);
    close(fd);
    XCTAssert(ret_value == 0, "Cannot create coredump, error %d (%s)",
        ret_value, strerror(ret_value));
    if (ret_value == 0) {
        [self validateCoreDump:fileName];
    }

    [[NSFileManager defaultManager] removeItemAtPath:fileName error:nil];
    [task terminate];
}
#endif

@end

