/*
 * Copyright (c) 2017 Apple Inc. All Rights Reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 *
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 *
 * @APPLE_LICENSE_HEADER_END@
 */

#define __STDC_WANT_LIB_EXT1__ 1
#include <string.h>

#include "SecArgParse.h"

// Trinary:
//  If flag is set and argument is not, it has no_argument
//  If flag is set and argument is set, it is optional_argument
//  If flag is not set and argument is set, it is required_argument
//  If flag is not set and argument is not set, what are you doing? There's no output.

static int argument_status(struct argument* option) {
    if(option == NULL) {
        return no_argument;
    }
    return (!!(option->flag) &&  !(option->argument) ? no_argument :
            (!!(option->flag) && !!(option->argument) ? optional_argument :
             ( !(option->flag) && !!(option->argument) ? required_argument :
              no_argument)));
}

static bool fill_long_option_array(struct argument* options, size_t noptions, struct option long_options[], size_t nloptions) {
    size_t i = 0;
    size_t longi = 0;

    for(i = 0; i <= noptions; i++) {
        if(longi >= nloptions) {
            return false;
        }

        if(options[i].longname) {
            long_options[longi].name = options[i].longname;
            long_options[longi].has_arg = argument_status(&options[i]);
            long_options[longi].flag = options[i].flag;
            long_options[longi].val = options[i].flagval;
            longi++;
        }
    }

    if(longi >= nloptions) {
        return false;
    }

    long_options[longi].name = NULL;
    long_options[longi].has_arg = 0;
    long_options[longi].flag = 0;
    long_options[longi].val = 0;

    return true;
}

static bool fill_short_option_array(struct argument* options, size_t noptions, char* short_options, size_t nshort_options) {
    size_t index = 0;
    for(size_t i = 0; i < noptions; i++) {
        if(options[i].shortname != '\0') {
            if(index >= nshort_options) {
                return false;
            }
            short_options[index] = options[i].shortname;
            index += 1;

            if(argument_status(&options[i]) == required_argument) {
                if(index >= nshort_options) {
                    return false;
                }
                short_options[index] = ':';
                index += 1;
            }
        }
    }
    short_options[index] = '\0';
    return true;
}

static void trigger(struct argument option, char* argument) {
    if(option.flag) {
        *(option.flag) = option.flagval;
    }
    if(option.argument) {
        asprintf(option.argument, "%.1048576s", argument);
    }
}

static size_t num_arguments(struct arguments* args) {
    size_t n = 0;

    if(!args) {
        return 0;
    }

    struct argument* a = args->arguments;
    // Make an all-zero struct
    struct argument final = {};

    // Only 1024 arguments allowed.
    while(a && n < 1024 && (memcmp(a, &final, sizeof(struct argument)) != 0)) {
        n++;
        a++;
    }

    return n;
}

bool options_parse(int argc, char * const *argv, struct arguments* args) {
    if(!args) {
        return false;
    }
    bool success = false;

    struct arguments realargs;
    realargs.programname = args->programname;
    realargs.description = args->description;
    size_t num_args = num_arguments(args);
    size_t noptions = num_args + 1;
    realargs.arguments = (struct argument*) malloc((noptions +1) * sizeof(struct argument)); // extra array slot for null array

    struct argument help = {.shortname = 'h', .longname="help", .description="show this help message and exit"};

    realargs.arguments[0] = help;
    // Adds one to include the null terminator struct!
    for(size_t i = 0; i < num_args + 1; i++) {
        realargs.arguments[i+1] = args->arguments[i];
    }

    struct option* long_options = (struct option*) calloc((noptions+1), sizeof(struct option));
    size_t short_options_length = 2* noptions * sizeof(char) + 2; // 2: one for -h, one for the null terminator
    char* short_options = (char*) malloc(short_options_length);

    fill_long_option_array(realargs.arguments, noptions, long_options, noptions);
    fill_short_option_array(realargs.arguments, noptions, short_options, short_options_length);

    int c;
    int option_index = 0;
    while((c = getopt_long(argc, argv, short_options, long_options, &option_index)) != -1) {
        // We have a short arg or an arg with an argument. Parse it.
        if(c==0) {
            if(option_index == 0) {
                // This is the --help option
                print_usage(&realargs);
                exit(1);
            } else {
                struct option* long_option = &long_options[option_index];

                for(size_t i = 0; i < noptions; i++) {
                    if(realargs.arguments[i].longname && long_option->name && strncmp(long_option->name, realargs.arguments[i].longname, strlen(realargs.arguments[i].longname)) == 0) {
                        trigger(realargs.arguments[i], optarg);
                    }
                }
            }
        } else {
            // Handle short name
            if(c == 'h') {
                // This is the --help option
                print_usage(&realargs);
                exit(1);
            }
            size_t i = 0;
            for(i = 0; i < noptions; i++) {
                if(realargs.arguments[i].shortname == c) {
                    trigger(realargs.arguments[i], optarg);
                    break;
                }
            }
            if(i == noptions) {
                success = false;
                goto out;
            }
        }
    }

    if(optind < argc) {
        bool command_triggered = false;
        size_t positional_argument_index = 0;

        for(int parg = optind; parg < argc; parg++) {
            for(size_t i = 0; i < noptions; i++) {
                if(realargs.arguments[i].command) {
                    if(strcmp(argv[parg], realargs.arguments[i].command) == 0) {
                        trigger(realargs.arguments[i], NULL);
                        command_triggered = true;
                        break;
                    }
                }
            }

            if(command_triggered) {
                break;
            }

            while(positional_argument_index < noptions && !realargs.arguments[positional_argument_index].positional_name) {
                positional_argument_index++;
            }

            if(positional_argument_index >= noptions) {
                // no positional argument found to save
                // explode
                goto out;
            } else {
                if(realargs.arguments[positional_argument_index].argument) {
                    *(realargs.arguments[positional_argument_index].argument) = argv[parg];
                    positional_argument_index++;
                }

            }
        }
    }

    success = true;

out:
    free(realargs.arguments);
    free(long_options);
    free(short_options);

    return success;
}

