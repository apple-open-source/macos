/*
 * Copyright (c) 2002-2004,2012 Apple Inc. All Rights Reserved.
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

//
// Policy.cpp - Working with Policies
//
#include <security_keychain/Policies.h>
#include <security_utilities/debugging.h>
#include <Security/oidsalg.h>
#include <sys/param.h>

/* Oids longer than this are considered invalid. */
#define MAX_OID_SIZE				32

//%%FIXME: need to use a common copy of this utility function
static
CFStringRef SecDERItemCopyOIDDecimalRepresentation(uint8 *oid, size_t oidLen)
{
	if (oidLen == 0)
		return CFSTR("<NULL>");

	if (oidLen > MAX_OID_SIZE)
		return CFSTR("Oid too long");

	CFMutableStringRef result = CFStringCreateMutable(kCFAllocatorDefault, 0);

	// The first two levels are encoded into one byte, since the root level
	// has only 3 nodes (40*x + y).  However if x = joint-iso-itu-t(2) then
	// y may be > 39, so we have to add special-case handling for this.
	uint32_t x = oid[0] / 40;
	uint32_t y = oid[0] % 40;
	if (x > 2)
	{
		// Handle special case for large y if x = 2
		y += (x - 2) * 40;
		x = 2;
	}
	CFStringAppendFormat(result, NULL, CFSTR("%u.%u"), x, y);

	unsigned long value = 0;
	for (x = 1; x < oidLen; ++x)
	{
		value = (value << 7) | (oid[x] & 0x7F);
		/* @@@ value may not span more than 4 bytes. */
		/* A max number of 20 values is allowed. */
		if (!(oid[x] & 0x80))
		{
			CFStringAppendFormat(result, NULL, CFSTR(".%lu"), value);
			value = 0;
		}
	}
	return result;
}


using namespace KeychainCore;

Policy::Policy(TP supportingTp, const CssmOid &policyOid)
    : mTp(supportingTp),
      mOid(Allocator::standard(), policyOid),
      mValue(Allocator::standard()),
      mAuxValue(Allocator::standard())
{
	// value is as yet unimplemented
	secdebug("policy", "Policy() this %p", this);
}

Policy::~Policy() throw()
{
	secdebug("policy", "~Policy() this %p", this);
}

void Policy::setValue(const CssmData &value)
{
	StLock<Mutex>_(mMutex);
	mValue = value;
	mAuxValue.reset();

	// Certain policy values may contain an embedded pointer. Ask me how I feel about that.
	if (mOid == CSSMOID_APPLE_TP_SSL ||
		mOid == CSSMOID_APPLE_TP_EAP ||
		mOid == CSSMOID_APPLE_TP_IP_SEC ||
		mOid == CSSMOID_APPLE_TP_APPLEID_SHARING)
	{
		CSSM_APPLE_TP_SSL_OPTIONS *opts = (CSSM_APPLE_TP_SSL_OPTIONS *)value.data();
		if (opts->Version == CSSM_APPLE_TP_SSL_OPTS_VERSION)
		{
			if (opts->ServerNameLen > 0)
			{
				// Copy auxiliary data, then update the embedded pointer to reference our copy
				mAuxValue.copy(const_cast<char*>(opts->ServerName), opts->ServerNameLen);
				mValue.get().interpretedAs<CSSM_APPLE_TP_SSL_OPTIONS>()->ServerName =
					reinterpret_cast<char*>(mAuxValue.data());
			}
			else
			{
				// Clear the embedded pointer!
				mValue.get().interpretedAs<CSSM_APPLE_TP_SSL_OPTIONS>()->ServerName =
					reinterpret_cast<char*>(NULL);
			}
		}
	}
	else if (mOid == CSSMOID_APPLE_TP_SMIME ||
			mOid == CSSMOID_APPLE_TP_ICHAT ||
			mOid == CSSMOID_APPLE_TP_PASSBOOK_SIGNING)
	{
		CSSM_APPLE_TP_SMIME_OPTIONS *opts = (CSSM_APPLE_TP_SMIME_OPTIONS *)value.data();
		if (opts->Version == CSSM_APPLE_TP_SMIME_OPTS_VERSION)
		{
			if (opts->SenderEmailLen > 0)
			{
				// Copy auxiliary data, then update the embedded pointer to reference our copy
				mAuxValue.copy(const_cast<char*>(opts->SenderEmail), opts->SenderEmailLen);
				mValue.get().interpretedAs<CSSM_APPLE_TP_SMIME_OPTIONS>()->SenderEmail =
					reinterpret_cast<char*>(mAuxValue.data());
			}
			else
			{
				// Clear the embedded pointer!
				mValue.get().interpretedAs<CSSM_APPLE_TP_SMIME_OPTIONS>()->SenderEmail =
					reinterpret_cast<char*>(NULL);
			}
		}
	}
}

