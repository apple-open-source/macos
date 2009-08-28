// column.cpp --
// $Id: column.cpp 1230 2007-03-09 15:58:53Z jcw $
// This is part of Metakit, the homepage is http://www.equi4.com/metakit.html

/** @file
 * Implements c4_Column, c4_ColOfInts, and c4_ColIter
 */

#include "header.h"
#include "column.h"
#include "persist.h"

#if !q4_INLINE
#include "column.inl"
#endif 

/////////////////////////////////////////////////////////////////////////////

#if !HAVE_MEMMOVE && !HAVE_BCOPY
// in case we have no library memmove, or one that can't handle overlap

void f4_memmove(void *to_, const void *from_, int n_) {
  char *to = (char*)to_;
  const char *from = (const char*)from_;

  if (to + n_ <= from || from + n_ <= to)
    memcpy(to, from, n_);
  else if (to < from)
    while (--n_ >= 0)
      *to++ =  *from++;
    else if (to > from)
      while (--n_ >= 0)
        to[n_] = from[n_];
}

#endif 

/////////////////////////////////////////////////////////////////////////////
// c4_Column

c4_Column::c4_Column(c4_Persist *persist_): _position(0), _size(0), _persist
  (persist_), _gap(0), _slack(0), _dirty(false){}

#if q4_CHECK

// debugging version to verify that the internal data is consistent
void c4_Column::Validate()const {
  d4_assert(0 <= _slack && _slack < kSegMax);

  if (_segments.GetSize() == 0)
    return ;
  // ok, not initialized

  d4_assert(_gap <= _size);

  int n = fSegIndex(_size + _slack);
  d4_assert(n == _segments.GetSize() - 1);

  t4_byte *p = (t4_byte*)_segments.GetAt(n);

  if (fSegRest(_size + _slack) == 0)
    d4_assert(p == 0);
  else
    d4_assert(p != 0);

  while (--n >= 0) {
    t4_byte *p = (t4_byte*)_segments.GetAt(n);
    d4_assert(p != 0);
  }
}

#else 

// nothing, so inline this thing to avoid even the calling overhead
d4_inline void c4_Column::Validate()const{}

#endif 

c4_Column::~c4_Column() {
  Validate();
  ReleaseAllSegments();

  // this is needed to remove this column from the cache
  d4_assert(_slack == 0);
  FinishSlack();

  _slack =  - 1; // bad value in case we try to set up again (!)
}

c4_Strategy &c4_Column::Strategy()const {
  d4_assert(_persist != 0);

  return _persist->Strategy();
}

bool c4_Column::IsMapped()const {
  return _position > 1 && _persist != 0 && Strategy()._mapStart != 0;
}

bool c4_Column::UsesMap(const t4_byte *ptr_)const {
  // the most common falsifying case is checked first
  return _persist != 0 &&
    ptr_ >= Strategy()._mapStart &&
    Strategy()._dataSize != 0 &&
    ptr_ < Strategy()._mapStart + Strategy()._dataSize;
}

bool c4_Column::RequiresMap()const {
  if (_persist != 0 && Strategy()._mapStart != 0)
    for (int i = _segments.GetSize(); --i >= 0;)
      if (UsesMap((t4_byte*)_segments.GetAt(i)))
        return true;
  return false;
}

void c4_Column::ReleaseSegment(int index_) {
  t4_byte *p = (t4_byte*)_segments.GetAt(index_);
  if (!UsesMap(p))
    delete [] p;
}

void c4_Column::ReleaseAllSegments() {
  //for (int i = 0; i < _segments.GetSize(); ++i)
  for (int i = _segments.GetSize(); --i >= 0;)
    ReleaseSegment(i);
  // last one might be a null pointer

  _segments.SetSize(0);

  _gap = 0;
  _slack = 0;

  if (_size == 0)
    _position = 0;

  _dirty = false;
}

//@func Define where data is on file, or setup buffers (opt cleared).
void c4_Column::SetLocation(t4_i32 pos_, t4_i32 size_) {
  d4_assert(size_ > 0 || pos_ == 0);

  ReleaseAllSegments();

  _position = pos_;
  _size = size_;

  //  There are two position settings:
  //
  //     0 = raw buffer, no file access
  //    >1 = file position from where data can be loaded on demand

  _dirty = pos_ == 0;
}

void c4_Column::PullLocation(const t4_byte * &ptr_) {
  d4_assert(_segments.GetSize() == 0);

  _size = PullValue(ptr_);
  _position = 0;
  if (_size > 0) {
    _position = PullValue(ptr_);
    if (_position > 0) {
      d4_assert(_persist != 0);
      _persist->OccupySpace(_position, _size);
    }
  }

  _dirty = false;
}

//@func How many contiguous bytes are there at a specified position.
int c4_Column::AvailAt(t4_i32 offset_)const {
  d4_assert(offset_ <= _size);
  d4_assert(_gap <= _size);

  t4_i32 limit = _gap;

  if (offset_ >= _gap) {
    offset_ += _slack;
    limit = _size + _slack;
  }

  int count = kSegMax - fSegRest(offset_);
  if (offset_ + count > limit)
    count = (int)(limit - offset_);

  // either some real data or it must be at the very end of all data
  d4_assert(0 < count && count <= kSegMax || count == 0 && offset_ == _size +
    _slack);
  return count;
}

