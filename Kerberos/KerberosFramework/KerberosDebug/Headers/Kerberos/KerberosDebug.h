/*
 * Shim header file to define a bunch of stuff that is normally in the MIT Debugging Library
 */
 
#ifndef __KERBEROSDEBUG
#define __KERBEROSDEBUG

#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef MACDEV_DEBUG
#define MACDEV_DEBUG 1
#endif

void dprintf (const char *format, ...);
void dprintmem (const void* data, size_t len);

#define SetSignalAction_(inAction)
#ifdef __PowerPlant__
#define GetSignalAction_() debugAction_Nothing
#else
#define GetSignalAction_() (0)
#endif

#ifdef __PowerPlant__
#	undef SignalPStr_
#	undef SignalCStr_
#	undef SignalIf_
#	undef SignalIfNot_
#endif /* __PowerPlant */

#define SignalPStr_(pstr)                                     \
    do {                                                      \
        dprintf ("Assertion failed: %.*s (%s: %d)\n",         \
                 (pstr) [0], (pstr) + 1, __FILE__, __LINE__); \
    } while (0)

#define SignalCStr_(cstr)                                     \
    do {                                                      \
        dprintf ("Assertion failed: %s (%s: %d)\n",           \
                 cstr, __FILE__, __LINE__);                   \
    } while (0)

#define SignalIf_(test)                                       \
    do {                                                      \
        if (test) SignalCStr_(#test);                         \
    } while (0)

#define SignalIfNot_(test) SignalIf_(!(test))

#define Assert_(test)      SignalIfNot_(test)

enum { errUncaughtException = 666 };

#define SafeTry_               try
#define SafeCatch_             catch (...)
#define SafeCatchOSErr_(error) catch (...) { SignalPStr_ ("\pUncaught exception"); error = errUncaughtException; }

#define DebugThrow_(e)                                                  \
    do {                                                                \
        dprintf ("Exception thrown from %s: %d\n", __FILE__, __LINE__); \
        throw (e);                                                      \
    } while (false)

#ifdef __cplusplus
}
#endif

#endif /* __KERBEROSDEBUG */