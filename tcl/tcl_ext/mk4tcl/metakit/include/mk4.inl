// mk4.inl --
// $Id: mk4.inl 1230 2007-03-09 15:58:53Z jcw $
// This is part of Metakit, the homepage is http://www.equi4.com/metakit.html

/** @file
 * Public definitions which are usually inlined
 */

/////////////////////////////////////////////////////////////////////////////
// Reordered inlines so they are always defined before their first use
 
d4_inline c4_Cursor c4_RowRef::operator& () const
{
  return _cursor;
}

/** Return a unique id for this property
 *
 *  A property object in fact merely represents an entry in a globally
 *  maintained symbol table.  Each property is assigned a unique id,
 *  which remains valid as long as some reference to that property
 *  exists.  In general, property id's remain unique as long as the
 *  application runs.  Do not store id's on file, since they are
 *  not guaranteed to remain the same across program invocations.
 *  All properties with the same name are given the same id.
 */
d4_inline int c4_Property::GetId() const
{
  return _id;
}

//////////////////////////////////////////////////////////////////////////////////

#if !q4_LONG64 && !defined (LONG_LONG) && !HAVE_LONG_LONG

d4_inline bool operator== (const t4_i64 a_, const t4_i64 b_)
{
  return a_.l1 == b_.l1 && a_.l2 == b_.l2;
}

d4_inline bool operator< (const t4_i64 a_, const t4_i64 b_)
{
  return a_.l2 < b_.l2 || a_.l2 == b_.l2 && a_.l2 < b_.l2;
}

#endif

//////////////////////////////////////////////////////////////////////////////////
// c4_View

/// Returns the number of entries in this view.
d4_inline int c4_View::GetSize() const
{
  return _seq->NumRows();
}

/** Change the size of this view
 * Since views act like dynamic arrays, you can quickly
 * change their size.  Increasing the size will append rows
 * with zero/empty values, while decreasing it will delete
 * the last rows.  The growBy_ parameter is currently unused.
 */
d4_inline void c4_View::SetSize(int newSize_, int growBy_)
{
  _seq->Resize(newSize_, growBy_);
}

/// Removes all entries (sets size to zero).
d4_inline void c4_View::RemoveAll()
{
  SetSize(0);
}

/// Return a pointer to the persistence handler, or zero
d4_inline c4_Persist* c4_View::Persist() const
{
  return _seq->Persist();
}

/**
 * Change the value of the specified entry.  If the new value has
 * other properties, these will be added to the underlying view.
 *
 * @param index_ the zero-based row index
 * @param newElem_ the row to copy to this view
 */
d4_inline void c4_View::SetAt(int index_, const c4_RowRef& newElem_)
{
  _seq->SetAt(index_, &newElem_);
}

/**
 * Insert a copy of the contents of another view.  This is identical to
 * inserting the specified number of default entries and then setting
 * each of them to the new element value passed as argument.
 */
d4_inline void c4_View::InsertAt(
	int index_, ///< zero-based row index
	const c4_RowRef& newElem_, ///< the value to insert
	int count_ ///< number of copies to insert, must be > 0
    )
{
  _seq->InsertAt(index_, &newElem_, count_);
}

/**
 * Remove entries starting at the given index.  Entries which have
 * other view references may cause these views to be deleted if their
 * reference counts drop to zero because of this removal.
 *
 * @param index_ the zero-based row index
 * @param count_ the number of entries to remove
 */
d4_inline void c4_View::RemoveAt(int index_, int count_)
{
  _seq->RemoveAt(index_, count_);
}

/** Return the number of properties present in this view.
 * @return A non-negative integer
 */
d4_inline int c4_View::NumProperties() const
{
  return _seq->NumHandlers();
}

/** Find the index of a property, given its id
 * @param propId_ Unique id associated to a specific propoerty
 * @return The index of the property, or -1 of it was not found
 */
d4_inline int c4_View::FindProperty(int propId_)
{
  return _seq->PropIndex(propId_);
}

    /// Return a decription if there is a fixed structure, else zero
