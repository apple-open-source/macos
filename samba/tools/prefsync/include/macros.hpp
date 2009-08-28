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
#include <SystemConfiguration/SCPreferences.h>

#include <stdexcept>
#include <ios>

template <class A> inline
void Assert_msg(A assertion,
		const char * const cond_string,
		const char * const location,
		int line)
{
    if (!assertion) {
	char buf[256];

	snprintf(buf, sizeof(buf),
	    "assertion failed at %s(%d): %s",
	    location, line, cond_string);

	throw std::runtime_error(buf);
    }
}

#define ASSERT(cond) \
    Assert_msg((cond), #cond, __FUNCTION__, __LINE__)

/* A verbose message (uses gcc CPP extension). */
#define VERBOSE(fmt, ...) \
    (Options::Verbose && std::fprintf(stdout, "%s: " fmt, getprogname(), ## __VA_ARGS__))

/* A debug message (uses gcc CPP extension). */
#define DEBUGMSG(fmt, ...) \
    (Options::Debug && \
     std::fprintf(stderr, "%s(%s): " fmt, \
	 __FILE__, __FUNCTION__, ## __VA_ARGS__))

template <class A> inline
void zero_struct(A& target)
{
    memset(&target, 0, sizeof(target));
}

template <class A> inline
void zero_struct(A * target)
{
    memset(target, 0, sizeof(*target));
}

/* Call CFRelease on a CFTypeRef and reset it to NULL. */
template <class A>
inline void safe_release(A& target)
{
    if (target) {
	CFRelease(target);
	target = NULL;
    }
}

/* Call CFRetain on a CFTypeRef and safely ignore NULL. */
template <class A>
inline A safe_retain(A& target)
{
    if (target) {
	/* We need the cast here because CFRetain returns a CFTypeRef which is
	 * a const void *, and we are declared to return whatever type A is,
	 * although it basically has to be some CF pointer type.
	 */
	return (A)CFRetain(target);
    } else {
	return NULL;
    }
}

/* CFDataRef insertion operator. Used to stringise preference signatures. */
template <class Ch, class Tr> std::basic_ostream<Ch, Tr>&
operator<<(std::basic_ostream<Ch, Tr>& out, CFDataRef data)
{
    CFIndex len;
    const UInt8 * ptr;

    if (data == NULL) {
	return out;
    }

    if ((len = CFDataGetLength(data)) <= 0 ||
	(ptr = CFDataGetBytePtr(data)) == NULL) {
	return out;
    }

    std::ios_base::fmtflags oldflags = out.setf(std::ios_base::hex,
					    std::ios_base::basefield);

    for (const UInt8 * b = ptr; b < (ptr + len); ++b) {
	/* This cast matters - std::basic_ostream hex formatting only ppears to
	 * work for int.
	 */
	out << (int)(*b);
    }

    out.flags(oldflags);
    return out;
}

std::string cftype_string(CFTypeID obj_type);

static inline
std::string cftype_string(CFTypeRef obj)
{
    return cftype_string(CFGetTypeID(obj));
}

/* Compare two CFTypeRefs. CFEqual doesn't handle being passed NULL, so this
 * is a safe wrapper.
 */
template <class T>
static bool cftype_equal(const T& lhs, const T& rhs)
{
	if (rhs == lhs) {
	    /* Pointer comparison matches. */
	    return true;
	} else if (lhs == NULL || rhs == NULL) {
	    /* One side (but not both) is NULL. */
	    return false;
	} else {
	    return (CFEqual(lhs, rhs) != 0);
	}
}

/* Return true if we are running on Mac OSX Server. */
bool is_server_system(void);

/* Convert a CFString to a UTF8 std::string. */
std::string cfstring_convert(CFStringRef cfstr);

/* Convert a CFString to an  arbitrary 8 bit std::string. */
std::string cfstring_convert(CFStringRef cfstr, CFStringEncoding e);

/* Wrap a CFString around a UTF8 C-style string. */
CFStringRef cfstring_wrap(const char * str);

#define SEC_TO_USEC(sec)    ((sec) * 1000000)
#define USEC_TO_NSEC(usec)  ((usec) * 1000)
#define USEC_TO_SEC(usec)   ((usec) / 1000000)
#define MSEC_TO_USEC(msec)  ((msec) * 1000)
#define MSEC_TO_NSEC(msec)  ((msec) * 1000000)

template <class T> class cf_typeref
{
public:
    cf_typeref(T ref) : m_ref(ref) {}
    ~cf_typeref() { safe_release(this->m_ref); }

    /* Return the CFTypeRef we are holding. */
    operator T() const { return this->m_ref; }

    /* We are false if the CFTypeRef we hold is NULL. */
    operator bool() const { return this->m_ref != NULL; }

    bool operator==(const T& rhs) const
    {
	return cftype_equal<T>(this->m_ref, rhs);
    }

    /* Compare to a matching cf_typeref<T>. */
    bool operator==(const cf_typeref& rhs) const
    {
	return cftype_equal<T>(this->m_ref, rhs->m_ref);
    }

private:
    /* Disable assignment and copy constructor. We don't wan to be doing
     * reference counting. This class is only intended for very simple RAII
     * codepaths.
     */
    cf_typeref(const cf_typeref&); // nocopy
    cf_typeref& operator=(const cf_typeref&); // nocopy

    T m_ref;
};

/* vim: set cindent ts=8 sts=4 tw=79 : */
