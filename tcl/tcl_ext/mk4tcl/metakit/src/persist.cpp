// persist.cpp --
// $Id: persist.cpp 1230 2007-03-09 15:58:53Z jcw $
// This is part of Metakit, the homepage is http://www.equi4.com/metakit.html

/** @file
 * Implementation of the main file management classes
 */

#include "header.h"
#include "column.h"
#include "persist.h"
#include "handler.h"
#include "store.h"
#include "field.h"

/////////////////////////////////////////////////////////////////////////////

class c4_FileMark {
    enum {
        kStorageFormat = 0x4C4A,  // b0 = 'J', b1 = <4C> (on Intel)
        kReverseFormat = 0x4A4C  // b0 = <4C>, b1 = 'J'
    };

    t4_byte _data[8];

  public:
    c4_FileMark();
    c4_FileMark(t4_i32 pos_, bool flipped_, bool extend_);
    c4_FileMark(t4_i32 pos_, int len_);

    t4_i32 Offset()const;
    t4_i32 OldOffset()const;

    bool IsHeader()const;
    bool IsOldHeader()const;
    bool IsFlipped()const;
};

/////////////////////////////////////////////////////////////////////////////

c4_FileMark::c4_FileMark() {
  d4_assert(sizeof *this == 8);
}

c4_FileMark::c4_FileMark(t4_i32 pos_, bool flipped_, bool extend_) {
  d4_assert(sizeof *this == 8);
  *(short*)_data = flipped_ ? kReverseFormat : kStorageFormat;
  _data[2] = extend_ ? 0x0A : 0x1A;
  _data[3] = 0;
  t4_byte *p = _data + 4;
  for (int i = 24; i >= 0; i -= 8)
    *p++ = (t4_byte)(pos_ >> i);
  d4_assert(p == _data + sizeof _data);
}

c4_FileMark::c4_FileMark(t4_i32 pos_, int len_) {
  d4_assert(sizeof *this == 8);
  t4_byte *p = _data;
  *p++ = 0x80;
  for (int j = 16; j >= 0; j -= 8)
    *p++ = (t4_byte)(len_ >> j);
  for (int i = 24; i >= 0; i -= 8)
    *p++ = (t4_byte)(pos_ >> i);
  d4_assert(p == _data + sizeof _data);
}

t4_i32 c4_FileMark::Offset()const {
  t4_i32 v = 0;
  for (int i = 4; i < 8; ++i)
    v = (v << 8) + _data[i];
  return v;
}

t4_i32 c4_FileMark::OldOffset()const {
  t4_i32 v = 0;
  for (int i = 8; --i >= 4;)
    v = (v << 8) + _data[i];
  return v;
}

bool c4_FileMark::IsHeader()const {
  return (_data[0] == 'J' || _data[0] == 'L') && (_data[0] ^ _data[1]) == ('J'
    ^ 'L') && _data[2] == 0x1A;
}

bool c4_FileMark::IsOldHeader()const {
  return IsHeader() && _data[3] == 0x80;
}

bool c4_FileMark::IsFlipped()const {
  return *(short*)_data == kReverseFormat;

}

/////////////////////////////////////////////////////////////////////////////

class c4_Allocator: public c4_DWordArray {
  public:
    c4_Allocator();

    void Initialize(t4_i32 first_ = 1);

    t4_i32 AllocationLimit()const;

    t4_i32 Allocate(t4_i32 len_);
    void Occupy(t4_i32 pos_, t4_i32 len_);
    void Release(t4_i32 pos_, t4_i32 len_);
    void Dump(const char *str_);
    t4_i32 FreeCounts(t4_i32 *bytes_ = 0);

  private:
    int Locate(t4_i32 pos_)const;
    void InsertPair(int i_, t4_i32 from_, t4_i32 to_);
    t4_i32 ReduceFrags(int goal_, int sHi_, int sLo_);
};

/////////////////////////////////////////////////////////////////////////////
//
//  Allocation of blocks is maintained in a separate data structure.
//  There is no allocation overhead in the allocation arena itself.
//  
//  A single vector of "walls" is maintained, sorted by position:
//  
//    * Each transition between free and allocated is a single entry.
//      The number of entries is <num-free-ranges> + <num-used-ranges>.
//    * By definition, free areas start at the positions indicated
//      by the entries on even indices. Allocated ones use odd entries.
//    * There is an extra <0,0> free slot at the very beginning. This
//      simplifies boundary conditions at the start of the arena.
//    * Position zero cannot be allocated, first slot starts at 1.
//
//  Properties of this approach:
//
//    * No allocation overhead for adjacent allocated areas. On the
//      other hand, the allocator does not know the size of used slots.
//    * Alternate function allows marking a specific range as occupied.
//    * Allocator can be initialized as either all free or all in-use.
//    * Allocation info contains only integers, it could be stored.
//    * To extend allocated slots: "occupy" extra bytes at the end.
//    * Generic: can be used for memory, disk files, and array entries.

