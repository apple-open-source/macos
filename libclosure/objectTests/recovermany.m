#import <Foundation/Foundation.h>
#import <Block.h>

// CONFIG GC RR -C99

int recovered = 0;

@interface TestObject : NSObject {
}
@end
@implementation TestObject
- (void)finalize {
    ++recovered;
    [super finalize];
}
- (void)dealloc {
    ++recovered;
    [super dealloc];
}
@end

void testRoutine() {
    __block id to = [[TestObject alloc] init];
    __block int i = 10;
    __block int j = 11;
    __block int k = 12;
    __block id to2 = [[TestObject alloc] init];
    void (^b)(void) = [^{
        [to self];
        ++i;
        k = i + ++j;
        [to2 self];
    } copy];
    for (int i = 0; i < 10; ++i)
        [b retain];
    for (int i = 0; i < 10; ++i)
        [b release];
    for (int i = 0; i < 10; ++i)
        Block_copy(b);
    for (int i = 0; i < 10; ++i)
        Block_release(b);
    [b release];
    [to release];
    // block_byref_release needed under non-GC to get rid of testobject
}
    
void testGC() {
    NSGarbageCollector *collector = [NSGarbageCollector defaultCollector];
    if (!collector) return;
    for (int i = 0; i < 200; ++i)
        [[TestObject alloc] init];
    [collector collectIfNeeded];
    [collector collectExhaustively];
    if (recovered == 200) {
        recovered = 0;
        return;
    }
    printf("only recovered %d of 200\n", recovered);
    exit(1);
}
    

int main(char *argc, char *argv[]) {
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
    NSGarbageCollector *collector = [NSGarbageCollector defaultCollector];
    //testGC();
    for (int i = 0; i < 200; ++i)   // do enough to trigger TLC if GC is on
        testRoutine();
    [collector collectIfNeeded]; // trust that we can kick off TLC
    [collector collectExhaustively];
    if (recovered != 0) {
        printf("%s: success\n", argv[0]);
        exit(0);
    }
    printf("%s: *** didn't recover byref block variable\n", argv[0]);
    exit(1);
}
