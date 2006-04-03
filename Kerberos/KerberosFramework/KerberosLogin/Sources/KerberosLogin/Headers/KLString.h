/*
 * KLString.h
 *
 * $Header$
 *
 * Copyright 2003 Massachusetts Institute of Technology.
 * All Rights Reserved.
 *
 * Export of this software from the United States of America may
 * require a specific license from the United States Government.
 * It is the responsibility of any person or organization contemplating
 * export to obtain such a license before exporting.
 *
 * WITHIN THAT CONSTRAINT, permission to use, copy, modify, and
 * distribute this software and its documentation for any purpose and
 * without fee is hereby granted, provided that the above copyright
 * notice appear in all copies and that both that copyright notice and
 * this permission notice appear in supporting documentation, and that
 * the name of M.I.T. not be used in advertising or publicity pertaining
 * to distribution of the software without specific, written prior
 * permission.  Furthermore if you modify this software you must label
 * your software as modified software and not distribute it in such a
 * fashion that it might be confused with the original M.I.T. software.
 * M.I.T. makes no representations about the suitability of
 * this software for any purpose.  It is provided "as is" without express
 * or implied warranty.
 */

/* Error handling syslogs for debugging purposes */

KLStatus __KerberosLoginError (KLStatus inError, const char *function, const char *file, int line);
KLStatus __KLRemapKerberos4Error (int err);

#define KLError_(err) __KerberosLoginError(err, __FUNCTION__, __FILE__, __LINE__)

/* String utilities */

KLStatus __KLCreateString (const char *inString, char **outString);
KLStatus __KLCreateStringFromCFString (CFStringRef inString, CFStringEncoding inEncoding, char **outString);
KLStatus __KLCreateStringFromBuffer (const char *inBuffer, KLIndex inBufferLength, char **outString);
KLStatus __KLAddPrefixToString (const char *inPrefix, char **ioString);
KLStatus __KLAppendToString (const char *inAppendString, char **ioString);
CFStringEncoding __KLApplicationGetTextEncoding (void);
KLStatus __KLGetLocalizedString (const char *inKeyString, char **outString);
KLStatus __KLGetApplicationPathString (char **outApplicationPath);

/* StringArray utilities */

struct OpaqueKLStringArray;
typedef struct OpaqueKLStringArray *KLStringArray;

extern const KLStringArray kKLEmptyStringArray;

KLStatus __KLCreateStringArray (KLStringArray *outArray);
KLStatus __KLCreateStringArrayFromStringArray (KLStringArray inArray, KLStringArray *outArray);
KLStatus __KLStringArrayGetStringCount (KLStringArray inArray, KLIndex *outCount);
KLStatus __KLStringArrayGetStringAtIndex (KLStringArray inArray, KLIndex inIndex, char **outString);
KLStatus __KLStringArrayCopyStringAtIndex (KLStringArray inArray, KLIndex inIndex, char **outString);
KLStatus __KLStringArrayGetIndexForString (KLStringArray inArray, const char *inString, KLIndex *outIndex);
KLStatus __KLStringArraySetStringAtIndex (KLStringArray inArray, const char *inString, KLIndex inIndex);
KLStatus __KLStringArrayInsertStringBeforeIndex (KLStringArray inArray, const char *inString, KLIndex inIndex);
KLStatus __KLStringArrayAddString (KLStringArray inArray, const char *inString);
KLStatus __KLStringArrayRemoveStringAtIndex (KLStringArray inArray, KLIndex inIndex);
KLStatus __KLDisposeStringArray (KLStringArray inArray);

KLStatus __KLVerifyKDCOffsetsForKerberos4 (const krb5_context inContext);
