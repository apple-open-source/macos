// derived.cpp --
// $Id: derived.cpp 1230 2007-03-09 15:58:53Z jcw $
// This is part of Metakit, see http://www.equi4.com/metakit.html

/** @file
 * Derived views are virtual views which track changes
 */

#include "header.h"
#include "handler.h"
#include "store.h"
#include "derived.h"

#include <stdlib.h>   // qsort

/////////////////////////////////////////////////////////////////////////////
// Implemented in this file

//  class c4_Sequence;
class c4_DerivedSeq;
class c4_FilterSeq;
class c4_SortSeq;
class c4_ProjectSeq;

/////////////////////////////////////////////////////////////////////////////

class c4_FilterSeq: public c4_DerivedSeq {
  protected:
    c4_DWordArray _rowMap;
    c4_DWordArray _revMap;
    c4_Row _lowRow;
    c4_Row _highRow;
    c4_Bytes _rowIds;

  protected:
    c4_FilterSeq(c4_Sequence &seq_);
    virtual ~c4_FilterSeq();

    void FixupReverseMap();
    int PosInMap(int index_)const;
    bool Match(int index_, c4_Sequence &seq_, const int * = 0, const int * = 0)
      const;
    bool MatchOne(int prop_, const c4_Bytes &data_)const;

  public:
    c4_FilterSeq(c4_Sequence &seq_, c4_Cursor low_, c4_Cursor high_);

    virtual int RemapIndex(int, const c4_Sequence*)const;

    virtual int NumRows()const;

    virtual int Compare(int, c4_Cursor)const;
    virtual bool Get(int, int, c4_Bytes &);

    virtual void InsertAt(int, c4_Cursor, int = 1);
    virtual void RemoveAt(int, int = 1);
    virtual void Set(int, const c4_Property &, const c4_Bytes &);
    virtual void SetSize(int);

    virtual c4_Notifier *PreChange(c4_Notifier &nf_);
    virtual void PostChange(c4_Notifier &nf_);
};

/////////////////////////////////////////////////////////////////////////////

c4_FilterSeq::c4_FilterSeq(c4_Sequence &seq_): c4_DerivedSeq(seq_) {
  _rowMap.SetSize(_seq.NumRows());
  _revMap.SetSize(_seq.NumRows());
  d4_assert(NumRows() == _seq.NumRows());

  for (int i = 0; i < NumRows(); ++i) {
    _rowMap.SetAt(i, i);
    _revMap.SetAt(i, i);
  }
}

c4_FilterSeq::c4_FilterSeq(c4_Sequence &seq_, c4_Cursor low_, c4_Cursor high_):
  c4_DerivedSeq(seq_), _lowRow(*low_), _highRow(*high_) {
  d4_assert((&_lowRow)._index == 0);
  d4_assert((&_highRow)._index == 0);

  // use a sneaky way to obtain the sequence pointers and indices
  c4_Sequence *lowSeq = (&_lowRow)._seq;
  c4_Sequence *highSeq = (&_highRow)._seq;
  d4_assert(lowSeq && highSeq);

  // prepare column numbers to avoid looking them up on every row
  // lowCols is a vector of column numbers to use for the low limits
  // highCols is a vector of column numbers to use for the high limits
  int nl = lowSeq->NumHandlers(), nh = highSeq->NumHandlers();
  c4_Bytes lowVec, highVec;
  int *lowCols = (int*)lowVec.SetBufferClear(nl *sizeof(int));
  int *highCols = (int*)highVec.SetBufferClear(nh *sizeof(int));

  for (int il = 0; il < nl; ++il)
    lowCols[il] = seq_.PropIndex(lowSeq->NthPropId(il));
  for (int ih = 0; ih < nh; ++ih)
    highCols[ih] = seq_.PropIndex(highSeq->NthPropId(ih));

  // set _rowIds flag buffer for fast matching
   {
    int max =  - 1;

     {
      for (int i1 = 0; i1 < nl; ++i1) {
        int n = lowSeq->NthPropId(i1);
        if (max < n)
          max = n;
      }
      for (int i2 = 0; i2 < nh; ++i2) {
        int n = highSeq->NthPropId(i2);
        if (max < n)
          max = n;
      }
    }

    t4_byte *p = _rowIds.SetBufferClear(max + 1);

     {
      for (int i1 = 0; i1 < nl; ++i1)
        p[lowSeq->NthPropId(i1)] |= 1;
      for (int i2 = 0; i2 < nh; ++i2)
        p[highSeq->NthPropId(i2)] |= 2;
    }
  }

  // now go through all rows and select the ones that are in range

  _rowMap.SetSize(_seq.NumRows()); // avoid growing, use safe upper bound

  int n = 0;

  for (int i = 0; i < _seq.NumRows(); ++i)
    if (Match(i, _seq, lowCols, highCols))
      _rowMap.SetAt(n++, i);

  _rowMap.SetSize(n);

  FixupReverseMap();
}

