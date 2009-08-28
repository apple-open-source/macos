// std.cpp --
// $Id: std.cpp 1230 2007-03-09 15:58:53Z jcw $
// This is part of Metakit, the homepage is http://www.equi4.com/metakit.html

/** @file
 * Implementation of STL-based strings and containers
 */

#include "header.h"

#if q4_STD // until end of source
/////////////////////////////////////////////////////////////////////////////

#include "column.h"   // c4_ColCache

#if !q4_INLINE
static char _mk4stdInl[] = "mk4str.inl";
#include "mk4str.inl"
#endif 

#if !q4_NO_NS
using namespace std;
#endif 

/////////////////////////////////////////////////////////////////////////////
// Implemented in this file

class c4_String;

/////////////////////////////////////////////////////////////////////////////

#if !q4_MSVC && !q4_WATC

// MS C/C++ has this handy stricmp: a case-insensitive version of strcmp
// This version only works with 7-bit ASCII characters 0x00 through 0x7F

static int stricmp(const char *p1, const char *p2) {
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

#endif 

/////////////////////////////////////////////////////////////////////////////
// c4_String

c4_String c4_String::Mid(int nFirst_, int nCount_)const {
  int n = length();
  if (nFirst_ > n)
    nFirst_ = n;
  if (nFirst_ + nCount_ > n)
    nCount_ = n - nFirst_;

  return substr(nFirst_, nCount_);
}

int c4_String::CompareNoCase(const char *str_)const {
  // this is not very "standard library-ish" ...
  return *(const string*)this == str_ ? 0 : stricmp(c_str(), str_);
}

/////////////////////////////////////////////////////////////////////////////
#endif // q4_STD