c4_Allocator::c4_Allocator() {
  Initialize();
}

void c4_Allocator::Initialize(t4_i32 first_) {
  SetSize(0, 1000); // empty, and growing in large chunks 
  Add(0); // fake block at start
  Add(0); // ... only used to avoid merging

  // if occupied, add a tiny free slot at the end, else add entire range
  const t4_i32 kMaxInt = 0x7fffffff;
  if (first_ == 0)
    first_ = kMaxInt;

  Add(first_); // start at a nicely aligned position
  Add(kMaxInt); // ... there is no limit on file size
}

t4_i32 c4_Allocator::Allocate(t4_i32 len_) {
  // zero arg is ok, it simply returns first allocatable position   
  for (int i = 2; i < GetSize(); i += 2)
  if (GetAt(i + 1) >= GetAt(i) + len_) {
    t4_i32 pos = GetAt(i);
    if ((t4_i32)GetAt(i + 1) > pos + len_)
      ElementAt(i) += len_;
    else
      RemoveAt(i, 2);
    return pos;
  }

  d4_assert(0);
  return 0; // not reached
}

void c4_Allocator::Occupy(t4_i32 pos_, t4_i32 len_) {
  d4_assert(pos_ > 0);
  // note that zero size simply checks if there is any space to extend

  int i = Locate(pos_);
  d4_assert(0 < i && i < GetSize());

  if (i % 2) {
    // allocation is not at start of free block
    d4_assert((t4_i32)GetAt(i - 1) < pos_);

    if ((t4_i32)GetAt(i) == pos_ + len_)
    // allocate from end of free block
      SetAt(i, pos_);
    else
    // split free block in two
      InsertPair(i, pos_, pos_ + len_);
  } else if ((t4_i32)GetAt(i) == pos_)
  /*
  This side of the if used to be unconditional, but that was
  incorrect if ReduceFrags gets called (which only happens with
  severely fragmented files) - there are cases when allocation
  leads to an occupy request of which the free space list knows
  nothing about because it dropped small segments.  The solution
  is to silently "allow" such allocations - fixed 29-02-2000
  Thanks to Andrew Kuchling for his help in chasing this bug.
   */
   {
    // else extend tail of allocated area
    if ((t4_i32)GetAt(i + 1) > pos_ + len_)
      ElementAt(i) += len_;
    // move start of next free up
    else
      RemoveAt(i, 2);
    // remove this slot
  }
}

void c4_Allocator::Release(t4_i32 pos, t4_i32 len) {
  int i = Locate(pos + len);
  d4_assert(0 < i && i < GetSize());
  d4_assert(i % 2 == 0); // don't release inside a free block

  if ((t4_i32)GetAt(i) == pos)
  // move start of next free down 
    ElementAt(i) -= len;
  else if ((t4_i32)GetAt(i - 1) == pos)
  // move end of previous free up
    ElementAt(i - 1) += len;
  else
  // insert a new entry
    InsertPair(i, pos, pos + len);

  if (GetAt(i - 1) == GetAt(i))
  // merge if adjacent free
    RemoveAt(i - 1, 2);
}

t4_i32 c4_Allocator::AllocationLimit()const {
  d4_assert(GetSize() >= 2);

  return GetAt(GetSize() - 2);
}

int c4_Allocator::Locate(t4_i32 pos)const {
  int lo = 0, hi = GetSize() - 1;

  while (lo < hi) {
    int i = (lo + hi) / 2;
    if (pos < (t4_i32)GetAt(i))
      hi = i - 1;
    else if (pos > (t4_i32)GetAt(i))
      lo = i + 1;
    else
      return i;
  }

  return lo < GetSize() && pos > (t4_i32)GetAt(lo) ? lo + 1: lo;
}

void c4_Allocator::InsertPair(int i_, t4_i32 from_, t4_i32 to_) {
  d4_assert(0 < i_);
  d4_assert(i_ < GetSize());

  d4_assert(from_ < to_);
  d4_assert((t4_i32)GetAt(i_ - 1) < from_);
  //!d4_assert(to_ < GetAt(i_));

  if (to_ >= (t4_i32)GetAt(i_))
    return ;
  // ignore 2nd allocation of used area

  InsertAt(i_, from_, 2);
  SetAt(i_ + 1, to_);

  // it's ok to have arrays up to some 30000 bytes
  if (GetSize() > 7500)
    ReduceFrags(5000, 12, 6);
}