void print_usage(struct arguments* args) {
    if(!args) {
        return;
    }
    printf("usage: %s", args->programname ? args->programname : "command");

    size_t num_args = num_arguments(args);

    // Print all short options
    for(size_t i = 0; i < num_args; i++) {
        if(args->arguments[i].shortname) {
            printf(" [-%c", args->arguments[i].shortname);

            if(argument_status(&args->arguments[i]) != no_argument) {
                printf(" %s", args->arguments[i].argname ? args->arguments[i].argname : "arg");
            }

            printf("]");
        }
    }

    // Print all long args->arguments that don't have short args->arguments
    for(size_t i = 0; i < num_args; i++) {
        if(args->arguments[i].longname && !args->arguments[i].shortname) {

            printf(" [--%s", args->arguments[i].longname);

            if(argument_status(&args->arguments[i]) != no_argument) {
                printf(" %s", args->arguments[i].argname ? args->arguments[i].argname : "arg");
            }

            printf("]");
        }
    }

    // Print all commands
    for(size_t i = 0; i < num_args; i++) {
        if(args->arguments[i].command) {
            printf(" [%s]", args->arguments[i].command);
        }
    }

    // Print all positional arguments
    for(size_t i = 0; i < num_args; i++) {
        if(args->arguments[i].positional_name) {
            if(args->arguments[i].positional_optional) {
                printf(" [<%s>]", args->arguments[i].positional_name);
            } else {
                printf(" <%s>", args->arguments[i].positional_name);
            }
        }
    }

    printf("\n");

    if(args->description) {
        printf("\n%s\n", args->description);
    }

    printf("\npositional arguments:\n");
    for(size_t i = 0; i < num_args; i++) {
        if(args->arguments[i].positional_name) {
            printf("  %-31s %s\n", args->arguments[i].positional_name, args->arguments[i].description);
        }
    }

    printf("\noptional arguments:\n");
    // List all short args->arguments
    for(size_t i = 0; i < num_args; i++) {
        if(args->arguments[i].shortname) {
            if(!args->arguments[i].longname) {

                if(argument_status(&args->arguments[i]) != no_argument) {
                    printf("  -%c %-*s", args->arguments[i].shortname, 28, "arg");
                } else {
                    printf("  -%-30c", args->arguments[i].shortname);
                }

            } else {
                printf("  -%c", args->arguments[i].shortname);

                if(argument_status(&args->arguments[i]) != no_argument) {
                    printf(" %s", args->arguments[i].argname ? args->arguments[i].argname : "arg");
                }

                if(args->arguments[i].longname) {
                    if(argument_status(&args->arguments[i]) == no_argument) {
                        printf(", --%-*s", 28 - (int) strlen(args->arguments[i].argname ? args->arguments[i].argname : "arg"), args->arguments[i].longname);
                    } else {
                        printf(", --%s %-*s",  args->arguments[i].longname, 28 -5 - (int) strlen(args->arguments[i].longname) - (int) strlen(args->arguments[i].argname ? args->arguments[i].argname : "arg"), "arg");
                    }
                }
            }

            printf("%s\n", args->arguments[i].description);
        }
    }

    // List all long args->arguments
    for(size_t i = 0; i < num_args; i++) {
        if(args->arguments[i].longname && !args->arguments[i].shortname) {
            if(argument_status(&args->arguments[i]) != no_argument) {
                printf("  --%s %-*s %s\n",
                       args->arguments[i].longname,
                       28 - (int) strlen(args->arguments[i].longname),
                       args->arguments[i].argname ? args->arguments[i].argname : "arg",
                       args->arguments[i].description);
            } else {
                printf("  --%-29s %s\n", args->arguments[i].longname, args->arguments[i].description);
            }
        }
    }

    printf("\noptional commands:\n");
    // Print all commands
    for(size_t i = 0; i < num_args; i++) {
        if(args->arguments[i].command) {
            printf("  %-30s %s\n", args->arguments[i].command, args->arguments[i].description);
        }
    }

    printf("\n");
}
