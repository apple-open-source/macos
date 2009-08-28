// remap.cpp --
// $Id: remap.cpp 1230 2007-03-09 15:58:53Z jcw $
// This is part of Metakit, the homepage is http://www.equi4.com/metakit.html

/** @file
 * Mapping and remapping custom viewers
 */

#include "header.h"
#include "remap.h"
#include "handler.h"

/////////////////////////////////////////////////////////////////////////////

class c4_ReadOnlyViewer: public c4_CustomViewer {
    c4_View _base;

  public:
    c4_ReadOnlyViewer(c4_Sequence &seq_): _base(&seq_){}
    virtual ~c4_ReadOnlyViewer(){}

    virtual c4_View GetTemplate() {
        return _base.Clone();
    }
    virtual int GetSize() {
        return _base.GetSize();
    }

    virtual int Lookup(c4_Cursor key_, int &count_) {
        int pos = 0;
        count_ = _base.GetSize();
        return _base.RestrictSearch(*key_, pos, count_);
    }

    virtual bool GetItem(int row_, int col_, c4_Bytes &buf_) {
        return _base.GetItem(row_, col_, buf_);
    }
};

/////////////////////////////////////////////////////////////////////////////

class c4_HashViewer: public c4_CustomViewer {
    c4_View _base;
    c4_View _map;
    int _numKeys;

    c4_IntProp _pHash;
    c4_IntProp _pRow;

    bool KeySame(int row_, c4_Cursor cursor_)const;
    t4_i32 CalcHash(c4_Cursor cursor_)const;
    int LookDict(t4_i32 hash_, c4_Cursor cursor_)const;
    void InsertDict(int row_);
    void RemoveDict(int pos_);
    bool DictResize(int minused);

    int Row(int i_)const {
        return _pRow(_map[i_]);
    }
    int Hash(int i_)const {
        return _pHash(_map[i_]);
    }

    void SetRow(int i_, int v_) {
        _pRow(_map[i_]) = v_;
    }
    void SetHash(int i_, int v_) {
        _pHash(_map[i_]) = v_;
    }

    bool IsUnused(int)const;
    bool IsDummy(int)const;
    bool IsActive(int i_)const {
        return Row(i_) >= 0;
    }

    int GetPoly()const;
    void SetPoly(int v_);
    int GetSpare()const;
    void SetSpare(int v_);

  public:
    c4_HashViewer(c4_Sequence &seq_, int numKeys_, c4_Sequence *map_ = 0);
    virtual ~c4_HashViewer();

    virtual c4_View GetTemplate();
    virtual int GetSize();
    virtual int Lookup(c4_Cursor key_, int &count_);
    virtual bool GetItem(int row_, int col_, c4_Bytes &buf_);
    virtual bool SetItem(int row_, int col_, const c4_Bytes &buf_);
    virtual bool InsertRows(int pos_, c4_Cursor value_, int count_ = 1);
    virtual bool RemoveRows(int pos_, int count_ = 1);
};

/////////////////////////////////////////////////////////////////////////////
// The following contains code derived froms Python's dictionaries, hence:
//  Copyright 1991-1995 by Stichting Mathematisch Centrum, Amsterdam,
//  The Netherlands.
// Reduced and turned into a fast C++ class by Christian Tismer, hence:
//  Copyright 1999 by Christian Tismer.
// Vectorized and reorganized further by Jean-Claude Wippler.
/////////////////////////////////////////////////////////////////////////////

//  Table of irreducible polynomials to efficiently cycle through
//  GF(2^n)-{0}, 2<=n<=30.

static long s_polys[] =  {
  4+3, 8+3, 16+3, 32+5, 64+3, 128+3, 256+29, 512+17, 1024+9, 2048+5, 4096+83,
    8192+27, 16384+43, 32768+3, 65536+45, 131072+9, 262144+39, 524288+39,
    1048576+9, 2097152+5, 4194304+3, 8388608+33, 16777216+27, 33554432+9,
    67108864+71, 134217728+39, 268435456+9, 536870912+5, 1073741824+83, 0
};

/////////////////////////////////////////////////////////////////////////////

c4_HashViewer::c4_HashViewer(c4_Sequence &seq_, int numKeys_, c4_Sequence *map_)
  : _base(&seq_), _map(map_), _numKeys(numKeys_), _pHash("_H"), _pRow("_R") {
  if (_map.GetSize() == 0)
    _map.SetSize(1);

  int poly = GetPoly();
  if (poly == 0 || _map.GetSize() <= _base.GetSize())
    DictResize(_base.GetSize());
}

