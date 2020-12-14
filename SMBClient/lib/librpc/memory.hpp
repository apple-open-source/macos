/*
 * Copyright (c) 2008 - 2010 Apple Inc. All rights reserved.
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

#ifndef MEMORY_HPP_8501DD7B_0F14_4CA8_B700_3F0C6B1653ED
#define MEMORY_HPP_8501DD7B_0F14_4CA8_B700_3F0C6B1653ED

#include "platform.h"
#include "compiler.h"

#if PLATFORM(UNIX)
#include <unistd.h> // getpagesize
#endif

#include <stddef.h> // size_t
#include <cstring> // std::memset
#include <cstdlib> // std::free

namespace platform {

template <class A> inline
void zero_memory(A& target)
{
    std::memset(&target, 0, sizeof(target));
}

template <class A, int SZ> inline
void zero_memory(A (&target)[SZ])
{
    std::memset(&target, 0, sizeof(target));
}

inline
void zero_memory(void * ptr, size_t nbytes)
{
    std::memset(ptr, 0, nbytes);
}

inline size_t pagesize(void)
{
#if PLATFORM(UNIX)
    return ::getpagesize();
#else
    return 4096;
#endif
}

/*!
 * Allocate or extend a buffer to the size given by nbytes.
 *
 * If the allocation fails, the installed new_handler is invoked. If the
 * new handler returns, the allocation is retried.
 *
 * The returned pointer must be released by free(3).
 */
void * allocate(void *, ::std::size_t);

/*!
 * Invoke the installed new_handler. The new_handler is installed with
 * std::set_new_handler and should either make more memory available or
 * terminate the process (the default new_handler throws std::bad_alloc).
 */
void invoke_new_handler(void);

/*!
 * Round an integral value up to the next boundary. This ought to be an
 * inline function, but we sometimes need it in constant expressions.
 */
#ifndef roundup
#define roundup(value, boundary) ( \
        ((value) + ((boundary) - 1)) & ~((boundary) - 1) \
    )
#endif

/*!
 * Align the given pointer to the correct byte boundary for type T.
 */
template <typename T, typename P>
void align_pointer(const P * (&ptr)) {
    uintptr_t tmp = reinterpret_cast<uintptr_t>(ptr);
    tmp = roundup(tmp, (uintptr_t)alignof(T));
    ptr = reinterpret_cast<const P *>(tmp);
}

/*!
 * Align the given pointer to the correct byte boundary for type T.
 */
template <typename T, typename P>
void align_pointer(P * (&ptr)) {
    uintptr_t tmp = reinterpret_cast<uintptr_t>(ptr);
    tmp = roundup(tmp, (uintptr_t)alignof(T));
    ptr = reinterpret_cast<P *>(tmp);
}

/*!
 * Align the given pointer to an arbitrary boundary.
 */
template <typename P>
void align_pointer(const P * (&ptr), unsigned n) {
    uintptr_t tmp = reinterpret_cast<uintptr_t>(ptr);
    tmp = roundup(tmp, (uintptr_t)n);
    ptr = reinterpret_cast<const P *>(tmp);
}

/*!
 * Align the given pointer to an arbitrary boundary.
 */
template <typename P>
void align_pointer(P * (&ptr), int n) {
    uintptr_t tmp = reinterpret_cast<uintptr_t>(ptr);
    tmp = roundup(tmp, n);
    ptr = reinterpret_cast<P *>(tmp);
}

template <typename T, typename P>
bool is_aligned(const P * ptr) {
    return ((uintptr_t)ptr % alignof(T)) == 0;
}

/*!
 * @class counted_ptr
 * @abstract a reference counting pointer for objects that contain their own
 * reference count.
 *
 * counted_ptr contrasts with shared_ptr, in which the reference
 * count is held outside the object. counted_ptr is preferable for objects
 * that you have the source for.
 *
 * Reference counts are make by doing unqualified calls to
 *       counted_ptr_addref(T *)
 *       counted_ptr_release(T *)
 *
 * If you need an atomically reference counted object, just inherit from
 * counted_ptr_base, eg:
 *      struct my_object : public counted_ptr_base {}
 *      typedef counted_ptr<my_object> my_object_ptr;
 *
 */
template <class T>
struct counted_ptr
{
    counted_ptr() : t_pointer(0) {}

    counted_ptr(const counted_ptr& p) : t_pointer(p.t_pointer) {
        if (t_pointer) {
            counted_ptr_addref(t_pointer);
        }
    }

    explicit counted_ptr(T * t) : t_pointer(t) {
        if (t_pointer) {
            counted_ptr_addref(t_pointer);
        }
    }

