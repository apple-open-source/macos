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
// cs_utils - shared utilities for CodeSigning tool commands
//
#include "codesign.h"
#include <Security/CodeSigning.h>
#include <Security/SecIdentitySearchPriv.h>
#include <Security/SecPolicyPriv.h>
#include <security_utilities/cfutilities.h>
#include <security_codesigning/reqdumper.h>
#include <security_codesigning/cdbuilder.h>
#include <security_codesigning/reqparser.h>
#include <security_codesigning/renum.h>
#include <Security/CMSEncoder.h>
#include <cstdio>
#include <cmath>
#include <getopt.h>
#include <sys/codesign.h>
#include <sys/param.h>		// MAXPATHLEN


using namespace CodeSigning;
using namespace UnixPlusPlus;


//
// Shared command-line arguments and options
//
unsigned verbose = 0;				// verbosity level
bool force = false;					// force overwrite flag
bool continueOnError = false;		// continue processing targets on error(s)

int exitcode = exitSuccess;			// cumulative exit code


//
// Build requirements data from outside sources.
// This automatically recognizes binary Requirement(s) blobs, on the
// assumption that the high byte of their magic is not a valid
// (first) character of a text Requirements syntax. The assumption is
// that they all share the same first-byte prefix (0xfa, for the 0xfade0cxx
// convention used for code signing magic numbers).
//
template <class ReqType>
const ReqType *readRequirement(const string &source)
{
	if (source[0] == '=') {	// =text
		return RequirementParser<ReqType>()(source.substr(1));
	} else if (source == "-") {	// stdin
		return RequirementParser<ReqType>()(stdin);
	} else if (FILE *f = fopen(source.c_str(), "r")) {
		int first = getc(f);
		ungetc(first, f);
		const ReqType *req;
		if (first == (Requirement::typeMagic >> 24)) // shared prefix: binary blob
			req = ReqType::readBlob(f);
		else										// presumably text
			req = RequirementParser<ReqType>()(f);
		fclose(f);
		if (!req)
			UnixError::throwMe();
		return req;
	} else {
		perror(source.c_str());
		fail("invalid requirement specification");
	}
}

// instantiate explicitly
template const Requirement *readRequirement(const string &);
template const Requirements *readRequirement(const string &);
template const BlobCore *readRequirement(const string &);


//
// ErrorCheck
//
void ErrorCheck::operator () (OSStatus rc)
{
	if (rc != noErr) {
		assert(mError);
		throw Error(mError);
	}
}


//
// Given a keychain or type of keychain item, return the string form of the path
// of the keychain containing it.
//
string keychainPath(CFTypeRef whatever)
{
	CFRef<SecKeychainRef> keychain;
	if (CFGetTypeID(whatever) == SecKeychainGetTypeID()) {
		keychain = SecKeychainRef(whatever);
	} else if (CFGetTypeID(whatever) == SecIdentityGetTypeID()) {
		CFRef<SecCertificateRef> cert;
		MacOSError::check(SecIdentityCopyCertificate(SecIdentityRef(whatever), &cert.aref()));
		MacOSError::check(SecKeychainItemCopyKeychain(cert.as<SecKeychainItemRef>(), &keychain.aref()));
	} else {	// assume keychain item
		switch (OSStatus rc = SecKeychainItemCopyKeychain(SecKeychainItemRef(whatever), &keychain.aref())) {
		case noErr:
			break;
		case errSecNoSuchKeychain:
			return "(nowhere)";
		default:
			MacOSError::throwMe(rc);
		}
	}
	char path[MAXPATHLEN];
	UInt32 length = sizeof(path);
	MacOSError::check(SecKeychainGetPath(keychain, &length, path));
	return path;
}


