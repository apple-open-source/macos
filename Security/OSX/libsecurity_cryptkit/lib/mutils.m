/* Copyright (c) 1998,2011,2014 Apple Inc.  All Rights Reserved.
 *
 * NOTICE: USE OF THE MATERIALS ACCOMPANYING THIS NOTICE IS SUBJECT
 * TO THE TERMS OF THE SIGNED "FAST ELLIPTIC ENCRYPTION (FEE) REFERENCE
 * SOURCE CODE EVALUATION AGREEMENT" BETWEEN APPLE, INC. AND THE
 * ORIGINAL LICENSEE THAT OBTAINED THESE MATERIALS FROM APPLE,
 * INC.  ANY USE OF THESE MATERIALS NOT PERMITTED BY SUCH AGREEMENT WILL
 * EXPOSE YOU TO LIABILITY.
 ***************************************************************************
 *
 * mutils.m - general private ObjC routine declarations
 *
 * Revision History
 * ----------------
 *  2 Aug 96 at NeXT
 *	Broke out from Blaine Garst's original NSCryptors.m
 */

#import <Foundation/Foundation.h>
#import "giantIntegers.h"
#import "ckutilities.h"
#import "mutils.h"
#import "feeFunctions.h"
#import <libc.h>

#if	defined(NeXT) && !defined(WIN32)

/*
 * Public, declared in NSCryptors.h
 */
NSString *NSPromptForPassPhrase(NSString *prompt) {
        // useful for command line (/dev/tty) programs
    char buffer[PHRASELEN];
    NSString *result;

    getpassword([prompt cString], buffer);
    if (buffer[0] == 0) return nil;
    result = [NSString stringWithCString:buffer];
    bzero(buffer, PHRASELEN);
    return result;
}


#endif NeXT
