//
//  GTrace.hpp
//
//  Created by bparke on 3/17/17.
//

#ifndef GTrace_hpp
#define GTrace_hpp

#include <string.h>

#if defined(_KERNEL_) || defined (KERNEL)

#include <IOKit/IOLib.h>
#include <IOKit/IOService.h>
#include <IOKit/assert.h>
#include <IOKit/IOLocks.h>

#include <libkern/libkern.h>
#include <libkern/c++/OSContainers.h>
#include <libkern/c++/OSObject.h>
#include <libkern/OSByteOrder.h>

#include <sys/kdebug.h>
#include <sys/queue.h>
#include <sys/systm.h>
#include <sys/types.h>
#include <sys/sysctl.h>

#include <mach/vm_param.h>
#include <mach/mach_types.h>
extern "C" {
#include <machine/cpu_number.h>
}
#else /*defined(_KERNEL_) || defined (KERNEL)*/

#include <mutex>

#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>

#include <mach/mach_time.h>
#include <IOKit/IOReturn.h>

#endif /*defined(_KERNEL_) || defined (KERNEL)*/

#include "GTraceTypes.h"


#pragma mark - Components
#define kGTRACE_IODISPLAYWRANGLER                           0x0000000001ULL
#define kGTRACE_IODISPLAY                                   0x0000000002ULL
#define kGTRACE_IODISPLAYCONNECT                            0x0000000004ULL
#define kGTRACE_IOFBCONTROLLER                              0x0000000008ULL
#define kGTRACE_IOFRAMEBUFFER                               0x0000000010ULL
#define kGTRACE_IOFRAMEBUFFERPARAMETERHANDLER               0x0000000020ULL
#define kGTRACE_FRAMEBUFFER                                 0x0000000040ULL
#define kGTRACE_APPLEBACKLIGHT                              0x0000000080ULL
#define kGTRACE_IOFBUSERCLIENT                              0x0000000100ULL
#define kGTRACE_IOFBSHAREDUSERCLIENT                        0x0000000200ULL
#define kGTRACE_IOI2INTERFACEUSERCLIENT                     0x0000000400ULL
#define kGTRACE_IOI2INTERFACE                               0x0000000800ULL
#define kGTRACE_IOFBI2INTERFACE                             0x0000001000ULL
#define kGTRACE_IOBOOTFRAMEBUFFER                           0x0000002000ULL
#define kGTRACE_IONDRVFRAMEBUFFER                         	0x0000004000ULL
#define kGTRACE_IOACCELERATOR                               0x0000008000ULL
#define kGTRACE_IOACCELERATORUSERCLIENT                     0x0000010000ULL
#define kGTRACE_APPLEGRAPHICSDISPLAYPOLICY                  0x0000020000ULL
#define kGTRACE_APPLEGRAPHICSMUXCONTROL                     0x0000040000ULL
#define kGTRACE_APPLEGRAPHICSPOWERCONTROL                   0x0000080000ULL
// ...
#define kGTRACE_VENDOR_INTEL                                0x2000000000ULL
#define kGTRACE_VENDOR_AMD                                  0x4000000000ULL
#define kGTRACE_VENDOR_NVIDIA                               0x8000000000ULL


#pragma mark - Masks
#define         kGTRACE_COMPONENT_MASK                      0x000000FFFFFFFFFFULL       // 40 bits
#define         kTHREAD_ID_MASK                             0x0000000000FFFFFFULL       // 24 bits
#define         kGTRACE_REGISTRYID_MASK                     0x00000000FFFFFFFFULL       // 32 bits


// Argument Tag Bits
#define kGTRACE_ARGUMENT_Reserved1                          0x1000  // Future Use
#define kGTRACE_ARGUMENT_Reserved2                          0x2000  // Future Use
#define kGTRACE_ARGUMENT_STRING                             0x4000  // Argument is a string
#define kGTRACE_ARGUMENT_OBFUSCATE                          0x8000  // Arguments tagged with this bit are pointers and will be obfuscated when copied/printed.
#define kGTRACE_ARGUMENT_MASK                               (kGTRACE_ARGUMENT_Reserved1 | \
                                                            kGTRACE_ARGUMENT_Reserved2 | \
                                                            kGTRACE_ARGUMENT_STRING | \
                                                            kGTRACE_ARGUMENT_OBFUSCATE)


