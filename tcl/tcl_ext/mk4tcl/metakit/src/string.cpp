// string.cpp --
// $Id: string.cpp 1230 2007-03-09 15:58:53Z jcw $
// This is part of Metakit, see http://www.equi4.com/metakit.html

/** @file
 * yet another string implementation
 */

#include "header.h"

/* these definitions could be used instead of header.h ...
#define q4_UNIV 1
#define d4_inline
#include "mk4str.h"
#define d4_reentrant
#define d4_assert(x)
 */

#if q4_UNIV // until end of source
/////////////////////////////////////////////////////////////////////////////

#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#ifdef _AIX
#include <strings.h>
#endif 

#if !q4_INLINE
#include "mk4str.inl"
#endif 

/////////////////////////////////////////////////////////////////////////////

#if q4_WINCE

// MS C/C++ has this handy stricmp: a case-insensitive version of strcmp
// This version only works with 7-bit ASCII characters 0x00 through 0x7F

static int strcasecmp(const char *p1, const char *p2) {
  int c1, c2;

#ifdef d4_USE_UNOPTIMIZED_CODE
  do {
    c1 = tolower(*p1++);
    c2 = tolower(*p2++);
  } while (c1 != 0 && c1 == c2);
#else 
  do {
    c1 =  *p1++;
    c2 =  *p2++;
  } while (c1 != 0 && (c1 == c2 || tolower(c1) == tolower(c2)));

  c1 = tolower(c1);
  c2 = tolower(c2);
#endif 

  return c1 - c2;
}

const char *strrchr(const char *p, char ch) {
  const char *q = 0;
  while (*p)
    if (*p++ == ch)
      q = p;
  return q;
}

#elif q4_MSVC || q4_WATC || q4_BORC || (q4_MWCW && __MWERKS__ < 0x3000)
#define strcasecmp stricmp
#endif 

/////////////////////////////////////////////////////////////////////////////
//
//  This string class implement functionality which is very similar to that
//  provided by the CString class of the Microsoft Framework Classes (MFC).
//  
//  There are also several major differences:
//  
//    1) This class uses reference counting to avoid massive copying.
//       Consequently, function return as well as assignment is very fast.
//    2) Strings of up to 255 bytes can contain any data, even null bytes.
//       Longer strings can not contain any null bytes past position 255.
//    3) This class can produce a "const char*" without overhead, but it
//       can also cast to the byte-counted "const unsigned char*" used
//       everywhere in Macintosh applications (as StringPtr, Str255, etc).
//    4) This source code is not derived from Microsoft's code in any way.
//    
//  A good way to use this class, is to always use c4_String for function
//  return values and "const [unsigned] char*" for all parameters. Together,
//  these two choices will remove the need for nearly any messy casts.
//
//  Note: MFC 4.0 has now adopted refcounts, and is a good alternative to
//      this code (but a bit bulkier, it also has Unicode support).

// 2001-11-27, stop releasing nullvec, to allow MT use
d4_reentrant static unsigned char *nullVec = 0;

static int fInc(unsigned char *p) {
  ++ *p;
  if (*p)
    return 1;

  -- *p;
  return 0;
}

inline static void fDec(unsigned char *p) {
  -- *p;
  if (! *p && p != nullVec)
    delete [] p;
}

c4_String::c4_String(char ch, int n /* =1 */) {
  if (n < 0)
    n = 0;

  _value = new unsigned char[n + 3];

  _value[0] = 1; // see Init() member
  memset(_value + 2, ch, n);
  _value[1] = (unsigned char)(n <= 255 ? n : 255);
  _value[n + 2] = 0;
}

c4_String::c4_String(const char *p) {
  Init(p, p != 0 ? strlen(p): 0);
}

c4_String::c4_String(const c4_String &s) {
  if (fInc(s._value))
    _value = s._value;
  else
    Init(s.Data(), s.GetLength());
}

c4_String::~c4_String() {
  fDec(_value);
}