//
// Locate a signing identity from the keychain search list.
//
SecIdentityRef findIdentity(SecKeychainRef keychain, const char *match)
{
	// the special value "-" (dash) indicates ad-hoc signing
	if (!strcmp(match, "-"))
		return SecIdentityRef(kCFNull);

	// all Code Signing capable identities are candidates
	CFRef<SecPolicyRef> policy;
	MacOSError::check(SecPolicyCopy(CSSM_CERT_X_509v3,
		&CSSMOID_APPLE_TP_CODE_SIGNING, &policy.aref()));
#if !defined(NDEBUG)
	if (getenv("CODESIGN_ANYCERT"))	// allow signing with any signing cert (for testing only)
		MacOSError::check(SecPolicyCopy(CSSM_CERT_X_509v3,
			&CSSMOID_APPLE_X509_BASIC, &policy.aref()));
#endif //NDEBUG
	CFRef<SecIdentitySearchRef> search;
	MacOSError::check(SecIdentitySearchCreateWithPolicy(policy, NULL,
			CSSM_KEYUSE_SIGN, keychain, false, &search.aref()));

	// filter all candidates against our match constraint
	CFRef<CFStringRef> cfmatch = makeCFString(match);
	CFRef<SecIdentityRef> bestMatch;
	CFRef<CFStringRef> bestMatchName;
	CSSM_DATA bestMatchData;
	for (;;) {
		CFRef<SecIdentityRef> candidate;
		switch (OSStatus rc = SecIdentitySearchCopyNext(search, &candidate.aref())) {
		case noErr:
			{
				CFRef<SecCertificateRef> cert;
				MacOSError::check(SecIdentityCopyCertificate(candidate, &cert.aref()));
				CFRef<CFStringRef> name;
				CSSM_DATA data;
				MacOSError::check(SecCertificateCopyCommonName(cert, &name.aref()));
				MacOSError::check(SecCertificateGetData(cert, &data));
				if (!strcmp(match, "*")) {	// means "just take the first one"
					note(1, "Using identity \"%s\"", cfString(name).c_str());
					return candidate.yield();
				}
				CFRange find = CFStringFind(name, cfmatch,
					kCFCompareCaseInsensitive | kCFCompareNonliteral);
				if (find.location == kCFNotFound)
					continue;		// no match
				if (bestMatch) {	// got two matches
					if (bestMatchData.Length == data.Length
							&& !memcmp(bestMatchData.Data, data.Data, data.Length)) { // same cert
						note(2, "%s: found in both %s and %s (this is all right)",
							match, keychainPath(bestMatch).c_str(), keychainPath(cert).c_str());
						continue;
					}
					// ambiguity - fail and try to explain it as well as we can
					string firstKeychain = keychainPath(bestMatch);
					string newKeychain = keychainPath(cert);
					if (firstKeychain == newKeychain)
						fail("%s: ambiguous (matches \"%s\" and \"%s\" in %s)",
							match, cfString(name).c_str(), cfString(bestMatchName).c_str(),
							newKeychain.c_str());
					else
						fail("%s: ambiguous (matches \"%s\" in %s and \"%s\" in %s)",
							match, cfString(name).c_str(), firstKeychain.c_str(),
							cfString(bestMatchName).c_str(), newKeychain.c_str());
				}
				bestMatch = candidate;
				bestMatchName = name;
				bestMatchData = data;
				break;
			}
		case errSecItemNotFound:
			if (bestMatch)
				return bestMatch.yield();
			fail("%s: no such identity", match);
		default:
			MacOSError::check(rc);
		}
	}
}


//
// Parse out textual description of CodeDirectory flags
//
uint32_t parseCdFlags(const char *arg)
{
	// if it's numeric, Just Do It
	if (isdigit(arg[0])) {
		char *remain;
		long r = strtol(arg, &remain, 0);
		if (remain[0])
			fail("%s: invalid flag(s)", arg);
		return r;
	}
	
	// otherwise, let's ask CodeDirectory for a canonical parse
	return CodeDirectory::textFlags(arg);
}


