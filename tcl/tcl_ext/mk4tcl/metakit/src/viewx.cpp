// viewx.cpp --
// $Id: viewx.cpp 1230 2007-03-09 15:58:53Z jcw $
// This is part of Metakit, see http://www.equi4.com/metakit.html

/** @file
 * Implements c4_Sequence, c4_Reference, and c4_...Ref
 */

#include "header.h"
#include "handler.h"
#include "store.h"
#include "column.h"

/////////////////////////////////////////////////////////////////////////////

c4_Sequence::c4_Sequence(): _refCount(0), _dependencies(0), _propertyLimit(0),
  _tempBuf(0){}

c4_Sequence::~c4_Sequence() {
  d4_assert(_refCount == 0);

  d4_assert(!_dependencies); // there can be no dependencies left

  ClearCache();

  delete _tempBuf;
}

c4_Persist *c4_Sequence::Persist()const {
  return 0;
}

/// Increment the reference count of this sequence
void c4_Sequence::IncRef() {
  ++_refCount;

  d4_assert(_refCount != 0);
}

/// Decrement the reference count, delete objects when last
void c4_Sequence::DecRef() {
  d4_assert(_refCount != 0);

  if (--_refCount == 0)
    delete this;
}

/// Return the current reference count
int c4_Sequence::NumRefs()const {
  return _refCount;
}

/// Compare the specified row with another one
int c4_Sequence::Compare(int index_, c4_Cursor cursor_)const {
  d4_assert(cursor_._seq != 0);

  c4_Bytes data;

  for (int colNum = 0; colNum < NumHandlers(); ++colNum) {
    c4_Handler &h = NthHandler(colNum);

    const c4_Sequence *hc = HandlerContext(colNum);
    int i = RemapIndex(index_, hc);

    if (!cursor_._seq->Get(cursor_._index, h.PropId(), data))
      h.ClearBytes(data);

    int f = h.Compare(i, data);
    if (f != 0)
      return f;
  }

  return 0;
}

/// Restrict the search range for rows
bool c4_Sequence::RestrictSearch(c4_Cursor, int &, int &) {
  return true;
}

/// Replace the contents of a specified row
void c4_Sequence::SetAt(int index_, c4_Cursor newElem_) {
  d4_assert(newElem_._seq != 0);

  c4_Bytes data;

  c4_Notifier change(this);
  if (GetDependencies())
    change.StartSetAt(index_, newElem_);

  for (int i = 0; i < newElem_._seq->NumHandlers(); ++i) {
    c4_Handler &h = newElem_._seq->NthHandler(i);

    // added 06-12-1999 to do index remapping for derived seq's
    const c4_Sequence *hc = newElem_._seq->HandlerContext(i);
    int ri = newElem_._seq->RemapIndex(newElem_._index, hc);

    h.GetBytes(ri, data);

    //    Set(index_, cursor._seq->NthProperty(i), data);
    int colNum = PropIndex(h.Property());
    d4_assert(colNum >= 0);

    NthHandler(colNum).Set(index_, data);
  }

  // if number of props in dest is larger after adding, clear the rest
  // this way, new props get copied and undefined props get cleared
  if (newElem_._seq->NumHandlers() < NumHandlers()) {
    for (int j = 0; j < NumHandlers(); ++j) {
      c4_Handler &h = NthHandler(j);

      // if the property does not appear in the source
      if (newElem_._seq->PropIndex(h.PropId()) < 0) {
        h.ClearBytes(data);
        h.Set(index_, data);
      }
    }
  }
}

/// Remap the index to an underlying view
int c4_Sequence::RemapIndex(int index_, const c4_Sequence *seq_)const {
  return seq_ == this ? index_ :  - 1;
}

/// Gives access to a general purpose temporary buffer
c4_Bytes &c4_Sequence::Buffer() {
  if (_tempBuf == 0)
    _tempBuf = d4_new c4_Bytes;
  return  *_tempBuf;
}

