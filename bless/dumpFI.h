/*
 *  dumpFI.h
 *  bless
 *
 *  Created by ssen on Thu Apr 19 2001.
 *  Copyright (c) 2001 Apple Computer, Inc. All rights reserved.
 *
 */

/* 8 words of "finder info" in volume
 * 0 & 1 often set to blessed system folder
 * boot blocks contain name of system file, name of shell app, and startup app
 * starting w/sys8 added file OpenFolderListDF ... which wins open at mount
 * there are per-file/folder "finder info" fields such as invis, location, etc
 * "next-folder in "open folder list" chain ... first item came from word 2
 * 3 & 5 co-opted in the X timeframe to store dirID's of dual-install sysF's 
 * 6 & 7 is the vsdb stuff (64 bits) to see if sysA has seen diskB
 *
 * 0 is blessed system folder
 * 1 is folder which contains startup app (reserved for Finder these days)
 * 2 is first link in linked list of folders to open at mount (deprecated)
 *   (9 and X are supposed to honor this if set and ignore OpenFolderListDF)
 *   (but the X Finder has only done this flakily)
 * 3 OS 9 blessed system folder (maybe OS X?)
 * 4 thought to be unused (reserved for Finder, once was PowerTalk Inbox)
 * 5 OS X blessed system folder (maybe OS 9?)
 * 6 & 7 are 64 bit volume identifier (high 32 bits in 6; low in 7)
 */

static char *messages[7][2] = {
        { "No Blessed System Folder", "Blessed System Folder is " },    /* 0 */
        { "No Startup App folder (ignored anyway)", "Startup App folder is " },
        { "Open-folder linked list empty", "1st dir in open-folder list is " },
        { "No OS Classic + X blessed 9 folder", "Classic blessed folder is " },  /* 3 */
        { "Unused field unset", "Thought-to-be-unused field points to " },
        { "No OS Classic + X blessed X folder", "OS X blessed folder is " },  /* 5 */
        { "64-bit VSDB volume id not present", "64-bit VSDB volume id: " }
};

/* DONT USE THIS STRUCT */

typedef struct {
  short   id;
  long   entryPoint;
  short   version;
  short   pageFlags;
  Str15   system;
  Str15   shellApplication;
  Str15   debugger1;
  Str15   debugger2;
  Str15   startupScreen;
  Str15   startupApplication;
  char    otherStuff[1024 - (2+4+2+2+16+16+16+16+16+16)];
} BootBlocks;
