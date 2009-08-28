// format.cpp --
// $Id: format.cpp 1230 2007-03-09 15:58:53Z jcw $
// This is part of Metakit, the homepage is http://www.equi4.com/metakit.html

/** @file
 * Format handlers deal with the representation of data
 */

#include "header.h"
#include "handler.h"
#include "column.h"
#include "format.h"
#include "persist.h"

/////////////////////////////////////////////////////////////////////////////

class c4_FormatHandler: public c4_Handler {
    c4_HandlerSeq &_owner;

  public:
    c4_FormatHandler(const c4_Property &prop_, c4_HandlerSeq &owner_);
    virtual ~c4_FormatHandler();

    virtual bool IsPersistent()const;

  protected:
    c4_HandlerSeq &Owner()const;
};

/////////////////////////////////////////////////////////////////////////////
// c4_FormatHandler

c4_FormatHandler::c4_FormatHandler(const c4_Property &prop_, c4_HandlerSeq
  &owner_): c4_Handler(prop_), _owner(owner_){}

c4_FormatHandler::~c4_FormatHandler(){}

d4_inline c4_HandlerSeq &c4_FormatHandler::Owner()const {
  return _owner;
}

bool c4_FormatHandler::IsPersistent()const {
  return _owner.Persist() != 0;
}

/////////////////////////////////////////////////////////////////////////////

class c4_FormatX: public c4_FormatHandler {
  public:
    c4_FormatX(const c4_Property &prop_, c4_HandlerSeq &seq_, int width_ =
      sizeof(t4_i32));

    virtual void Define(int, const t4_byte **);
    virtual void OldDefine(char type_, c4_Persist &);
    virtual void FlipBytes();

    virtual int ItemSize(int index_);
    virtual const void *Get(int index_, int &length_);
    virtual void Set(int index_, const c4_Bytes &buf_);

    virtual void Insert(int index_, const c4_Bytes &buf_, int count_);
    virtual void Remove(int index_, int count_);

    virtual void Commit(c4_SaveContext &ar_);

    virtual void Unmapped();

    static int DoCompare(const c4_Bytes &b1_, const c4_Bytes &b2_);

  protected:
    c4_ColOfInts _data;
};

/////////////////////////////////////////////////////////////////////////////

c4_FormatX::c4_FormatX(const c4_Property &p_, c4_HandlerSeq &s_, int w_):
  c4_FormatHandler(p_, s_), _data(s_.Persist(), w_){}

int c4_FormatX::DoCompare(const c4_Bytes &b1_, const c4_Bytes &b2_) {
  return c4_ColOfInts::DoCompare(b1_, b2_);
}

void c4_FormatX::Commit(c4_SaveContext &ar_) {
  _data.FixSize(true);
  ar_.CommitColumn(_data);
  //_data.FixSize(false);
}

void c4_FormatX::Define(int rows_, const t4_byte **ptr_) {
  if (ptr_ != 0)
    _data.PullLocation(*ptr_);

  _data.SetRowCount(rows_);
}

void c4_FormatX::OldDefine(char, c4_Persist &pers_) {
  pers_.FetchOldLocation(_data);
  _data.SetRowCount(Owner().NumRows());
}

void c4_FormatX::FlipBytes() {
  _data.FlipBytes();
}

int c4_FormatX::ItemSize(int index_) {
  return _data.ItemSize(index_);
}

const void *c4_FormatX::Get(int index_, int &length_) {
  return _data.Get(index_, length_);
}

void c4_FormatX::Set(int index_, const c4_Bytes &buf_) {
  _data.Set(index_, buf_);
}

void c4_FormatX::Insert(int index_, const c4_Bytes &buf_, int count_) {
  _data.Insert(index_, buf_, count_);
}

void c4_FormatX::Remove(int index_, int count_) {
  _data.Remove(index_, count_);
}

void c4_FormatX::Unmapped() {
  _data.ReleaseAllSegments();
}

/////////////////////////////////////////////////////////////////////////////
#if !q4_TINY
/////////////////////////////////////////////////////////////////////////////

class c4_FormatL: public c4_FormatX {
  public:
    c4_FormatL(const c4_Property &prop_, c4_HandlerSeq &seq_);

    virtual void Define(int, const t4_byte **);

    static int DoCompare(const c4_Bytes &b1_, const c4_Bytes &b2_);
};

/////////////////////////////////////////////////////////////////////////////

c4_FormatL::c4_FormatL(const c4_Property &prop_, c4_HandlerSeq &seq_):
  c4_FormatX(prop_, seq_, sizeof(t4_i64)) {
  // force maximum size, autosizing more than 32 bits won't work
  _data.SetAccessWidth(8 *sizeof(t4_i64));
}

int c4_FormatL::DoCompare(const c4_Bytes &b1_, const c4_Bytes &b2_) {
  d4_assert(b1_.Size() == sizeof(t4_i64));
  d4_assert(b2_.Size() == sizeof(t4_i64));

  t4_i64 v1 = *(const t4_i64*)b1_.Contents();
  t4_i64 v2 = *(const t4_i64*)b2_.Contents();

  return v1 == v2 ? 0 : v1 < v2 ?  - 1:  + 1;
}

