//
//  nullblockisa.m
//  testObjects
//
//  Created by Blaine Garst on 9/24/08.
//  Copyright 2008 __MyCompanyName__. All rights reserved.
//
// CONFIG rdar://6244520



#include <stdio.h>
#include <stdlib.h>
#include <Block_private.h>


void check(void (^b)(void)) {
    struct Block_byref *innerp;
    innerp = (struct Block_byref *)(((long)(void *)b) + (Block_size(b)));
    //printf("block is at %p, size is %lx, inner is %p\n", (void *)b, Block_size(b), innerp);
    if (innerp->isa != (void *)NULL) {
        printf("not a NULL __block isa\n");
        exit(1);
    }
    return;
}
        
int main(int argc, char *argv[]) {

   __block int i;
   
   check(^{ printf("%d\n", ++i); });
   printf("%s: success\n", argv[0]);
   return 0;
}
   