// 1.8.5: extra buffer to hold returned description strings
const char *c4_Sequence::UseTempBuffer(const char *str_) {
  return strcpy((char*)Buffer().SetBuffer(strlen(str_) + 1), str_);
}

/// Change number of rows, either by inserting or removing them
void c4_Sequence::Resize(int newSize_, int) {
  if (NumHandlers() > 0) {
    int diff = newSize_ - NumRows();

    if (diff > 0) {
      c4_Row empty; // make sure this doesn't recurse, see below
      InsertAt(NumRows(), &empty, diff);
    } else if (diff < 0)
      RemoveAt(newSize_,  - diff);
  } else
  // need special case to avoid recursion for c4_Row allocations
    SetNumRows(newSize_);
}

/// Insert one or more rows into this sequence
void c4_Sequence::InsertAt(int index_, c4_Cursor newElem_, int count_) {
  d4_assert(newElem_._seq != 0);

  c4_Notifier change(this);
  if (GetDependencies())
    change.StartInsertAt(index_, newElem_, count_);

  SetNumRows(NumRows() + count_);

  c4_Bytes data;

  for (int i = 0; i < newElem_._seq->NumHandlers(); ++i) {
    c4_Handler &h = newElem_._seq->NthHandler(i);

    // added 06-12-1999 to do index remapping for derived seq's
    const c4_Sequence *hc = newElem_._seq->HandlerContext(i);
    int ri = newElem_._seq->RemapIndex(newElem_._index, hc);

    int colNum = PropIndex(h.Property());
    d4_assert(colNum >= 0);

    if (h.Property().Type() == 'V') {
      // If inserting from self: Make sure we get a copy of the bytes,
      // so we don't get an invalid pointer if the memory get realloc'ed
      h.GetBytes(ri, data, newElem_._seq == this);

      // special treatment for subviews, insert empty, then overwrite
      // changed 19990904 - probably fixes a long-standing limitation
      c4_Bytes temp;
      h.ClearBytes(temp);

      c4_Handler &h2 = NthHandler(colNum);
      h2.Insert(index_, temp, count_);

      for (int j = 0; j < count_; ++j)
        h2.Set(index_ + j, data);
    } else {
      h.GetBytes(ri, data);
      NthHandler(colNum).Insert(index_, data, count_);
    }
  }

  // if number of props in dest is larger after adding, clear the rest
  // this way, new props get copied and undefined props get cleared
  if (newElem_._seq->NumHandlers() < NumHandlers()) {
    for (int j = 0; j < NumHandlers(); ++j) {
      c4_Handler &h = NthHandler(j);

      // if the property does not appear in the source
      if (newElem_._seq->PropIndex(h.PropId()) < 0) {
        h.ClearBytes(data);
        h.Insert(index_, data, count_);
      }
    }
  }
}

/// Remove one or more rows from this sequence
void c4_Sequence::RemoveAt(int index_, int count_) {
  c4_Notifier change(this);
  if (GetDependencies())
    change.StartRemoveAt(index_, count_);

  SetNumRows(NumRows() - count_);

  //! careful, this does no index remapping, wrong for derived seq's
  for (int i = 0; i < NumHandlers(); ++i)
    NthHandler(i).Remove(index_, count_);
}

/// Move a row to another position
void c4_Sequence::Move(int from_, int to_) {
  c4_Notifier change(this);
  if (GetDependencies())
    change.StartMove(from_, to_);

  //! careful, this does no index remapping, wrong for derived seq's
  for (int i = 0; i < NumHandlers(); ++i)
    NthHandler(i).Move(from_, to_);
}

/// Return the id of the N-th property
int c4_Sequence::NthPropId(int index_)const {
  return NthHandler(index_).PropId();
}

void c4_Sequence::ClearCache() {
  if (_propertyLimit > 0) {
    delete [] _propertyMap; // property indexes may change
    _propertyLimit = 0;
  }
}