void c4_FormatL::Define(int rows_, const t4_byte **ptr_) {
  if (ptr_ == 0 && rows_ > 0) {
    d4_assert(_data.ColSize() == 0);
    _data.InsertData(0, rows_ *sizeof(t4_i64), true);
  }

  c4_FormatX::Define(rows_, ptr_);
}

/////////////////////////////////////////////////////////////////////////////

class c4_FormatF: public c4_FormatX {
  public:
    c4_FormatF(const c4_Property &prop_, c4_HandlerSeq &seq_);

    static int DoCompare(const c4_Bytes &b1_, const c4_Bytes &b2_);
};

/////////////////////////////////////////////////////////////////////////////

c4_FormatF::c4_FormatF(const c4_Property &prop_, c4_HandlerSeq &seq_):
  c4_FormatX(prop_, seq_, sizeof(float)){}

int c4_FormatF::DoCompare(const c4_Bytes &b1_, const c4_Bytes &b2_) {
  d4_assert(b1_.Size() == sizeof(float));
  d4_assert(b2_.Size() == sizeof(float));

  float v1 = *(const float*)b1_.Contents();
  float v2 = *(const float*)b2_.Contents();

  return v1 == v2 ? 0 : v1 < v2 ?  - 1:  + 1;
}

/////////////////////////////////////////////////////////////////////////////

class c4_FormatD: public c4_FormatX {
  public:
    c4_FormatD(const c4_Property &prop_, c4_HandlerSeq &seq_);

    virtual void Define(int, const t4_byte **);

    static int DoCompare(const c4_Bytes &b1_, const c4_Bytes &b2_);
};

/////////////////////////////////////////////////////////////////////////////

c4_FormatD::c4_FormatD(const c4_Property &prop_, c4_HandlerSeq &seq_):
  c4_FormatX(prop_, seq_, sizeof(double)) {
  // force maximum size, autosizing more than 32 bits won't work
  _data.SetAccessWidth(8 *sizeof(double));
}

int c4_FormatD::DoCompare(const c4_Bytes &b1_, const c4_Bytes &b2_) {
  d4_assert(b1_.Size() == sizeof(double));
  d4_assert(b2_.Size() == sizeof(double));

  double v1 = *(const double*)b1_.Contents();
  double v2 = *(const double*)b2_.Contents();

  return v1 == v2 ? 0 : v1 < v2 ?  - 1:  + 1;
}

void c4_FormatD::Define(int rows_, const t4_byte **ptr_) {
  if (ptr_ == 0 && rows_ > 0) {
    d4_assert(_data.ColSize() == 0);
    _data.InsertData(0, rows_ *sizeof(double), true);
  }

  c4_FormatX::Define(rows_, ptr_);
}

/////////////////////////////////////////////////////////////////////////////
#endif // !q4_TINY
/////////////////////////////////////////////////////////////////////////////

/*
Byte properties are used for raw bytes and for indirect (memo) data.

There are two columns: the actual data and the item sizes.  If the data
is indirect, then the size is stored as a negative value.

In addition, there is an in-memory-only vector of columns (_memos).
Columns are created when asked for, and stay around until released with
a commit call.  If the column object exists and is not dirty, then it
is either a real column (size < 0), or simply a duplicate of the data
stored inline as bytes.
 */

class c4_FormatB: public c4_FormatHandler {
  public:
    c4_FormatB(const c4_Property &prop_, c4_HandlerSeq &seq_);
    virtual ~c4_FormatB();

    virtual void Define(int, const t4_byte **);
    virtual void OldDefine(char type_, c4_Persist &);
    virtual void Commit(c4_SaveContext &ar_);

    virtual int ItemSize(int index_);
    virtual const void *Get(int index_, int &length_);
    virtual void Set(int index_, const c4_Bytes &buf_);

    virtual void Insert(int index_, const c4_Bytes &buf_, int count_);
    virtual void Remove(int index_, int count_);

    virtual c4_Column *GetNthMemoCol(int index_, bool alloc_);

    virtual void Unmapped();

    static int DoCompare(const c4_Bytes &b1_, const c4_Bytes &b2_);

  protected:
    const void *GetOne(int index_, int &length_);
    void SetOne(int index_, const c4_Bytes &buf_, bool ignoreMemos_ = false);

  private:
    t4_i32 Offset(int index_)const;
    bool ShouldBeMemo(int length_)const;
    int ItemLenOffCol(int index_, t4_i32 &off_, c4_Column * &col_);
    bool CommitItem(c4_SaveContext &ar_, int index_);
    void InitOffsets(c4_ColOfInts &sizes_);

    c4_Column _data;
    c4_ColOfInts _sizeCol; // 2001-11-27: keep, to track position on disk
    c4_Column _memoCol; // 2001-11-27: keep, to track position on disk
    c4_DWordArray _offsets;
    c4_PtrArray _memos;
    bool _recalc; // 2001-11-27: remember when to redo _{size,memo}Col
};

/////////////////////////////////////////////////////////////////////////////

c4_FormatB::c4_FormatB(const c4_Property &prop_, c4_HandlerSeq &seq_):
  c4_FormatHandler(prop_, seq_), _data(seq_.Persist()), _sizeCol(seq_.Persist())
  , _memoCol(seq_.Persist()), _recalc(false) {
  _offsets.SetSize(1, 100);
  _offsets.SetAt(0, 0);
}

