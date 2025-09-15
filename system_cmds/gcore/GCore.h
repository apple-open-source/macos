/*
 * Copyright (c) 2024 Apple Inc.  All rights reserved.
 */

#import <Foundation/Foundation.h>

/**
 * Definitions for gcore corefile generation.
 *
 * Flags which are undocumented on the gcore(1) manpage may be highly
 * experimental, are SPI and are subject to change or removal, and may
 * not function as expected in all (or any) environments.
 *
 * Correspondence between keys recognized by the framework and the gcore CLI:
 * \code
 * SPI ARGUMENT                CLI Counterpart
 * ------------                ---------------
 * GCORE_OPTION_VERBOSE        -v       Add log information to stdout
 * GCORE_OPTION_SUSPEND        -s       Suspend target while dumping core
 * GCORE_OPTION_OUT_FILENAME   -o pathname
 * GCORE_OPTION_PATHFORMAT     -c pathformat
 * GCORE_OPTION_MAX_SIZE       -b MiB   Maximum size of core file
 * GCORE_OPTION_PID            ""       PID of process to create a gcore
 * GCORE_OPTION_CONTENT        -x       stack, compact or full content
 *
 * GCORE_OPTION_QUIET          -q       Send errors only to the log
 * GCORE_OPTION_CORPSIFY       -C       Create a corpse for core file generation
 * GCORE_OPTION_ANNOTATIONS    -N       Add memory annotations
 * GCORE_OPTION_TASK_PORT               (No equivalent)
 * GCORE_OPTION_FD             -f       write corefile to file descriptor
 * GCORE_OPTION_DEBUG          -d       Add debug information to stdout (cannot
 *                                      use more than once on framework, has
 *                                      integer parameter with the debug level)
 *\endcode
 */

#define GCORE_OPTION_VERBOSE        "verbose"
#define GCORE_OPTION_SUSPEND        "suspend"
#define GCORE_OPTION_OUT_FILENAME   "filename"
#define GCORE_OPTION_PATHFORMAT     "pathformat"
#define GCORE_OPTION_MAX_SIZE       "maxsize"
#define GCORE_OPTION_PID            "pid"
#define GCORE_OPTION_CONTENT        "content"

#define GCORE_OPTION_QUIET          "quiet"
#define GCORE_OPTION_CORPSIFY       "corpsify"
#define GCORE_OPTION_ANNOTATIONS    "annotations"
#define GCORE_OPTION_TASK_PORT      "port"
#define GCORE_OPTION_FD             "filedesc"
#define GCORE_OPTION_DEBUG          "debug"

/**
 * Create a coredump with the selected options.
 * Returns 0 if the coredump was properly created, or an error code.
 *
 * Possible errors include:
 *   EINVAL:    An argument or a key is mangled or the wrong type
 *              The combination of arguments not supported by gcore
 *   EDOM:      An option is not recognized
 *   ERANGE:    An argument should have a parameter but is not
 *              found, or is the wrong type
 *   ENOMEM:    Insufficient memory
 *   EPERM:     Insufficient privilege for the gcore operation
 *   EACCES:    gcore could not create the core file
 *   EAGAIN:    Temporary shortage of system resources
 *   EIO:       I/O error while writing the core file
 *   ENODEV:    Bad or incomplete configuration
 */
extern int create_gcore_with_options(NSDictionary *options);