/// Find the index of a property by its id
int c4_Sequence::PropIndex(int propId_) {
  //! CACHING NOTE: derived views will fail if underlying view is restructured
  //          still, this cache is kept, since sort will fail anyway...
  //  The only safe change in these cases is adding new properties at the end.

  // use the map for the fastest result once known
  if (propId_ < _propertyLimit && _propertyMap[propId_] >= 0)
    return _propertyMap[propId_];

  // locate the property using a linear search, return if not present
  int n = NumHandlers();
  do {
    if (--n < 0)
      return  - 1;
  } while (NthPropId(n) != propId_);

  // if the map is too small, resize it (with a little slack)
  if (propId_ >= _propertyLimit) {
    int round = (propId_ + 8) &~0x07;
    short *vec = d4_new short[round];

    for (int i = 0; i < round; ++i)
      vec[i] = i < _propertyLimit ? _propertyMap[i]:  - 1;

    if (_propertyLimit > 0)
      delete [] _propertyMap;

    _propertyMap = vec;
    _propertyLimit = round;
  }

  // we have a map, adjust the entry and return
  return _propertyMap[propId_] = n;
}

/// Find the index of a property, or create a new entry
int c4_Sequence::PropIndex(const c4_Property &prop_) {
  int pos = PropIndex(prop_.GetId());
  if (pos >= 0) {
    d4_assert(NthHandler(pos).Property().Type() == prop_.Type());
    return pos;
  }

  c4_Handler *h = CreateHandler(prop_);
  d4_assert(h != 0);

  int i = AddHandler(h);
  if (i >= 0 && NumRows() > 0) {
    c4_Bytes data;
    h->ClearBytes(data);
    h->Insert(0, data, NumRows());
  }

  return i;
}

const char *c4_Sequence::Description() {
  return 0;
}

int c4_Sequence::ItemSize(int index_, int propId_) {
  int colNum = PropIndex(propId_);
  return colNum >= 0 ? NthHandler(colNum).ItemSize(index_):  - 1;
}

bool c4_Sequence::Get(int index_, int propId_, c4_Bytes &buf_) {
  int colNum = PropIndex(propId_);
  if (colNum < 0)
    return false;

  NthHandler(colNum).GetBytes(index_, buf_);
  return true;
}

void c4_Sequence::Set(int index_, const c4_Property &prop_, const c4_Bytes
  &buf_) {
  int colNum = PropIndex(prop_);
  d4_assert(colNum >= 0);

  c4_Handler &h = NthHandler(colNum);

  c4_Notifier change(this);
  if (GetDependencies())
    change.StartSet(index_, prop_.GetId(), buf_);

  if (buf_.Size())
    h.Set(index_, buf_);
  else {
    c4_Bytes empty;
    h.ClearBytes(empty);
    h.Set(index_, empty);
  }
}

/// Register a sequence to receive change notifications
void c4_Sequence::Attach(c4_Sequence *child_) {
  IncRef();

  if (!_dependencies)
    _dependencies = d4_new c4_Dependencies;

  _dependencies->Add(child_);
}

/// Unregister a sequence which received change notifications
void c4_Sequence::Detach(c4_Sequence *child_) {
  d4_assert(_dependencies != 0);

  if (!_dependencies->Remove(child_)) {
    delete _dependencies;
    _dependencies = 0;
  }

  DecRef();
}

/// Called just before a change is made to the sequence
c4_Notifier *c4_Sequence::PreChange(c4_Notifier &) {
  d4_assert(0); // should not be called, because it should not attach
  return 0;
}

/// Called after changes have been made to the sequence
void c4_Sequence::PostChange(c4_Notifier &){}

/////////////////////////////////////////////////////////////////////////////

c4_Reference &c4_Reference::operator = (const c4_Reference &value_) {
  c4_Bytes result;
  value_.GetData(result);
  SetData(result);

  return  *this;
}