t4_i32 c4_Allocator::ReduceFrags(int goal_, int sHi_, int sLo_) {
  // drastic fail-safe measure: remove small gaps if vec gets too long
  // this will cause some lost free space but avoids array overflow
  // the lost space will most probably be re-used after the next commit

  int limit = GetSize() - 2;
  t4_i32 loss = 0;

  // go through all entries and remove gaps under the given threshold
  for (int shift = sHi_; shift >= sLo_; --shift) {
    // the threshold is a fraction of the current size of the arena
    t4_i32 threshold = AllocationLimit() >> shift;
    if (threshold == 0)
      continue;

    int n = 2;
    for (int i = n; i < limit; i += 2)
    if ((t4_i32)GetAt(i + 1) - (t4_i32)GetAt(i) > threshold) {
      SetAt(n++, GetAt(i));
      SetAt(n++, GetAt(i + 1));
    } else
      loss += GetAt(i + 1) - GetAt(i);

    limit = n;

    // if (GetSize() < goal_) - suboptimal, fixed 29-02-2000
    if (limit < goal_)
      break;
    // got rid of enough entries, that's enough
  }

  int n = GetSize() - 2;
  SetAt(limit++, GetAt(n++));
  SetAt(limit++, GetAt(n));
  SetSize(limit);

  return loss;
}

#if q4_CHECK
#include <stdio.h>

void c4_Allocator::Dump(const char *str_) {
  fprintf(stderr, "c4_Allocator::Dump, %d entries <%s>\n", GetSize(), str_);
  for (int i = 2; i < GetSize(); i += 2)
    fprintf(stderr, "  %10ld .. %ld\n", GetAt(i - 1), GetAt(i));
  fprintf(stderr, "END\n");
}

#else 

void c4_Allocator::Dump(const char *str_){}

#endif 

t4_i32 c4_Allocator::FreeCounts(t4_i32 *bytes_) {
  if (bytes_ != 0) {
    t4_i32 total = 0;
    for (int i = 2; i < GetSize() - 2; i += 2)
      total += GetAt(i + 1) - GetAt(i);
    *bytes_ = total;
  }
  return GetSize() / 2-2;
}

/////////////////////////////////////////////////////////////////////////////

class c4_Differ {
  public:
    c4_Differ(c4_Storage &storage_);
    ~c4_Differ();

    int NewDiffID();
    void CreateDiff(int id_, c4_Column &col_);
    t4_i32 BaseOfDiff(int id_);
    void ApplyDiff(int id_, c4_Column &col_)const;

    void GetRoot(c4_Bytes &buffer_);

    c4_Storage _storage;
    c4_View _diffs;
    c4_View _temp;

  private:
    void AddEntry(t4_i32, t4_i32, const c4_Bytes &);

    c4_ViewProp pCols; //  column info:
    c4_IntProp pOrig; //    original position
    c4_ViewProp pDiff; //    difference chunks:
    c4_IntProp pKeep; //      offset
    c4_IntProp pResize; //      length
    c4_BytesProp pBytes; //      data
};

c4_Differ::c4_Differ(c4_Storage &storage_): _storage(storage_), pCols("_C"),
  pOrig("_O"), pDiff("_D"), pKeep("_K"), pResize("_R"), pBytes("_B") {
  // weird names, to avoid clashing with existing ones (capitalization!)
  _diffs = _storage.GetAs("_C[_O:I,_D[_K:I,_R:I,_B:B]]");
}

c4_Differ::~c4_Differ() {
  _diffs = c4_View();
}

void c4_Differ::AddEntry(t4_i32 off_, t4_i32 len_, const c4_Bytes &data_) {
  int n = _temp.GetSize();
  _temp.SetSize(n + 1);
  c4_RowRef r = _temp[n];

  pKeep(r) = (t4_i32)off_;
  pResize(r) = (t4_i32)len_;
  pBytes(r).SetData(data_);
}

int c4_Differ::NewDiffID() {
  int n = _diffs.GetSize();
  _diffs.SetSize(n + 1);
  return n;
}

void c4_Differ::CreateDiff(int id_, c4_Column &col_) {
  _temp.SetSize(0);
#if 0
  t4_i32 offset = 0;
  t4_i32 savedOff = 0;
  t4_i32 savedLen = 0;

  c4_Strategy *strat = col_.Persist() != 0 ? &col_.Strategy(): 0;

  c4_ColIter iter(col_, 0, col_.ColSize());
  while (iter.Next()) {
    const t4_byte *p = iter.BufLoad();
    if (strat != 0 && strat->_mapStart != 0 && p >= strat->_mapStart && p -
      strat->_mapStart < strat->_dataSize) {
      t4_i32 nextOff = p - strat->_mapStart;
      if (savedLen == 0)
        savedOff = nextOff;
      if (nextOff == savedOff + savedLen) {
        savedLen += iter.BufLen();
        continue;
      }

      if (savedLen > 0)
        AddEntry(savedOff, savedLen, c4_Bytes());

      savedOff = nextOff;
      savedLen = iter.BufLen();
    } else {
      AddEntry(savedOff, savedLen, c4_Bytes(p, iter.BufLen()));
      savedLen = 0;
    }

    offset += iter.BufLen();
  }

  c4_View diff = pDiff(_diffs[id_]);
  if (_temp.GetSize() != diff.GetSize() || _temp != diff)
#else 
    c4_Bytes t1;
  const t4_byte *p = col_.FetchBytes(0, col_.ColSize(), t1, false);
  AddEntry(0, 0, c4_Bytes(p, col_.ColSize()));
#endif 
  pDiff(_diffs[id_]) = _temp;

  pOrig(_diffs[id_]) = col_.Position();
}

