/*
 * Copyright (c) 2025 Apple Inc.  All rights reserved.
 */

#import <Foundation/Foundation.h>
#include <os/log.h>

#include "GCore.h"

#include <sys/types.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdarg.h>
#include <signal.h>
#include <unistd.h>
#include <errno.h>
#include <ctype.h>
#include <fcntl.h>
#include <spawn.h>
#include <mach/mach.h>
#include <errno.h>
#include <crt_externs.h>
#include <sysexits.h>

#define GCORE_CLI_OPTION_VERBOSE       "-v"
#define GCORE_CLI_OPTION_SUSPEND       "-s"
#define GCORE_CLI_OPTION_OUT_FILENAME  "-o"
#define GCORE_CLI_OPTION_PATHFORMAT    "-c"
#define GCORE_CLI_OPTION_MAX_SIZE      "-b"
#define GCORE_CLI_OPTION_PID           ""         /* must be last argument */
#define GCORE_CLI_OPTION_CONTENT       "-x"

#define GCORE_CLI_OPTION_QUIET         "-q"
#define GCORE_CLI_OPTION_CORPSIFY      "-C"
#define GCORE_CLI_OPTION_ANNOTATIONS   "-N"
#define GCORE_CLI_OPTION_TASK_PORT     ""         /* (not a real option!) */
#define GCORE_CLI_OPTION_FD            "-f"
// #define GCORE_CLI_OPTION_NCACHE_THR    "-t"
#define GCORE_CLI_OPTION_DEBUG         "-d"

#define STR_NSINTEGER "NSINTEGER"
#define STR_NSSTRING  "NSSTRING"

#define LOG_SUBSYSTEM "com.apple.gcore"
#define LOG_CATEGORY  "framework"

#define os_assert_zero(X) {      \
    int __aret; \
    if ((__aret = (X)) != 0) { \
        os_log_error(logger, "Expr " #X " failed with result %d", __aret); \
    } \
}

typedef struct options_types_t {
    const char *dictionary_option_name;
    const char *command_line_option;
    const char *type_name_for_argument;
} options_types;

static options_types parsing_options[] = {
    // Options corresponding to gcore.1
    { .dictionary_option_name = GCORE_OPTION_VERBOSE,        .command_line_option = GCORE_CLI_OPTION_VERBOSE      , .type_name_for_argument = NULL          },
    { .dictionary_option_name = GCORE_OPTION_SUSPEND,        .command_line_option = GCORE_CLI_OPTION_SUSPEND      , .type_name_for_argument = NULL          },
    { .dictionary_option_name = GCORE_OPTION_OUT_FILENAME,   .command_line_option = GCORE_CLI_OPTION_OUT_FILENAME , .type_name_for_argument = STR_NSSTRING  },
    { .dictionary_option_name = GCORE_OPTION_PATHFORMAT,     .command_line_option =
        GCORE_CLI_OPTION_PATHFORMAT   , .type_name_for_argument = STR_NSSTRING  },
    { .dictionary_option_name = GCORE_OPTION_MAX_SIZE,       .command_line_option = GCORE_CLI_OPTION_MAX_SIZE     , .type_name_for_argument = STR_NSINTEGER },
    { .dictionary_option_name = GCORE_OPTION_PID,            .command_line_option = GCORE_CLI_OPTION_PID          , .type_name_for_argument = STR_NSINTEGER },
    { .dictionary_option_name = GCORE_OPTION_CONTENT,        .command_line_option =
        GCORE_CLI_OPTION_CONTENT      , .type_name_for_argument = STR_NSSTRING  },
    // Options corresponding to gcore-internal.1
    { .dictionary_option_name = GCORE_OPTION_QUIET,          .command_line_option = GCORE_CLI_OPTION_QUIET        , .type_name_for_argument = NULL          },
    { .dictionary_option_name = GCORE_OPTION_CORPSIFY,       .command_line_option = GCORE_CLI_OPTION_CORPSIFY     , .type_name_for_argument = NULL          },
    { .dictionary_option_name = GCORE_OPTION_ANNOTATIONS,    .command_line_option = GCORE_CLI_OPTION_ANNOTATIONS  , .type_name_for_argument = NULL          },
    { .dictionary_option_name = GCORE_OPTION_TASK_PORT,      .command_line_option = GCORE_CLI_OPTION_TASK_PORT    , .type_name_for_argument = NULL          },
    { .dictionary_option_name = GCORE_OPTION_FD,             .command_line_option = GCORE_CLI_OPTION_FD           , .type_name_for_argument = STR_NSINTEGER },
    { .dictionary_option_name = GCORE_OPTION_DEBUG,          .command_line_option = GCORE_CLI_OPTION_DEBUG        , .type_name_for_argument = STR_NSINTEGER },
};

