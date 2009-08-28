//
//  escape.m
//  btest
//
//  Created by Apple on 6/12/08.
//  Copyright 2008 Apple. All rights reserved.
//


#import "common.h"


void test(void) {
     __block int  i = 0;
    vv block = ^{  ++i; };
    vv blockCopy = Block_copy(block);
    lastUse(i);
    Block_release(blockCopy);
    lastUse(i);
}
