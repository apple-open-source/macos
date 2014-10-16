/*
 *  check_entitlements.c
 *
 *
 *  Created by Conrad Sauerwald on 7/9/08.
 *  Copyright 2008 __MyCompanyName__. All rights reserved.
 *
 */

#include <paths.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <getopt.h>
#include <stdbool.h>
#include <limits.h>

#define DEBUG_ASSERT_PRODUCTION_CODE 0
#include <AssertMacros.h>

#include <CoreFoundation/CoreFoundation.h>

#define log(format, args...)    \
    fprintf(stderr, "codesign_wrapper " format "\n", ##args);
#define cflog(format, args...) do { \
CFStringRef logstr = CFStringCreateWithFormat(NULL, NULL, CFSTR(format), ##args); \
if (logstr) { CFShow(logstr); CFRelease(logstr); } \
} while(0); \

enum { CSMAGIC_EMBEDDED_ENTITLEMENTS = 0xfade7171 };

typedef struct {
    uint32_t type;
    uint32_t offset;
} cs_blob_index;

CFDataRef
extract_entitlements_blob(const uint8_t *data, size_t length)
{
    CFDataRef entitlements = NULL;
    cs_blob_index *csbi = (cs_blob_index *)data;

    require(data && length, out);
    require(csbi->type == ntohl(CSMAGIC_EMBEDDED_ENTITLEMENTS), out);
    require(length == ntohl(csbi->offset), out);
    entitlements = CFDataCreate(kCFAllocatorDefault,
        (uint8_t*)(data + sizeof(cs_blob_index)),
        (CFIndex)(length - sizeof(cs_blob_index)));
out:
    return entitlements;
}

typedef struct {
    bool valid_application_identifier;
    bool valid_keychain_access_group;
    bool illegal_entitlement;
} filter_whitelist_ctx;

void
filter_entitlement(const void *key, const void *value,
        filter_whitelist_ctx *ctx)
{
    if (CFEqual(key, CFSTR("application-identifier"))) {
        // value isn't string
        if (CFGetTypeID(value) != CFStringGetTypeID()) {
            cflog("ERR: Illegal entitlement value %@ for key %@", value, key);
            return;
        }

        // log it for posterity
        cflog("NOTICE: application-identifier := '%@'", value);

        // - put in an application-identifier that is messed up: <char-string>.<char-string-repeat>.
        // split ident by periods and make sure the first two are not the same
        // - put in an application-identifier they're not allowed to have: but we have no way to tell, although "apple" is illegal
        // is apple, superseded by doesn't have at least 2 components split by a period

        CFArrayRef identifier_pieces = CFStringCreateArrayBySeparatingStrings(kCFAllocatorDefault, value, CFSTR("."));	/* No separators in the string returns array with that string; string == sep returns two empty strings */
        if (!identifier_pieces || (CFArrayGetCount(identifier_pieces) < 2)) {
            cflog("ERR: Malformed identifier %@ := %@", key, value);
            return;
        }

        // doubled-up identifier
        if (CFEqual(CFArrayGetValueAtIndex(identifier_pieces, 0),
            CFArrayGetValueAtIndex(identifier_pieces, 1))) {
                cflog("ERR: Malformed identifier %@ := %@", key, value);
                return;
        }

        // incomplete identifier: "blabla."
        if (CFEqual(CFArrayGetValueAtIndex(identifier_pieces, 1), CFSTR(""))) {
            cflog("ERR: Malformed identifier %@ := %@", key, value);
            return;
        }

        ctx->valid_application_identifier = true;
        return;
    }

    if (CFEqual(key, CFSTR("keychain-access-groups"))) {
        // if there is one, false until proven correct
        ctx->valid_keychain_access_group = false;

        // log it for posterity - we're not expecting people to use it yet
        cflog("NOTICE: keychain-access-groups := %@", value);

        // - put in keychain-access-groups containing "apple"
        if (CFGetTypeID(value) == CFStringGetTypeID()) {
            if (CFEqual(CFSTR("apple"), value) ||
                (CFStringFind(value, CFSTR("*"), 0).location != kCFNotFound)) {
                cflog("ERR: Illegal keychain access group value %@ for key %@", value, key);
                return;
            }
        } else if (CFGetTypeID(value) == CFArrayGetTypeID()) {
            CFIndex i, count = CFArrayGetCount(value);
            for (i=0; i<count; i++) {
                CFStringRef val = (CFStringRef)CFArrayGetValueAtIndex(value, i);
                if (CFGetTypeID(val) != CFStringGetTypeID()) {
                    cflog("ERR: Illegal value in keychain access groups array %@", val);
                    return;
                }
                if (CFEqual(CFSTR("apple"), val) ||
                    (CFStringFind(val, CFSTR("*"), 0).location != kCFNotFound)) {
                    cflog("ERR: Illegal keychain access group value %@ for key %@", value, key);
                    return;
                }
            }
        } else {
            cflog("ERR: Illegal entitlement value %@ for key %@", value, key);
            return;
        }

        ctx->valid_keychain_access_group = true;
        return;
    }

    // - double check there's no "get-task-allow"
    // - nothing else should be allowed
    cflog("ERR: Illegal entitlement key '%@' := '%@'", key, value);
    ctx->illegal_entitlement = true;
}

bool
filter_entitlements(CFDictionaryRef entitlements)
{
    if (!entitlements)
        return true;

    // did not put in an application-identifier: that keeps us from identifying the app securely
    filter_whitelist_ctx ctx = { /* valid_application_identifier */ false,
                                 /* valid_keychain_access_group */ true,
                                 /* illegal_entitlement */ false };
    CFDictionaryApplyFunction(entitlements,
            (CFDictionaryApplierFunction)filter_entitlement, &ctx);
    return (ctx.valid_application_identifier && ctx.valid_keychain_access_group &&
        !ctx.illegal_entitlement);
}

static pid_t kill_child = -1;
void child_timeout(int sig)
{
    if (kill_child != -1) {
        kill(kill_child, sig);
        kill_child = -1;
    }
}

pid_t fork_child(void (*pre_exec)(void *arg), void *pre_exec_arg,
        const char * const argv[])
{
    unsigned delay = 1, maxDelay = 60;
    for (;;) {
        pid_t pid;
        switch (pid = fork()) {
            case -1: /* fork failed */
                switch (errno) {
                    case EINTR:
                        continue; /* no problem */
                    case EAGAIN:
                        if (delay < maxDelay) {
                            sleep(delay);
                            delay *= 2;
                            continue;
                        }
                        /* fall through */
                    default:
                        perror("fork");
                        return -1;
                }
                assert(-1); /* unreached */

            case 0: /* child */
                if (pre_exec)
                    pre_exec(pre_exec_arg);
                execv(argv[0], (char * const *)argv);
                perror("execv");
                _exit(1);

            default: /* parent */
                return pid;
                break;
        }
        break;
    }
    return -1;
}


int fork_child_timeout(void (*pre_exec)(), char *pre_exec_arg,
        const char * const argv[], int timeout)
{
    int exit_status = -1;
    pid_t child_pid = fork_child(pre_exec, pre_exec_arg, argv);
    if (timeout) {
        kill_child = child_pid;
        alarm(timeout);
    }
    while (1) {
        int err = wait4(child_pid, &exit_status, 0, NULL);
        if (err == -1) {
            perror("wait4");
            if (errno == EINTR)
                continue;
        }
        if (err == child_pid) {
            if (WIFSIGNALED(exit_status)) {
                log("child %d received signal %d", child_pid, WTERMSIG(exit_status));
                kill(child_pid, SIGHUP);
                return -2;
            }
            if (WIFEXITED(exit_status))
                return WEXITSTATUS(exit_status);
            return -1;
        }
    }
}


void dup_io(int arg[])
{
    dup2(arg[0], arg[1]);
    close(arg[0]);
}


int fork_child_timeout_output(int child_fd, int *parent_fd, const char * const argv[], int timeout)
{
    int output[2];
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, output))
        return -1;
    fcntl(output[1], F_SETFD, 1); /* close in child */
    int redirect_child[] = { output[0], child_fd };
    int err = fork_child_timeout(dup_io, (void*)redirect_child, argv, timeout);
    if (!err) {
        close(output[0]); /* close the child side in the parent */
        *parent_fd = output[1];
    }
    return err;
}

