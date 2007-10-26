/*
 *  Copyright (c) 2004 Apple Computer, Inc. All Rights Reserved.
 * 
 *  @APPLE_LICENSE_HEADER_START@
 *  
 *  This file contains Original Code and/or Modifications of Original Code
 *  as defined in and that are subject to the Apple Public Source License
 *  Version 2.0 (the 'License'). You may not use this file except in
 *  compliance with the License. Please obtain a copy of the License at
 *  http://www.opensource.apple.com/apsl/ and read it before using this
 *  file.
 *  
 *  The Original Code and all software distributed under the License are
 *  distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 *  EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 *  INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 *  FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 *  Please see the License for the specific language governing rights and
 *  limitations under the License.
 *  
 *  @APPLE_LICENSE_HEADER_END@
 */

/*
 *  MscError.cpp
 *  TokendMuscle
 */

#include "MscError.h"

//
// MacOSError exceptions
//
MscError::MscError(int err) : error(err)
{
	IFDEBUG(debugDiagnose(this));
}

const char *MscError::what() const throw ()
{ return "Musclecard error"; }

OSStatus MscError::osStatus() const
{ return error; }

int MscError::unixError() const
{
	switch (error)
	{
	default:
		// cannot map this to errno space
		return -1;
    }
}

void MscError::throwMe(int error)
{ throw MscError(error); }

#if !defined(NDEBUG)
void MscError::debugDiagnose(const void *id) const
{
    secdebug("exception", "%p MscError %s (%d)",
		id, mscerrorstr(error), error);
}

const char *MscError::mscerrorstr(int err) const
{
    switch (err)
	{
	// Musclecard Errors
	case MSC_SUCCESS:				return "Success";
	case MSC_NO_MEMORY_LEFT:		return "There have been memory problems on the card";
	case MSC_AUTH_FAILED:			return "Entered PIN is not correct";
	case MSC_OPERATION_NOT_ALLOWED: return "Required operation is not allowed in actual circumstances";
	case MSC_INCONSISTENT_STATUS:   return "Required operation is inconsistent with memory contents";
	case MSC_UNSUPPORTED_FEATURE:   return "Required feature is not (yet) supported";
	case MSC_UNAUTHORIZED:			return "Required operation was not authorized because of a lack of privileges";
	case MSC_OBJECT_NOT_FOUND:		return "Required object is missing";
	case MSC_OBJECT_EXISTS:			return "New object ID already in use";
	case MSC_INCORRECT_ALG:			return "Algorithm specified is not correct";
	case MSC_SIGNATURE_INVALID:		return "Verify operation detected an invalid signature";
	case MSC_IDENTITY_BLOCKED:		return "Operation has been blocked for security reason";
	case MSC_UNSPECIFIED_ERROR:		return "Unspecified error";
	case MSC_TRANSPORT_ERROR:		return "PCSC and driver transport errors";
	case MSC_INVALID_PARAMETER:		return "Invalid parameter given";
	case MSC_INCORRECT_P1:			return "Incorrect P1 parameter";
	case MSC_INCORRECT_P2:			return "Incorrect P2 parameter";
	case MSC_SEQUENCE_END:			return "End of sequence";
	case MSC_INTERNAL_ERROR:		return "For debugging purposes - Internal error";
	case MSC_CANCELLED:				return "A blocking event has been cancelled";
	case MSC_INSUFFICIENT_BUFFER:   return "The buffer provided is too short";
	case MSC_UNRECOGNIZED_TOKEN:	return "The selected token is not recognized";
	case MSC_SERVICE_UNRESPONSIVE:  return "The PC/SC services is not available";
	case MSC_TIMEOUT_OCCURRED:		return "The action has timed out";
	case MSC_TOKEN_REMOVED:			return "The token has been removed";
	case MSC_TOKEN_RESET:			return "The token has been reset";
	case MSC_TOKEN_INSERTED:		return "The token has been inserted";
	case MSC_TOKEN_UNRESPONSIVE:	return "The token is unresponsive";
	case MSC_INVALID_HANDLE:		return "The handle is invalid";
	case MSC_SHARING_VIOLATION:		return "Invalid sharing";

	// PCSC Errors
	case SCARD_S_SUCCESS:
	case SCARD_E_CANCELLED:
	case SCARD_E_CANT_DISPOSE:
	case SCARD_E_INSUFFICIENT_BUFFER:
	case SCARD_E_INVALID_ATR:
	case SCARD_E_INVALID_HANDLE:
	case SCARD_E_INVALID_PARAMETER:
	case SCARD_E_INVALID_TARGET:
	case SCARD_E_INVALID_VALUE:
	case SCARD_E_NO_MEMORY:
	case SCARD_F_COMM_ERROR:
	case SCARD_F_INTERNAL_ERROR:
	case SCARD_F_UNKNOWN_ERROR:
	case SCARD_F_WAITED_TOO_LONG:
	case SCARD_E_UNKNOWN_READER:
	case SCARD_E_TIMEOUT:
	case SCARD_E_SHARING_VIOLATION:
	case SCARD_E_NO_SMARTCARD:
	case SCARD_E_UNKNOWN_CARD:
	case SCARD_E_PROTO_MISMATCH:
	case SCARD_E_NOT_READY:
	case SCARD_E_SYSTEM_CANCELLED:
	case SCARD_E_NOT_TRANSACTED:
	case SCARD_E_READER_UNAVAILABLE:
	case SCARD_W_UNSUPPORTED_CARD:
	case SCARD_W_UNRESPONSIVE_CARD:
	case SCARD_W_UNPOWERED_CARD:
	case SCARD_W_RESET_CARD:
	case SCARD_W_REMOVED_CARD:
	case SCARD_E_PCI_TOO_SMALL:
	case SCARD_E_READER_UNSUPPORTED:
	case SCARD_E_DUPLICATE_READER:
	case SCARD_E_CARD_UNSUPPORTED:
	case SCARD_E_NO_SERVICE:
	case SCARD_E_SERVICE_STOPPED:
		return pcsc_stringify_error(err);
	default:
		return "Unknown error";
	}
}
#endif //NDEBUG

