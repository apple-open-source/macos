/*
 * Copyright (c) 2003-2004,2011,2013-2014 Apple Inc. All Rights Reserved.
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

/*
    Note: the comments that appear after these errors are used to create SecErrorMessages.strings.
    The comments must not be multi-line, and should be in a form meaningful to an end user. If
    a different or additional comment is needed, it can be put in the header doc format, or on a
    line that does not start with errZZZ.
*/

/* Definitions for miscellaneous OS errors that can be returned (with appropriate error strings) */
enum
{
    errSecMisc_nsvErr                = -35,     /* The disk couldn't be found. It may have been ejected. */
    errSecMisc_bdNamErr              = -37,     /* Tried to open a file whose name contains an illegal character. */
    errSecMisc_fnfErr                = -43,     /* The file could not be found. */
    errSecMisc_wPrErr                = -44,     /* The disk is write-protected. */
    errSecMisc_fLckdErr              = -45,     /* The file is locked. */
    errSecMisc_vLckdErr              = -46,     /* The volume is locked. */
    errSecMisc_fBsyErr               = -47,     /* The file is busy. It may be in use by another application. */
    errSecMisc_dupFNErr              = -48,     /* A file with the same name already exists. */
    errSecMisc_opWrErr               = -49,     /* The file is already open with write permission. */
    errSecMisc_volOffLinErr          = -53,     /* The volume is no longer available. It may have been ejected. */
    errSecMisc_permErr               = -54,     /* The file could not be opened. It may be in use by another application. */
    errSecMisc_extFSErr              = -58,     /* This volume does not appear to be compatible. */
    errSecMisc_wrPermErr             = -61,     /* Could not write to the file. It may have been opened with insufficient access privileges. */
    errSecMisc_offLinErr             = -65,     /* The storage device is no longer available. It may have been ejected. */
    errSecMisc_memFullErr            = -108,
    errSecMisc_dirNFErr              = -120,    /* The directory could not be found. */
    errSecMisc_volGoneErr            = -124,    /* The server volume is no longer available. It may have been disconnected. */
	errSecMisc_userCanceledErr		 = -128,	// The operation was cancelled by the user.
    errSecMisc_resNotFound           = -192,    /* A required resource could not be found. */
    errSecMisc_resFNotFound          = -193,    /* A required resource is missing or damaged. */
    errSecMisc_icNoURLErr            = -673,    /* The specified location (URL) is an unknown type, or does not contain enough information. */
    errSecMisc_icConfigNotFoundErr   = -674,    /* A helper application to open the specified URL could not be found. */
    errSecMisc_cantGetFlavorErr      = -1854,   /* The location (URL) of this item is missing or improperly formatted. */
    errSecMisc_afpAccessDenied       = -5000,   /* Access to this item was denied. */
    errSecMisc_afpUserNotAuth        = -5023,   /* Authentication failed. The password for this server may have changed since the item was added to the keychain. */
    errSecMisc_afpPwdPolicyErr       = -5046    /* This AppleShare IP server is configured to not allow users to save passwords for automatic login. Contact the server administrator for more information. */
};