/* Logger to be used with the operation. */
static os_log_t logger;

static options_types *
get_option_for_name(NSString *name)
{
	if (name != NULL) {
		const char *cmd_str = [name UTF8String];
		for (uint64_t index = 0; index < sizeof(parsing_options) / sizeof(parsing_options[0]); index++) {
			options_types *entry = &parsing_options[index];

			if (!strncmp(cmd_str, entry->dictionary_option_name, strlen(entry->dictionary_option_name))) {
				return entry;
			}
		}
	}
	return NULL;
}

/**
 * Obtains a integer value and copies its reference to return value.
 * If the value cannot be found or is invalid returns false, else
 * (when the value can be obtained) returns true.
 */
static bool
get_integer_value(NSString *key, NSDictionary *dict, NSNumber **return_value, int *error_ptr)
{
	id val_raw = dict[key];
	bool ret_status = false;
	if (error_ptr != NULL) {
		*error_ptr = 0;
	}
	if (![val_raw isKindOfClass:[NSNumber class]]) {
		if (error_ptr != NULL) {
			*error_ptr = ERANGE;
		}
	} else {
		*return_value = val_raw;
		ret_status = true;
	}
	return ret_status;
}

// This is difficult to translate in general, but it's
// roughly how gcore maps its exit code meanings

static int
sysexit_to_errno(int code)
{
    switch (code) {
        case EX_OK:
            return 0;
        case EX_NOPERM:
            return EPERM;
        case EX_CANTCREAT:
            return EACCES;
        case EX_SOFTWARE:
        case EX_OSERR:
            return ECONNREFUSED; // quite a stretch!
        case EX_TEMPFAIL:
            return EAGAIN;
        case EX_IOERR:
            return EIO;
        case EX_CONFIG:
            return ENODEV;
        case EX_USAGE:
        case EX_DATAERR:
        default:
            return EINVAL;
    }
}

/**
 * Spawn to create a gcore instance
 */
