/*
cc idtest.c -o /tmp/idtest -Wall -framework IOKit -framework CoreFoundation
 */

#include <assert.h>
#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/IOKitKeys.h>

int main(int argc, char **argv)
{
    io_registry_entry_t    service;
    io_connect_t           connect;
    SInt32                 id1, id2, id3;
    kern_return_t          status;

    service = IORegistryEntryFromPath(kIOMasterPortDefault, 
                                    kIOServicePlane ":/IOResources/IODisplayWrangler");
    assert(service);
    if (service) 
    {
        status = IOServiceOpen(service, mach_task_self(), 0, &connect);
        IOObjectRelease(service);
        assert(kIOReturnSuccess == status);

    }

    enum { kAlloc, kFree };
enum {
    kIOAccelSpecificID          = 0x00000001
};


    status = IOConnectMethodScalarIScalarO(connect, kAlloc, 2, 1, kNilOptions, 0, &id1);
    assert(kIOReturnSuccess == status);
    printf("ID: %x\n", id1);
    status = IOConnectMethodScalarIScalarO(connect, kFree, 2, 0, kNilOptions, id1);
    assert(kIOReturnSuccess == status);
    status = IOConnectMethodScalarIScalarO(connect, kAlloc, 2, 1, kNilOptions, 0, &id1);
    assert(kIOReturnSuccess == status);
    printf("ID: %x\n", id1);


    status = IOConnectMethodScalarIScalarO(connect, kAlloc, 2, 1, kIOAccelSpecificID, 53, &id2);
    assert(kIOReturnSuccess == status);
    printf("ID: %x\n", id2);


    status = IOConnectMethodScalarIScalarO(connect, kFree, 2, 0, kNilOptions, id1);
    assert(kIOReturnSuccess == status);
    printf("free ID: %d\n", id1);

    status = IOConnectMethodScalarIScalarO(connect, kFree, 2, 0, kNilOptions, id2);
    assert(kIOReturnSuccess == status);
    printf("free ID: %d\n", id2);

    exit(0);    
}


