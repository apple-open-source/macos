/*
 *  bless.h
 *  bless
 *
 *  Created by ssen on Tue Apr 17 2001.
 *  Copyright (c) 2001 Apple Computer, Inc. All rights reserved.
 *
 */

#import <libkern/OSTypes.h>
#include <IOKit/IOTypes.h>
#include <sys/attr.h>

#define PROGRAM "bless"
#define BOOTX "BootX"
#define SYSTEM "\pSystem"

struct cataloginfo {
    attrreference_t name;
    fsid_t volid;
    fsobj_id_t objectid;
    fsobj_id_t parentid;
    char namebuffer[NAME_MAX];
};

struct cataloginforeturn {
    unsigned long length;
    struct cataloginfo c;
};

struct volinfobuf {
    UInt32 info_length;
    UInt32 finderinfo[8];
}; 

struct blessconfig {
    short debug;
    short info;
    short plist;
    short verbose;
    short quiet;
    short setOF;
    short use9;
  short bblocks;
};

/* command-line options */
struct clopt {
    char *description;
    char *flag;
    short takesarg;
};

extern struct clopt commandlineopts[];
extern struct blessconfig config;

enum {
    kbootinfo = 0,
    kbootblocks,
    kdebug,
    kfolder,
    kfolder9,
    kinfo,
    kquiet,
    kplist,
    ksetOF,
    kuse9,
    kverbose,
    klast
};



void usage();
int createBootX(unsigned char bootXsrc[], unsigned char folder[]);
int setTypeCreator(unsigned char path[], UInt32 type, UInt32 creator);
int getFolderID(unsigned char path[], UInt32 *folderID);
int getMountPoint(unsigned char f1[], unsigned char f2[], unsigned char mountp[]);
int blessDir(unsigned char mountpoint[], UInt32 dirX, UInt32 dir9);
int dumpFI(unsigned char mount[]);
int lookupIDOnMount(unsigned char mount[], unsigned long fileID, unsigned char out[]);

int setFinderInfo(unsigned char mountpoint[], UInt32 words[]);
int getFinderInfo(unsigned char mountpoint[], UInt32 words[]);


int setOpenFirmware(unsigned char mountpoint[]);
int getOFInfo(unsigned char mountpoint[], char ofstring[]);
int getOFDev(io_object_t obj, unsigned long * pNum,
	     unsigned char parentDev[],
	     unsigned char OFDev[]);
int getNewWorld();

int getBootBlocks(unsigned char mountpoint[], UInt32 dir9, unsigned char blocks[]);
int setBootBlocks(unsigned char mountpoint[], UInt32 dir9);


int verboseprintf(char const *fmt, ...);
int regularprintf(char const *fmt, ...);
int errorprintf(char const *fmt, ...);
int warningprintf(char const *fmt, ...);