c4_FormatB::~c4_FormatB() {
  // cleanup allocated columns
  //better? for (int i = _memos.GetSize(); --i >= 0 ;)
  for (int i = 0; i < _memos.GetSize(); ++i)
    delete (c4_Column*)_memos.GetAt(i);
}

d4_inline t4_i32 c4_FormatB::Offset(int index_)const {
  d4_assert((t4_i32)_offsets.GetAt(_offsets.GetSize() - 1) == _data.ColSize());
  d4_assert(_offsets.GetSize() == _memos.GetSize() + 1);
  d4_assert(index_ < _offsets.GetSize());

  // extend offset vectors for missing empty entries at end 
  int n = _offsets.GetSize();
  d4_assert(n > 0);

  if (index_ >= n)
    index_ = n - 1;

  return _offsets.GetAt(index_);
}

d4_inline bool c4_FormatB::ShouldBeMemo(int length_)const {
  // items over 10000 bytes are always memos
  // items up to 100 bytes are never memos
  //
  // else, memo only if the column would be under 1 Mb
  // (assuming all items had the same size as this one)
  //
  // the effect is that as the number of rows increases,
  // smaller and smaller items get turned into memos
  //
  // note that items which are no memo right now stay
  // as is, and so do memos which have not been modified

  int rows = _memos.GetSize() + 1; // avoids divide by zero
  return length_ > 10000 || length_ > 100 && length_ > 1000000 / rows;
}

int c4_FormatB::ItemLenOffCol(int index_, t4_i32 &off_, c4_Column * &col_) {
  col_ = (c4_Column*)_memos.GetAt(index_);
  if (col_ != 0) {
    off_ = 0;
    return col_->ColSize();
  }

  col_ = &_data;
  off_ = Offset(index_);
  return Offset(index_ + 1) - off_;
}

c4_Column *c4_FormatB::GetNthMemoCol(int index_, bool alloc_) {
  t4_i32 start;
  c4_Column *col;
  int n = ItemLenOffCol(index_, start, col);

  if (col ==  &_data && alloc_) {
    col = d4_new c4_Column(_data.Persist());
    _memos.SetAt(index_, col);

    if (n > 0)
    if (_data.IsDirty()) {
      c4_Bytes temp;
      _data.FetchBytes(start, n, temp, true);
      col->SetBuffer(n);
      col->StoreBytes(0, temp);
    } else
      col->SetLocation(_data.Position() + start, n);
  }

  return col;
}

void c4_FormatB::Unmapped() {
  _data.ReleaseAllSegments();
  _sizeCol.ReleaseAllSegments();
  _memoCol.ReleaseAllSegments();

  for (int i = 0; i < _memos.GetSize(); ++i) {
    c4_Column *cp = (c4_Column*)_memos.GetAt(i);
    if (cp != 0)
      cp->ReleaseAllSegments();
  }
}

void c4_FormatB::Define(int, const t4_byte **ptr_) {
  d4_assert(_memos.GetSize() == 0);

  if (ptr_ != 0) {
    _data.PullLocation(*ptr_);
    if (_data.ColSize() > 0)
      _sizeCol.PullLocation(*ptr_);
    _memoCol.PullLocation(*ptr_);
  }

  // everything below this point could be delayed until use
  // in that case, watch out that column space use is properly tracked

  InitOffsets(_sizeCol);

  if (_memoCol.ColSize() > 0) {
    c4_Bytes walk;
    _memoCol.FetchBytes(0, _memoCol.ColSize(), walk, true);

    const t4_byte *p = walk.Contents();

    for (int row = 0; p < walk.Contents() + walk.Size(); ++row) {
      row += c4_Column::PullValue(p);
      d4_assert(row < _memos.GetSize());

      c4_Column *mc = d4_new c4_Column(_data.Persist());
      d4_assert(mc != 0);
      _memos.SetAt(row, mc);

      mc->PullLocation(p);
    }

    d4_assert(p == walk.Contents() + walk.Size());
  }
}

