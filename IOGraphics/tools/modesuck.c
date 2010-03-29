// cc -o /tmp/modesuck -g modesuck.c -framework ApplicationServices -framework IOKit -Wall


#include <mach/mach.h>
#include <mach/thread_switch.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <math.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include <IOKit/IOKitLib.h>
#include <IOKit/graphics/IOGraphicsTypesPrivate.h>
#include <ApplicationServices/ApplicationServices.h>


__private_extern__ IOReturn
readFile(const char *path, vm_offset_t * objAddr, vm_size_t * objSize)
{
    int fd;
    int err;
    struct stat stat_buf;

    *objAddr = 0;
    *objSize = 0;

    if((fd = open(path, O_RDONLY)) == -1)
        return errno;

    do {
        if(fstat(fd, &stat_buf) == -1) {
            err = errno;
            continue;
        }
        if (0 == (stat_buf.st_mode & S_IFREG)) 
        {
            *objAddr = 0;
            *objSize = 0;
            err = kIOReturnNotReadable;
            continue;
        }
        *objSize = stat_buf.st_size;

        if( KERN_SUCCESS != map_fd(fd, 0, objAddr, TRUE, *objSize)) {
            *objAddr = 0;
            *objSize = 0;
            err = errno;
            continue;
        }

        err = kIOReturnSuccess;

    } while( false );

    close(fd);

    return( err );
}

__private_extern__ CFMutableDictionaryRef
readPlist( const char * path, UInt32 key )
{
    IOReturn                    err;
    vm_offset_t                 bytes;
    vm_size_t                   byteLen;
    CFDataRef                   data;
    CFMutableDictionaryRef      obj = 0;

    err = readFile( path, &bytes, &byteLen );

    if( kIOReturnSuccess != err) 
        return (0);
    
    data = CFDataCreateWithBytesNoCopy( kCFAllocatorDefault,
                                (const UInt8 *) bytes, byteLen, kCFAllocatorNull );
    if( data) {
        obj = (CFMutableDictionaryRef) CFPropertyListCreateFromXMLData( kCFAllocatorDefault, data,
                                            kCFPropertyListMutableContainers,
                                            (CFStringRef *) NULL );
        CFRelease( data );
    }
    vm_deallocate( mach_task_self(), bytes, byteLen );

    return (obj);
}