c4_HashViewer::~c4_HashViewer(){}

bool c4_HashViewer::IsUnused(int row_)const {
  c4_RowRef r = _map[row_];
  return _pRow(r) < 0 && _pHash(r) == 0;
}

bool c4_HashViewer::IsDummy(int row_)const {
  c4_RowRef r = _map[row_];
  return _pRow(r) < 0 && _pHash(r) < 0;
}

int c4_HashViewer::GetPoly()const {
  return Hash(_map.GetSize() - 1);
}

void c4_HashViewer::SetPoly(int v_) {
  SetHash(_map.GetSize() - 1, v_);
}

int c4_HashViewer::GetSpare()const {
  return Row(_map.GetSize() - 1);
}

void c4_HashViewer::SetSpare(int v_) {
  SetRow(_map.GetSize() - 1, v_);
}

bool c4_HashViewer::KeySame(int row_, c4_Cursor cursor_)const {
  for (int i = 0; i < _numKeys; ++i) {
    c4_Bytes buffer;
    _base.GetItem(row_, i, buffer);

    c4_Handler &h = cursor_._seq->NthHandler(i);
    if (h.Compare(cursor_._index, buffer) != 0)
      return false;
  }

  return true;
}

/// Create mapped view which is uses a second view for hashing
t4_i32 c4_HashViewer::CalcHash(c4_Cursor cursor_)const {
  c4_Bytes buffer, buf2;
  const t4_i32 endian = 0x03020100;
  t4_i32 hash = 0;

  for (int i = 0; i < _numKeys; ++i) {
    c4_Handler &h = cursor_._seq->NthHandler(i);
    cursor_._seq->Get(cursor_._index, h.PropId(), buffer);

    // this code borrows from Python's stringobject.c/string_hash()
    int len = buffer.Size();
    if (len > 0) {
      const t4_byte *p = buffer.Contents();

      // 20030218: careful to avoid endian-ness sensitivity
      if (*(const t4_byte*) &endian)
      // true on big-endian systems
      switch (h.Property().Type()) {
        case 'I':
        case 'L':
        case 'F':
        case 'D':
           {
            t4_byte *q = buf2.SetBuffer(len);
            for (int j = 0; j < len; ++j)
              q[len - j - 1] = p[j];
            p = q;
          }
      }

      long x =  *p << 7;

      // modifications are risky, this code avoid scanning huge blobs
      if (len > 200)
        len = 100;

      while (--len >= 0)
        x = (1000003 *x) ^  *p++;

      if (buffer.Size() > 200) {
        len = 100;
        p += buffer.Size() - 200;
        while (--len >= 0)
          x = (1000003 *x) ^  *p++;
      }

      x ^= buffer.Size();
      hash ^= x ^ i;
    }
  }

  if (hash == 0)
    hash =  - 1;

  return hash;
}

/*
 * Types of slots:
 *  Unused: row = -1, hash = 0
 *  Dummy:  row = -1, hash = -1
 *  Active: row >= 0
 * There must be at least one Unused slot at all times.
 */

int c4_HashViewer::LookDict(t4_i32 hash_, c4_Cursor cursor_)const {
  const unsigned int mask = _map.GetSize() - 2;
  /* We must come up with (i, incr) such that 0 <= i < _size
  and 0 < incr < _size and both are a function of hash */
  int i = mask &~hash_;
  /* We use ~hash_ instead of hash_, as degenerate hash functions, such
  as for ints <sigh>, can have lots of leading zeros. It's not
  really a performance risk, but better safe than sorry. */
  if (IsUnused(i) || Hash(i) == hash_ && KeySame(Row(i), cursor_))
    return i;

  int freeslot = IsDummy(i) ? i :  - 1;

  /* Derive incr from hash_, just to make it more arbitrary. Note that
  incr must not be 0, or we will get into an infinite loop.*/
  unsigned incr = (hash_ ^ ((unsigned long)hash_ >> 3)) &mask;
  if (!incr)
    incr = mask;

  int poly = GetPoly();
  for (;;) {
    i = (i + incr) &mask;
    if (IsUnused(i))
      break;
    if (Hash(i) == hash_ && KeySame(Row(i), cursor_))
      return i;
    if (freeslot ==  - 1 && IsDummy(i))
      freeslot = i;
    /* Cycle through GF(2^n)-{0} */
    incr = incr << 1;
    if (incr > mask)
      incr ^= poly;
     /* This will implicitely clear the highest bit */
  }

  return freeslot !=  - 1 ? freeslot : i;
}