void c4_Column::SetupSegments() {
  d4_assert(_segments.GetSize() == 0);
  d4_assert(_gap == 0);
  d4_assert(_slack == 0);

  //  The last entry in the _segments array is either a partial block
  //  or a null pointer, so calling "fSegIndex(_size)" is always allowed.

  int n = fSegIndex(_size) + 1;
  _segments.SetSize(n);

  // treat last block differently if it is a partial entry
  int last = n;
  if (fSegRest(_size))
    --last;
  // this block is partial, size is 1 .. kSegMax-1
  else
    --n;
  // the last block is left as a null pointer

  int id =  - 1;
  if (_position < 0) {
    // special aside id, figure out the real position
    d4_assert(_persist != 0);
    id = ~_position;
    _position = _persist->LookupAside(id);
    d4_assert(_position >= 0);
  }

  if (IsMapped()) {
    // setup for mapped files is quick, just fill in the pointers
    d4_assert(_position > 1);
    d4_assert(_position + (n - 1) *kSegMax <= Strategy()._dataSize);
    const t4_byte *map = Strategy()._mapStart + _position;

    for (int i = 0; i < n; ++i) {
      _segments.SetAt(i, (t4_byte*)map); // loses const
      map += kSegMax;
    }
  } else {
    int chunk = kSegMax;
    t4_i32 pos = _position;

    // allocate buffers, load them if necessary
    for (int i = 0; i < n; ++i) {
      if (i == last)
        chunk = fSegRest(_size);

      t4_byte *p = d4_new t4_byte[chunk];
      _segments.SetAt(i, p);

      if (_position > 0) {
        d4_dbgdef(int n = )Strategy().DataRead(pos, p, chunk);
        d4_assert(n == chunk);
        pos += chunk;
      }
    }
  }

  if (id >= 0) {
    d4_assert(_persist != 0);
    _persist->ApplyAside(id,  *this);
  }

  Validate();
}

//@func Makes sure the requested data is in a modifiable buffer.
t4_byte *c4_Column::CopyNow(t4_i32 offset_) {
  d4_assert(offset_ <= _size);

  _dirty = true;

  const t4_byte *ptr = LoadNow(offset_);
  if (UsesMap(ptr)) {
    if (offset_ >= _gap)
      offset_ += _slack;

    // this will only force creation of a buffer
    ptr = CopyData(offset_, offset_, 0);
    d4_assert(!UsesMap(ptr));
  }

  return (t4_byte*)ptr;
}

//@func Copies data, creating a buffer if needed.  Must be in single segment.
t4_byte *c4_Column::CopyData(t4_i32 to_, t4_i32 from_, int count_) {
  int i = fSegIndex(to_);
  t4_byte *p = (t4_byte*)_segments.GetAt(i);

  if (UsesMap(p)) {
    int n = kSegMax;
    if (fSegOffset(i) + n > _size + _slack)
      n = (int)(_size + _slack - fSegOffset(i));

    d4_assert(n > 0);

    t4_byte *q = d4_new t4_byte[n];
    memcpy(q, p, n); // some copying can be avoided, overwritten below...
    _segments.SetAt(i, q);

    p = q;
  }

  p += fSegRest(to_);

  if (count_ > 0) {
    d4_assert(fSegIndex(to_ + count_ - 1) == i);

    const t4_byte *src = (const t4_byte*)_segments.GetAt(fSegIndex(from_));
    d4_memmove(p, src + fSegRest(from_), count_);
  }

  return p;
}

/*
 *  Resizing a segmented vector can be a complicated operation.
 *  For now, simply making it work in all cases is the first priority.
 *
 *  A major simplification - and good performance improvement - is caused
 *  by the trick of maintaining a "gap" in the data, which can be "moved"
 *  around to allow fast insertion as well as simple (delayed) deletion.
 *  
 *  The only complexity comes from the fact that the gap must end up being 
 *  less than one full segment in size.  Therefore, insertion and removal
 *  across segment boundaries needs to handle a variety of situations.
 *
 *  Since complete segments can be inserted quickly, this approach avoids
 *  lots of copying when consecutive insertions/deletions are clustered.
 *  Even random changes move half as much (on average) as without a gap.
 *
 *  The price is the overhead of up to one segment of empty space, and the
 *  complexity of this code (all the magic is within this c4_Column class).
 */

void c4_Column::MoveGapUp(t4_i32 dest_) {
  d4_assert(dest_ <= _size);
  d4_assert(_gap < dest_);
  d4_assert(_slack > 0);

  // forward loop to copy contents down, in little pieces if need be
  while (_gap < dest_) {
    int n = kSegMax - fSegRest(_gap);
    t4_i32 curr = _gap + n;
    if (curr > dest_)
      curr = dest_;

    // copy to [_gap..curr), which is inside one segment
    d4_assert(_gap < curr);
    d4_assert(fSegIndex(_gap) == fSegIndex(curr - 1));

    // copy from [_gap + _slack .. curr + _slack), of the same size
    t4_i32 fromBeg = _gap + _slack;
    t4_i32 fromEnd = curr + _slack;

    while (fromBeg < fromEnd) {
      int k = kSegMax - fSegRest(fromBeg);
      if (fromBeg + k > fromEnd)
        k = (int)(fromEnd - fromBeg);

      d4_assert(k > 0);

      CopyData(_gap, fromBeg, k);

      _gap += k;
      fromBeg += k;
    }

    _gap = curr;
  }

  d4_assert(_gap == dest_);
}

