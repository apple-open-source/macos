// univ.inl --
// $Id: univ.inl 1230 2007-03-09 15:58:53Z jcw $
// This is part of Metakit, see http://www.equi4.com/metakit.html

/** @file
 * Inlined members of the container classes
 */

/////////////////////////////////////////////////////////////////////////////
// c4_BaseArray

d4_inline int c4_BaseArray::GetLength() const
{
  return _size;
}

d4_inline const void* c4_BaseArray::GetData(int nIndex) const
{
  return _data + nIndex;
}

d4_inline void* c4_BaseArray::GetData(int nIndex)
{
  return _data + nIndex;
}

/////////////////////////////////////////////////////////////////////////////
// c4_PtrArray

d4_inline c4_PtrArray::c4_PtrArray ()
{ 
}

d4_inline c4_PtrArray::~c4_PtrArray ()
{ 
}

d4_inline int c4_PtrArray::Off(int n_)
{
  return n_ * sizeof (void*); 
}

d4_inline int c4_PtrArray::GetSize() const
{ 
  return _vector.GetLength() / sizeof (void*); 
}

d4_inline void c4_PtrArray::SetSize(int nNewSize, int)
{ 
  _vector.SetLength(Off(nNewSize)); 
}

d4_inline void* c4_PtrArray::GetAt(int nIndex) const
{ 
  return *(void* const*) _vector.GetData(Off(nIndex)); 
}

d4_inline void c4_PtrArray::SetAt(int nIndex, const void* newElement)
{ 
  *(const void**) _vector.GetData(Off(nIndex)) = newElement; 
}

d4_inline void*& c4_PtrArray::ElementAt(int nIndex)
{ 
  return *(void**) _vector.GetData(Off(nIndex)); 
}

/////////////////////////////////////////////////////////////////////////////
// c4_DWordArray

d4_inline c4_DWordArray::c4_DWordArray ()
{ 
}

d4_inline c4_DWordArray::~c4_DWordArray ()
{ 
}

d4_inline int c4_DWordArray::Off(int n_)
{
  return n_ * sizeof (t4_i32); 
}

d4_inline int c4_DWordArray::GetSize() const
{ 
  return _vector.GetLength() / sizeof (t4_i32); 
}

d4_inline void c4_DWordArray::SetSize(int nNewSize, int)
{ 
  _vector.SetLength(Off(nNewSize)); 
}

d4_inline t4_i32 c4_DWordArray::GetAt(int nIndex) const
{ 
  return *(const t4_i32*) _vector.GetData(Off(nIndex)); 
}

d4_inline void c4_DWordArray::SetAt(int nIndex, t4_i32 newElement)
{ 
  *(t4_i32*) _vector.GetData(Off(nIndex)) = newElement; 
}

d4_inline t4_i32& c4_DWordArray::ElementAt(int nIndex)
{ 
  return *(t4_i32*) _vector.GetData(Off(nIndex)); 
}

/////////////////////////////////////////////////////////////////////////////
// c4_StringArray

d4_inline c4_StringArray::c4_StringArray ()
{ 
}

d4_inline int c4_StringArray::GetSize() const
{ 
  return _ptrs.GetSize(); 
}

d4_inline const char* c4_StringArray::GetAt(int nIndex) const
{ 
  return (const char*) _ptrs.GetAt(nIndex); 
}

/////////////////////////////////////////////////////////////////////////////