t4_i32 c4_Differ::BaseOfDiff(int id_) {
  d4_assert(0 <= id_ && id_ < _diffs.GetSize());

  return pOrig(_diffs[id_]);
}

void c4_Differ::ApplyDiff(int id_, c4_Column &col_)const {
  d4_assert(0 <= id_ && id_ < _diffs.GetSize());

  c4_View diff = pDiff(_diffs[id_]);
  t4_i32 offset = 0;

  for (int n = 0; n < diff.GetSize(); ++n) {
    c4_RowRef row(diff[n]);
    offset += pKeep(row);

    c4_Bytes data;
    pBytes(row).GetData(data);

    // the following code is a lot like c4_MemoRef::Modify
    const t4_i32 change = pResize(row);
    if (change < 0)
      col_.Shrink(offset,  - change);
    else if (change > 0)
      col_.Grow(offset, change);

    col_.StoreBytes(offset, data);
    offset += data.Size();
  }

  if (offset > col_.ColSize())
    col_.Shrink(offset, offset - col_.ColSize());
}

void c4_Differ::GetRoot(c4_Bytes &buffer_) {
  int last = _diffs.GetSize() - 1;
  if (last >= 0) {
    c4_Bytes temp;
    c4_View diff = pDiff(_diffs[last]);
    if (diff.GetSize() > 0)
      pBytes(diff[0]).GetData(buffer_);
  }
}

/////////////////////////////////////////////////////////////////////////////

c4_SaveContext::c4_SaveContext(c4_Strategy &strategy_, bool fullScan_, int
  mode_, c4_Differ *differ_, c4_Allocator *space_): _strategy(strategy_), _walk
  (0), _differ(differ_), _space(space_), _cleanup(0), _nextSpace(0), _preflight
  (true), _fullScan(fullScan_), _mode(mode_), _nextPosIndex(0), _bufPtr(_buffer)
  , _curr(_buffer), _limit(_buffer) {
  if (_space == 0)
    _space = _cleanup = d4_new c4_Allocator;

  _nextSpace = _mode == 1 ? d4_new c4_Allocator: _space;
}

c4_SaveContext::~c4_SaveContext() {
  delete _cleanup;
  if (_nextSpace != _space)
    delete _nextSpace;
}

bool c4_SaveContext::IsFlipped()const {
  return _strategy._bytesFlipped;
}

bool c4_SaveContext::Serializing()const {
  return _fullScan;
}

void c4_SaveContext::AllocDump(const char *str_, bool next_) {
  c4_Allocator *ap = next_ ? _nextSpace : _space;
  if (ap != 0)
    ap->Dump(str_);
}

void c4_SaveContext::FlushBuffer() {
  int n = _curr - _bufPtr;
  if (_walk != 0 && n > 0) {
    t4_i32 end = _walk->ColSize();
    _walk->Grow(end, n);
    _walk->StoreBytes(end, c4_Bytes(_bufPtr, n));
  }

  _curr = _bufPtr = _buffer;
  _limit = _buffer + sizeof _buffer;
}

c4_Column *c4_SaveContext::SetWalkBuffer(c4_Column *col_) {
  FlushBuffer();

  c4_Column *prev = _walk;
  _walk = col_;
  return prev;
}

void c4_SaveContext::Write(const void *buf_, int len_) {
  // use buffering if possible
  if (_curr + len_ <= _limit) {
    memcpy(_curr, buf_, len_);
    _curr += len_;
  } else {
    FlushBuffer();
    _bufPtr = (t4_byte*)buf_; // also loses const
    _curr = _limit = _bufPtr + len_;
    FlushBuffer();
  }
}

void c4_SaveContext::StoreValue(t4_i32 v_) {
  if (_walk == 0)
    return ;

  if (_curr + 10 >= _limit)
    FlushBuffer();

  d4_assert(_curr + 10 < _limit);
  c4_Column::PushValue(_curr, v_);
}