void c4_HashViewer::InsertDict(int row_) {
  c4_Cursor cursor = &_base[row_];

  t4_i32 hash = CalcHash(cursor);
  int i = LookDict(hash, cursor);

  if (IsDummy(i)) {
    int n = GetSpare();
    d4_assert(n > 0);
    SetSpare(n - 1);
  }

  SetHash(i, hash);
  SetRow(i, row_);
}

void c4_HashViewer::RemoveDict(int pos_) {
  c4_Cursor key = &_base[pos_];
  t4_i32 hash = CalcHash(key);
  int i = LookDict(hash, key);
  d4_assert(i >= 0);

  d4_assert(Row(i) == pos_);

  SetHash(i,  - 1);
  SetRow(i,  - 1);

  SetSpare(GetSpare() + 1);
}

bool c4_HashViewer::DictResize(int minused) {
  int i, newsize, newpoly;
  for (i = 0, newsize = 4;; i++, newsize <<= 1) {
    if (s_polys[i] == 0)
      return false;
    else if (newsize > minused) {
      newpoly = s_polys[i];
      break;
    }
  }

  _map.SetSize(0);

  c4_Row empty;
  _pRow(empty) =  - 1;
  _map.InsertAt(0, empty, newsize + 1);

  SetPoly(newpoly);
  SetSpare(0);

  for (int j = 0; j < _base.GetSize(); ++j)
    InsertDict(j);

  return true;
}

c4_View c4_HashViewer::GetTemplate() {
  return _base.Clone();
}

int c4_HashViewer::GetSize() {
  return _base.GetSize();
}

int c4_HashViewer::Lookup(c4_Cursor key_, int &count_) {
  // can only use hashing if the properties match the query
  // XXX it appears that this loop takes some 300 uS!
  c4_View kv = (*key_).Container();
  for (int k = 0; k < _numKeys; ++k)
    if (kv.FindProperty(_base.NthProperty(k).GetId()) < 0)
      return  - 1;

  t4_i32 hash = CalcHash(key_); // TODO should combine with above loop
  int i = LookDict(hash, key_);

  int row = Row(i);
  count_ = row >= 0 && KeySame(row, key_) ? 1 : 0;
  return count_ ? row : 0; // don't return -1, we *know* it's not there
}

bool c4_HashViewer::GetItem(int row_, int col_, c4_Bytes &buf_) {
  return _base.GetItem(row_, col_, buf_);
}

bool c4_HashViewer::SetItem(int row_, int col_, const c4_Bytes &buf_) {
  if (col_ < _numKeys) {
    c4_Bytes temp;
    _base.GetItem(row_, col_, temp);
    if (buf_ == temp)
      return true;
    // this call will have no effect, just ignore it

    RemoveDict(row_);
  }

  _base.SetItem(row_, col_, buf_);

  if (col_ < _numKeys) {
    // careful if changing a key to one which is already present:
    // in that case, delete the other row to preserve uniqueness
    //
    // Note: this is a tricky and confusing issue, because now the
    // mere act of *setting* a property value can *delete* a row!
    //
    // The big problem here is that setting the rest of the values
    // in a loop can end up *wrong*, if the row has moved down!!!
    int n;
    int i = Lookup(&_base[row_], n);
    if (i >= 0 && n > 0) {
      RemoveRows(i, 1);
      if (i < row_)
        --row_;
    }

    InsertDict(row_);
  }

  return true;
}

