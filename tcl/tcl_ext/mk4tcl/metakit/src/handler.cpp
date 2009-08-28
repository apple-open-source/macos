// handler.cpp --
// $Id: handler.cpp 1230 2007-03-09 15:58:53Z jcw $
// This is part of Metakit, see http://www.equi4.com/metakit.html

/** @file
 * Handlers store data in column-wise format
 */

#include "header.h"
#include "handler.h"
#include "format.h"
#include "field.h"
#include "column.h"
#include "persist.h"

#if !q4_INLINE
#include "handler.inl"
#endif 

/////////////////////////////////////////////////////////////////////////////
// c4_Handler

void c4_Handler::ClearBytes(c4_Bytes &buf_)const {
  static char zeros[8];

  int n = f4_ClearFormat(Property().Type());
  d4_assert(n <= sizeof zeros);

  buf_ = c4_Bytes(zeros, n);
}

int c4_Handler::Compare(int index_, const c4_Bytes &buf_) {
  // create a copy for small data, since ints use a common _item buffer
  c4_Bytes copy(buf_.Contents(), buf_.Size(), buf_.Size() <= 8);

  c4_Bytes data;
  GetBytes(index_, data);

  return f4_CompareFormat(Property().Type(), data, copy);
}

void c4_Handler::Commit(c4_SaveContext &) {
  d4_assert(0);
}

void c4_Handler::OldDefine(char, c4_Persist &) {
  d4_assert(0);
}

// this is how the old "Get" was, keep it until no longer needed
void c4_Handler::GetBytes(int index_, c4_Bytes &buf_, bool copySmall_) {
  int n;
  const void *p = Get(index_, n);
  buf_ = c4_Bytes(p, n, copySmall_ && n <= 8);
}

void c4_Handler::Move(int from_, int to_) {
  if (from_ != to_) {
    c4_Bytes data;
    GetBytes(from_, data);

    Remove(from_, 1);

    if (to_ > from_)
      --to_;

    Insert(to_, data, 1);
  }
}

/////////////////////////////////////////////////////////////////////////////
// c4_HandlerSeq

c4_HandlerSeq::c4_HandlerSeq(c4_Persist *persist_): _persist(persist_), _field
  (0), _parent(0), _numRows(0){}

c4_HandlerSeq::c4_HandlerSeq(c4_HandlerSeq &owner_, c4_Handler *handler_):
  _persist(owner_.Persist()), _field(owner_.FindField(handler_)), _parent
  (&owner_), _numRows(0) {
  for (int i = 0; i < NumFields(); ++i) {
    c4_Field &field = Field(i);
    c4_Property prop(field.Type(), field.Name());

    d4_dbgdef(int n = )AddHandler(f4_CreateFormat(prop,  *this));
    d4_assert(n == i);
  }
}

c4_HandlerSeq::~c4_HandlerSeq() {
  const bool rootLevel = _parent == this;
  c4_Persist *pers = _persist;

  if (rootLevel && pers != 0)
    pers->DoAutoCommit();

  DetachFromParent();
  DetachFromStorage(true);

  for (int i = 0; i < NumHandlers(); ++i)
    delete  &NthHandler(i);
  _handlers.SetSize(0);

  ClearCache();

  if (rootLevel) {
    delete _field;

    d4_assert(pers != 0);
    delete pers;
  }
}

c4_Persist *c4_HandlerSeq::Persist()const {
  return _persist;
}

void c4_HandlerSeq::DefineRoot() {
  d4_assert(_field == 0);
  d4_assert(_parent == 0);

  SetNumRows(1);

  const char *desc = "[]";
  _field = d4_new c4_Field(desc);
  d4_assert(! *desc);

  _parent = this;
}

c4_Handler *c4_HandlerSeq::CreateHandler(const c4_Property &prop_) {
  return f4_CreateFormat(prop_,  *this);
}

c4_Field &c4_HandlerSeq::Definition()const {
  d4_assert(_field != 0);

  return  *_field;
}

void c4_HandlerSeq::DetachFromParent() {
  if (_field != 0) {
    const char *desc = "[]";
    c4_Field f(desc);
    d4_assert(! *desc);
    Restructure(f, false);
    _field = 0;
  }

  _parent = 0;
}

void c4_HandlerSeq::DetachFromStorage(bool full_) {
  if (_persist != 0) {
    int limit = full_ ? 0 : NumFields();

    // get rid of all handlers which might do I/O
    for (int c = NumHandlers(); --c >= 0;) {
      c4_Handler &h = NthHandler(c);

      // all nested fields are detached recursively
      if (IsNested(c))
        for (int r = 0; r < NumRows(); ++r)
          if (h.HasSubview(r))
            SubEntry(c, r).DetachFromStorage(full_);

      if (c >= limit) {
        if (h.IsPersistent()) {
          delete  &h;
          _handlers.RemoveAt(c);
          ClearCache();
        }
      }
    }

    if (full_) {
      //UnmappedAll();
      _persist = 0;
    }
  }
}

