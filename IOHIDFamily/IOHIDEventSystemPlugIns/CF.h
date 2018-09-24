//
//  CF.h
//  M8
//
//  Created by YG on 6/1/15.
//  Copyright © 2015 Yevgen Goryachok. All rights reserved.
//


#ifndef _LIBPP_CF_h
#define _LIBPP_CF_h
#include <CoreFoundation/CoreFoundation.h>
#include <iostream>
#include <type_traits>
#include <vector>

class CFStringRefWrap;

template <class T>
class CFRefWrap {
 
  T ref_;
  
protected:

  CFRefWrap () {
  }

public:

  CFRefWrap (T ref, bool own = false) : ref_ (ref) {
    if (ref_ && !own) {
      CFRetain (ref_);
    }
  }

  CFRefWrap (const CFRefWrap& src): ref_(src.ref_) {
    if (ref_) {
      CFRetain (ref_);
    }
  }

  virtual ~CFRefWrap () {
    CFTypeRef tmp = ref_;
    ref_ = NULL;
    if (tmp) {
      CFRelease (tmp);
    }
  }
  
  CFRefWrap& operator= (T ref) {
    T tmp = ref_;
    ref_ = ref;
    if (ref_) {
      CFRetain(ref);
    }
    if (tmp) {
      CFRelease (tmp);
    }
    return *this;
  }

  CFRefWrap& operator= (const CFRefWrap& src) {
    T tmp = ref_;
    ref_ = src.ref_;
    if (ref_ != NULL) {
      CFRetain (ref_);
    }
    if (tmp) {
      CFRelease (tmp);
    }
    return *this;
  }

  void Set(T ref) {
    ref_ = ref;
  }

  T Reset() {
    T tmp = ref_;
    ref_ = NULL;
    return tmp;
  }

  operator T() const {
    return (T)ref_;
  }
  
  size_t RetainCount () {
    if (ref_ != NULL) {
      return CFGetRetainCount(ref_);
    }
    return 0;
  }
  
  T Reference() const {
    return ref_;
  }

  explicit operator bool() const {
      return ref_ ? true : false;
  }
    
  std::string Description() const {
    std::string result = "";
    if (Reference()) {
      CFStringRef desc = CFCopyDescription(Reference());
      result = CFStringGetCStringPtr(desc, kCFStringEncodingMacRoman);
      CFRelease(desc);
    }
    return result;
  }
};


class CFBooleanRefWrap : public CFRefWrap<CFBooleanRef> {

public:
  
  CFBooleanRefWrap (CFTypeRef value, bool own = false):CFRefWrap((value && CFGetTypeID(value) == CFBooleanGetTypeID()) ? (CFBooleanRef)value : NULL, own) {
    
  }
  CFBooleanRefWrap (CFBooleanRef value, bool own = false):CFRefWrap((value && CFGetTypeID(value) == CFBooleanGetTypeID()) ? value : NULL, own) {

  }
  explicit operator bool() const {
      return (Reference() && CFGetTypeID(Reference()) == CFBooleanGetTypeID() && CFBooleanGetValue(Reference()))  ? true : false;
  }

};


class CFNumberRefWrap : public CFRefWrap<CFNumberRef> {

  template <class T, typename Enable = void> struct TypeToNumber;
  template <class T> struct TypeToNumber <T, typename std::enable_if<std::is_floating_point<T>::value && (sizeof(T) == 4)>::type> {enum {type = kCFNumberFloatType};};
  template <class T> struct TypeToNumber <T, typename std::enable_if<std::is_floating_point<T>::value && (sizeof(T) == 8)>::type> {enum {type = kCFNumberDoubleType};};
  template <class T> struct TypeToNumber <T, typename std::enable_if<std::is_integral<T>::value && (sizeof(T) == 1)>::type> {enum {type = kCFNumberSInt8Type };};
  template <class T> struct TypeToNumber <T, typename std::enable_if<std::is_integral<T>::value && (sizeof(T) == 2)>::type> {enum {type = kCFNumberSInt16Type};};
  template <class T> struct TypeToNumber <T, typename std::enable_if<std::is_integral<T>::value && (sizeof(T) == 4)>::type> {enum {type = kCFNumberSInt32Type};};
  template <class T> struct TypeToNumber <T, typename std::enable_if<std::is_integral<T>::value && (sizeof(T) == 8)>::type> {enum {type = kCFNumberSInt64Type};};

public:
  