bool c4_HashViewer::InsertRows(int pos_, c4_Cursor value_, int count_) {
  d4_assert(count_ > 0);

  int n;
  int i = Lookup(value_, n);
  if (i >= 0 && n > 0) {
    _base.SetAt(i,  *value_); // replace existing
    return true;
  }

  // adjust row numbers if the insertion is not at the end
  //
  // TODO this could be optimized to go through the rows which
  // were moved up, and then adjusting the map through a lookup
  // (probably better than full scan if pos_ is relatively high)
  if (pos_ < _base.GetSize()) {
    for (int r = 0; r < _map.GetSize() - 1; ++r) {
      int n2 = Row(r);
      if (n2 >= pos_)
        SetRow(r, n2 + 1);
    }
  }

  _base.InsertAt(pos_,  *value_);
  InsertDict(pos_);

  int used = _base.GetSize();
  int fill = used + GetSpare();
  if (fill *3 >= (_map.GetSize() - 1) *2 && !DictResize(used *2))
    return false;
  // bail out

  d4_assert(_base.GetSize() + GetSpare() < _map.GetSize() - 1);
  return true;
}

bool c4_HashViewer::RemoveRows(int pos_, int count_) {
  while (--count_ >= 0) {
    // since the map persists, be somewhat more aggressive than the
    // original code in resizing down when the map is getting empty
    if (_base.GetSize() *3 < _map.GetSize() - 1 && !DictResize(_base.GetSize()))
      return false;
    // bail out

    RemoveDict(pos_);

    // move rows down for now
    //
    // optionally: consider replacing with last entry (much faster)
    for (int r = 0; r < _map.GetSize() - 1; ++r) {
      int n = Row(r);
      if (n > pos_)
        SetRow(r, n - 1);
    }

    _base.RemoveAt(pos_, 1);
  }

  return true;
}

/////////////////////////////////////////////////////////////////////////////

class c4_BlockedViewer: public c4_CustomViewer {
    enum {
        kLimit = 1000
    };

    c4_View _base;

    c4_ViewProp _pBlock;
    c4_DWordArray _offsets;

    int _last_base, _last_limit, _last_slot;
    c4_View _last_view;

    int Slot(int &pos_);
    void Split(int block_, int row_);
    void Merge(int block_);
    void Validate()const;

    // 2004-04-27 new cache logic, thx MB
    void SetLast(int row_);
    void ClearLast(int slot_) {
        if (_last_slot >= slot_) {
            _last_limit = _last_slot =  - 1;
            _last_view = c4_View();
        }
    }

  public:
    c4_BlockedViewer(c4_Sequence &seq_);
    virtual ~c4_BlockedViewer();

    virtual c4_View GetTemplate();
    virtual int GetSize();
    virtual bool GetItem(int row_, int col_, c4_Bytes &buf_);
    virtual bool SetItem(int row_, int col_, const c4_Bytes &buf_);
    virtual bool InsertRows(int pos_, c4_Cursor value_, int count_ = 1);
    virtual bool RemoveRows(int pos_, int count_ = 1);
};

/////////////////////////////////////////////////////////////////////////////

#if q4_CHECK

// debugging version to verify that the internal data is consistent
void c4_BlockedViewer::Validate()const {
  d4_assert(_base.GetSize() >= 2);

  int n = _base.GetSize() - 1;
  d4_assert(_offsets.GetSize() == n);

  int total = 0;
  for (int i = 0; i < n; i++) {
    c4_View bv = _pBlock(_base[i]);
    d4_assert(bv.GetSize() > 0 || i == 0);
    total += bv.GetSize();
    d4_assert((int)_offsets.GetAt(i) == total++);
  }

  c4_View be = _pBlock(_base[n]);
  d4_assert(be.GetSize() == n - 1);
}

#else 

// nothing, so inline this thing to avoid even the calling overhead
d4_inline void c4_BlockedViewer::Validate()const{}

#endif 

c4_BlockedViewer::c4_BlockedViewer(c4_Sequence &seq_): _base(&seq_), _pBlock(
  "_B"), _last_base( - 1), _last_limit( - 1), _last_slot( - 1) {
  if (_base.GetSize() < 2)
    _base.SetSize(2);

  int n = _base.GetSize() - 1;
  _offsets.SetSize(n);

  int total = 0;
  for (int i = 0; i < n; i++) {
    c4_View bv = _pBlock(_base[i]);
    total += bv.GetSize();
    _offsets.SetAt(i, total++);
  }
  Validate();
}

c4_BlockedViewer::~c4_BlockedViewer(){}

