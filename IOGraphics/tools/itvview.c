/* cc -o /tmp/itvview itvview.c -framework IOKit -framework ApplicationServices -Wall -g
*/

#include <IOKit/IOKitLib.h>
#include <ApplicationServices/ApplicationServices.h>
#include <IOKit/i2c/IOI2CInterface.h>

#include <assert.h>
#include <stdio.h>

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

void EDIDTest( IOI2CConnectRef connect )
{
    kern_return_t kr;
    IOI2CRequest        request;
    UInt8               data[128];
    int i;

    bzero( &request, sizeof(request) );

    request.commFlags                   = 0;

    request.sendAddress                 = 0xA0;
    request.sendTransactionType         = kIOI2CSimpleTransactionType;
    request.sendBuffer                  = (vm_address_t) &data[0];
    request.sendBytes                   = 0x01;
    data[0]                             = 0x00;

    request.replyAddress                = 0xA1;
    request.replyTransactionType        = kIOI2CSimpleTransactionType;
    request.replyBuffer                 = (vm_address_t) &data[0];
    request.replyBytes                  = 128;
    bzero( &data[0], request.replyBytes );

    kr = IOI2CSendRequest( connect, kNilOptions, &request );
    assert( kIOReturnSuccess == kr );
    printf("read result 0x%x, 0x%lx bytes\n", request.result, request.replyBytes);
    if( kIOReturnSuccess != request.result)
        return;

    printf("    0  1  2  3  4  5  6  7  8  9  A  B  C  D  E  F");
    for( i = 0; i < request.replyBytes; i++) {
        if( 0 == (i & 15))
            printf("\n%02x: ", i);
        printf("%02x ", data[i]);
    }
    printf("\n");

}

void iTVTest( IOI2CConnectRef connect )
{
    kern_return_t       kr;
    IOI2CRequest        request;
    UInt8               outData[2];
    UInt8               inData[2];
    int                 i;

    bzero( &request, sizeof(request) );

    request.commFlags           = kIOI2CUseSubAddressCommFlag;
    request.minReplyDelay       = 0;

    for( i = 0; i < 50; i++ ) {

        request.sendAddress             = 0x94;
        request.sendSubAddress          = 0x70;
        request.sendTransactionType     = kIOI2CSimpleTransactionType;
        request.sendBuffer              = (vm_address_t) &outData[0];
        request.sendBytes               = 0x02;
        outData[0]                      = i * 2;
        outData[1]                      = 256 - i;
    
        request.replyTransactionType    = kIOI2CNoTransactionType;
        request.replyBytes              = 0;
    
        kr = IOI2CSendRequest( connect, kNilOptions, &request );
        assert( kIOReturnSuccess == kr );
        if( 0 == i)
            printf("write result 0x%x\n", request.result);
        if( kIOReturnSuccess != request.result)
            return;
    
        request.sendTransactionType     = kIOI2CNoTransactionType;
        request.sendBytes               = 0;
    
        request.replyAddress            = 0x95;
        request.replySubAddress         = 0x70;
        request.replyTransactionType    = kIOI2CCombinedTransactionType;
        request.replyBuffer             = (vm_address_t) &inData[0];
        request.replyBytes              = 2;
        bzero( &inData[0], request.replyBytes );
    
        kr = IOI2CSendRequest( connect, kNilOptions, &request );
        assert( kIOReturnSuccess == kr );
        if( 0 == i)
            printf("read result 0x%x, 0x%lx bytes\n", request.result, request.replyBytes);
        if( kIOReturnSuccess != request.result)
            return;
    
//        printf("%02x %02x\n", inData[0], inData[1]);

        if( (inData[0] != outData[0]) || (inData[1] != outData[1])) {
            printf("mismatch\n");
            break;
        }
    }
    printf("compares OK\n");
}
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

int main( int argc, char * argv[] )
{
    kern_return_t kr;
    io_service_t  framebuffer, interface;
    IOOptionBits  bus;
    IOItemCount   busCount;

    framebuffer = CGDisplayIOServicePort(CGMainDisplayID());

    {
        io_string_t path;
        kr = IORegistryEntryGetPath(framebuffer, kIOServicePlane, path);
        assert( KERN_SUCCESS == kr );
        printf("\nUsing device: %s\n", path);

        kr = IOFBGetI2CInterfaceCount( framebuffer, &busCount );
        assert( kIOReturnSuccess == kr );
    
        for( bus = 0; bus < busCount; bus++ )
        {
            IOI2CConnectRef  connect;
    
            kr = IOFBCopyI2CInterfaceForBus(framebuffer, bus, &interface);
            if( kIOReturnSuccess != kr)
                continue;
    
            kr = IOI2CInterfaceOpen( interface, kNilOptions, &connect );
    
            IOObjectRelease(interface);
            assert( kIOReturnSuccess == kr );
            if( kIOReturnSuccess != kr)
                continue;
    
            printf("\nEDID using bus %ld:\n", bus);
            EDIDTest( connect );
    
            printf("\niTV using bus %ld:\n", bus);
            iTVTest( connect );
    
            IOI2CInterfaceClose( connect, kNilOptions );
        }
    }

    exit(0);
    return(0);
}