void c4_SaveContext::SaveIt(c4_HandlerSeq &root_, c4_Allocator **spacePtr_,
  c4_Bytes &rootWalk_) {
  d4_assert(_space != 0);

  const t4_i32 size = _strategy.FileSize();
  if (_strategy._failure != 0)
    return ;

  const t4_i32 end = _fullScan ? 0 : size - _strategy._baseOffset;

  if (_differ == 0) {
    if (_mode != 1)
      _space->Initialize();

    // don't allocate anything inside the file in extend mode
    if (_mode == 2 && end > 0) {
      _space->Occupy(1, end - 1);
      _nextSpace->Occupy(1, end - 1);
    }

    // the header is always reserved
    _space->Occupy(1, 7);
    _nextSpace->Occupy(1, 7);

    if (end > 0) {
      d4_assert(end >= 16);
      _space->Occupy(end - 16, 16);
      _nextSpace->Occupy(end - 16, 16);
      _space->Occupy(end, 8);
      _nextSpace->Occupy(end, 8);
    }
  }

  //AllocDump("a1", false);
  //AllocDump("a2", true);

  // first pass allocates columns and constructs shallow walks
  c4_Column walk(root_.Persist());
  SetWalkBuffer(&walk);
  CommitSequence(root_, true);
  SetWalkBuffer(0);
  CommitColumn(walk);

  c4_Bytes tempWalk;
  walk.FetchBytes(0, walk.ColSize(), tempWalk, true);

  t4_i32 limit = _nextSpace->AllocationLimit();
  d4_assert(limit >= 8 || _differ != 0);

  if (limit < 0) {
    // 2006-01-12 #2: catch file size exceeding 2 Gb
    _strategy._failure =  - 1; // unusual non-zero value flags this case
    return ;
  }

  bool changed = _fullScan || tempWalk != rootWalk_;

  rootWalk_ = c4_Bytes(tempWalk.Contents(), tempWalk.Size(), true);

  _preflight = false;

  // special-case to avoid saving data if file is logically empty
  // in that case, the data is 0x80 0x81 0x80 (plus the header)
  if (!_fullScan && limit <= 11 && _differ == 0) {
    _space->Initialize();
    _nextSpace->Initialize();
    changed = false;
  }

  if (!changed)
    return ;

  //AllocDump("b1", false);
  //AllocDump("b2", true);

  if (_differ != 0) {
    int n = _differ->NewDiffID();
    _differ->CreateDiff(n, walk);
    return ;
  }

  d4_assert(_mode != 0 || _fullScan);

  // this is the place where writing may start

  // figure out where the new file ends and write a skip tail there
  t4_i32 end0 = end;

  // true if the file need not be extended due to internal free space
  bool inPlace = end0 == limit - 8;
  if (inPlace) {
    d4_assert(!_fullScan);
    _space->Release(end0, 8);
    _nextSpace->Release(end0, 8);
    end0 -= 16; // overwrite existing tail markers
  } else {
    /* 18-11-2005 write new end marker and flush it before *anything* else! */
    if (!_fullScan && end0 < limit) {
      c4_FileMark mark1(limit, 0);
      _strategy.DataWrite(limit, &mark1, sizeof mark1);
      _strategy.DataCommit(0);
      if (_strategy._failure != 0)
        return ;
    }

    c4_FileMark head(limit + 16-end, _strategy._bytesFlipped, end > 0);
    _strategy.DataWrite(end, &head, sizeof head);

    if (end0 < limit)
      end0 = limit;
    // create a gap
  }

  t4_i32 end1 = end0 + 8;
  t4_i32 end2 = end1 + 8;

  if (!_fullScan && !inPlace) {
    c4_FileMark mark1(end0, 0);
    _strategy.DataWrite(end0, &mark1, sizeof mark1);
#if q4_WIN32
    /* March 8, 2002
     * On at least NT4 with NTFS, extending a file can cause it to be
     * rounded up further than expected.  To prevent creating a bad
     * file (since the file does then not end with a marker), the
     * workaround it so simply accept the new end instead and rewrite.
     * Note that between these two writes, the file is in a bad state.
     */
    t4_i32 realend = _strategy.FileSize() - _strategy._baseOffset;
    if (realend > end1) {
      end0 = limit = realend - 8;
      end1 = realend;
      end2 = realend + 8;
      c4_FileMark mark1a(end0, 0);
      _strategy.DataWrite(end0, &mark1a, sizeof mark1a);
    }
#endif 
    d4_assert(_strategy.FileSize() == _strategy._baseOffset + end1);
  }

  _space->Occupy(end0, 16);
  _nextSpace->Occupy(end0, 16);

  // strategy.DataCommit(0); // may be needed, need more info on how FS's work
  // but this would need more work, since we can't adjust file-mapping here

  // second pass saves the columns and structure to disk
  CommitSequence(root_, true); // writes changed columns
  CommitColumn(walk);

  //! d4_assert(_curr == 0);
  d4_assert(_nextPosIndex == _newPositions.GetSize());

  if (_fullScan) {
    c4_FileMark mark1(limit, 0);
    _strategy.DataWrite(_strategy.FileSize() - _strategy._baseOffset,  &mark1,
      sizeof mark1);

    c4_FileMark mark2(limit - walk.ColSize(), walk.ColSize());
    _strategy.DataWrite(_strategy.FileSize() - _strategy._baseOffset,  &mark2,
      sizeof mark2);

    return ;
  }

  if (inPlace)
    d4_assert(_strategy.FileSize() == _strategy._baseOffset + end2);
  else {
    // make sure the allocated size hasn't changed
    d4_assert(_nextSpace->AllocationLimit() == limit + 16);
    d4_assert(end0 >= limit);
    d4_assert(_strategy.FileSize() - _strategy._baseOffset == end1);
  }

  if (walk.Position() == 0 || _strategy._failure != 0)
    return ;

  _strategy.DataCommit(0);

  c4_FileMark mark2(walk.Position(), walk.ColSize());
  _strategy.DataWrite(end1, &mark2, sizeof mark2);
  d4_assert(_strategy.FileSize() - _strategy._baseOffset == end2);

  // do not alter the file header in extend mode, unless it is new
  if (!_fullScan && (_mode == 1 || end == 0)) {
    _strategy.DataCommit(0);

    c4_FileMark head(end2, _strategy._bytesFlipped, false);
    d4_assert(head.IsHeader());
    _strategy.DataWrite(0, &head, sizeof head);

    // if the file became smaller, we could shrink it
    if (limit + 16 < end0) {
      /*
      Not yet, this depends on the strategy class being able to truncate, but
      there is no way to find out whether it does (the solution is to write tail
      markers in such a way that the file won't grow unnecessarily if it doesn't).

      The logic will probably be:

       * write new skip + commit "tails" at limit (no visible effect on file)
       * overwrite commit tail at end  with a skip to this new one (equivalent)
       * replace header with one pointing to that internal new one (equivalent)
       * flush (now the file is valid both truncated and not-yet-truncated

      end = limit;
       */
    }
  }

  // if using memory mapped files, make sure the map is no longer in use
  if (_strategy._mapStart != 0)
    root_.UnmappedAll();

  // commit and tell strategy object what the new file size is, this
  // may be smaller now, if old data at the end is no longer referenced
  _strategy.DataCommit(end2);

  d4_assert(_strategy.FileSize() - _strategy._baseOffset == end2);

  if (spacePtr_ != 0 && _space != _nextSpace) {
    d4_assert(*spacePtr_ == _space);
    delete  *spacePtr_;
    *spacePtr_ = _nextSpace;
    _nextSpace = 0;
  }
}

