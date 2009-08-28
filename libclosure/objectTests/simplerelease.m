#import <Foundation/Foundation.h>

int global = 0;

@interface TestObject : NSObject
@end
@implementation TestObject
- (void)release {
    printf("I am released\n");
    global = 1;
    [super release];
}
@end

enum {
    BLOCK_REFCOUNT_MASK =     (0xffff),
    BLOCK_NEEDS_FREE =        (1 << 24),
    BLOCK_HAS_COPY_DISPOSE =  (1 << 25),
    BLOCK_HAS_CTOR =          (1 << 26), // helpers have C++ code
    BLOCK_IS_GC =             (1 << 27),
    BLOCK_IS_GLOBAL =         (1 << 28)
};

struct Block_basic {
    void *isa;
    int Block_flags;  // int32_t
    int Block_size; // XXX should be packed into Block_flags
    void (*Block_invoke)(void *);
    void (*Block_copy)(void *dst, void *src);  // iff BLOCK_HAS_COPY_DISPOSE
    void (*Block_dispose)(void *);             // iff BLOCK_HAS_COPY_DISPOSE
    //long params[0];  // where const imports, __block storage references, etc. get laid down
};

int main(int argc, char *argv) {
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];

    TestObject *to = [[TestObject alloc] init];
    void (^b)(void) = ^{ printf("to is at %p\n", to); };

    // verify that b has a copy/dispose helper
    struct Block_basic *block = (struct Block_basic *)(void *)b;
    if (!(block->Block_flags & BLOCK_HAS_COPY_DISPOSE)) {
        printf("Whoops, no copy dispose!\n");
        exit(1);
    }
    if (!(block->Block_dispose)) {
        printf("Whoops, no block dispose helper function!\n");
        exit(2);
    }
    block->Block_dispose(block);
    if (global != 1) {
	printf("Whoops, helper routine didn't release captive object\n");
        exit(3);
    }
    printf("success!\n");
    exit(0);
}