void c4_Column::MoveGapDown(t4_i32 dest_) {
  d4_assert(dest_ <= _size);
  d4_assert(_gap > dest_);
  d4_assert(_slack > 0);

  // reverse loop to copy contents up, in little pieces if need be
  t4_i32 toEnd = _gap + _slack;
  t4_i32 toBeg = dest_ + _slack;

  while (toEnd > toBeg) {
    int n = fSegRest(toEnd);
    t4_i32 curr = toEnd - (n ? n : kSegMax);
    if (curr < toBeg)
      curr = toBeg;

    // copy to [curr..toEnd), which is inside one segment
    d4_assert(curr < toEnd);
    d4_assert(fSegIndex(curr) == fSegIndex(toEnd - 1));

    // copy from [fromBeg .. _gap), which has the same size
    t4_i32 fromBeg = _gap - (toEnd - curr);

    while (_gap > fromBeg) {
      int k = fSegRest(_gap);
      if (k == 0)
        k = kSegMax;
      if (_gap - k < fromBeg)
        k = (int)(_gap - fromBeg);

      d4_assert(k > 0);

      toEnd -= k;
      _gap -= k;

      CopyData(toEnd, _gap, k);
    }
  }

  d4_assert(_gap == dest_);
}

void c4_Column::MoveGapTo(t4_i32 pos_) {
  d4_assert(pos_ <= _size);

  if (_slack == 0)
  // if there is no real gap, then just move it
    _gap = pos_;
  else if (_gap < pos_)
  // move the gap up, ie. some bytes down
    MoveGapUp(pos_);
  else if (_gap > pos_)
  // move the gap down, ie. some bytes up
  if (_gap - pos_ > _size - _gap + fSegRest(pos_)) {
    RemoveGap(); // it's faster to get rid of the gap instead
    _gap = pos_;
  } else
  // normal case, move some bytes up
    MoveGapDown(pos_);

  d4_assert(_gap == pos_);

  Validate();
}

void c4_Column::RemoveGap() {
  if (_slack > 0) {
    if (_gap < _size)
      MoveGapUp(_size);

    d4_assert(_gap == _size); // the gap is now at the end
    d4_assert(_slack < kSegMax);

    //  Case 1: gap is at start of segment
    //  ==================================
    //
    //    G G+S 
    //
    //    |  |
    //    :----+xx:
    //    |   |
    //
    //    i    i+1 (limit)
    //
    //  Case 2: gap is inside segment
    //  =============================
    //
    //       G G+S
    //
    //       |  |
    //    :--+--+x:
    //    |   |
    //
    //    i    i+1 (limit)
    //
    //  Case 3: gap runs to end of segment
    //  ==================================
    //
    //       G   G+S
    //
    //       |  |
    //    :--+----:0000000:
    //    |   |   |
    //
    //    i    i+1     i+2 (limit)
    //
    //  Case 4: gap is across two segments
    //  ==================================
    //
    //       G   G+S
    //
    //       |    |
    //    :--+----:-+xxxxx:
    //    |   |   |
    //
    //    i    i+1     i+2 (limit)

    int i = fSegIndex(_gap);
    int n = fSegRest(_gap);

    if (n == 0) {
      // case 1
      ReleaseSegment(i);
      _segments.SetAt(i, 0);
    } else {
      if (n + _slack > kSegMax)
      // case 4
        ReleaseSegment(i + 1);

      // truncate rest of segment
      t4_byte *p = d4_new t4_byte[n];
      memcpy(p, _segments.GetAt(i), n);

      ReleaseSegment(i);
      _segments.SetAt(i, p);
      _segments.SetSize(i + 1);
    }

    _slack = 0;
  }

  Validate();
}

void c4_Column::Grow(t4_i32 off_, t4_i32 diff_) {
  d4_assert(off_ <= _size);
  d4_assert(diff_ > 0);

  if (_segments.GetSize() == 0)
    SetupSegments();

  Validate();

  _dirty = true;

  // move the gap so it starts where we want to insert
  MoveGapTo(off_);

  t4_i32 bigSlack = _slack;
  if (bigSlack < diff_) {
    // only do more if this isn't good enough
    // number of segments to insert
    int n = fSegIndex(diff_ - _slack + kSegMax - 1);
    d4_assert(n > 0);

    int i1 = fSegIndex(_gap);
    int i2 = fSegIndex(_gap + _slack);

    bool moveBack = false;

    if (i2 > i1)
    // cases 3 and 4
      ++i1;
    else if (fSegRest(_gap))
    // case 2
      moveBack = true;

    _segments.InsertAt(i1, 0, n);
    for (int i = 0; i < n; ++i)
      _segments.SetAt(i1 + i, d4_new t4_byte[(int)kSegMax]);

    bigSlack += fSegOffset(n);

    if (moveBack) {
      d4_assert(i1 == fSegIndex(_gap));

      //  we have inserted too low, move bytes in front of gap back
      CopyData(fSegOffset(i1), fSegOffset(i1 + n), fSegRest(_gap));
    }
  }

  d4_assert(diff_ <= bigSlack && bigSlack < diff_ + kSegMax);

  _gap += diff_;
  _slack = (int)(bigSlack - diff_);
  _size += diff_;

  FinishSlack();
}

