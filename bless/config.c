/*
 *  config.c
 *  bless
 *
 *  Created by ssen on Sun Apr 22 2001.
 *  Copyright (c) 2001 Apple Computer, Inc. All rights reserved.
 *
 */


#include "bless.h"

struct clopt commandlineopts[] = {
    { "Path to a bootx.bootinfo file to be used as a BootX", "bootinfo", 1 },
    { "Set boot blocks if an OS 9 folder was specified", "bootBlocks", 0 },
    { "Show what would have been done, without causing any changes", "noexec", 0},
    { "Darwin/Mac OS X folder to be blessed", "folder", 1},
    { "Classic/Mac OS 9 folder to be blessed", "folder9", 1},
    { "Print out Finder info fields for the specified volume", "info", 1},
    { "Quiet mode", "quiet", 0},
    { "Plist format when -info specified", "plist", 0},
    { "Set Open Firmware to boot off this partition", "setOF", 0 },
    { "If both an X and 9 folder is specified, prefer the 9 one", "use9", 0},
    { "Verbose mode", "verbose", 0},
};

struct blessconfig config = { 0, 0, 0, 0, 0, 0, 0};