d4_inline const char* c4_View::Description() const
{
  return _seq->Description();
}

    /// Increase the reference count of the associated sequence
d4_inline void c4_View::_IncSeqRef()
{
  _seq->IncRef();
}

    /// Decrease the reference count of the associated sequence
d4_inline void c4_View::_DecSeqRef()
{
  _seq->DecRef();
}

/// Destructor, decrements reference count
d4_inline c4_View::~c4_View ()
{
  _DecSeqRef();
}

    /// Return true if the contents of both views are equal
d4_inline bool operator== (const c4_View& a_, const c4_View& b_)
{
  return a_.GetSize() == b_.GetSize() && a_.Compare(b_) == 0;
}

    /// Return true if the contents of both views are not equal
d4_inline bool operator!= (const c4_View& a_, const c4_View& b_)
{
  return !(a_ == b_);
}

    /// True if first view is less than second view
d4_inline bool operator< (const c4_View& a_, const c4_View& b_)
{
  return a_.Compare(b_) < 0;
}

    /// True if first view is greater than second view
d4_inline bool operator> (const c4_View& a_, const c4_View& b_)
{
  return b_ < a_;
}

    /// True if first view is less or equal to second view
d4_inline bool operator<= (const c4_View& a_, const c4_View& b_)
{
  return !(b_ < a_);
}

    /// True if first view is greater or equal to second view
d4_inline bool operator>= (const c4_View& a_, const c4_View& b_)
{                     
  return !(a_ < b_);
}

/////////////////////////////////////////////////////////////////////////////
// c4_Cursor

/** Constructs a new cursor.
 *
 * Cursor cannot be created without an underlying view, but you could
 * define a global "nullView" object and then initialize the cursor with
 * "&nullView[0]". This works because cursors need not point to a valid row.
 */
d4_inline c4_Cursor::c4_Cursor (c4_Sequence& seq_, int index_)
    : _seq (&seq_), _index (index_)
{
}

/// Pre-increments the cursor.
d4_inline c4_Cursor& c4_Cursor::operator++ ()
{
  ++_index;
  return *this;
}

/// Post-increments the cursor.
d4_inline c4_Cursor c4_Cursor::operator++ (int)
{
  return c4_Cursor (*_seq, _index++);
}

/// Pre-decrements the cursor.
d4_inline c4_Cursor& c4_Cursor::operator-- ()
{
  --_index;
  return *this;
}

/// Post-decrements the cursor.
d4_inline c4_Cursor c4_Cursor::operator-- (int)
{
  return c4_Cursor (*_seq, _index--);
}

/// Advances by a given offset.
d4_inline c4_Cursor& c4_Cursor::operator+= (int offset_)
{
  _index += offset_;
  return *this;
}

/// Backs up by a given offset.
d4_inline c4_Cursor& c4_Cursor::operator-= (int offset_)
{
  _index -= offset_;
  return *this;
}

/// Subtracts a specified offset.
d4_inline c4_Cursor c4_Cursor::operator- (int offset_) const
{
  return c4_Cursor (*_seq, _index - offset_);
}

/// Returns the distance between two cursors.
d4_inline int c4_Cursor::operator- (c4_Cursor cursor_) const
{
  return _index - cursor_._index;
}

/// Add a specified offset.
d4_inline c4_Cursor operator+ (c4_Cursor cursor_, int offset_)
{
  return c4_Cursor (*cursor_._seq, cursor_._index + offset_);
}

/// Adds specified offset to cursor.
d4_inline c4_Cursor operator+ (int offset_, c4_Cursor cursor_)
{
  return cursor_ + offset_;
}

d4_inline bool operator== (c4_Cursor a_, c4_Cursor b_)
{
  return a_._seq == b_._seq && a_._index == b_._index;
}

d4_inline bool operator!= (c4_Cursor a_, c4_Cursor b_)
{
  return !(a_ == b_);
}

d4_inline bool operator< (c4_Cursor a_, c4_Cursor b_)
{
  return a_._seq < b_._seq ||
	  a_._seq == b_._seq && a_._index < b_._index;
}

