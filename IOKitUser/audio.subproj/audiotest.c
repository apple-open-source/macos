/*
 * Copyright (c) 2000 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.1 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
/*
cc -o audiotest audiotest.c -framework iokit -Wall
*/

#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>

#include <mach/mach.h>

#include <CoreFoundation/CoreFoundation.h>

#include <IOKit/IOKitLib.h>
#include <IOKit/IOCFSerialize.h>
#include <IOKit/audio/IOAudioLib.h>

mach_port_t	master_device_port;

void TestSound( int freq )
{
    kern_return_t		kr;
    io_connect_t		fb;
    io_iterator_t		iter;
    io_object_t			obj;
    vm_address_t		bufMem, statMem;
    vm_size_t			shmemSize;
    int				i,j;
    int				isOut;
    IOAudioStreamStatus *	status;

    printf("IOAudioStream, iterating\n");
    kr = IORegistryCreateIterator(
        master_device_port, kIOServicePlane, true, &iter);

    while(kIOReturnSuccess == kr) {
        CFTypeRef	object;
        CFDictionaryRef	properties;
        CFDataRef	data;

        printf("=%d, registry IOIteratorNext\n", kr);
        while( (obj = IOIteratorNext( iter ))) {
            if( IOObjectConformsTo( obj, "IOAudioStream" ))
                break;

            IOObjectRelease( obj );
        }
        if(!obj)
            break;

        printf("=%d, getting properties\n", kr);
	kr = IORegistryEntryCreateCFProperties(obj, &object,
                                    kCFAllocatorDefault, kNilOptions);
        if (kr || !object) {
            printf("IORegistryEntryCreateCFProperties failed, "
			"kr=%d, object=0x%x\n", kr, obj);
            break;
        }
        properties = (CFDictionaryRef)object;
        printf("properties count %d\n", (int) CFDictionaryGetCount(properties));
        data = IOCFSerialize(properties, kNilOptions);
	if(data) {
            printf("properties = \"%s\"\n", CFDataGetBytePtr(data));
            CFRelease(data);
	}
	CFRelease(properties);

        kr = IOAudioIsOutput( obj, &isOut);
        printf("IsOut:%d, kr = %d\n", isOut, kr);
        if(kIOReturnSuccess == kr && 1 == isOut) {
            printf("=%d, connecting\n", kr);
            kr = IOServiceOpen( obj,
                            mach_task_self(),
                            0, // Fix!!!
                            &fb);
            printf("=%d\n", kr);
            if(kIOReturnSuccess == kr)
                break;	// Got an output stream.
        }
    }
    printf("=%d, IORegistryDisposeEnumerator\n", kr);
    kr = IOObjectRelease( iter);

    if(1 == isOut) {
	int old;

        printf("=%d, map fb DMA Buffer\n", kr);
        kr = IOMapMemory( fb, kSampleBuffer, mach_task_self(),
                        &bufMem, &shmemSize, TRUE);
        printf("=%d, mapped @ %x Status\n", kr, bufMem);
        kr = IOMapMemory( fb, kStatus, mach_task_self(),
                &statMem, &shmemSize, TRUE);
        printf("=%d mapped @ %x\n", kr, statMem);
        status = (IOAudioStreamStatus *)statMem;
        printf("Erase flag is %d\n", status->fErases);

        kr = IOAudioSetErase(fb, !status->fErases, &old);
               printf("IOAudioSetErase return %d, oldVal %d, now %d\n",
			kr, old, status->fErases);
	{
            int dmaBlockStart, dmaBlockEnd;

            short *sampleBuffer;
            int div, mul;
            int size;
            int val;

            sampleBuffer = (short *)bufMem;

            // Put some sound in the buffer, except for the current block.
            div = status->fDataRate/status->fSampleSize/status->fChannels/freq;
            mul = 0x4000/div;
            size = status->fDataRate/status->fSampleSize/status->fChannels/10;
            dmaBlockStart = status->fCurrentBlock * status->fBlockSize;
            dmaBlockEnd = dmaBlockStart + status->fBlockSize;
            for(i=dmaBlockEnd/2; i<status->fBufSize/2; i += status->fChannels){
                if(!--size)
                    break;
                val = (size) % div;
                if(val > div/2)
                    val = div/2 - val;
                val = val*mul - 0x1000;
                for(j=0; j<status->fChannels; j++) {
                    sampleBuffer[i+j] = val;
                }
            }
            if(size) {
                int savedSize = size;
                for(i=0; i<dmaBlockStart/2; i += status->fChannels){
                    if(!--size)
                        break;
                    val = (size) % div;
                    if(val > div/2)
                        val = div/2 - val;
                    val = val*mul - 0x1000;
                    for(j=0; j<status->fChannels; j++) {
                        sampleBuffer[i+j] = val;
                    }
                }
                printf("Wrapped in buffer:%d\n", savedSize);
            }
	}
    }
}

