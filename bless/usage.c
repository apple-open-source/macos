/*
 * Copyright (c) 2001-2007 Apple Inc. All Rights Reserved.
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
 *  usage.c
 *  bless
 *
 *  Created by Shantonu Sen <ssen@apple.com> on Wed Nov 14 2001.
 *  Copyright (c) 2001-2007 Apple Inc. All Rights Reserved.
 *
 *  $Id: usage.c,v 1.31 2005/12/05 12:59:30 ssen Exp $
 *
 */

#include <stdlib.h>
#include <stdio.h>

#include "bless.h"

#include "enums.h"
#include "structs.h"
#include "protos.h"

void usage(void) {
    fprintf(stderr, "Usage: %s [options]\n", getprogname());
    fputs(
"\t--help\t\tThis usage statement\n"
"\n"
"Info Mode:\n"
"\t--info [dir]\tPrint blessing information for a specific volume, or the\n"
"\t\t\tcurrently active boot volume if <dir> is not specified\n"
"\t--getBoot\tSuppress normal output and print the active boot volume\n"
"\t--version\tPrint bless version number\n"
"\t--plist\t\tFor any output type, use a plist representation\n"
"\t--verbose\tVerbose output\n"
"\n"
"File/Folder Mode:\n"
"\t--file file\tSet <file> as the blessed boot file\n"
"\t--folder dir\tSet <dir> as the blessed directory\n"
"\t--bootinfo [file]\tUse <file> to create a \"BootX\" file in the\n"
"\t\t\tblessed dir\n"
"\t--bootefi [file]\tUse <file> to create a \"boot.efi\" file in the\n"
"\t\t\tblessed dir\n"
"\t--setBoot\tSet firmware to boot from this volume\n"
"\t--openfolder dir\tSet <dir> to be the visible Finder directory\n"
"\t--verbose\tVerbose output\n"
"\n"
"Mount Mode:\n"
"\t--mount dir\tUse this mountpoint in conjunction with --setBoot\n"
"\t--file file\tSet firmware to boot from <file>\n"
"\t--setBoot\tSet firmware to boot from this volume\n"
"\t--verbose\tVerbose output\n"
"\n"
"Device Mode:\n"
"\t--device dev\tUse this block device in conjunction with --setBoot\n"
"\t--setBoot\tSet firmware to boot from this volume\n"
"\t--verbose\tVerbose output\n"
"\n"
"NetBoot Mode:\n"
"\t--netboot\tSet firmware to boot from the network\n"
"\t--server url\tUse BDSP to fetch boot parameters from <url>\n"
"\t--verbose\tVerbose output\n"
          
          ,
          stderr);
    
    exit(1);
}

/* Basically lifted from the man page */
void usage_short(void) {
    fprintf(stderr, "Usage: %s [options]\n", getprogname());
    fputs(
"bless --help\n"
"\n"
"bless --folder directory [--file file]\n"
"\t[--bootinfo [file]] [--bootefi [file]]\n"
"\t[--setBoot] [--openfolder directory] [--verbose]\n"
"\n"
"bless --mount directory [--file file] [--setBoot] [--verbose]\n"
"\n"
"bless --device device [--setBoot] [--verbose]\n"
"\n"
"bless --netboot --server url [--verbose]\n"
"\n"
"bless --info [directory] [--getBoot] [--plist] [--verbose] [--version]\n"
,
	  stderr);
    exit(1);
}