d4_inline bool operator> (c4_Cursor a_, c4_Cursor b_)
{
  return b_ < a_;
}

d4_inline bool operator<= (c4_Cursor a_, c4_Cursor b_)
{
  return !(b_ < a_);
}

d4_inline bool operator>= (c4_Cursor a_, c4_Cursor b_)
{                     
  return !(a_ < b_);
}

/////////////////////////////////////////////////////////////////////////////
// c4_RowRef

d4_inline c4_RowRef::c4_RowRef (c4_Cursor cursor_)
    : _cursor (cursor_)
{
}

d4_inline c4_RowRef c4_RowRef::operator= (const c4_RowRef& rowRef_)
{
  if (_cursor != rowRef_._cursor)
    _cursor._seq->SetAt(_cursor._index, &rowRef_);
  
  return *this;
}

d4_inline c4_View c4_RowRef::Container() const
{
  return _cursor._seq;
}

d4_inline bool operator== (const c4_RowRef& a_, const c4_RowRef& b_)
{
  return (&a_)._seq->Compare((&a_)._index, &b_) == 0;
}               

d4_inline bool operator!= (const c4_RowRef& a_, const c4_RowRef& b_)
{
  return !(a_ == b_);
}

d4_inline bool operator< (const c4_RowRef& a_, const c4_RowRef& b_)
{
      // 25-5-1998: don't exchange a and b, this comparison is -not- symmetric
  return (&a_)._seq->Compare((&a_)._index, &b_) < 0;
}               

d4_inline bool operator> (const c4_RowRef& a_, const c4_RowRef& b_)
{
      // 25-5-1998: don't exchange a and b, this comparison is -not- symmetric
  return (&a_)._seq->Compare((&a_)._index, &b_) > 0;
}

d4_inline bool operator<= (const c4_RowRef& a_, const c4_RowRef& b_)
{
  return !(a_ > b_);
}

d4_inline bool operator>= (const c4_RowRef& a_, const c4_RowRef& b_)
{                     
  return !(a_ < b_);
}

/////////////////////////////////////////////////////////////////////////////
// c4_Bytes

    /// Construct an empty binary object
d4_inline c4_Bytes::c4_Bytes ()
    : _size (0), _copy (false)
{ 
  _contents = 0; // moved out of intializers for DEC CXX 5.7
}

    /// Construct an object with contents, no copy
d4_inline c4_Bytes::c4_Bytes (const void* buf_, int len_)
    : _size (len_), _copy (false)
{
  _contents = (t4_byte*) buf_; // moved out of intializers for DEC CXX 5.7
}

/// Returns a pointer to the contents.
d4_inline const t4_byte* c4_Bytes::Contents() const
{
  return _contents;
}

/// Returns the number of bytes of its contents.
d4_inline int c4_Bytes::Size() const
{
  return _size;
}

d4_inline void c4_Bytes::_LoseCopy()
{
  if (_copy)
    delete [] (char*) _contents;
}

/// Returns true if the contents of both objects is not equal.
d4_inline bool operator!= (const c4_Bytes& a_, const c4_Bytes& b_)
{
  return !(a_ == b_);
}

/// Destructor, if a copy was made, it will be released here.
d4_inline c4_Bytes::~c4_Bytes ()
{
  _LoseCopy();
}
    
/////////////////////////////////////////////////////////////////////////////
// c4_Reference

d4_inline c4_Reference::c4_Reference (const c4_RowRef& rowRef_,
                                        const c4_Property& prop_)
    : _cursor (&rowRef_), _property (prop_)
{
}

d4_inline int c4_Reference::GetSize() const
{
  return _cursor._seq->ItemSize(_cursor._index, _property.GetId());
}

d4_inline bool c4_Reference::GetData(c4_Bytes& buf_) const
{
  return _cursor._seq->Get(_cursor._index, _property.GetId(), buf_);
}

