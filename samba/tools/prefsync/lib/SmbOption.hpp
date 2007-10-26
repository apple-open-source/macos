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

#include <CoreFoundation/CoreFoundation.h>

class Preferences;
class SmbConfig;

class SmbOption
{
public:
    SmbOption(const char * optname) : m_optname(optname) {}
    virtual ~SmbOption() {}

    virtual void reset(const Preferences&) = 0;
    virtual void emit(SmbConfig&) = 0;
    virtual const char * name() const { return m_optname; }

private:
    SmbOption(const SmbOption&); // nocopy
    SmbOption& operator=(const SmbOption&); // nocopy

    const char * m_optname;
};

/* An option that is defined but has no corresponding smb.conf parameter. */
class NullOption : public SmbOption
{
public:
    NullOption(const char * optname) : SmbOption(optname) {}
    ~NullOption() {}

    virtual void reset(const Preferences& prefs) {}

    virtual void emit(SmbConfig& config)
    {
	VERBOSE("ignoring unimplemented option '%s'\n",
	    this->name());
    }
};

template <class T> class SimpleOption : public SmbOption
{
public:
    SimpleOption(const char * optname, const char * param, const T def)
	: SmbOption(optname), m_param(param), m_value(def) {}
    ~SimpleOption() {}

    virtual void reset(const Preferences&);
    virtual void emit(SmbConfig&);

private:
    SimpleOption(const SimpleOption&); // nocopy
    SimpleOption& operator=(const SimpleOption&); // nocopy

    const char *    m_param;	/* smb.conf param name */
    T		    m_value;
};

/* No implementation of this, just giving the compiler a heads-up. */
template <class T> T property_convert(CFPropertyListRef val);

template <>
std::string property_convert<std::string>(CFPropertyListRef val);

template <>
unsigned property_convert<unsigned>(CFPropertyListRef val);

template <>
bool property_convert<bool>(CFPropertyListRef val);

template <class T>
void SimpleOption<T>::reset(const Preferences& prefs)
{
    try {

	CFPropertyListRef	val;
	cf_typeref<CFStringRef>	prefkey(cfstring_wrap(this->name()));

	val = prefs.get_value(prefkey);

	if (val != NULL) {
	    VERBOSE("found %s value for %s\n",
		    cftype_string(val).c_str(), this->name());
#ifdef DEBUG
	    CFShow(val);
#endif
		this->m_value = property_convert<T>(val);
	}

    } catch (std::exception& e) {
	VERBOSE("error setting %s: %s\n",
	    this->name(), e.what());
    }

}

template <class T>
void SimpleOption<T>::emit(SmbConfig& smb)
{
    smb.set_param(SmbConfig::GLOBAL,
	    make_smb_param(this->m_param, this->m_value));
}

#define CATCH_TYPE_ERROR(expr) do { try { \
    expr ; \
} catch (std::exception& e) { \
	    VERBOSE("error setting %s: %s\n", this->name(), e.what()); \
} } while (0)

/* vim: set cindent ts=8 sts=4 tw=79 : */
