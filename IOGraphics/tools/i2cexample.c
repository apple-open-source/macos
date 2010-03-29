/*cc -o /tmp/i2cexample i2cexample.c -framework IOKit -framework ApplicationServices -Wall -g
*/


#include <IOKit/IOKitLib.h>
#include <ApplicationServices/ApplicationServices.h>
#include <IOKit/i2c/IOI2CInterface.h>

#include <assert.h>
#include <stdio.h>
#include <unistd.h>

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

SInt32 EDIDSum( const UInt8 * bytes, IOByteCount len )
{
    int i,j;
    UInt8 sum;

    for (j=0; j < len; j += 128)
    {
        sum = 0;
        for (i=0; i < 128; i++)
            sum += bytes[j+i];
        if(sum)
            return (j/128);
    }
    return (-1);
}

void EDIDDump( const UInt8 * bytes, IOByteCount len )
{
    int i;

    fprintf(stderr, "/*    0    1    2    3    4    5    6    7    8    9    A    B    C    D    E    F */");
    for (i = 0; i < len; i++)
    {
        if( 0 == (i & 15))
            fprintf(stderr, "\n    ");
        fprintf(stderr, "0x%02x,", bytes[i]);
    }
    fprintf(stderr, "\n");
}

void EDIDRead( IOI2CConnectRef connect, Boolean save )
{
    kern_return_t       kr;
    IOI2CRequest        request;
    UInt8               data[128];
    int                 i;

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
    request.replyBytes                  = sizeof(data);
    bzero( &data[0], request.replyBytes );

    kr = IOI2CSendRequest( connect, kNilOptions, &request );
    assert( kIOReturnSuccess == kr );
    fprintf(stderr, "/* Read result 0x%x, 0x%lx bytes */\n", request.result, request.replyBytes);
    if( kIOReturnSuccess != request.result)
        return;

    EDIDDump( data, request.replyBytes );

    i = EDIDSum( &data[0], request.replyBytes );

    if( i >= 0)
        fprintf(stderr, "/* Block %d checksum bad */\n", i);
    else
        fprintf(stderr, "/* Checksums ok */\n");

    if( save)
        write( STDOUT_FILENO, data, request.replyBytes );
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

int main( int argc, char * argv[] )
{
    kern_return_t kr;
    io_service_t  framebuffer, interface;
    io_string_t   path;
    IOOptionBits  bus;
    IOItemCount   busCount;
    Boolean       save;

    framebuffer = CGDisplayIOServicePort(CGMainDisplayID());

    {
        kr = IORegistryEntryGetPath(framebuffer, kIOServicePlane, path);
        assert( KERN_SUCCESS == kr );
        fprintf(stderr, "\n/* Using device: %s */\n", path);

        kr = IOFBGetI2CInterfaceCount( framebuffer, &busCount );
        assert( kIOReturnSuccess == kr );
    
        for( bus = 0; bus < busCount; bus++ )
        {
            IOI2CConnectRef  connect;

            fprintf(stderr, "/* Bus %ld: */\n", bus);
    
            kr = IOFBCopyI2CInterfaceForBus(framebuffer, bus, &interface);
            if( kIOReturnSuccess != kr)
                continue;
    
            kr = IOI2CInterfaceOpen( interface, kNilOptions, &connect );
    
            IOObjectRelease(interface);
            assert( kIOReturnSuccess == kr );
            if( kIOReturnSuccess != kr)
                continue;
    
            save = (argc > 1) && (argv[1][0] == 's');

            EDIDRead( connect, save );
        
            IOI2CInterfaceClose( connect, kNilOptions );
        }
    }

    exit(0);
    return(0);
}