void c4_FormatB::OldDefine(char type_, c4_Persist &pers_) {
  int rows = Owner().NumRows();

  c4_ColOfInts sizes(_data.Persist());

  if (type_ == 'M') {
    InitOffsets(sizes);

    c4_ColOfInts szVec(_data.Persist());
    pers_.FetchOldLocation(szVec);
    szVec.SetRowCount(rows);

    c4_ColOfInts posVec(_data.Persist());
    pers_.FetchOldLocation(posVec);
    posVec.SetRowCount(rows);

    for (int r = 0; r < rows; ++r) {
      t4_i32 sz = szVec.GetInt(r);
      if (sz > 0) {
        c4_Column *mc = d4_new c4_Column(_data.Persist());
        d4_assert(mc != 0);
        _memos.SetAt(r, mc);

        mc->SetLocation(posVec.GetInt(r), sz);
      }
    }
  } else {
    pers_.FetchOldLocation(_data);

    if (type_ == 'B') {
      pers_.FetchOldLocation(sizes);

#if !q4_OLD_IS_ALWAYS_V2

      // WARNING - HUGE HACK AHEAD - THIS IS NOT 100% FULLPROOF!
      //
      // The above is correct for MK versions 2.0 and up, but *NOT*
      // for MK 1.8.6 datafiles, which store sizes first (OUCH!!!).
      // This means that there is not a 100% safe way to auto-convert
      // both 1.8.6 and 2.0 files - since there is no way to detect
      // unambiguously which version a datafile is.  All we can do,
      // is to carefully check both vectors, and *hope* that only one
      // of them is valid as sizes vector.  This problem applies to
      // the 'B' (bytes) property type only, and only pre 2.0 files.
      //
      // To build a version which *always* converts assuming 1.8.6,
      // add flag "-Dq4_OLD_IS_PRE_V2" to the compiler command line.
      // Conversely, "-Dq4_OLD_IS_ALWAYS_V2" forces 2.0 conversion.

      if (rows > 0) {
        t4_i32 s1 = sizes.ColSize();
        t4_i32 s2 = _data.ColSize();

#if !q4_OLD_IS_PRE_V2
        // if the size vector is clearly impossible, swap vectors
        bool fix = c4_ColOfInts::CalcAccessWidth(rows, s1) < 0;

        // if the other vector might be valid as well, check further
        if (!fix && c4_ColOfInts::CalcAccessWidth(rows, s2) >= 0) {
          sizes.SetRowCount(rows);
          t4_i32 total = 0;
          for (int i = 0; i < rows; ++i) {
            t4_i32 w = sizes.GetInt(i);
            if (w < 0 || total > s2) {
              total =  - 1;
              break;
            }
            total += w;
          }

          // if the sizes don't add up, swap vectors
          fix = total != s2;
        }

        if (fix)
#endif 
         {
          t4_i32 p1 = sizes.Position();
          t4_i32 p2 = _data.Position();
          _data.SetLocation(p1, s1);
          sizes.SetLocation(p2, s2);
        }
      }
#endif 
      InitOffsets(sizes);
    } else {
      d4_assert(type_ == 'S');

      sizes.SetRowCount(rows);

      t4_i32 pos = 0;
      t4_i32 lastEnd = 0;
      int k = 0;

      c4_ColIter iter(_data, 0, _data.ColSize());
      while (iter.Next()) {
        const t4_byte *p = iter.BufLoad();
        for (int j = 0; j < iter.BufLen(); ++j)
        if (!p[j]) {
          sizes.SetInt(k++, pos + j + 1-lastEnd);
          lastEnd = pos + j + 1;
        }

        pos += iter.BufLen();
      }

      d4_assert(pos == _data.ColSize());

      if (lastEnd < pos) {
        // last entry had no zero byte
        _data.InsertData(pos++, 1, true);
        sizes.SetInt(k, pos - lastEnd);
      }

      InitOffsets(sizes);

      // get rid of entries with just a null byte
      for (int r = 0; r < rows; ++r)
        if (c4_FormatB::ItemSize(r) == 1)
          SetOne(r, c4_Bytes());
    }
  }
}

void c4_FormatB::InitOffsets(c4_ColOfInts &sizes_) {
  int rows = Owner().NumRows();

  if (sizes_.RowCount() != rows) {
    sizes_.SetRowCount(rows);
  }

  _memos.SetSize(rows);
  _offsets.SetSize(rows + 1);

  if (_data.ColSize() > 0) {
    t4_i32 total = 0;

    for (int r = 0; r < rows; ++r) {
      int n = sizes_.GetInt(r);
      d4_assert(n >= 0);
      total += n;
      _offsets.SetAt(r + 1, total);
    }

    d4_assert(total == _data.ColSize());
  }

}

int c4_FormatB::ItemSize(int index_) {
  t4_i32 start;
  c4_Column *col;
  return ItemLenOffCol(index_, start, col);
}

const void *c4_FormatB::GetOne(int index_, int &length_) {
  t4_i32 start;
  c4_Column *cp;
  length_ = ItemLenOffCol(index_, start, cp);
  d4_assert(length_ >= 0);

  if (length_ == 0)
    return "";

  return cp->FetchBytes(start, length_, Owner().Buffer(), false);
}

const void *c4_FormatB::Get(int index_, int &length_) {
  return GetOne(index_, length_);
}

void c4_FormatB::SetOne(int index_, const c4_Bytes &xbuf_, bool ignoreMemos_) {
  // this fixes bug in 2.4.0 when copying string from higher row
  // TODO: this fix is very conservative, figure out when to copy
  // (can probably look at pointer to see whether it's from us)
  int sz = xbuf_.Size();
  c4_Bytes buf_(xbuf_.Contents(), sz, 0 < sz && sz <= c4_Column::kSegMax);

  c4_Column *cp = &_data;
  t4_i32 start = Offset(index_);
  int len = Offset(index_ + 1) - start;

  if (!ignoreMemos_ && _memos.GetAt(index_) != 0)
    len = ItemLenOffCol(index_, start, cp);

  int m = buf_.Size();
  int n = m - len;

  if (n > 0)
    cp->Grow(start, n);
  else if (n < 0)
    cp->Shrink(start,  - n);
  else if (m == 0)
    return ;
  // no size change and no contents

  _recalc = true;

  cp->StoreBytes(start, buf_);

  if (n && cp ==  &_data) {
    // if size has changed
    int k = _offsets.GetSize() - 1;

    // if filling in an empty entry at end: extend offsets first
    if (m > 0 && index_ >= k) {
      _offsets.InsertAt(k, _offsets.GetAt(k), index_ - k + 1);

      k = index_ + 1;
      d4_assert(k == _offsets.GetSize() - 1);
    }

    // adjust following entry offsets
    while (++index_ <= k)
      _offsets.ElementAt(index_) += n;
  }

  d4_assert((t4_i32)_offsets.GetAt(_offsets.GetSize() - 1) == _data.ColSize());
}