static int
spawn_gcore(int argc, char **argv, mach_port_t corpse_mach_port,
    int corefd, bool need_stdout)
{
	if (argc < 2) { /* [0] executable, at least one arg (pid or port) */
		return EINVAL;
	}

    if (MACH_PORT_VALID(corpse_mach_port)) {
        const kern_return_t kr = mach_ports_register(mach_task_self(),
            (mach_port_t[]){corpse_mach_port}, 1);
        if (kr != KERN_SUCCESS) {
            os_log_error(logger, "Cannot register corpse port error 0x%x (%s)",
                kr, mach_error_string(kr));
            return EINVAL;
        }
    }
    posix_spawnattr_t spawnattr;
    os_assert_zero(posix_spawnattr_init(&spawnattr));
    
    os_assert_zero(posix_spawnattr_setflags(&spawnattr, POSIX_SPAWN_SETPGROUP));
    os_assert_zero(posix_spawnattr_setflags(&spawnattr, POSIX_SPAWN_CLOEXEC_DEFAULT));
    os_assert_zero(posix_spawnattr_setflags(&spawnattr,
        POSIX_SPAWN_SETSIGDEF | POSIX_SPAWN_SETSIGMASK));

    posix_spawn_file_actions_t factions = NULL;
    if (corefd >= 0 || need_stdout) {
        /*
         * Since CLOEXEC_DEFAULT is set above, fd's will be marked
         * close-on-exec by default. Optionally mark stdout and
         * corefd as inherited.
         */
        os_assert_zero(posix_spawnattr_init(&factions));
        if (corefd >= 0) {
            os_assert_zero(posix_spawn_file_actions_addinherit_np(&factions,
                corefd));
        }
        if (need_stdout) {
            os_assert_zero(posix_spawn_file_actions_addinherit_np(&factions,
                fileno(stdout)));
        }
    }
    
    sigset_t all_signals;
    sigset_t no_signals;
    os_assert_zero(sigfillset(&all_signals));
    os_assert_zero(sigemptyset(&no_signals));

    os_assert_zero(posix_spawnattr_setsigdefault(&spawnattr, &all_signals));
    os_assert_zero(posix_spawnattr_setsigmask(&spawnattr, &no_signals));
    
    pid_t spawn_pid = -1;
    int status = posix_spawnp(&spawn_pid, argv[0], &factions, &spawnattr, argv, *_NSGetEnviron());
    posix_spawnattr_destroy(&spawnattr);

    if (status != 0) {
        os_log_error(logger, "posix_spawn %s returns %d (%s)",
            argv[0], status, strerror(status));
        return status;
    }
    
    int retValue = 0;
    do {
        os_log_debug(logger, "wait4 %s pid %d", argv[0], spawn_pid);
        const int wpid = wait4(spawn_pid, &status, 0, NULL);
        if (wpid == -1) {
            const int error = errno;
            os_log_debug(logger, "wait4: %d error: errno %d (%s)",
                spawn_pid, error, strerror(error));
            // Assuming wait4 arguments are sane, might receive:
            // - ECHILD: no child - some other process reaped it first?
            // - EINTR: the caller was interrupted, likely by SIGCHLD
            if (error == EINTR) {
                continue;
            }
            retValue = error;
            break;
        }
        os_log_debug(logger, "wait4: %d status 0x%x\n", wpid, status);
        if (WIFEXITED(status)) {
            const int ecode = WEXITSTATUS(status);
            if (ecode == EX_OK) {
                break;
            }
            retValue = sysexit_to_errno(ecode);
            os_log_error(logger, "wait4: %d %s exit status %d, ret %d",
                wpid, argv[0], ecode, retValue);
            break;
        }
        if (WIFSIGNALED(status)) {
            retValue = EINTR;
            const int sig = WTERMSIG(status);
            os_log_error(logger, "wait4: %d signal %d (%s), ret %d",
                wpid, sig, strsignal(sig), retValue);
            break;
        }
        if (WIFSTOPPED(status) || WIFCONTINUED(status)) {
            // Since WUNTRACED wasn't set, this means the child is being traced
            const int sig = WSTOPSIG(status);
            os_log_debug(logger, "wait4: %d signal %d (%s)",
                wpid, sig, strsignal(sig));
        }
    } while (true);

    return retValue;
}

/**
 * Create a coredump with the selected options, return 0 if the coredump
 * was properly created or an errno
 */