bool c4_SaveContext::CommitColumn(c4_Column &col_) {
  bool changed = col_.IsDirty() || _fullScan;

  t4_i32 sz = col_.ColSize();
  StoreValue(sz);
  if (sz > 0) {
    t4_i32 pos = col_.Position();

    if (_differ) {
      if (changed) {
        int n = pos < 0 ? ~pos: _differ->NewDiffID();
        _differ->CreateDiff(n, col_);

        d4_assert(n >= 0);
        pos = ~n;
      }
    } else if (_preflight) {
      if (changed)
        pos = _space->Allocate(sz);

      _nextSpace->Occupy(pos, sz);
      _newPositions.Add(pos);
    } else {
      pos = _newPositions.GetAt(_nextPosIndex++);

      if (changed)
        col_.SaveNow(_strategy, pos);

      if (!_fullScan)
        col_.SetLocation(pos, sz);
    }

    StoreValue(pos);
  }

  return changed;
}

void c4_SaveContext::CommitSequence(c4_HandlerSeq &seq_, bool selfDesc_) {
  StoreValue(0); // sias prefix

  if (selfDesc_) {
    c4_String desc = seq_.Description();
    int k = desc.GetLength();
    StoreValue(k);
    Write((const char*)desc, k);
  }

  StoreValue(seq_.NumRows());
  if (seq_.NumRows() > 0)
    for (int i = 0; i < seq_.NumFields(); ++i)
      seq_.NthHandler(i).Commit(*this);
}

/////////////////////////////////////////////////////////////////////////////

// used for on-the-fly conversion of old-format datafiles
t4_byte *_oldBuf;
const t4_byte *_oldCurr;
const t4_byte *_oldLimit;
t4_i32 _oldSeek;


c4_Persist::c4_Persist(c4_Strategy &strategy_, bool owned_, int mode_): _space
  (0), _strategy(strategy_), _root(0), _differ(0), _fCommit(0), _mode(mode_),
  _owned(owned_), _oldBuf(0), _oldCurr(0), _oldLimit(0), _oldSeek( - 1) {
  if (_mode == 1)
    _space = d4_new c4_Allocator;
}

