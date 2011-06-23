/*cc -o /tmp/readfb readfb.c -framework IOKit -framework ApplicationServices -Wall -g -arch i386
*/


#include <IOKit/IOKitLib.h>
#include <ApplicationServices/ApplicationServices.h>
#include <IOKit/i2c/IOI2CInterface.h>
#include <IOKit/graphics/IOFramebufferShared.h>
#include <IOKit/graphics/IOAccelSurfaceControl.h>
#include <IOKit/graphics/IOGraphicsLib.h>


IOReturn
IOAccelReadFramebuffer(io_service_t framebuffer, uint32_t width, uint32_t height, size_t rowBytes,
                        vm_address_t * result, vm_size_t * bytecount)
{
    IOReturn     err;
    io_service_t accelerator;
    UInt32       framebufferIndex;
    size_t       size = 0;
    UInt32       surfaceID = 155;
    vm_address_t buffer = 0;
    IOAccelConnect                      connect = MACH_PORT_NULL;
    IOAccelDeviceRegion *               region = NULL;
    IOAccelSurfaceInformation           surfaceInfo;
    IOGraphicsAcceleratorInterface **   interface = 0;
    IOBlitterPtr                        copyRegionProc;
    IOBlitCopyRegion                    op;
    IOBlitSurface                       dest;
    SInt32                              quality = 0;

    *result    = 0;
    *bytecount = 0;
    dest.interfaceRef = NULL;

    do
    {
        err = IOAccelFindAccelerator(framebuffer, &accelerator, &framebufferIndex);
        if (kIOReturnSuccess != err) continue;
        err = IOAccelCreateSurface(accelerator, surfaceID, 
                                   kIOAccelSurfaceModeWindowedBit | kIOAccelSurfaceModeColorDepth8888,
                                   &connect);
        IOObjectRelease(accelerator);
        if (kIOReturnSuccess != err) continue;
    
        size = rowBytes * height;
    
        region = calloc(1, sizeof (IOAccelDeviceRegion) + sizeof(IOAccelBounds));
        if (!region) continue;
    
        region->num_rects = 1;
        region->bounds.x = region->rect[0].x = 0;
        region->bounds.y = region->rect[0].y = 0;
        region->bounds.h = region->rect[0].h = height;
        region->bounds.w = region->rect[0].w = width;
        
        err = vm_allocate(mach_task_self(), &buffer, size, 
                          VM_FLAGS_ANYWHERE | VM_MAKE_TAG(VM_MEMORY_COREGRAPHICS_FRAMEBUFFERS));
        if (kIOReturnSuccess != err) continue;
    
        err = IOAccelSetSurfaceFramebufferShapeWithBackingAndLength(connect, region,
                    kIOAccelSurfaceShapeIdentityScaleBit| 
                    kIOAccelSurfaceShapeNonBlockingBit| 
                    //kIOAccelSurfaceShapeStaleBackingBit |
                    kIOAccelSurfaceShapeNonSimpleBit,
                    0,
                    (IOVirtualAddress) buffer,
                    (UInt32) rowBytes,
                    (UInt32) size);
        if (kIOReturnSuccess != err) continue;
        err = IOCreatePlugInInterfaceForService(framebuffer,
                            kIOGraphicsAcceleratorTypeID,
                            kIOGraphicsAcceleratorInterfaceID,
                            (IOCFPlugInInterface ***)&interface, &quality );
        if (kIOReturnSuccess != err) continue;
        err = (*interface)->GetBlitter(interface,
                                    kIOBlitAllOptions,
                                    (kIOBlitTypeCopyRegion | kIOBlitTypeOperationType0),
                                    kIOBlitSourceFramebuffer,
                                    &copyRegionProc);
        if (kIOReturnSuccess != err) continue;
        err = (*interface)->AllocateSurface(interface, kIOBlitHasCGSSurface, &dest, (void *) surfaceID);
        if (kIOReturnSuccess != err) continue;
        err = (*interface)->SetDestination(interface, kIOBlitSurfaceDestination, &dest);
        if (kIOReturnSuccess != err) continue;
        op.region = region;
        op.deltaX = 0;
        op.deltaY = 0;
        err = (*copyRegionProc)(interface,
                        kNilOptions,
                        (kIOBlitTypeCopyRegion | kIOBlitTypeOperationType0),
                        kIOBlitSourceFramebuffer,
                        &op.operation,
                        (void *) 0);
        if (kIOReturnSuccess != err) continue;
        (*interface)->Flush(interface, kNilOptions);
        err = IOAccelWriteLockSurfaceWithOptions(connect,
                kIOAccelSurfaceLockInBacking, &surfaceInfo, sizeof(surfaceInfo));
        if (kIOReturnSuccess != err) continue;
    
        (void ) IOAccelWriteUnlockSurfaceWithOptions(connect, kIOAccelSurfaceLockInBacking);
    }
    while (false);

    if (dest.interfaceRef) (*interface)->FreeSurface(interface, kIOBlitHasCGSSurface, &dest);

    // destroy the surface
    if (connect) (void) IOAccelDestroySurface(connect);

    if (region) free(region);

    if (interface) IODestroyPlugInInterface((IOCFPlugInInterface **)interface);

    if (kIOReturnSuccess == err) 
    {
        *result    = buffer;
        *bytecount = size;
    }

    return (err);
}

int main(int argc, char * argv[])
{
    IOReturn            err;
    CGDirectDisplayID   dspy = CGMainDisplayID();
    io_service_t        framebuffer;
    CGRect              bounds;
    vm_address_t        buffer;
    vm_size_t           size, rowBytes;

    framebuffer = CGDisplayIOServicePort(dspy);
    assert (framebuffer != MACH_PORT_NULL);
    dspy = CGMainDisplayID();
    bounds = CGDisplayBounds(dspy);
    rowBytes = CGDisplayBytesPerRow(dspy);

    err = IOAccelReadFramebuffer(framebuffer, bounds.size.width, bounds.size.height, rowBytes,
                                 &buffer, &size);
    if (kIOReturnSuccess == err)
    {
        fprintf(stderr, "writing 0x%x bytes from 0x%x\n", size, buffer);
        write(STDOUT_FILENO, (const void *) buffer, size);
        vm_deallocate(mach_task_self(), buffer, size);
    }
    return (0);
}

