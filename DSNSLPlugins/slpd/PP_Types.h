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
//	PP_Types.h					PowerPlant 1.9.3	©1993-1998 Metrowerks Inc.
// ===========================================================================

#ifndef _H_PP_Types
#define _H_PP_Types
#pragma once

#include <PP_Macros.h>
#include <Carbon/Carbon.h>				// Toolbox data types

#if PP_Uses_Pragma_Import
	#pragma import on
#endif

//PP_Begin_Namespace_PowerPlant

// ---------------------------------------------------------------------------
//	Integer types

	//	These types are obsolete. Use the equivalent Toolbox types instead.

								
#if PP_Uses_Old_Integer_Types						// Toolbox Type

	typedef		signed char				Int8;		// SInt8
	typedef		signed short			Int16;		// SInt16
	typedef		signed long				Int32;		// SInt32

	typedef		unsigned char			Uint8;		// UInt8
	typedef		unsigned short			Uint16;		// UInt16
	typedef		unsigned long			Uint32;		// UInt32

	typedef		UInt16					Char16;		// UInt16
	typedef		unsigned char			Uchar;		// UInt8
	
#endif


#if UNIVERSAL_INTERFACES_VERSION < 0x0320
													// Defined by Universal
													//   Headers 3.2
	typedef		const unsigned char*	ConstStringPtr;
#endif


// ---------------------------------------------------------------------------
//	Enumeration for tri-state hierarchical properties

enum	ETriState {
	triState_Off,			// Setting is OFF
	triState_Latent,		// Setting is ON, but Super's setting is OFF
	triState_On				// Setting is ON, and Super's setting is ON
};


// ---------------------------------------------------------------------------
//	Types for PowerPlant Identifiers

typedef		SInt32			CommandT;
typedef		SInt32			MessageT;

typedef		SInt16			ResIDT;
typedef		SInt32			PaneIDT;
typedef		FourCharCode	ClassIDT;
typedef		FourCharCode	DataIDT;
typedef		FourCharCode	ObjectIDT;

//PP_End_Namespace_PowerPlant


// ---------------------------------------------------------------------------
//	Macros for accessing the top-left and bottom-right corners of a Rect

	//	The original Toolbox specifies these accessors, but Apple no longer
	//	includes them in the Universal Headers. We define them because a lot
	//	of existing code uses them.

#ifndef topLeft
	#define topLeft(r)	(((Point *) &(r))[0])
#endif

#ifndef botRight
	#define botRight(r)	(((Point *) &(r))[1])
#endif


// ---------------------------------------------------------------------------
//	Definition of type double_t

	// double_t is the most efficient type at least as wide as double for
	// the target CPU. fp.h and math.h both define double_t, but we define
	// it here in case you don't want to include one of those headers.
	
#if !defined(__FP__) && !defined(__cmath__)
	#if TARGET_CPU_PPC
		typedef float 							float_t;
		typedef double 							double_t;
	#elif TARGET_CPU_68K
		typedef long double 					float_t;
		typedef long double 					double_t;
	#elif TARGET_CPU_X86
		#if NeXT
			typedef double 						float_t;
			typedef double 						double_t;
		#else
            // Changed to match fp.h
            //typedef long double 				float_t;
            //typedef long double 				double_t;
            typedef double                          float_t;
            typedef double                          double_t;
		#endif  /* NeXT */
	#elif TARGET_CPU_MIPS
		typedef double 							float_t;
		typedef double 							double_t;
	#elif TARGET_CPU_ALPHA
		typedef double 							float_t;
		typedef double 							double_t;
	#elif TARGET_CPU_SPARC
		typedef double 							float_t;
		typedef double 							double_t;
	#else
		#error unsupported CPU
	#endif  /*  */
#endif


#if PP_Uses_Pragma_Import
	#pragma import reset
#endif

#endif