c4_FilterSeq::~c4_FilterSeq(){}

void c4_FilterSeq::FixupReverseMap() {
  int n = _seq.NumRows();

  _revMap.SetSize(0);

  if (n > 0) {
    _revMap.InsertAt(0, ~(t4_i32)0, n); //!

    for (int i = 0; i < _rowMap.GetSize(); ++i)
      _revMap.SetAt((int)_rowMap.GetAt(i), i);
  }
}

bool c4_FilterSeq::MatchOne(int prop_, const c4_Bytes &data_)const {
  d4_assert(prop_ < _rowIds.Size());

  t4_byte flag = _rowIds.Contents()[prop_];
  d4_assert(flag);

  if (flag &1) {
    c4_Sequence *lowSeq = (&_lowRow)._seq;

    c4_Handler &h = lowSeq->NthHandler(lowSeq->PropIndex(prop_));
    if (h.Compare(0, data_) > 0)
      return false;
  }

  if (flag &2) {
    c4_Sequence *highSeq = (&_highRow)._seq;

    c4_Handler &h = highSeq->NthHandler(highSeq->PropIndex(prop_));
    if (h.Compare(0, data_) < 0)
      return false;
  }

  return true;
}

bool c4_FilterSeq::Match(int index_, c4_Sequence &seq_, const int *lowCols_,
  const int *highCols_)const {
  // use a sneaky way to obtain the sequence pointers and indices
  c4_Sequence *lowSeq = (&_lowRow)._seq;
  c4_Sequence *highSeq = (&_highRow)._seq;
  d4_assert(lowSeq && highSeq);

  int nl = lowSeq->NumHandlers(), nh = highSeq->NumHandlers();

  c4_Bytes data;

  // check each of the lower limits
  for (int cl = 0; cl < nl; ++cl) {
    c4_Handler &hl = lowSeq->NthHandler(cl);

    int n = lowCols_ ? lowCols_[cl]: seq_.PropIndex(lowSeq->NthPropId(cl));
    if (n >= 0) {
      c4_Handler &h = seq_.NthHandler(n);
      const c4_Sequence *hc = seq_.HandlerContext(n);
      int i = seq_.RemapIndex(index_, hc);

      h.GetBytes(i, data);
    } else
      hl.ClearBytes(data);

    if (hl.Compare(0, data) > 0)
      return false;
  }

  // check each of the upper limits
  for (int ch = 0; ch < nh; ++ch) {
    c4_Handler &hh = highSeq->NthHandler(ch);

    int n = highCols_ ? highCols_[ch]: seq_.PropIndex(highSeq->NthPropId(ch));
    if (n >= 0) {
      c4_Handler &h = seq_.NthHandler(n);
      const c4_Sequence *hc = seq_.HandlerContext(n);
      int i = seq_.RemapIndex(index_, hc);

      h.GetBytes(i, data);
    } else
      hh.ClearBytes(data);

    if (hh.Compare(0, data) < 0)
      return false;
  }

  return true;
}

int c4_FilterSeq::RemapIndex(int index_, const c4_Sequence *seq_)const {
  return seq_ == this ? index_: _seq.RemapIndex((int)_rowMap.GetAt(index_),
    seq_);
}

