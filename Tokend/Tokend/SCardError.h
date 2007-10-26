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
 *  SCardError.h
 *  TokendMuscle
 */

#ifndef _TOKEND_SCARDERROR_H_
#define _TOKEND_SCARDERROR_H_

#include <security_utilities/debugging.h>
#include <security_utilities/errors.h>


/* ISO/IEC 7816 part 3 and 4 error codes. */

/** success */
#define SCARD_SUCCESS                        0x9000


/* '61XX'	SW2 indicates the number of response bytes still available. */
#define SCARD_BYTES_LEFT_IN_SW2              0x6100


/* '62XX'	Warning processings - State of non-volatile memory unchanged. */

/** Execution warning, state of non-volatile memory unchanged */
#define SCARD_EXECUTION_WARNING              0x6200

/** Part of returned data may be corrupted. */
#define SCARD_RETURNED_DATA_CORRUPTED        0x6281

/** End of file/record reached before reading Le bytes. */
#define SCARD_END_OF_FILE_REACHED            0x6282

/** Selected file invalidated. */
#define SCARD_FILE_INVALIDATED               0x6283

/** FCI not formatted according to 1.1.5. */
#define SCARD_FCI_INVALID                    0x6284


/* '62XX'	Warning processings - State of non-volatile memory changed. */

/** Authentication failed. */
#define SCARD_AUTHENTICATION_FAILED          0x6300

/** File filled up by the last write. */
#define SCARD_FILE_FILLED                    0x6381

/** Authentication failed, 0 retries left. */
#define SCARD_AUTHENTICATION_FAILED_0        0x63C0

/** Authentication failed, 1 retry left. */
#define SCARD_AUTHENTICATION_FAILED_1        0x63C1

/** Authentication failed, 2 retries left. */
#define SCARD_AUTHENTICATION_FAILED_2        0x63C2

/** Authentication failed, 3 retries left. */
#define SCARD_AUTHENTICATION_FAILED_3        0x63C3

/** Authentication failed, 4 retries left. */
#define SCARD_AUTHENTICATION_FAILED_4        0x63C4

/** Authentication failed, 5 retries left. */
#define SCARD_AUTHENTICATION_FAILED_5        0x63C5

/** Authentication failed, 6 retries left. */
#define SCARD_AUTHENTICATION_FAILED_6        0x63C6

/** Authentication failed, 7 retries left. */
#define SCARD_AUTHENTICATION_FAILED_7        0x63C7

/** Authentication failed, 8 retries left. */
#define SCARD_AUTHENTICATION_FAILED_8        0x63C8

/** Authentication failed, 9 retries left. */
#define SCARD_AUTHENTICATION_FAILED_9        0x63C9

/** Authentication failed, 10 retries left. */
#define SCARD_AUTHENTICATION_FAILED_10       0x63CA

/** Authentication failed, 11 retries left. */
#define SCARD_AUTHENTICATION_FAILED_11       0x63CB

/** Authentication failed, 12 retries left. */
#define SCARD_AUTHENTICATION_FAILED_12       0x63CC

/** Authentication failed, 13 retries left. */
#define SCARD_AUTHENTICATION_FAILED_13       0x63CD

/** Authentication failed, 14 retries left. */
#define SCARD_AUTHENTICATION_FAILED_14       0x63CE

/** Authentication failed, 15 retries left. */
#define SCARD_AUTHENTICATION_FAILED_15       0x63CF


/* '64XX'	Execution errors - State of non-volatile memory unchanged. */

/** Execution error, state of non-volatile memory unchanged. */
#define SCARD_EXECUTION_ERROR                0x6400


/* '65XX'	Execution errors - State of non-volatile memory changed. */

/** Execution error, state of non-volatile memory changed. */
#define SCARD_CHANGED_ERROR                  0x6500

/** Memory failure. */
#define SCARD_MEMORY_FAILURE                 0x6581


/* '66XX'	Reserved for security-related issues. */

/* '6700'	Wrong length. */

/** The length is incorrect. */
#define SCARD_LENGTH_INCORRECT               0x6700


