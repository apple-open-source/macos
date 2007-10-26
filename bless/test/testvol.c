#include <unistd.h>
#include <sys/types.h>
#include <sys/attr.h>
#include <stdint.h>
#include <stdio.h>
#include <err.h>

struct volinfobuf {
  uint32_t info_length;
  uint32_t finderinfo[8];
}; 


int main(int argc, char *argv[]) {
    int ret, i;
    struct volinfobuf vinfo;
    struct attrlist alist;


    alist.bitmapcount = 5;
    alist.reserved = 0;
    alist.commonattr = ATTR_CMN_FNDRINFO;
    alist.volattr = ATTR_VOL_INFO;
    alist.dirattr = 0;
    alist.fileattr = 0;
    alist.forkattr = 0;
    
    ret = getattrlist(argv[1], &alist, &vinfo, sizeof(vinfo), 0);
    if(ret)
      err(1, "getattrlist");

    printf("%u\n", ntohl(vinfo.finderinfo[0]));
    
    return 0;
}