// Registry Properties
#if defined(_KERNEL_) || defined (KERNEL)
#define         kGTracePropertyTokens                       "GTrace_Tokens"
#define         kGTracePropertyTokenCount                   "GTrace_Count"
#define         kGTracePropertyTokenMarker                  "GTrace_Marker"
#endif /*defined(_KERNEL_) || defined (KERNEL)*/


#pragma mark - GTrace Marcos
// TODO: Provide sample of GTRACE() macro usage.
#define GTRACEOBJ                                           gGTrace                     // This will need to be defined in implementation
#define GTRACERECORDTOKEN                                   GTRACEOBJ->recordToken      // Define the object deref for the implementation
#define GPACKNODATA                                         0                           // 0 is a valid value, NoData used to distinguish
#define GPACKUINT8T(x, y)                                   ((((uint64_t)(y)) & 0x000000ff) << ((x) << 3))
#define GPACKUINT16T(x, y)                                  ((((uint64_t)(y)) & 0x0000ffff) << ((x) << 4))
#define GPACKUINT32T(x, y)                                  ((((uint64_t)(y)) & 0xffffffff) << ((x) << 5))
#define GPACKUINT64T(x)                                     (x)
#define GPACKBITS(x, y)                                     ((uint64_t)(y) & ((1ULL << (x)) - 1))
#define MAKEGTRACETAG(x)                                    static_cast<uint16_t>(x)
// FIXME: Tagging won't work with definitions.  Need to come up with a means to create TAG IDs on the fly.  Was: gTraceTag_##x
// Main tracing Macro
#define GTRACE(tag0, arg0, tag1, arg1, tag2, arg2, tag3, arg3) \
                                                            do{\
                                                                if(GTRACEOBJ){\
                                                                    GTRACERECORDTOKEN(__LINE__,\
                                                                    MAKEGTRACETAG(tag0),static_cast<uint64_t>(arg0),\
                                                                    MAKEGTRACETAG(tag1),static_cast<uint64_t>(arg1),\
                                                                    MAKEGTRACETAG(tag2),static_cast<uint64_t>(arg2),\
                                                                    MAKEGTRACETAG(tag3),static_cast<uint64_t>(arg3));\
                                                                }\
                                                            }while(0)


#if defined(_KERNEL_) || defined (KERNEL)

#define GTRACE_USING_SUPER                                  using super = OSObject
#define GTRACE_PUBLIC                                       : public OSObject
#define GTRACE_SERVICE                                      IOService
#define GTRACE_MALLOC(_s_)                                  IOMalloc(_s_)
#define GTRACE_SAFE_FREE(_p_,_s_)                           do{if(_p_){IOFree(_p_,_s_);_p_=NULL;}}while(0)
#define GTRACE_SLEEP(_d_)                                   do{IOSleep(_d_*1000);}while(0)
#define GTRACE_RAII_LOCK(inLck)                             GTraceLock lock(inLck)
#define GTRACE_RETAIN                                       retain()
#define GTRACE_RELEASE                                      release()
#define GTRACE_PRINTF                                       kprintf

#else /*defined(_KERNEL_) || defined (KERNEL)*/

#define GTRACE_USING_SUPER
#define GTRACE_PUBLIC
#define GTRACE_SERVICE                                      uintptr_t
#define GTRACE_MALLOC(_s_)                                  malloc(_s_)
#define GTRACE_SAFE_FREE(_p_,_s_)                           do{if(_p_){free((void *)_p_);_p_=NULL;}}while(0)
#define OSSafeReleaseNULL(_p_)                              do{if(_p_){_p_=NULL;}}while(0)
#define GTRACE_SLEEP(_d_)                                   do{sleep(_d_);}while(0)
#define GTRACE_RAII_LOCK(inLck)                             std::lock_guard<std::mutex> lock(inLck)
#define GTRACE_RETAIN                                       GTRACE_RAII_LOCK(fInUseLock)
#define GTRACE_RELEASE                                      
#define GTRACE_PRINTF                                       printf

#endif /*defined(_KERNEL_) || defined (KERNEL)*/