    ~counted_ptr() {
        if (t_pointer) {
            counted_ptr_release(t_pointer);
        }
    }

    template <typename A>
    counted_ptr& operator=(const counted_ptr<A>& rhs) {
        T * tmp = t_pointer;

        if ((t_pointer = rhs.t_pointer)) {
            counted_ptr_addref(t_pointer);
        }

        if (tmp) {
            counted_ptr_release(tmp);
        }

        return *this;
    }

    counted_ptr& operator=(const counted_ptr& p) {
        return operator=<T>(p);
    }

    bool operator==(const counted_ptr& rhs) const {
        return t_pointer == rhs.t_pointer;
    }

    bool operator<(const counted_ptr & rhs) const {
        return t_pointer < rhs.t_pointer;
    }

    T * get() const { return t_pointer; }
    operator bool() const { return t_pointer != 0; }
    T& operator*() const { return *t_pointer; }
    T * operator->() const { return t_pointer; }

private:
    T * t_pointer;
};

// NOTE: We need comparison operators so that counted_ptr objects sort into
// the standard STL containers correctly. The default sort isn't sufficient
// and will end up not adding objects to std::set correctly.

template<class T, class U>
bool operator>(const counted_ptr<T>& lhs, const counted_ptr<U>& rhs) {
    return rhs < lhs;
}

template<class T, class U>
bool operator<=(const counted_ptr<T>& lhs, const counted_ptr<U>& rhs) {
    return !(rhs < lhs);
}

template<class T, class U>
bool operator>=(const counted_ptr<T>& lhs, const counted_ptr<U>& rhs) {
    return !(lhs < rhs);
}

template <typename T>
struct scoped_ptr_delete
{
    void operator()(T * t) { delete t; }

};

template <typename T>
struct scoped_ptr_array_delete
{
    void operator()(T * t) { delete [] t; }
};

template <typename T, void (*D)(T*) >
struct scoped_ptr_free
{
    void operator()(T * t) { D(t); }
};

typedef scoped_ptr_free<void, std::free> malloc_ptr_free;

/*!
 * @class scoped_ptr
 * @abstract just like std::auto_ptr, except that it takes a custom deletor
 * type.
 *
 * The default behavior of scoped_ptr is to use the delete operator
 * to delete the contained pointer. If you have a custom delete
 * function (eg, std::free), then you can use scoped_ptr_free as the
 * deletor object.
 *
 * scoped_ptr is intended to be identical to std::auto_ptr, so the
 * usual auto_ptr caveats apply;
 */
template <typename T, typename D = scoped_ptr_delete<T> >
struct scoped_ptr {

    scoped_ptr() : pointer(0) {}
    explicit scoped_ptr(T * t) : pointer(t) {}
    scoped_ptr(scoped_ptr& rhs) : pointer(rhs.release()) {}

    ~scoped_ptr() {
        D deletor; deletor(pointer);
    }

    // Release and copy, just like std::auto_ptr.
    template <typename A>
    scoped_ptr& operator=(scoped_ptr<A, D>& rhs) throw() {
        pointer = rhs.release();
        return *this;
    }

    scoped_ptr& operator=(scoped_ptr& rhs) throw() {
        return operator=<T>(rhs);
    }

    T * get() const { return pointer; }
    operator bool() const { return pointer != 0; }
    T& operator*() const { return *pointer; }
    T * operator->() const { return pointer; }

    T& operator[](unsigned offset) { return pointer[offset]; }
    const T& operator[](unsigned offset) const { return pointer[offset]; }

    T * release() throw() {
        T * tmp = pointer;
        pointer = 0;
        return tmp;
    }

    void reset(T * ptr = 0) throw() {
        if (ptr != pointer) {
            D destroy; destroy(pointer);
            pointer = ptr;
        }
    }

private:

    T * pointer;
};

/*!
 * Convenience class for using scoped_ptr with free(3).
 *
 * Instead of:
 *     typedef platform::scoped_ptr<char, platform::malloc_ptr_free> char_ptr
 * you can do the shorter, more legible:
 *     typedef platform::malloc_ptr<char>::scoped_ptr char_ptr;
 *
 * This is a partial workaround for the lack of template typedefs.
 */
template <typename T>
struct malloc_ptr
{
    typedef scoped_ptr<T, malloc_ptr_free> scoped_ptr;
};

} // namespace platform

#endif // MEMORY_HPP_8501DD7B_0F14_4CA8_B700_3F0C6B1653ED
/* vim: set ts=4 sw=4 tw=79 et cindent : */
