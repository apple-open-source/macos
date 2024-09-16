/*
 * Copyright (c) 2024 Apple Inc.  All rights reserved.
 */

#import <Foundation/Foundation.h>
#include <os/log.h>
#include "options.h"
#include "utils.h"
#include "corefile.h"
#include "sparse.h"
#include "convert.h"
#include "GCore.h"
#include <sys/types.h>
#include <sys/sysctl.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <libproc.h>
#include <sys/kauth.h>
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
#include <assert.h>
#include <libutil.h>
#include <spawn.h>
#include <mach/mach.h>
#include <errno.h>
#include <spawn_private.h>
#include <crt_externs.h>


#define GCORE_CLI_OPTION_CORPSIFY      "-C"
#define GCORE_CLI_OPTION_SUSPEND       "-s"
#define GCORE_CLI_OPTION_PRESERVE      "-F"
#define GCORE_CLI_OPTION_VERBOSE       "-v"
#ifdef CONFIG_DEBUG
#define GCORE_CLI_OPTION_DEBUG         "-d"
#endif
#define GCORE_CLI_OPTION_EXTENDED      "-x"
#define GCORE_CLI_OPTION_MAX_SIZE      "-b"
#define GCORE_CLI_OPTION_NCACHE_THR    "-t"
#define GCORE_CLI_OPTION_ALL_FILEREFS  "-F"
#define GCORE_CLI_OPTION_GZIP          "-g"
#define GCORE_CLI_OPTION_STREAM        "-S"
#define GCORE_CLI_OPTION_ANNOTATIONS   "-N"
#define GCORE_CLI_OPTION_TASK_PORT     ""         /* No option created, just used in internal processing */
#define GCORE_CLI_OPTION_OUT_FILENAME  "-o"
#define GCORE_CLI_OPTION_PID           ""
#define GCORE_CLI_OPTION_FD            "-f"
#define GCORE_CLI_PORT_IS_CORPSE       "-e"

#define STR_NSINTEGER "NSINTEGER"
#define STR_NSSTRING  "NSSTRING"

#define LOG_SUBSYSTEM "com.apple.system_cmds"
#define LOG_CATEGORY  "gcore_framework"

#define os_assert_zero(X) {      \
    int assert_ret; \
    if ((assert_ret = (X)) != 0 ) \
    { \
        os_log_error(logger, "Expr " #X " failed with result %d",assert_ret); \
    } \
}


typedef struct options_types_t {
    const char *dictionary_option_name;
    const char *command_line_option;
    const char *type_name_for_argument;
} options_types;

static options_types parsing_options[] = {
    { .dictionary_option_name = GCORE_OPTION_CORPSIFY,       .command_line_option = GCORE_CLI_OPTION_CORPSIFY     , .type_name_for_argument = NULL          },
    { .dictionary_option_name = GCORE_OPTION_VERBOSE,        .command_line_option = GCORE_CLI_OPTION_VERBOSE      , .type_name_for_argument = NULL          },
#ifdef CONFIG_DEBUG
    { .dictionary_option_name = GCORE_OPTION_DEBUG,          .command_line_option = GCORE_CLI_OPTION_DEBUG        , .type_name_for_argument = STR_NSINTEGER },
#endif
    { .dictionary_option_name = GCORE_OPTION_ANNOTATIONS,    .command_line_option = GCORE_CLI_OPTION_ANNOTATIONS  , .type_name_for_argument = NULL          },
    { .dictionary_option_name = GCORE_OPTION_TASK_PORT,      .command_line_option = GCORE_CLI_OPTION_TASK_PORT    , .type_name_for_argument = NULL          },
    { .dictionary_option_name = GCORE_OPTION_OUT_FILENAME,   .command_line_option = GCORE_CLI_OPTION_OUT_FILENAME , .type_name_for_argument = STR_NSSTRING  },
    { .dictionary_option_name = GCORE_OPTION_PID,            .command_line_option = GCORE_CLI_OPTION_PID          , .type_name_for_argument = STR_NSINTEGER },
    { .dictionary_option_name = GCORE_OPTION_FD,             .command_line_option = GCORE_CLI_OPTION_FD           , .type_name_for_argument = STR_NSINTEGER },

};

