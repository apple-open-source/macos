#import <Foundation/Foundation.h>

#import <Block_private.h>

// CONFIG RR

typedef void (^void_block_t)(void);

int main (int argc, const char * argv[]) {
    void_block_t c = ^{ NSLog(@"Hello!"); };
    
    //printf("global block c looks like: %s\n", _Block_dump(c));
    int j;
    for (j = 0; j < 1000; j++)
    {
        void_block_t d = [c copy];
        //if (j == 0) printf("copy looks like %s\n", _Block_dump(d));
        [d release];
    }
    
    return 0;
}
