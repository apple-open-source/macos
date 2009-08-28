/*
 * Copyright (c) 2006-2007 Apple Inc. All Rights Reserved.
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
// cs_dump - codesign dump/display operations
//
#include "codesign.h"
#include <Security/CSCommonPriv.h>
#include <Security/SecCodePriv.h>
#include <security_codesigning/codedirectory.h>		// strictly for the data format

using namespace UnixPlusPlus;


//
// Local functions
//
static void extractCertificates(const char *prefix, CFArrayRef certChain);
static string flagForm(uint32_t flags);


//
// Dump a signed object's signing data.
// The more verbosity, the more data.
//
void dump(const char *target)
{
	// get the code object (static or dynamic)
	CFRef<SecStaticCodeRef> codeRef = codePath(target);	// dynamic input
	if (!codeRef)
		MacOSError::check(SecStaticCodeCreateWithPath(CFTempURL(target), kSecCSDefaultFlags, &codeRef.aref()));
	if (detached)
		if (CFRef<CFDataRef> dsig = cfLoadFile(detached))
			MacOSError::check(SecCodeSetDetachedSignature(codeRef, dsig, kSecCSDefaultFlags));
		else
			fail("%s: cannot load detached signature", detached);
	
	// get official (API driven) information
	struct Info : public CFDictionary {
		Info() : CFDictionary(errSecCSInternalError) { }
		const std::string string(CFStringRef key) { return cfString(get<CFStringRef>(key)); }
		const std::string url(CFStringRef key) { return cfString(get<CFURLRef>(key)); }
		using CFDictionary::get;	// ... and all the others
	};
	Info api;
	SecCSFlags flags = kSecCSInternalInformation
		| kSecCSSigningInformation
		| kSecCSRequirementInformation
		| kSecCSInternalInformation;
	if (modifiedFiles)
		flags |= kSecCSContentInformation;
	MacOSError::check(SecCodeCopySigningInformation(codeRef, flags, &api.aref()));
 	
	// if the code is not signed, stop here
	if (!api.get(kSecCodeInfoIdentifier))
		MacOSError::throwMe(errSecCSUnsigned);
	
	// basic stuff
	note(0, "Executable=%s", api.url(kSecCodeInfoMainExecutable).c_str());
	note(1, "Identifier=%s", api.string(kSecCodeInfoIdentifier).c_str());
	note(1, "Format=%s", api.string(kSecCodeInfoFormat).c_str());

	// code directory
	using namespace CodeSigning;
	const CodeDirectory *dir =
		(const CodeDirectory *)CFDataGetBytePtr(api.get<CFDataRef>(kSecCodeInfoCodeDirectory));
	note(1, "CodeDirectory v=%x size=%d flags=%s hashes=%d+%d location=%s",
		int(dir->version), dir->length(), flagForm(dir->flags).c_str(),
		int(dir->nCodeSlots), int(dir->nSpecialSlots), api.string(kSecCodeInfoSource).c_str());

	if (const CodeDirectory::Scatter *scatter = dir->scatterVector()) {
		const CodeDirectory::Scatter *end = scatter;
		while (end->count) end++;
		note(1, "ScatterVector count=%d", int(end - scatter));
		for (const CodeDirectory::Scatter *s = scatter; s < end; s++)
			note(3, "Scatter i=%u count=%u base=%u offset=0x%llx",
				unsigned(s - scatter), unsigned(s->count), unsigned(s->base), uint64_t(s->targetOffset));
	}

	if (verbose > 2)
		if (CFTypeRef hashInfo = api.get(kSecCodeInfoUnique)) {
			CFDataRef hash = CFDataRef(hashInfo);
			if (CFDataGetLength(hash) != sizeof(SHA1::Digest))
				note(3, "CDHash=(unknown format)");
			else
				note(3, "CDHash=%s", hashString(CFDataGetBytePtr(hash)).c_str());
		}

	// signature
	if (dir->flags & kSecCodeSignatureAdhoc) {
		note(1, "Signature=adhoc");
	} else if (CFDataRef signature = api.get<CFDataRef>(kSecCodeInfoCMS)) {
		note(1, "Signature size=%d", CFDataGetLength(signature));
		CFArrayRef certChain = api.get<CFArrayRef>(kSecCodeInfoCertificates);
		if (verbose > 1) {
			// dump cert chain
			CFIndex count = CFArrayGetCount(certChain);
			for (CFIndex n = 0; n < count; n++) {
				SecCertificateRef cert = SecCertificateRef(CFArrayGetValueAtIndex(certChain, n));
				CFRef<CFStringRef> commonName;
				MacOSError::check(SecCertificateCopyCommonName(cert, &commonName.aref()));
				note(2, "Authority=%s", cfString(commonName).c_str());
			}
		}
		if (extractCerts)
			extractCertificates(extractCerts, certChain);
		if (CFDateRef time = CFDateRef(CFDictionaryGetValue(api, kSecCodeInfoTime))) {
			CFRef<CFLocaleRef> userLocale = CFLocaleCopyCurrent();
			CFRef<CFDateFormatterRef> format = CFDateFormatterCreate(NULL, userLocale,
				kCFDateFormatterMediumStyle, kCFDateFormatterMediumStyle);
			CFRef<CFStringRef> s = CFDateFormatterCreateStringWithDate(NULL, format, time);
			note(1, "Signed Time=%s", cfString(s).c_str());
		}
	} else {
		fprintf(stderr, "%s: no signature\n", target);
		// but continue dumping
	}

	if (CFDictionaryRef info = api.get<CFDictionaryRef>(kSecCodeInfoPList))
		note(1, "Info.plist entries=%d", CFDictionaryGetCount(info));
	else
		note(1, "Info.plist=not bound");
	
	if (CFDictionaryRef resources = api.get<CFDictionaryRef>(kSecCodeInfoResourceDirectory)) {
		CFDictionaryRef rules =
			CFDictionaryRef(CFDictionaryGetValue(resources, CFSTR("rules")));
		CFDictionaryRef files
			= CFDictionaryRef(CFDictionaryGetValue(resources, CFSTR("files")));
		note(1, "Sealed Resources rules=%d files=%d",
			CFDictionaryGetCount(rules), CFDictionaryGetCount(files));
	} else
		note(1, "Sealed Resources=none");
	
	CFDataRef ireqdata = api.get<CFDataRef>(kSecCodeInfoRequirementData);
	CFRef<CFDictionaryRef> ireqset;
	if (ireqdata)
		MacOSError::check(SecRequirementsCopyRequirements(ireqdata, kSecCSDefaultFlags, &ireqset.aref()));
	if (internalReq) {
		FILE *output;
		if (!strcmp(internalReq, "-")) {
			output = stdout;
		} else if (!(output = fopen(internalReq, "w"))) {
			perror(internalReq);
			exit(exitFailure);
		}
		if (CFStringRef ireqs = api.get<CFStringRef>(kSecCodeInfoRequirements))
			fprintf(output, "%s", cfString(ireqs).c_str());
		if (!(ireqset && CFDictionaryContainsKey(ireqset, CFTempNumber(uint32_t(kSecDesignatedRequirementType))))) {	// no explicit DR
			CFRef<SecRequirementRef> dr;
			MacOSError::check(SecCodeCopyDesignatedRequirement(codeRef, kSecCSDefaultFlags, &dr.aref()));
			CFRef<CFStringRef> drstring;
			MacOSError::check(SecRequirementCopyString(dr, kSecCSDefaultFlags, &drstring.aref()));
			fprintf(output, "# designated => %s\n", cfString(drstring).c_str());
		}
		if (output != stdout)
			fclose(output);
	} else {
		if (ireqdata) {
			note(1, "Internal requirements count=%d size=%d",
				CFDictionaryGetCount(ireqset), CFDataGetLength(ireqdata));
		} else
			note(1, "Internal requirements=none");
	}

	if (entitlements)
		writeData(CFDataRef(CFDictionaryGetValue(api, kSecCodeInfoEntitlements)), entitlements, "a");

	if (modifiedFiles)
		writeFileList(CFArrayRef(CFDictionaryGetValue(api, kSecCodeInfoChangedFiles)), modifiedFiles, "a");
}


//
// Extract the entire embedded certificate chain from a signature.
// This generates DER-form certificate files, one cert per file, named
// prefix_n (where prefix is specified by the caller).
//
void extractCertificates(const char *prefix, CFArrayRef certChain)
{
	CFIndex count = CFArrayGetCount(certChain);
	for (CFIndex n = 0; n < count; n++) {
		SecCertificateRef cert = SecCertificateRef(CFArrayGetValueAtIndex(certChain, n));
		CSSM_DATA certData;
		MacOSError::check(SecCertificateGetData(cert, &certData));
		char name[PATH_MAX];
		snprintf(name, sizeof(name), "%s%ld", prefix, n);
		AutoFileDesc(name, O_WRONLY | O_CREAT | O_TRUNC).writeAll(certData.Data, certData.Length);
	}
}


string flagForm(uint32_t flags)
{
	if (flags == 0)
		return "0x0(none)";

	string r;
	uint32_t leftover = flags;
	for (const SecCodeDirectoryFlagTable *item = kSecCodeDirectoryFlagTable; item->name; item++)
		if (flags & item->value) {
			r = r + "," + item->name;
			leftover &= ~item->value;
		}
	if (leftover)
		r += ",???";
	char buf[80];
	snprintf(buf, sizeof(buf), "0x%x", flags);
	return string(buf) + "(" + r.substr(1) + ")";
}
