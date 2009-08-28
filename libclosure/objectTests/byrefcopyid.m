//
//  byrefcopyid.m
//  testObjects
//
//  Created by Blaine Garst on 5/13/08.
//  Copyright 2008 __MyCompanyName__. All rights reserved.
//

// NON-GC ONLY
// Tests copying of blocks with byref ints and an id
// CONFIG RR

#import <Foundation/Foundation.h>

#import <Block.h>
#import <Block_private.h>




int CalledRetain = 0;
int CalledRelease = 0;
int CalledSelf = 0;


@interface DumbObject : NSObject {
}
@end

@implementation DumbObject
- retain {
    CalledRetain = 1;
    return [super retain];
}
- (void)release {
    CalledRelease = 1;
    [super release];
}
- self {
    CalledSelf = 1;
    return self;
}
@end


void callVoidVoid(void (^closure)(void)) {
    closure();
}

void (^dummy)(void);



id testRoutine(const char *whoami) {
    __block id  dumbo = [DumbObject new];
    dummy = ^{
        [dumbo self];
    };
    
    
    //doHack(dummy);
    id copy = Block_copy(dummy);
    
    callVoidVoid(copy);
    if (CalledSelf == 0) {
        printf("%s: **** copy helper of byref id didn't call self\n", whoami);
        exit(1);
    }

    return copy;
}

int main(int argc, char *argv[]) {
    id copy = testRoutine(argv[0]);
    Block_release(copy);
    if (CalledRetain != 0) {
        printf("%s: **** copy helper of byref retained the id\n", argv[0]);
        return 1;
    }
    if (CalledRelease != 0) {
        printf("%s: **** copy helper of byref id did release the id\n", argv[0]);
        return 1;
    }
    
    
    printf("%s: success\n", argv[0]);
    return 0;
}