void c4_FormatB::Set(int index_, const c4_Bytes &buf_) {
  SetOne(index_, buf_);
}

int c4_FormatB::DoCompare(const c4_Bytes &b1_, const c4_Bytes &b2_) {
  int n = b1_.Size();
  if (n > b2_.Size())
    n = b2_.Size();

  int f = memcmp(b1_.Contents(), b2_.Contents(), n);
  return f ? f : b1_.Size() - b2_.Size();
}

void c4_FormatB::Insert(int index_, const c4_Bytes &buf_, int count_) {
  d4_assert(count_ > 0);

  _recalc = true;

  int m = buf_.Size();
  t4_i32 off = Offset(index_);

  _memos.InsertAt(index_, 0, count_);

  // insert the appropriate number of bytes
  t4_i32 n = count_ *(t4_i32)m;
  if (n > 0) {
    _data.Grow(off, n);

    // store as many copies as needed, but may have to do it in chunks
    int spos = 0;

    c4_ColIter iter(_data, off, off + n);
    while (iter.Next(m - spos)) {
      memcpy(iter.BufSave(), buf_.Contents() + spos, iter.BufLen());

      spos += iter.BufLen();
      if (spos >= m)
        spos = 0;
    }

    d4_assert(spos == 0); // must have copied an exact multiple of the data
  }

  // define offsets of the new entries
  _offsets.InsertAt(index_, 0, count_);
  d4_assert(_offsets.GetSize() <= _memos.GetSize() + 1);

  while (--count_ >= 0) {
    _offsets.SetAt(index_++, off);
    off += m;
  }

  d4_assert(index_ < _offsets.GetSize());

  // adjust all following entries
  while (index_ < _offsets.GetSize())
    _offsets.ElementAt(index_++) += n;

  d4_assert((t4_i32)_offsets.GetAt(index_ - 1) == _data.ColSize());
  d4_assert(index_ <= _memos.GetSize() + 1);
}

void c4_FormatB::Remove(int index_, int count_) {
  _recalc = true;

  t4_i32 off = Offset(index_);
  t4_i32 n = Offset(index_ + count_) - off;
  d4_assert(n >= 0);

  // remove the columns, if present
  for (int i = 0; i < count_; ++i)
    delete (c4_Column*)_memos.GetAt(index_ + i);
  _memos.RemoveAt(index_, count_);

  if (n > 0)
    _data.Shrink(off, n);

  _offsets.RemoveAt(index_, count_);

  d4_assert(index_ < _offsets.GetSize());

  // adjust all following entries
  while (index_ < _offsets.GetSize())
    _offsets.ElementAt(index_++) -= n;

  d4_assert((t4_i32)_offsets.GetAt(index_ - 1) == _data.ColSize());
  d4_assert(index_ <= _memos.GetSize() + 1);
}

void c4_FormatB::Commit(c4_SaveContext &ar_) {
  int rows = _memos.GetSize();
  d4_assert(rows > 0);

  bool full = _recalc || ar_.Serializing();

  if (!full)
  for (int i = 0; i < rows; ++i) {
    c4_Column *col = (c4_Column*)_memos.GetAt(i);
    if (col != 0) {
      full = true;
      break;
    }
  }
  d4_assert(_recalc || _sizeCol.RowCount() == rows);

  if (full) {
    _memoCol.SetBuffer(0);
    _sizeCol.SetBuffer(0);
    _sizeCol.SetAccessWidth(0);
    _sizeCol.SetRowCount(rows);

    int skip = 0;

    c4_Column *saved = ar_.SetWalkBuffer(&_memoCol);

    for (int r = 0; r < rows; ++r) {
      ++skip;

      t4_i32 start;
      c4_Column *col;
      int len = ItemLenOffCol(r, start, col);

      bool oldMemo = col !=  &_data;
      bool newMemo = ShouldBeMemo(len);

      if (!oldMemo && newMemo) {
        col = GetNthMemoCol(r, true);
        d4_assert(col !=  &_data);
        //? start = 0;
      }

      c4_Bytes temp;

      if (newMemo) {
        // it now is a memo, inlined data will be empty
        ar_.StoreValue(skip - 1);
        skip = 0;
        ar_.CommitColumn(*col);
      } else if (!oldMemo) {
        // it was no memo, done if it hasn't become one
        _sizeCol.SetInt(r, len);
        continue;
      } else {
        // it was a memo, but it no longer is
        d4_assert(start == 0);
        if (len > 0) {
          _sizeCol.SetInt(r, len);
          col->FetchBytes(start, len, temp, true);
          delete (c4_Column*)_memos.GetAt(r); // 28-11-2001: fix mem leak
          _memos.SetAt(r, 0); // 02-11-2001: fix for use after commit
        }
      }

      SetOne(r, temp, true); // bypass current memo pointer
    }

    ar_.SetWalkBuffer(saved);
  }

  ar_.CommitColumn(_data);

  if (_data.ColSize() > 0) {
    _sizeCol.FixSize(true);
    ar_.CommitColumn(_sizeCol);
    //_sizeCol.FixSize(false);
  }

  ar_.CommitColumn(_memoCol);

  // need a way to find out when the data has been committed (on 2nd pass)
  // both _sizeCol and _memoCol will be clean again when it has
  // but be careful because dirty flag is only useful if size is nonzero
  if (_recalc && !ar_.Serializing())
    _recalc = _sizeCol.ColSize() > 0 && _sizeCol.IsDirty() || _memoCol.ColSize()
      > 0 && _memoCol.IsDirty();
}

