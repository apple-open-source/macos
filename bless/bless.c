/*
 * Copyright (c) 2001-2009 Apple Inc. All Rights Reserved.
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
 *  Copyright (c) 2001-2007 Apple Inc. All Rights Reserved.
 *
 *  $Id: bless.c,v 1.85 2006/07/17 22:19:05 ssen Exp $
 *
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>
#include <getopt.h>
#include <err.h>
#include <sys/stat.h>
#include <sys/mount.h>
#include <sys/types.h>
#include <sys/sysctl.h>

#include "enums.h"
#include "structs.h"

#include "bless.h"
#include "bless_private.h"
#include "protos.h"

struct clarg actargs[klast];

/*
 * To add an option, allocate an enum in enums.h, add a getopt_long entry here,
 * add to main(), add to usage and man page
 */

/* options descriptor */
static struct option longopts[] = {
{ "alternateos",    required_argument,      0,              kalternateos},
{ "alternateOS",    required_argument,      0,              kalternateos},
{ "bootinfo",       optional_argument,      0,              kbootinfo},
{ "bootefi",		optional_argument,      0,              kbootefi},
{ "bootBlockFile",  required_argument,      0,              kbootblockfile },
{ "bootblockfile",  required_argument,      0,              kbootblockfile },
{ "booter",         required_argument,      0,              kbooter },
{ "device",         required_argument,      0,              kdevice },
{ "firmware",       required_argument,      0,              kfirmware },	
{ "file",           required_argument,      0,              kfile },
{ "folder",         required_argument,      0,              kfolder },
{ "folder9",        required_argument,      0,              kfolder9 },
{ "getBoot",        no_argument,            0,              kgetboot },
{ "getboot",        no_argument,            0,              kgetboot },
{ "help",           no_argument,            0,              khelp },
{ "info",           optional_argument,      0,              kinfo },
{ "kernel",         required_argument,      0,              kkernel },
{ "kernelcache",    required_argument,      0,              kkernelcache },
{ "label",          required_argument,      0,              klabel },
{ "labelfile",      required_argument,      0,              klabelfile },
{ "legacy",         no_argument,            0,              klegacy },
{ "legacydrivehint",required_argument,      0,              klegacydrivehint },
{ "mkext",          required_argument,      0,              kmkext },
{ "mount",          required_argument,      0,              kmount },
{ "netboot",        no_argument,            0,              knetboot},
{ "nextonly",       no_argument,            0,              knextonly},
{ "openfolder",     required_argument,      0,              kopenfolder },
{ "options",        required_argument,      0,              koptions },
{ "payload",        required_argument,      0,              kpayload },
{ "plist",          no_argument,            0,              kplist },
{ "quiet",          no_argument,            0,              kquiet },
{ "recovery",		no_argument,            0,              krecovery },
{ "reset",          no_argument,            0,              kreset },
{ "save9",          no_argument,            0,              ksave9 },
{ "saveX",          no_argument,            0,              ksaveX },
{ "server",         required_argument,      0,              kserver },
{ "setBoot",        no_argument,            0,              ksetboot },
{ "setboot",        no_argument,            0,              ksetboot },
{ "setOF",          no_argument,            0,              ksetboot }, // legacy option name
{ "shortform",      no_argument,            0,              kshortform },
{ "startupfile",    required_argument,      0,              kstartupfile },
{ "unbless",        required_argument,      0,              kunbless },
{ "use9",           no_argument,            0,              kuse9 },
{ "verbose",        no_argument,            0,              kverbose },
{ "version",        no_argument,            0,              kversion },
{ 0,            0,                      0,              0 }
};

extern char *optarg;
extern int optind;

int main (int argc, char * argv[])
{

    int ch, longindex;
    BLContext context;
    struct blesscon bcon;
    extern double blessVersionNumber;

    bcon.quiet = 0;
    bcon.verbose = 0;

    context.version = 0;
    context.logstring = blesslog;
    context.logrefcon = &bcon;

    if(argc == 1) {
        usage_short();
    }
    
    if(getenv("BL_PRINT_ARGUMENTS")) {
        int i;
        for(i=0; i < argc; i++) {
            fprintf(stderr, "argv[%d] = '%s'\n", i, argv[i]);
        }
    }
    

    
    while ((ch = getopt_long_only(argc, argv, "", longopts, &longindex)) != -1) {
        
        switch(ch) {
            case khelp:
                usage();
                break;
            case kquiet:
                break;
            case kverbose:
                bcon.verbose = 1;
                break;
            case kversion:
                printf("%.1f\n", blessVersionNumber);
                exit(0);
                break;
            case kpayload:
                actargs[ch].present = 1;
                break;
            case ksave9:
                // ignore, this is now always saved as alternateos
                break;
			case kbootblockfile:
				errx(1, "The bootblockfile option is now obsolete.");
				break;
            case '?':
            case ':':
                usage_short();
                break;
            default:
                // common handling for all other options
            {
                struct option *opt = &longopts[longindex];
                
                
                if(actargs[ch].present) {
                    warnx("Option \"%s\" already specified", opt->name);
                    usage_short();
                    break;
                } else {
                    actargs[ch].present = 1;
                }
                
                switch(opt->has_arg) {
                    case no_argument:
                        actargs[ch].hasArg = 0;
                        break;
                    case required_argument:
                        actargs[ch].hasArg = 1;
                        strlcpy(actargs[ch].argument, optarg, sizeof(actargs[ch].argument));
                        break;
                    case optional_argument:
                        if(argv[optind] && argv[optind][0] != '-') {
                            actargs[ch].hasArg = 1;
                            strlcpy(actargs[ch].argument, argv[optind], sizeof(actargs[ch].argument));
                        } else {
                            actargs[ch].hasArg = 0;
                        }
                        break;
                }
            }
                break;
        }
    }

    argc -= optind;
    argc += optind;
    
    /* There are 5 public modes of execution: info, device, folder, netboot, unbless
     * There is 1 private mode: firmware
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
	
	if(actargs[knetboot].present) {
		return modeNetboot(&context, actargs);
	}
	
	if (actargs[kunbless].present) {
		return modeUnbless(&context, actargs);
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