void c4_Column::Shrink(t4_i32 off_, t4_i32 diff_) {
  d4_assert(off_ <= _size);
  d4_assert(diff_ > 0);

  if (_segments.GetSize() == 0)
    SetupSegments();

  Validate();

  _dirty = true;

  // the simplification here is that we have in fact simply *two*
  // gaps and we must merge them together and end up with just one

  if (_slack > 0) {
    if (_gap < off_)
    // if too low, move the gap up
      MoveGapTo(off_);
    else if (off_ + diff_ < _gap)
    // if too high, move down to end
      MoveGapTo(off_ + diff_);

    // the gap is now inside, or adjacent to, the deleted area
    d4_assert(off_ <= _gap && _gap <= off_ + diff_);
  }

  _gap = off_;

  // check whether the merged gap would cross a segment boundary
  int i1 = fSegIndex(_gap);
  int i2 = fSegIndex(_gap + _slack + diff_);

  // drop complete segments, not a partially filled boundary
  if (fSegRest(_gap))
    ++i1;

  // moved up (was after the next if in the 1.7 May 28 build)
  _slack += diff_;
  _size -= diff_;

  int n = i2 - i1;
  if (n > 0) {
    for (int i = i1; i < i2; ++i)
      ReleaseSegment(i);

    _segments.RemoveAt(i1, n);

    // the logic in 1.7 of May 28 was warped (the assert "fix" was wrong)
    d4_assert(_slack >= fSegOffset(n));
    _slack -= fSegOffset(n);
  }

  d4_assert(0 <= _slack && _slack < 2 *kSegMax);

  // if the gap is at the end, get rid of a partial segment after it
  if (_gap == _size) {
    int i = fSegIndex(_size + _slack);
    if (i != fSegIndex(_gap)) {
      d4_assert(i == fSegIndex(_gap) + 1);
      d4_assert(i == _segments.GetSize() - 1);

      ReleaseSegment(i);
      _segments.SetAt(i, 0);

      _slack -= fSegRest(_size + _slack);

      d4_assert(_slack < kSegMax);
      d4_assert(fSegRest(_gap + _slack) == 0);
    }
  }

  // the slack may still be too large to leave as is
  if (_slack >= kSegMax) {
    // move the bytes just after the end of the gap one segment down
    int x = fSegRest(_gap + _slack);
    int r = kSegMax - x;
    if (_gap + r > _size)
      r = (int)(_size - _gap);

    CopyData(_gap, _gap + _slack, r);

    int i = fSegIndex(_gap + kSegMax - 1);
    ReleaseSegment(i);

    if (r + x < kSegMax)
      _segments.SetAt(i, 0);
    else
      _segments.RemoveAt(i);

    _slack -= r + x;
    _gap += r;
  }

  // if we have no data anymore, make sure not to use the file map either
  if (_size == 0 && _slack > 0)
    CopyNow(0);

  FinishSlack();
}

void c4_Column::FinishSlack() {
  Validate();

  // optimization: if partial end segment easily fits in slack, move it down
  t4_i32 gapEnd = _gap + _slack;
  if (!fSegRest(gapEnd) && gapEnd >= _size + 500) {
    // slack is at least 500 bytes more than the partial end segment
    // also, the gap must end exactly on a segment boundary
    int i = fSegIndex(gapEnd);
    d4_assert(i == _segments.GetSize() - 1);

    int n = _size - _gap;
    CopyData(gapEnd - n, gapEnd, n);

    ReleaseSegment(i);
    _segments.SetAt(i, 0);

    _slack -= n;
    d4_assert(_slack >= 500);

    Validate();
  }
}

void c4_Column::SaveNow(c4_Strategy &strategy_, t4_i32 pos_) {
  if (_segments.GetSize() == 0)
    SetupSegments();

  // write all segments
  c4_ColIter iter(*this, 0, _size);
  while (iter.Next(kSegMax)) {
    int n = iter.BufLen();
    strategy_.DataWrite(pos_, iter.BufLoad(), n);
    if (strategy_._failure != 0)
      break;
    pos_ += n;
  }
}

const t4_byte *c4_Column::FetchBytes(t4_i32 pos_, int len_, c4_Bytes &buffer_,
  bool forceCopy_) {
  d4_assert(len_ > 0);
  d4_assert(pos_ + len_ <= ColSize());
  d4_assert(0 <= _slack && _slack < kSegMax);

  c4_ColIter iter(*this, pos_, pos_ + len_);
  iter.Next();

  // most common case, all bytes are inside the same segment
  if (!forceCopy_ && iter.BufLen() == len_)
    return iter.BufLoad();

  t4_byte *p = buffer_.SetBuffer(len_);
  do {
    d4_assert(iter.BufLen() > 0);
    memcpy(p, iter.BufLoad(), iter.BufLen());
    p += iter.BufLen();
  } while (iter.Next());
  d4_assert(p == buffer_.Contents() + len_);

  return buffer_.Contents();
}

void c4_Column::StoreBytes(t4_i32 pos_, const c4_Bytes &buffer_) {
  int n = buffer_.Size();
  if (n > 0) {
    d4_assert(pos_ + n <= ColSize());

    c4_ColIter iter(*this, pos_, pos_ + n);

    const t4_byte *p = buffer_.Contents();
    while (iter.Next(n)) {
      d4_assert(iter.BufLen() > 0);
      memcpy(iter.BufSave(), p, iter.BufLen());
      p += iter.BufLen();
    }
    d4_assert(p == buffer_.Contents() + n);
  }
}

/*
PushValue and PullValue deal with variable-sized storage of
one unsigned integer value of up to 32 bits. Depending on the
magnitude of the integer, 1..6 bytes are used to represent it.
Each byte holds 7 significant bits and one continuation bit.
This saves storage, but it is also byte order independent.
Negative values are stored as a zero byte plus positive value.
 */

t4_i32 c4_Column::PullValue(const t4_byte * &ptr_) {
  t4_i32 mask =  *ptr_ ? 0 : ~0;

  t4_i32 v = 0;
  for (;;) {
    v = (v << 7) +  *ptr_;
    if (*ptr_++ &0x80)
      break;
  }

  return mask ^ (v - 0x80); // oops, last byte had bit 7 set
}

void c4_Column::PushValue(t4_byte * &ptr_, t4_i32 v_) {
  if (v_ < 0) {
    v_ = ~v_;
    *ptr_++ = 0;
  }

  int n = 0;
  do {
    n += 7;
  } while ((v_ >> n) && n < 32);

  while (n) {
    n -= 7;
    t4_byte b = (t4_byte)((v_ >> n) &0x7F);
    if (!n)
      b |= 0x80;
    // set bit 7 on the last byte
    *ptr_++ = b;
  }
}