  template<typename T>
  CFNumberRefWrap (T value, typename std::enable_if<std::is_arithmetic<T>::value, void >::type* = NULL) : CFRefWrap<CFNumberRef>(NULL) {
    Set(CFNumberCreate (kCFAllocatorDefault, (CFNumberType)TypeToNumber<T>::type, &value));
  }

  CFNumberRefWrap (CFTypeRef value, bool own = false):CFRefWrap((value && CFGetTypeID(value) == CFNumberGetTypeID()) ? (CFNumberRef)value : NULL, own) {
  }
  
  CFNumberRefWrap (CFNumberRef value, bool own = false):CFRefWrap((value && CFGetTypeID(value) == CFNumberGetTypeID()) ? value : NULL, own) {
  }

  CFNumberRefWrap (const CFNumberRefWrap &value) : CFRefWrap(value) {
  }

  CFNumberRefWrap () : CFRefWrap (NULL) {
  }

  explicit operator bool() const {
      return Reference() ? (CFGetTypeID( Reference()) == CFNumberGetTypeID()) : false;
  }

  template<class T, class = typename std::enable_if<std::is_arithmetic<T>::value>::type>
  operator T () const {
    T value = 0;
    CFNumberGetValue ((CFNumberRef)Reference(), (CFNumberType)TypeToNumber<T>::type, (void*)&value);
    return value;
  }
  friend bool operator==(const CFNumberRefWrap &lhs, const CFNumberRefWrap &rhs) {
    return CFEqual(lhs.Reference(), rhs.Reference());
  }
  friend bool operator!=(const CFNumberRefWrap &lhs, const CFNumberRefWrap &rhs) {
    return !CFEqual(lhs.Reference(), rhs.Reference());
  }
};


class CFStringRefWrap : public CFRefWrap<CFStringRef> {
  
public:
  CFStringRefWrap (CFStringRef str, bool own = false) : CFRefWrap ((str && CFGetTypeID(str) == CFStringGetTypeID()) ? str : NULL, own) {
  }
  CFStringRefWrap (CFTypeRef str, bool own = false) : CFRefWrap ((str && CFGetTypeID(str) == CFStringGetTypeID()) ? (CFStringRef)str : NULL, own) {
  }
  CFStringRefWrap (const char* str) : CFRefWrap () {
    Set(CFStringCreateWithCString(NULL , str, kCFStringEncodingMacRoman));
  }
  CFStringRefWrap (std::string str) : CFRefWrap () {
    Set(CFStringCreateWithCString(NULL , str.c_str(), kCFStringEncodingMacRoman));
  }
  operator std::string() {
    if (Reference() == NULL) {
      return "";
    }
    return CFStringGetCStringPtr(Reference(), kCFStringEncodingMacRoman);
  }
};

template <typename T>
class _CFDictionaryRefWrap : public CFRefWrap<T> {
public:
  _CFDictionaryRefWrap ():  CFRefWrap<T> () {
  }
  _CFDictionaryRefWrap (T value, bool own = false):  CFRefWrap<T> ((value && CFGetTypeID(value) == CFDictionaryGetTypeID()) ? value : NULL, own) {
  }
  CFTypeRef operator[] (CFTypeRef key) const {
    return CFDictionaryGetValue(CFRefWrap<T>::Reference(), key);
  }
  CFIndex Count () const {
    return CFDictionaryGetCount(CFRefWrap<T>::Reference()) ;
  }
  boolean_t ContainValue (CFTypeRef value) const {
    return CFDictionaryContainsValue(CFRefWrap<T>::Reference(), value) ;
  }
  boolean_t ContainKey (CFTypeRef key) const {
    return CFDictionaryContainsKey (CFRefWrap<T>::Reference(), key);
  }
};

