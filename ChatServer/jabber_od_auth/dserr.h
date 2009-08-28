/*
 *  dserr.h
 *
 *  korver@apple.com
 *
 *  Copyright (c) 2009, Apple Inc. All rights reserved.
 */

#ifndef __DSERR_H__
#define __DSERR_H__

#define IS_EXPECTED_DS_ERROR(err) (\
    ((err) == eDSNoErr) /* nothing went wrong at all */ \
    || ((err) == eDSAuthBadPassword) \
    || ((err) == eDSAuthFailed) \
    || ((err) == eDSAuthInvalidUserName) \
    || ((err) == eDSAuthMethodNotSupported) \
    || ((err) == eDSAuthNewPasswordRequired) \
    || ((err) == eDSAuthPasswordExpired) \
    || ((err) == eDSAuthUnknownUser) \
    || ((err) == eDSInvalidName) \
    )

#endif
