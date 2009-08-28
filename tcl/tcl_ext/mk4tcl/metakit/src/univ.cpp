// univ.cpp --
// $Id: univ.cpp 1230 2007-03-09 15:58:53Z jcw $
// This is part of Metakit, see http://www.equi4.com/metakit.html

/** @file
 * A simple implementation of dynamic arrays
 */

#include "header.h"

#if q4_UNIV // until end of source
/////////////////////////////////////////////////////////////////////////////

#include <stdlib.h>   // malloc

#if !q4_INLINE
#include "univ.inl"
#endif 

/////////////////////////////////////////////////////////////////////////////

#if q4_UNIX || __MINGW32__
#define _strdup strdup
#elif !q4_BORC && !q4_MSVC && !q4_WATC && !(q4_MWCW && defined(_WIN32)) && \
!(q4_MWCW && __MWERKS__ >= 0x3000)

static char *_strdup(const char *p) {
  if (!p)
    return 0;

  char *s = (char*)malloc(strlen(p) + 1);
  return strcpy(s, p);
}

#endif 

//  The Borland C++ RTL does not want file handle objects to cross
//  DLL boundaries, so we add special fopen/fclose hooks to this DLL.

#if q4_BORC
#include <stdio.h>

#if q4_WIN32
__declspec(dllexport)FILE *
#else 
FILE *__export 
#endif 
f4_FileOpenInDLL(const char *name_, const char *mode_) {
  return fopen(name_, mode_);
}

#if q4_WIN32
__declspec(dllexport)
#else 
int __export 
#endif 
f4_FileCloseInDLL(FILE *file_) {
  return fclose(file_);
}

#endif 

/////////////////////////////////////////////////////////////////////////////
// c4_BaseArray

c4_BaseArray::c4_BaseArray(): _data(0), _size(0){}

c4_BaseArray::~c4_BaseArray() {
  SetLength(0);
}

void c4_BaseArray::SetLength(int nNewSize) {
  // 2001-11-25: use more granular allocation, as optimization
  const int bits = 6;

  if (((_size - 1) ^ (nNewSize - 1)) >> bits) {
    const int n = (nNewSize + (1 << bits) - 1) & - (1 << bits);
    _data = _data == 0 ? n == 0 ? (char*)0: (char*)malloc(n): n == 0 ? (free
      (_data), (char*)0): (char*)realloc(_data, n);
  }

  d4_assert(_data != 0 || nNewSize == 0);

  int n = _size;
  _size = nNewSize;

  if (nNewSize > n)
    memset(GetData(n), 0, nNewSize - n);
}

void c4_BaseArray::Grow(int nIndex) {
  if (nIndex > _size)
    SetLength(nIndex);
}

void c4_BaseArray::InsertAt(int nIndex, int nCount) {
  SetLength(_size + nCount);

  int to = nIndex + nCount;
  if (_size > to)
    d4_memmove(GetData(to), GetData(nIndex), _size - to);
}

void c4_BaseArray::RemoveAt(int nIndex, int nCount) {
  int from = nIndex + nCount;
  if (_size > from)
    d4_memmove(GetData(nIndex), GetData(from), _size - from);

  SetLength(_size - nCount);
}

/////////////////////////////////////////////////////////////////////////////
// c4_DWordArray

int c4_DWordArray::Add(t4_i32 newElement) {
  int n = GetSize();
  _vector.Grow(Off(n + 1));
  SetAt(n, newElement);
  return n;
}

void c4_DWordArray::InsertAt(int nIndex, t4_i32 newElement, int nCount) {
  _vector.InsertAt(Off(nIndex), nCount *sizeof(t4_i32));

  while (--nCount >= 0)
    SetAt(nIndex++, newElement);
}

void c4_DWordArray::RemoveAt(int nIndex, int nCount) {
  _vector.RemoveAt(Off(nIndex), nCount *sizeof(t4_i32));
}

/////////////////////////////////////////////////////////////////////////////
// c4_PtrArray

int c4_PtrArray::Add(void *newElement) {
  int n = GetSize();
  _vector.Grow(Off(n + 1));
  SetAt(n, newElement);
  return n;
}

void c4_PtrArray::InsertAt(int nIndex, void *newElement, int nCount) {
  _vector.InsertAt(Off(nIndex), nCount *sizeof(void*));

  while (--nCount >= 0)
    SetAt(nIndex++, newElement);
}

void c4_PtrArray::RemoveAt(int nIndex, int nCount) {
  _vector.RemoveAt(Off(nIndex), nCount *sizeof(void*));
}

/////////////////////////////////////////////////////////////////////////////
// c4_StringArray

c4_StringArray::~c4_StringArray() {
  SetSize(0);
}

void c4_StringArray::SetSize(int nNewSize, int) {
  int i = nNewSize;

  while (i < GetSize())
    SetAt(i++, 0);

  _ptrs.SetSize(nNewSize);

  while (i < GetSize())
    _ptrs.SetAt(i++, "");
}

void c4_StringArray::SetAt(int nIndex, const char *newElement) {
  char *s = (char*)_ptrs.GetAt(nIndex);
  if (s &&  *s)
    free(s);

  _ptrs.SetAt(nIndex, newElement &&  *newElement ? _strdup(newElement): "");
}

int c4_StringArray::Add(const char *newElement) {
  int n = _ptrs.Add(0);
  SetAt(n, newElement);
  return n;
}

void c4_StringArray::InsertAt(int nIndex, const char *newElement, int nCount) {
  _ptrs.InsertAt(nIndex, 0, nCount);

  while (--nCount >= 0)
    SetAt(nIndex++, newElement);
}

void c4_StringArray::RemoveAt(int nIndex, int nCount) {
  for (int i = 0; i < nCount; ++i)
    SetAt(nIndex + i, 0);

  _ptrs.RemoveAt(nIndex, nCount);
}

/////////////////////////////////////////////////////////////////////////////
#endif // q4_UNIV
