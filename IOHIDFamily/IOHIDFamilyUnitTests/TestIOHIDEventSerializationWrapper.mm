//
//  TestEvent.m
//  IOHIDFamilyUnitTests
//
//  Created by AB on 2/26/18.
//


#include <stdlib.h>
#include "TestIOHIDEventSerializationWrapper.h"

OSArray *arrInstance = NULL;

void  IOFree(void * address, size_t size)
{
    if (address && size) {
        free(address);
    }
}

void * IOMalloc(size_t size)
{
    void *ptr = malloc(size);
    return ptr;
}

void OSObject::release()
{
    
    _refcount--;
    if (_refcount == 0) {
        delete this;
    }
}
void OSObject::retain()
{
    _refcount++;
}
void OSObject::free()
{
    delete this;
}
bool OSObject::init()
{
    return true;
}
OSArray* OSArray::withObjects(const OSObject* object[], int size)
{
    OSArray *me  = new OSArray;
    if (!me) {
        return NULL;
    }
    
    for (uint32_t i=0 ;i < size; i++) {
        OSObject *ob = (OSObject*)object[i];
        ob->retain();
        me->_array.push_back(object[i]);
    }
    
    return me;
}
void OSArray::release()
{
    _array.clear();
}

bool OSArray::setObject(void* object)
{
    OSObject *ob = (OSObject*)object;
    ob->retain();
    _array.push_back((const OSObject*)object);
    return true;
}
const OSObject* OSArray::getObject(int index)
{
    if (index >= _array.size()) {
        return NULL;
    }
    return _array[index];
}
unsigned int OSArray::getCount()
{
    return (unsigned int)_array.size();
}
void absolutetime_to_nanoseconds(uint64_t abstime, uint64_t *result)
{
    if (abstime && result) {
        //DUMMY CODE
    }
}
