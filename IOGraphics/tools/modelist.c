/*cc -o /tmp/modelist modelist.c -framework IOKit -framework ApplicationServices -Wall -g
*/

#include <mach/mach.h>
#include <mach/thread_switch.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>

#include <CoreFoundation/CoreFoundation.h>
#include <ApplicationServices/ApplicationServices.h>

#include <IOKit/IOKitLib.h>
#include <libkern/OSByteOrder.h>
#include <IOKit/IOMessage.h>
#include <IOKit/IOCFURLAccess.h>
#include <IOKit/graphics/IOGraphicsLib.h>
#include <IOKit/graphics/IOGraphicsLibPrivate.h>

#include <assert.h>

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

struct IOFBModeList
{
    CFBundleRef                 bundle;
    io_service_t                framebuffer;

    CFMutableDictionaryRef      kernelInfo;
    CFMutableDictionaryRef      modes;
    CFMutableArrayRef           modesArray;
    CFMutableDictionaryRef      overrides;

    Boolean                     suppressRefresh;
    Boolean                     detailedRefresh;

    IOItemCount                 safemodeCount;
    IOItemCount                 builtinCount;
    IOItemCount                 televisionCount;
    IOItemCount                 simulscanCount;
    IOItemCount                 maxRefreshDigits;

    CFMutableArrayRef           refreshNames;

    Boolean                     refreshList;
    uint32_t                    significantFlags;
};
typedef struct IOFBModeList * IOFBModeListRef;

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

struct ModeInfo
{
    IODisplayModeInformation *  info;
    UInt32                      index;
    UInt32                      cgIndex;
    SInt32                      numRefresh;
    SInt32                      numSafe;
    SInt32                      numPreset;
    UInt32                      digits;
    Boolean                     notReco;
};
typedef struct ModeInfo ModeInfo;

enum
{
    kCompareRefresh = 0x00000001,
    kCompareAll     = 0xffffffff,
};

static int
CompareModes(IOFBModeListRef modeListRef,
            IODisplayModeInformation * left, IODisplayModeInformation * right,
            IOOptionBits compare)
{
    UInt32 leftFlags, rightFlags, differFlags;

    if (!left)
        return (1);
    else if (!right)
        return (-1);
    
    leftFlags = left->flags;
    rightFlags = right->flags;
    differFlags = leftFlags ^ rightFlags;

    if (modeListRef->simulscanCount && (kDisplayModeSimulscanFlag & differFlags))
    {
        if (kDisplayModeSimulscanFlag & leftFlags)
            return (1);
        else
            return (-1);
    }
    if (modeListRef->builtinCount && (kDisplayModeBuiltInFlag & differFlags))
    {
        if (kDisplayModeBuiltInFlag & leftFlags)
            return (1);
        else
            return (-1);
    }

    if (kDisplayModeTelevisionFlag & leftFlags & rightFlags)
    {
        // both TV, order ntsc first
        if ((left->refreshRate < 55*65536) && (right->refreshRate > 55*65536))
            return (1);
        else if ((left->refreshRate > 55*65536) && (right->refreshRate < 55*65536))
            return (-1);
    }

    if (left->nominalWidth > right->nominalWidth)
        return (1);
    else if (left->nominalWidth != right->nominalWidth)
        return (-1);
    if (left->nominalHeight > right->nominalHeight)
        return (1);
    else if (left->nominalHeight != right->nominalHeight)
        return (-1);

    if (kDisplayModeStretchedFlag & differFlags)
    {
        if (kDisplayModeStretchedFlag & leftFlags)
            return (1);
        else
            return (-1);
    }

    if (kDisplayModeInterlacedFlag & differFlags)
    {
        if (kDisplayModeInterlacedFlag & leftFlags)
            return (1);
        else
            return (-1);
    }

    if (compare & kCompareRefresh)
    {
        if (left->refreshRate > right->refreshRate)
            return (1);
        else if (left->refreshRate != right->refreshRate)
            return (-1);
    }

    return (0);
}

