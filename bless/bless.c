/*
 * Copyright (c) 2001-2020 Apple Inc. All Rights Reserved.
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
#include <paths.h>
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
{ "apfsdriver",     required_argument,      0,              kapfsdriver},
{ "alternateos",    required_argument,      0,              kalternateos},
{ "alternateOS",    required_argument,      0,              kalternateos},
{ "allowUI",        no_argument,            0,              kallowui},
{ "bootinfo",       optional_argument,      0,              kbootinfo},
{ "bootefi",        optional_argument,      0,              kbootefi},
{ "bootBlockFile",  required_argument,      0,              kbootblockfile },
{ "bootblockfile",  required_argument,      0,              kbootblockfile },
{ "booter",         required_argument,      0,              kbooter },
{ "create-snapshot",no_argument,            0,              kcreatesnapshot },
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
{ "last-sealed-snapshot",no_argument,       0,              klastsealedsnapshot },
{ "legacy",         no_argument,            0,              klegacy },
{ "legacydrivehint",required_argument,      0,              klegacydrivehint },
{ "mkext",          required_argument,      0,              kmkext },
{ "mount",          required_argument,      0,              kmount },
{ "netboot",        no_argument,            0,              knetboot},
{ "nextonly",       no_argument,            0,              knextonly},
{ "noapfsdriver",   no_argument,            0,              knoapfsdriver},
{ "openfolder",     required_argument,      0,              kopenfolder },
{ "options",        required_argument,      0,              koptions },
{ "passpromt",      no_argument,            0,              kpasspromt },
{ "payload",        required_argument,      0,              kpayload },
{ "personalize",    no_argument,            0,              kpersonalize },
{ "plist",          no_argument,            0,              kplist },
{ "quiet",          no_argument,            0,              kquiet },
{ "recovery",        no_argument,           0,              krecovery },
{ "reset",          no_argument,            0,              kreset },
{ "save9",          no_argument,            0,              ksave9 },
{ "saveX",          no_argument,            0,              ksaveX },
{ "server",         required_argument,      0,              kserver },
{ "setBoot",        no_argument,            0,              ksetboot },
{ "setboot",        no_argument,            0,              ksetboot },
{ "setOF",          no_argument,            0,              ksetboot }, // legacy option name
{ "shortform",      no_argument,            0,              kshortform },
{ "startupfile",    required_argument,      0,              kstartupfile },
{ "stdinpass",      no_argument,            0,              kstdinpass },
{ "unbless",        required_argument,      0,              kunbless },
{ "user",           required_argument,      0,              kuser },
{ "use9",           no_argument,            0,              kuse9 },
{ "use-tdm-as-external", no_argument,       0,              kusetdmasexternal },
{ "verbose",        no_argument,            0,              kverbose },
{ "version",        no_argument,            0,              kversion },
{ "snapshot",       required_argument,      0,              ksnapshot },
{ 0,            0,                      0,              0 }
};

extern char *optarg;
extern int optind;
extern char blessVersionString[];
char blessVersionNumString[64];

int main (int argc, char * argv[])
{

    int ch, longindex;
    BLContext context;
    struct blesscon bcon;

	strlcpy(blessVersionNumString, strrchr(blessVersionString, '-') + 1, sizeof blessVersionNumString);
	*strchr(blessVersionNumString, '\n') = '\0';
	
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
                printf("%s\n", blessVersionNumString);
                exit(0);
                break;
#if TARGET_CPU_ARM64
            case kapfsdriver:
                errx(1, "The 'apfsdriver' option is not supported on Apple Silicon devices.");
                break;
            case kbootinfo:
                errx(1, "The 'bootinfo' option is not supported on Apple Silicon devices.");
                break;
            case kbooter:
                errx(1, "The 'booter' option is not supported on Apple Silicon devices.");
                break;
            case kfirmware:
                errx(1, "The 'firmware' option is not supported on Apple Silicon devices.");
                break;
            case kfolder9:
                errx(1, "The 'folder9' option is not supported on Apple Silicon devices.");
                break;
            case kkernel:
                errx(1, "The 'kernel' option is not supported on Apple Silicon devices.");
                break;
            case kkernelcache:
                errx(1, "The 'kernelcache' option is not supported on Apple Silicon devices.");
                break;
            case klegacy:
            case klegacydrivehint:
                errx(1, "The 'legacy' and 'legacydriverhint' options are not supported on Apple Silicon devices.");
                break;
            case kmkext:
                errx(1, "The 'mkext' option is not supported on Apple Silicon devices.");
                break;
            case knetboot:
                errx(1, "The 'netboot' option is not supported on Apple Silicon devices.");
                break;
            case kopenfolder:
                errx(1, "The 'openfolder' is not supported on Apple Silicon devices.");
                break;
            case kpayload:
                errx(1, "The 'payload' option is not supported on Apple Silicon devices.");
                break;
            case kserver:
                errx(1, "The 'server' option is not supported on Apple Silicon devices.");
                break;
#else  /* TARGET_CPU_ARM64 */
            case kpayload:
                actargs[ch].present = 1;
                break;