int c4_FilterSeq::NumRows()const {
  return _rowMap.GetSize();
}

int c4_FilterSeq::Compare(int index_, c4_Cursor cursor_)const {
  return _seq.Compare((int)_rowMap.GetAt(index_), cursor_);
}

bool c4_FilterSeq::Get(int index_, int propId_, c4_Bytes &bytes_) {
  return _seq.Get((int)_rowMap.GetAt(index_), propId_, bytes_);
}

void c4_FilterSeq::InsertAt(int, c4_Cursor, int) {
  d4_assert(0);
}

void c4_FilterSeq::RemoveAt(int, int) {
  d4_assert(0);
}

void c4_FilterSeq::Set(int, const c4_Property &, const c4_Bytes &) {
  d4_assert(0);
}

void c4_FilterSeq::SetSize(int) {
  d4_assert(0);
}

int c4_FilterSeq::PosInMap(int index_)const {
  int i = 0;

  while (i < NumRows())
    if ((int)_rowMap.GetAt(i) >= index_)
      break;
    else
      ++i;

  return i;
}

c4_Notifier *c4_FilterSeq::PreChange(c4_Notifier &nf_) {
  if (!GetDependencies())
    return 0;

  c4_Notifier *chg = d4_new c4_Notifier(this);

  bool pass = false;

  switch (nf_._type) {
    case c4_Notifier::kSet: pass = nf_._propId >= _rowIds.Size() ||
      _rowIds.Contents()[nf_._propId] == 0;
    // fall through...

    case c4_Notifier::kSetAt:  {
      int r = (int)_revMap.GetAt(nf_._index);

      bool includeRow = r >= 0;
      if (!pass)
      if (nf_._type == c4_Notifier::kSetAt) {
        d4_assert(nf_._cursor != 0);
        includeRow = Match(nf_._cursor->_index, *nf_._cursor->_seq);
      }
       else
      // set just one property, and it's not in a row yet
        includeRow = MatchOne(nf_._propId,  *nf_._bytes);

      if (r >= 0 && !includeRow)
        chg->StartRemoveAt(r, 1);
      else if (r < 0 && includeRow)
        chg->StartInsertAt(PosInMap(nf_._index),  *nf_._cursor, 1);
      else if (includeRow) {
        d4_assert(r >= 0);

        if (nf_._type == c4_Notifier::kSetAt)
          chg->StartSetAt(r,  *nf_._cursor);
        else
          chg->StartSet(r, nf_._propId,  *nf_._bytes);
      }
    }
    break;

    case c4_Notifier::kInsertAt:  {
      int i = PosInMap(nf_._index);

      d4_assert(nf_._cursor != 0);
      if (Match(nf_._cursor->_index,  *nf_._cursor->_seq))
        chg->StartInsertAt(i,  *nf_._cursor, nf_._count);
    }
    break;

    case c4_Notifier::kRemoveAt:  {
      int i = PosInMap(nf_._index);
      int j = PosInMap(nf_._index + nf_._count);
      d4_assert(j >= i);

      if (j > i)
        chg->StartRemoveAt(i, j - i);
    }
    break;

    case c4_Notifier::kMove:  {
      int i = PosInMap(nf_._index);
      bool inMap = i < NumRows() && (int)_rowMap.GetAt(i) == nf_._index;

      if (inMap && nf_._index != nf_._count)
        chg->StartMove(i, PosInMap(nf_._count));
    }
    break;
  }

  return chg;
}

