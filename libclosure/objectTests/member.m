// CONFIG C++ GC RR

#import <Foundation/Foundation.h>
#import <Block.h>

int recovered = 0;


#ifdef __cplusplus

int constructors = 0;
int destructors = 0;

#define CONST const

class TestObject
{
public:
	TestObject(CONST TestObject& inObj);
	TestObject();
	~TestObject();
	
	//TestObject& operator=(CONST TestObject& inObj);
        
        void test(void);

	int version() CONST { return _version; }
private:
	mutable int _version;
};

TestObject::TestObject(CONST TestObject& inObj)
	
{
        ++constructors;
        _version = inObj._version;
	printf("%p (%d) -- TestObject(const TestObject&) called", this, _version); 
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

#if 0
TestObject& TestObject::operator=(CONST TestObject& inObj)
{
	printf("%p -- operator= called", this);
        _version = inObj._version;
	return *this;
}
#endif

void TestObject::test(void)  {
    void (^b)(void) = ^{ recovered = _version; };
    void (^b2)(void) = [b copy];
    b2();
}

#endif

void testRoutine() {
#ifdef __cplusplus
    TestObject one;
    
    one.test();
#endif
}
    
    

int main(char *argc, char *argv[]) {
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
    NSGarbageCollector *collector = [NSGarbageCollector defaultCollector];
    testRoutine();
    [collector collectIfNeeded]; // trust that we can kick off TLC
    [collector collectExhaustively];
#ifdef __cplusplus
    if (recovered == 1) {
        printf("%s: success\n", argv[0]);
        exit(0);
    }
    printf("%s: *** didn't recover byref block variable\n", argv[0]);
    exit(1);
#else
    printf("%s: placeholder success\n", argv[0]);
    return 0;
#endif
}