#endif  /* TARGET_CPU_ARM64 */
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
#if TARGET_CPU_ARM64
    /* External TDM devices are treated as standalone devices.
     * In order to use a TDM device as an external media to boot the current Apple silicon device use
     * the '--use-tdm-as-external' option.
     * For external/removable devices which are not TDM devices the following rules apply:
     * '--folder', '--file' and '--info' are used to boot/bless and get information based on x86 architecture
     * with the intent that this external/removable device will be used for booting an x86 Mac.
     * All other options are used to boot/bless and get information based on arm64e architecture
     * with the intent that this external/removable device will be used for booting this Apple Silicon Mac.
     */
    bool tdm = false;
    bool external = false;
    bool removable = false;
    char bsdName[BSD_NAME_SIZE];
    int ret = 0;

    if (!actargs[kgetboot].present) {
        if (actargs[kinfo].present) {
            ret = BLGetCommonMountPoint(&context, actargs[kinfo].argument, "", actargs[kmount].argument);
        } else if (actargs[kunbless].present) {
            ret = BLGetCommonMountPoint(&context, actargs[kunbless].argument, "", actargs[kmount].argument);
        } else if (actargs[kfolder].present) {
            ret = extractMountPoint(&context, actargs);
        }
        if (ret) {
            return ret;
        }
        if (actargs[kdevice].present) {
            strlcpy(bsdName, actargs[kdevice].argument + strlen(_PATH_DEV), sizeof(bsdName));
        } else if ((ret = extractDiskFromMountPoint(&context, actargs[kmount].argument, bsdName, BSD_NAME_SIZE)) != 0) {
            blesscontextprintf(&context, kBLLogLevelError, "Could not extract BSD name from mount point %s\n", actargs[kmount].argument);
            return ret;
        }

        ret = isMediaTDM(&context, bsdName, &tdm);
        if (ret) {
            return ret;
        }
        ret = isMediaExternal(&context, bsdName, &external);
        if (ret) {
            return ret;
        }
        ret = isMediaRemovable(&context, bsdName, &removable);
        if (ret) {
            return ret;
        }
    }

    /* '--setboot' and '--nextonly' relevant only for booting current internal Apple Silicon device
     * '--folder' is used to boot device based on Intel Atchitecture. Therefore those options
     * are not relevant.
     */
    if (actargs[kfolder].present) {
        if (actargs[ksetboot].present) {
            errx(1, "For Apple Silicon Macs, 'setboot' option is not supported in Folder mode");
        }
        if (actargs[knextonly].present) {
            errx(1, "For Apple Silicon Macs, 'nextonly' option is not supported in Folder mode");
        }
    }

    /* Handle Info mode (--info, --getBoot):
     * TDM is also considered an external devices
     */
    if (actargs[kgetboot].present ||
        (actargs[kinfo].present && external) ||
        (actargs[kinfo].present && removable)) {
        /* If it was requested, print out the Finder Info words */
        return modeInfo(&context, actargs);
    } else if (actargs[kinfo].present) {
        errx(1, "For Apple Silicon Macs, the 'info' option is only supported for external devices.");
    }

    /* Handle TDM devices (Device mode, Unbless mode, File/Folder mode, Mount mode,
     * Info mode is handled above):
     */
    if (tdm && !actargs[kusetdmasexternal].present) {
        if (actargs[kdevice].present) {
            return modeDevice(&context, actargs);
        }
        if (actargs[kunbless].present) {
            return modeUnbless(&context, actargs);
        }
        return modeFolder(&context, actargs);
    }

    /* Handle external and removable devices not in TDM (Folder mode): */
    if (!tdm && (external || removable)) {
        if (actargs[kfolder].present) {
            return modeFolder(&context, actargs);
        }
    }

    if (actargs[kunbless].present) {
        errx(1, "For Apple Silicon Macs, the 'unbless' option is only supported for Intel architecture based devices in TDM");
    }
    if (actargs[kfolder].present) {
        errx(1, "For Apple Silicon Macs, the 'folder' option is only supported for external devices");
    }
    if (actargs[kfile].present) {
        errx(1, "For Apple Silicon Macs, the 'file' option is supported for external devices in Folder mode only");
    }

    /* Handle AS devices (Device mode, Mount mode,
     * Info mode is handled above):
     */
    return blessViaBootability(&context, actargs);

#else /* TARGET_CPU_ARM64 */
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

#endif /* TARGET_CPU_ARM64 */

}



// note that libbless has its own (similar) contextprintf()
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

