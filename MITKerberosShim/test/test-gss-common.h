/*
 * common.h - gssSample helper functions
 *
 * Copyright 2004-2005 Massachusetts Institute of Technology.
 * All Rights Reserved.
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

#define kDefaultPort 2000

int ReadToken (int inSocket, char **outTokenValue, size_t *outTokenLength);
int WriteToken (int inSocket, const char *inTokenValue, size_t inTokenLength);

int ReadEncryptedToken (int inSocket, const gss_ctx_id_t inContext, char **outTokenValue, size_t *outTokenLength);
int WriteEncryptedToken (int inSocket, const gss_ctx_id_t inContext, const char *inToken, size_t inTokenLength);

void printError (int inError, const char *inString);
void printGSSErrors (const char *inRoutineName, OM_uint32 inMajorStatus, OM_uint32 inMinorStatus);