void c4_Column::InsertData(t4_i32 index_, t4_i32 count_, bool clear_) {
  d4_assert(index_ <= ColSize());

  if (count_ > 0) {
    Grow(index_, count_);

    // clear the contents, in separate chunks if necessary
    if (clear_) {
      c4_ColIter iter(*this, index_, index_ + count_);
      while (iter.Next())
        memset(iter.BufSave(), 0, iter.BufLen());
    }
  }
}

void c4_Column::RemoveData(t4_i32 index_, t4_i32 count_) {
  d4_assert(index_ + count_ <= ColSize());

  if (count_ > 0)
    Shrink(index_, count_);
}

/////////////////////////////////////////////////////////////////////////////

void c4_ColOfInts::Get_0b(int) {
  *(t4_i32*)_item = 0;
}

void c4_ColOfInts::Get_1b(int index_) {
  t4_i32 off = index_ >> 3;
  d4_assert(off < ColSize());

  *(t4_i32*)_item = (*LoadNow(off) >> (index_ &7)) &0x01;
}

void c4_ColOfInts::Get_2b(int index_) {
  t4_i32 off = index_ >> 2;
  d4_assert(off < ColSize());

  *(t4_i32*)_item = (*LoadNow(off) >> ((index_ &3) << 1)) &0x03;
}

void c4_ColOfInts::Get_4b(int index_) {
  t4_i32 off = index_ >> 1;
  d4_assert(off < ColSize());

  *(t4_i32*)_item = (*LoadNow(off) >> ((index_ &1) << 2)) &0x0F;
}

void c4_ColOfInts::Get_8i(int index_) {
  t4_i32 off = index_;
  d4_assert(off < ColSize());

  *(t4_i32*)_item = *(const signed char*)LoadNow(off);
}

void c4_ColOfInts::Get_16i(int index_) {
  t4_i32 off = index_ *(t4_i32)2;
  d4_assert(off + 2 <= ColSize());

  const t4_byte *vec = LoadNow(off);

  _item[0] = vec[0];
  _item[1] = vec[1];

  *(t4_i32*)_item = *(const short*)_item;
}

void c4_ColOfInts::Get_16r(int index_) {
  t4_i32 off = index_ *(t4_i32)2;
  d4_assert(off + 2 <= ColSize());

  const t4_byte *vec = LoadNow(off);

  // 2003-02-02 - gcc 3.2.1 on linux (!) fails to compile this
  // sign-extension trick properly, use a temp buffer instead:
  //*(t4_i32*) _item = *(const short*) _item;

  t4_byte temp[2];
  temp[1] = vec[0];
  temp[0] = vec[1];
  *(t4_i32*)_item = *(const short*)temp;
}

void c4_ColOfInts::Get_32i(int index_) {
  t4_i32 off = index_ *(t4_i32)4;
  d4_assert(off + 4 <= ColSize());

  const t4_byte *vec = LoadNow(off);

  _item[0] = vec[0];
  _item[1] = vec[1];
  _item[2] = vec[2];
  _item[3] = vec[3];
}

void c4_ColOfInts::Get_32r(int index_) {
  t4_i32 off = index_ *(t4_i32)4;
  d4_assert(off + 4 <= ColSize());

  const t4_byte *vec = LoadNow(off);

  _item[3] = vec[0];
  _item[2] = vec[1];
  _item[1] = vec[2];
  _item[0] = vec[3];
}

void c4_ColOfInts::Get_64i(int index_) {
  t4_i32 off = index_ *(t4_i32)8;
  d4_assert(off + 8 <= ColSize());

  const t4_byte *vec = LoadNow(off);

  for (int i = 0; i < 8; ++i)
    _item[i] = vec[i];
}

void c4_ColOfInts::Get_64r(int index_) {
  t4_i32 off = index_ *(t4_i32)8;
  d4_assert(off + 8 <= ColSize());

  const t4_byte *vec = LoadNow(off);

  for (int i = 0; i < 8; ++i)
    _item[7-i] = vec[i];
}

/////////////////////////////////////////////////////////////////////////////

static int fBitsNeeded(t4_i32 v) {
  if ((v >> 4) == 0) {
    static int bits[] =  {
      0, 1, 2, 2, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4
    };
    return bits[(int)v];
  }

  if (v < 0)
  // first flip all bits if bit 31 is set
    v = ~v;
  // ... bit 31 is now always zero

  // then check if bits 15-31 used (32b), 7-31 used (16b), else (8b)
  return v >> 15 ? 32 : v >> 7 ? 16 : 8;
}

bool c4_ColOfInts::Set_0b(int, const t4_byte *item_) {
  t4_i32 v = *(const t4_i32*)item_;
  return v == 0;
}

bool c4_ColOfInts::Set_1b(int index_, const t4_byte *item_) {
  t4_i32 v = *(const t4_i32*)item_;

  t4_i32 off = index_ >> 3;
  d4_assert(off < ColSize());

  index_ &= 7;

  t4_byte *p = CopyNow(off);
  *p = (*p &~(1 << index_)) | (((t4_byte)v &1) << index_);

  return (v >> 1) == 0;
}

bool c4_ColOfInts::Set_2b(int index_, const t4_byte *item_) {
  t4_i32 v = *(const t4_i32*)item_;

  t4_i32 off = index_ >> 2;
  d4_assert(off < ColSize());

  const int n = (index_ &3) << 1;

  t4_byte *p = CopyNow(off);
  *p = (*p &~(0x03 << n)) | (((t4_byte)v &0x03) << n);

  return (v >> 2) == 0;
}