/* '68XX'	Functions in CLA not supported. */

/** No information given. */
#define SCARD_CLA_UNSUPPORTED                0x6800

/** Logical channel not supported. */
#define SCARD_LOGICAL_CHANNEL_UNSUPPORTED    0x6881

/** Secure messaging not supported. */
#define SCARD_SECURE_MESSAGING_UNSUPPORTED   0x6882


/* '69XX'	Command not allowed. */

/** Command not allowed. */
#define SCARD_COMMAND_NOT_ALLOWED            0x6900

/** Command incompatible with file structure. */
#define SCARD_COMMAND_INCOMPATIBLE           0x6981

/** Security status not satisfied. */
#define SCARD_NOT_AUTHORIZED                 0x6982

/** Authentication method blocked. */
#define SCARD_AUTHENTICATION_BLOCKED         0x6983

/** Referenced data invalidated. */
#define SCARD_REFERENCED_DATA_INVALIDATED    0x6984

/** Conditions of use not satisfied. */
#define SCARD_USE_CONDITIONS_NOT_MET         0x6985

/** Command not allowed (no current EF). */
#define SCARD_NO_CURRENT_EF                  0x6986

/** Expected SM data objects missing. */
#define SCARD_SM_DATA_OBJECTS_MISSING        0x6987

/** SM data objects incorrect. */
#define SCARD_SM_DATA_NOT_ALLOWED            0x6988


/* '6AXX'	Wrong parameter(s) P1-P2. */

/** Wrong parameter. */
#define SCARD_WRONG_PARAMETER                0x6A00

/** Incorrect parameters in the data field. */
#define SCARD_DATA_INCORRECT                 0x6A80

/** Function not supported. */
#define SCARD_FUNCTION_NOT_SUPPORTED         0x6A81

/** File not found. */
#define SCARD_FILE_NOT_FOUND                 0x6A82

/** Record not found. */
#define SCARD_RECORD_NOT_FOUND               0x6A83

/** Not enough memory space in the file. */
#define SCARD_NO_MEMORY_LEFT                 0x6A84

/** Lc inconsistent with TLV structure. */
#define SCARD_LC_INCONSISTENT_TLV            0x6A85

/** Incorrect parameters P1-P2. */
#define SCARD_INCORRECT_P1_P2                0x6A86

/** Lc inconsistent with P1-P2. */
#define SCARD_LC_INCONSISTENT_P1_P2          0x6A87

/** Referenced data not found. */
#define SCARD_REFERENCED_DATA_NOT_FOUND      0x6A88


/* '6B00'	Wrong parameter(s) P1-P2. */

/** Wrong parameter(s) P1-P2. */
#define SCARD_WRONG_PARAMETER_P1_P2          0x6B00


/* '6CXX'	Wrong length Le: SW2 indicates the exact length */
#define SCARD_LE_IN_SW2                      0x6C00


/* '6D00'	Instruction code not supported or invalid. */

/** The instruction code is not programmed or is invalid. */
#define SCARD_INSTRUCTION_CODE_INVALID       0x6D00


/* '6E00'	Class not supported. */

/** The card does not support the instruction class. */
#define SCARD_INSTRUCTION_CLASS_UNSUPPORTED  0x6E00


/* '6F00'	No precise diagnosis. */

/** No precise diagnostic is given. */
#define SCARD_UNSPECIFIED_ERROR              0x6F00


namespace Tokend
{

class SCardError : public Security::CommonError
{
protected:
    SCardError(uint16_t sw);
public:
    const uint16_t statusWord;
    virtual OSStatus osStatus() const;
	virtual int unixError() const;
    virtual const char *what () const throw ();

    static void check(uint16_t sw)	{ if (sw != SCARD_SUCCESS) throwMe(sw); }
    static void throwMe(uint16_t sw) __attribute__((noreturn));
    
protected:
    IFDEBUG(void debugDiagnose(const void *id) const;)
    IFDEBUG(static const char *errorstr(uint16_t sw);)
};

} // end namespace Tokend

#endif /* !_TOKEND_SCARDERROR_H_ */