int c4_BlockedViewer::Slot(int &pos_) {
  const int n = _offsets.GetSize();

  d4_assert(n > 0);
  d4_assert(pos_ <= (int)_offsets.GetAt(n - 1));

#if 0
  // not sure the following adds any performance (the logic looks ok)
  // reason: won't work if inserts/removes always invalidate the cache
  if (_last_base <= pos_ && pos_ < _last_limit) {
    pos_ -= _last_base;
    return _last_slot;
  }
#endif 

#if 0
  int h;
  for (h = 0; h < n; ++h)
    if (pos_ <= (t4_i32)_offsets.GetAt(h))
      break;
#else 
  // switch to binary search, adapted from code by Zhang Dehua, 28-3-2002
  // slows down some 5%, but said to be much better with 5 million rows
  int l = 0, h = n - 1;
  while (l < h) {
    int m = l + (h - l) / 2;
    if ((t4_i32)_offsets.GetAt(m) < pos_)
      l = m + 1;
    else
      h = m;
  }
#endif 

  if (h > 0)
    pos_ -= _offsets.GetAt(h - 1) + 1;

  return h;
}

void c4_BlockedViewer::Split(int bno_, int row_) {
  ClearLast(bno_);

  int z = _offsets.GetSize();
  d4_assert(bno_ < z);
  c4_View bz = _pBlock(_base[z]);
  c4_View bv = _pBlock(_base[bno_]);
  d4_assert(row_ < bv.GetSize());

  _offsets.InsertAt(bno_, _offsets.GetAt(bno_) - bv.GetSize() + row_);

  _base.InsertAt(bno_ + 1, c4_Row());
  c4_View bn = _pBlock(_base[bno_ + 1]);

  bv.RelocateRows(row_ + 1,  - 1, bn, 0);
  bv.RelocateRows(row_, 1, bz, bno_);

  Validate();
}

void c4_BlockedViewer::Merge(int bno_) {
  ClearLast(bno_);

  int z = _offsets.GetSize();
  c4_View bz = _pBlock(_base[z]);
  c4_View bv1 = _pBlock(_base[bno_]);
  c4_View bv2 = _pBlock(_base[bno_ + 1]);

  _offsets.RemoveAt(bno_);

  bz.RelocateRows(bno_, 1, bv1,  - 1);
  bv2.RelocateRows(0,  - 1, bv1,  - 1);

  _base.RemoveAt(bno_ + 1);

  Validate();
}

c4_View c4_BlockedViewer::GetTemplate() {
  c4_View bv = _pBlock(_base[0]);
  return bv.Clone();
}

int c4_BlockedViewer::GetSize() {
  int n = _offsets.GetAt(_offsets.GetSize() - 1);
  d4_assert(n >= 0);
  return n;
}

void c4_BlockedViewer::SetLast(int row_) {
  int orig = row_;

  int i = Slot(row_);
  d4_assert(0 <= i && i < _offsets.GetSize());

  _last_limit = _offsets.GetAt(i);

  if (_last_limit == orig) {
    row_ = i;
    i = _offsets.GetSize();
    _last_limit = 0; // force miss next time, but view is still cached
  }

  if (i != _last_slot) {
    _last_slot = i;
    _last_view = _pBlock(_base[i]);
  }

  _last_base = orig - row_;
}

bool c4_BlockedViewer::GetItem(int row_, int col_, c4_Bytes &buf_) {
  if (row_ < _last_base || row_ >= _last_limit)
    SetLast(row_);
  return _last_view.GetItem(row_ - _last_base, col_, buf_);
}

bool c4_BlockedViewer::SetItem(int row_, int col_, const c4_Bytes &buf_) {
  if (row_ < _last_base || row_ >= _last_limit)
    SetLast(row_);
  _last_view.SetItem(row_ - _last_base, col_, buf_);
  return true;
}

bool c4_BlockedViewer::InsertRows(int pos_, c4_Cursor value_, int count_) {
  d4_assert(count_ > 0);

  bool atEnd = pos_ == GetSize();

  int z = _offsets.GetSize();
  int i = Slot(pos_);
  d4_assert(0 <= i && i < z);

  ClearLast(i);

  c4_View bv = _pBlock(_base[i]);
  d4_assert(0 <= pos_ && pos_ <= bv.GetSize());

  bv.InsertAt(pos_,  *value_, count_);
  for (int j = i; j < z; ++j)
    _offsets.SetAt(j, _offsets.GetAt(j) + count_);

  // massive insertions are first split off
  while (bv.GetSize() >= 2 *kLimit)
    Split(i, bv.GetSize() - kLimit - 2);

  if (bv.GetSize() > kLimit)
    Split(i, atEnd ? kLimit - 1: bv.GetSize() / 2);
  // 23-3-2002, from MB

  Validate();

  return true;
}

