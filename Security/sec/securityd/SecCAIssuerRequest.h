/*
 *  SecCAIssuerRequest.h
 *  Security
 *
 *  Created by Michael Brouwer on 9/17/09.
 *  Copyright (c) 2009-2010 Apple Inc.. All Rights Reserved.
 *
 */

#include <Security/SecCertificate.h>
#include <CoreFoundation/CFArray.h>

bool SecCAIssuerCopyParents(SecCertificateRef certificate,
    void *context, void (*callback)(void *, CFArrayRef));