/////////////////////////////////////////////////////////////////////////////

class c4_FormatS: public c4_FormatB {
  public:
    c4_FormatS(const c4_Property &prop_, c4_HandlerSeq &seq_);

    virtual int ItemSize(int index_);
    virtual const void *Get(int index_, int &length_);
    virtual void Set(int index_, const c4_Bytes &buf_);

    virtual void Insert(int index_, const c4_Bytes &buf_, int count_);

    static int DoCompare(const c4_Bytes &b1_, const c4_Bytes &b2_);
};

/////////////////////////////////////////////////////////////////////////////

c4_FormatS::c4_FormatS(const c4_Property &prop_, c4_HandlerSeq &seq_):
  c4_FormatB(prop_, seq_){}

int c4_FormatS::ItemSize(int index_) {
  int n = c4_FormatB::ItemSize(index_) - 1;
  return n >= 0 ? n : 0;
}

const void *c4_FormatS::Get(int index_, int &length_) {
  const void *ptr = GetOne(index_, length_);

  if (length_ == 0) {
    length_ = 1;
    ptr = "";
  }

  d4_assert(((const char*)ptr)[length_ - 1] == 0);
  return ptr;
}

void c4_FormatS::Set(int index_, const c4_Bytes &buf_) {
  int m = buf_.Size();
  if (--m >= 0) {
    d4_assert(buf_.Contents()[m] == 0);
    if (m == 0) {
      SetOne(index_, c4_Bytes()); // don't store data for empty strings
      return ;
    }
  }

  SetOne(index_, buf_);
}

int c4_FormatS::DoCompare(const c4_Bytes &b1_, const c4_Bytes &b2_) {
  c4_String v1((const char*)b1_.Contents(), b1_.Size());
  c4_String v2((const char*)b2_.Contents(), b2_.Size());

  return v1.CompareNoCase(v2);
}

void c4_FormatS::Insert(int index_, const c4_Bytes &buf_, int count_) {
  d4_assert(count_ > 0);

  int m = buf_.Size();
  if (--m >= 0) {
    d4_assert(buf_.Contents()[m] == 0);
    if (m == 0) {
      c4_FormatB::Insert(index_, c4_Bytes(), count_);
      return ;
    }
  }

  c4_FormatB::Insert(index_, buf_, count_);
}

/////////////////////////////////////////////////////////////////////////////

class c4_FormatV: public c4_FormatHandler {
  public:
    c4_FormatV(const c4_Property &prop_, c4_HandlerSeq &seq_);
    virtual ~c4_FormatV();

    virtual void Define(int rows_, const t4_byte **ptr_);
    virtual void OldDefine(char type_, c4_Persist &);
    virtual void Commit(c4_SaveContext &ar_);

    virtual void FlipBytes();

    virtual int ItemSize(int index_);
    virtual const void *Get(int index_, int &length_);
    virtual void Set(int index_, const c4_Bytes &buf_);

    virtual void Insert(int index_, const c4_Bytes &buf_, int count_);
    virtual void Remove(int index_, int count_);

    virtual void Unmapped();
    virtual bool HasSubview(int index_);

    static int DoCompare(const c4_Bytes &b1_, const c4_Bytes &b2_);

  private:
    c4_HandlerSeq &At(int index_);
    void Replace(int index_, c4_HandlerSeq *seq_);
    void SetupAllSubviews();
    void ForgetSubview(int index_);

    c4_Column _data;
    c4_PtrArray _subSeqs;
    bool _inited;
};

/////////////////////////////////////////////////////////////////////////////

c4_FormatV::c4_FormatV(const c4_Property &prop_, c4_HandlerSeq &seq_):
  c4_FormatHandler(prop_, seq_), _data(seq_.Persist()), _inited(false){}

c4_FormatV::~c4_FormatV() {
  for (int i = 0; i < _subSeqs.GetSize(); ++i)
    ForgetSubview(i);
}

c4_HandlerSeq &c4_FormatV::At(int index_) {
  d4_assert(_inited);

  c4_HandlerSeq * &hs = (c4_HandlerSeq * &)_subSeqs.ElementAt(index_);
  if (hs == 0) {
    hs = d4_new c4_HandlerSeq(Owner(), this);
    hs->IncRef();
  }

  return  *hs;
}