static int
qsort_cmp(void * ref, const void * _left, const void * _right)
{
    IOFBModeListRef modeListRef = (IOFBModeListRef) ref;

    IODisplayModeInformation * left  = ((ModeInfo *) _left)->info;
    IODisplayModeInformation * right = ((ModeInfo *) _right)->info;

    return (CompareModes(modeListRef, left, right, kCompareAll));
}



#define k256ColorString                 "256 Colors" 
#define kThousandsString                "Thousands" 
#define kMillionsString                 "Millions"
#define kTrillionsString                "Trillions"

#define kPALString                      "PAL"
#define kNTSCString                     "NTSC"

#define kNoRefreshRateString            "n/a"

#define kStretchedString                "stretched"
#define kSimulscanString                "simulscan"
#define kInterlacedString               "interlaced"
#define kExternalString                 "external"

#define kRefreshString                  "%%.%ldf Hertz"
#define kTVRefreshString                "%%.%ldf Hertz (%%@)"

#define kHxVString                      "%d x %d"
#define kHxVpString                     "%d x %dp"
#define kHxViString                     "%d x %di"

#define kNoHertz0FlagString             "%@"
#define kNoHertz1FlagString             "%@ (%@)"
#define kNoHertz2FlagString             "%@ (%@, %@)"
#define kNoHertz3FlagString             "%@ (%@, %@, %@)"
#define kNoHertz4FlagString             "%@ (%@, %@, %@, %@)"
#define kNoHertz5FlagString             "%@ (%@, %@, %@, %@, %@)"

#define kHertz0FlagString               "%%@, %%.%ldf Hz"
#define kHertz1FlagString               "%%@, %%.%ldf Hz (%%@)"
#define kHertz2FlagString               "%%@, %%.%ldf Hz (%%@, %%@)"
#define kHertz3FlagString               "%%@, %%.%ldf Hz (%%@, %%@, %%@)"
#define kHertz4FlagString               "%%@, %%.%ldf Hz (%%@, %%@, %%@, %%@)"
#define kHertz5FlagString               "%%@, %%.%ldf Hz (%%@, %%@, %%@, %%@, %%@)"
                
