#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/param.h>
#include <sys/uio.h>
#include <string.h>

#include "bless.h"
#include "compatCarbon.h"

int getBootBlocks(unsigned char mountpoint[], UInt32 dir9, unsigned char bootBlocks[]) {

    FSSpec                          spec, systemFile;
    SInt16                          fRefNum;
    Handle                          bbH;
    int err;

    bzero(bootBlocks, 1024);
    
    if(!loadCarbonCore()) {
        errorprintf("CarbonCore could not be dynamically loaded\n");
        return 10;
    }
    
    if(err = _bless_NativePathNameToFSSpec(mountpoint, &spec, 0)) {
       return 1;
    }
    
    if(err = _bless_FSMakeFSSpec(spec.vRefNum, dir9,  SYSTEM , &systemFile)) {
        return 2;
    }

    fRefNum = _bless_FSpOpenResFile(&systemFile, fsRdPerm);
    if (fRefNum == -1) {
        return 3;
    }

    bbH = _bless_Get1Resource('boot', 1);
    if (!bbH) {
        return 4;
    }

    _bless_DetachResource(bbH);
    memcpy(bootBlocks, *bbH, 1024);
    _bless_DisposeHandle(bbH);
    _bless_CloseResFile(fRefNum);
        
    return 0;
}