c4_Persist::~c4_Persist() {
  delete _differ;

  if (_owned) {
    if (_root != 0)
      _root->UnmappedAll();
    delete  &_strategy;
  }

  delete _space;

  if (_oldBuf != 0)
    delete [] _oldBuf;
}

c4_HandlerSeq &c4_Persist::Root()const {
  d4_assert(_root != 0);
  return  *_root;
}

void c4_Persist::SetRoot(c4_HandlerSeq *root_) {
  d4_assert(_root == 0);
  _root = root_;
}

c4_Strategy &c4_Persist::Strategy()const {
  return _strategy;
}

bool c4_Persist::AutoCommit(bool flag_) {
  bool prev = _fCommit != 0;
  if (flag_)
    _fCommit = &c4_Persist::Commit;
  else
    _fCommit = 0;
  return prev;
}

void c4_Persist::DoAutoCommit() {
  if (_fCommit != 0)
    (this->*_fCommit)(false);
}

bool c4_Persist::SetAside(c4_Storage &aside_) {
  delete _differ;
  _differ = d4_new c4_Differ(aside_);
  Rollback(false);
  return true; //! true if the generation matches
}

c4_Storage *c4_Persist::GetAside()const {
  return _differ != 0 ? &_differ->_storage: 0;
}

bool c4_Persist::Commit(bool full_) {
  // 1-Mar-1999, new semantics! return success status of commits
  _strategy._failure = 0;

  if (!_strategy.IsValid())
    return false;

  if (_mode == 0 && (_differ == 0 || full_))
  // can't commit to r/o file
    return false;
  // note that _strategy._failure is *zero* in this case

  c4_SaveContext ar(_strategy, false, _mode, full_ ? 0 : _differ, _space);

  // get rid of temp properties which still use the datafile
  if (_mode == 1)
    _root->DetachFromStorage(false);

  // 30-3-2001: moved down, fixes "crash every 2nd call of mkdemo/dbg"
  ar.SaveIt(*_root, &_space, _rootWalk);
  return _strategy._failure == 0;
}

bool c4_Persist::Rollback(bool full_) {
  _root->DetachFromParent();
  _root->DetachFromStorage(true);
  _root = 0;

  if (_space != 0)
    _space->Initialize();

  c4_HandlerSeq *seq = d4_new c4_HandlerSeq(this);
  seq->DefineRoot();
  SetRoot(seq);

  if (full_) {
    delete _differ;
    _differ = 0;
  }

  LoadAll();

  return _strategy._failure == 0;
}

bool c4_Persist::LoadIt(c4_Column &walk_) {
  t4_i32 limit = _strategy.FileSize();
  if (_strategy._failure != 0)
    return false;

  if (_strategy.EndOfData(limit) < 0) {
    _strategy.SetBase(limit);
    d4_assert(_strategy._failure == 0); // file is ok, but empty
    return false;
  }

  if (_strategy._rootLen > 0)
    walk_.SetLocation(_strategy._rootPos, _strategy._rootLen);

  // if the file size has increased, we must remap
  if (_strategy._mapStart != 0 && _strategy.FileSize() > _strategy._baseOffset 
    + _strategy._dataSize)
    _strategy.ResetFileMapping();

  return true;
}

void c4_Persist::LoadAll() {
  c4_Column walk(this);
  if (!LoadIt(walk))
    return ;

  if (_strategy._rootLen < 0) {
    _oldSeek = _strategy._rootPos;
    _oldBuf = d4_new t4_byte[512];
    _oldCurr = _oldLimit = _oldBuf;

    t4_i32 n = FetchOldValue();
    d4_assert(n == 0);
    n = FetchOldValue();
    d4_assert(n > 0);

    c4_Bytes temp;
    t4_byte *buf = temp.SetBuffer(n);
    d4_dbgdef(int n2 = )OldRead(buf, n);
    d4_assert(n2 == n);

    c4_String s = "[" + c4_String((const char*)buf, n) + "]";
    const char *desc = s;

    c4_Field *f = d4_new c4_Field(desc);
    d4_assert(! *desc);

    //?_root->DefineRoot();
    _root->Restructure(*f, false);

    _root->OldPrepare();

    // don't touch data inside while converting the file
    if (_strategy.FileSize() >= 0)
      OccupySpace(1, _strategy.FileSize());
  } else {
    walk.FetchBytes(0, walk.ColSize(), _rootWalk, true);
    if (_differ)
      _differ->GetRoot(_rootWalk);

    // 2006-08-01: maintain stable-storage space usage on re-open
    OccupySpace(_strategy._rootPos, _strategy._rootLen);

    // define and fill the root table 
    const t4_byte *ptr = _rootWalk.Contents();
    _root->Prepare(&ptr, true);
    d4_assert(ptr == _rootWalk.Contents() + _rootWalk.Size());
  }
}

