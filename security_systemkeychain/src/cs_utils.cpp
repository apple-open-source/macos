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
// cs_utils - shared utilities for CodeSigning tool commands
//
#include "codesign.h"
#include <Security/CodeSigning.h>
#include <Security/SecCertificatePriv.h>
#include <Security/CSCommonPriv.h>
#include <Security/SecIdentitySearchPriv.h>
#include <Security/SecPolicyPriv.h>
#include <security_utilities/cfutilities.h>
#include <security_utilities/cfmunge.h>
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

using namespace UnixPlusPlus;


//
// Shared command-line arguments and options
//
unsigned verbose = 0;				// verbosity level
bool force = false;					// force overwrite flag
bool continueOnError = false;		// continue processing targets on error(s)

int exitcode = exitSuccess;			// cumulative exit code


//
// Convert between hash code numbers and human-readable form.
// We accept unambiguous prefix strings for conversion.
//
static const HashType hashTypes[] = {
	{ "sha1",			kSecCodeSignatureHashSHA1,						SHA1::digestLength },
	{ "sha256",			kSecCodeSignatureHashSHA256,					256 / 8 },
	{ "skein160x256",	kSecCodeSignatureHashPrestandardSkein160x256,	160 / 8 },
	{ "skein256x512",	kSecCodeSignatureHashPrestandardSkein256x512,	256 / 8 },
	{ NULL }
};

const HashType *findHashType(const char *hashName)
{
	size_t length = strlen(hashName);
	const HashType *match = NULL;
	for (const HashType *h = hashTypes; h->name; h++)
		if (!strncmp(hashName, h->name, length))	// prefix match
			if (match)
				fail("%s: ambiguous hash specification (%s or %s)",
					hashName, match->name, h->name);
			else
				match = h;
	if (match)
		return match;
	fail("%s: unknown hash specification", hashName);
}

const HashType *findHashType(uint32_t hashCode)
{
	for (const HashType *h = hashTypes; h->name; h++)
		if (h->code == hashCode)
			return h;
	return NULL;
}


//
// Build requirements data from outside sources.
// This automatically recognizes binary Requirement(s) blobs, on the
// assumption that the high byte of their magic is not a valid
// (first) character of a text Requirements syntax. The assumption is
// that they all share the same first-byte prefix (0xfa, for the 0xfade0cxx
// convention used for code signing magic numbers).
//
CFTypeRef readRequirement(const string &source, SecCSFlags flags)
{
	CFTypeRef result;
	ErrorCheck check;
	if (source[0] == '=') {	// =text
		check(SecRequirementsCreateWithString(CFTempString(source.substr(1)), flags, &result, check));
		return result;
	}
	FILE *f;
	if (source == "-") {
		f = stdin;
	} else if (!(f = fopen(source.c_str(), "r"))) {
		perror(source.c_str());
		fail("invalid requirement specification");
	}
	int first = getc(f);
	ungetc(first, f);
	if (first == kSecCodeMagicByte) {			// binary blob
		BlobCore *blob = BlobCore::readBlob(f);
		switch (blob->magic()) {
		case kSecCodeMagicRequirement:
			MacOSError::check(SecRequirementCreateWithData(CFTempData(*blob), kSecCSDefaultFlags, (SecRequirementRef *)&result));
			break;
		case kSecCodeMagicRequirementSet:
			result = makeCFData(*blob);
			break;
		default:
			fail((source + ": not a recognized requirement file").c_str());
		}
		::free(blob);
	} else {									// presumably text
		char buffer[10240];		// arbitrary size
		int length = fread(buffer, 1, sizeof(buffer) - 1, f);
		buffer[length] = '\0';
		check(SecRequirementsCreateWithString(CFTempString(buffer), flags, &result, check));
	}
	if (f != stdin)
		fclose(f);
	return result;
}


