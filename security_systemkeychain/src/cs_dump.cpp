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
// codesign - Swiss Army Knife tool for Code Signing operations
//
#include "codesign.h"
#include <security_codesigning/reqdumper.h>

using namespace CodeSigning;
using namespace UnixPlusPlus;


//
// Local functions
//
static void dumpRequirements(CodeSigning::SecStaticCode *ondisk, FILE *output);
static void extractCertificates(const char *prefix, CFArrayRef certChain);
static string flagForm(uint32_t flags);


//
// Dump a signed object's signing data.
// The more verbosity, the more data.
//
void dump(const char *target)
{
	try {
		CFRef<SecStaticCodeRef> codeRef;
		MacOSError::check(SecStaticCodeCreateWithPath(CFTempURL(target), 0, &codeRef.aref()));
		if (detached) {
			CFRef<CFDataRef> dsig = cfLoadFile(detached);
			MacOSError::check(SecCodeSetDetachedSignature(codeRef, dsig, kSecCSDefaultFlags));
		}
		
		// get official (API driven) information
		CFRef<CFDictionaryRef> api;
		SecCSFlags flags = kSecCSInternalInformation
			| kSecCSSigningInformation
			| kSecCSRequirementInformation;
		if (modifiedFiles)
			flags |= kSecCSContentInformation;
		MacOSError::check(SecCodeCopySigningInformation(codeRef, flags, &api.aref()));
		
		// dive below the API line to get at the internal state
		SecStaticCode *code =
			static_cast<SecStaticCode *>(SecCFObject::required(codeRef, 0));
		const CodeDirectory *dir = code->codeDirectory();
		
		// basic stuff
		note(0, "Executable=%s", code->mainExecutablePath().c_str());
		note(1, "Identifier=%s", code->identifier().c_str());
		note(1, "Format=%s", code->format().c_str());

		// code directory
		const char *cdLocation = detached ? "detached" : "embedded";
		note(1, "CodeDirectory v=%x size=%d flags=%s hashes=%d+%d location=%s",
			int(dir->version), dir->length(), flagForm(dir->flags).c_str(),
			int(dir->nCodeSlots), int(dir->nSpecialSlots), cdLocation);
		if (verbose > 2) {
			SHA1 hash;
			hash(dir, dir->length());
			note(3, "CDHash=%s", hashString(hash).c_str());
		}

		// signature
		if (dir->flags & kSecCodeSignatureAdhoc) {
			note(1, "Signature=adhoc");
		} else if (CFDataRef signature = code->signature()) {
			note(1, "Signature size=%d", CFDataGetLength(signature));
		CFArrayRef certChain = CFArrayRef(CFDictionaryGetValue(api, kSecCodeInfoCertificates));
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

		if (CFDictionaryRef info = code->infoDictionary())
			note(1, "Info.plist entries=%d", CFDictionaryGetCount(info));
		else
			note(1, "Info.plist=not bound");
		
		if (CFDictionaryRef resources = code->resourceDictionary()) {
			CFDictionaryRef rules =
				CFDictionaryRef(CFDictionaryGetValue(resources, CFSTR("rules")));
			CFDictionaryRef files
				= CFDictionaryRef(CFDictionaryGetValue(resources, CFSTR("files")));
			note(1, "Sealed Resources rules=%d files=%d",
				CFDictionaryGetCount(rules), CFDictionaryGetCount(files));
		} else
			note(1, "Sealed Resources=none");
		
		const Requirements *reqs = code->internalRequirements();
		if (internalReq) {
			if (!strcmp(internalReq, "-")) {
				dumpRequirements(code, stdout);
			} else if (FILE *f = fopen(internalReq, "w")) {
				dumpRequirements(code, f);
				fclose(f);
			} else {
				perror(internalReq);
				exit(exitFailure);
			}
		} else {
			if (reqs)
				note(1, "Internal requirements count=%d size=%d",
					reqs->count(), reqs->length());
			else
				note(1, "Internal requirements=none");
		}
		
		if (entitlements)
			writeData(CFDataRef(CFDictionaryGetValue(api, kSecCodeInfoEntitlements)), entitlements, "a");

		if (modifiedFiles)
			writeFileList(CFArrayRef(CFDictionaryGetValue(api, kSecCodeInfoChangedFiles)), modifiedFiles, "a");
		
	} catch (...) {
		diagnose(target, exitFailure);
	}
}


//
// Dump the requirements of an internal OnDisk object to a file.
// This includes any implicit Designated Requirement.
//
void dumpRequirements(SecStaticCode *ondisk, FILE *output)
{
	const Requirements *reqs = ondisk->internalRequirements();
	puts(Dumper::dump(reqs).c_str());
	if (ondisk->internalRequirement(kSecDesignatedRequirementType) == NULL) {
		try {
			const Requirement *req = ondisk->designatedRequirement();
			fprintf(output, "# designated => %s\n",
				Dumper::dump(req).c_str());
		} catch (...) {
			fprintf(output, "# Unable to generate implicit designated requirement\n");
		}
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
		snprintf(name, sizeof(name), "%s%d", prefix, n);
		AutoFileDesc(name, O_WRONLY | O_CREAT | O_TRUNC).writeAll(certData.Data, certData.Length);
	}
}


string flagForm(uint32_t flags)
{
	if (flags == 0)
		return "0x0(none)";

	string r;
	if (flags & kSecCodeSignatureHost)
		r += ",host";
	if (flags & kSecCodeSignatureAdhoc)
		r += ",adhoc";
	if (flags & kSecCodeSignatureForceKill)
		r += ",kill";
	if (flags & kSecCodeSignatureForceHard)
		r += ",hard";
	if (flags & ~(kSecCodeSignatureHost
			| kSecCodeSignatureAdhoc
			| kSecCodeSignatureForceKill
			| kSecCodeSignatureForceHard)) {
		r += ",???";
	}
	char buf[80];
	snprintf(buf, sizeof(buf), "0x%x", flags);
	return string(buf) + "(" + r.substr(1) + ")";
}