int main(int argc, char * argv[])
{
    io_service_t                framebuffer;
    CGError                     err;
    int                         i;
    CGDisplayCount              max;
    CGDirectDisplayID           displayIDs[8];
    IODisplayModeInformation *  modeInfo;
    IODetailedTimingInformationV2 *timingInfo;

    CFDictionaryRef             dict;
    CFArrayRef                  modes;
    CFIndex                     count;
    CFDictionaryRef             mode;
    CFMutableDictionaryRef      result, stdModes, timingIDs;
    CFDataRef                   data;
    CFNumberRef                 num;
    CFStringRef                 cfStr;
    char                        key[64];
    const void *                keys[2];
    const void *                values[2];

    err = CGGetOnlineDisplayList(8, displayIDs, &max);
    if(err != kCGErrorSuccess)
        exit(1);
    if(max > 8)
        max = 8;

    for(i = 0; i < max; i++ ) {

        framebuffer = CGDisplayIOServicePort(displayIDs[i]);

        dict = IORegistryEntryCreateCFProperty(framebuffer, CFSTR(kIOFBConfigKey),
                                                kCFAllocatorDefault, kNilOptions);
        assert(dict);

        modes = CFDictionaryGetValue(dict, CFSTR(kIOFBModesKey));
        assert(modes);


        result = readPlist("/System/Library/Frameworks/IOKit.framework/"
                                    "Resources/IOGraphicsProperties.plist", 0);

        if (result)
        {
            stdModes = (CFMutableDictionaryRef) CFDictionaryGetValue(result, CFSTR("std-modes"));
            assert(stdModes);
            timingIDs = (CFMutableDictionaryRef) CFDictionaryGetValue(result, CFSTR("timing-ids"));
            assert(timingIDs);

            data = CFDictionaryGetValue(result, CFSTR("apple-edid"));
            if (data)
            {
                UInt32 ids[24] = { 0 };

                UInt32 * p = (UInt32 *) CFDataGetBytePtr(data);
                int i;

                for( i = 0; i < (CFDataGetLength(data)/4); i+=2)
                {
                    UInt32 bit, id;
                    id = p[i];
                    bit = p[i + 1] >> 16;
                    if( bit < 24)
                    {
                        bit = (0x10 - (bit & 0xf8)) | (bit & 7);

                        if( ids[bit])
                            printf("bit %ld, id %ld dup\n", bit, id);
                        else
                            ids[bit] = id;
                    }
                }

                data = CFDataCreate( kCFAllocatorDefault,
                                            (const UInt8 *) ids, sizeof(ids) );
                CFDictionarySetValue( result, CFSTR("established-ids"), data);
                CFRelease(data);
            }
        }
        else
        {
            result = CFDictionaryCreateMutable( kCFAllocatorDefault, 0,
                                        &kCFTypeDictionaryKeyCallBacks,
                                        &kCFTypeDictionaryValueCallBacks );
            assert(result);
    
            stdModes = CFDictionaryCreateMutable( kCFAllocatorDefault, 0,
                                        &kCFTypeDictionaryKeyCallBacks,
                                        &kCFTypeDictionaryValueCallBacks );
            assert(stdModes);
            timingIDs = CFDictionaryCreateMutable( kCFAllocatorDefault, 0,
                                        &kCFTypeDictionaryKeyCallBacks,
                                        &kCFTypeDictionaryValueCallBacks );
            assert(timingIDs);
        }

        count = CFArrayGetCount(modes);
        for (i = 0; i < count; i++)
        {
            SInt32 aid;

            mode = CFArrayGetValueAtIndex(modes, i);

            num = CFDictionaryGetValue(mode, CFSTR(kIOFBModeAIDKey));
            if (num)
                CFNumberGetValue( num, kCFNumberSInt32Type, &aid );
            else
                aid = timingInvalid;

            data = CFDictionaryGetValue(mode, CFSTR(kIOFBModeDMKey));
            if (!data)
                continue;
            modeInfo = (IODisplayModeInformation *) CFDataGetBytePtr(data);

            data = CFDictionaryGetValue(mode, CFSTR(kIOFBModeTMKey));
            if (!data)
                continue;
            timingInfo = (IODetailedTimingInformationV2 *) CFDataGetBytePtr(data);

            printf("%ldx%ld@%ld, %ld\n", modeInfo->nominalWidth, modeInfo->nominalHeight, ((modeInfo->refreshRate + 0x8000) >> 16), aid);

if( timingInfo->horizontalActive & 7) printf("horizontalActive & 7\n");
if( timingInfo->horizontalBlanking & 7) printf("horizontalBlanking & 7\n");
if( timingInfo->horizontalSyncOffset & 7) printf("horizontalSyncOffset & 7\n");
if( timingInfo->horizontalSyncPulseWidth & 7) printf("horizontalSyncPulseWidth & 7\n");


            if( (aid == timingInvalid)
             || (aid == timingInvalid_SM_T24)
             || (aid == timingApple_FixedRateLCD)
             || (aid == timingGTF_640x480_120hz)
             || (aid == timingAppleNTSC_ST)
             || (aid == timingAppleNTSC_FF)
             || (aid == timingAppleNTSC_STconv)
             || (aid == timingAppleNTSC_FFconv)
             || (aid == timingApplePAL_ST)
             || (aid == timingApplePAL_FF)
             || (aid == timingApplePAL_STconv)
             || (aid == timingApplePAL_FFconv)
             || (aid == timingSMPTE240M_60hz)
             || (aid == timingFilmRate_48hz)
             || (aid == timingApple_0x0_0hz_Offline))
             continue;

if(modeInfo->flags & (1<<kModeShowNever)) 
{
    printf("nv!\n");
    continue;
}

            if( true
             && (aid != timingApple_1024x768_75hz)
             )
            {

                if( aid == timingVESA_640x480_72hz)
                    modeInfo->refreshRate = 72 << 16;   // from 72.8

                sprintf(key, "%ld", ((modeInfo->nominalWidth << 20) | (modeInfo->nominalHeight << 8) | ((modeInfo->refreshRate + 0x8000) >> 16)));

                cfStr =  CFStringCreateWithCString( kCFAllocatorDefault, key,
                                                            kCFStringEncodingMacRoman );
                if (CFDictionaryGetValue( timingIDs, cfStr ))
                    printf("%ld timing id dup\n", aid);
                else
                {
                    printf("ADDING\n");
                    CFDictionarySetValue( timingIDs, cfStr, num );
                }
            }

            sprintf(key, "%ld", aid);

            cfStr =  CFStringCreateWithCString( kCFAllocatorDefault, key,
                                                        kCFStringEncodingMacRoman );

            keys  [0] = CFSTR(kIOFBModeTMKey);
            values[0] = data;
            keys  [1] = CFSTR(kIOFBModeAIDKey);
            values[1] = num;

            dict = CFDictionaryCreate( kCFAllocatorDefault, keys, values, 1, 
                                        &kCFTypeDictionaryKeyCallBacks,
                                        &kCFTypeDictionaryValueCallBacks );

            assert(dict);

            if (CFDictionaryGetValue( stdModes, cfStr ))
                printf("%ld timing id dup\n", aid);
            else
            {
                printf("ADDING\n");
                CFDictionarySetValue( stdModes, cfStr, dict );
            }
            CFRelease(dict);

        }

        CFDictionarySetValue(result, CFSTR("std-modes"), stdModes);
        CFDictionarySetValue(result, CFSTR("timing-ids"), timingIDs);

        data = CFPropertyListCreateXMLData( kCFAllocatorDefault, result );
        if (data)
        {
            char * str = (char *) CFDataGetBytePtr(data);
            str[CFDataGetLength(data)] = 0;
            printf( str );
        }

    }
    
    exit(0);
    return(0);
}