void c4_FormatV::SetupAllSubviews() {
  d4_assert(!_inited);
  _inited = true;

  if (_data.ColSize() > 0) {
    c4_Bytes temp;
    _data.FetchBytes(0, _data.ColSize(), temp, true);
    const t4_byte *ptr = temp.Contents();

    for (int r = 0; r < _subSeqs.GetSize(); ++r) {
      // don't materialize subview if it is empty
      // duplicates code which is in c4_HandlerSeq::Prepare
      const t4_byte *p2 = ptr;
      d4_dbgdef(t4_i32 sias = )c4_Column::PullValue(p2);
      d4_assert(sias == 0); // not yet

      if (c4_Column::PullValue(p2) > 0)
        At(r).Prepare(&ptr, false);
      else
        ptr = p2;
    }

    d4_assert(ptr == temp.Contents() + temp.Size());
  }
}

void c4_FormatV::Define(int rows_, const t4_byte **ptr_) {
  if (_inited) {
    // big oops: a root handler already contains data

    for (int i = 0; i < _subSeqs.GetSize(); ++i)
      ForgetSubview(i);

    _inited = false;
  }

  _subSeqs.SetSize(rows_);
  if (ptr_ != 0)
    _data.PullLocation(*ptr_);
}

void c4_FormatV::OldDefine(char, c4_Persist &pers_) {
  int rows = Owner().NumRows();
  _subSeqs.SetSize(rows);

  for (int i = 0; i < rows; ++i) {
    int n = pers_.FetchOldValue();
    if (n) {
      // 14-11-2000: do not create again (this causes a mem leak)
      // 04-12-2000: but do create if absent (fixes occasional crash)
      c4_HandlerSeq *hs = (c4_HandlerSeq*)_subSeqs.GetAt(i);
      if (hs == 0) {
        hs = d4_new c4_HandlerSeq(Owner(), this);
        _subSeqs.SetAt(i, hs);
        hs->IncRef();
      }
      hs->SetNumRows(n);
      hs->OldPrepare();
    }
  }
}

void c4_FormatV::FlipBytes() {
  if (!_inited)
    SetupAllSubviews();

  for (int i = 0; i < _subSeqs.GetSize(); ++i)
    At(i).FlipAllBytes();
}

int c4_FormatV::ItemSize(int index_) {
  if (!_inited)
    SetupAllSubviews();

  // 06-02-2002: avoid creating empty subview
  c4_HandlerSeq *hs = (c4_HandlerSeq * &)_subSeqs.ElementAt(index_);
  return hs == 0 ? 0 : hs->NumRows();
}

const void *c4_FormatV::Get(int index_, int &length_) {
  if (!_inited)
    SetupAllSubviews();

  At(index_); // forces existence of a real entry
  c4_HandlerSeq * &e = (c4_HandlerSeq * &)_subSeqs.ElementAt(index_);

  length_ = sizeof(c4_HandlerSeq **);
  return  &e;
}

void c4_FormatV::Set(int index_, const c4_Bytes &buf_) {
  d4_assert(buf_.Size() == sizeof(c4_Sequence*));

  if (!_inited)
    SetupAllSubviews();

  c4_HandlerSeq *value = *(c4_HandlerSeq *const*)buf_.Contents();

  if (value !=  &At(index_))
    Replace(index_, value);
}

void c4_FormatV::Replace(int index_, c4_HandlerSeq *seq_) {
  if (!_inited)
    SetupAllSubviews();

  c4_HandlerSeq * &curr = (c4_HandlerSeq * &)_subSeqs.ElementAt(index_);
  if (seq_ == curr)
    return ;

  if (curr != 0) {
    d4_assert(&curr->Parent() ==  &Owner());
    curr->DetachFromParent();
    curr->DetachFromStorage(true);

    curr->DecRef();
    curr = 0;
  }

  if (seq_) {
    int n = seq_->NumRows();

    c4_HandlerSeq &t = At(index_);
    d4_assert(t.NumRows() == 0);

    t.Resize(n);

    c4_Bytes data;

    // this dest seq has only the persistent handlers
    // and maybe in a different order
    // create any others we need as temporary properties
    for (int i = 0; i < seq_->NumHandlers(); ++i) {
      c4_Handler &h1 = seq_->NthHandler(i);

      int j = t.PropIndex(h1.Property());
      d4_assert(j >= 0);

      c4_Handler &h2 = t.NthHandler(j);

      for (int k = 0; k < n; ++k)
        if (seq_->Get(k, h1.PropId(), data))
          h2.Set(k, data);
    }
  }
}

int c4_FormatV::DoCompare(const c4_Bytes &b1_, const c4_Bytes &b2_) {
  d4_assert(b1_.Size() == sizeof(c4_Sequence*));
  d4_assert(b2_.Size() == sizeof(c4_Sequence*));

  c4_View v1 = *(c4_Sequence *const*)b1_.Contents();
  c4_View v2 = *(c4_Sequence *const*)b2_.Contents();

  return v1.Compare(v2);
}