d4_inline void c4_Reference::SetData(const c4_Bytes& buf_) const
{
  _cursor._seq->Set(_cursor._index, _property, buf_);
}

d4_inline bool operator!= (const c4_Reference& a_, const c4_Reference& b_)
{
  return !(a_ == b_);
}

/////////////////////////////////////////////////////////////////////////////
// c4_IntRef

d4_inline c4_IntRef::c4_IntRef (const c4_Reference& value_)
    : c4_Reference (value_)
{
}

/////////////////////////////////////////////////////////////////////////////
#if !q4_TINY
/////////////////////////////////////////////////////////////////////////////
// c4_LongRef

d4_inline c4_LongRef::c4_LongRef (const c4_Reference& value_)
    : c4_Reference (value_)
{
}

/////////////////////////////////////////////////////////////////////////////
// c4_FloatRef

d4_inline c4_FloatRef::c4_FloatRef (const c4_Reference& value_)
    : c4_Reference (value_)
{
}

/////////////////////////////////////////////////////////////////////////////
// c4_DoubleRef

d4_inline c4_DoubleRef::c4_DoubleRef (const c4_Reference& value_)
    : c4_Reference (value_)
{
}

/////////////////////////////////////////////////////////////////////////////
#endif // !q4_TINY
/////////////////////////////////////////////////////////////////////////////
// c4_BytesRef

d4_inline c4_BytesRef::c4_BytesRef (const c4_Reference& value_)
    : c4_Reference (value_)
{
}

/////////////////////////////////////////////////////////////////////////////
// c4_StringRef

d4_inline c4_StringRef::c4_StringRef (const c4_Reference& value_)
    : c4_Reference (value_)
{
}

/////////////////////////////////////////////////////////////////////////////
// c4_ViewRef

d4_inline c4_ViewRef::c4_ViewRef (const c4_Reference& value_)
    : c4_Reference (value_)
{
}

/////////////////////////////////////////////////////////////////////////////
// c4_Property

d4_inline c4_Property::c4_Property (char type_, int id_)
    : _id ((short) id_), _type (type_)
{
}

    /// Get or set this untyped property in a row
d4_inline c4_Reference c4_Property::operator() (const c4_RowRef& rowRef_) const
{
  return c4_Reference (rowRef_, *this);
}

    /// Return a view like the first, with a property appended to it
d4_inline c4_View c4_Property::operator, (const c4_Property& prop_) const
{
  return c4_View (*this), prop_;
}

    /// Return the type of this property
d4_inline char c4_Property::Type() const
{
  return _type;
}

/////////////////////////////////////////////////////////////////////////////
// c4_IntProp

d4_inline c4_IntProp::c4_IntProp (const char* name_) 
    : c4_Property ('I', name_)
{
}

d4_inline c4_IntProp::~c4_IntProp ()
{
}

d4_inline c4_IntRef c4_IntProp::operator() (const c4_RowRef& rowRef_) const
{
  return c4_Reference (rowRef_, *this);
}

d4_inline t4_i32 c4_IntProp::Get(const c4_RowRef& rowRef_) const
{
  return operator() (rowRef_);
}

d4_inline void c4_IntProp::Set(const c4_RowRef& rowRef_, t4_i32 value_) const
{
  operator() (rowRef_) = value_;
}

d4_inline c4_Row c4_IntProp::AsRow(t4_i32 value_) const
{
  c4_Row row;
  operator() (row) = value_;
  return row;
}
    
d4_inline c4_Row c4_IntProp::operator[] (t4_i32 value_) const
{
  return AsRow(value_);
}
    
/////////////////////////////////////////////////////////////////////////////
#if !q4_TINY
/////////////////////////////////////////////////////////////////////////////
// c4_LongProp

d4_inline c4_LongProp::c4_LongProp (const char* name_) 
    : c4_Property ('L', name_)
{
}

d4_inline c4_LongProp::~c4_LongProp ()
{
}

d4_inline c4_LongRef c4_LongProp::operator() (const c4_RowRef& rowRef_) const
{
  return c4_Reference (rowRef_, *this);
}

