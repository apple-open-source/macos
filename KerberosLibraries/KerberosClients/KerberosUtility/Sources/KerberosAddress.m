/*
 * KerberosAddress.m
 *
 * $Header$
 *
 * Copyright 2004 Massachusetts Institute of Technology.
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

#import "KerberosAddress.h"

@implementation KerberosAddress

// ---------------------------------------------------------------------------

- (id) initWithType: (krb5_addrtype) type length: (unsigned int) length contents: (krb5_octet *) contents
{
    if ((self = [super init])) {
        address.magic = KV5M_ADDRESS;
        address.addrtype = type;
        address.length = length;
        address.contents = (krb5_octet *) malloc (length);
        if (address.contents) {
            memcpy (address.contents, contents, length);
        } else {
            [super dealloc];
            self = nil;
        }
    }
    return self;
}

// ---------------------------------------------------------------------------

- (void) dealloc
{
    if (address.contents) { free (address.contents); }
    [super dealloc];
}

// ---------------------------------------------------------------------------

- (krb5_address *) krb5_address
{
    return &address;
}

// ---------------------------------------------------------------------------

- (NSString *) stringValue
{
    NSString *string = NULL;
    char addressString[INET6_ADDRSTRLEN+1];
    int family = (address.addrtype == ADDRTYPE_INET6) ? AF_INET6 : AF_INET;
    
    if (inet_ntop (family, address.contents, addressString, sizeof(addressString))) {
        string = [NSString stringWithCString: addressString];
    }
    
    return (string) ? string : @"error";
}

@end