//
// Parse a date/time string into CFDateRef form.
// The special value "none" is translated to cfNull.
//
CFDateRef parseDate(const char *string)
{
	if (!string || !strcasecmp(string, "none"))
		return CFDateRef(kCFNull);
	CFRef<CFLocaleRef> userLocale = CFLocaleCopyCurrent();
	CFRef<CFDateFormatterRef> formatter = CFDateFormatterCreate(NULL, userLocale,
		kCFDateFormatterMediumStyle, kCFDateFormatterMediumStyle);
	CFDateFormatterSetProperty(formatter, kCFDateFormatterIsLenient, kCFBooleanTrue);
	CFRef<CFDateRef> date = CFDateFormatterCreateDateFromString(NULL, formatter, CFTempString(string), NULL);
	if (!date)
		fail("%s: invalid date/time", string);
	return date.yield();
}


//
// Convert a SHA-1 hash into a string of hex digits (40 of them, of course)
//
std::string hashString(SHA1::Digest hash)
{
	char s[2 * SHA1::digestLength + 1];
	for (unsigned n = 0; n < SHA1::digestLength; n++)
		sprintf(&s[2*n], "%02.2x", hash[n]);
	return s;
}

std::string hashString(SHA1 &hash)
{
	SHA1::Digest digest;
	hash.finish(digest);
	return hashString(digest);
}


//
// Diagnostic and messaging support
//
void fail(const char *format, ...)
{
	va_list args;
	va_start(args, format);
	vfprintf(stderr, format, args);
	fprintf(stderr, "\n");
	va_end(args);
	if (continueOnError)
		throw Fail(exitFailure);
	else
		exit(exitFailure);
}

void note(unsigned level, const char *format, ...)
{
	if (verbose >= level) {
		va_list args;
		va_start(args, format);
		vfprintf(stderr, format, args);
		fprintf(stderr, "\n");
		va_end(args);
	}
}


//
// Resolve an exception into a readable error message, then exit with error
//
static void diagnose1(const char *type, CFTypeRef value);
static void diagnose1(const char *context, OSStatus rc);

void diagnose(const char *context /* = NULL */, int stop /* = 0 */)
{
	try {
		throw;
	} catch (const ErrorCheck::Error &err) {
		diagnose(context, err.error);
	} catch (const CSError &err) {
		diagnose(context, err.osStatus(), err.infoDict());
	} catch (const MacOSError &err) {
		diagnose1(context, err.osStatus());
	} catch (const UnixError &err) {
		errno = err.error;
		perror(context);
	} catch (const Fail &failure) {
		// source has printed any error information
	} catch (...) {
		if (context)
			fprintf(stderr, "%s: ", context);
		fprintf(stderr, "unknown exception\n");
	}
	if (stop)
		exit(stop);
}

void diagnose(const char *context, CFErrorRef err)
{
	diagnose(context, CFErrorGetCode(err), CFErrorCopyUserInfo(err));
}

static void diagnose1(const char *context, OSStatus rc)
{
	if (rc >= errSecErrnoBase && rc < errSecErrnoLimit) {
		errno = rc - errSecErrnoBase;
		perror(context);
	} else
		cssmPerror(context, rc);
}

void diagnose(const char *context, OSStatus rc, CFDictionaryRef info)
{
	diagnose1(context, rc);
	
	if (CFTypeRef detail = CFDictionaryGetValue(info, kSecCFErrorRequirementSyntax))
			fprintf(stderr, "Requirement syntax error(s):\n%s", cfString(CFStringRef(detail)).c_str());

	if (verbose) {
		if (CFTypeRef detail = CFDictionaryGetValue(info, kSecCFErrorResourceAdded))
			diagnose1("resource added", detail);
		if (CFTypeRef detail = CFDictionaryGetValue(info, kSecCFErrorResourceAltered))
			diagnose1("resource modified", detail);
		if (CFTypeRef detail = CFDictionaryGetValue(info, kSecCFErrorResourceMissing))
			diagnose1("resource missing", detail);
	}
}

static void diagnose1(const char *type, CFTypeRef value)
{
	if (CFGetTypeID(value) == CFArrayGetTypeID()) {
		CFArrayRef array = CFArrayRef(value);
		CFIndex size = CFArrayGetCount(array);
		for (CFIndex n = 0; n < size; n++)
			diagnose1(type, CFArrayGetValueAtIndex(array, n));
	} else if (CFGetTypeID(value) == CFURLGetTypeID()) {
		printf("%s: %s\n", cfString(CFURLRef(value)).c_str(), type);
	} else
		printf("<unexpected CF type>: %s\n", type);
}


