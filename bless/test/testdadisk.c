#include <libc.h>
#include <DiskArbitration/DiskArbitration.h>
#include <err.h>
#include "UtilitiesCFPrettyPrint.h"

// cc -o testdadisk testdadisk.c UtilitiesCFPrettyPrint.c -framework CoreFoundation -framework DiskArbitration


int main(int argc, char *argv[]) {
  
  char *dev = NULL;
  DADiskRef disk = NULL;
  DASessionRef session = NULL;
  CFDictionaryRef props = NULL;


  if(argc != 2) {
    fprintf(stderr, "Usage: %s disk1s2\n", getprogname());
    exit(1);
  }

  dev = argv[1];

  session = DASessionCreate(kCFAllocatorDefault);
  if(session == NULL)
    errx(1, "DASessionCreate");

  disk = DADiskCreateFromBSDName(kCFAllocatorDefault, session, dev);
  if(disk == NULL) {
    CFRelease(session);
    errx(1, "DADiskCreateFromBSDName");
  }

  props = DADiskCopyDescription(disk);
  if(props == NULL) {
    CFRelease(session);
    CFRelease(disk);
    errx(1, "DADiskCopyDescription");
  }

  TAOCFPrettyPrint(disk);
  TAOCFPrettyPrint(props);

  CFRelease(session);
  CFRelease(disk);
  CFRelease(props);

  return 0;
}
