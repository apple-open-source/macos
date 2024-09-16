/*
 * Copyright (c) 2024 Apple Inc.  All rights reserved.
 */


#import <Foundation/Foundation.h>

/**
 * Different definitions for the gcore generation.
 * There is a correspondence between the arguments to the framework and the command line:
 * \code
 *    SPI ARGUMENT               CLI Counterpart
 * --------------------------  ----------------------
 *   GCORE_OPTION_CORPSIFY        -C                                    Create a corpse for core file generation
 *   GCORE_OPTION_VERBOSE         -v                                    Add log information to stdout
 *   GCORE_OPTION_DEBUG           -d                                    Add debug information to stdout (cannot use more than once on framework), have a parameter (integer) with the debug level
 *   GCORE_OPTION_ANNOTATIONS     -N
 *   GCORE_OPTION_OUT_FILENAME    -o
 *   GCORE_OPTION_PID             ""                                    PID of process to create a gcore
 *   GCORE_OPTION_FD              "-f"                                  Use a file rather than a filename handle to perform file IO operations
 *\endcode
 * 
 */
#define GCORE_OPTION_CORPSIFY       "corpsify"
#define GCORE_OPTION_SUSPEND        "suspend"
#define GCORE_OPTION_VERBOSE        "verbose"
#ifdef CONFIG_DEBUG
#define GCORE_OPTION_DEBUG          "debug"
#endif
#define GCORE_OPTION_ANNOTATIONS    "annotations"
#define GCORE_OPTION_TASK_PORT      "port"
#define GCORE_OPTION_OUT_FILENAME   "filename"
#define GCORE_OPTION_PID            "pid"
#define GCORE_OPTION_FD             "filedesc"
/**
 * Create a coredump with the selected options, return 0 if the coredump was properly created or an error code.
 *
 * Possible errors are:
 *   EINVAL: One argument or a key is not a NSString, cannot process the value.
 *   EDOM:   One option is not recognized.
 *   ERANGE: One argument should have a parameter but is not found, or the data type is not a NSNumber
 *   ENOMEM: There was not enought memory for internal allocations
 *
 */
int create_gcore_with_options(NSDictionary *options);

