/* Use this header for Carbon on OSX */
/* --das */

#define USE_TCL_STUBS

#ifdef __MWERKS__ //  das 260601
#if __ide_target("TclAE-PPC FakeX Debug")
#define TARGET_API_MAC_OS8 1
#define TARGET_API_MAC_CARBON 0
#define TARGET_API_MAC_OSX 0
#define TCLAE_NO_AESUBDESCS
#define TCLAE_NO_EPPC
#endif
#define TCLAE_CW 1
#undef		OLDROUTINENAMES
#include "tclMacCommonPch.h"
#undef		OLDROUTINENAMES
#endif

#ifndef TARGET_API_MAC_CARBON
#define TARGET_API_MAC_CARBON 1
#define TARGET_API_MAC_OSX 1
#define TCLAE_OSX 1 //  das 260601
#define TCLAE_CARBON 1
#endif

#ifdef __APPLE_CC__ //  das 260601
#define TCLAE_GCC 1
#endif

#ifdef TCLAE_GCC //  das 260601
//  das 260601 GCC will actually include the precompiled header <Carbon/Carbon.p> if present
#ifdef TCLAE_USE_FRAMEWORK_INCLUDES
#include <Carbon/Carbon.h>
#else
#include <ConditionalMacros.h>
#endif
//  das 260601 dummy typedefs for old types, remove when no longer needed
struct SFReply {};
typedef struct SFReply SFReply;
typedef struct SFReply StandardFileReply;
#else
#include <ConditionalMacros.h>
#endif //TCLAE_GCC

#if TARGET_RT_MAC_MACHO //  das 260601 compiling natively on OSX, either with GCC or a new CW
#define TCLAE_MACHO 1
#endif

#ifdef TCLAE_MACHO //  das 260601
#define TCLAE_PATH_SEP '/'
#else
#define TCLAE_PATH_SEP ':'
#endif

#ifdef TCLAE_USE_FRAMEWORK_INCLUDES
#include	<Tcl/tcl.h>
#else
#include	<tcl.h>
#endif

#ifndef CONST84 // Tcl 8.4 backwards compatibility
#      define CONST84 
#      define CONST84_RETURN CONST
#endif

#include "tclAE.h"
