//
//  SecOTRErrors.h
//  libsecurity_libSecOTR
//
//  Created by Keith Henrickson on 4/30/12.
//
//

#ifndef messsageProtection_SecMessageProtectionErrors_h
#define messsageProtection_SecMessageProtectionErrors_h

static const CFIndex kSecOTRErrorFailedToEncrypt = -1;
static const CFIndex kSecOTRErrorFailedToDecrypt = -2;
static const CFIndex kSecOTRErrorFailedToVerify = -3;
static const CFIndex kSecOTRErrorFailedToSign = -4;
static const CFIndex kSecOTRErrorSignatureDidNotMatch = -5;
static const CFIndex kSecOTRErrorFailedSelfTest = -6;
static const CFIndex kSecOTRErrorParameterError = -7;
static const CFIndex kSecOTRErrorUnknownFormat = -8;
static const CFIndex kSecOTRErrorCreatePublicIdentity = -9;
static const CFIndex kSecOTRErrorCreatePublicBytes = -10;

// Errors 100-199 reserved for errors being genrated by workarounds/known issues failing
static const CFIndex kSecOTRErrorSignatureTooLarge = -100;
static const CFIndex kSecOTRErrorSignatureDidNotRecreate = -101;

#endif