bool c4_BlockedViewer::RemoveRows(int pos_, int count_) {
  d4_assert(count_ > 0);
  d4_assert(pos_ + count_ <= GetSize());

  int z = _offsets.GetSize();
  int i = Slot(pos_);
  d4_assert(0 <= i && i < z);

  ClearLast(i);

  c4_View bv = _pBlock(_base[i]);
  d4_assert(0 <= pos_ && pos_ <= bv.GetSize());

  int todo = count_;

  // optimize if the deletion goes past the end of this block...
  int overshoot = pos_ + count_ - bv.GetSize();
  if (overshoot > 0) {

    // first, delete blocks which are going away completely
    while (i + 1 < _offsets.GetSize()) {
      int nextsize = _offsets.GetAt(i + 1) - _offsets.GetAt(i);
      if (overshoot < nextsize)
        break;
      todo -= nextsize;
      overshoot -= nextsize;

      // drop the block and forget it ever existed
      for (int j = i + 1; j < z; ++j)
        _offsets.SetAt(j, _offsets.GetAt(j) - nextsize);
      _offsets.RemoveAt(i + 1);

      _base.RemoveAt(i + 1);
      --z;
      c4_View bz = _pBlock(_base[z]);
      bz.RemoveAt(i);

      Validate();
    }

    // delete before merging, to avoid useless copying
    if (overshoot > 1) {
      c4_View bv2 = _pBlock(_base[i + 1]);
      bv2.RemoveAt(0, overshoot - 1);
      todo -= overshoot - 1;

      for (int j = i + 1; j < z; ++j)
        _offsets.SetAt(j, _offsets.GetAt(j) - (overshoot - 1));

      // if the next block is filled enough, rotate the separator
      // this avoids an expensive and unnecessary merge + split
      if (bv2.GetSize() > kLimit / 2) {
        c4_View bz = _pBlock(_base[z]);
        bz[i] = bv2[0];
        bv2.RemoveAt(0);
        --todo;
        d4_assert(pos_ + todo <= bv.GetSize());
        d4_assert(i < _offsets.GetSize());

        for (int j = i + 1; j < z; ++j)
          _offsets.SetAt(j, _offsets.GetAt(j) - 1);
      }
    }

    // merge into one block
    if (pos_ + todo > bv.GetSize()) {
      d4_assert(i < z - 1);
      Merge(i);
      --z;
    }
  }
  d4_assert(pos_ + todo <= bv.GetSize());

  // now remove the rows and adjust offsets
  if (todo > 0)
    bv.RemoveAt(pos_, todo);

  for (int j = i; j < z; ++j)
    _offsets.SetAt(j, _offsets.GetAt(j) - todo);

  // if the block underflows, merge it
  if (bv.GetSize() < kLimit / 2) {
    if (i > 0)
    // merge with predecessor, preferably
      bv = _pBlock(_base[--i]);
    if (i >= z - 1)
    // unless there is no successor to merge with
      return true;
    Merge(i);
  }

  // if the block overflows, split it
  if (bv.GetSize() > kLimit)
    Split(i, bv.GetSize() / 2);

  Validate();

  return true;
}

/////////////////////////////////////////////////////////////////////////////

class c4_OrderedViewer: public c4_CustomViewer {
    c4_View _base;
    int _numKeys;

    int KeyCompare(int row_, c4_Cursor cursor_)const;

  public:
    c4_OrderedViewer(c4_Sequence &seq_, int numKeys_);
    virtual ~c4_OrderedViewer();

    virtual c4_View GetTemplate();
    virtual int GetSize();
    virtual int Lookup(c4_Cursor key_, int &count_);
    virtual bool GetItem(int row_, int col_, c4_Bytes &buf_);
    virtual bool SetItem(int row_, int col_, const c4_Bytes &buf_);
    virtual bool InsertRows(int pos_, c4_Cursor value_, int count_ = 1);
    virtual bool RemoveRows(int pos_, int count_ = 1);
};

/////////////////////////////////////////////////////////////////////////////

c4_OrderedViewer::c4_OrderedViewer(c4_Sequence &seq_, int numKeys_): _base
  (&seq_), _numKeys(numKeys_){}