void Policy::setProperties(CFDictionaryRef properties)
{
	// Set the policy value based on the provided dictionary keys.
	if (properties == NULL)
		return;

	if (mOid == CSSMOID_APPLE_TP_SSL ||
		mOid == CSSMOID_APPLE_TP_EAP ||
		mOid == CSSMOID_APPLE_TP_IP_SEC ||
		mOid == CSSMOID_APPLE_TP_APPLEID_SHARING)
	{
		CSSM_APPLE_TP_SSL_OPTIONS options = { CSSM_APPLE_TP_SSL_OPTS_VERSION, 0, NULL, 0 };
		char *buf = NULL;
		CFStringRef nameStr = NULL;
		if (CFDictionaryGetValueIfPresent(properties, (const void *)kSecPolicyName, (const void **)&nameStr)) {
			buf = (char *)malloc(MAXPATHLEN);
			if (buf) {
				if (CFStringGetCString(nameStr, buf, MAXPATHLEN, kCFStringEncodingUTF8)) {
					options.ServerName = buf;
					options.ServerNameLen = (unsigned)(strlen(buf)+1); // include terminating null
				}
			}
		}
		CFBooleanRef clientRef = NULL;
		if (CFDictionaryGetValueIfPresent(properties, (const void *)kSecPolicyClient, (const void **)&clientRef)
			&& CFBooleanGetValue(clientRef) == true)
			options.Flags |= CSSM_APPLE_TP_SSL_CLIENT;

		const CssmData value((uint8*)&options, sizeof(options));
		this->setValue(value);

		if (buf) free(buf);
	}
	else if (mOid == CSSMOID_APPLE_TP_SMIME ||
			mOid == CSSMOID_APPLE_TP_ICHAT ||
			mOid == CSSMOID_APPLE_TP_PASSBOOK_SIGNING)
	{
		CSSM_APPLE_TP_SMIME_OPTIONS options = { CSSM_APPLE_TP_SMIME_OPTS_VERSION, 0, 0, NULL };
		char *buf = NULL;
		CFStringRef nameStr = NULL;
		if (CFDictionaryGetValueIfPresent(properties, (const void *)kSecPolicyName, (const void **)&nameStr)) {
			buf = (char *)malloc(MAXPATHLEN);
			if (buf) {
				if (CFStringGetCString(nameStr, buf, MAXPATHLEN, kCFStringEncodingUTF8)) {
					CFStringRef teamIDStr = NULL;
					if (CFDictionaryGetValueIfPresent(properties, (const void *)kSecPolicyTeamIdentifier, (const void **)&teamIDStr)) {
						char *buf2 = (char *)malloc(MAXPATHLEN);
						if (buf2) {
							if (CFStringGetCString(teamIDStr, buf2, MAXPATHLEN, kCFStringEncodingUTF8)) {
								/* append tab separator and team identifier */
								strlcat(buf, "\t", MAXPATHLEN);
								strlcat(buf, buf2, MAXPATHLEN);
							}
							free(buf2);
						}
					}
					options.SenderEmail = buf;
					options.SenderEmailLen = (unsigned)(strlen(buf)+1); // include terminating null
				}
			}
		}
		CFBooleanRef kuRef = NULL;
		if (CFDictionaryGetValueIfPresent(properties, (const void *)kSecPolicyKU_DigitalSignature, (const void **)&kuRef)
			&& CFBooleanGetValue(kuRef) == true)
			options.IntendedUsage |= CE_KU_DigitalSignature;
		if (CFDictionaryGetValueIfPresent(properties, (const void *)kSecPolicyKU_NonRepudiation, (const void **)&kuRef)
			&& CFBooleanGetValue(kuRef) == true)
			options.IntendedUsage |= CE_KU_NonRepudiation;
		if (CFDictionaryGetValueIfPresent(properties, (const void *)kSecPolicyKU_KeyEncipherment, (const void **)&kuRef)
			&& CFBooleanGetValue(kuRef) == true)
			options.IntendedUsage |= CE_KU_KeyEncipherment;
		if (CFDictionaryGetValueIfPresent(properties, (const void *)kSecPolicyKU_DataEncipherment, (const void **)&kuRef)
			&& CFBooleanGetValue(kuRef) == true)
			options.IntendedUsage |= CE_KU_DataEncipherment;
		if (CFDictionaryGetValueIfPresent(properties, (const void *)kSecPolicyKU_KeyAgreement, (const void **)&kuRef)
			&& CFBooleanGetValue(kuRef) == true)
			options.IntendedUsage |= CE_KU_KeyAgreement;
		if (CFDictionaryGetValueIfPresent(properties, (const void *)kSecPolicyKU_KeyCertSign, (const void **)&kuRef)
			&& CFBooleanGetValue(kuRef) == true)
			options.IntendedUsage |= CE_KU_KeyCertSign;
		if (CFDictionaryGetValueIfPresent(properties, (const void *)kSecPolicyKU_CRLSign, (const void **)&kuRef)
			&& CFBooleanGetValue(kuRef) == true)
			options.IntendedUsage |= CE_KU_CRLSign;
		if (CFDictionaryGetValueIfPresent(properties, (const void *)kSecPolicyKU_EncipherOnly, (const void **)&kuRef)
			&& CFBooleanGetValue(kuRef) == true)
			options.IntendedUsage |= CE_KU_EncipherOnly;
		if (CFDictionaryGetValueIfPresent(properties, (const void *)kSecPolicyKU_DecipherOnly, (const void **)&kuRef)
			&& CFBooleanGetValue(kuRef) == true)
			options.IntendedUsage |= CE_KU_DecipherOnly;

		const CssmData value((uint8*)&options, sizeof(options));
		this->setValue(value);

		if (buf) free(buf);
    }

}

