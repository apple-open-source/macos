//
//  recoverextra.M
//  testObjects
//
//  Created by Blaine Garst on 10/13/08.
//  Copyright 2008 __MyCompanyName__. All rights reserved.
//



#import <Foundation/Foundation.h>
#import <Block.h>

// CONFIG C++ GC RR rdar://6289367

#if __APPLE_CC__ < 5618
int main(int argc, char *argv[]) {
    printf("compiler version %d < 5618, not trying c++ object tests\n", argv[0], __APPLE_CC__);
    return 0;
}
#else

int constructors = 0;
int destructors = 0;


#ifdef __cplusplus
#import <Block_private.h>

void hack(void *block) {
    printf("Block_dump says: %s\n", _Block_dump(block));
}

#define CONST const

class TestObject
{
public:
	TestObject(CONST TestObject& inObj);
	TestObject();
	~TestObject();
	
	TestObject& operator=(CONST TestObject& inObj);

	int version() CONST { return _version; }
private:
	mutable int _version;
};

TestObject::TestObject(CONST TestObject& inObj)
	
{
        ++constructors;
        _version = inObj._version;
	//printf("%p (%d) -- TestObject(const TestObject&) called\n", this, _version); 
}


TestObject::TestObject()
{
        _version = ++constructors;
	//printf("%p (%d) -- TestObject() called\n", this, _version); 
}


TestObject::~TestObject()
{
	//printf("%p -- ~TestObject() called\n", this);
        ++destructors;
}


TestObject& TestObject::operator=(CONST TestObject& inObj)
{
	//printf("%p -- operator= called\n", this);
        _version = inObj._version;
	return *this;
}


void testRoutine() {
    TestObject one;
    int i = 10;
    int (^intblock)(void) = ^{ printf("have i at %d\n", i); return i; };
    void (^b)(void) = [^{ printf("my copy of one is %d, and intblock is %d\n", one.version(), intblock()); } copy];
#if 0
// just try one copy, one release
    for (int i = 0; i < 10; ++i)
        [b retain];
    for (int i = 0; i < 10; ++i)
        [b release];
    for (int i = 0; i < 10; ++i)
        Block_copy(b);
    for (int i = 0; i < 10; ++i)
        Block_release(b);
#endif
    //hack(^{ printf("my copy of one is %d\n", one.version()); });
    //hack(b);
    [b release];
}
#else
void testRoutine() { }
#endif

int main(char *argc, char *argv[]) {
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
    NSGarbageCollector *collector = [NSGarbageCollector defaultCollector];
    //testGC();
    for (int i = 0; i < 200; ++i)   // do enough to trigger TLC if GC is on
        testRoutine();
    [collector collectIfNeeded]; // trust that we can kick off TLC
    [collector collectExhaustively];
    if ((destructors + 10) >= constructors) {   // allow some GC slop
        printf("%s: success\n", argv[0]);
        exit(0);
    }
    printf("%s: *** didn't recover %d const copies\n", argv[0], constructors - destructors);
    exit(1);
}
#endif