/* Logger to be used along the operation. */
static os_log_t logger;

#pragma mark "Static functions declarations"

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
 * If the value cannot be found or is invalid returns false, else (when the value can be obtained) returns true.
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
/**
 * Spawn to create a process for handling the gcore generation
 */
static int
spawn_gcore(int argc, char **argv, mach_port_t corpse_mach_port)
{
    int retValue = 0;
	pid_t spawn_pid;
	int status = EINVAL;
	int ret_value = 0;
	if (argc < 2) { /* [0] executable, at least one arg (pid or port) */
		return status;
	}

    if (MACH_PORT_VALID(corpse_mach_port)) {
		ret_value = mach_ports_register(mach_task_self(), (mach_port_t[]){corpse_mach_port}, 1);
		if (ret_value != 0) {
			os_log_error(logger, "Cannot register corpse port err %d", ret_value);
			return ret_value;
		}
	}
    posix_spawnattr_t spawnattr;
    posix_spawnattr_init(&spawnattr);
    
    os_assert_zero(posix_spawnattr_setflags(&spawnattr, POSIX_SPAWN_SETPGROUP));
    os_assert_zero(posix_spawnattr_setflags(&spawnattr, POSIX_SPAWN_CLOEXEC_DEFAULT));
    os_assert_zero(posix_spawnattr_setflags(&spawnattr, POSIX_SPAWN_SETSIGDEF | POSIX_SPAWN_SETSIGMASK));

    sigset_t all_signals;
    sigset_t no_signals;
    os_assert_zero(sigfillset(&all_signals));
    os_assert_zero(sigemptyset(&no_signals));

    os_assert_zero(posix_spawnattr_setsigdefault(&spawnattr, &all_signals));
    os_assert_zero(posix_spawnattr_setsigmask(&spawnattr, &no_signals));

    
    status = posix_spawnp(&spawn_pid, argv[0], NULL, &spawnattr, argv, *_NSGetEnviron());
    posix_spawnattr_destroy(&spawnattr);

	if (status != 0) {
		os_log_error(logger, "Error spawning \"%s\"",strerror(errno));
		return status;
	} else {
        int wait_ret_value;
        os_log_debug(logger, "gCore PID %d",spawn_pid);
        do {
            os_log_debug(logger, "entering in waitpid");
            errno = 0;
           struct rusage usage;
            wait_ret_value = wait4(spawn_pid, &status, 0, &usage);
            os_log_debug(logger, "leaving waitpid status=%d wait_ret_value=%d WEXITSTATUS(%d)  errno %d (\"%s\")\n", 
                         status,wait_ret_value,status,WEXITSTATUS(status),strerror(status),errno,strerror(errno));
            if (wait_ret_value == -1) {
                if (errno == 0) {
                    os_log_error(logger, "Wait finished with result %d (\"%s\") child_ret_value %d \"%s\" errno %d (\"%s\") but no error, trying again",
                                 wait_ret_value,strerror(wait_ret_value),status,strerror(status),errno,strerror(errno));
                    continue;
                }
                retValue = wait_ret_value;
                os_log_error(logger, "Wait finished with result %d (\"%s\") child_ret_value %d \"%s\" errno %d (\"%s\")",
                             wait_ret_value,strerror(wait_ret_value),status,strerror(status),errno,strerror(errno));
                break;
            }
            if (WIFEXITED(status)) {
                retValue = WEXITSTATUS(status);
                os_log_debug(logger, "gcore exited, status=%d wait_ret_value=%d WEXITSTATUS(%d)\n", 
                             status,wait_ret_value,status,WEXITSTATUS(status));
             } else if (WIFSIGNALED(status)) {
                 retValue = EINTR;
                 os_log_error(logger, "gcore killed by signal %d\n", WTERMSIG(status));
             } else if (WIFSTOPPED(status)) {
                 os_log_error(logger, "gcore stopped by signal %d\n", WSTOPSIG(status));
                 printf("stopped by signal %d\n", WSTOPSIG(status));
             } else if (WIFCONTINUED(status)) {
                 printf("continued\n");
             }
        } while (!WIFEXITED(status) && !WIFSIGNALED(status));
	}

	return retValue;
}
/**
 * Create a coredump with the selected options, return 0 if the coredump was properly created or an error code
 */
