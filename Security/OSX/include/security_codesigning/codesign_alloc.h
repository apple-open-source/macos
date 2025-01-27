/*
 * Copyright (c) 2021 Apple Inc. All Rights Reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 *
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 *
 * @APPLE_LICENSE_HEADER_END@
 */

#ifdef __cplusplus
extern "C" {
#endif

//
// Reads from (possibly fat) mach-o file at path 'existingFilePath'
// and writes a new file at path 'newOutputFilePath'.
// Returns true on success, otherwise returns false and the errorMessage
// parameter is set to a malloced failure messsage.
// For each mach-o slice in the file, the block 'getSigSpaceNeeded' is called
// and expected to return the amount of space need for the code signature.
//
extern bool code_sign_allocate(const char* existingFilePath,
                               const char* newOutputFilePath,
                               unsigned int (^getSigSpaceNeeded)(cpu_type_t cputype, cpu_subtype_t cpusubtype),
                               char*& errorMessage);


//
// Reads from (possibly fat) mach-o file at path 'existingFilePath'
// and writes a new file at path 'newOutputFilePath'.  For each
// mach-o slice in the file, if there is an existing code signature,
// the code signature data in the LINKEDIT is removed along with
// the LC_CODE_SIGNATURE load command.  Returns true on success,
// otherwise returns false and the errorMessage parameter is set
// to a malloced failure messsage.
//
bool code_sign_deallocate(const char* existingFilePath,
                          const char* newOutputFilePath,
                          char*& errorMessage);

#ifdef __cplusplus
}
#endif