void
setVal(char *compType, char *controlType, int val)
{
    kern_return_t	kr;
    io_connect_t	component;
    io_iterator_t	iter;
    io_object_t		obj;
    CFDictionaryRef 	properties;
    CFStringRef		compTypeStr;
    CFStringRef		controlTypeStr;

    printf("Setting %s of all %s components to %d\n",
				controlType, compType, val);

    compTypeStr = CFStringCreateWithCString(kCFAllocatorDefault, compType,
						kCFStringEncodingMacRoman);
    controlTypeStr = CFStringCreateWithCString(kCFAllocatorDefault, controlType,
						kCFStringEncodingMacRoman);

    kr = IORegistryCreateIterator(
        master_device_port, kIOAudioPlane, true, &iter);

    while(kIOReturnSuccess == kr) {
	CFStringRef	gotType;

        while( (obj = IOIteratorNext( iter ))) {
            if( IOObjectConformsTo( obj, "IOAudioComponent" ))
                break;

            IOObjectRelease( obj );
        }
        if(!obj)
            break;

	kr = IORegistryEntryCreateCFProperties(obj, (CFTypeRef *)&properties,
                                    kCFAllocatorDefault, kNilOptions);
        if (kr || !properties) {
            printf("IORegistryEntryCreateCFProperties failed, "
			"kr=%d, object=0x%x\n", kr, obj);
            break;
        }
	gotType = CFDictionaryGetValue(properties, CFSTR("Type"));
	if( gotType && (CFGetTypeID(gotType) != CFStringGetTypeID()))
	    gotType = 0;

        if(gotType && (kCFCompareEqualTo == CFStringCompare(gotType, compTypeStr, kNilOptions))) {
            CFDictionaryRef		controls;
            CFMutableDictionaryRef	changeControl;
            CFNumberRef			changeVal;
            printf("Found a %s component\n", compType);

            controls = CFDictionaryGetValue(properties, CFSTR("Controls"));
            if( CFGetTypeID(controls) != CFDictionaryGetTypeID())
                controls = 0;

            if (controls) {
                changeControl = (CFMutableDictionaryRef)
				CFDictionaryGetValue(controls, controlTypeStr);
                if( changeControl && (CFGetTypeID(changeControl) != CFDictionaryGetTypeID()))
                    changeControl = 0;

		if (changeControl) {
                    printf("With a %s control\n", controlType);
		    changeVal = CFDictionaryGetValue(changeControl, CFSTR("Val"));
                    if (CFGetTypeID(changeVal) != CFNumberGetTypeID())
                        changeVal = 0;

                    if (changeVal) {
			SInt32	current;
			CFNumberGetValue(changeVal, kCFNumberSInt32Type, &current);
                        printf("Current value %d\n", (int) current);

			changeVal = CFNumberCreate(kCFAllocatorDefault,
							kCFNumberSInt32Type, (SInt32 *)&val);
                        CFDictionarySetValue(changeControl, CFSTR("Val"), changeVal);

                        kr = IOServiceOpen( obj, mach_task_self(), 0, &component);
                        if(kr) {
                            printf("IOServiceOpen failed: 0x%x\n", kr);
                            break;
                        }
                        kr = IOConnectSetCFProperties(component, properties);

                        if(kr) {
                            printf("IOConnectSetCFProperties failed: 0x%x\n", kr);
                            break;
                        }
                        IOServiceClose(component);
                    }
                }
            }
        }
        CFRelease(properties);
    }
    kr = IOObjectRelease( iter);

    CFRelease(compTypeStr);
    CFRelease(controlTypeStr);
}