const c4_String &c4_String::operator = (const c4_String &s) {
  unsigned char *oldVal = _value;
  if (fInc(s._value))
    _value = s._value;
  else
    Init(s.Data(), s.GetLength());
  fDec(oldVal);

  return  *this;
}

c4_String operator + (const c4_String &a, const c4_String &b) {
  const int aCnt = a.GetLength();
  int sum = aCnt + b.GetLength();

  c4_String result('\0', sum); // set up correct size, then fix contents
  memcpy(result._value + 2, a.Data(), aCnt);
  memcpy(result._value + 2+aCnt, b.Data(), sum - aCnt);

  return result;
}

void c4_String::Init(const void *p, int n) {
  if (p == NULL || n <= 0) {
    //  Optimization to significantly speed-up init of empty strings:
    //  share a common entry, which avoids *LOTS* of tiny mem allocs.
    //  
    //  Especially "new [...] c4_String" will benefit a lot, as well as:
    //  
    //    c4_String s;    // this would have caused a new allocation
    //    s = ...     // then immediately drops the count back
    //  
    //  2001/11/27: changed to never release this empty vector, for MT use
    //  		the new logic is to completely ignore its ref count

    if (!nullVec) {
      // obtain a valid new empty string buffer to keep around
      unsigned char *nv = new unsigned char[3];
      nv[0] = nv[1] = nv[2] = 0;
      // only set static value after item is fully inited (avoid MT race)
      nullVec = nv;
    }

    _value = nullVec; // use this buffer as our empty string
    return ; // done... that was quick, wasn't it?
  }

  _value = new unsigned char[n + 3];

  _value[0] = 1; // many assumptions here: set the reference count to 1

  if (n > 0)
    memcpy(_value + 2, p, n);
  _value[1] = (unsigned char)(n <= 255 ? n : 255);
  _value[n + 2] = 0;
}

int c4_String::FullLength()const {
  int n = _value[1];
  return n < 255 ? n : n + strlen((const char*)_value + 2+255);
}

c4_String c4_String::Mid(int nFirst, int nCount)const {
  if (nFirst >= GetLength())
    return c4_String();

  if (nFirst + nCount > GetLength())
    nCount = GetLength() - nFirst;

  if (nFirst == 0 && nCount == GetLength())
    return  *this;

  return c4_String(Data() + nFirst, nCount);
}

c4_String c4_String::Left(int nCount)const {
  if (nCount >= GetLength())
    return  *this;

  return c4_String(Data(), nCount);
}

c4_String c4_String::Right(int nCount)const {
  if (nCount >= GetLength())
    return  *this;

  return c4_String(Data() + GetLength() - nCount, nCount);
}

bool operator == (const c4_String &a, const c4_String &b) {
  return a._value == b._value || a.GetLength() == b.GetLength() && memcmp
    (a.Data(), b.Data(), a.GetLength()) == 0;
}

int c4_String::Compare(const char *str)const {
  return Data() == str ? 0 : strcmp(Data(), str);
}

int c4_String::CompareNoCase(const char *str)const {
  return Data() == str ? 0 : strcasecmp(Data(), str);
}

int c4_String::Find(char ch)const {
  const char *p = strchr(Data(), ch);
  return p != 0 ? p - Data():  - 1;
}

int c4_String::ReverseFind(char ch)const {
  const char *p = strrchr(Data(), ch);
  return p != 0 ? p - Data():  - 1;
}

int c4_String::FindOneOf(const char *set)const {
  const char *p = strpbrk(Data(), set);
  return p != 0 ? p - Data():  - 1;
}

int c4_String::Find(const char *sub)const {
  const char *p = strstr(Data(), sub);
  return p != 0 ? p - Data():  - 1;
}

c4_String c4_String::SpanIncluding(const char *set)const {
  return Left(strspn(Data(), set));
}

c4_String c4_String::SpanExcluding(const char *set)const {
  return Left(strcspn(Data(), set));
}

/////////////////////////////////////////////////////////////////////////////
#endif // q4_UNIV