d4_inline t4_i64 c4_LongProp::Get(const c4_RowRef& rowRef_) const
{
  return operator() (rowRef_);
}

d4_inline void c4_LongProp::Set(const c4_RowRef& rowRef_, t4_i64 value_) const
{
  operator() (rowRef_) = value_;
}

d4_inline c4_Row c4_LongProp::AsRow(t4_i64 value_) const
{
  c4_Row row;
  operator() (row) = value_;
  return row;
}
    
d4_inline c4_Row c4_LongProp::operator[] (t4_i64 value_) const
{
  return AsRow(value_);
}
    
/////////////////////////////////////////////////////////////////////////////
// c4_FloatProp

d4_inline c4_FloatProp::c4_FloatProp (const char* name_) 
    : c4_Property ('F', name_)
{
}

d4_inline c4_FloatProp::~c4_FloatProp ()
{
}

d4_inline c4_FloatRef c4_FloatProp::operator() (const c4_RowRef& rowRef_) const
{
  return c4_Reference (rowRef_, *this);
}

d4_inline double c4_FloatProp::Get(const c4_RowRef& rowRef_) const
{
  return operator() (rowRef_);
}

d4_inline void c4_FloatProp::Set(const c4_RowRef& rowRef_, double value_) const
{
  operator() (rowRef_) = value_;
}

d4_inline c4_Row c4_FloatProp::AsRow(double value_) const
{
  c4_Row row;
  operator() (row) = value_;
  return row;
}
    
d4_inline c4_Row c4_FloatProp::operator[] (double value_) const
{
  return AsRow(value_);
}
    
/////////////////////////////////////////////////////////////////////////////
// c4_DoubleProp

d4_inline c4_DoubleProp::c4_DoubleProp (const char* name_) 
    : c4_Property ('D', name_)
{
}

d4_inline c4_DoubleProp::~c4_DoubleProp ()
{
}

d4_inline c4_DoubleRef c4_DoubleProp::operator() (const c4_RowRef& rowRef_) const
{
  return c4_Reference (rowRef_, *this);
}

d4_inline double c4_DoubleProp::Get(const c4_RowRef& rowRef_) const
{
  return operator() (rowRef_);
}

d4_inline void c4_DoubleProp::Set(const c4_RowRef& rowRef_, double value_) const
{
  operator() (rowRef_) = value_;
}

d4_inline c4_Row c4_DoubleProp::AsRow(double value_) const
{
  c4_Row row;
  operator() (row) = value_;
  return row;
}
    
d4_inline c4_Row c4_DoubleProp::operator[] (double value_) const
{
  return AsRow(value_);
}
    
/////////////////////////////////////////////////////////////////////////////
#endif // !q4_TINY
/////////////////////////////////////////////////////////////////////////////
// c4_BytesProp

d4_inline c4_BytesProp::c4_BytesProp (const char* name_) 
    : c4_Property ('B', name_)
{
}
    
d4_inline c4_BytesProp::~c4_BytesProp ()
{
}

d4_inline c4_BytesRef c4_BytesProp::operator() (const c4_RowRef& rowRef_) const
{
  return c4_Reference (rowRef_, *this);
}

d4_inline c4_Bytes c4_BytesProp::Get(const c4_RowRef& rowRef_) const
{
  return operator() (rowRef_);
}

d4_inline void c4_BytesProp::Set(const c4_RowRef& rowRef_, const c4_Bytes& value_) const
{
  operator() (rowRef_) = value_;
}

d4_inline c4_Row c4_BytesProp::AsRow(const c4_Bytes& value_) const
{
  c4_Row row;
  operator() (row) = value_;
  return row;
}
    
d4_inline c4_Row c4_BytesProp::operator[] (const c4_Bytes& value_) const
{
  return AsRow(value_);
}
    
/////////////////////////////////////////////////////////////////////////////
// c4_StringProp

d4_inline c4_StringProp::c4_StringProp (const char* name_) 
    : c4_Property ('S', name_)
{
}
    
