//
//  IOHIDDigitizerCollection.cpp
//  HIDDriverKit
//
//  Created by dekom on 2/6/19.
//

#include <AssertMacros.h>
#include <DriverKit/DriverKit.h>
#include <DriverKit/OSCollections.h>
#include <HIDDriverKit/HIDDriverKit_Private.h>

struct IOHIDDigitizerCollection_IVars
{
    IOHIDElement *parentCollection;
    OSArray *elements;
    IOHIDDigitizerCollectionType type;
    bool touch;
    bool inRange;
    IOFixed x;
    IOFixed y;
    IOFixed z;
};

#define _parentCollection   ivars->parentCollection
#define _elements           ivars->elements
#define _type               ivars->type
#define _touch              ivars->touch
#define _inRange            ivars->inRange
#define _x                  ivars->x
#define _y                  ivars->y
#define _z                  ivars->z

#define super OSContainer

void IOHIDDigitizerCollection::free()
{
    if (ivars) {
        OSSafeReleaseNULL(_elements);
    }
    
    IOSafeDeleteNULL(ivars, IOHIDDigitizerCollection_IVars, 1);
    super::free();
}

bool IOHIDDigitizerCollection::initWithType(IOHIDDigitizerCollectionType type,
                                            IOHIDElement *parentCollection)
{
    bool result = false;
    
    require(super::init(), exit);
    
    ivars = IONewZero(IOHIDDigitizerCollection_IVars, 1);
    
    _type = type;
    _parentCollection = parentCollection;
    
    result = true;
    
exit:
    return result;
}

IOHIDDigitizerCollection *IOHIDDigitizerCollection::withType(IOHIDDigitizerCollectionType type,
                                                             IOHIDElement *parentCollection)
{
    IOHIDDigitizerCollection *me = NULL;
    
    me = OSTypeAlloc(IOHIDDigitizerCollection);
    
    if (me && !me->initWithType(type, parentCollection)) {
        me->release();
        return NULL;
    }
    
    return me;
}

void IOHIDDigitizerCollection::addElement(IOHIDElement *element)
{
    if (!_elements) {
        _elements = OSArray::withCapacity(1);
    }
    
    _elements->setObject(element);
}

OSArray *IOHIDDigitizerCollection::getElements()
{
    return _elements;
}

IOHIDElement *IOHIDDigitizerCollection::getParentCollection()
{
    return _parentCollection;
}

IOHIDDigitizerCollectionType IOHIDDigitizerCollection::getType()
{
    return _type;
}

bool IOHIDDigitizerCollection::getTouch()
{
    return _touch;
}

void IOHIDDigitizerCollection::setTouch(bool touch)
{
    _touch = touch;
}

bool IOHIDDigitizerCollection::getInRange()
{
    return _inRange;
}

void IOHIDDigitizerCollection::setInRange(bool inRange)
{
    _inRange = inRange;
}

IOFixed IOHIDDigitizerCollection::getX()
{
    return _x;
}

void IOHIDDigitizerCollection::setX(IOFixed x)
{
    _x = x;
}

IOFixed IOHIDDigitizerCollection::getY()
{
    return _y;
}

void IOHIDDigitizerCollection::setY(IOFixed y)
{
    _y = y;
}

IOFixed IOHIDDigitizerCollection::getZ()
{
    return _z;
}

void IOHIDDigitizerCollection::setZ(IOFixed z)
{
    _z = z;
}