CFStringRef
TimingName(IOFBModeListRef modeListRef, IODisplayModeInformation * info, CFIndex * refRateIndex)
{

    CFStringRef formatStr, formatStr2, refStr, resStr, finalStr;
    CFStringRef palNTSCStr, stretchedStr, simulscanStr, interlacedStr, externalStr;
    uint32_t flags, flagCount;
    CFStringRef flagStrs[5] = { 0 };
    CFIndex k, count;

    float refRate = info->refreshRate / 65536.0;

    flagCount = 0;

    if (kDisplayModeTelevisionFlag & info->flags)
    {
        palNTSCStr = refRate < 55.0 ? CFSTR(kPALString) : CFSTR(kNTSCString);
        palNTSCStr = CFBundleCopyLocalizedString(modeListRef->bundle, palNTSCStr, palNTSCStr, 0);
    }
    else
        palNTSCStr = 0;

    stretchedStr  = CFBundleCopyLocalizedString(modeListRef->bundle, CFSTR(kStretchedString), CFSTR(kStretchedString), 0);
    simulscanStr  = CFBundleCopyLocalizedString(modeListRef->bundle, CFSTR(kSimulscanString), CFSTR(kSimulscanString), 0);
    interlacedStr = CFBundleCopyLocalizedString(modeListRef->bundle, CFSTR(kInterlacedString), CFSTR(kInterlacedString), 0);
    externalStr   = CFBundleCopyLocalizedString(modeListRef->bundle, CFSTR(kExternalString), CFSTR(kExternalString), 0);

    if (modeListRef->refreshList)
    {

        if (modeListRef->suppressRefresh)
        {
            refStr = CFSTR(kNoRefreshRateString);
            refStr = CFBundleCopyLocalizedString(modeListRef->bundle, refStr, refStr, 0);
        }
        else
        {
            if (kDisplayModeTelevisionFlag & info->flags)
                formatStr = CFSTR(kTVRefreshString);
            else
                formatStr = CFSTR(kRefreshString);

            formatStr = CFBundleCopyLocalizedString(modeListRef->bundle, formatStr, formatStr, 0);

            formatStr2 = CFStringCreateWithFormat(kCFAllocatorDefault, NULL,
                                    formatStr, 
                                    modeListRef->maxRefreshDigits);
            refStr = CFStringCreateWithFormat(kCFAllocatorDefault, NULL,
                    formatStr2, refRate,
                    palNTSCStr );
            CFRelease(formatStr);
        }
    
        count = CFArrayGetCount(modeListRef->refreshNames);
        for (k = 0;
            (k < count) && !CFEqual(refStr, CFArrayGetValueAtIndex(modeListRef->refreshNames, k));
                k++)    {}
    
        if (k == count)
            CFArrayAppendValue(modeListRef->refreshNames, refStr);
        CFRelease(refStr);

        if (refRateIndex)
            *refRateIndex = k;
    }

    flags = modeListRef->significantFlags & info->flags;

    if (!modeListRef->refreshList && (kDisplayModeTelevisionFlag & info->flags))
        flagStrs[flagCount++] = palNTSCStr;

    if (flags & kDisplayModeStretchedFlag)
        flagStrs[flagCount++] = stretchedStr;

    if (flags & kDisplayModeSimulscanFlag)
        flagStrs[flagCount++] = simulscanStr;

    if ((kDisplayModeBuiltInFlag & modeListRef->significantFlags)
     &&  !(flags & kDisplayModeBuiltInFlag))
        flagStrs[flagCount++] = externalStr;

    if (kDisplayModeInterlacedFlag ==
            (flags & (kDisplayModeInterlacedFlag | kDisplayModeTelevisionFlag)))
        flagStrs[flagCount++] = interlacedStr;

    
    resStr = CFStringCreateWithFormat(kCFAllocatorDefault, NULL,
                CFSTR(kHxVString),
                info->nominalWidth, info->nominalHeight);

    if (modeListRef->refreshList || modeListRef->suppressRefresh)
    {
        switch (flagCount)
        {
        case 0: formatStr = CFSTR(kNoHertz0FlagString); break;
        case 1: formatStr = CFSTR(kNoHertz1FlagString); break;
        case 2: formatStr = CFSTR(kNoHertz2FlagString); break;
        case 3: formatStr = CFSTR(kNoHertz3FlagString); break;
        case 4: formatStr = CFSTR(kNoHertz4FlagString); break;
        default:
        case 5: formatStr = CFSTR(kNoHertz5FlagString); break;
        }

        formatStr = CFBundleCopyLocalizedString(modeListRef->bundle, formatStr, formatStr, 0);

        finalStr = CFStringCreateWithFormat(kCFAllocatorDefault, NULL,
                        formatStr, 
                        resStr,
                        flagStrs[0],
                        flagStrs[1],
                        flagStrs[2],
                        flagStrs[3],
                        flagStrs[4]);
    }
    else
    {
        switch (flagCount)
        {
        case 0: formatStr2 = CFSTR(kHertz0FlagString); break;
        case 1: formatStr2 = CFSTR(kHertz1FlagString); break;
        case 2: formatStr2 = CFSTR(kHertz2FlagString); break;
        case 3: formatStr2 = CFSTR(kHertz3FlagString); break;
        case 4: formatStr2 = CFSTR(kHertz4FlagString); break;
        default:
        case 5: formatStr2 = CFSTR(kHertz5FlagString); break;
        }

        formatStr2 = CFBundleCopyLocalizedString(modeListRef->bundle, formatStr2, formatStr2, 0);

        formatStr = CFStringCreateWithFormat(kCFAllocatorDefault, NULL,
                                formatStr2, 
                                modeListRef->maxRefreshDigits);

        CFRelease(formatStr2);

        finalStr = CFStringCreateWithFormat(kCFAllocatorDefault, NULL,
                        formatStr, 
                        resStr,
                        refRate,
                        flagStrs[0],
                        flagStrs[1],
                        flagStrs[2],
                        flagStrs[3],
                        flagStrs[4]);
    }

    CFRelease(formatStr);

    if (palNTSCStr)
        CFRelease(palNTSCStr);
    if (stretchedStr)
        CFRelease(stretchedStr);
    if (simulscanStr)
        CFRelease(simulscanStr);
    if (interlacedStr)
        CFRelease(interlacedStr);
    if (externalStr)
        CFRelease(externalStr);

    return (finalStr);
}