CFDictionaryRef Policy::properties()
{
	// Builds and returns a dictionary which the caller must release.
	CFMutableDictionaryRef properties = CFDictionaryCreateMutable(NULL, 0,
		&kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
	if (!properties) return NULL;

	// kSecPolicyOid
	CFStringRef oidStr = SecDERItemCopyOIDDecimalRepresentation((uint8*)mOid.data(), mOid.length());
	if (oidStr) {
		CFDictionarySetValue(properties, (const void *)kSecPolicyOid, (const void *)oidStr);
		CFRelease(oidStr);
	}

	// kSecPolicyName
	if (mAuxValue) {
		CFStringRef nameStr = CFStringCreateWithBytes(NULL,
			(const UInt8 *)reinterpret_cast<char*>(mAuxValue.data()),
			(CFIndex)mAuxValue.length(), kCFStringEncodingUTF8, false);
		if (nameStr) {
			if (mOid == CSSMOID_APPLE_TP_PASSBOOK_SIGNING) {
				CFArrayRef strs = CFStringCreateArrayBySeparatingStrings(kCFAllocatorDefault, nameStr, CFSTR("\t"));
				if (strs) {
					CFIndex count = CFArrayGetCount(strs);
					if (count > 0)
						CFDictionarySetValue(properties, (const void *)kSecPolicyName, (const void *)CFArrayGetValueAtIndex(strs, 0));
					if (count > 1)
						CFDictionarySetValue(properties, (const void *)kSecPolicyTeamIdentifier, (const void *)CFArrayGetValueAtIndex(strs, 1));
					CFRelease(strs);
				}
			}
			else {
				CFDictionarySetValue(properties, (const void *)kSecPolicyName, (const void *)nameStr);
			}
			CFRelease(nameStr);
		}
	}

	// kSecPolicyClient
	if (mValue) {
		if (mOid == CSSMOID_APPLE_TP_SSL ||
			mOid == CSSMOID_APPLE_TP_EAP ||
			mOid == CSSMOID_APPLE_TP_IP_SEC ||
			mOid == CSSMOID_APPLE_TP_APPLEID_SHARING)
		{
			CSSM_APPLE_TP_SSL_OPTIONS *opts = (CSSM_APPLE_TP_SSL_OPTIONS *)mValue.data();
			if (opts->Flags & CSSM_APPLE_TP_SSL_CLIENT) {
				CFDictionarySetValue(properties, (const void *)kSecPolicyClient, (const void *)kCFBooleanTrue);
			}
		}
	}

	// key usage flags (currently only for S/MIME and iChat policies)
	if (mValue) {
		if (mOid == CSSMOID_APPLE_TP_SMIME ||
			mOid == CSSMOID_APPLE_TP_ICHAT)
		{
			CSSM_APPLE_TP_SMIME_OPTIONS *opts = (CSSM_APPLE_TP_SMIME_OPTIONS *)mValue.data();
			CE_KeyUsage usage = opts->IntendedUsage;
			if (usage & CE_KU_DigitalSignature)
				CFDictionarySetValue(properties, (const void *)kSecPolicyKU_DigitalSignature, (const void *)kCFBooleanTrue);
			if (usage & CE_KU_NonRepudiation)
				CFDictionarySetValue(properties, (const void *)kSecPolicyKU_NonRepudiation, (const void *)kCFBooleanTrue);
			if (usage & CE_KU_KeyEncipherment)
				CFDictionarySetValue(properties, (const void *)kSecPolicyKU_KeyEncipherment, (const void *)kCFBooleanTrue);
			if (usage & CE_KU_DataEncipherment)
				CFDictionarySetValue(properties, (const void *)kSecPolicyKU_DataEncipherment, (const void *)kCFBooleanTrue);
			if (usage & CE_KU_KeyAgreement)
				CFDictionarySetValue(properties, (const void *)kSecPolicyKU_KeyAgreement, (const void *)kCFBooleanTrue);
			if (usage & CE_KU_KeyCertSign)
				CFDictionarySetValue(properties, (const void *)kSecPolicyKU_KeyCertSign, (const void *)kCFBooleanTrue);
			if (usage & CE_KU_CRLSign)
				CFDictionarySetValue(properties, (const void *)kSecPolicyKU_CRLSign, (const void *)kCFBooleanTrue);
			if (usage & CE_KU_EncipherOnly)
				CFDictionarySetValue(properties, (const void *)kSecPolicyKU_EncipherOnly, (const void *)kCFBooleanTrue);
			if (usage & CE_KU_DecipherOnly)
				CFDictionarySetValue(properties, (const void *)kSecPolicyKU_DecipherOnly, (const void *)kCFBooleanTrue);
		}
	}
	return properties;
}


bool Policy::operator < (const Policy& other) const
{
    //@@@ inefficient
    return (oid() < other.oid()) ||
        (oid() == other.oid() && value() < other.value());
}

bool Policy::operator == (const Policy& other) const
{
    return oid() == other.oid() && value() == other.value();
}