__attribute__((visibility("default"))) int
create_gcore_with_options(NSDictionary *options)
{
    int ret_value = EPERM;
    int file_descriptor_core = -1;
    bool need_stdout = false;
    int debug_log_level = 0;
    mach_port_t process_port = MACH_PORT_NULL;
    NSString *target_pid = nil;

    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        logger = os_log_create(LOG_SUBSYSTEM, LOG_CATEGORY);
    });
    
	@autoreleasepool {
		NSMutableArray *options_processed = [[NSMutableArray alloc] init];
		// Iterate all the user options and convert them to gcore options
		// To print out all key-value pairs in the NSDictionary myDict
        for (id key in options) {
            NSString *stringfy_argument = nil;
            if (![key isKindOfClass:[NSString class]]) {
                return EINVAL; // Invalid argument
            }
            NSString *key_str = (NSString *)key;
            // If the parameter is debug, need to intercept the log level
            if ([key_str compare:@GCORE_OPTION_DEBUG] == NSOrderedSame) {
                need_stdout = true;
                int error_code;
                NSNumber *number_param;
                if (!get_integer_value(key, options, &number_param, &error_code)) {
                    return error_code;
                }
                debug_log_level = [number_param intValue];
                continue;
            }
            if ([key_str compare:@GCORE_OPTION_VERBOSE] == NSOrderedSame) {
                 need_stdout = true;
                 continue;
            }
			// If the parameter is a FD, need to intercept it
			if ([key_str compare:@GCORE_OPTION_FD] == NSOrderedSame) {
				int error_code;
				NSNumber *number_param;
				if (!get_integer_value(key, options, &number_param, &error_code)) {
					return error_code;
				}
				file_descriptor_core = [number_param intValue];
                continue;
			}
			// If the parameter is a mach port, need to intercept it
			if ([key_str compare:@GCORE_OPTION_TASK_PORT] == NSOrderedSame) {
				int error_code;
				NSNumber *number_param;

				if (!get_integer_value(key, options, &number_param, &error_code)) {
					return error_code;
				}
				process_port = [number_param intValue];
                continue;    // Do not add a CLI parameter to gcore, just share the port.
			}
			options_types *option_declaration = get_option_for_name(key_str);
			if (option_declaration == NULL) {
                os_log(logger, "unrecognized option: %s", [key_str UTF8String]);
				return EDOM;
			}
			// Does the current option require a value?
			if (option_declaration->type_name_for_argument != NULL) {
				id user_value = options[key];
				if (user_value == nil) {
					return ERANGE;
				}
				if (!strncmp(option_declaration->type_name_for_argument, STR_NSINTEGER, strlen(STR_NSINTEGER))) {
					int error_code;
					NSNumber *number_param;
					if (!get_integer_value(key, options, &number_param, &error_code)) {
						return error_code;
					}

					stringfy_argument =  [number_param stringValue];
					if ([key_str compare:@GCORE_OPTION_PID] == NSOrderedSame) {
						target_pid = stringfy_argument;
						continue;
					}
				} else if (!strncmp(option_declaration->type_name_for_argument, STR_NSSTRING, strlen(STR_NSSTRING))) {
					// Value is integer, matches to dictionary value?
					if (![user_value isKindOfClass:[NSString class]]) {
						return EINVAL;
					}
					stringfy_argument =  ((NSString *)user_value);
				}
			}
			NSString *option_name = [NSString stringWithUTF8String: option_declaration->command_line_option];
			[options_processed addObject: option_name];
			if (stringfy_argument != NULL) {
				[options_processed addObject: stringfy_argument];
			}
		}
        // If there is a debug level, it has to be processed specially
        while (debug_log_level) {
            [options_processed addObject: @GCORE_CLI_OPTION_DEBUG];
            debug_log_level--;
        }
        
		// If there is a pid, it has to be inserted at end
		if (target_pid != NULL) {
			[options_processed addObject: target_pid];
		}
		// Now we have to convert all the options and parameters to an array of char *
		char ** argv = malloc(sizeof(char *) * ([options_processed count] + 2));
            // +1 because argv[0] is the executable name +2 for NULL ptr at end
		if (argv == NULL) {
			return ENOMEM;
		}
		argv[0] = strdup("gcore");
		for (NSUInteger j = 1; j <= [options_processed count]; j++) {
			argv[j] = strdup( [options_processed[j - 1] UTF8String]);
		}
		argv[[options_processed count] + 1] = NULL;
		ret_value = spawn_gcore((int)[options_processed count] + 1, argv,
            process_port, file_descriptor_core, need_stdout);
		// free elements
		for (NSUInteger j = 0; j < [options_processed count] + 2; j++) {
			free(argv[j]);
		}
        free(argv);
	}
	return ret_value;
}