bool c4_ColOfInts::Set_4b(int index_, const t4_byte *item_) {
  t4_i32 v = *(const t4_i32*)item_;

  t4_i32 off = index_ >> 1;
  d4_assert(off < ColSize());

  const int n = (index_ &1) << 2;

  t4_byte *p = CopyNow(off);
  *p = (*p &~(0x0F << n)) | (((t4_byte)v &0x0F) << n);

  return (v >> 4) == 0;
}

// avoid a bug in MS EVC 3.0's code gen for ARM (i.e. WinCE)
#ifdef _ARM_
#pragma optimize("g",off)
#endif 

bool c4_ColOfInts::Set_8i(int index_, const t4_byte *item_) {
  t4_i32 v = *(const t4_i32*)item_;

  t4_i32 off = index_;
  d4_assert(off < ColSize());

  *(char*)CopyNow(off) = (char)v;

  return v == (signed char)v;
}

#ifdef _ARM_
#pragma optimize("",on)
#endif 

bool c4_ColOfInts::Set_16i(int index_, const t4_byte *item_) {
  t4_i32 v = *(const t4_i32*)item_;

  t4_i32 off = index_ *(t4_i32)2;
  d4_assert(off + 2 <= ColSize());

  *(short*)CopyNow(off) = (short)v;

  return v == (short)v;
}

bool c4_ColOfInts::Set_16r(int index_, const t4_byte *item_) {
  t4_i32 v = *(const t4_i32*)item_;

  t4_byte buf[2];
  *(short*)buf = (short)v;

  t4_i32 off = index_ *(t4_i32)2;
  d4_assert(off + 2 <= ColSize());

  t4_byte *vec = CopyNow(off);

  vec[1] = buf[0];
  vec[0] = buf[1];

  return v == (short)v;
}

bool c4_ColOfInts::Set_32i(int index_, const t4_byte *item_) {
  t4_i32 v = *(const t4_i32*)item_;

  t4_i32 off = index_ *(t4_i32)4;
  d4_assert(off + 4 <= ColSize());

  *(t4_i32*)CopyNow(off) = (t4_i32)v;

  return true;
}

bool c4_ColOfInts::Set_32r(int index_, const t4_byte *item_) {
  t4_i32 off = index_ *(t4_i32)4;
  d4_assert(off + 4 <= ColSize());

  t4_byte *vec = CopyNow(off);

  vec[3] = item_[0];
  vec[2] = item_[1];
  vec[1] = item_[2];
  vec[0] = item_[3];

  return true;
}

bool c4_ColOfInts::Set_64i(int index_, const t4_byte *item_) {
  t4_i32 off = index_ *(t4_i32)8;
  d4_assert(off + 8 <= ColSize());

  t4_byte *vec = CopyNow(off);

  for (int i = 0; i < 8; ++i)
    vec[i] = item_[i];

  return true;
}

bool c4_ColOfInts::Set_64r(int index_, const t4_byte *item_) {
  t4_i32 off = index_ *(t4_i32)8;
  d4_assert(off + 8 <= ColSize());

  t4_byte *vec = CopyNow(off);

  for (int i = 0; i < 8; ++i)
    vec[7-i] = item_[i];

  return true;
}

/////////////////////////////////////////////////////////////////////////////

c4_ColOfInts::c4_ColOfInts(c4_Persist *persist_, int width_): c4_Column
  (persist_), _getter(&c4_ColOfInts::Get_0b), _setter(&c4_ColOfInts::Set_0b),
  _currWidth(0), _dataWidth(width_), _numRows(0), _mustFlip(false){}

void c4_ColOfInts::ForceFlip() {
  _mustFlip = true;
}

int c4_ColOfInts::RowCount()const {
  d4_assert(_numRows >= 0);

  return _numRows;
}

int c4_ColOfInts::CalcAccessWidth(int numRows_, t4_i32 colSize_) {
  d4_assert(numRows_ > 0);

  int w = (int)((colSize_ << 3) / numRows_);

  // deduce sub-byte sizes for small vectors, see c4_ColOfInts::Set
  if (numRows_ <= 7 && 0 < colSize_ && colSize_ <= 6) {
    static t4_byte realWidth[][6] =  {
      // sz =  1:  2:  3:  4:  5:  6:
       {
        8, 16, 1, 32, 2, 4
      }
      ,  { //  n = 1
        4, 8, 1, 16, 2, 0
      }
      ,  { //  n = 2
        2, 4, 8, 1, 0, 16
      }
      ,  { //  n = 3
        2, 4, 0, 8, 1, 0
      }
      ,  { //  n = 4
        1, 2, 4, 0, 8, 0
      }
      ,  { //  n = 5
        1, 2, 4, 0, 0, 8
      }
      ,  { //  n = 6
        1, 2, 0, 4, 0, 0
      }
      ,  //  n = 7
    };

    w = realWidth[numRows_ - 1][(int)colSize_ - 1];
    d4_assert(w > 0);
  }

  return (w &(w - 1)) == 0 ? w :  - 1;
}

void c4_ColOfInts::SetRowCount(int numRows_) {
  _numRows = numRows_;
  if (numRows_ > 0) {
    int w = CalcAccessWidth(numRows_, ColSize());
    d4_assert(w >= 0);
    SetAccessWidth(w);
  }
}

void c4_ColOfInts::FlipBytes() {
  if (_currWidth > 8) {
    int step = _currWidth >> 3;

    c4_ColIter iter(*this, 0, ColSize());
    while (iter.Next(step)) {
      t4_byte *data = iter.BufSave();
      d4_assert(data != 0);

      for (int j = 0; j < step; ++j) {
        t4_byte c = data[j];
        data[j] = data[step - j - 1];
        data[step - j - 1] = c;
      }
    }
  }
}