d4_inline c4_StringProp::~c4_StringProp ()
{
}

d4_inline c4_StringRef c4_StringProp::operator() (const c4_RowRef& rowRef_) const
{
  return c4_Reference (rowRef_, *this);
}

d4_inline const char* c4_StringProp::Get(const c4_RowRef& rowRef_) const
{
  return operator() (rowRef_);
}

d4_inline void c4_StringProp::Set(const c4_RowRef& rowRef_, const char* value_) const
{
  operator() (rowRef_) = value_;
}

d4_inline c4_Row c4_StringProp::AsRow(const char* value_) const
{
  c4_Row row;
  operator() (row) = value_;
  return row;
}
    
d4_inline c4_Row c4_StringProp::operator[] (const char* value_) const
{
  return AsRow(value_);
}
    
/////////////////////////////////////////////////////////////////////////////
// c4_ViewProp

d4_inline c4_ViewProp::c4_ViewProp (const char* name_)
    : c4_Property ('V', name_)
{
}
    
d4_inline c4_ViewProp::~c4_ViewProp ()
{
}

d4_inline c4_ViewRef c4_ViewProp::operator() (const c4_RowRef& rowRef_) const
{
  return c4_Reference (rowRef_, *this);
}

d4_inline c4_View c4_ViewProp::Get(const c4_RowRef& rowRef_) const
{
  return operator() (rowRef_);
}

d4_inline void c4_ViewProp::Set(const c4_RowRef& rowRef_, const c4_View& value_) const
{
  operator() (rowRef_) = value_;
}

d4_inline c4_Row c4_ViewProp::AsRow(const c4_View& value_) const
{
  c4_Row row;
  operator() (row) = value_;
  return row;
}
    
d4_inline c4_Row c4_ViewProp::operator[] (const c4_View& value_) const
{
  return AsRow(value_);
}
    
/////////////////////////////////////////////////////////////////////////////
// c4_Strategy

    /// True if we can do I/O with this object
d4_inline bool c4_Strategy::IsValid() const
{ 
  return false; 
}

/////////////////////////////////////////////////////////////////////////////
// c4_CustomViewer

d4_inline c4_CustomViewer::c4_CustomViewer()
{
}

d4_inline int c4_CustomViewer::Lookup(const c4_RowRef& r_, int& n_)
{
  return Lookup(&r_, n_); // c4_Cursor
}

d4_inline bool c4_CustomViewer::InsertRows(int p_, const c4_RowRef& r_, int n_)
{
  return InsertRows(p_, &r_, n_); // c4_Cursor
}

/////////////////////////////////////////////////////////////////////////////
// c4_Sequence

d4_inline c4_Dependencies* c4_Sequence::GetDependencies() const
{
  return _dependencies;
}

/////////////////////////////////////////////////////////////////////////////
// Reordered inlines so they are always used after their definition
 
/// Dereferences this cursor to "almost" a row.
d4_inline c4_RowRef c4_Cursor::operator* () const
{
  return *(c4_Cursor*) this; // cast avoids a const problem with BCPP 4.52
}

/// This is the same as *(cursor + offset).
d4_inline c4_RowRef c4_Cursor::operator[] (int offset_) const
{
  return *(*this + offset_);
}

/// Returns a reference to specified entry, for use as RHS or LHS
d4_inline c4_RowRef c4_View::GetAt(int index_) const
{
  return * c4_Cursor (*_seq, index_);
}

/** Element access, shorthand for GetAt
 * @return A reference to the specified row in the view.
 * This reference can be used on either side of the assignment operator.
 */
d4_inline c4_RowRef c4_View::operator[] (
	int index_ ///< zero-based row index
    ) const
{
  return GetAt(index_);
}
    
/** Element access, shorthand for GetAt
 * @return A reference to the specified row in the view.
 * This reference can be used on either side of the assignment operator.
 */
d4_inline c4_RowRef c4_View::ElementAt(
	int index_ ///< zero-based row index
    )
{
  return GetAt(index_);
}

/////////////////////////////////////////////////////////////////////////////