class CFDictionaryRefWrap : public _CFDictionaryRefWrap<CFDictionaryRef> {

public:
  
  CFDictionaryRefWrap (const CFDictionaryRefWrap &value) : _CFDictionaryRefWrap (value) {
  }

  CFDictionaryRefWrap (CFDictionaryRef value, bool own = false):  _CFDictionaryRefWrap(value, own) {
  }

  CFDictionaryRefWrap (std::initializer_list<CFTypeRef> keys, std::initializer_list<CFTypeRef> values):_CFDictionaryRefWrap() {
     const std::vector<CFTypeRef> key_array = keys;
     const std::vector<CFTypeRef> values_array = values;
     Set (CFDictionaryCreate(kCFAllocatorDefault, (const void**)key_array.data(), (const void**)values_array.data(), key_array.size(), &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));
  }

  CFDictionaryRefWrap (std::string xml):  _CFDictionaryRefWrap() {
    CFErrorRef            err;
    CFPropertyListFormat  format;
    CFPropertyListRef dict = CFPropertyListCreateWithData (
                              NULL,
                              CFDataCreate(NULL, (const UInt8 *)xml.c_str(), xml.length()),
                              0,
                              &format,
                              &err
                              );
    Set ((CFDictionaryRef)dict);
  }
};

class CFMutableDictionaryRefWrap : public _CFDictionaryRefWrap<CFMutableDictionaryRef> {

public:

  CFMutableDictionaryRefWrap (int capacity = 0) : _CFDictionaryRefWrap () {
    Set (CFDictionaryCreateMutable (NULL, capacity, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));
  }
  
  CFMutableDictionaryRefWrap (const CFMutableDictionaryRefWrap &value) : _CFDictionaryRefWrap (value) {
  
  }
  
  explicit CFMutableDictionaryRefWrap (CFMutableDictionaryRef value):  _CFDictionaryRefWrap (value) {

  }

  inline CFMutableDictionaryRefWrap& operator=(const CFMutableDictionaryRef& rhs) {
    return (CFMutableDictionaryRefWrap&)CFRefWrap<CFMutableDictionaryRef>::operator=(rhs);
  }

  void SetValueForKey (CFTypeRef key, CFTypeRef value) {
    if (key && value) {
      CFDictionarySetValue(Reference(), key, value);
    }
  }
  
  void SetValueForKey (CFTypeRef key, CFNumberRefWrap  value) {
    SetValueForKey (key, (CFTypeRef)value);
  }
  
  void SetValueForKey (CFTypeRef key, std::string  value) {
    SetValueForKey (key, (CFTypeRef)CFStringRefWrap(value));
  }
  
  void SetValueForKey (CFTypeRef key, boolean_t  value) {
    SetValueForKey (key, value ? kCFBooleanTrue : kCFBooleanFalse);
  }
  
  template<typename T>
  void SetValueForKey (CFTypeRef key, T value, typename std::enable_if<std::is_arithmetic<T>::value, void >::type* = NULL) {
    SetValueForKey (key, (CFTypeRef)CFNumberRefWrap(value));
  }
  
  void Remove (CFTypeRef key) {
    CFDictionaryRemoveValue(Reference(), key);
  }

  void RemoveAll () {
    CFDictionaryRemoveAllValues(Reference());
  }
  
  typedef std::function<void (const void *key, const void *value)> DictionaryApplier;

  void Apply (DictionaryApplier applier) const {
      CFDictionaryApplyFunction(
                                Reference(),
                                [](const void *key, const void *value, void *context)->void {
                                    DictionaryApplier &__applier = *(DictionaryApplier *) context ;
                                    __applier(key, value);
                                },
                                (void*)&applier);
  }
};


template <typename T>
class _CFArrayRefWrap : public CFRefWrap<T> {
public:
  _CFArrayRefWrap (T value, bool own = false):  CFRefWrap<T> ((value && CFGetTypeID(value) == CFArrayGetTypeID()) ? value : NULL, own) {
  }
  CFTypeRef operator[] (CFIndex index) {
    return CFArrayGetValueAtIndex (CFRefWrap<T>::Reference(), index);
  }
  CFIndex Count () const {
    return CFArrayGetCount(CFRefWrap<T>::Reference()) ;
  }
  boolean_t ContainValue (CFTypeRef value) const {
    return CFArrayContainsValue(CFRefWrap<T>::Reference(), {0, static_cast<CFIndex>(Count())}, value) ;
  }
};

