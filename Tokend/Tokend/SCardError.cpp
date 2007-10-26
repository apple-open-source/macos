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
 *  SCardError.cpp
 *  TokendMuscle
 */

#include "SCardError.h"

#include <Security/cssmerr.h>

namespace Tokend
{

/*
Excerpt from ISO/IEC 7816 part 3:

Status bytes (SW1=$6x or $9x, expect $60; SW2 any value) 
-------------------------------------------------------- 
The end sequence SW1-SW2 gives the card status at the end of the command.

The normal ending is indicated by SW1-SW2 = $90-$00.

When the most significant half byte SW1 is $6, the meaning of SW1 is
independant of the application. The following five values are defined:

$6E The card does not support the instruction class. 
$6D The instruction code is not programmed or is invalid. 
$6B The reference is incorrect. 
$67 The length is incorrect. 
$6F No precise diagnostic is given.

Other values are reserved for future use by ISO7816. When SW1 is neither $6E
nor $6D, the card support the instruction. This part of ISO7816 does not
interprets neither $9X SW1 bytes, nor SW2 
bytes; Their meaning relates to the application itself.

Supplement (were seen sometimes): 
--------------------------------- 
SW1 SW2 Meaning

62 81 Returned data may be corrupted. 
62 82 The end of the file has been reached before the end of reading. 
62 84 Selected file is not valid. 
65 01 Memory failure. There have been problems in writing or reading 
the EEPROM. Other hardware problems may also bring this error. 
68 00 The request function is not supported by the card. 
6A 00 Bytes P1 and/or P2 are incorrect. 
6A 80 The parameters in the data field are incorrect. 
6A 82 File not found. 
6A 83 Record not found. 
6A 84 There is insufficient memory space in record or file. 
6A 87 The P3 value is not consistent with the P1 and P2 values. 
6A 88 Referenced data not found. 
6C XX Incorrect P3 length.


Excerpt from ISO/IEC 7816 part 4:

Due to specifications in part 3 of ISO/IEC 7816, this part does not define the
following values of SW1-SW2 :

'60XX'
'67XX', '6BXX', '6DXX', '6EXX', '6FXX'; in each case if 'XX'!='00'
'9XXX', if 'XXX'!='000'
The following values of SW1-SW2 are defined whichever protocol is used (see
examples in annex A).

If a command is aborted with a response where SW1='6C', then SW2 indicates the
value to be given to the short Le field (exact length of requested data) when
re-issuing the same command before issuing any other command.
If a command (which may be of case 2 or 4, see table 4 and figure 4) is
processed with a response where SW1='61', then SW2 indicates the maximum value
to be given to the short Le field (length of extra data still available) in
a GET RESPONSE command issued before issuing any other command.
NOTE - A functionality similar to that offered by '61XX' may be offered at
application level by '9FXX'. However, applications may use '9FXX' for other
purposes.

Table 12 completed by tables 13 to 18 shows the general meanings of the values
of SW1-SW2 defined in this part of ISO/IEC 7816. For each command, an
appropriate clause provides more detailed meanings.

Tables 13 to 18 specify values of SW2 when SW1 is valued to '62', '63', '65',
'68', '69' and '6A'. The values of SW2 not defined in tables 13 to 18 are RFU,
except the values from 'F0' to 'FF' which are not defined in this part of
ISO/IEC 7816.


Table 12 - Coding of SW1-SW2

SW1-SW2	Meaning
Normal processing
'9000'	No further qualification
'61XX'	SW2 indicates the number of response bytes still available
(see text below)
Warning processings
'62XX'	State of non-volatile memory unchanged (further qualification in SW2,
see table 13)
'63XX'	State of non-volatile memory changed (further qualification in SW2,
see table 14)
Execution errors
'64XX'	State of non-volatile memory unchanged (SW2='00', other values are RFU)
'65XX'	State of non-volatile memory changed (further qualification in SW2,
see table 15)
'66XX'	Reserved for security-related issues (not defined in this part of
ISO/IEC 7816)
Checking errors
'6700'	Wrong length
'68XX'	Functions in CLA not supported (further qualification in SW2, see
table 16)
'69XX'	Command not allowed (further qualification in SW2, see table 17)
'6AXX'	Wrong parameter(s) P1-P2 (further qualification in SW2, see table 18)
'6B00'	Wrong parameter(s) P1-P2
'6CXX'	Wrong length Le: SW2 indicates the exact length (see text below)
'6D00'	Instruction code not supported or invalid
'6E00'	Class not supported
'6F00'	No precise diagnosis

Table 13 - Coding of SW2 when SW1='62'

SW2	Meaning
'00'	No information given
'81'	Part of returned data may be corrupted
'82'	End of file/record reached before reading Le bytes
'83'	Selected file invalidated
'84'	FCI not formatted according to 1.1.5

Table 14 - Coding of SW2 when SW1='63'

SW2	Meaning
'00'	No information given
'81'	File filled up by the last write
'CX'	Counter provided by 'X' (valued from 0 to 15) (exact meaning depending
on the command)

Table 15 - Coding of SW2 when SW1='65'

SW2	Meaning
'00'	No information given
'81'	Memory failure

Table 16 - Coding of SW2 when SW1='68'

SW2	Meaning
'00'	No information given
'81'	Logical channel not supported
'82'	Secure messaging not supported

Table 17 - Coding of SW2 when SW1='69'

SW2	Meaning
'00'	No information given
'81'	Command incompatible with file structure
'82'	Security status not satisfied
'83'	Authentication method blocked
'84'	Referenced data invalidated
'85'	Conditions of use not satisfied
'86'	Command not allowed (no current EF)
'87'	Expected SM data objects missing
'88'	SM data objects incorrect

Table 18 - Coding of SW2 when SW1='6A'

SW2	Meaning
'00'	No information given
'80'	Incorrect parameters in the data field
'81'	Function not supported
'82'	File not found
'83'	Record not found
'84'	Not enough memory space in the file
'85'	Lc inconsistent with TLV structure
'86'	Incorrect parameters P1-P2
'87'	Lc inconsistent with P1-P2
'88'	Referenced data not found

*/

//
// SCardError exceptions
//
SCardError::SCardError(uint16_t sw) : statusWord(sw)
{
	IFDEBUG(debugDiagnose(this));
}

const char *SCardError::what() const throw ()
{ return "SCardError"; }

OSStatus SCardError::osStatus() const
{
    switch (statusWord)
    {
	case SCARD_SUCCESS:
		return 0;

	case SCARD_FILE_FILLED:
	case SCARD_MEMORY_FAILURE:
	case SCARD_NO_MEMORY_LEFT:
		return CSSM_ERRCODE_MEMORY_ERROR;

	case SCARD_AUTHENTICATION_FAILED:
	case SCARD_AUTHENTICATION_FAILED_0:
	case SCARD_AUTHENTICATION_FAILED_1:
	case SCARD_AUTHENTICATION_FAILED_2:
	case SCARD_AUTHENTICATION_FAILED_3:
	case SCARD_AUTHENTICATION_FAILED_4:
	case SCARD_AUTHENTICATION_FAILED_5:
	case SCARD_AUTHENTICATION_FAILED_6:
	case SCARD_AUTHENTICATION_FAILED_7:
	case SCARD_AUTHENTICATION_FAILED_8:
	case SCARD_AUTHENTICATION_FAILED_9:
	case SCARD_AUTHENTICATION_FAILED_10:
	case SCARD_AUTHENTICATION_FAILED_11:
	case SCARD_AUTHENTICATION_FAILED_12:
	case SCARD_AUTHENTICATION_FAILED_13:
	case SCARD_AUTHENTICATION_FAILED_14:
	case SCARD_AUTHENTICATION_FAILED_15:
	case SCARD_AUTHENTICATION_BLOCKED:
        return CSSM_ERRCODE_OPERATION_AUTH_DENIED;

	case SCARD_COMMAND_NOT_ALLOWED:
	case SCARD_NOT_AUTHORIZED:
	case SCARD_USE_CONDITIONS_NOT_MET:
        return CSSM_ERRCODE_OBJECT_USE_AUTH_DENIED;

	case SCARD_FUNCTION_NOT_SUPPORTED:
	case SCARD_INSTRUCTION_CODE_INVALID:
		return CSSM_ERRCODE_FUNCTION_NOT_IMPLEMENTED;

	case SCARD_FILE_NOT_FOUND:
	case SCARD_RECORD_NOT_FOUND:
		return CSSMERR_DL_RECORD_NOT_FOUND;

	case SCARD_BYTES_LEFT_IN_SW2:
	case SCARD_EXECUTION_WARNING:
	case SCARD_RETURNED_DATA_CORRUPTED:
	case SCARD_END_OF_FILE_REACHED:
	case SCARD_FILE_INVALIDATED:
	case SCARD_FCI_INVALID:
	case SCARD_EXECUTION_ERROR:
	case SCARD_CHANGED_ERROR:
	case SCARD_LENGTH_INCORRECT:
	case SCARD_CLA_UNSUPPORTED:
	case SCARD_LOGICAL_CHANNEL_UNSUPPORTED:
	case SCARD_SECURE_MESSAGING_UNSUPPORTED:
	case SCARD_COMMAND_INCOMPATIBLE:
	case SCARD_REFERENCED_DATA_INVALIDATED:
	case SCARD_NO_CURRENT_EF:
	case SCARD_SM_DATA_OBJECTS_MISSING:
	case SCARD_SM_DATA_NOT_ALLOWED:
	case SCARD_WRONG_PARAMETER:
	case SCARD_DATA_INCORRECT:
	case SCARD_LC_INCONSISTENT_TLV:
	case SCARD_INCORRECT_P1_P2:
	case SCARD_LC_INCONSISTENT_P1_P2:
	case SCARD_REFERENCED_DATA_NOT_FOUND:
	case SCARD_WRONG_PARAMETER_P1_P2:
	case SCARD_LE_IN_SW2:
	case SCARD_INSTRUCTION_CLASS_UNSUPPORTED:
	case SCARD_UNSPECIFIED_ERROR:
    default:
        return CSSM_ERRCODE_INTERNAL_ERROR;
    }
}

int SCardError::unixError() const
{
	switch (statusWord)
	{
        default:
            // cannot map this to errno space
            return -1;
    }
}

void SCardError::throwMe(uint16_t sw)
{ throw SCardError(sw); }

#if !defined(NDEBUG)

void SCardError::debugDiagnose(const void *id) const
{
    secdebug("exception", "%p Error %s (%04hX)",
             id, errorstr(statusWord), statusWord);
}

const char *SCardError::errorstr(uint16_t sw)
{
    switch (sw)
	{
	case SCARD_SUCCESS:
		return "Success";
	case SCARD_BYTES_LEFT_IN_SW2:
		return "SW2 indicates the number of response bytes still available";
	case SCARD_EXECUTION_WARNING:
		return "Execution warning, state of non-volatile memory unchanged";
	case SCARD_RETURNED_DATA_CORRUPTED:
		return "Part of returned data may be corrupted.";
	case SCARD_END_OF_FILE_REACHED:
		return "End of file/record reached before reading Le bytes.";
	case SCARD_FILE_INVALIDATED:
		return "Selected file invalidated.";
	case SCARD_FCI_INVALID:
		return "FCI not formatted according to 1.1.5.";
	case SCARD_AUTHENTICATION_FAILED:
		return "Authentication failed.";
	case SCARD_FILE_FILLED:
		return "File filled up by the last write.";
	case SCARD_AUTHENTICATION_FAILED_0:
		return "Authentication failed, 0 retries left.";
	case SCARD_AUTHENTICATION_FAILED_1:
		return "Authentication failed, 1 retry left.";
	case SCARD_AUTHENTICATION_FAILED_2:
		return "Authentication failed, 2 retries left.";
	case SCARD_AUTHENTICATION_FAILED_3:
		return "Authentication failed, 3 retries left.";
	case SCARD_AUTHENTICATION_FAILED_4:
		return "Authentication failed, 4 retries left.";
	case SCARD_AUTHENTICATION_FAILED_5:
		return "Authentication failed, 5 retries left.";
	case SCARD_AUTHENTICATION_FAILED_6:
		return "Authentication failed, 6 retries left.";
	case SCARD_AUTHENTICATION_FAILED_7:
		return "Authentication failed, 7 retries left.";
	case SCARD_AUTHENTICATION_FAILED_8:
		return "Authentication failed, 8 retries left.";
	case SCARD_AUTHENTICATION_FAILED_9:
		return "Authentication failed, 9 retries left.";
	case SCARD_AUTHENTICATION_FAILED_10:
		return "Authentication failed, 10 retries left.";
	case SCARD_AUTHENTICATION_FAILED_11:
		return "Authentication failed, 11 retries left.";
	case SCARD_AUTHENTICATION_FAILED_12:
		return "Authentication failed, 12 retries left.";
	case SCARD_AUTHENTICATION_FAILED_13:
		return "Authentication failed, 13 retries left.";
	case SCARD_AUTHENTICATION_FAILED_14:
		return "Authentication failed, 14 retries left.";
	case SCARD_AUTHENTICATION_FAILED_15:
		return "Authentication failed, 15 retries left.";
	case SCARD_EXECUTION_ERROR:
		return "Execution error, state of non-volatile memory unchanged.";
	case SCARD_CHANGED_ERROR:
		return "Execution error, state of non-volatile memory changed.";
	case SCARD_MEMORY_FAILURE:
		return "Memory failure.";
	case SCARD_LENGTH_INCORRECT:
		return "The length is incorrect.";
	case SCARD_CLA_UNSUPPORTED:
		return "Functions in CLA not supported.";
	case SCARD_LOGICAL_CHANNEL_UNSUPPORTED:
		return "Logical channel not supported.";
	case SCARD_SECURE_MESSAGING_UNSUPPORTED:
		return "Secure messaging not supported.";
	case SCARD_COMMAND_NOT_ALLOWED:
		return "Command not allowed.";
	case SCARD_COMMAND_INCOMPATIBLE:
		return "Command incompatible with file structure.";
	case SCARD_NOT_AUTHORIZED:
		return "Security status not satisfied.";
	case SCARD_AUTHENTICATION_BLOCKED:
		return "Authentication method blocked.";
	case SCARD_REFERENCED_DATA_INVALIDATED:
		return "Referenced data invalidated.";
	case SCARD_USE_CONDITIONS_NOT_MET:
		return "Conditions of use not satisfied.";
	case SCARD_NO_CURRENT_EF:
		return "Command not allowed (no current EF).";
	case SCARD_SM_DATA_OBJECTS_MISSING:
		return "Expected SM data objects missing.";
	case SCARD_SM_DATA_NOT_ALLOWED:
		return "SM data objects incorrect.";
	case SCARD_WRONG_PARAMETER:
		return "Wrong parameter.";
	case SCARD_DATA_INCORRECT:
		return "Incorrect parameters in the data field.";
	case SCARD_FUNCTION_NOT_SUPPORTED:
		return "Function not supported.";
	case SCARD_FILE_NOT_FOUND:
		return "File not found.";
	case SCARD_RECORD_NOT_FOUND:
		return "Record not found.";
	case SCARD_NO_MEMORY_LEFT:
		return "Not enough memory space in the file.";
	case SCARD_LC_INCONSISTENT_TLV:
		return "Lc inconsistent with TLV structure.";
	case SCARD_INCORRECT_P1_P2:
		return "Incorrect parameters P1-P2.";
	case SCARD_LC_INCONSISTENT_P1_P2:
		return "Lc inconsistent with P1-P2.";
	case SCARD_REFERENCED_DATA_NOT_FOUND:
		return "Referenced data not found.";
	case SCARD_WRONG_PARAMETER_P1_P2:
		return "Wrong parameter(s) P1-P2.";
	case SCARD_LE_IN_SW2:
		return "Wrong length Le: SW2 indicates the exact length";
	case SCARD_INSTRUCTION_CODE_INVALID:
		return "The instruction code is not programmed or is invalid.";
	case SCARD_INSTRUCTION_CLASS_UNSUPPORTED:
		return "The card does not support the instruction class.";
	case SCARD_UNSPECIFIED_ERROR:
		return "No precise diagnostic is given.";
	default:
		return "Unknown error";
	}
}

#endif //NDEBUG

} // end namespace Tokend

