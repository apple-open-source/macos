
//
// IrDADebugging.h
//       A place control logging and asserts
//
// Todo: figure out pb's build styles
//
#ifndef __IrDADebugging__
#define __IrDADebugging__

#include <kern/macro_help.h>

//#define hasTracing        1       // control irdalogs and irdalog.c
//#define hasDebugging  1       // off for no asserts

// todo: add a call to panic or use the kernel's assert macro instead?
#if (hasDebugging > 0)
//#define DebugLog(msg)             IOLog("IrDA: %s, %s, line %d, %s\n", __FILE__, __PRETTY_FUNCTION__, __LINE__ , msg)
//#define DebugLog(fmt, args...)     IOLog("IrDA: %s, %s, line %d, " fmt "\n", __FILE__, __PRETTY_FUNCTION__, __LINE__ , ## args)
#define DebugLog(fmt, args...)     IOLog("IrDA: %s, %s, line %d: " fmt "\n", __FILE__, __FUNCTION__, __LINE__ , ## args)
#define LOGIT(expr)                         \
    IOLog("IrDA: Assertion \"%s\" failed!  File %s, function %s, line %d\n",    \
	#expr, __FILE__, __FUNCTION__, __LINE__)
#else
#define LOGIT(expr) ((void)0)
#define DebugLog(x...) ((void)0)
#endif

// per source module irdalog settings
#define hasAppleSCCIrDATracing      1   // AppleSCCIrDA.cpp
#define hasAppleUSBIrDATracing      1   // AppleUSBIrDA.cpp (not impl)
#define hasIrDACommTracing      1   // IrDAComm.cpp
#define hasCBufferSegTracing    1   // CBufferSegment.cpp
#define hasIrEventTracing       1   // IrEvent.cpp
#define hasIrGlueTracing        1   // IrGlue.cpp
#define hasCTimerTracing        1   // CTimer.cpp

#define hasIrStreamTracing      1   // IrStream.cpp
#define hasIrDiscoveryTracing   1   // IrDiscovery.cpp
#define hasLAPConnTracing       1   // IrLAPConn.cpp
#define hasLAPTracing           1   // IrLAP.cpp
#define hasLMPTracing           1   // IrLMP.cpp
#define hasLAPConnTracing       1   // IrLAPConn.cpp
#define hasLSAPConnTracing      1   // IrLSAPConn.cpp
#define hasCIrDeviceTracing     2   // CIrDevice.cpp (full packet dump if > 1)
#define hasIrQOSTracing         1   // IrQOS.cpp
#define hasIASServiceTracing    1   // IrIASService.cpp
#define hasIASServerTracing     1   // IrIASServer.cpp
#define hasCListTracing         1   // CList.cpp
//#define hasCDynamicArrayTracing   1   // CDynamicArray.cpp
#define hasIASClientTracing     1   // IrIASClient.cpp
#define hasIrLSAPTracing        1   // CIrLSAP.cpp

#define hasTTPTracing           1   // ttp.cpp
#define hasTTP2Tracing          1   // ttp2.cpp
#define hasTTP3Tracing          1   // ttp3.cpp
#define hasTTPLMPTracing        1   // ttplmp.cpp
#define hasTTPPduTracing        1   // ttppdu.cpp
#define hasIrCommTracing        1   // ircomm.cpp

// consider moving this and/or making a real control for it, used by qos.cpp
// the bits are:
// 001  2400 baud           010 57.6        100 4mbit
// 002  9600                020 115k
// 004  19.2                040 .5 mbit
// 008  38.4                080 1mbit
//#define THROTTLE_SPEED    0x002e          // define to throttle

#ifdef assert
#undef assert           // nuke IOKit/kernel's assert
#endif

#define assert(expr)                    \
    MACRO_BEGIN                         \
	if (expr) { }                   \
	else {                          \
	    LOGIT(expr);                \
	}                               \
    MACRO_END

#define require(expr, failed)           \
    MACRO_BEGIN                         \
	if (expr) { }                   \
	else {                          \
	    LOGIT(expr);                \
	    goto failed;                \
	}                               \
    MACRO_END
	
#define nrequire(expr, failed)          \
    MACRO_BEGIN                         \
	if (!(expr)) { }                \
	else {                          \
	    LOGIT(expr);                \
	    goto failed;                \
	}                               \
    MACRO_END
	
// Support old flavors of assert macros

#define check(expr)                             assert(expr)
#define ncheck(expr)                            assert(!(expr))
#define XASSERT(expr)                           check(expr)
#define XASSERTNOT(expr)                        ncheck(expr)
#define XREQUIRE(expr, label)                   require(expr, label)
#define XREQUIRENOT(expr, label)                nrequire(expr,label)

#endif          //  __IrDADebugging__