void c4_ColOfInts::SetAccessWidth(int bits_) {
  d4_assert((bits_ &(bits_ - 1)) == 0);

  int l2bp1 = 0; // "log2 bits plus one" needed to represent value
  while (bits_) {
    ++l2bp1;
    bits_ >>= 1;
  }
  d4_assert(0 <= l2bp1 && l2bp1 < 8);

  _currWidth = (1 << l2bp1) >> 1;

  if (l2bp1 > 4 && (_mustFlip || Persist() != 0 && Strategy()._bytesFlipped))
    l2bp1 += 3;
  // switch to the trailing entries for byte flipping

  // Metrowerks Codewarrior 11 is dumb, it requires the "&c4_ColOfInts::"

  static tGetter gTab[] =  {
     &c4_ColOfInts::Get_0b,  //  0:  0 bits/entry
     &c4_ColOfInts::Get_1b,  //  1:  1 bits/entry
     &c4_ColOfInts::Get_2b,  //  2:  2 bits/entry
     &c4_ColOfInts::Get_4b,  //  3:  4 bits/entry

     &c4_ColOfInts::Get_8i,  //  4:  8 bits/entry
     &c4_ColOfInts::Get_16i,  //  5: 16 bits/entry
     &c4_ColOfInts::Get_32i,  //  6: 32 bits/entry
     &c4_ColOfInts::Get_64i,  //  7: 64 bits/entry

     &c4_ColOfInts::Get_16r,  //  8: 16 bits/entry, reversed
     &c4_ColOfInts::Get_32r,  //  9: 32 bits/entry, reversed
     &c4_ColOfInts::Get_64r,  // 10: 64 bits/entry, reversed
  };

  static tSetter sTab[] =  {
     &c4_ColOfInts::Set_0b,  //  0:  0 bits/entry
     &c4_ColOfInts::Set_1b,  //  1:  1 bits/entry
     &c4_ColOfInts::Set_2b,  //  2:  2 bits/entry
     &c4_ColOfInts::Set_4b,  //  3:  4 bits/entry

     &c4_ColOfInts::Set_8i,  //  4:  8 bits/entry
     &c4_ColOfInts::Set_16i,  //  5: 16 bits/entry
     &c4_ColOfInts::Set_32i,  //  6: 32 bits/entry
     &c4_ColOfInts::Set_64i,  //  7: 64 bits/entry

     &c4_ColOfInts::Set_16r,  //  8: 16 bits/entry, reversed
     &c4_ColOfInts::Set_32r,  //  9: 32 bits/entry, reversed
     &c4_ColOfInts::Set_64r,  // 10: 64 bits/entry, reversed
  };

  d4_assert(l2bp1 < sizeof gTab / sizeof * gTab);

  _getter = gTab[l2bp1];
  _setter = sTab[l2bp1];

  d4_assert(_getter != 0 && _setter != 0);
}

int c4_ColOfInts::ItemSize(int) {
  return _currWidth >= 8 ? _currWidth >> 3:  - _currWidth;
}

const void *c4_ColOfInts::Get(int index_, int &length_) {
  d4_assert(sizeof _item >= _dataWidth);

  (this->*_getter)(index_);

  length_ = _dataWidth;
  return _item;
}

void c4_ColOfInts::Set(int index_, const c4_Bytes &buf_) {
  d4_assert(buf_.Size() == _dataWidth);

  if ((this->*_setter)(index_, buf_.Contents()))
    return ;

  d4_assert(buf_.Size() == sizeof(t4_i32));

  int n = fBitsNeeded(*(const t4_i32*)buf_.Contents());
  if (n > _currWidth) {
    int k = RowCount();

    t4_i32 oldEnd = ColSize();
    t4_i32 newEnd = ((t4_i32)k *n + 7) >> 3;

    if (newEnd > oldEnd) {
      InsertData(oldEnd, newEnd - oldEnd, _currWidth == 0);

      // 14-5-2002: need to get rid of gap in case it risks not being a
      //  multiple of the increased size (bug, see s46 regression test)
      //
      // Example scenario: gap size is odd, data gets resized to 2/4-byte
      // ints, data at end fits without moving gap to end, then we end
      // up with a vector that has an int split *across* the gap - this
      // commits just fine, but access to that split int is now bad.
      //
      // Lesson: need stricter/simpler consistency, it's way too complex!
      if (n > 8)
        RemoveGap();
    }

    // data value exceeds width, expand to new size and repeat
    if (_currWidth > 0) {
      d4_assert(n % _currWidth == 0); // must be expanding by a multiple

      // To expand, we start by inserting a new appropriate chunk
      // at the end, and expand the entries in place (last to first).

      tGetter oldGetter = _getter;
      SetAccessWidth(n);

      d4_assert(sizeof _item >= _dataWidth);

      // this expansion in place works because it runs backwards
      while (--k >= 0) {
        (this->*oldGetter)(k);
        (this->*_setter)(k, _item);
      }
    } else {
      if (_dataWidth > (int)sizeof(t4_i32))
        n = _dataWidth << 3;
      // don't trust setter result, use max instead

      SetAccessWidth(n);
    }

    // now repeat the failed call to _setter
     /* bool f = */(this->*_setter)(index_, buf_.Contents());
    //? d4_assert(f);
  }
}

t4_i32 c4_ColOfInts::GetInt(int index_) {
  int n;
  const void *p = Get(index_, n);
  d4_assert(n == sizeof(t4_i32));
  return *(const t4_i32*)p;
}

void c4_ColOfInts::SetInt(int index_, t4_i32 value_) {
  Set(index_, c4_Bytes(&value_, sizeof value_));
}

