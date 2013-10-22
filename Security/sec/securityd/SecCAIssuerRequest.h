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
#include <dispatch/dispatch.h>

bool SecCAIssuerCopyParents(SecCertificateRef certificate,
    dispatch_queue_t queue, void *context, void (*callback)(void *, CFArrayRef));
