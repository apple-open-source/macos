/*
 *  dumpFI.c
 *  bless
 *
 *  Created by ssen on Thu Apr 19 2001.
 *  Copyright (c) 2001 Apple Computer, Inc. All rights reserved.
 *
 */

#include <CoreFoundation/CoreFoundation.h>

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <string.h>
#include <sys/param.h>
#include <sys/attr.h>

#include "bless.h"
#include "dumpFI.h"

/*
 * 1. getattr on the mountpoint to get the volume id
 * 2. read in the finder words
 * 3. for the directories we're interested in, get the entries in /.vol
 */
int dumpFI(unsigned char mount[]) {
    UInt32 finderinfo[8];
    int err;
    UInt32 i;
    UInt32 dirID;
    CFMutableDictionaryRef dict = NULL;
    CFMutableArrayRef infarray = NULL;
    CFDataRef bdata = NULL;
    CFDataRef xmldata = NULL;
    UInt8 *xbuff = NULL;
    CFIndex datasize;

    unsigned char blesspath[MAXPATHLEN];


    if(err = getFinderInfo(mount, finderinfo)) {
        return 1;
    }

    infarray = CFArrayCreateMutable(NULL,
				    8,
				    NULL);

    dict =  CFDictionaryCreateMutable(NULL, 3, NULL, NULL);

    for(i = 0; i< 8-2; i++) {
      CFMutableDictionaryRef word =
	CFDictionaryCreateMutable(NULL,2,NULL,NULL);
      
      
      dirID = finderinfo[i];
      blesspath[0] = '\0';
      
      if(err = lookupIDOnMount(mount, dirID, blesspath)) {
	return 1;
      }
      if(!config.plist) {
	regularprintf("finderinfo[%i]: %6lu => %s%s\n", i, dirID, messages[i][dirID > 0], blesspath);
      }
      
      CFDictionaryAddValue(word, CFSTR("Directory ID"),
			   CFNumberCreate(NULL, kCFNumberLongType, &dirID));
      
      CFDictionaryAddValue(word, CFSTR("Path"),
			   CFStringCreateWithCString(NULL, blesspath, kCFStringEncodingUTF8));
      
      CFArrayAppendValue(infarray, word);
			     
    }

    CFDictionaryAddValue(dict, CFSTR("Finder Info"),
			 infarray);
    
    if(config.bblocks) {

      fbootstraptransfer_t        bbr;
      int                         fd;
      unsigned char                       bbPtr[1024];
  
      unsigned char    system[16];
      unsigned char    shellApplication[16];
      unsigned char    debugger1[16];
      unsigned char    debugger2[16];
      unsigned char    startupScreen[16];
      unsigned char    startupApplication[16];

      CFMutableDictionaryRef bdict =
	CFDictionaryCreateMutable(NULL, 7, NULL, NULL);

      fd = open(mount, O_RDONLY);
      if (fd == -1) {
	errorprintf("Can't open volume mount point for %s\n", mount);
	return 2;
      }
  
      bbr.fbt_offset = 0;
      bbr.fbt_length = 1024;
      bbr.fbt_buffer = bbPtr;
  
      err = fcntl(fd, F_READBOOTSTRAP, &bbr);
      if (err) {
        errorprintf("Can't read boot blocks\n");
        close(fd);
        return 3;
      } else {
        verboseprintf("Boot blocks read successfully\n");
      }
      close(fd);

      //            write(fileno(stdout), bbPtr, 1024);
  

      snprintf(system, 16, "%.*s",  bbPtr[2+4+2+2], &bbPtr[2+4+2+2+1]);
      snprintf(shellApplication, 16, "%.*s",
	       bbPtr[2+4+2+2+16], &bbPtr[2+4+2+2+16+1]);
      snprintf(debugger1, 16, "%.*s",
	       bbPtr[2+4+2+2+16+16], &bbPtr[2+4+2+2+16+16+1]);
      snprintf(debugger2, 16, "%.*s",
	       bbPtr[2+4+2+2+16+16+16], &bbPtr[2+4+2+2+16+16+16+1]);
      snprintf(startupScreen, 16, "%.*s",
	       bbPtr[2+4+2+2+16+16+16+16], &bbPtr[2+4+2+2+16+16+16+16+1]);
      snprintf(startupApplication, 16, "%.*s",
	       bbPtr[2+4+2+2+16+16+16+16+16], &bbPtr[2+4+2+2+16+16+16+16+16+1]);

      if(!config.plist) {
	regularprintf("System: %s\n", system);
	regularprintf("ShellApplication: %s\n", shellApplication);
	regularprintf("Debugger1: %s\n", debugger1);
	regularprintf("Debugger2: %s\n", debugger2);
	regularprintf("StartupScreen: %s\n", startupScreen);
	regularprintf("StartupApplication: %s\n", startupApplication);
      }

      CFDictionaryAddValue(bdict, CFSTR("System"),
			   CFStringCreateWithCString(NULL,
						     system,
						     kCFStringEncodingUTF8));

      CFDictionaryAddValue(bdict, CFSTR("ShellApplication"),
			   CFStringCreateWithCString(NULL,
						     shellApplication,
						     kCFStringEncodingUTF8));

      CFDictionaryAddValue(bdict, CFSTR("Debugger1"),
			   CFStringCreateWithCString(NULL,
						     debugger1,
						     kCFStringEncodingUTF8));

      CFDictionaryAddValue(bdict, CFSTR("Debugger2"),
			   CFStringCreateWithCString(NULL,
						     debugger2,
						     kCFStringEncodingUTF8));

      CFDictionaryAddValue(bdict, CFSTR("StartupScreen"),
			   CFStringCreateWithCString(NULL,
						     startupScreen,
						     kCFStringEncodingUTF8));

      CFDictionaryAddValue(bdict, CFSTR("StartupApplication"),
			   CFStringCreateWithCString(NULL,
						     startupApplication,
						     kCFStringEncodingUTF8));

      bdata = CFDataCreate(NULL, bbPtr, 1024);

      CFDictionaryAddValue(bdict, CFSTR("Data"),
			   bdata);
 
      //	CFDataRef xmldata  = CFPropertyListCreateXMLData(NULL, bdata);
    //	UInt8 *xbuff    = (UInt8 *)CFDataGetBytePtr(xmldata);
    //	int datasize = CFDataGetLength(xmldata);
    //	write(fileno(stdout), xbuff, datasize);  

      CFDictionaryAddValue(dict, CFSTR("BootBlocks"),
			   bdict);
    
    }

    if(config.plist) {
      xmldata  = CFPropertyListCreateXMLData(NULL, dict);
      xbuff    = (UInt8 *)CFDataGetBytePtr(xmldata);
      datasize = CFDataGetLength(xmldata);
      write(fileno(stdout), xbuff, datasize);  
    }
    return 0;
}