__attribute__((visibility("default"))) int
create_gcore_with_options(NSDictionary *options)
{
	int ret_value = EPERM;
	int file_descriptor_core = -1;
    int debug_log_level = 0;
	mach_port_t process_port = MACH_PORT_NULL;
	NSString *target_pid = NULL;
	optind = 1;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        logger = os_log_create(LOG_SUBSYSTEM, LOG_CATEGORY);
    });
    
	@autoreleasepool {
		NSMutableArray *options_processed = [[NSMutableArray alloc] init];
		// Iterate all the user options and convert them to gcore options
		// To print out all key-value pairs in the NSDictionary myDict
		for (id key in options) {
			NSString *stringfy_argument = NULL;
			if (![key isKindOfClass:[NSString class]]) {
				return EINVAL; // Invalid argument
			}
			NSString *key_str = (NSString *)key;
#ifdef CONFIG_DEBUG
            // If the parameter is a debug, we need to intercept the log level
            if ([key_str compare:@GCORE_OPTION_DEBUG] == NSOrderedSame) {
                int error_code;
                NSNumber *number_param;
                if (get_integer_value(key, options, &number_param, &error_code) == false) {
                    return error_code;
                }
                debug_log_level = [number_param intValue];
                continue;
            }
#endif
			// If the parameter is a FD, we need to intercept it
			if ([key_str compare:@GCORE_OPTION_FD] == NSOrderedSame) {
				int error_code;
				NSNumber *number_param;
				if (get_integer_value(key, options, &number_param, &error_code) == false) {
					return error_code;
				}
				file_descriptor_core = [number_param intValue];
			}
			// If the parameter is a mach port, we need to intercept it
			if ([key_str compare:@GCORE_OPTION_TASK_PORT] == NSOrderedSame) {
				int error_code;
				NSNumber *number_param;

				if (get_integer_value(key, options, &number_param, &error_code) == false) {
					return error_code;
				}
				process_port = [number_param intValue];
                continue;    // We do not want to add a command line parameter to gcore, just to share the port.
			}
			options_types *option_declaration = get_option_for_name(key_str);
			if (option_declaration == NULL) {
				return EDOM;
			}
			// Does the current option require a value?
			if (option_declaration->type_name_for_argument != NULL) {
				id user_value = options[key];
				if (user_value == NULL) {
					return ERANGE;
				}
				if (!strncmp(option_declaration->type_name_for_argument, STR_NSINTEGER, strlen(STR_NSINTEGER))) {
					int error_code;
					NSNumber *number_param;
					if (get_integer_value(key, options, &number_param, &error_code) == false) {
						return error_code;
					}

					stringfy_argument =  [number_param stringValue];
					if ([key_str compare:@GCORE_OPTION_PID] == NSOrderedSame) {
						target_pid = stringfy_argument;
						continue;
					}
				} else if (!strncmp(option_declaration->type_name_for_argument, STR_NSSTRING, strlen(STR_NSSTRING))) {
					// Value is intenger, matches to dictionary value?
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
        // If there is a log level, it have to be process on a special manner
        while (debug_log_level)
        {
            [options_processed addObject: @GCORE_CLI_OPTION_DEBUG];
            debug_log_level--;
        }
        
		// If there is a pid, it has to be inserted at end
		if (target_pid != NULL) {
			[options_processed addObject: target_pid];
		}
		// Now we have to convert all the options and parameters to an array of char *
		char ** argv = malloc(sizeof(char *) * ([options_processed count] + 2)); // +1 because argv[0] is the executable name +2 for NULL ptr at end
		if (argv == NULL) {
			return ENOMEM;
		}
		argv[0] = strdup("gcore");
		for (NSUInteger j = 1; j <= [options_processed count]; j++) {
			argv[j] = strdup( [options_processed[j - 1] UTF8String]);
		}
		argv[[options_processed count] + 1] = NULL;
		ret_value = spawn_gcore((int)[options_processed count] + 1, argv, process_port);
		// free elements
		for (NSUInteger j = 0; j < [options_processed count] + 2; j++) {
			free(argv[j]);
		}
	}
	return ret_value;
}