class CFArrayRefWrap : public _CFArrayRefWrap <CFArrayRef> {
public:
  CFArrayRefWrap (const CFArrayRefWrap &value) : _CFArrayRefWrap (value) {
  }
  CFArrayRefWrap (CFArrayRef value, boolean_t own = false) : _CFArrayRefWrap (value, own) {
  }
  CFArrayRefWrap Clone () {
    CFArrayRefWrap result (CFArrayCreateCopy(CFGetAllocator(Reference()), Reference()), true);
    return result;
  }
};

class CFMutableArrayRefWrap : public _CFArrayRefWrap <CFMutableArrayRef> {
public:
  CFMutableArrayRefWrap (const CFMutableArrayRefWrap &value) : _CFArrayRefWrap (value) {
  }
  CFMutableArrayRefWrap (CFMutableArrayRef value, boolean_t own = false) : _CFArrayRefWrap (value, own) {
  }
  CFMutableArrayRefWrap (CFIndex capacity = 0, const CFAllocatorRef alloc = kCFAllocatorDefault) : _CFArrayRefWrap (CFArrayCreateMutable (alloc, capacity, &kCFTypeArrayCallBacks), true) {
  }
  CFMutableArrayRefWrap & Append (CFTypeRef value)  {
    CFArrayAppendValue(Reference(), value);
    return *this;
  }
  CFMutableArrayRefWrap & Remove (CFIndex index)  {
    CFArrayRemoveValueAtIndex(Reference(), index);
    return *this;
  }

  CFMutableArrayRefWrap & RemoveAll ()  {
    CFArrayRemoveAllValues(Reference());
    return *this;
  }

  CFMutableArrayRefWrap Clone () {
    CFMutableArrayRefWrap result (CFArrayCreateMutableCopy(CFGetAllocator(Reference()), 0, Reference()), true);
    return result;
  }
};

template <typename T>
class _CFSetRefWrap : public CFRefWrap<T> {
public:
  _CFSetRefWrap (T value, bool own = false):  CFRefWrap<T> ((value && CFGetTypeID(value) == CFSetGetTypeID()) ? value : NULL, own) {
  }
  CFTypeRef operator[] (CFTypeRef value) {
    return CFSetGetValue (CFRefWrap<T>::Reference(), value);
  }
  CFIndex Count () const {
    return CFSetGetCount(CFRefWrap<T>::Reference()) ;
  }
  boolean_t ContainValue (CFTypeRef value) const {
    return CFSetContainsValue(CFRefWrap<T>::Reference(), value) ;
  }
  
  typedef std::function<void (const void *value)> Applier;

  void Apply (_CFSetRefWrap::Applier applier) const {
      CFSetApplyFunction(
                        CFRefWrap<T>::Reference(),
                        [](const void *value, void *context)->void {
                          Applier &__applier = *(Applier *) context ;
                          __applier(value);
                        },
                        (void*)&applier);
  }
};

class CFMutableSetRefWrap : public _CFSetRefWrap <CFMutableSetRef> {
public:
  
  CFMutableSetRefWrap (int capacity = 0) : _CFSetRefWrap (NULL) {
    Set (CFSetCreateMutable (kCFAllocatorDefault, capacity, &kCFTypeSetCallBacks));
  }
  CFMutableSetRefWrap (const CFMutableSetRefWrap &value) : _CFSetRefWrap (value) {
  }
  CFMutableSetRefWrap (CFMutableSetRef value, boolean_t own = false) : _CFSetRefWrap (value, own) {
  }
  void SetValue (CFTypeRef value) {
    CFSetSetValue(Reference(), value);
  }
  void RemoveValue (CFTypeRef value) {
    CFSetRemoveValue(Reference(), value);
  }
};


#endif /* _LIBPP_CF_h */