void c4_FilterSeq::PostChange(c4_Notifier &nf_) {
  bool pass = false;

  switch (nf_._type) {
    case c4_Notifier::kSet: pass = nf_._propId >= _rowIds.Size() ||
      _rowIds.Contents()[nf_._propId] == 0;
    // fall through...

    case c4_Notifier::kSetAt:  {
      int r = (int)_revMap.GetAt(nf_._index);

      bool includeRow = r >= 0;
      if (!pass)
      if (nf_._type == c4_Notifier::kSetAt) {
        d4_assert(nf_._cursor != 0);
        includeRow = Match(nf_._cursor->_index, *nf_._cursor->_seq);
      }
       else
      // set just one property, and it's not in a row yet
        includeRow = MatchOne(nf_._propId,  *nf_._bytes);

      if (r >= 0 && !includeRow)
        _rowMap.RemoveAt(r);
      else if (r < 0 && includeRow)
        _rowMap.InsertAt(PosInMap(nf_._index), nf_._index);
      else
        break;

      FixupReverseMap();
    }
    break;

    case c4_Notifier::kInsertAt:  {
      int i = PosInMap(nf_._index);

      if (Match(nf_._index, _seq)) {
        _rowMap.InsertAt(i, 0, nf_._count);

        for (int j = 0; j < nf_._count; ++j)
          _rowMap.SetAt(i++, nf_._index + j);
      }

      while (i < NumRows())
        _rowMap.ElementAt(i++) += nf_._count;

      FixupReverseMap();
    }
    break;

    case c4_Notifier::kRemoveAt:  {
      int i = PosInMap(nf_._index);
      int j = PosInMap(nf_._index + nf_._count);
      d4_assert(j >= i);

      if (j > i)
        _rowMap.RemoveAt(i, j - i);

      while (i < NumRows())
        _rowMap.ElementAt(i++) -= nf_._count;

      FixupReverseMap();
    }
    break;

    case c4_Notifier::kMove:  {
      int i = PosInMap(nf_._index);
      bool inMap = i < NumRows() && (int)_rowMap.GetAt(i) == nf_._index;

      if (inMap && nf_._index != nf_._count) {
        int j = PosInMap(nf_._count);

        _rowMap.RemoveAt(i);

        if (j > i)
          --j;

        _rowMap.InsertAt(j, nf_._count);

        FixupReverseMap();
      }
    }
    break;
  }
}

/////////////////////////////////////////////////////////////////////////////

class c4_SortSeq: public c4_FilterSeq {
  public:
    typedef t4_i32 T;

    c4_SortSeq(c4_Sequence &seq_, c4_Sequence *down_);
    virtual ~c4_SortSeq();

    virtual c4_Notifier *PreChange(c4_Notifier &nf_);
    virtual void PostChange(c4_Notifier &nf_);

  private:
    struct c4_SortInfo {
        c4_Handler *_handler;
        const c4_Sequence *_context;
        c4_Bytes _buffer;

        int CompareOne(c4_Sequence &seq_, T a, T b) {
            _handler->GetBytes(seq_.RemapIndex((int)b, _context), _buffer, true)
              ;
            return _handler->Compare(seq_.RemapIndex((int)a, _context), _buffer)
              ;
        } 
    };

    bool LessThan(T a, T b);
    bool TestSwap(T &first, T &second);
    void MergeSortThis(T *ar, int size, T scratch[]);
    void MergeSort(T ar[], int size);

    virtual int Compare(int, c4_Cursor)const;
    int PosInMap(c4_Cursor cursor_)const;

    c4_SortInfo *_info;
    c4_Bytes _down;
    int _width;
};

/////////////////////////////////////////////////////////////////////////////

bool c4_SortSeq::LessThan(T a, T b) {
  if (a == b)
    return false;

  // go through each of the columns and compare values, but since
  // handler access is used, we must be careful to remap indices

  c4_SortInfo *info;

  for (info = _info; info->_handler; ++info) {
    int f = info->CompareOne(_seq, a, b);
    if (f) {
      int n = info - _info;
      if (_width < n)
        _width = n;

      return (_down.Contents()[n] ?  - f: f) < 0;
    }
  }

  _width = info - _info;
  return a < b;
}

inline bool c4_SortSeq::TestSwap(T &first, T &second) {
  if (LessThan(second, first)) {
    T temp = first;
    first = second;
    second = temp;
    return true;
  }

  return false;
}