void testNotify(char *compType, char *inputType)
{
    kern_return_t	kr;
    io_connect_t	component;
    io_iterator_t	iter;
    io_object_t		obj;
    CFDictionaryRef	properties;
    CFStringRef		compTypeStr;
    CFStringRef		inputTypeStr;
    mach_port_t		notifyPort;
    struct {
        IOAudioNotifyMsg	msg;
        mach_msg_trailer_t	trailer;
    } grr;

    printf("Waiting for input %s of %s to change\n", inputType, compType);
    kr = mach_port_allocate(mach_task_self(),
				MACH_PORT_RIGHT_RECEIVE, &notifyPort);
    if(kr != KERN_SUCCESS) {
        printf("mach_port_allocate failed:%d\n", kr);
        return;
    }

    compTypeStr = CFStringCreateWithCString(kCFAllocatorDefault, compType,
						kCFStringEncodingMacRoman);
    inputTypeStr = CFStringCreateWithCString(kCFAllocatorDefault, inputType,
						kCFStringEncodingMacRoman);

    kr = IORegistryCreateIterator(
        master_device_port, kIOAudioPlane, true, &iter);

    while(kIOReturnSuccess == kr) {
	CFStringRef	gotType;

        while( (obj = IOIteratorNext( iter ))) {
            if( IOObjectConformsTo( obj, "IOAudioComponent" ))
                break;

            IOObjectRelease( obj );
        }
        if(!obj)
            break;

        kr = IORegistryEntryCreateCFProperties(obj, (CFTypeRef *) &properties,
                                    kCFAllocatorDefault, kNilOptions);
        if (kr || !properties) {
            printf("IORegistryEntryCreateCFProperties failed, "
			"kr=%d, object=0x%x\n", kr, obj);
            break;
        }

	gotType = CFDictionaryGetValue(properties, CFSTR("Type"));
	if( gotType && (CFGetTypeID(gotType) != CFStringGetTypeID()))
	    gotType = 0;

        if(gotType && (kCFCompareEqualTo == CFStringCompare(gotType, compTypeStr, kNilOptions))) {
	    CFDataRef		data;
            CFDictionaryRef	inputs;

            printf("Found a %s component\n", compType);

            printf("properties count %d\n", (int) CFDictionaryGetCount(properties));
            data = IOCFSerialize(properties, kNilOptions);
            if(data) {
                printf("properties = \"%s\"\n", CFDataGetBytePtr(data));
                CFRelease(data);
            }

            inputs = CFDictionaryGetValue(properties, CFSTR("Inputs"));
            if( inputs && (CFGetTypeID(inputs) != CFDictionaryGetTypeID()))
                inputs = 0;

	    if( inputs && CFDictionaryGetValue(inputs, inputTypeStr)) {

                printf("With a %s input\n", inputType);
                kr = IOServiceOpen( obj, mach_task_self(), 0, &component);
                if(kr) {
                    printf("IOServiceOpen failed: 0x%x\n", kr);
                    break;
                }

                kr = IOConnectSetNotificationPort(component,
				kIOAudioInputNotification, notifyPort, 12345);
                printf("IOConnectSetNotificationPort: %d\n", kr);

                kr = mach_msg(&grr.msg.h, MACH_RCV_MSG,
                            0, sizeof(grr), notifyPort, 0, MACH_PORT_NULL);
                if(kr != KERN_SUCCESS) {
                    printf("mach_msg failed:%d\n", kr);
                    //return;
                }
                printf("Got message, id=%d, refCon = %ld\n",
			grr.msg.h.msgh_id, grr.msg.refCon);
             
                IOServiceClose(component);
            }
        }
	CFRelease(properties);
    }
    kr = IOObjectRelease( iter);

    CFRelease(compTypeStr);
    CFRelease(inputTypeStr);
}

int
main(int argc, char **argv)
{
    kern_return_t	kr;
    int 		val;
    int freq = 440;

    /*
          * Get master device port
          */
    kr = IOMasterPort(bootstrap_port,
                              &master_device_port);

    if (kr != KERN_SUCCESS) {
        printf("IOMasterPort: %x\n", kr);
        return kr;
    }

    switch (argc) {
        case 2:
            freq = atoi(argv[1]);
        case 1:
            printf("Starting Tests, freq = %d\n", freq);
            TestSound(freq);
            break;

        case 3:
            testNotify(argv[1], argv[2]);
            break;
        case 4:
            val = atoi(argv[3]);
            setVal(argv[1], argv[2], val);
            break;
    }
    return(0);
}