t4_i32 c4_Persist::FetchOldValue() {
  d4_assert(_oldSeek >= 0);

  if (_oldCurr == _oldLimit) {
    int n = OldRead(_oldBuf, 500);
    _oldLimit = _oldCurr + n;
    _oldBuf[n] = 0x80; // to force end
  }

  const t4_byte *p = _oldCurr;
  t4_i32 value = c4_Column::PullValue(p);

  if (p > _oldLimit) {
    int k = _oldLimit - _oldCurr;
    d4_assert(0 < k && k < 10);
    memcpy(_oldBuf, _oldCurr, k);

    int n = OldRead(_oldBuf + k, 500);
    _oldCurr = _oldBuf + k;
    _oldLimit = _oldCurr + n;
    _oldBuf[n + k] = 0x80; // to force end

    p = _oldCurr;
    value = c4_Column::PullValue(p);
    d4_assert(p <= _oldLimit);
  }

  _oldCurr = p;
  return value;
}

void c4_Persist::FetchOldLocation(c4_Column &col_) {
  d4_assert(_oldSeek >= 0);

  t4_i32 sz = FetchOldValue();
  if (sz > 0)
    col_.SetLocation(FetchOldValue(), sz);
}

t4_i32 c4_Persist::FreeBytes(t4_i32 *bytes_) {
  return _space == 0 ?  - 1: _space->FreeCounts(bytes_);
}

int c4_Persist::OldRead(t4_byte *buf_, int len_) {
  d4_assert(_oldSeek >= 0);

  t4_i32 newSeek = _oldSeek + _oldCurr - _oldLimit;
  int n = _strategy.DataRead(newSeek, buf_, len_);
  d4_assert(n > 0);
  _oldSeek = newSeek + n;
  _oldCurr = _oldLimit = _oldBuf;
  return n;
}

c4_HandlerSeq *c4_Persist::Load(c4_Stream *stream_) {
  d4_assert(stream_ != 0);

  c4_FileMark head;
  if (stream_->Read(&head, sizeof head) != sizeof head || !head.IsHeader())
    return 0;
  // no data in file

  //_oldStyle = head._data[3] == 0x80;
  d4_assert(!head.IsOldHeader());

  t4_i32 limit = head.Offset();

  c4_StreamStrategy *strat = d4_new c4_StreamStrategy(limit);
  strat->_bytesFlipped = head.IsFlipped();
  strat->DataWrite(strat->FileSize() - strat->_baseOffset, &head, sizeof head);

  while (strat->FileSize() - strat->_baseOffset < limit) {
    char buffer[4096];
    int n = stream_->Read(buffer, sizeof buffer);
    d4_assert(n > 0);
    strat->DataWrite(strat->FileSize() - strat->_baseOffset, buffer, n);
  }

  c4_Persist *pers = d4_new c4_Persist(*strat, true, 0);
  c4_HandlerSeq *seq = d4_new c4_HandlerSeq(pers);
  seq->DefineRoot();
  pers->SetRoot(seq);

  c4_Column walk(pers);
  if (!pers->LoadIt(walk)) {
    seq->IncRef();
    seq->DecRef(); // a funny way to delete
    return 0;
  }

  c4_Bytes tempWalk;
  walk.FetchBytes(0, walk.ColSize(), tempWalk, true);

  const t4_byte *ptr = tempWalk.Contents();
  seq->Prepare(&ptr, true);
  d4_assert(ptr == tempWalk.Contents() + tempWalk.Size());

  return seq;
}

void c4_Persist::Save(c4_Stream *stream_, c4_HandlerSeq &root_) {
  d4_assert(stream_ != 0);

  c4_StreamStrategy strat(stream_);

  // 31-01-2002: streaming must adopt byte order of origin datafile
  c4_Persist *p = root_.Persist();
  if (p != 0)
    strat._bytesFlipped = p->Strategy()._bytesFlipped;

  c4_SaveContext ar(strat, true, 0, 0, 0);
  c4_Bytes tempWalk;
  ar.SaveIt(root_, 0, tempWalk);
}

t4_i32 c4_Persist::LookupAside(int id_) {
  d4_assert(_differ != 0);

  return _differ->BaseOfDiff(id_);
}

void c4_Persist::ApplyAside(int id_, c4_Column &col_) {
  d4_assert(_differ != 0);

  _differ->ApplyDiff(id_, col_);
}

void c4_Persist::OccupySpace(t4_i32 pos_, t4_i32 len_) {
  d4_assert(_mode != 1 || _space != 0);

  if (_space != 0)
    _space->Occupy(pos_, len_);
}

/////////////////////////////////////////////////////////////////////////////