kern_return_t
ModeList(IOFBModeListRef modeListRef, Boolean reco)
{
    CFMutableDictionaryRef      dict;
    CFMutableArrayRef           array;
    CFDataRef                   data;
    SInt32                      i, j, k;
    CFIndex                     modeCount, newCount, cgIndex = 0;
    IODisplayModeInformation *  info;
    ModeInfo *                  modeArray;

    do
    {
        dict = CFDictionaryCreateMutable( kCFAllocatorDefault, (CFIndex) 0,
                                                (CFDictionaryKeyCallBacks *) 0,
                                                &kCFTypeDictionaryValueCallBacks );
        modeListRef->modes = dict;
    
        dict = (CFMutableDictionaryRef) IORegistryEntryCreateCFProperty(
                                            modeListRef->framebuffer, 
                                            CFSTR(kIOFBConfigKey),
                                            kCFAllocatorDefault, kNilOptions);
        if (!dict)
            break;
        array = (CFMutableArrayRef) CFDictionaryGetValue(dict, CFSTR(kIOFBModesKey));
        if (!array)
            break;

        // pick up existing config
        modeListRef->kernelInfo = dict;
        CFRetain(array);
        modeListRef->modesArray = array;

        modeListRef->suppressRefresh = (0 != CFDictionaryGetValue(dict, CFSTR("IOFB0Hz")));

        modeListRef->detailedRefresh = (0 != CFDictionaryGetValue(dict, CFSTR("IOFBmHz")));

        modeCount  = CFArrayGetCount( modeListRef->modesArray );
        newCount = modeCount;

        modeArray = calloc(modeCount, sizeof(ModeInfo));

        for( i = 0; i < modeCount; i++ )
        {
            const void * key;
            CFNumberRef  num;

            dict = (CFMutableDictionaryRef) CFArrayGetValueAtIndex( modeListRef->modesArray, i );
            num = CFDictionaryGetValue( dict, CFSTR(kIOFBModeIDKey) );
            CFNumberGetValue( num, kCFNumberSInt32Type, (SInt32 *) &key );
            CFDictionarySetValue( modeListRef->modes, key, dict );

            if (!dict)
                break;
            data = CFDictionaryGetValue(dict, CFSTR(kIOFBModeDMKey));
            if (!data)
                break;

            info = (IODisplayModeInformation *) CFDataGetBytePtr(data);

//          if (info->flags & kDisplayModeNeverShowFlag)
//              continue;

            modeArray[i].index = i;
            modeArray[i].info  = info;
            modeArray[i].cgIndex = cgIndex;
            cgIndex += info->maxDepthIndex + 1;

#if 0
            printf("%ld: %ld x %ld  @ %f Hz\n", modeArray[i].cgIndex, 
                    info->nominalWidth, info->nominalHeight, 
                    info->refreshRate / 65536.0);
#endif

            if (info->flags & kDisplayModeSafeFlag)
                modeListRef->safemodeCount++;

//              if (info->flags & kDisplayModeAlwaysShowFlag)
//              if (info->flags & kDisplayModeDefaultFlag)

            if (info->flags & kDisplayModeSimulscanFlag)
                modeListRef->simulscanCount++;
            if (info->flags & kDisplayModeBuiltInFlag)
                modeListRef->builtinCount++;
            if (info->flags & kDisplayModeTelevisionFlag)
                modeListRef->televisionCount++;
        }


        qsort_r(modeArray, modeCount, sizeof(modeArray[0]), modeListRef, &qsort_cmp);


#define discard()       { modeArray[i].notReco = true; continue; }


        // group refresh rates

        {
            UInt32 lastSame;

            lastSame = 0;
            for (i = 0; i < modeCount; i++)
            {
                if (i != lastSame)
                {
                    if (0 == CompareModes(modeListRef, modeArray[lastSame].info, modeArray[i].info, 0))
                    {
                        // number that follow (total - 1)
                        modeArray[lastSame].numRefresh++;
    
                        modeArray[i].numRefresh = lastSame - i;
                    }
                    else
                        lastSame = i;
                }
                if (modeArray[i].info->flags & kDisplayModeSafeFlag)
                    modeArray[lastSame].numSafe++;

                if (!(modeArray[i].info->flags & kDisplayModeNotPresetFlag))
                    modeArray[lastSame].numPreset++;
            }
        }


        if (reco)
        {
            // prune with safety / not preset

            for( i = 0; i < modeCount; i++ )
            {
                info = modeArray[i].info;
                do 
                {
//                  if (modeListRef->safemodeCount
//                   && !(info->flags & (kDisplayModeSafeFlag | kDisplayModeTelevisionFlag)))
//                      discard();

//                  if (info->flags & kDisplayModeNotPresetFlag)
//                      discard();
                }
                while (false);
            }
        }

        // prune refresh rates
        if (reco)
        {
            for( i = 0; i < modeCount; i += modeArray[i].numRefresh + 1 )
            {
                info = modeArray[i].info;
                do 
                {
                    if (info->flags & kDisplayModeTelevisionFlag)
                        continue;

//                  if (modeListRef->safemodeCount)
                    {
                        // keep only highest
                        Boolean haveHighestReco = false;
                        for (j = i + modeArray[i].numRefresh; j >= i; j--)
                        {
                            if (haveHighestReco)
                                modeArray[j].notReco = true;
                            else {

                                if ((!modeArray[i].numPreset)
                                || (!(modeArray[j].info->flags & kDisplayModeNotPresetFlag)))
                                {
                                    uint32_t target;
                                    target = (modeArray[j].info->flags & kDisplayModeSafeFlag) 
                                                ? (86 << 16) : (86 << 16);

                                    haveHighestReco = !modeArray[j].notReco
                                                    && (modeArray[j].info->refreshRate < target);
                                }
                                if (!haveHighestReco)
                                    modeArray[j].notReco = true;
                            }
                        }
                        continue;
                    }
                }
                while (false);
            }
        }
        // <reco/>

        // unique refresh rates

        for( i = 0; i < modeCount; i += modeArray[i].numRefresh + 1 )
        {
            float ref1, ref2, mult;

            modeArray[i].digits = 0; //modeListRef->maxRefreshDigits;
            mult = 1.0;

            for (j = i; j < (i + modeArray[i].numRefresh + 1); j++)
            {
                if (modeArray[j].notReco)
                    continue;
                ref1 = modeArray[j].info->refreshRate / 65536.0;
                for (k = i; k < (i + modeArray[i].numRefresh + 1); k++)
                {
                    if (k == j)
                        continue;
                    if (modeArray[k].notReco)
                        continue;
                    ref2 = modeArray[k].info->refreshRate / 65536.0;
                    while (modeArray[i].digits < 5)
                    {
//if (modeArray[i].digits)
//    printf("-----> %f, %f, %f, %f\n", ref1, ref2, roundf(ref1 * mult), roundf(ref2 * mult));
                        if (roundf(ref1 * mult) != roundf(ref2 * mult))
                            break;
                        modeArray[i].digits++;
                        mult *= 10.0;
                    }
                }
            }

            if (modeArray[i].digits > modeListRef->maxRefreshDigits)
                modeListRef->maxRefreshDigits = modeArray[i].digits;
        }
        // <unique/>

        printf("\nOut:\n");
        
        modeListRef->refreshNames = CFArrayCreateMutable( kCFAllocatorDefault, 0,
                                             &kCFTypeArrayCallBacks );

        modeListRef->refreshList = false;

        do
        {
            modeListRef->significantFlags = 
                        1 * kDisplayModeStretchedFlag 
                        | 1 * kDisplayModeSimulscanFlag
                        | 0 * kDisplayModeBuiltInFlag;
    
            printf("-------------\n");
            for (i = 0; i < modeCount; i += modeArray[i].numRefresh + 1)
            {
                CFStringRef name;
                CFIndex     idx;
                CFIndex mask = 0;
    
                info = modeArray[i].info;
    
    #if 0
                printf("%ld x %ld \n", 
                        info->nominalWidth, info->nominalHeight);
    
                printf("numRefresh %d, numSafe %d, numPreset %d\n", 
                        modeArray[i].numRefresh,
                        modeArray[i].numSafe,
                        modeArray[i].numPreset);
    #endif
    
                for (j = i; j < (i + modeArray[i].numRefresh + 1); j++)
                {
                    info = modeArray[j].info;
    
                    name = TimingName(modeListRef, info, &idx);
                    if (!modeListRef->refreshList || (j == i))
                    {
                        if (!modeListRef->refreshList && modeArray[j].notReco)
                            printf("*");
                        printf("%s", CFStringGetCStringPtr(name, kCFStringEncodingMacRoman));
                        if (!modeListRef->refreshList)
                            printf("\n");
                    }
                    if (modeListRef->refreshList)
                    {
                        printf(" %c[%ld]", modeArray[j].notReco ? '*' : ' ', idx);
                        mask |= (1 << idx);
                    }
    
                    CFRelease(name);
    #if 0
                    printf("   %s%s%s",
                        (info->flags & kDisplayModeSafeFlag) ? "safe " : "",
                        (info->flags & kDisplayModeDefaultFlag) ? "default " : "",
                        (info->flags & kDisplayModeNotPresetFlag) ? "notpreset " : ""
                    );
    #endif
                }
                if (modeListRef->refreshList)
                    printf("\n");
            }
            if (modeListRef->refreshList)
            {
                CFShow(modeListRef->refreshNames);
                break;
            }
            modeListRef->refreshList = true;
        }
        while (true);
    }
    while (false);

//      CFShow(modeListRef->modesArray);
    return( kIOReturnSuccess );
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

int main( int argc, char * argv[] )
{
    kern_return_t kr;
    io_string_t   path;
        CFURLRef url;
    CFIndex       i;
    CGError             err;
    CGDisplayCount      max;
    CGDirectDisplayID   displayIDs[8];

    err = CGGetOnlineDisplayList(8, displayIDs, &max);
    if(err != kCGErrorSuccess)
        exit(1);
    if(max > 8)
        max = 8;



    for(i = 0; i < max; i++ )
    {
        struct IOFBModeList _xxx = { 0 };
        IOFBModeListRef modeListRef = &_xxx;
        modeListRef->framebuffer = CGDisplayIOServicePort(displayIDs[i]);

    
        url = CFURLCreateWithFileSystemPath(
            kCFAllocatorDefault,
            CFSTR("/System/Library/Frameworks/IOKit.framework"),
            kCFURLPOSIXPathStyle, true);
        if (url)
            modeListRef->bundle = CFBundleCreate(kCFAllocatorDefault, url);

if (!modeListRef->bundle) exit(1);

        kr = IORegistryEntryGetPath(modeListRef->framebuffer, kIOServicePlane, path);
        assert( KERN_SUCCESS == kr );
        printf("\nDisplay %#x: %s\n", displayIDs[i], path);

        ModeList(modeListRef, true || true);
    }

    exit(0);
    return(0);
}

