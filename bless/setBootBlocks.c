#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>

#include "bless.h"

int setBootBlocks(unsigned char mountpoint[], UInt32 dir9) {
  /* If a Classic system folder was specified, and we need to set the boot blocks
   * for the volume, read it from the boot 0 resource of the System file, and write it */
  fbootstraptransfer_t        bbr;
  int                         fd;
  unsigned char                       bbPtr[1024];
  int err;
  
  if(err = getBootBlocks(mountpoint, dir9, bbPtr)) {
    errorprintf("Can't get boot blocks for %s\n", mountpoint);
    return 1;
  }
  
  fd = open(mountpoint, O_RDONLY);
  if (fd == -1) {
    errorprintf("Can't open volume mount point for %s\n", mountpoint);
    return 2;
  }
  
  bbr.fbt_offset = 0;
  bbr.fbt_length = 1024;
  bbr.fbt_buffer = bbPtr;
  
  if(!config.debug) {
    err = fcntl(fd, F_WRITEBOOTSTRAP, &bbr);
    if (err) {
        errorprintf("Can't write boot blocks\n");
        close(fd);
        return 3;
    } else {
        verboseprintf("Boot blocks written successfully\n");
    }
  }
  close(fd);
  
  return 0;
}