c4_OrderedViewer::~c4_OrderedViewer(){}

int c4_OrderedViewer::KeyCompare(int row_, c4_Cursor cursor_)const {
  for (int i = 0; i < _numKeys; ++i) {
    c4_Bytes buffer;
    _base.GetItem(row_, i, buffer);

    c4_Handler &h = cursor_._seq->NthHandler(i);
    int f = h.Compare(cursor_._index, buffer);
    if (f != 0)
      return f;
  }

  return 0;
}

c4_View c4_OrderedViewer::GetTemplate() {
  return _base.Clone();
}

int c4_OrderedViewer::GetSize() {
  return _base.GetSize();
}

int c4_OrderedViewer::Lookup(c4_Cursor key_, int &count_) {
  // can only use bsearch if the properties match the query
  // XXX with ord1.tcl (dict words), this loop takes 300 uS!
  c4_View kv = (*key_).Container();
  for (int k = 0; k < _numKeys; ++k)
    if (kv.FindProperty(_base.NthProperty(k).GetId()) < 0)
      return  - 1;

#if 0 // Locate gets the count wrong, it seems 2000-06-15
  int pos;
  count_ = _base.Locate(*key_, &pos);
#else 
  int pos = _base.Search(*key_);
  count_ = pos < _base.GetSize() && KeyCompare(pos, key_) == 0 ? 1 : 0;
#endif 
  return pos;
}

bool c4_OrderedViewer::GetItem(int row_, int col_, c4_Bytes &buf_) {
  return _base.GetItem(row_, col_, buf_);
}

bool c4_OrderedViewer::SetItem(int row_, int col_, const c4_Bytes &buf_) {
  if (col_ < _numKeys) {
    c4_Bytes temp;
    _base.GetItem(row_, col_, temp);
    if (buf_ == temp)
      return true;
    // this call will have no effect, just ignore it
  }

  _base.SetItem(row_, col_, buf_);

  if (col_ < _numKeys) {
    c4_Row copy = _base[row_];
    // have to remove the row because it messes up searching
    // it would be more efficient to search *around* this row
    // or perhaps figure out new pos before changing any data
    RemoveRows(row_);
    InsertRows(0, &copy); // position is ignored
  }

  return true;
}

bool c4_OrderedViewer::InsertRows(int, c4_Cursor value_, int count_) {
  d4_assert(count_ > 0);

  int n;
  int i = Lookup(value_, n);

  // XXX if the lookup does not work, then insert as first element (!?)
  d4_assert(i >= 0);
  if (i < 0)
    i = 0;

  if (n == 0)
    _base.InsertAt(i,  *value_);
  else {
    d4_assert(i < _base.GetSize());
    _base.SetAt(i,  *value_); // replace existing
  }

  return true;
}

bool c4_OrderedViewer::RemoveRows(int pos_, int count_) {
  _base.RemoveAt(pos_, count_);
  return true;
}

/////////////////////////////////////////////////////////////////////////////

class c4_IndexedViewer: public c4_CustomViewer {
    c4_View _base;
    c4_View _map;
    c4_View _props;
    bool _unique;
    c4_IntProp _mapProp;

    int KeyCompare(int row_, c4_Cursor cursor_)const;

  public:
    c4_IndexedViewer(c4_Sequence &seq_, c4_Sequence &map_, const c4_View
      &props_, bool unique_);
    virtual ~c4_IndexedViewer();

    virtual c4_View GetTemplate();
    virtual int GetSize();
    virtual int Lookup(c4_Cursor key_, int &count_);
    virtual bool GetItem(int row_, int col_, c4_Bytes &buf_);
    virtual bool SetItem(int row_, int col_, const c4_Bytes &buf_);
    virtual bool InsertRows(int pos_, c4_Cursor value_, int count_ = 1);
    virtual bool RemoveRows(int pos_, int count_ = 1);
};

/////////////////////////////////////////////////////////////////////////////

c4_IndexedViewer::c4_IndexedViewer(c4_Sequence &seq_, c4_Sequence &map_, const
  c4_View &props_, bool unique_): _base(&seq_), _map(&map_), _props(props_),
  _unique(unique_), _mapProp((const c4_IntProp &)_map.NthProperty(0)) {
  int n = _base.GetSize();
  if (_map.GetSize() != n) {
    c4_View sorted = _base.SortOn(_props);

    _map.SetSize(n);
    for (int i = 0; i < n; ++i)
      _mapProp(_map[i]) = _base.GetIndexOf(sorted[i]);
  }
}