#pragma mark - GTrace Class
class GTrace GTRACE_PUBLIC
{
#if defined(_KERNEL_) || defined (KERNEL)
    OSDeclareDefaultStructors(GTrace);
#endif /*defined(_KERNEL_) || defined (KERNEL)*/

private:
    GTRACE_USING_SUPER;

public:
#if defined(_KERNEL_) || defined (KERNEL)
    virtual bool init() APPLE_KEXT_OVERRIDE;
    virtual void free() APPLE_KEXT_OVERRIDE;
#endif /*defined(_KERNEL_) || defined (KERNEL)*/

    /*! @function freeGTraceResources
     @abstract Disables token recording and frees any buffers allocated as the result of calling initWithDeviceCountAndID().
     @discussion Kernel clients are not required to call this API.  Kernel's free() will handle the disposal of resources allocated as the result of calling initWithDeviceCountAndID().
     */
    void freeGTraceResources( void );

    /*! @function initWithDeviceCountAndID
     @abstract Initialize the GTrace buffer for the associated provider and component.
     @discussion GTrace main registration function.  Must be called and return success before GTrace can be used.
     @param provider The owning GTRACE_SERVICE provider.  provider is not used for userland clients.
     @param lineCount The maximum number of lines to be recorded  lineCount should be power of two, else the value is rounded down to the nearest power of two.
     @param componentID The component's trace ID.  See kGTRACE_ components
     @result Return kIOReturnSuccess if GTrace was inititalized, else a descriptive error.
     */
    IOReturn initWithDeviceCountAndID( GTRACE_SERVICE * provider, const uint32_t lineCount, const uint64_t componentID );

    /*! @function recordToken
     @abstract Add the token data to the token stream
     @discussion GTrace supports tokenized tracing.  IOFramebuffer::recordToken can be used any client that has access to an IOFramebuffer object to insert a tokenized KTrace style trace into the trace buffer.
     @param line The line number associated with the token
     #param tag1 An implementation specific tag associated with arg1
     #param tag2 An implementation specific tag associated with arg2
     #param tag3 An implementation specific tag associated with arg3
     #param tag4 An implementation specific tag associated with arg4
     @param arg1 Component/implementation specific value.
     @param arg2 Component/implementation specific value.
     @param arg3 Component/implementation specific value.
     @param arg4 Component/implementation specific value.
     */
    void recordToken(const uint16_t line,
                     const uint16_t tag1, const uint64_t arg1,
                     const uint16_t tag2, const uint64_t arg2,
                     const uint16_t tag3, const uint64_t arg3,
                     const uint16_t tag4, const uint64_t arg4);

    /*! @function publishTokens
     @abstract Publish token data to provider registry.
     @discussion Publishes the tokens (kGTracePropertyTokens key), total token capacity (kGTracePropertyTokenCount key) and the last token processed (kGTracePropertyTokenMarker key) into the provider's registry.  Published tokens are removed from the registry when the GTrace object is freed.
     @result kIOReturn success if the properties where published, else a descriptive error.  Not supported in userland contexts.
     */
    IOReturn publishTokens( void );

    /*! @function copyTokens
     @abstract Copies the raw tokenized data from the internal buffer into a newly allocated buffer.
     @discussion Copies the tokens data from the internal buffers into the callee's buffer up to the buffer size as provided via the length parameter or the internal buffer size (which ever is less).  The token marker and token capacity can optionally be acquired via the tokenMarker and tokeCount parameters.  The buffer memory is not cleared before copy, therefore callers should ensure thier memory is pre-cleared.
     @param buffer Pointer to a pointer where the buffer address will be returned.  It is the clients responsibility to call kernel:IOFree() userland:free() on this memory when done with it.
     @param length Length of buffer allocated.
     @param tokenMarker The token line number for the last recorded token.
     @param tokenCount The total number of token lines in the buffer.
     @result kIOReturn success if the copy was successful, else a descriptive error.
     */
    IOReturn copyTokens( uintptr_t ** buffer,
                        uint32_t * length,
                        uint32_t * tokenMarker,
                        uint32_t * tokenCount );

    /*! @function printToken
     @abstract Prints a single token as a string of hex values.
     @discussion A simple function that prints the line as a string of hex values via printf (user land) or kprintf (kernel land)
     @param line the line number of the token buffer to be printed
     */
    IOReturn printToken( uint32_t line );