int c4_ColOfInts::DoCompare(const c4_Bytes &b1_, const c4_Bytes &b2_) {
  d4_assert(b1_.Size() == sizeof(t4_i32));
  d4_assert(b2_.Size() == sizeof(t4_i32));

  t4_i32 v1 = *(const t4_i32*)b1_.Contents();
  t4_i32 v2 = *(const t4_i32*)b2_.Contents();

  return v1 == v2 ? 0 : v1 < v2 ?  - 1:  + 1;
}

void c4_ColOfInts::Insert(int index_, const c4_Bytes &buf_, int count_) {
  d4_assert(buf_.Size() == _dataWidth);
  d4_assert(count_ > 0);

  bool clear = true;
  const t4_byte *ptr = buf_.Contents();

  for (int i = 0; i < _dataWidth; ++i)
  if (*ptr++) {
    clear = false;
    break;
  }

  ResizeData(index_, count_, clear);

  if (!clear)
    while (--count_ >= 0)
      Set(index_++, buf_);
}

void c4_ColOfInts::Remove(int index_, int count_) {
  d4_assert(count_ > 0);

  ResizeData(index_,  - count_);
}

void c4_ColOfInts::ResizeData(int index_, int count_, bool clear_) {
  _numRows += count_;

  if (!(_currWidth &7)) {
    // not 1, 2, or 4
    const t4_i32 w = (t4_i32)(_currWidth >> 3);
    if (count_ > 0)
      InsertData(index_ *w, count_ *w, clear_);
    else
      RemoveData(index_ *w,  - count_ * w);
    return ;
  }

  d4_assert(_currWidth == 1 || _currWidth == 2 || _currWidth == 4);

  /*  _currwidth    1:  2:  4:
   *    shiftPos     3   2   1    shift the offset right this much
   *    maskPos      7   3   1    mask the offset with this
   */

  const int shiftPos = _currWidth == 4 ? 1 : 4-_currWidth;
  const int maskPos = (1 << shiftPos) - 1;

  // the following code is similar to c4_Column::Resize, but at bit level

  // turn insertion into deletion by inserting entire bytes
  if (count_ > 0) {
    unsigned off = (unsigned)index_ >> shiftPos;
    int gapBytes = (count_ + maskPos) >> shiftPos;

    InsertData(off, gapBytes, clear_);

    // oops, we might have inserted too low by a few entries
    const int bits = (index_ &maskPos) *_currWidth;
    if (bits) {
      const int maskLow = (1 << bits) - 1;

      // move the first few bits to start of inserted range
      t4_byte *p = CopyNow(off + gapBytes);
      t4_byte one =  *p &maskLow;
      *p &= ~maskLow;

      *CopyNow(off) = one;
    }

    index_ += count_;
    count_ -= gapBytes << shiftPos;
    d4_assert(count_ <= 0);
  }

  // now perform a deletion using a forward loop to copy down
  if (count_ < 0) {
    c4_Bytes temp;

    while (index_ < _numRows) {
      int length;
      const void *ptr = Get(index_ - count_, length);
      Set(index_++, c4_Bytes(ptr, length));
    }
  } else
    d4_assert(count_ == 0);

  FixSize(false);
}

void c4_ColOfInts::FixSize(bool fudge_) {
  int n = RowCount();
  t4_i32 needBytes = ((t4_i32)n *_currWidth + 7) >> 3;

  // use a special trick to mark sizes less than 1 byte in storage
  if (fudge_ && 1 <= n && n <= 4 && (_currWidth &7)) {
    const int shiftPos = _currWidth == 4 ? 1 : 4-_currWidth;

    static t4_byte fakeSizes[3][4] =  {
       { //  n:  1:  2:  3:  4:
        6, 1, 2, 2
      }
      ,  { //  4-bit entries:   4b  8b 12b 16b
        5, 5, 1, 1
      }
      ,  { //  2-bit entries:   2b  4b  6b  8b
        3, 3, 4, 5
      }
      ,  //  1-bit entries:   1b  2b  3b  4b
    };

    // The idea is to use an "impossible" size (ie. 5, for n = 2)
    // to give information about the current bit packing density.
    d4_assert(needBytes <= 2);
    needBytes = fakeSizes[shiftPos - 1][n - 1];
  }

  t4_i32 currSize = ColSize();

  if (needBytes < currSize)
    RemoveData(needBytes, currSize - needBytes);
  else if (needBytes > currSize)
    InsertData(currSize, needBytes - currSize, true);
}

/////////////////////////////////////////////////////////////////////////////

bool c4_ColIter::Next() {
  _pos += _len;

  _len = _column.AvailAt(_pos);
  _ptr = _column.LoadNow(_pos);

  if (!_ptr)
    _len = 0;
  else if (_pos + _len >= _limit)
    _len = _limit - _pos;
  else {
    // 19990831 - optimization to avoid most copying
    // while the end is adjacent to the next segment, extend it
    while (_ptr + _len == _column.LoadNow(_pos + _len)) {
      int n = _column.AvailAt(_pos + _len);
      if (n == 0)
        break;
      // may be a short column (strings)

      _len += n;

      if (_pos + _len >= _limit) {
        _len = _limit - _pos;
        break;
      }
    }
  }

  return _len > 0;
}

bool c4_ColIter::Next(int max_) {
  _pos += _len;

  _len = _column.AvailAt(_pos);
  _ptr = _column.LoadNow(_pos);

  if (!_ptr)
    _len = 0;
  else if (_pos + _len > _limit)
    _len = _limit - _pos;

  if (_len <= 0)
    return false;

  if (_len > max_)
    _len = max_;

  return true;
}

/////////////////////////////////////////////////////////////////////////////
