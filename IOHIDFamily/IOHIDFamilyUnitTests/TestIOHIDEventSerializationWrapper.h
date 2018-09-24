//
//  TestEvent.h
//  IOHIDFamily
//
//  Created by AB on 2/26/18.
//

#ifndef TestEvent_h
#define TestEvent_h

#include <stddef.h>
#import <mach/mach_time.h>
#include <vector>


#define TEST_USERSPACE_EVENTS 1

void absolutetime_to_nanoseconds(uint64_t abstime, uint64_t *result);


#define EXTERNAL ((unsigned int) -1)
#define AbsoluteTime_to_scalar(x)    (*(uint64_t *)(x))
#define AbsoluteTime UInt64

#define SUB_ABSOLUTETIME(t1, t2)                \
(AbsoluteTime_to_scalar(t1) -=                \
AbsoluteTime_to_scalar(t2))

class OSObject
{
    uint32_t _refcount;
public:
    void release();
    void retain();
    bool init();
    void free();
    OSObject() : _refcount(1) {}
    virtual ~OSObject() {}
    
};

class OSArray
{
public:
    static OSArray* withObjects(const OSObject* object[], int size);
    void release();
    bool setObject(void* object);
    const OSObject* getObject(int index);
    unsigned int getCount();
    std::vector<const OSObject*> _array;
    
};

void  IOFree(void * address, size_t size);
void * IOMalloc(size_t size);


#endif /* TestEvent_h */



