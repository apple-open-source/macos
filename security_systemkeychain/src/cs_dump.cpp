/*
 * Copyright (c) 2006-2010 Apple Inc. All Rights Reserved.
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
#include <security_utilities/cfmunge.h>
#include <security_codesigning/codedirectory.h>		// strictly for the data format
#include <cmath>

using namespace UnixPlusPlus;


//
// Local functions
//
static void extractCertificates(const char *prefix, CFArrayRef certChain);
static void listNestedCode(const void *key, const void *value, void *context);
static string flagForm(uint32_t flags);

//
// Dump a signed object's signing data.
// The more verbosity, the more data.
//
void dump(const char *target)
{
	// get the code object (static or dynamic)
	CFRef<SecStaticCodeRef> codeRef = dynamicCodePath(target);	// dynamic input
	if (!codeRef)
		codeRef = staticCodePath(target, architecture, bundleVersion);
	if (detached) {
		if (CFRef<CFDataRef> dsig = cfLoadFile(detached))
			MacOSError::check(SecCodeSetDetachedSignature(codeRef, dsig, kSecCSDefaultFlags));
		else
			fail("%s: cannot load detached signature", detached);
	}
	
	// get official (API driven) information
	struct Info : public CFDictionary {
		Info() : CFDictionary(errSecCSInternalError) { }
		const std::string string(CFStringRef key) { return cfString(get<CFStringRef>(key)); }
		const std::string url(CFStringRef key) { return cfString(get<CFURLRef>(key)); }
		uint32_t number(CFStringRef key) { return cfNumber(get<CFNumberRef>(key)); }
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
		int(dir->nCodeSlots), int(dir->nSpecialSlots),
		api.string(kSecCodeInfoSource).c_str());
    
	if (verbose > 2) {
		uint32_t hashType = api.number(kSecCodeInfoDigestAlgorithm);
		if (const HashType *type = findHashType(hashType))
			note(3, "Hash type=%s size=%d", type->name, type->size);
		else
			note(3, "Hash UNKNOWN type=%d", hashType);
	}
	if (verbose > 4) {
		typedef CodeDirectory::Slot Slot;
		Slot end = (verbose > 5) ? Slot(dir->nCodeSlots) : -1;
		for (Slot slot = -dir->nSpecialSlots; slot < end; slot++)
			note(5, "%6d=%s", slot, hashString((*dir)[slot]).c_str());
	}

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
			
		CFRef<CFLocaleRef> userLocale = CFLocaleCopyCurrent();
		CFRef<CFDateFormatterRef> format = CFDateFormatterCreate(NULL, userLocale,
			kCFDateFormatterMediumStyle, kCFDateFormatterMediumStyle);
		CFDateRef softTime = CFDateRef(CFDictionaryGetValue(api, kSecCodeInfoTime));
		CFDateRef hardTime = CFDateRef(CFDictionaryGetValue(api, kSecCodeInfoTimestamp));
		if (hardTime) {
			CFRef<CFStringRef> s = CFDateFormatterCreateStringWithDate(NULL, format, hardTime);
			note(1, "Timestamp=%s", cfString(s).c_str());
			if (CFAbsoluteTimeGetCurrent() < CFDateGetAbsoluteTime(hardTime) - timestampSlop)
				fail("%s: postdated timestamp or bad system clock", target);
			if (softTime) {
				CFAbsoluteTime slop = abs(CFDateGetAbsoluteTime(softTime) - CFDateGetAbsoluteTime(hardTime));
				if (slop > timestampSlop) {
					CFRef<CFStringRef> s = CFDateFormatterCreateStringWithDate(NULL, format, softTime);
					note(0, "%s: timestamp mismatch: internal time %s (%g seconds apart)", target, cfString(s).c_str(), slop);
				}
			}
		} else if (softTime) {
			CFRef<CFStringRef> s = CFDateFormatterCreateStringWithDate(NULL, format, softTime);
			note(1, "Signed Time=%s", cfString(s).c_str());
			if (CFAbsoluteTimeGetCurrent() < CFDateGetAbsoluteTime(softTime) - timestampSlop)
				note(0, "%s: postdated signing date or bad system clock", target);
		}
        
        /** Dump the code directory to disk as a binary. Do this last so that if there 
         is a problem writing/creating the file, the rest of it is still printed */
		if (dumpBinary)
			AutoFileDesc(dumpBinary, O_WRONLY|O_CREAT|O_TRUNC, 0666).writeAll(dir, dir->length());
	} else {
		fprintf(stderr, "%s: no signature\n", target);
		// but continue dumping
	}

	if (CFDictionaryRef info = api.get<CFDictionaryRef>(kSecCodeInfoPList))
		note(1, "Info.plist entries=%d", CFDictionaryGetCount(info));
	else
		note(1, "Info.plist=not bound");
    
	if (CFStringRef teamID = api.get<CFStringRef>(kSecCodeInfoTeamIdentifier))
		note(1, "TeamIdentifier=%s", cfString(teamID).c_str());
	else
		note(1, "TeamIdentifier=not set");

	
	if (CFDictionaryRef resources = api.get<CFDictionaryRef>(kSecCodeInfoResourceDirectory)) {
		CFDictionaryRef rules1 =
			CFDictionaryRef(CFDictionaryGetValue(resources, CFSTR("rules")));
		CFDictionaryRef rules2 =
			CFDictionaryRef(CFDictionaryGetValue(resources, CFSTR("rules2")));
		CFDictionaryRef rules;
		CFDictionaryRef files;
		int version = 0;
		if (rules2) {
			rules = rules2;
			files = CFDictionaryRef(CFDictionaryGetValue(resources, CFSTR("files2")));
			version = 2;
		} else {
			rules = rules1;
			files = CFDictionaryRef(CFDictionaryGetValue(resources, CFSTR("files")));
			version = 1;
		}
		note(1, "Sealed Resources version=%d rules=%d files=%d",
			version, CFDictionaryGetCount(rules), CFDictionaryGetCount(files));
		if (resourceRules) {
			FILE *output;
			if (!strcmp(resourceRules, "-")) {
				output = stdout;
			} else if (!(output = fopen(resourceRules, "w"))) {
				perror(resourceRules);
				exit(exitFailure);
			}
			CFCopyRef<CFDictionaryRef> data = resources;
			if (verbose <= 4) {
				if (version > 1)
					data = cfmake<CFDictionaryRef>("{rules=%O,rules2=%O}", rules, rules2);
				else
					data = cfmake<CFDictionaryRef>("{rules=%O}", rules);
			}
			CFRef<CFDataRef> rulesData = makeCFData(data.get());
			fwrite(CFDataGetBytePtr(rulesData), CFDataGetLength(rulesData), 1, output);
			if (output != stdout)
				fclose(output);
		}
		if (nested && version > 1)
			CFDictionaryApplyFunction(files, listNestedCode, NULL);
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

	if (entitlements) {
		CFCopyRef<CFDataRef> data = CFDataRef(CFDictionaryGetValue(api, kSecCodeInfoEntitlements));
		// a destination prefixed with ':' means strip the binary header off the data
		if (entitlements[0] == ':') {
			static const unsigned headerSize = sizeof(BlobCore);
			if (data)
				data = CFDataCreateWithBytesNoCopy(NULL,
					CFDataGetBytePtr(data) + headerSize, CFDataGetLength(data) - headerSize, kCFAllocatorNull);
			entitlements++;
		}
		writeData(data, entitlements, "a");
	}

	if (modifiedFiles)
		writeFileList(CFArrayRef(CFDictionaryGetValue(api, kSecCodeInfoChangedFiles)), modifiedFiles, "a");
}


//
// Show details of resource seal entries
//
static void listNestedCode(const void *key, const void *value, void *context)
{
	if (value && CFGetTypeID(value) == CFDictionaryGetTypeID()) {
		CFDictionary seal(value, noErr);
		if (seal && CFDictionaryGetValue(seal, CFSTR("requirement")))
			note(0, "Nested=%s", cfString(CFStringRef(key)).c_str());
	}
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