    /*! @function setTokens
     @abstract Updates the internal token buffer with the provided token data.
     @discussion Copies the data from buffer to the internal buffer size if the bufferSize is within the limits of the internal buffer.  The token data is not validated and assumed to be correct.
     @param buffer A pointer to a buffer of bufferSize that contains valid token data.
     @param bufferSize Length of buffer provided.
     */
    IOReturn setTokens( const uintptr_t * buffer, const uint32_t bufferSize );

private:
    /*! @function getLine
     @abstract Returns the current line index.
     @discussion Atomically increments the line index and returns the available index.
     */
    inline uint32_t getLine(void) {
        uint32_t    ln = fNextLine++;
        ln &= fLineMask;
        return (ln);
    }

    /*! @function sizeToLines
     @abstract Converts the byte buffer size into a power-of-two number of lines.
     @discussion Converts the byte buffer size into a power-of-two number of lines.
     */
    inline uint32_t sizeToLines( const uint32_t bufSize ) {
        uint32_t    lines = 0;
        if (bufSize > (kGTraceMaximumLineCount * sizeof(sGTrace))) {
            lines = kGTraceMaximumLineCount;
        } else if (bufSize >= sizeof(sGTrace)) {
            lines = (1 << (32 - (__builtin_clz(bufSize) + 1))) / sizeof(sGTrace);
        }
        return (lines);
    }

    /*! @function linesToSize
     @abstract Converts the number lines into a byte size.
     @discussion Converts the number lines into a byte size.
     */
    inline uint32_t linesToSize( const uint32_t lines ) {
        uint32_t    bufSize = 0;
        if (lines > kGTraceMaximumLineCount) {
            bufSize = kGTraceMaximumLineCount * sizeof(sGTrace);
        } else {
            bufSize = lines * sizeof(sGTrace);
            bufSize = (sizeToLines(bufSize) * sizeof(sGTrace));
        }
        return (bufSize);
    }

    /*! @function delayFor
     @abstract Delay for a number of seconds
     @discussion If token recording is active, delay for the define number of seconds or until token recording goes inactive, whichever comes first.
     */
    void delayFor(uint8_t seconds);

    /*! @function makeExternalReference
     @abstract Obfuscates kernel pointers
     @discussion If the argument tag specifies that the argument is to be obfuscated, then the obfuscation will occur when the pointer is printed or copied, but not when recorded.  This allows for core dump/kernel debugging with recorded pointers.
     */
    inline uint64_t makeExternalReference(const sGTrace& entry, const int tagIndex)
    {
        uint64_t ret = entry.traceEntry.args.ARGS.u64s[tagIndex];
#if defined(_KERNEL_) || defined (KERNEL)
        const auto tag = entry.traceEntry.argsTag.TAG.targ[tagIndex];
        if (kGTRACE_ARGUMENT_OBFUSCATE & tag) {
            vm_offset_t     vmRep = 0;
            vm_kernel_addrperm_external(static_cast<vm_offset_t>(ret), &vmRep);
            ret = (static_cast<uint64_t>(vmRep));
        }
#endif /*defined(_KERNEL_) || defined (KERNEL)*/
        return (ret);
    }

#if defined(_KERNEL_) || defined (KERNEL)
    struct GTraceLock
    {
        GTraceLock(IOLock *inLock) : fGTraceLock(inLock) { IOLockLock(fGTraceLock); }
        ~GTraceLock() { IOLockUnlock(fGTraceLock); }
    private:
        IOLock * const  fGTraceLock;
    };
#endif /*defined(_KERNEL_) || defined (KERNEL)*/

    
private:
    _Atomic(bool)           fInitialized;

#if defined(_KERNEL_) || defined (KERNEL)
    IOLock                  * fInUseLock;
#else /*defined(_KERNEL_) || defined (KERNEL)*/
    std::mutex              fInUseLock;
#endif /*defined(_KERNEL_) || defined (KERNEL)*/

    GTRACE_SERVICE          * fProvider;

    uint32_t                fLineMask;
    uint32_t                fLineCount;

    _Atomic(uint32_t)       fNextLine;

    sGTrace                 * fBuffer;
    uint32_t                fBufferSize;

    uint64_t                fComponentID;
    uint64_t                fObjectID;
};


#endif /* GTrace_h */
