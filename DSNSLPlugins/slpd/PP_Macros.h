/*
CarbonÅ is currently under development and is not yet complete. Any comments
related to Carbon are preliminary, and you should confirm that any workarounds
we (Metrowerks) implement do work in the latest shipping version of Carbon.

If we (Metrowerks) are using an API from the CarbonLib, its version and date
that version was current will be identified in a comment using the following
format:

	// Carbon API from CarbonLib vX.X, version current as of MM/DD/YY

If we (Metrowerks) are working around the CarbonLib for whatever reason, the
version of the CarbonLib we are working around and the date at which that
version of the CarbonLib was current will be identified in the comment using
the following format:

	// CarbonLib workaround for CarbonLib vX.X, version current as of MM/DD/YY
*/

// ===========================================================================
//	PP_Macros.h					PowerPlant 1.9.3	©1993-1998 Metrowerks Inc.
// ===========================================================================
//
//	Macro definitions for controlling conditional compilation options
//
//	The basic format of most of the options is:
//
//		#ifndef Option_Name
//			#define Option_Name		Default_Value
//		#endif
//
//		#if Option_Name
//			// Define symbols for the option being ON
//		#else
//			// Define symbols for the option being OFF
//		#endif
//
//	If you wish to set the option yourself, you should place the appropriate
//	#define of the Option_Name in a header file that gets #include'd before
//	this one, normally your project prefix file or precompiled header.

#ifndef _H_PP_Macros
#define _H_PP_Macros
#pragma once

// ---------------------------------------------------------------------------
//	PowerPlant version number

#define		__PowerPlant__	0x01938000	// Version 1.9.3


// ---------------------------------------------------------------------------
//	Target OS for MSL

#ifndef __dest_os
	#define __dest_os  __mac_os
#endif


// ---------------------------------------------------------------------------
//	Target OS for Carbon changes

	// Carbon API from CarbonLib v1.0d10 (WWDC release), version current as of 05/01/99

#ifndef PP_Target_Carbon
	#define PP_Target_Carbon				1		// default is ON (currently)
#endif

// sanity check (these are temporary)

#if PP_Target_Carbon && !defined(TARGET_API_MAC_CARBON)
	#define TARGET_API_MAC_CARBON			1		// temp Apple macro
#endif

#if PP_Target_Carbon && !TARGET_API_MAC_CARBON
	#error "TARGET_API_MAC_CARBON must be true if PP_Target_Carbon is true."
#endif

#if TARGET_API_MAC_CARBON && !PP_Target_Carbon
	#error "PP_Target_Carbon must be true if TARGET_API_MAC_CARBON is true."
#endif


// ---------------------------------------------------------------------------
//	PowerPlant Namespace

#ifndef PP_Uses_PowerPlant_Namespace
	#define PP_Uses_PowerPlant_Namespace	0		// Default to OFF
#endif

#if PP_Uses_PowerPlant_Namespace
	#define PP_Begin_Namespace_PowerPlant	namespace PowerPlant {
	#define PP_End_Namespace_PowerPlant		}
	#define PP_Using_Namespace_PowerPlant	using namespace PowerPlant;
	#define PP_PowerPlant					PowerPlant
#else
	#define PP_Begin_Namespace_PowerPlant
	#define PP_End_Namespace_PowerPlant
	#define PP_Using_Namespace_PowerPlant
	#define PP_PowerPlant
#endif


// ---------------------------------------------------------------------------
//	std Namespace

	// Macro for controlling use of "std" namespace for the C++
	// standard libraries. Within CodeWarrior, this setting should
	// be ON if _MSL_USING_NAMESPACE is #define'd.
	//
	// Set to OFF only if you have explicitly disabled namespace support
	// in the MSL or are using another implementation of the C++
	// standard libraries that does not support namespaces.
	
#ifndef PP_Uses_Std_Namespace
	#define	PP_Uses_Std_Namespace			1		// Default to ON
#endif

#if PP_Uses_Std_Namespace
	#define PP_Using_Namespace_Std			using namespace std;
	#define PP_STD							std
#else
	#define PP_Using_Namespace_Std
	#define PP_STD
#endif


