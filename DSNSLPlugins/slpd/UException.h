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
//	UException.h				PowerPlant 1.9.3	©1994-1998 Metrowerks Inc.
// ===========================================================================
//
//	Useful macros for throwing exceptions. The macros throw and exception
//	based on some test condition. The macros use the Throw_() macro,
//	which supports debugging options. See UDebugging.h for details.

#ifndef _H_UException
#define _H_UException
#pragma once

#include <UDebugging.h>

#include <Carbon/Carbon.h>
#include <Carbon/Carbon.h>
#include <Carbon/Carbon.h>

#if PP_Uses_Pragma_Import
	#pragma import on
#endif

//PP_Begin_Namespace_PowerPlant

	// Exception codes

const ExceptionCode	err_NilPointer		= FOUR_CHAR_CODE('nilP');
const ExceptionCode	err_AssertFailed	= FOUR_CHAR_CODE('asrt');


		// Exception Handling Macros
		//	These now map to the "real" Exception Handling syntax

#define Try_			try
#define Catch_(err)		catch(PP_PowerPlant::ExceptionCode err)
#define EndCatch_


	// Useful macros for signaling common failures
	
	
		// This macro avoids evaluating "err" twice by assigning
		// its value to a local variable.
		
#define	ThrowThisIf_(test,err)										\
	do {															\
		if (test) Throw_(err);										\
	} while (false)

#define	ThrowThisIfNot_(test,err)									\
	do {															\
		if (!(test)) Throw_(err);									\
	} while (false)

#define	ThrowThisIfNULL_(ptr,err)									\
	do {															\
		if ((ptr) == NULL) Throw_(err);								\
	} while (false)

#define ThrowThisIfOSErr_(err,value)								\
	do {															\
		OSErr	__theErr = err;										\
		if (__theErr != eDSNoErr) {									\
			Throw_(value);											\
		}															\
	} while (false)

#define ThrowIfOSStatus_(err)										\
	do {															\
		OSStatus	__theErr = err;									\
		if (__theErr != noErr) {									\
			Throw_(__theErr);										\
		}															\
	} while (false)
	
#define ThrowIfOSErr_(err)											\
	do {															\
		OSErr	__theErr = err;										\
		if (__theErr != noErr) {									\
			Throw_(__theErr);										\
		}															\
	} while (false)
	
#define ThrowIfError_(err)											\
	do {															\
		PP_PowerPlant::ExceptionCode	__theErr = err;				\
		if (__theErr != 0) {										\
			Throw_(__theErr);										\
		}															\
	} while (false)

#define ThrowOSErr_(err)	Throw_(err)

#define	ThrowIfNil_(ptr)											\
	do {															\
		if ((ptr) == nil) Throw_(PP_PowerPlant::err_NilPointer);	\
	} while (false)

#define	ThrowIfNULL_(ptr)											\
	do {															\
		if ((ptr) == nil) Throw_(PP_PowerPlant::err_NilPointer);	\
	} while (false)

#define	ThrowIfResError_()	ThrowIfOSErr_(::ResError())
#define	ThrowIfMemError_()	ThrowIfOSErr_(::MemError())
#define	ThrowIfQDError_()	ThrowIfOSErr_(::QDError())
#define ThrowIfPrError_()	ThrowIfOSErr_(::PrError())

#define	ThrowIfResFail_(h)											\
	do {															\
		if ((h) == nil) {											\
			OSErr	__theErr = ::ResError();						\
			if (__theErr == noErr) {								\
				__theErr = resNotFound;								\
			}														\
			Throw_(__theErr);										\
		}															\
	} while (false)
	
#define	ThrowIfMemFail_(p)											\
	do {															\
		if ((p) == nil) {											\
			OSErr	__theErr = ::MemError();						\
			if (__theErr == noErr) __theErr = memFullErr;			\
			Throw_(__theErr);										\
		}															\
	} while (false)

#define	ThrowIf_(test)												\
	do {															\
		if (test) Throw_(PP_PowerPlant::err_AssertFailed);			\
	} while (false)

#define	ThrowIfNot_(test)											\
	do {															\
		if (!(test)) Throw_(PP_PowerPlant::err_AssertFailed);		\
	} while (false)

#define	FailOSErr_(err)		ThrowIfOSErr_(err)
#define FailNIL_(ptr)		ThrowIfNil_(ptr)

//PP_End_Namespace_PowerPlant

#if PP_Uses_Pragma_Import
	#pragma import reset
#endif

#endif