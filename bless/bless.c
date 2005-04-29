/*
 * Copyright (c) 2001-2005 Apple Computer, Inc. All rights reserved.
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
/*
 *  bless.c
 *  bless
 *
 *  Created by Shantonu Sen <ssen@apple.com> on Wed Nov 14 2001.
 *  Copyright (c) 2001-2005 Apple Computer, Inc. All rights reserved.
 *
 *  $Id: bless.c,v 1.69 2005/02/23 21:42:26 ssen Exp $
 *
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>
#include <getopt.h>
#include <err.h>

#include "enums.h"
#include "structs.h"

#include "bless.h"

struct clarg actargs[klast];

/* options descriptor */
static struct option longopts[] = {
{ "bootinfo",       optional_argument,      0,              kbootinfo},
{ "bootBlockFile",  required_argument,      0,              kbootblockfile },
{ "device",         required_argument,      0,              kdevice },
{ "firmware",       required_argument,      0,              kfirmware },	
{ "folder",         required_argument,      0,              kfolder },
{ "folder9",        required_argument,      0,              kfolder9 },
{ "getBoot",        no_argument,            0,              kgetboot },
{ "help",           no_argument,            0,              khelp },
{ "info",           optional_argument,      0,              kinfo },
{ "label",          required_argument,      0,              klabel },
{ "labelfile",      required_argument,      0,              klabelfile },
{ "mount",          required_argument,      0,              kmount },
{ "openfolder",     required_argument,      0,              kopenfolder },
{ "plist",          no_argument,            0,              kplist },
{ "quiet",          no_argument,            0,              kquiet },
{ "save9",          no_argument,            0,              ksave9 },
{ "saveX",          no_argument,            0,              ksaveX },
{ "setBoot",        no_argument,            0,              ksetboot },
{ "setOF",          no_argument,            0,              ksetOF },
{ "startupfile",    required_argument,      0,              kstartupfile },
{ "use9",           no_argument,            0,              kuse9 },
{ "verbose",        no_argument,            0,              kverbose },
{ "version",        no_argument,            0,              kversion },
{ 0,            0,                      0,              0 }
};

extern char *optarg;
extern int optind;


int modeInfo(BLContextPtr context, struct clarg actargs[klast]);
int modeDevice(BLContextPtr context, struct clarg actargs[klast]);
int modeFolder(BLContextPtr context, struct clarg actargs[klast]);
int modeFirmware(BLContextPtr context, struct clarg actargs[klast]);

int blesslog(void *context, int loglevel, const char *string);
extern void usage();
extern void usage_short();
static char ** canon_argv(int argc, char *argv[]);

void arg_err(char *message, char *opt);

int main (int argc, char * argv[])
{

    int ch;
    char **newargv = NULL;
    
    BLContext context;
    struct blesscon bcon;

    bcon.quiet = 0;
    bcon.verbose = 0;

    context.version = 0;
    context.logstring = blesslog;
    context.logrefcon = &bcon;

    if(argc == 1) {
        arg_err(NULL, NULL);
    }
    
    newargv = canon_argv(argc, argv);

    while ((ch = getopt_long(argc, newargv, "", longopts, NULL)) != -1) {
        if(ch == ksetOF) ch = ksetboot; // remap here
        
        switch(ch) {
            case khelp:
                usage();
                break;
            case kquiet:
                break;
            case kverbose:
                bcon.verbose = 1;
                break;
                
            // common handling for all other options
            case kbootinfo:
            case kbootblockfile:
            case kdevice:
			case kfirmware:
            case kfolder:
            case kfolder9:
            case kgetboot:
            case kinfo:
            case klabel:
            case klabelfile:
            case kmount:
            case kopenfolder:
            case kplist:
            case ksave9:
            case ksaveX:
            case ksetboot:
            case kstartupfile:
            case kuse9:
            case kversion:
                if(actargs[ch].present) {
                    warnx("Option \"%s\" already specified", longopts[ch-1].name);
                    usage_short();
                    break;
                } else {
                    actargs[ch].present = 1;
                }
                
                switch(longopts[ch-1].has_arg) {
                    case no_argument:
                        actargs[ch].hasArg = 0;
                        break;
                    case required_argument:
                        actargs[ch].hasArg = 1;
                        strcpy(actargs[ch].argument, optarg);
                        break;
                    case optional_argument:
                        if(newargv[optind] && newargv[optind][0] != '-') {
                            actargs[ch].hasArg = 1;
                            strcpy(actargs[ch].argument, newargv[optind]);
                        } else {
                            actargs[ch].hasArg = 0;
                        }
                        break;
                }
                break;
            case '?':
            default:
                usage_short();
        }
    }

    argc -= optind;
    argc += optind;
    
    if(actargs[kversion].present) {
        extern double blessVersionNumber;
        printf("%.1f\n", blessVersionNumber);
        return 0;
    }

    /* There are three modes of execution: info, device, folder.
     * These are all one-way function jumps.
     */

    /* If it was requested, print out the Finder Info words */
    if(actargs[kinfo].present || actargs[kgetboot].present) {
        return modeInfo(&context, actargs);
    }

    if(actargs[kdevice].present) {
        return modeDevice(&context, actargs);
    }

	if(actargs[kfirmware].present) {
		return modeFirmware(&context, actargs);
	}
	
    /* default */
    return modeFolder(&context, actargs);

}



int blesscontextprintf(BLContextPtr context, int loglevel, char const *fmt, ...) {
    int ret;
    char *out;
    va_list ap;

    va_start(ap, fmt);
    ret = vasprintf(&out, fmt, ap);  
    va_end(ap);
    
    if((ret == -1) || (out == NULL)) {
        return context->logstring(context->logrefcon, loglevel, "Memory error, log entry not available");
    }

    ret = context->logstring(context->logrefcon, loglevel, out);
    free(out);
    return ret;
}

void arg_err(char *message, char *opt) {
    if(!(message == NULL && opt == NULL))
	fprintf(stderr, "%s \"%s\"\n", message, opt);

    usage_short();
}

static char ** canon_argv(int argc, char *argv[])
{
    char ** newargv = NULL;
    int i;
    
    newargv = calloc(argc+1, sizeof(char *));
    for(i=0; i < argc; i++) {
        char *newarg = NULL;
        if(argv[i][0] == '-' && argv[i][1] != '-') {
            // turn -foo into --foo, for getopt_long
            newarg = malloc(strlen(argv[i]) + 1 + 1);
            sprintf(newarg, "-%s", argv[i]);
        } else {
            newarg = strdup(argv[i]);
        }
        newargv[i] = newarg;
    }
    
    return newargv;
}