bool operator == (const c4_Reference &a_, const c4_Reference &b_) {
  c4_Bytes buf1;
  bool f1 = a_.GetData(buf1);

  c4_Bytes buf2;
  bool f2 = b_.GetData(buf2);

  // if absent, fill either with zero bytes to match length
  if (!f1)
    buf1.SetBufferClear(buf2.Size());
  if (!f2)
    buf2.SetBufferClear(buf1.Size());

  return buf1 == buf2;
}

/////////////////////////////////////////////////////////////////////////////

c4_IntRef::operator t4_i32()const {
  c4_Bytes result;
  if (!GetData(result))
    return 0;

  d4_assert(result.Size() == sizeof(t4_i32));
  return *(const t4_i32*)result.Contents();
}

c4_IntRef &c4_IntRef::operator = (t4_i32 value_) {
  SetData(c4_Bytes(&value_, sizeof value_));
  return  *this;
}

/////////////////////////////////////////////////////////////////////////////
#if !q4_TINY
/////////////////////////////////////////////////////////////////////////////

c4_LongRef::operator t4_i64()const {
  c4_Bytes result;
  if (!GetData(result)) {
    static t4_i64 zero;
    return zero;
  }

  d4_assert(result.Size() == sizeof(t4_i64));
  return *(const t4_i64*)result.Contents();
}

c4_LongRef &c4_LongRef::operator = (t4_i64 value_) {
  SetData(c4_Bytes(&value_, sizeof value_));
  return  *this;
}

/////////////////////////////////////////////////////////////////////////////

c4_FloatRef::operator double()const {
  c4_Bytes result;
  if (!GetData(result))
    return 0;

  d4_assert(result.Size() == sizeof(float));
  return *(const float*)result.Contents();
}

c4_FloatRef &c4_FloatRef::operator = (double value_) {
  float v = (float)value_; // loses precision
  SetData(c4_Bytes(&v, sizeof v));
  return  *this;
}

/////////////////////////////////////////////////////////////////////////////

c4_DoubleRef::operator double()const {
  c4_Bytes result;
  if (!GetData(result))
    return 0;

  d4_assert(result.Size() == sizeof(double));
  return *(const double*)result.Contents();
}

c4_DoubleRef &c4_DoubleRef::operator = (double value_) {
  SetData(c4_Bytes(&value_, sizeof value_));
  return  *this;
}

/////////////////////////////////////////////////////////////////////////////
#endif // !q4_TINY
/////////////////////////////////////////////////////////////////////////////

c4_BytesRef::operator c4_Bytes()const {
  c4_Bytes result;
  GetData(result);

  // the result must immediately be used, its lifetime may be limited
  return result;
}

c4_BytesRef &c4_BytesRef::operator = (const c4_Bytes &value_) {
  SetData(value_);
  return  *this;
}

c4_Bytes c4_BytesRef::Access(t4_i32 off_, int len_, bool noCopy_)const {
  c4_Bytes &buffer = _cursor._seq->Buffer();

  int colNum = _cursor._seq->PropIndex(_property.GetId());
  if (colNum >= 0) {
    c4_Handler &h = _cursor._seq->NthHandler(colNum);
    int sz = h.ItemSize(_cursor._index);
    if (len_ == 0 || off_ + len_ > sz)
      len_ = sz - off_;

    if (len_ > 0) {
      c4_Column *col = h.GetNthMemoCol(_cursor._index, true);
      if (col != 0) {
        if (noCopy_) {
          // 21-11-2005 optimization by A. Stigsen
          // return just the first segment (even if it is smaller than
          // len). this avoids any expensive memcopies, but you have to
          // remember to check length of the returned bytes.
          c4_ColIter iter(*col, off_, off_ + len_);
          iter.Next();
          return c4_Bytes(iter.BufLoad(), iter.BufLen() < len_ ? iter.BufLen():
            len_);
        } else {
          const t4_byte *bytes = col->FetchBytes(off_, len_, buffer, false);
          if (bytes == buffer.Contents())
            return buffer;
          return c4_Bytes(bytes, len_);
        }
      } else
       { // do it the hard way for custom/mapped views (2002-03-13)
        c4_Bytes result;
        GetData(result);
        d4_assert(off_ + len_ <= result.Size());
        return c4_Bytes(result.Contents() + off_, len_, true);
      }
    }
  }

  return c4_Bytes();
}

