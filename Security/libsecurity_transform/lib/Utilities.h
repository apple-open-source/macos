#ifndef __UTILITIES__
#define __UTILITIES__

#ifdef __cplusplus
extern "C" {
#endif


#include <CoreFoundation/CoreFoundation.h>


void MyDispatchAsync(dispatch_queue_t queue, void(^block)(void));
dispatch_queue_t MyDispatchQueueCreate(const char* name, dispatch_queue_attr_t attr);

CFErrorRef CreateGenericErrorRef(CFStringRef domain, int errorCode, const char* format, ...);
CFErrorRef CreateSecTransformErrorRef(int errorCode, const char* format, ...);
CFErrorRef CreateSecTransformErrorRefWithCFType(int errorCode, CFTypeRef errorMsg);

CFTypeRef DebugRetain(const void* owner, CFTypeRef type);
void DebugRelease(const void* owner, CFTypeRef type);

void transforms_bug(size_t line, long val) __attribute__((__noinline__));
    
// Borrowed form libdispatch, fastpath's x should normally be true; slowpath's x should normally be false.
// The compiler will generate correct code in either case, but it is faster if you hint right.   If you
// hint wrong it could be slower then no hints.
#define fastpath(x)        ((typeof(x))__builtin_expect((long)(x), ~0l))
#define slowpath(x)        ((typeof(x))__builtin_expect((long)(x), 0l))

/*
 * Borrowed (with minor changes) from libdispatch.
 *
 * For reporting bugs or impedance mismatches between SecTransform and external subsystems,
 * and for internal consistency failures. These do NOT abort(), and are always compiled
 * into the product.
 *
 * Libdispatch wraps all system-calls with assume() macros, and we ought to do the same
 * (but don't yet).
 */
#define transforms_assume(e)        ({        \
        typeof(e) _e = fastpath(e); /* always eval 'e' */        \
        if (!_e) {        \
            if (__builtin_constant_p(e)) {        \
                char __compile_time_assert__[(e) ? 1 : -1];        \
                (void)__compile_time_assert__;        \
            }        \
            transforms_bug(__LINE__, (long)_e);        \
        }        \
        _e;        \
    })

/* A lot of API return zero upon success and not-zero on fail. Let's capture and log the non-zero value */    
#define transforms_assume_zero(e)        ({        \
        typeof(e) _e = slowpath(e); /* always eval 'e' */        \
        if (_e) {        \
            if (__builtin_constant_p(e)) {        \
                char __compile_time_assert__[(e) ? -1 : 1];        \
                (void)__compile_time_assert__;        \
            }        \
            transforms_bug(__LINE__, (long)_e);        \
        }        \
        _e;        \
    })

#ifdef __cplusplus
};
#endif

#endif
