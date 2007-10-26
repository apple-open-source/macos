/*
 * Copyright (c) 2007 Apple Inc. All rights reserved.
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

#include "macros.hpp"
#include "common.hpp"

#include <SystemConfiguration/SystemConfiguration.h>

#include <sys/types.h>
#include <sys/stat.h>

struct signature
{
    ino_t   sig_ino;
    dev_t   sig_dev;
    size_t  sig_size;
    struct timespec sig_mtime;
    struct timespec sig_ctime;
};

/* Appriximately the same as SCPreferencesGetSignature. */
static CFDataRef get_path_signature(const char * path)
{
	struct stat sbuf;
	struct signature sig;

	zero_struct(sig);
	if (stat(path, &sbuf) == -1) {
	    return NULL;
	}

        sig.sig_dev     = sbuf.st_dev;
        sig.sig_ino     = sbuf.st_ino;
        sig.sig_mtime	= sbuf.st_mtimespec;
        sig.sig_ctime	= sbuf.st_ctimespec;
        sig.sig_size    = sbuf.st_size;

	return CFDataCreate(kCFAllocatorDefault,
			    (const UInt8 *)(&sig), sizeof(struct signature));
}

static CFPropertyListRef load_plist_from_path(const char * path)
{
    CFURLRef	    url = NULL;
    CFDataRef	    data = NULL;
    CFStringRef	    err = NULL;

    CFPropertyListRef plist = NULL;

    url = CFURLCreateFromFileSystemRepresentation(kCFAllocatorDefault,
		    (const UInt8 *)path, strlen(path),
		    false /* isDirectory */);

    if (url == NULL) {
	VERBOSE("failed to build CFURL for '%s'\n", path);
	goto done;
    }

    if (!CFURLCreateDataAndPropertiesFromResource(kCFAllocatorDefault,
			url, &data, NULL, NULL, NULL)) {
	VERBOSE("failed to read CFURL data for '%s'\n", path);
	goto done;
    }

    /* Despite the name, this actually reads both binary and XML plists. */
    plist = CFPropertyListCreateFromXMLData(kCFAllocatorDefault,
		    data, kCFPropertyListImmutable, &err);
    if (!plist) {
	char errbuf[256];

	CFStringGetCString(err, errbuf, sizeof(errbuf), kCFStringEncodingUTF8);
	VERBOSE("plist parse error for '%s': %s\n",
	    path, errbuf);
    }

done:
    safe_release(err);
    safe_release(url);
    safe_release(data);

    return plist;
}

/* Initialise a Preferences object from the given pspec. This might be an
 * SCPreferences AppID or it might be the path to a plist file.
 */
Preferences::Preferences(const char * pspec)
    : m_pspec(pspec), m_plist(NULL), m_scpref(NULL)
{

    /* Try to load this as a plist file first. The SCPreferences API does a
     * bunch of work if it can't find the file, and we should avoid that unless
     * we have a good idea that it's necessary.
     */
    this->m_plist = load_plist_from_path(this->m_pspec.c_str());
    if (this->m_plist) {
	/* Loading a plist should always give you back a dictionary. */
	ASSERT(CFDictionaryGetTypeID() == CFGetTypeID(this->m_plist));
	VERBOSE("loaded plist %s\n", pspec);
	return;
    }

    cf_typeref<CFStringRef> appname(cfstring_wrap(getprogname()));
    cf_typeref<CFStringRef> appid(cfstring_wrap(this->m_pspec.c_str()));

    this->m_scpref = SCPreferencesCreate(kCFAllocatorDefault, appname, appid);
    if (this->m_scpref == NULL) {
	return;
    }

    /* If there was no existing preferences file, SCError() should return
     * kSCStatusNoConfigFile. We are only interested in reading preferences, so
     * we want to fail if there's no existing config.
     */

    if (SCError() != kSCStatusOK) {
	safe_release(this->m_scpref);
    }

    if (this->m_scpref) {
	VERBOSE("loaded SC preferences %s\n", pspec);
    }

}

Preferences::~Preferences()
{
    safe_release(this->m_plist);
    safe_release(this->m_scpref);
}

CFDataRef Preferences::create_signature(void) const
{
    if (this->m_plist) {
	return get_path_signature(this->m_pspec.c_str());
    }

    if (this->m_scpref) {
	CFDataRef sig;

	/* Copy the signature so we adhere to the get rule. */
	if ((sig = SCPreferencesGetSignature(m_scpref))) {
	    return CFDataCreateCopy(kCFAllocatorDefault, sig);
	}
    }

    return NULL;
}

/* Get the value for the given key. We don't allow NULL ad a value, so
 * returning NULL means failure (not present).
 */
CFPropertyListRef Preferences::get_value(CFStringRef key) const
{
    CFPropertyListRef prefval = NULL;

    if (!this->is_loaded()) {
	return NULL;
    }

    if (this->m_plist) {
	prefval = CFDictionaryGetValue((CFDictionaryRef)this->m_plist, key);
    } else if (this->m_scpref) {
	prefval = SCPreferencesGetValue(this->m_scpref, key);
    }

    /* Dump the raw keys for debugging. Useful for figuring out whether our
     * type conversion has gone awry.
     */
    if (Options::Debug) {
	DEBUGMSG("%s value for key %s:\n",
		this->m_pspec.c_str(), cfstring_convert(key).c_str());
	CFShow(prefval);
    }

    return prefval;
}

/* vim: set cindent ts=8 sts=4 tw=79 : */