bool c4_BytesRef::Modify(const c4_Bytes &buf_, t4_i32 off_, int diff_)const {
  int colNum = _cursor._seq->PropIndex(_property.GetId());
  if (colNum >= 0) {
    c4_Handler &h = _cursor._seq->NthHandler(colNum);
    const int n = buf_.Size();
    const t4_i32 limit = off_ + n; // past changed bytes
    const t4_i32 overshoot = limit - h.ItemSize(_cursor._index);

    if (diff_ < overshoot)
      diff_ = overshoot;

    c4_Column *col = h.GetNthMemoCol(_cursor._index, true);
    if (col != 0) {
      if (diff_ < 0)
        col->Shrink(limit,  - diff_);
      else if (diff_ > 0)
      // insert bytes in the highest possible spot
      // if a gap is created, it will contain garbage
        col->Grow(overshoot > 0 ? col->ColSize(): diff_ > n ? off_ : limit -
          diff_, diff_);

      col->StoreBytes(off_, buf_);
    } else
     { // do it the hard way for custom/mapped views (2002-03-13)
      c4_Bytes orig;
      GetData(orig);

      c4_Bytes result;
      t4_byte *ptr = result.SetBuffer(orig.Size() + diff_);

      memcpy(ptr, orig.Contents(), off_);
      memcpy(ptr + off_, buf_.Contents(), n);
      memcpy(ptr + off_ + n, orig.Contents() + off_, orig.Size() - off_);

      SetData(result);
    }
    return true;
  }

  return false;
}

/////////////////////////////////////////////////////////////////////////////

c4_StringRef::operator const char *()const {
  c4_Bytes result;
  GetData(result);

  return result.Size() > 0 ? (const char*)result.Contents(): "";
}

c4_StringRef &c4_StringRef::operator = (const char *value_) {
  SetData(c4_Bytes(value_, strlen(value_) + 1));
  return  *this;
}

/////////////////////////////////////////////////////////////////////////////

c4_ViewRef::operator c4_View()const {
  c4_Bytes result;
  if (!GetData(result))
    return (c4_Sequence*)0;
  // resolve ambiguity

  d4_assert(result.Size() == sizeof(c4_Sequence*));
  return *(c4_Sequence *const*)result.Contents();
}

c4_ViewRef &c4_ViewRef::operator = (const c4_View &value_) {
  SetData(c4_Bytes(&value_._seq, sizeof value_._seq));
  return  *this;
}

/////////////////////////////////////////////////////////////////////////////

c4_Stream::~c4_Stream(){}

/////////////////////////////////////////////////////////////////////////////

c4_Strategy::c4_Strategy(): _bytesFlipped(false), _failure(0), _mapStart(0),
  _dataSize(0), _baseOffset(0), _rootPos( - 1), _rootLen( - 1){}

c4_Strategy::~c4_Strategy() {
  d4_assert(_mapStart == 0);
}

/// Read a number of bytes
int c4_Strategy::DataRead(t4_i32, void *, int) {
  /*
  if (_mapStart != 0 && pos_ + length_ <= _dataSize)
  {
  memcpy(buffer_, _mapStart + pos_, length_);
  return length_;
  }
   */
  ++_failure;
  return  - 1;
}

/// Write a number of bytes, return true if successful
void c4_Strategy::DataWrite(t4_i32, const void *, int) {
  ++_failure;
}

/// Flush and truncate file
void c4_Strategy::DataCommit(t4_i32){}

/// Override to support memory-mapped files
void c4_Strategy::ResetFileMapping(){}

/// Report total size of the datafile
t4_i32 c4_Strategy::FileSize() {
  return _dataSize;
}