c4_IndexedViewer::~c4_IndexedViewer(){}

int c4_IndexedViewer::KeyCompare(int row_, c4_Cursor cursor_)const {
  int n = _props.NumProperties();
  for (int i = 0; i < n; ++i) {
    c4_Bytes buffer;
    _base.GetItem(row_, i, buffer);

    c4_Handler &h = cursor_._seq->NthHandler(i);
    int f = h.Compare(cursor_._index, buffer);
    if (f != 0)
      return f;
  }

  return 0;
}

c4_View c4_IndexedViewer::GetTemplate() {
  return _base.Clone();
}

int c4_IndexedViewer::GetSize() {
  return _base.GetSize();
}

int c4_IndexedViewer::Lookup(c4_Cursor key_, int &count_) {
  // can only use bsearch if the properties match the query
  // XXX with ord1.tcl (dict words), this loop takes 300 uS!
  c4_View kv = (*key_).Container();
  int n = _props.NumProperties();
  for (int k = 0; k < n; ++k)
    if (kv.FindProperty(_props.NthProperty(k).GetId()) < 0)
      return  - 1;

#if 0 // Locate gets the count wrong, it seems 2000-06-15
  int pos;
  count_ = _base.Locate(*key_, &pos);
#else 
  int pos = _base.Search(*key_);
  count_ = pos < _base.GetSize() && KeyCompare(pos, key_) == 0 ? 1 : 0;
#endif 
  return pos;
}

bool c4_IndexedViewer::GetItem(int row_, int col_, c4_Bytes &buf_) {
  return _base.GetItem(row_, col_, buf_);
}

bool c4_IndexedViewer::SetItem(int row_, int col_, const c4_Bytes &buf_) {
  const int id = _base.NthProperty(col_).GetId();
  const bool keyMod = _props.FindProperty(id) >= 0;

  if (keyMod) {
    c4_Bytes temp;
    _base.GetItem(row_, col_, temp);
    if (buf_ == temp)
      return true;
    // this call will have no effect, just ignore it
  }

  _base.SetItem(row_, col_, buf_);

  if (keyMod) {
    // TODO adjust index
  }

  return true;
}

bool c4_IndexedViewer::InsertRows(int, c4_Cursor value_, int count_) {
  d4_assert(count_ > 0);

  if (_unique)
    count_ = 1;

  int n;
  int i = Lookup(value_, n);

  // XXX if the lookup does not work, then insert as first element (!?)
  d4_assert(i >= 0);
  if (i < 0)
    i = 0;

  if (n == 0)
    _base.InsertAt(i,  *value_);
  else {
    d4_assert(i < _base.GetSize());
    _base.SetAt(i,  *value_); // replace existing
  }

  return true;
}

bool c4_IndexedViewer::RemoveRows(int pos_, int count_) {
  _base.RemoveAt(pos_, count_);

  int n = _map.GetSize();
  while (--n >= 0) {
    int v = _mapProp(_map[n]);
    if (v >= pos_)
      if (v < pos_ + count_)
        _map.RemoveAt(n);
      else
        _mapProp(_map[n]) = v - count_;
  }

  return true;
}

/////////////////////////////////////////////////////////////////////////////

c4_CustomViewer *f4_CreateReadOnly(c4_Sequence &seq_) {
  return d4_new c4_ReadOnlyViewer(seq_);
}

c4_CustomViewer *f4_CreateHash(c4_Sequence &seq_, int nk_, c4_Sequence *map_) {
  return d4_new c4_HashViewer(seq_, nk_, map_);
}

c4_CustomViewer *f4_CreateBlocked(c4_Sequence &seq_) {
  return d4_new c4_BlockedViewer(seq_);
}

c4_CustomViewer *f4_CreateOrdered(c4_Sequence &seq_, int nk_) {
  return d4_new c4_OrderedViewer(seq_, nk_);
}

c4_CustomViewer *f4_CreateIndexed(c4_Sequence &seq_, c4_Sequence &map_, const
  c4_View &props_, bool unique_) {
  return d4_new c4_IndexedViewer(seq_, map_, props_, unique_);
}

/////////////////////////////////////////////////////////////////////////////