void c4_SortSeq::MergeSortThis(T *ar, int size, T scratch[]) {
  switch (size) {
    //Handle the special cases for speed:
    case 2:
      TestSwap(ar[0], ar[1]);
      break;

    case 3:
      TestSwap(ar[0], ar[1]);
      if (TestSwap(ar[1], ar[2]))
        TestSwap(ar[0], ar[1]);
      break;

    case 4:
      //Gotta optimize this....
      TestSwap(ar[0], ar[1]);
      TestSwap(ar[2], ar[3]);
      TestSwap(ar[0], ar[2]);
      TestSwap(ar[1], ar[3]);
      TestSwap(ar[1], ar[2]);
      break;

      //Gotta do special case for list of five.

    default:
      //Subdivide the list, recurse, and merge
       {
        int s1 = size / 2;
        int s2 = size - s1;
        T *from1_ = scratch;
        T *from2_ = scratch + s1;
        MergeSortThis(from1_, s1, ar);
        MergeSortThis(from2_, s2, ar + s1);

        T *to1_ = from1_ + s1;
        T *to2_ = from2_ + s2;

        for (;;) {
          if (LessThan(*from1_,  *from2_)) {
            *ar++ =  *from1_++;

            if (from1_ >= to1_) {
              while (from2_ < to2_)
                *ar++ =  *from2_++;
              break;
            }
          }
           else {
            *ar++ =  *from2_++;

            if (from2_ >= to2_) {
              while (from1_ < to1_)
                *ar++ =  *from1_++;
              break;
            }
          }
        }
      }
  }
}

void c4_SortSeq::MergeSort(T ar[], int size) {
  if (size > 1) {
    T *scratch = d4_new T[size];
    memcpy(scratch, ar, size *sizeof(T));
    MergeSortThis(ar, size, scratch);
    delete [] scratch;
  }
}

c4_SortSeq::c4_SortSeq(c4_Sequence &seq_, c4_Sequence *down_): c4_FilterSeq
  (seq_), _info(0), _width( - 1) {
  d4_assert(NumRows() == seq_.NumRows());

  if (NumRows() > 0) {
    // down is a vector of flags, true to sort in reverse order
    char *down = (char*)_down.SetBufferClear(NumHandlers());

    // set the down flag for all properties to be sorted in reverse
    if (down_)
      for (int i = 0; i < NumHandlers(); ++i)
        if (down_->PropIndex(NthPropId(i)) >= 0)
          down[i] = 1;

    _width =  - 1;
    int n = NumHandlers() + 1;
    _info = d4_new c4_SortInfo[n];

    int j;

    for (j = 0; j < NumHandlers(); ++j) {
      _info[j]._handler = &_seq.NthHandler(j);
      _info[j]._context = _seq.HandlerContext(j);
    }

    _info[j]._handler = 0;

    // everything is ready, go sort the row index vector
    MergeSort((T*) &_rowMap.ElementAt(0), NumRows());

    delete [] _info;
    _info = 0;

    FixupReverseMap();
  }
}

c4_SortSeq::~c4_SortSeq() {
  d4_assert(!_info);
}

int c4_SortSeq::Compare(int index_, c4_Cursor cursor_)const {
  d4_assert(cursor_._seq != 0);

  const char *down = (const char*)_down.Contents();
  d4_assert(_down.Size() <= NumHandlers());

  c4_Bytes data;

  for (int colNum = 0; colNum < NumHandlers(); ++colNum) {
    c4_Handler &h = NthHandler(colNum);
    const c4_Sequence *hc = HandlerContext(colNum);

    if (!cursor_._seq->Get(cursor_._index, h.PropId(), data))
      h.ClearBytes(data);

    int f = h.Compare(RemapIndex(index_, hc), data);
    if (f != 0)
      return colNum < _down.Size() && down[colNum] ?  - f:  + f;
  }

  return 0;
}

int c4_SortSeq::PosInMap(c4_Cursor cursor_)const {
  int i = 0;

  while (i < NumRows())
    if (Compare(i, cursor_) >= 0)
      break;
    else
      ++i;

  d4_assert(i == NumRows() || Compare(i, cursor_) >= 0);
  return i;
}