// ---------------------------------------------------------------------------
//	String Literals

	// Mac OS uses Pascal-style strings (length byte followed by characters)
	// for most Toolbox calls. Prefixing string literals with \p (which
	// the compiler replaces with a length byte) is a non-standard extension
	// to C supported by Mac OS compilers.
	//
	// If you use literal strings in your code and want to build with
	// multiple compilers that may or may not support Pascal strings, you
	// must be able to handle both C and Pascal strings. One way to do
	// this is to only use string literals as arguments to functions,
	// and to overload such functions so that there is a version that
	// accepts C strings (char *) and one that accepts Pascal strings
	// (unsigned char *).
	
#ifndef PP_Supports_Pascal_Strings
	#define PP_Supports_Pascal_Strings		1		// Default is true
#endif

#if PP_Supports_Pascal_Strings
	#define StringLiteral_(str)		("\p" str)		// Pascal string
#else
	#define StringLiteral_(str)		(str)			// C string
#endif


// ---------------------------------------------------------------------------
//	Option for implementation of standard dialogs

	// Navigation Services provides standard dialogs for confirming
	// document saves and for choosing and specifying files. However,
	// Nav Services is not present in all Systems. It is available as
	// a separate SDK for System 8.1 and will ship with System 8.5.
	// For compatibility, PP offers three implementations of the dialogs.
	//
	//		1) Classic Only - Uses Alert and Standard File, which is
	//							available in System 7 or later
	//
	//		2) Nav Services Only - You are responsible for checking for
	//								Nav Services before using the dialogs.
	//								You may want to check at launch and
	//								refuse to run or disable features.
	//
	//		3) Conditional - Checks at runtime. Uses Nav Services if
	//							available, otherwise Classic

#define	PP_StdDialogs_ClassicOnly			1		// Always use Classic
#define	PP_StdDialogs_NavServicesOnly		2		// Always use Nav Services
#define PP_StdDialogs_Conditional			3		// Check at runtime

#ifndef	PP_StdDialogs_Option						// Default is Classic

#if PP_Target_Carbon
	// Carbon API from CarbonLib v1.0d10 (WWDC release), version current as of 05/01/99

	// Under Carbon, only Navigation Services are supports, so we'll do
	// our best to enforce that.
	
	#define PP_StdDialogs_Option	PP_StdDialogs_NavServicesOnly

#else

	#define PP_StdDialogs_Option	PP_StdDialogs_ClassicOnly

#endif

#endif

// Carbon sanity check

#if PP_Target_Carbon && (PP_StdDialogs_Option != PP_StdDialogs_NavServicesOnly)
	#warning "Cannot use non-NavServices option under Carbon"
#endif


// ---------------------------------------------------------------------------
//	Option for defining PP integer types

		// PP_Types.h has typedef's for signed and unsigned types
		// of char, short, and long. Apple's MacTypes.h also has
		// typedef's for those items but with slightly different names.
		//
		// You should use the Apple typedef's. The PP typedef's are
		// deprecated.
		//
		// NOTE: Some PP code still uses the old types.

#ifndef PP_Uses_Old_Integer_Types
	#define	PP_Uses_Old_Integer_Types		1		// Default to ON
#endif


// ---------------------------------------------------------------------------
//	Option for forcing a compiler error if using the obsolete
//	LCommander::AllowTargetSwitch() function.

		// If you override AllowTargetSwitch() to perform data entry
		// validation, you must rename that function to AllowBeTarget().
		// You may also want to update your code to take advantage of the
		// greater flexibility of the AllowBeTarget/AllowDontBeTarget calls.

#ifndef PP_Obsolete_AllowTargetSwitch
	#define PP_Obsolete_AllowTargetSwitch	1		// Default to ON
#endif


// ---------------------------------------------------------------------------
//	Import option for CFM68K

#if defined(__CFM68K__) && !defined(__USING_STATIC_LIBS__)
	#define PP_Uses_Pragma_Import	1
#else
	#define PP_Uses_Pragma_Import	0
#endif


// ---------------------------------------------------------------------------
//	Preprocessor symbols for Apple Universal Headers options

#undef SystemSevenOrLater				// PowerPlant requires System 7
#define SystemSevenOrLater	1

#undef OLDROUTINENAMES					// PP uses only new names
#define OLDROUTINENAMES		0

#undef OLDROUTINELOCATIONS				// PP uses only new header locations
#define OLDROUTINELOCATIONS	0


#endif
