#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/param.h>
#include <sys/mount.h>

#include "bless.h"

#define isoption(kfl) (!strcmp(&(*current)[1], commandlineopts[kfl].flag))
#define fetchoptionarg(buf)  current++; i++; \
    if(*current) { strncpy(buf, *current, MAXPATHLEN-1); buf[MAXPATHLEN-1] = '\0'; \
    } else { usage(); }


int main (int argc, const char * argv[])
{

    /* parameter variables */
    unsigned char folderXpath[MAXPATHLEN];  // Path to OS X folder, usually /System/Library/CoreServices
    unsigned char folder9path[MAXPATHLEN];  // Path to OS 9 folder, usually /System Folder
    unsigned char bootXpath[MAXPATHLEN];    // Path to bootx.bootinfo,
                                            // usually /usr/standalone/ppc/bootx.bootinfo

    UInt32 folderXid = 0;                   // The directory ID specified by folderXpath
    UInt32 folder9id = 0;                   // The directory ID specified by folder9path

    unsigned char infopath[MAXPATHLEN];    // Volume to print info


    unsigned char mountpoint[MNAMELEN];
    int i, err;
    char **current;
    
    if(argc == 1) {
        usage();
    }
    
    folderXpath[0] = '\0';
    folder9path[0] = '\0';
    bootXpath[0] = '\0';
    
    for(i=1, current=argv+1; i < argc; i++, current++) {
      
        if(isoption(kbootinfo)) {
            fetchoptionarg(bootXpath);
        } else if(isoption(kbootblocks)) {
	  config.bblocks = 1;
        } else if(isoption(kdebug)) {
            config.debug = 1;
        } else if(isoption(kfolder)) {
            fetchoptionarg(folderXpath);
        } else if(isoption(kfolder9)) {
            fetchoptionarg(folder9path);
        } else if(isoption(kinfo)) {
            config.info = 1;
            fetchoptionarg(infopath);
        } else if(isoption(kplist)) {
            config.plist = 1;
        } else if(isoption(kquiet)) {
            config.quiet = 1;
        } else if(isoption(ksetOF)) {
            config.setOF= 1;
        } else if(isoption(kuse9)) {
            config.use9= 1;
        } else if(isoption(kverbose)) {
            config.verbose= 1;
        } else {
            usage();
        }
    }

    /* If it was requested, print out the Finder Info words */
    if(config.info) {
        unsigned char infom[MAXPATHLEN];
        
        if(err = getMountPoint(infopath, "", infom)) {
            errorprintf("Can't get mount point for %s\n", infopath);
        }
        if(err = dumpFI(infom)) {
            errorprintf("Can't print Finder information\n");
            exit(1);
        }
        exit(0);
    }

    /* Quick sanity testing of the command-line arguments */
    if(folderXpath[0] == '\0' && folder9path[0] == '\0')	{ usage(); }
    if(folderXpath[0] == '\0' && bootXpath[0] != '\0')		{ usage(); }
    if(folder9path[0] == '\0' && config.use9)			{ usage(); }
    if(folder9path[0] == '\0' && config.bblocks)		{ usage(); }

    if(!(geteuid() == 0 || getuid() == 0) && !config.debug) {
      errorprintf("Not run as root, enabling -noexec mode\n");
      config.debug = 1;
    }

    /* If user gave options that require BootX creation, do it now. */
    if(bootXpath[0] != '\0') {
        if(err = createBootX(bootXpath, folderXpath)) {
            errorprintf("Could not create BootX at %s/%s\n", folderXpath, BOOTX);
        } else {
            verboseprintf("BootX created successfully at %s/%s\n", folderXpath, BOOTX);
        }
    } else {
        verboseprintf("No BootX creation requested\n");
    }


    /* We shouldn't need to create anything else at this point. Just bless */
    
    /* First get any directory IDs we need */
    if(folderXpath[0] != '\0') {
        if(err = getFolderID(folderXpath, &folderXid)) {
            errorprintf("Error while get directory ID of %s\n", folderXpath);
        } else {
            verboseprintf("Got directory ID of %ld for %s\n", folderXid, folderXpath);
        }
    }

    if(folder9path[0] != '\0') {
        if(err = getFolderID(folder9path, &folder9id)) {
            errorprintf("Error while get directory ID of %s\n", folder9path);
        } else {
            verboseprintf("Got directory ID of %ld for %s\n", folder9id, folder9path);
        }
    }
    

    /* We know that at least one folder has been specified */
    if(err = getMountPoint(folderXpath, folder9path, mountpoint)) {
        errorprintf("Can't determine mount point of '%s' and '%s'\n", folderXpath, folder9path);
	exit(1);
    } else {
      verboseprintf("Common mount point of '%s' and '%s' is %s\n", folderXpath, folder9path, mountpoint);
    }



    /* Bless the folders */
    if(err = blessDir(mountpoint, folderXid, folder9id)) {
        errorprintf("Can't bless directories\n");
	exit(1);
    } else {
      verboseprintf("Volume at %s blessed successfully\n", mountpoint);
    }


#if !defined(DARWIN)
    if(config.bblocks) {
      if(err = setBootBlocks(mountpoint, folder9id)) {
	errorprintf("Can't set boot blocks for %s\n", mountpoint);
	exit(1);
      } else {
	verboseprintf("Boot blocks set successfully\n");
      }
    }
#endif // !defined(DARWIN)

    /* Set Open Firmware to boot off the specified volume*/
    if(config.setOF) {
        if(err = setOpenFirmware(mountpoint)) {
            errorprintf("Can't set Open Firmware\n");
            exit(1);
        } else {
	  verboseprintf("Open Firmware set successfully\n");
	}
    }

    if(config.debug) {
        verboseprintf("Bless was successful\n");
    }
    exit(0);
}