void c4_FormatV::Insert(int index_, const c4_Bytes &buf_, int count_) {
  d4_assert(buf_.Size() == sizeof(c4_Sequence*));
  d4_assert(count_ > 0);

  // can only insert an empty entry!
  d4_assert(*(c4_Sequence *const*)buf_.Contents() == 0);

  if (!_inited)
    SetupAllSubviews();

  _subSeqs.InsertAt(index_, 0, count_);
  _data.SetBuffer(0); // 2004-01-18 force dirty
}

void c4_FormatV::Remove(int index_, int count_) {
  d4_assert(count_ > 0);

  if (!_inited)
    SetupAllSubviews();

  for (int i = 0; i < count_; ++i)
    ForgetSubview(index_ + i);

  _subSeqs.RemoveAt(index_, count_);
  _data.SetBuffer(0); // 2004-01-18 force dirty
}

void c4_FormatV::Unmapped() {
  if (_inited)
    for (int i = 0; i < _subSeqs.GetSize(); ++i)
  if (HasSubview(i)) {
    c4_HandlerSeq &hs = At(i);
    hs.UnmappedAll();
    if (hs.NumRefs() == 1 && hs.NumRows() == 0)
        ForgetSubview(i);
  }

  _data.ReleaseAllSegments();
}

bool c4_FormatV::HasSubview(int index_) {
  if (!_inited)
    SetupAllSubviews();

  return _subSeqs.ElementAt(index_) != 0;
}

void c4_FormatV::ForgetSubview(int index_) {
  c4_HandlerSeq * &seq = (c4_HandlerSeq * &)_subSeqs.ElementAt(index_);
  if (seq != 0) {
    d4_assert(&seq->Parent() ==  &Owner());
    seq->DetachFromParent();
    seq->DetachFromStorage(true);
    seq->UnmappedAll();
    seq->DecRef();
    seq = 0;
  }
}

void c4_FormatV::Commit(c4_SaveContext &ar_) {
  if (!_inited)
    SetupAllSubviews();

  int rows = _subSeqs.GetSize();
  d4_assert(rows > 0);

  c4_Column temp(0);
  c4_Column *saved = ar_.SetWalkBuffer(&temp);

  for (int r = 0; r < rows; ++r)
  if (HasSubview(r)) {
    c4_HandlerSeq &hs = At(r);
    ar_.CommitSequence(hs, false);
    if (hs.NumRefs() == 1 && hs.NumRows() == 0)
      ForgetSubview(r);
  } else {
    ar_.StoreValue(0); // sias
    ar_.StoreValue(0); // row count
  }

  ar_.SetWalkBuffer(saved);

  c4_Bytes buf;
  temp.FetchBytes(0, temp.ColSize(), buf, true);

  bool changed = temp.ColSize() != _data.ColSize();

  if (!changed) {
    c4_Bytes buf2;
    _data.FetchBytes(0, _data.ColSize(), buf2, true);
    changed = buf != buf2;
  }

  if (changed) {
    _data.SetBuffer(buf.Size());
    _data.StoreBytes(0, buf);
  }

  ar_.CommitColumn(_data);
}

/////////////////////////////////////////////////////////////////////////////

c4_Handler *f4_CreateFormat(const c4_Property &prop_, c4_HandlerSeq &seq_) {
  switch (prop_.Type()) {
    case 'I':
      return d4_new c4_FormatX(prop_, seq_);
#if !q4_TINY
    case 'L':
      return d4_new c4_FormatL(prop_, seq_);
    case 'F':
      return d4_new c4_FormatF(prop_, seq_);
    case 'D':
      return d4_new c4_FormatD(prop_, seq_);
#endif 
    case 'B':
      return d4_new c4_FormatB(prop_, seq_);
    case 'S':
      return d4_new c4_FormatS(prop_, seq_);
    case 'V':
      return d4_new c4_FormatV(prop_, seq_);
  }

  d4_assert(0);
  // 2004-01-16 turn bad definition type into an int property to avoid crash
  return d4_new c4_FormatX(c4_IntProp(prop_.Name()), seq_);
}

/////////////////////////////////////////////////////////////////////////////

int f4_ClearFormat(char type_) {
  switch (type_) {
    case 'I':
      return sizeof(t4_i32);
#if !q4_TINY
    case 'L':
      return sizeof(t4_i64);
    case 'F':
      return sizeof(float);
    case 'D':
      return sizeof(double);
#endif 
    case 'S':
      return 1;
    case 'V':
      return sizeof(c4_Sequence*);
  }

  return 0;
}

/////////////////////////////////////////////////////////////////////////////

int f4_CompareFormat(char type_, const c4_Bytes &b1_, const c4_Bytes &b2_) {
  switch (type_) {
    case 'I':
      return c4_FormatX::DoCompare(b1_, b2_);
#if !q4_TINY
    case 'L':
      return c4_FormatL::DoCompare(b1_, b2_);
    case 'F':
      return c4_FormatF::DoCompare(b1_, b2_);
    case 'D':
      return c4_FormatD::DoCompare(b1_, b2_);
#endif 
    case 'B':
      return c4_FormatB::DoCompare(b1_, b2_);
    case 'S':
      return c4_FormatS::DoCompare(b1_, b2_);
    case 'V':
      return c4_FormatV::DoCompare(b1_, b2_);
  }

  d4_assert(0);
  return 0;
}

/////////////////////////////////////////////////////////////////////////////
