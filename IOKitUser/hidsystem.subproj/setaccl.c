/*
cc -o /tmp/setaccl setaccl.c -Wall -framework IOKit
*/

#include <IOKit/IOKitLib.h>
#include <drivers/event_status_driver.h>
#include <IOKit/hidsystem/IOHIDLib.h>
#include <IOKit/hidsystem/IOHIDParameter.h>
#include <stdio.h>
#include <assert.h>

int
main(int argc, char **argv)
{
    NXEventHandle	hdl;
    double		dbl1;
    CFStringRef		key;

    hdl = NXOpenEventStatus();
    assert( hdl );

    if( argc > 2) {

        if( 't' == argv[2][0])
            key = CFSTR(kIOHIDTrackpadAccelerationType);
        else
            key = CFSTR(kIOHIDMouseAccelerationType);

        assert( KERN_SUCCESS == IOHIDGetAccelerationWithKey(hdl, key, &dbl1));
        printf("IOHIDGetAccelerationWithKey = %f\n", dbl1);
        sscanf( argv[1], "%lf", &dbl1 );
        assert( KERN_SUCCESS == IOHIDSetAccelerationWithKey(hdl, key, dbl1));
        assert( KERN_SUCCESS == IOHIDGetAccelerationWithKey(hdl, key, &dbl1));
        printf("now IOHIDGetAccelerationWithKey = %f\n", dbl1);
    } else {
        assert( KERN_SUCCESS == IOHIDGetMouseAcceleration(hdl, &dbl1));
        printf("IOHIDGetMouseAcceleration = %f\n", dbl1);
        sscanf( argv[1], "%lf", &dbl1 );
        assert( KERN_SUCCESS == IOHIDSetMouseAcceleration(hdl, dbl1));
        assert( KERN_SUCCESS == IOHIDGetMouseAcceleration(hdl, &dbl1));
        printf("now IOHIDGetMouseAcceleration = %f\n", dbl1);
    }

    return( 0 );
}