//
// Take an array of CFURLRefs and write it to a file as a list of filesystem paths,
// one path per line.
//
void writeFileList(CFArrayRef list, const char *destination, const char *mode)
{
	FILE *out;
	if (!strcmp(destination, "-")) {
		out = stdout;
	} else if (!(out = fopen(destination, mode))) {
		perror(destination);
		exit(1);
	}
	CFIndex count = CFArrayGetCount(list);
	for (CFIndex n = 0; n < count; n++)
		fprintf(out, "%s\n", cfString(CFURLRef(CFArrayGetValueAtIndex(list, n))).c_str());
	if (strcmp(destination, "-"))
		fclose(out);
}


//
// Take a CFData and write it to a file as a property list.
//
void writeData(CFDataRef data, const char *destination, const char *mode)
{
	FILE *out;
	if (!strcmp(destination, "-")) {
		out = stdout;
	} else if (!(out = fopen(destination, mode))) {
		perror(destination);
		exit(1);
	}
	if (data)
		fwrite(CFDataGetBytePtr(data), 1, CFDataGetLength(data), out);
	if (strcmp(destination, "-"))
		fclose(out);
}


//
// Accept various forms of dynamic target specifications
// and return a SecCodeRef for the designated code.
// Returns NULL if the syntax isn't recognized as a dynamic
// host designation. Fails (calls fail which exits) if the
// argument looks dynamic but has bad syntax.
//
static CFRef<SecCodeRef> descend(SecCodeRef host, CFRef<CFMutableDictionaryRef> attrs, string path);
static void parsePath(CFMutableDictionaryRef attrs, string form);
static void parseAttribute(CFMutableDictionaryRef attrs, string form);

CFRef<SecCodeRef> codePath(const char *target)
{
	if (!isdigit(target[0]))
		return NULL;	// not a dynamic spec
	
	char *path;
	int pid = strtol(target, &path, 10);
	CFMutableDictionaryRef attrs = makeCFMutableDictionary(1,
		kSecGuestAttributePid, CFTempNumber(pid).get()
	);
	
	return descend(NULL, attrs, path);
}


static CFRef<SecCodeRef> descend(SecCodeRef host, CFRef<CFMutableDictionaryRef> attrs, string path)
{
	string::size_type colon = path.find(':');
	if (colon == string::npos)	// last element
		parsePath(attrs, path);
	else
		parsePath(attrs, path.substr(0, colon));
	CFRef<SecCodeRef> guest;
	MacOSError::check(SecCodeCopyGuestWithAttributes(host,
		attrs, kSecCSDefaultFlags, &guest.aref()));
	if (colon == string::npos)
		return guest;
	else
		return descend(guest, makeCFMutableDictionary(0), path.substr(colon + 1));
}
	

// generate a guest selector dictionary for an attr=value specification
static void parsePath(CFMutableDictionaryRef attrs, string form)
{
	for (string::size_type comma = form.find(','); comma != string::npos; comma = form.find(',')) {
		parseAttribute(attrs, form.substr(0, comma));
		form = form.substr(comma + 1);
	}
	parseAttribute(attrs, form);
}

static void parseAttribute(CFMutableDictionaryRef attrs, string form)
{
	if (form.empty())		// nothing to add
		return;
	string::size_type eq = form.find('=');
	CFRef<CFStringRef> key;
	if (eq == string::npos) {
		key = kSecGuestAttributeCanonical;
	} else {
		key.take(makeCFString(form.substr(0, eq)));
		form = form.substr(eq + 1);
	}
	CFRef<CFTypeRef> value = (isdigit(form[0])) ?
		CFTypeRef(makeCFNumber(strtol(form.c_str(), NULL, 0))) : CFTypeRef(makeCFString(form));
	CFDictionaryAddValue(attrs, key, value);
}