//
// ErrorCheck
//
void ErrorCheck::throwError()
{
	assert(mError);
	throw Error(mError);
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
// Exit with error diagnosed if we can't find exactly one match in the user's
// keychain environment.
//
static SecIdentityRef findIdentity(SecKeychainRef keychain, const char *match, SecPolicyRef policy);

SecIdentityRef findIdentity(SecKeychainRef keychain, const char *match)
{
	// the special value "-" (dash) indicates ad-hoc signing
	if (!strcmp(match, "-"))
		return SecIdentityRef(kCFNull);
	
	// interpret the match as a (full) identity preference name
	CFRef<SecIdentityRef> preference;
	switch (OSStatus rc = SecIdentityCopyPreference(CFTempString(match), 0, NULL, &preference.aref())) {
	case noErr:
		if (preference)
			return preference.yield();
		break;
	case errSecItemNotFound:
		break;
	default:
		MacOSError::throwMe(rc);
	}

	// look for qualified identities
	CFRef<SecPolicyRef> policy;
	MacOSError::check(SecPolicyCopy(CSSM_CERT_X_509v3,
		&CSSMOID_APPLE_TP_CODE_SIGNING, &policy.aref()));
	if (SecIdentityRef identity = findIdentity(keychain, match, policy))
		return identity;	// good match

	// now try again, using the generic policy
	MacOSError::check(SecPolicyCopy(CSSM_CERT_X_509v3,
		&CSSMOID_APPLE_X509_BASIC, &policy.aref()));
	if (SecIdentityRef identity = findIdentity(keychain, match, policy)) {
		// found a match, but it's not good for Code Signing
#if !defined(NDEBUG)
		if (getenv("CODESIGN_ANYCERT")) {
			note(1, "Using unqualified identity for test - signature will not verify");
			return identity;
		}
#endif //NDEBUG
		CFRef<SecCertificateRef> cert;
		MacOSError::check(SecIdentityCopyCertificate(identity, &cert.aref()));
		CFRef<CFStringRef> name;
		MacOSError::check(SecCertificateCopyCommonName(cert, &name.aref()));
		fail("%s: this identity cannot be used for signing code", cfString(name).c_str());
	}
	fail("%s: no identity found", match);
}


static SecIdentityRef findIdentity(SecKeychainRef keychain, const char *match, SecPolicyRef policy)
{
	CFRef<SecIdentitySearchRef> search;
	MacOSError::check(SecIdentitySearchCreateWithPolicy(policy, NULL,
			CSSM_KEYUSE_SIGN, keychain, false, &search.aref()));

	// recognize an exact hex expression of a SHA1 (no abbreviations allowed)
	CFRef<CFDataRef> certHash;
	const char hexDigits[] = "0123456789abcdefABCDEF";
	if (strlen(match) == 2 * SHA1::digestLength && strspn(match, hexDigits) == 2 * SHA1::digestLength) {
		SHA1::Digest digest;
		stringHash(match, digest);
		certHash = CFDataCreate(NULL, digest, sizeof(digest));
	}

	// filter all candidates against our match constraint
	CFRef<CFStringRef> cfmatch = makeCFString(match);
	CFRef<SecIdentityRef> bestMatch;
	CFRef<CFStringRef> bestMatchName;
	bool exactMatch = false;
	CSSM_DATA bestMatchData;
	for (;;) {
		CFRef<SecIdentityRef> candidate;
		switch (OSStatus rc = SecIdentitySearchCopyNext(search, &candidate.aref())) {
		case noErr:
			{
				CFRef<SecCertificateRef> cert;
				MacOSError::check(SecIdentityCopyCertificate(candidate, &cert.aref()));
				
				// match on certificate hash if that was requested
				if (certHash) {
					CFRef<CFDataRef> hash = certificateHash(cert);
					if (CFEqual(hash, certHash))
						return candidate.yield();
				}
				
				// otherwise, match on best CN substring
				CFRef<CFStringRef> name;
				CSSM_DATA data;
				MacOSError::check(SecCertificateCopyCommonName(cert, &name.aref()));
				MacOSError::check(SecCertificateGetData(cert, &data));
				if (!strcmp(match, "*")) {	// means "just take the first one"
					note(1, "Using identity \"%s\"", cfString(name).c_str());
					return candidate.yield();
				}
				if (!name)		// certificate has no subject common name (can't find it by name)
					continue;
				CFRange find = CFStringFind(name, cfmatch,
					kCFCompareCaseInsensitive | kCFCompareNonliteral);
				if (find.location == kCFNotFound)
					continue;		// no match
				bool isExact = find.location == 0 && find.length == CFStringGetLength(name);
				if (bestMatch) {	// got two matches
					if (exactMatch && !isExact)		// prior is better; ignore this one
						continue;
					if (exactMatch == isExact) {	// same class of match
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
				}
				bestMatch = candidate;
				bestMatchName = name;
				bestMatchData = data;
				exactMatch = isExact;
				break;
			}
		case errSecItemNotFound:
			return bestMatch.yield();
		default:
			MacOSError::check(rc);
		}
	}
}


//
// Parse all forms of the -o/--options argument (CodeDirectory flags)
//
uint32_t parseOptionTable(const char *arg, const SecCodeDirectoryFlagTable *options)
{
	uint32_t flags = 0;
	std::string text = arg;
	for (string::size_type comma = text.find(','); ; text = text.substr(comma+1), comma = text.find(',')) {
		string word = (comma == string::npos) ? text : text.substr(0, comma);
		const SecCodeDirectoryFlagTable *item;
		for (item = options; item->name; item++)
			if (item->signable && !strncmp(word.c_str(), item->name, word.size())) {
				flags |= item->value;
				break;
			}
		if (!item->name)  // not found
			MacOSError::throwMe(errSecCSInvalidFlags);
		if (comma == string::npos)  // last word
			break;
	}
    return flags;
}

uint32_t parseCdFlags(const char *arg)
{
	// if it's numeric, Just Do It
	if (isdigit(arg[0])) {		// numeric - any form of number
		char *remain;
		uint32_t flags = strtol(arg, &remain, 0);
		if (remain[0])
			fail("%s: invalid flag(s)", arg);
		return flags;
	} else {
		return parseOptionTable(arg, kSecCodeDirectoryFlagTable);
    }
}


//
// Parse a date/time string into CFDateRef form.
// The special value "none" is translated to cfNull.
//
CF_EXPORT CFStringRef const kCFDateFormatterIsLenientKey;

CFDateRef parseDate(const char *string)
{
	if (!string || !strcasecmp(string, "none"))
		return CFDateRef(kCFNull);
	CFRef<CFLocaleRef> userLocale = CFLocaleCopyCurrent();
	CFRef<CFDateFormatterRef> formatter = CFDateFormatterCreate(NULL, userLocale,
		kCFDateFormatterMediumStyle, kCFDateFormatterMediumStyle);
	CFDateFormatterSetProperty(formatter, kCFDateFormatterIsLenientKey, kCFBooleanTrue);
	CFRef<CFDateRef> date = CFDateFormatterCreateDateFromString(NULL, formatter, CFTempString(string), NULL);
	if (!date)
		fail("%s: invalid date/time", string);
	return date.yield();
}


//
// Clean up a pathname.
//
std::string cleanPath(const char *path)
{
	char answer[PATH_MAX];
	if (const char *r = realpath(path, answer))
		return r;
	perror(path);
	exit(1);
}


//
// Convert a SHA-1 hash into a string of hex digits (40 of them, of course)
//
std::string hashString(const SHA1::Byte *hash)
{
	char s[2 * SHA1::digestLength + 1];
	for (unsigned n = 0; n < SHA1::digestLength; n++)
		sprintf(&s[2*n], "%2.2x", hash[n]);
	return s;
}

std::string hashString(SHA1 &hash)
{
	SHA1::Digest digest;
	hash.finish(digest);
	return hashString(digest);
}

void stringHash(const char *string, SHA1::Digest digest)
{
	for (unsigned n = 0; n < SHA1::digestLength; n++)
		sscanf(string+2*n, "%2hhx", digest+n);
}

CFDataRef certificateHash(SecCertificateRef cert)
{
	CFRef<CFDataRef> certData = SecCertificateCopyData(cert);
	SHA1 hash;
	hash(CFDataGetBytePtr(certData), CFDataGetLength(certData));
	SHA1::Digest digest;
	hash.finish(digest);
	return CFDataCreate(NULL, digest, sizeof(digest));
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
	
	if (CFTypeRef path = CFDictionaryGetValue(info, kSecCFErrorPath))
			fprintf(stderr, "In subcomponent: %s\n", cfString(CFURLRef(path)).c_str());
			
	if (CFTypeRef detail = CFDictionaryGetValue(info, kSecCFErrorArchitecture))
			fprintf(stderr, "In architecture: %s\n", cfString(CFStringRef(detail)).c_str());
	
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
	} else
		printf("%s: %s\n", type, cfString(value, noErr).c_str());
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
// Take a CFDictionary and write it to a file as a property list.
//
void writeDictionary(CFDictionaryRef dict, const char *destination, const char *mode)
{
	FILE *out;
	if (!strcmp(destination, "-")) {
		out = stdout;
	} else if (!(out = fopen(destination, mode))) {
		perror(destination);
		exit(1);
	}
	if (dict) {
		CFRef<CFDataRef> data = makeCFData(dict);
		fwrite(CFDataGetBytePtr(data), 1, CFDataGetLength(data), out);
	}
	if (strcmp(destination, "-"))
		fclose(out);
}


//
// Take a CFDictionary and write it to a file as a property list.
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
// Create a static code object, using command context as available
//
SecStaticCodeRef staticCodePath(const char *target, const Architecture &arch, const char *version)
{
	CFRef<CFMutableDictionaryRef> attributes;
	if (arch || version) {
		attributes = makeCFMutableDictionary();
		if (arch)
			cfadd(attributes, "{%O=%d,%O=%d}",
				kSecCodeAttributeArchitecture, arch.cpuType(),
				kSecCodeAttributeSubarchitecture, arch.cpuSubtype());;
		if (version)
			cfadd(attributes, "{%O=%s}", kSecCodeAttributeBundleVersion, version);
	}
	SecStaticCodeRef code;
	MacOSError::check(SecStaticCodeCreateWithPathAndAttributes(CFTempURL(cleanPath(target)), kSecCSDefaultFlags,
		attributes, &code));
	return code;
}


//
// Accept various forms of dynamic target specifications
// and return a SecCodeRef for the designated code.
// Returns NULL if the syntax isn't recognized as a dynamic
// host designation. Fails (calls fail which exits) if the
// argument looks dynamic but has bad syntax.
//
static SecCodeRef descend(SecCodeRef host, CFRef<CFMutableDictionaryRef> attrs, string path);
static void parsePath(CFMutableDictionaryRef attrs, string form);
static void parseAttribute(CFMutableDictionaryRef attrs, string form);

SecCodeRef dynamicCodePath(const char *target)
{
	if (!isdigit(target[0]))
		return NULL;	// not a dynamic spec
	
	char *path;
	int pid = strtol(target, &path, 10);
	
	return descend(NULL,
		cfmake<CFMutableDictionaryRef>("{%O=%d}", kSecGuestAttributePid, pid),
		path);
}


static SecCodeRef descend(SecCodeRef host, CFRef<CFMutableDictionaryRef> attrs, string path)
{
	string::size_type colon = path.find(':');
	if (colon == string::npos)	// last element
		parsePath(attrs, path);
	else
		parsePath(attrs, path.substr(0, colon));
	CFRef<SecCodeRef> guest;
	MacOSError::check(SecCodeCopyGuestWithAttributes(host, attrs, kSecCSDefaultFlags, &guest.aref()));
	if (colon == string::npos)
		return guest.yield();
	else
		return descend(guest, makeCFMutableDictionary(), path.substr(colon + 1));
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
	
	if (isdigit(form[0]))	// number
		CFDictionaryAddValue(attrs, key, CFTempNumber(strtol(form.c_str(), NULL, 0)));
	else					// string
		CFDictionaryAddValue(attrs, key, CFTempString(form));
}