void c4_HandlerSeq::DetermineSpaceUsage() {
  for (int c = 0; c < NumFields(); ++c)
  if (IsNested(c)) {
    c4_Handler &h = NthHandler(c);
    for (int r = 0; r < NumRows(); ++r)
      if (h.HasSubview(r))
        SubEntry(c, r).DetermineSpaceUsage();
  }
}

void c4_HandlerSeq::SetNumRows(int numRows_) {
  d4_assert(_numRows >= 0);

  _numRows = numRows_;
}

int c4_HandlerSeq::AddHandler(c4_Handler *handler_) {
  d4_assert(handler_ != 0);

  return _handlers.Add(handler_);
}

const char *c4_HandlerSeq::Description() {
  // 19-01-2003: avoid too dense code, since Sun CC seems to choke on it
  //return _field != 0 ? UseTempBuffer(Definition().DescribeSubFields()) : 0;
  if (_field == 0)
    return 0;
  c4_String s = _field->DescribeSubFields();
  return UseTempBuffer(s);
}

void c4_HandlerSeq::Restructure(c4_Field &field_, bool remove_) {
  //d4_assert(_field != 0);

  // all nested fields must be set up, before we shuffle them around
  for (int k = 0; k < NumHandlers(); ++k)
  if (IsNested(k)) {
    c4_Handler &h = NthHandler(k);
    for (int n = 0; n < NumRows(); ++n)
      if (h.HasSubview(n))
        SubEntry(k, n);
  }

  for (int i = 0; i < field_.NumSubFields(); ++i) {
    c4_Field &nf = field_.SubField(i);
    c4_Property prop(nf.Type(), nf.Name());

    int n = PropIndex(prop.GetId());
    if (n == i)
      continue;

    if (n < 0) {
      _handlers.InsertAt(i, f4_CreateFormat(prop,  *this));
      NthHandler(i).Define(NumRows(), 0);
    } else {
      // move the handler to the front
      d4_assert(n > i);
      _handlers.InsertAt(i, _handlers.GetAt(n));
      _handlers.RemoveAt(++n);
    }

    ClearCache(); // we mess with the order of handler, keep clearing it

    d4_assert(PropIndex(prop.GetId()) == i);
  }

  c4_Field *ofld = _field;
  // special case if we're "restructuring a view out of persistence", see below

  _field = remove_ ? 0 : &field_;

  // let handler do additional init once all have been prepared
  //for (int n = 0; n < NumHandlers(); ++n)
  //    NthHandler(n).Define(NumRows(), 0);

  const char *desc = "[]";
  c4_Field temp(desc);

  // all nested fields are restructured recursively
  for (int j = 0; j < NumHandlers(); ++j)
  if (IsNested(j)) {
    c4_Handler &h = NthHandler(j);
    for (int n = 0; n < NumRows(); ++n)
    if (h.HasSubview(n)) {
      c4_HandlerSeq &seq = SubEntry(j, n);
      if (j < NumFields())
        seq.Restructure(field_.SubField(j), false);
      else if (seq._field != 0)
        seq.Restructure(temp, true);
    }
  }

  if (_parent == this)
    delete ofld;
  // the root table owns its field structure tree
}

int c4_HandlerSeq::NumFields()const {
  return _field != 0 ? _field->NumSubFields(): 0;
}

char c4_HandlerSeq::ColumnType(int index_)const {
  return NthHandler(index_).Property().Type();
}

bool c4_HandlerSeq::IsNested(int index_)const {
  return ColumnType(index_) == 'V';
}

c4_Field &c4_HandlerSeq::Field(int index_)const {
  d4_assert(_field != 0);

  return _field->SubField(index_);
}

void c4_HandlerSeq::Prepare(const t4_byte **ptr_, bool selfDesc_) {
  if (ptr_ != 0) {
    d4_dbgdef(t4_i32 sias = )c4_Column::PullValue(*ptr_);
    d4_assert(sias == 0); // not yet

    if (selfDesc_) {
      t4_i32 n = c4_Column::PullValue(*ptr_);
      if (n > 0) {
        c4_String s = "[" + c4_String((const char*) * ptr_, n) + "]";
        const char *desc = s;

        c4_Field *f = d4_new c4_Field(desc);
        d4_assert(! *desc);

        Restructure(*f, false);
        *ptr_ += n;
      }
    }

    int rows = (int)c4_Column::PullValue(*ptr_);
    if (rows > 0) {
      SetNumRows(rows);

      for (int i = 0; i < NumFields(); ++i)
        NthHandler(i).Define(rows, ptr_);
    }
  }
}

void c4_HandlerSeq::OldPrepare() {
  d4_assert(_persist != 0);

  for (int i = 0; i < NumFields(); ++i) {
    char origType = _field->SubField(i).OrigType();
    NthHandler(i).OldDefine(origType,  *_persist);
  }
}

