//
//  IOHIDUnitTestUtility.c
//  IOHIDFamily
//
//  Created by YG on 10/24/16.
//
//

#include "IOHIDUnitTestUtility.h"
#include <dispatch/private.h>
#include <mach/mach_time.h>
#include <AssertMacros.h>
#import <spawn.h>
#include <sys/time.h>
#include <IOKit/hid/IOHIDLibPrivate.h>

int __IOHIDUnitTestAttributeInit(pthread_attr_t * p_pthread_attr, int priority, int policy);
void * __IOHIDUnitTestCreateRunLoopThread(void * context);

int __IOHIDUnitTestAttributeInit(pthread_attr_t * p_pthread_attr, int priority, int policy)
{
    int err;
    struct sched_param param;
    
    err = pthread_attr_init(p_pthread_attr);
    require_noerr(err, exit);
    pthread_attr_setschedpolicy(p_pthread_attr, policy);
    require_noerr(err, exit);
    err = pthread_attr_getschedparam(p_pthread_attr, &param);
    require_noerr(err, exit);
    param.sched_priority = priority;
    err = pthread_attr_setschedparam(p_pthread_attr, &param);
    require_noerr(err, exit);
    err = pthread_attr_setdetachstate(p_pthread_attr, PTHREAD_CREATE_JOINABLE);
    require_noerr(err, exit);
    
exit:
    if ( err != 0 )
        pthread_attr_destroy( p_pthread_attr );
    
    return err;
}


dispatch_queue_t IOHIDUnitTestCreateRootQueue (int priority, int poolSize)  {
  int                ret;
  
  dispatch_queue_t   queue = nil;
  
  pthread_attr_t     attribute;
  struct sched_param param;

  ret = pthread_attr_init(&attribute);
  if (ret) {
    return queue;
  }
  ret = pthread_attr_setschedpolicy(&attribute, SCHED_RR);
  if (ret) {
    goto exit;
  }
  ret = pthread_attr_getschedparam(&attribute, &param);
  if (ret) {
    goto exit;
  }
  
  param.sched_priority = priority;

  ret = pthread_attr_setschedparam(&attribute, &param);
  if (ret) {
    goto exit;
  }
  ret = pthread_attr_setdetachstate(&attribute, PTHREAD_CREATE_JOINABLE);
  if (ret) {
    goto exit;
  }
  queue = dispatch_pthread_root_queue_create("IOHIDUnitTestCreateRootQueue - Root", dispatch_pthread_root_queue_flags_pool_size(poolSize), &attribute, NULL);
  
exit:
  
  pthread_attr_destroy(&attribute);
  
  return queue;
}


uint64_t  IOHIDInitTestAbsoluteTimeToNanosecond (uint64_t abs) {
  static mach_timebase_info_data_t timebase;
  if (!timebase.denom) {
    mach_timebase_info(&timebase);
  }
  return (abs * timebase.numer) / (timebase.denom);
}

dispatch_semaphore_t    __sema = NULL;
CFRunLoopRef            __runLoop = NULL;

void * __IOHIDUnitTestCreateRunLoopThread(void * context __unused)
{
    CFRunLoopRef runloop = CFRunLoopGetCurrent();
    CFRunLoopSourceRef      dummySource     = NULL;
    CFRunLoopSourceContext  dummyContext    = {};
    
    dummySource = CFRunLoopSourceCreate(kCFAllocatorDefault, 0, &dummyContext);
    require(dummySource, exit);
    
    __runLoop = runloop;
    
    CFRunLoopAddSource(runloop, dummySource, kCFRunLoopDefaultMode);
    
    dispatch_semaphore_signal(__sema);
    
    CFRunLoopRun();
    
    CFRunLoopRemoveSource(runloop, dummySource, kCFRunLoopDefaultMode);
    
exit:
    if ( dummySource )
        CFRelease(dummySource);
    
    return NULL;
}

pthread_t hidThread;

CFRunLoopRef IOHIDUnitTestCreateRunLoop (int priority)  {
    pthread_attr_t attribute;
    
    if (__IOHIDUnitTestAttributeInit (&attribute, priority, SCHED_RR)) {
        return NULL;
    }
    __sema = dispatch_semaphore_create(0);
    
    if (!pthread_create(&hidThread, &attribute,__IOHIDUnitTestCreateRunLoopThread, NULL)) {
        dispatch_semaphore_wait(__sema, DISPATCH_TIME_FOREVER);
    }
    
    __sema = nil;
    
    if (__runLoop) {
        CFRetain(__runLoop);
    }
    return __runLoop;
}