c4_Notifier *c4_SortSeq::PreChange(c4_Notifier & /*nf_*/) {
  if (!GetDependencies())
    return 0;

#if 0
  c4_Notifier *chg = d4_new c4_Notifier(this);

  switch (nf_._type) {
    case c4_Notifier::kSetAt: case c4_Notifier::kSet:  {
      d4_assert(0); // also needs nested propagation

      /*
      change can require a move *and* a change of contents
       */
    }
    break;

    case c4_Notifier::kInsertAt:  {
      d4_assert(0); // this case isn't really difficult
    }
    break;

    case c4_Notifier::kRemoveAt:  {
      d4_assert(0); // nested propagation is too difficult for now
      // i.e. can only use sort as last derived view
      /*
      possible solution:

      if 1 row, simple
      else if contig in map, also simple
      else propagate reorder first, then delete contig

      it can be done here, as multiple notifications,
      by simulating n-1 SetAt's of first row in others
      needs some map juggling, allow temp dup entries?

      or perhaps more consistent with n separate removes
       */
    }
    break;

    case c4_Notifier::kMove:  {
      // incorrect: may need to move if recnum matters (recs same)
    }
    break;
  }

  return chg;
#endif 

  //  d4_assert(0); // fail, cannot handle a view dependent on this one yet
  return 0;
}

void c4_SortSeq::PostChange(c4_Notifier &nf_) {
  switch (nf_._type) {
    case c4_Notifier::kSet: if (_seq.PropIndex(nf_._propId) > _width)
      break;
    // cannot affect sort order, valuable optimization

    case c4_Notifier::kSetAt:  {
      int oi = (int)_revMap.GetAt(nf_._index);
      d4_assert(oi >= 0);

      c4_Cursor cursor(_seq, nf_._index);

      // move the entry if the sort order has been disrupted
      if ((oi > 0 && Compare(oi - 1, cursor) > 0) || (oi + 1 < NumRows() &&
        Compare(oi + 1, cursor) < 0)) {
        _rowMap.RemoveAt(oi);
        _rowMap.InsertAt(PosInMap(cursor), nf_._index);

        FixupReverseMap();
      }

      _width = NumHandlers(); // sorry, no more optimization
    }
    break;

    case c4_Notifier::kInsertAt:  {
      // if cursor was not set, it started out as a single Set
      c4_Cursor cursor(_seq, nf_._index);
      if (nf_._cursor)
        cursor =  *nf_._cursor;

      for (int n = 0; n < NumRows(); ++n)
        if ((int)_rowMap.GetAt(n) >= nf_._index)
          _rowMap.ElementAt(n) += nf_._count;

      int i = PosInMap(cursor);
      _rowMap.InsertAt(i, 0, nf_._count);

      for (int j = 0; j < nf_._count; ++j)
        _rowMap.SetAt(i++, nf_._index + j);

      FixupReverseMap();

      _width = NumHandlers(); // sorry, no more optimization
    }
    break;

    case c4_Notifier::kRemoveAt:  {
      int lo = nf_._index;
      int hi = nf_._index + nf_._count;

      int j = 0;
      for (int i = 0; i < NumRows(); ++i) {
        int n = (int)_rowMap.GetAt(i);

        if (n >= hi)
          _rowMap.ElementAt(i) -= nf_._count;

        if (!(lo <= n && n < hi))
          _rowMap.SetAt(j++, _rowMap.GetAt(i));
      }

      d4_assert(j + nf_._count == NumRows());
      _rowMap.SetSize(j);

      FixupReverseMap();

      _width = NumHandlers(); // sorry, no more optimization
    }
    break;

    case c4_Notifier::kMove:  {
      // incorrect: may need to move if recnum matters (recs same)
    }
    break;
  }
}

/////////////////////////////////////////////////////////////////////////////

class c4_ProjectSeq: public c4_DerivedSeq {
    c4_DWordArray _colMap; // a bit large, but bytes would be too small
    bool _frozen;
    int _omitCount; // if > 0 then this is a dynamic "project without"