void c4_HandlerSeq::FlipAllBytes() {
  for (int i = 0; i < NumHandlers(); ++i) {
    c4_Handler &h = NthHandler(i);
    h.FlipBytes();
  }
}

// New 19990903: swap rows in tables without touching the memo fields 
// or subviews on disk.  This is used by the new c4_View::RelocateRows.

void c4_HandlerSeq::ExchangeEntries(int srcPos_, c4_HandlerSeq &dst_, int
  dstPos_) {
  d4_assert(NumHandlers() == dst_.NumHandlers());

  c4_Bytes t1, t2;

  for (int col = 0; col < NumHandlers(); ++col) {
    if (IsNested(col)) {
      d4_assert(dst_.IsNested(col));

      int n;
      c4_HandlerSeq **e1 = (c4_HandlerSeq **)NthHandler(col).Get(srcPos_, n);
      c4_HandlerSeq **e2 = (c4_HandlerSeq **)dst_.NthHandler(col).Get(dstPos_,
        n);
      d4_assert(*e1 != 0 &&  *e2 != 0);

      // swap the two entries
      c4_HandlerSeq *e =  *e1;
      *e1 =  *e2;
      *e2 = e;

      // shorthand, *after* the swap
      c4_HandlerSeq &t1 = SubEntry(col, srcPos_);
      c4_HandlerSeq &t2 = dst_.SubEntry(col, dstPos_);

      // adjust the parents
      t1._parent = this;
      t2._parent = &dst_;

      // reattach the proper field structures
      t1.Restructure(Field(col), false);
      t2.Restructure(dst_.Field(col), false);
    } else {
      d4_assert(ColumnType(col) == dst_.ColumnType(col));

      c4_Handler &h1 = NthHandler(col);
      c4_Handler &h2 = dst_.NthHandler(col);

#if 0 // memo's are 'B' now, but tricky to deal with, so copy them for now
      if (ColumnType(col) == 'M') {
        c4_Column *c1 = h1.GetNthMemoCol(srcPos_, true);
        c4_Column *c2 = h2.GetNthMemoCol(dstPos_, true);

        t4_i32 p1 = c1 ? c1->Position(): 0;
        t4_i32 p2 = c2 ? c2->Position(): 0;

        t4_i32 s1 = c1 ? c1->ColSize(): 0;
        t4_i32 s2 = c2 ? c2->ColSize(): 0;

        d4_assert(false); // broken
        //!h1.SetNthMemoPos(srcPos_, p2, s2, c2);
        //!h2.SetNthMemoPos(dstPos_, p1, s1, c1);
      }
#endif 
      // 10-4-2002: Need to use copies in case either item points into
      // memory that could move, or if access re-uses a shared buffer.
      // The special cases are sufficiently tricky that it's NOT being
      // optimized for now (temp bufs, mmap ptrs, c4_Bytes buffering).

      int n1, n2;
      const void *p1 = h1.Get(srcPos_, n1);
      const void *p2 = h2.Get(dstPos_, n2);

      c4_Bytes t1(p1, n1, true);
      c4_Bytes t2(p2, n2, true);

      h1.Set(srcPos_, t2);
      h2.Set(dstPos_, t1);
    }
  }
}

c4_HandlerSeq &c4_HandlerSeq::SubEntry(int col_, int row_)const {
  d4_assert(IsNested(col_));

  c4_Bytes temp;
  NthHandler(col_).GetBytes(row_, temp);

  d4_assert(temp.Size() == sizeof(c4_HandlerSeq **));
  c4_HandlerSeq **p = (c4_HandlerSeq **)temp.Contents(); // loses const

  d4_assert(p != 0 &&  *p != 0);

  return  **p;
}

c4_Field *c4_HandlerSeq::FindField(const c4_Handler *handler_) {
  for (int i = 0; i < NumFields(); ++i)
    if (handler_ ==  &NthHandler(i))
      return  &Field(i);
  return 0;
}

void c4_HandlerSeq::UnmappedAll() {
  for (int i = 0; i < NumFields(); ++i)
    NthHandler(i).Unmapped();
}

// construct meta view from a pre-parsed field tree structure
// this will one day be converted to directly parse the description string
void c4_HandlerSeq::BuildMeta(int parent_, int colnum_, c4_View &meta_, const
  c4_Field &field_) {
  c4_IntProp pP("P"), pC("C");
  c4_ViewProp pF("F");
  c4_StringProp pN("N"), pT("T");

  int n = meta_.Add(pP[parent_] + pC[colnum_]);
  c4_View fields = pF(meta_[n]);

  for (int i = 0; i < field_.NumSubFields(); ++i) {
    const c4_Field &f = field_.SubField(i);
    char type = f.Type();
    fields.Add(pN[f.Name()] + pT[c4_String(&type, 1)]);
    if (type == 'V')
      BuildMeta(n, i, meta_, f);
  }
}

/////////////////////////////////////////////////////////////////////////////