void IOHIDUnitTestDestroyRunLoop (CFRunLoopRef runloop)  {
    CFRunLoopStop(runloop);
    CFRelease(runloop);
}

char * const  hidutil[]  = { "/usr/local/bin/hidutil", "dump", NULL };
const char * spindump[]  = { "/usr/sbin/spindump", "-notarget", "5", "10", "-o", NULL, NULL};
char *const logcollect[] = { "/usr/bin/log", "show", "--debug", "--predicate", "senderImagePath contains \"IOHIDFamily\" or subsystem == \"com.apple.iohid\"", NULL };
char *const ioreg[]      = { "/usr/sbin/ioreg", "-lw0", NULL };
const char * tailspin[]  = { "/usr/bin/tailspin", "save", "-r", "hidxctest", "-o" , "-n" , NULL, NULL};

void IOHIDUnitTestRunPosixCommand(char *const argv[], NSString *stdoutFile)
{
    int status;
    pid_t pid;
    posix_spawn_file_actions_t child_fd_actions = 0;
    
    if (stdoutFile) {
        // for reading stdout
        posix_spawn_file_actions_init(&child_fd_actions);
        status = posix_spawn_file_actions_addopen(&child_fd_actions, STDOUT_FILENO, [stdoutFile cStringUsingEncoding:NSUTF8StringEncoding], O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (status) {
            NSLog(@"posix_spawn_file_actions_addopen (,,%@,,):%s", stdoutFile, strerror(status));
        }
    }
    
    NSLog(@"Running %s > %s", argv[0], [stdoutFile cStringUsingEncoding:NSUTF8StringEncoding]);
    status = posix_spawnp(&pid, argv[0], &child_fd_actions, NULL, argv, NULL);
    if (status) {
        NSLog(@"posix_spawnp: %s", strerror(status));
    } else {
        waitpid(pid, &status, 0);
        NSLog(@"%s exited with status %i\n", argv[0], status);
    }
}

CFStringRef IOHIDUnitTestCreateStringFromTimeval(CFAllocatorRef allocator, struct timeval *tv)
{
    struct tm tmd;
    struct tm *local_time;
    char time_str[32] = { 0, };
    
    local_time = localtime_r(&tv->tv_sec, &tmd);
    if (local_time == NULL) {
        local_time = gmtime_r(&tv->tv_sec, &tmd);
    }
    
    if (local_time) {
        strftime(time_str, sizeof(time_str), "%F_%H:%M:%S", local_time);
    }
    
    
    CFStringRef time = CFStringCreateWithFormat(allocator, NULL, CFSTR("%s.%06d"), time_str, tv->tv_usec);
    return time;
}

void IOHIDUnitTestCollectLogs (uint32_t logTypes, char * fileName, int line)
{
    struct timeval t;
    gettimeofday(&t, NULL);
  
    NSString * path = [NSString stringWithFormat:@"%s/%s", kLogDestinationDir, fileName];
    
    NSString * base = [NSString stringWithFormat:@"%@/%d_%@",
                                                path,
                                                line,
                                                CFBridgingRelease(IOHIDUnitTestCreateStringFromTimeval(kCFAllocatorDefault, &t))];
    
    NSFileManager * manager =  [NSFileManager defaultManager];
    
    if ([manager fileExistsAtPath: path] == NO) {
        [manager createDirectoryAtPath:path withIntermediateDirectories:YES attributes:nil error:nil];
    }
    if (logTypes & COLLECT_SPINDUMP) {
        NSString * outFile = [NSString stringWithFormat:@"%@_spindump.txt", base];
        spindump[5] = [outFile cStringUsingEncoding:NSUTF8StringEncoding];
        IOHIDUnitTestRunPosixCommand((char * const *)spindump, NULL);
    }
    if (logTypes & COLLECT_TAILSPIN) {
        NSString * outFile = [NSString stringWithFormat:@"%@_tailspin.tailspin", base];
        tailspin[6] = [outFile cStringUsingEncoding:NSUTF8StringEncoding];
        IOHIDUnitTestRunPosixCommand((char * const *)tailspin, NULL);
    }
    if (logTypes & COLLECT_HIDUTIL) {
        IOHIDUnitTestRunPosixCommand(hidutil, [NSString stringWithFormat:@"%@_hidutil.plist", base]);
    }
    if (logTypes & COLLECT_IOREG) {
        IOHIDUnitTestRunPosixCommand(ioreg, [NSString stringWithFormat:@"%@_ioreg.txt", base]);
    }
    if (logTypes & COLLECT_LOGARCHIVE) {
        IOHIDUnitTestRunPosixCommand(logcollect, [NSString stringWithFormat:@"%@_logdump.txt", base]);
    }
}