  public:
    c4_ProjectSeq(c4_Sequence &seq_, c4_Sequence &in_, bool, c4_Sequence *out_);
    virtual ~c4_ProjectSeq();

    virtual int NumHandlers()const;
    virtual c4_Handler &NthHandler(int)const;
    virtual const c4_Sequence *HandlerContext(int)const;
    virtual int AddHandler(c4_Handler*);

    virtual bool Get(int, int, c4_Bytes &);
    virtual void Set(int, const c4_Property &, const c4_Bytes &);
};

/////////////////////////////////////////////////////////////////////////////

c4_ProjectSeq::c4_ProjectSeq(c4_Sequence &seq_, c4_Sequence &in_, bool reorder_,
  c4_Sequence *out_): c4_DerivedSeq(seq_), _frozen(!reorder_ && !out_),
  _omitCount(0) {
  // build the array with column indexes
  for (int j = 0; j < in_.NumHandlers(); ++j) {
    int propId = in_.NthPropId(j);
    int idx = _seq.PropIndex(propId);

    // if the j'th property is in the sequence, add it
    if (idx >= 0) {
      // but only if it's not in the out_ view
      if (out_ && out_->PropIndex(propId) >= 0)
        ++_omitCount;
      else
        _colMap.Add(idx);
    }
  }

  // if only reordering, append remaining columns from original view
  if (reorder_) {
    for (int i = 0; i < _seq.NumHandlers(); ++i) {
      int propId = _seq.NthPropId(i);

      // only consider properties we did not deal with before
      if (in_.PropIndex(propId) < 0)
        _colMap.Add(i);
    }

    d4_assert(_colMap.GetSize() == _seq.NumHandlers());
  }
}

c4_ProjectSeq::~c4_ProjectSeq(){}

int c4_ProjectSeq::NumHandlers()const {
  return _frozen ? _colMap.GetSize(): _seq.NumHandlers() - _omitCount;
}

c4_Handler &c4_ProjectSeq::NthHandler(int colNum_)const {
  int n = colNum_ < _colMap.GetSize() ? _colMap.GetAt(colNum_): colNum_;
  return _seq.NthHandler(n);
}

const c4_Sequence *c4_ProjectSeq::HandlerContext(int colNum_)const {
  int n = colNum_ < _colMap.GetSize() ? _colMap.GetAt(colNum_): colNum_;
  return _seq.HandlerContext(n);
}

int c4_ProjectSeq::AddHandler(c4_Handler *handler_) {
  int n = _seq.AddHandler(handler_);
  return _frozen ? _colMap.Add(n): n - _omitCount;
}

bool c4_ProjectSeq::Get(int index_, int propId_, c4_Bytes &buf_) {
  // fixed in 1.8: check that the property is visible
  return PropIndex(propId_) >= 0 && _seq.Get(index_, propId_, buf_);
}

void c4_ProjectSeq::Set(int index_, const c4_Property &prop_, const c4_Bytes
  &bytes_) {
  int n = _seq.NumHandlers();
  _seq.Set(index_, prop_, bytes_);

  // if the number of handlers changed, then one must have been added
  if (n != _seq.NumHandlers()) {
    d4_assert(n == _seq.NumHandlers() - 1);

    if (_frozen)
      _colMap.Add(n);
  }
}

/////////////////////////////////////////////////////////////////////////////

c4_Sequence *f4_CreateFilter(c4_Sequence &seq_, c4_Cursor l_, c4_Cursor h_) {
  return d4_new c4_FilterSeq(seq_, l_, h_);
}

c4_Sequence *f4_CreateSort(c4_Sequence &seq_, c4_Sequence *down_) {
  return d4_new c4_SortSeq(seq_, down_);
}

c4_Sequence *f4_CreateProject(c4_Sequence &seq_, c4_Sequence &in_, bool
  reorder_, c4_Sequence *out_) {
  return d4_new c4_ProjectSeq(seq_, in_, reorder_, out_);
}

/////////////////////////////////////////////////////////////////////////////
