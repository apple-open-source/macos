// mk4str.h --
// $Id: mk4str.h 1230 2007-03-09 15:58:53Z jcw $
// This is part of Metakit, see http://www.equi4.com/metakit.html

/** @file
 * Declarations of the string package.
 */

#ifndef __MK4STR_H__
#define __MK4STR_H__

/////////////////////////////////////////////////////////////////////////////

#if q4_MFC                      // Microsoft Foundation Classes

#ifdef _WINDOWS
#include <afxwin.h>
#else 
#include <afxcoll.h>
#endif 

#if _MSC_VER == 800
// MSVC 1.52 thinks a typedef has no constructor, use define instead
#define c4_String CString
#elif _MSC_VER >= 1300
// VC 7.0 does not like "class" (6-2-2002, Zhang Dehua)
typedef CString c4_String;
#else 
typedef class CString c4_String;
#endif 

#elif q4_STD                    // STL and standard strings

#include <string>

#if !defined (d4_std)           // the default is to use namespaces
#define d4_std std
#endif 

/// STL-based string class, modeled after the MFC version
class c4_String: public d4_std::string {
    typedef d4_std::string string;

  public:
    c4_String();
    c4_String(char ch, int nDup = 1);
    c4_String(const char *str);
    c4_String(const void *ptr, int len);
    c4_String(const d4_std::string &s);
    c4_String(const c4_String &s);
    ~c4_String();

    const c4_String &operator = (const c4_String &);

    operator const char *()const;

    char operator[](int i)const;

    friend c4_String operator + (const c4_String &, const c4_String &);
    friend c4_String operator + (const c4_String &, const char*);
    friend c4_String operator + (const char *, const c4_String &);

    const c4_String &operator += (const c4_String &s);
    const c4_String &operator += (const char *s);

    int GetLength()const;
    bool IsEmpty()const;
    void Empty();

    c4_String Mid(int nFirst, int nCount = 25000)const;
    c4_String Left(int nCount)const;
    c4_String Right(int nCount)const;

    int Compare(const char *str)const;
    int CompareNoCase(const char *str)const;

    bool operator < (const c4_String &str)const;

    int Find(char ch)const;
    int ReverseFind(char ch)const;
    int FindOneOf(const char *set)const;

    int Find(const char *sub)const;

    c4_String SpanIncluding(const char *set)const;
    c4_String SpanExcluding(const char *set)const;
};

bool operator == (const c4_String &, const c4_String &);
bool operator != (const c4_String &, const c4_String &);

d4_inline bool operator == (const c4_String &s1, const char *s2);
d4_inline bool operator == (const char *s1, const c4_String &s2);

d4_inline bool operator != (const c4_String &s1, const char *s2);
d4_inline bool operator != (const char *s1, const c4_String &s2);

#else // Universal replacement classes

/// An efficient string class, modeled after the MFC version
class c4_String {
  public:
    c4_String();
    c4_String(char ch, int nDup = 1);
    c4_String(const char *str);
    c4_String(const unsigned char *str);
    c4_String(const void *ptr, int len);
    c4_String(const c4_String &s);
    ~c4_String();

    const c4_String &operator = (const c4_String &);

    operator const char *()const;
    operator const unsigned char *()const;

    char operator[](int i)const;

    friend c4_String operator + (const c4_String &, const c4_String &);
    friend c4_String operator + (const c4_String &, const char*);
    friend c4_String operator + (const char *, const c4_String &);
    //  friend c4_String operator+ (const c4_String&, char);
    //  friend c4_String operator+ (char, const c4_String&);

    const c4_String &operator += (const c4_String &s);
    const c4_String &operator += (const char *s);
    //  const c4_String& operator+= (char c);

    int GetLength()const;
    bool IsEmpty()const;
    void Empty(); // free up the data

    c4_String Mid(int nFirst, int nCount = 25000)const;
    c4_String Left(int nCount)const; // first nCount chars
    c4_String Right(int nCount)const; // last nCount chars

    friend bool operator == (const c4_String &, const c4_String &); // memcmp
    friend bool operator != (const c4_String &, const c4_String &); // opposite

    // only defined for strings having no zero bytes inside them:

    int Compare(const char *str)const; // strcmp
    int CompareNoCase(const char *str)const; // stricmp

    bool operator < (const c4_String &str)const;

    int Find(char ch)const; // strchr
    int ReverseFind(char ch)const; // strrchr
    int FindOneOf(const char *set)const; // strpbrk

    int Find(const char *sub)const; // strstr

    c4_String SpanIncluding(const char *set)const; // strspn
    c4_String SpanExcluding(const char *set)const; // strcspn

  private:
    void Init(const void *p, int n);
    const char *Data()const;
    int FullLength()const;

    unsigned char *_value;
};

bool operator == (const c4_String &s1, const char *s2);
bool operator == (const char *s1, const c4_String &s2);

bool operator != (const c4_String &s1, const char *s2);
bool operator != (const char *s1, const c4_String &s2);

#endif // q4_MFC elif q4_STD else q4_UNIV

/////////////////////////////////////////////////////////////////////////////

#if q4_INLINE
#include "mk4str.inl"
#endif 

/////////////////////////////////////////////////////////////////////////////

#endif // __MK4STR_H__