/// Return a value to use as fresh generation counter
t4_i32 c4_Strategy::FreshGeneration() {
  return 1;
}

/// Define the base offset where data is stored
void c4_Strategy::SetBase(t4_i32 base_) {
  t4_i32 off = base_ - _baseOffset;
  _baseOffset = base_;
  _dataSize -= off;
  if (_mapStart != 0)
    _mapStart += off;
}

/*
end_ is file position to start from (0 defaults to FileSize())

result is the logical end of the datafile (or -1 if no data)

This code uses a tiny state machine so all the code to read and decode
file marks is in one place within the loop.
 */

/// Scan datafile head/tail markers, return logical end of data
t4_i32 c4_Strategy::EndOfData(t4_i32 end_) {
  enum {
    kStateAtEnd, kStateCommit, kStateHead, kStateOld, kStateDone
  };

  t4_i32 pos = (end_ >= 0 ? end_ : FileSize()) - _baseOffset;
  t4_i32 last = pos;
  t4_i32 rootPos = 0;
  t4_i32 rootLen =  - 1; // impossible value, flags old-style header
  t4_byte mark[8];

  for (int state = kStateAtEnd; state != kStateDone;) {
    pos -= 8;
    if (pos + _baseOffset < 0 && state != kStateOld) {
      // bad offset, try old-style header from start of file
      pos =  - _baseOffset;
      state = kStateOld;
    }

    if (DataRead(pos, &mark, sizeof mark) != sizeof mark)
      return  - 1;

    t4_i32 count = 0;
    for (int i = 1; i < 4; ++i)
      count = (count << 8) + mark[i];

    t4_i32 offset = 0;
    for (int j = 4; j < 8; ++j)
      offset = (offset << 8) + mark[j];

    const bool isSkipTail = ((mark[0] & 0xF0) == 0x90 /* 2006-11-11 */ ||
                             mark[0] == 0x80 && count == 0) && offset > 0;
    const bool isCommitTail = mark[0] == 0x80 && count > 0 && offset > 0;
    const bool isHeader = (mark[0] == 'J' || mark[0] == 'L') && (mark[0] ^
      mark[1]) == ('J' ^ 'L') && mark[2] == 0x1A && (mark[3] & 0x40) == 0;
      
    switch (state) {
      case kStateAtEnd:
        // no commit tail found yet

        if (isSkipTail) {
          pos -= offset;
          last = pos;
        }
         else if (isCommitTail) {
          rootPos = offset;
          rootLen = count;
          state = kStateCommit;
        }
         else {
          pos = 8;
          state = kStateOld;
        }
        break;

      case kStateCommit:
        // commit tail must be preceded by skip tail

        if (!isSkipTail)
          return  - 1;
        pos -= offset - 8;
        state = kStateHead;
        break;

      case kStateHead:
        // fetch the header

        if (!isHeader) {
          pos = 8;
          state = kStateOld;
        }
         else {
          state = kStateDone;
        }
        break;

      case kStateOld:
        // old format, look for header in first 4 Kb

        if (isHeader && mark[3] == 0x80) {
          d4_assert(rootPos == 0);
          for (int k = 8; --k >= 4;)
          // old header is little-endian
            rootPos = (rootPos << 8) + mark[k];
          state = kStateDone;
        }
         else {
          pos += 16;
          if (pos > 4096)
            return  - 1;
        }
        break;
    }
  }

  last += _baseOffset; // all seeks were relative to current offset

  if (end_ >= 0)
   { // if end was specified, then adjust this strategy object
    _baseOffset += pos;
    d4_assert(_baseOffset >= 0);
    if (_mapStart != 0) {
      _mapStart += pos;
      _dataSize -= pos;
    }

    _rootPos = rootPos;
    _rootLen = rootLen;
  }

  d4_assert(mark[0] == 'J' || mark[1] == 'J');
  _bytesFlipped = (char)*(const short*)mark != 'J';

  return last;
}

/////////////////////////////////////////////////////////////////////////////