ssize_t read_fd(int fd, void **buffer)
{
    int err = -1;
    size_t capacity = 1024;
    char * data = malloc(capacity);
    size_t size = 0;
    while (1) {
        int bytes_left = capacity - size;
        int bytes_read = read(fd, data + size, bytes_left);
        if (bytes_read >= 0) {
            size += bytes_read;
            if (capacity == size) {
                capacity *= 2;
                data = realloc(data, capacity);
                if (!data) {
                    err = -1;
                    break;
                }
                continue;
            }
            err = 0;
        } else
            err = -1;
        break;
    }
    if (0 == size) {
        if (data)
            free(data);
        return err;
    }

    *buffer = data;
    return size;
}

static char * codesign_binary = "/usr/bin/codesign";

int
main(int argc, char *argv[])
{
#if 0
    if (argc == 1) {
        CFArrayRef empty_array = CFArrayCreate(NULL, NULL, 0, NULL);
        CFMutableDictionaryRef dict = CFDictionaryCreateMutable(kCFAllocatorDefault,
            0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);

        // empty
        require(!filter_entitlements(dict), fail_test);

        CFDictionarySetValue(dict, CFSTR("get-task-allow"), kCFBooleanTrue);

        // no get-task-allow allowed
        require(!filter_entitlements(dict), fail_test);
        CFDictionaryRemoveValue(dict, CFSTR("get-task-allow"));

        CFDictionarySetValue(dict, CFSTR("application-identifier"), empty_array);
        require(!filter_entitlements(dict), fail_test);

        CFDictionarySetValue(dict, CFSTR("application-identifier"), CFSTR("apple"));
        require(!filter_entitlements(dict), fail_test);
        CFDictionarySetValue(dict, CFSTR("application-identifier"), CFSTR("AJ$K#GK$.AJ$K#GK$.hoi"));
        require(!filter_entitlements(dict), fail_test);
        CFDictionarySetValue(dict, CFSTR("application-identifier"), CFSTR("AJ$K#GK$."));
        require(!filter_entitlements(dict), fail_test);
        CFDictionarySetValue(dict, CFSTR("application-identifier"), CFSTR("AJ$K#GK$.hoi"));
        require(filter_entitlements(dict), fail_test);

        CFDictionarySetValue(dict, CFSTR("keychain-access-groups"), CFSTR("apple"));
        require(!filter_entitlements(dict), fail_test);
        const void *ary[] = { CFSTR("test"), CFSTR("apple") };
        CFArrayRef ka_array = CFArrayCreate(NULL, ary, sizeof(ary)/sizeof(*ary), NULL);
        CFDictionarySetValue(dict, CFSTR("keychain-access-groups"), ka_array);
        require(!filter_entitlements(dict), fail_test);
        CFDictionarySetValue(dict, CFSTR("keychain-access-groups"), CFSTR("AJ$K#GK$.joh"));
        require(filter_entitlements(dict), fail_test);
        CFDictionarySetValue(dict, CFSTR("this-should-not"), CFSTR("be-there"));
        require(!filter_entitlements(dict), fail_test);

        exit(0);
fail_test:
        fprintf(stderr, "failed internal test\n");
        exit(1);
    }
#endif

    if (argc != 2) {
        fprintf(stderr, "usage: %s <file>\n", argv[0]);
        exit(1);
    }

    do {

        fprintf(stderr, "NOTICE: check_entitlements on %s", argv[1]);

        int exit_status;
        const char * const extract_entitlements[] =
        { codesign_binary, "--display", "--entitlements", "-", argv[1], NULL };
        int entitlements_fd;
        if ((exit_status = fork_child_timeout_output(STDOUT_FILENO, &entitlements_fd,
                        extract_entitlements, 0))) {
            fprintf(stderr, "ERR: failed to extract entitlements: %d\n", exit_status);
            break;
        }

        void *entitlements = NULL;
        size_t entitlements_size = read_fd(entitlements_fd, &entitlements);
        if (entitlements_size == -1)
            break;
        close(entitlements_fd);

        if (entitlements && entitlements_size) {
            CFDataRef ent = extract_entitlements_blob(entitlements, entitlements_size);
            free(entitlements);
            require(ent, out);
            CFPropertyListRef entitlements_dict =
                CFPropertyListCreateFromXMLData(kCFAllocatorDefault,
                ent, kCFPropertyListImmutable, NULL);
            CFRelease(ent);
            require(entitlements_dict, out);
            if (!filter_entitlements(entitlements_dict)) {
                fprintf(stderr, "ERR: bad entitlements\n");
                exit(1);
            }
            exit(0);
        } else {
            fprintf(stderr, "ERR: no entitlements!\n");
        }
    } while (0);
out:
    return 1;
}