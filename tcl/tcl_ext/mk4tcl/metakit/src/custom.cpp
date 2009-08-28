// custom.cpp --
// $Id: custom.cpp 1230 2007-03-09 15:58:53Z jcw $
// This is part of Metakit, see http://www.equi4.com/metakit.html

/** @file
 * Implementation of many custom viewer classes
 */

#include "header.h"

#include "custom.h"
#include "format.h"

/////////////////////////////////////////////////////////////////////////////

class c4_CustomHandler: public c4_Handler {
    c4_CustomSeq *_seq;

  public:
    c4_CustomHandler(const c4_Property &prop_, c4_CustomSeq *seq_);
    virtual ~c4_CustomHandler();

    virtual int ItemSize(int index_);
    virtual const void *Get(int index_, int &length_);
    virtual void Set(int index_, const c4_Bytes &buf_);

    virtual void Insert(int index_, const c4_Bytes &buf_, int count_);
    virtual void Remove(int index_, int count_);
};

/////////////////////////////////////////////////////////////////////////////

c4_CustomHandler::c4_CustomHandler(const c4_Property &prop_, c4_CustomSeq *seq_)
  : c4_Handler(prop_), _seq(seq_) {
  d4_assert(_seq != 0);
}

c4_CustomHandler::~c4_CustomHandler(){}

int c4_CustomHandler::ItemSize(int index_) {
  c4_Bytes &buf = _seq->Buffer();

  int colnum = _seq->PropIndex(Property().GetId());
  d4_assert(colnum >= 0);

  if (!_seq->DoGet(index_, colnum, buf))
    return 0;

  return buf.Size();
}

const void *c4_CustomHandler::Get(int index_, int &length_) {
  c4_Bytes &buf = _seq->Buffer();

  int colnum = _seq->PropIndex(Property().GetId());
  d4_assert(colnum >= 0);

  if (!_seq->DoGet(index_, colnum, buf))
    ClearBytes(buf);

  length_ = buf.Size();
  return buf.Contents();
}

void c4_CustomHandler::Set(int index_, const c4_Bytes &buf_) {
  int colnum = _seq->PropIndex(Property().GetId());
  d4_assert(colnum >= 0);

  _seq->DoSet(index_, colnum, buf_);
}

void c4_CustomHandler::Insert(int, const c4_Bytes &, int) {
  d4_assert(0); //! not yet
}

void c4_CustomHandler::Remove(int, int) {
  d4_assert(0); //! not yet
}

c4_Handler *c4_CustomSeq::CreateHandler(const c4_Property &prop_) {
  return d4_new c4_CustomHandler(prop_, this);
}

/////////////////////////////////////////////////////////////////////////////

c4_CustomSeq::c4_CustomSeq(c4_CustomViewer *viewer_): c4_HandlerSeq(0), _viewer
  (viewer_), _inited(false) {
  d4_assert(_viewer != 0);

  // set up handlers to match a template obtained from the viewer
  c4_View v = viewer_->GetTemplate();

  for (int i = 0; i < v.NumProperties(); ++i)
    PropIndex(v.NthProperty(i));

  _inited = true;
}

c4_CustomSeq::~c4_CustomSeq() {
  delete _viewer;
}

int c4_CustomSeq::NumRows()const {
  return _inited ? _viewer->GetSize(): 0;
}

bool c4_CustomSeq::RestrictSearch(c4_Cursor cursor_, int &pos_, int &count_) {
  if (count_ > 0) {
    int n;
    int o = _viewer->Lookup(cursor_, n);
    // a -1 result means: "don't know, please scan all"
    if (o < 0)
      return count_ > 0;

    if (n > 0) {
      if (pos_ < o) {
        count_ -= o - pos_;
        pos_ = o;
      }

      if (pos_ + count_ > o + n)
        count_ = o + n - pos_;

      if (count_ > 0)
        return true;
    }
  }

  count_ = 0;
  return false;
}

void c4_CustomSeq::InsertAt(int p_, c4_Cursor c_, int n_) {
  _viewer->InsertRows(p_, c_, n_);
}

void c4_CustomSeq::RemoveAt(int p_, int n_) {
  _viewer->RemoveRows(p_, n_);
}

void c4_CustomSeq::Move(int, int) {
  d4_assert(false); //! not yet
}

bool c4_CustomSeq::DoGet(int row_, int col_, c4_Bytes &buf_)const {
  d4_assert(_inited);

  return _viewer->GetItem(row_, col_, buf_);
}

void c4_CustomSeq::DoSet(int row_, int col_, const c4_Bytes &buf_) {
  d4_assert(_inited);

  d4_dbgdef(const bool f = )_viewer->SetItem(row_, col_, buf_);
  d4_assert(f);
}

/////////////////////////////////////////////////////////////////////////////

/** @class c4_CustomViewer
 *
 *  Abstract base class for definition of custom views.
 *
 *  A custom view is a view which can be accessed like any other view, using
 *  row and property operations, but which is fully managed by a customized
 *  "viewer" class.  The viewer will eventually handle all requests for the
 *  view, such as defining its structure and size, as well as providing the
 *  actual data values when requested.
 *
 *  Custom views cannot propagate changes.
 *
 *  To implement a custom view, you must derive your viewer from this base
 *  class and define each of the virtual members.  Then create a new object
 *  of this type on the heap and pass it to the c4_View constructor.  Your
 *  viewer will automatically be destroyed when the last reference to its
 *  view goes away.  See the DBF2MK sample code for an example of a viewer.
 */

c4_CustomViewer::~c4_CustomViewer(){}

/// Locate a row in this view, try to use native searches
int c4_CustomViewer::Lookup(c4_Cursor, int &count_) {
  count_ = GetSize();
  return 0; // not implemented, return entire view range
}

/// Store one data item, supplied as a generic data value
bool c4_CustomViewer::SetItem(int, int, const c4_Bytes &) {
  return false; // default is not modifiable
}

/// Insert one or more copies of a row (if possible)
bool c4_CustomViewer::InsertRows(int, c4_Cursor, int) {
  return false; // default is not modifiable
}

/// Remove one or more rows (this is not always possible)
bool c4_CustomViewer::RemoveRows(int, int) {
  return false; // default is not modifiable
}

/////////////////////////////////////////////////////////////////////////////

class c4_SliceViewer: public c4_CustomViewer {
    c4_View _parent;
    int _first, _limit, _step;

  public:
    c4_SliceViewer(c4_Sequence &seq_, int first_, int limit_, int step_);
    virtual ~c4_SliceViewer();

    virtual c4_View GetTemplate();
    virtual int GetSize();
    virtual bool GetItem(int row_, int col_, c4_Bytes &buf_);
    bool SetItem(int row_, int col_, const c4_Bytes &buf_);
    virtual bool InsertRows(int pos_, c4_Cursor value_, int count_ = 1);
    virtual bool RemoveRows(int pos_, int count_ = 1);
};

c4_SliceViewer::c4_SliceViewer(c4_Sequence &seq_, int first_, int limit_, int
  step_): _parent(&seq_), _first(first_), _limit(limit_), _step(step_) {
  d4_assert(_step != 0);
}

c4_SliceViewer::~c4_SliceViewer(){}

c4_View c4_SliceViewer::GetTemplate() {
  return _parent.Clone(); // could probably return _parent just as well
}

int c4_SliceViewer::GetSize() {
  int n = _limit >= 0 ? _limit : _parent.GetSize();
  if (n < _first)
    n = _first;

  int k = _step < 0 ?  - _step: _step;
  return (n - _first + k - 1) / k;
}

bool c4_SliceViewer::GetItem(int row_, int col_, c4_Bytes &buf_) {
  row_ = _first + _step *(_step > 0 ? row_ : row_ - GetSize() + 1);

  return _parent.GetItem(row_, col_, buf_);
}

bool c4_SliceViewer::SetItem(int row_, int col_, const c4_Bytes &buf_) {
  row_ = _first + _step *(_step > 0 ? row_ : row_ - GetSize() + 1);

  _parent.SetItem(row_, col_, buf_);
  return true;
}

bool c4_SliceViewer::InsertRows(int pos_, c4_Cursor value_, int count_) {
  if (_step != 1)
    return false;

  pos_ = _first + _step *(_step > 0 ? pos_ : pos_ - GetSize() + 1);
  if (_limit >= 0)
    _limit += count_;

  _parent.InsertAt(pos_,  *value_, count_);
  return true;
}

bool c4_SliceViewer::RemoveRows(int pos_, int count_) {
  if (_step != 1)
    return false;

  pos_ = _first + _step *(_step > 0 ? pos_ : pos_ - GetSize() + 1);
  if (_limit >= 0)
    _limit -= count_;

  _parent.RemoveAt(pos_, count_);
  return true;
}

c4_CustomViewer *f4_CustSlice(c4_Sequence &seq_, int first_, int limit_, int
  step_) {
  return d4_new c4_SliceViewer(seq_, first_, limit_, step_);
}

/////////////////////////////////////////////////////////////////////////////

class c4_ProductViewer: public c4_CustomViewer {
    c4_View _parent, _argView, _template;

  public:
    c4_ProductViewer(c4_Sequence &seq_, const c4_View &view_);
    virtual ~c4_ProductViewer();

    virtual c4_View GetTemplate();
    virtual int GetSize();
    virtual bool GetItem(int row_, int col_, c4_Bytes &buf_);
};

c4_ProductViewer::c4_ProductViewer(c4_Sequence &seq_, const c4_View &view_):
  _parent(&seq_), _argView(view_), _template(_parent.Clone()) {
  for (int i = 0; i < _argView.NumProperties(); ++i)
    _template.AddProperty(_argView.NthProperty(i));
}

c4_ProductViewer::~c4_ProductViewer(){}

c4_View c4_ProductViewer::GetTemplate() {
  return _template;
}

int c4_ProductViewer::GetSize() {
  return _parent.GetSize() *_argView.GetSize();
}

bool c4_ProductViewer::GetItem(int row_, int col_, c4_Bytes &buf_) {
  c4_View v = _parent;

  if (col_ < v.NumProperties()) {
    row_ /= _argView.GetSize();
  } else {
    v = _argView;
    row_ %= _argView.GetSize();
    col_ = v.FindProperty(_template.NthProperty(col_).GetId());

    d4_assert(col_ >= 0);
  }

  return v.GetItem(row_, col_, buf_);
}

c4_CustomViewer *f4_CustProduct(c4_Sequence &seq_, const c4_View &view_) {
  return d4_new c4_ProductViewer(seq_, view_);
}

/////////////////////////////////////////////////////////////////////////////

class c4_RemapWithViewer: public c4_CustomViewer {
    c4_View _parent, _argView;

  public:
    c4_RemapWithViewer(c4_Sequence &seq_, const c4_View &view_);
    virtual ~c4_RemapWithViewer();

    virtual c4_View GetTemplate();
    virtual int GetSize();
    virtual bool GetItem(int row_, int col_, c4_Bytes &buf_);
    bool SetItem(int row_, int col_, const c4_Bytes &buf_);
};

c4_RemapWithViewer::c4_RemapWithViewer(c4_Sequence &seq_, const c4_View &view_)
  : _parent(&seq_), _argView(view_){}

c4_RemapWithViewer::~c4_RemapWithViewer(){}

c4_View c4_RemapWithViewer::GetTemplate() {
  return _parent.Clone(); // could probably return _parent just as well
}

int c4_RemapWithViewer::GetSize() {
  return _argView.GetSize();
}

bool c4_RemapWithViewer::GetItem(int row_, int col_, c4_Bytes &buf_) {
  const c4_Property &map = _argView.NthProperty(0);
  d4_assert(map.Type() == 'I');

  row_ = ((const c4_IntProp &)map)(_argView[row_]);

  return _parent.GetItem(row_, col_, buf_);
}

bool c4_RemapWithViewer::SetItem(int row_, int col_, const c4_Bytes &buf_) {
  const c4_Property &map = _argView.NthProperty(0);
  d4_assert(map.Type() == 'I');

  row_ = ((const c4_IntProp &)map)(_argView[row_]);

  _parent.SetItem(row_, col_, buf_);
  return true;
}

c4_CustomViewer *f4_CustRemapWith(c4_Sequence &seq_, const c4_View &view_) {
  return d4_new c4_RemapWithViewer(seq_, view_);
}

/////////////////////////////////////////////////////////////////////////////

class c4_PairViewer: public c4_CustomViewer {
    c4_View _parent, _argView, _template;

  public:
    c4_PairViewer(c4_Sequence &seq_, const c4_View &view_);
    virtual ~c4_PairViewer();

    virtual c4_View GetTemplate();
    virtual int GetSize();
    virtual bool GetItem(int row_, int col_, c4_Bytes &buf_);
    bool SetItem(int row_, int col_, const c4_Bytes &buf_);
    virtual bool InsertRows(int pos_, c4_Cursor value_, int count_ = 1);
    virtual bool RemoveRows(int pos_, int count_ = 1);
};

c4_PairViewer::c4_PairViewer(c4_Sequence &seq_, const c4_View &view_): _parent
  (&seq_), _argView(view_), _template(_parent.Clone()) {
  for (int i = 0; i < _argView.NumProperties(); ++i)
    _template.AddProperty(_argView.NthProperty(i));
}

c4_PairViewer::~c4_PairViewer(){}

c4_View c4_PairViewer::GetTemplate() {
  return _template;
}

int c4_PairViewer::GetSize() {
  return _parent.GetSize();
}

bool c4_PairViewer::GetItem(int row_, int col_, c4_Bytes &buf_) {
  c4_View v = _parent;

  if (col_ >= v.NumProperties()) {
    v = _argView;
    col_ = v.FindProperty(_template.NthProperty(col_).GetId());
    d4_assert(col_ >= 0);
  }

  return v.GetItem(row_, col_, buf_);
}

bool c4_PairViewer::SetItem(int row_, int col_, const c4_Bytes &buf_) {
  c4_View v = _parent;

  if (col_ >= v.NumProperties()) {
    v = _argView;
    col_ = v.FindProperty(_template.NthProperty(col_).GetId());
    d4_assert(col_ >= 0);
  }

  v.SetItem(row_, col_, buf_);
  return true;
}

bool c4_PairViewer::InsertRows(int pos_, c4_Cursor value_, int count_) {
  _parent.InsertAt(pos_,  *value_, count_);
  _argView.InsertAt(pos_,  *value_, count_);
  return true;
}

bool c4_PairViewer::RemoveRows(int pos_, int count_) {
  _parent.RemoveAt(pos_, count_);
  _argView.RemoveAt(pos_, count_);
  return true;
}

c4_CustomViewer *f4_CustPair(c4_Sequence &seq_, const c4_View &view_) {
  return d4_new c4_PairViewer(seq_, view_);
}

/////////////////////////////////////////////////////////////////////////////

class c4_ConcatViewer: public c4_CustomViewer {
    c4_View _parent, _argView;

  public:
    c4_ConcatViewer(c4_Sequence &seq_, const c4_View &view_);
    virtual ~c4_ConcatViewer();

    virtual c4_View GetTemplate();
    virtual int GetSize();
    virtual bool GetItem(int row_, int col_, c4_Bytes &buf_);
    bool SetItem(int row_, int col_, const c4_Bytes &buf_);
};

c4_ConcatViewer::c4_ConcatViewer(c4_Sequence &seq_, const c4_View &view_):
  _parent(&seq_), _argView(view_){}

c4_ConcatViewer::~c4_ConcatViewer(){}

c4_View c4_ConcatViewer::GetTemplate() {
  return _parent.Clone(); // could probably return _parent just as well
}

int c4_ConcatViewer::GetSize() {
  return _parent.GetSize() + _argView.GetSize();
}

bool c4_ConcatViewer::GetItem(int row_, int col_, c4_Bytes &buf_) {
  c4_View v = _parent;

  if (row_ >= _parent.GetSize()) {
    v = _argView;
    row_ -= _parent.GetSize();
    col_ = v.FindProperty(_parent.NthProperty(col_).GetId());

    if (col_ < 0)
      return false;
  }

  return v.GetItem(row_, col_, buf_);
}

bool c4_ConcatViewer::SetItem(int row_, int col_, const c4_Bytes &buf_) {
  c4_View v = _parent;

  if (row_ >= _parent.GetSize()) {
    v = _argView;
    row_ -= _parent.GetSize();
    col_ = v.FindProperty(_parent.NthProperty(col_).GetId());
    d4_assert(col_ >= 0);
  }

  v.SetItem(row_, col_, buf_);
  return true;
}

c4_CustomViewer *f4_CustConcat(c4_Sequence &seq_, const c4_View &view_) {
  return d4_new c4_ConcatViewer(seq_, view_);
}

/////////////////////////////////////////////////////////////////////////////

class c4_RenameViewer: public c4_CustomViewer {
    c4_View _parent, _template;

  public:
    c4_RenameViewer(c4_Sequence &seq_, const c4_Property &old_, const
      c4_Property &new_);
    virtual ~c4_RenameViewer();

    virtual c4_View GetTemplate();
    virtual int GetSize();
    virtual bool GetItem(int row_, int col_, c4_Bytes &buf_);
    virtual bool SetItem(int row_, int col_, const c4_Bytes &buf_);
    //virtual bool InsertRows(int pos_, c4_Cursor value_, int count_=1);
    //virtual bool RemoveRows(int pos_, int count_=1);
};

c4_RenameViewer::c4_RenameViewer(c4_Sequence &seq_, const c4_Property &old_,
  const c4_Property &new_): _parent(&seq_) {
  for (int i = 0; i < _parent.NumProperties(); ++i) {
    const c4_Property &prop = _parent.NthProperty(i);
    _template.AddProperty(prop.GetId() == old_.GetId() ? new_ : prop);
  }
}

c4_RenameViewer::~c4_RenameViewer(){}

c4_View c4_RenameViewer::GetTemplate() {
  return _template;
}

int c4_RenameViewer::GetSize() {
  return _parent.GetSize();
}

bool c4_RenameViewer::GetItem(int row_, int col_, c4_Bytes &buf_) {
  return _parent.GetItem(row_, col_, buf_);
}

bool c4_RenameViewer::SetItem(int row_, int col_, const c4_Bytes &buf_) {
  _parent.SetItem(row_, col_, buf_);
  return true;
}

c4_CustomViewer *f4_CustRename(c4_Sequence &seq_, const c4_Property &old_,
  const c4_Property &new_) {
  return d4_new c4_RenameViewer(seq_, old_, new_);
}

/////////////////////////////////////////////////////////////////////////////

class c4_GroupByViewer: public c4_CustomViewer {
    c4_View _parent, _keys, _sorted, _temp;
    c4_Property _result;
    c4_DWordArray _map;

    int ScanTransitions(int lo_, int hi_, t4_byte *flags_, const c4_View
      &match_)const;

  public:
    c4_GroupByViewer(c4_Sequence &seq_, const c4_View &keys_, const c4_Property
      &result_);
    virtual ~c4_GroupByViewer();

    virtual c4_View GetTemplate();
    virtual int GetSize();
    virtual bool GetItem(int row_, int col_, c4_Bytes &buf_);
};

c4_GroupByViewer::c4_GroupByViewer(c4_Sequence &seq_, const c4_View &keys_,
  const c4_Property &result_): _parent(&seq_), _keys(keys_), _result(result_) {
  _sorted = _parent.SortOn(_keys);
  int n = _sorted.GetSize();

  c4_Bytes temp;
  t4_byte *buf = temp.SetBufferClear(n);

  int groups = 0;
  if (n > 0) {
    ++buf[0]; // the first entry is always a transition
    groups = 1+ScanTransitions(1, n, buf, _sorted.Project(_keys));
  }

  // set up a map pointing to each transition
  _map.SetSize(groups + 1);
  int j = 0;

  for (int i = 0; i < n; ++i)
    if (buf[i])
      _map.SetAt(j++, i);

  // also append an entry to point just past the end
  _map.SetAt(j, n);

  d4_assert(_map.GetAt(0) == 0);
  d4_assert(j == groups);
}

c4_GroupByViewer::~c4_GroupByViewer(){}

int c4_GroupByViewer::ScanTransitions(int lo_, int hi_, t4_byte *flags_, const
  c4_View &match_)const {
  d4_assert(lo_ > 0);

  int m = hi_ - lo_;
  d4_assert(m >= 0);

  // done if nothing left or if entire range is identical
  if (m == 0 || match_[lo_ - 1] == match_[hi_ - 1])
    return 0;

  // range has a transition, done if it is exactly of size one
  if (m == 1) {
    ++(flags_[lo_]);
    return 1;
  }

  // use binary splitting if the range has enough entries
  if (m >= 5)
    return ScanTransitions(lo_, lo_ + m / 2, flags_, match_) + ScanTransitions
      (lo_ + m / 2, hi_, flags_, match_);

  // else use a normal linear scan
  int n = 0;

  for (int i = lo_; i < hi_; ++i)
  if (match_[i] != match_[i - 1]) {
    ++(flags_[i]);
    ++n;
  }

  return n;
}

c4_View c4_GroupByViewer::GetTemplate() {
  c4_View v = _keys.Clone();
  v.AddProperty(_result);

  return v;
}

int c4_GroupByViewer::GetSize() {
  d4_assert(_map.GetSize() > 0);

  return _map.GetSize() - 1;
}

bool c4_GroupByViewer::GetItem(int row_, int col_, c4_Bytes &buf_) {
  if (col_ < _keys.NumProperties())
    return _sorted.GetItem(_map.GetAt(row_), col_, buf_);

  d4_assert(col_ == _keys.NumProperties());

  t4_i32 count;
  switch (_result.Type()) {
    case 'I':
      count = _map.GetAt(row_ + 1) - _map.GetAt(row_);
      buf_ = c4_Bytes(&count, sizeof count, true);
      break;
    case 'V':
      _temp = _sorted.Slice(_map.GetAt(row_), _map.GetAt(row_ + 1))
        .ProjectWithout(_keys);
      buf_ = c4_Bytes(&_temp, sizeof _temp, true);
      break;
    default:
      d4_assert(0);
  }

  return true;
}

c4_CustomViewer *f4_CustGroupBy(c4_Sequence &seq_, const c4_View &template_,
  const c4_Property &result_) {
  return d4_new c4_GroupByViewer(seq_, template_, result_);
}

/////////////////////////////////////////////////////////////////////////////

class c4_JoinPropViewer: public c4_CustomViewer {
    c4_View _parent, _template;
    c4_ViewProp _sub;
    int _subPos, _subWidth;
    c4_DWordArray _base, _offset;

  public:
    c4_JoinPropViewer(c4_Sequence &seq_, const c4_ViewProp &sub_, bool outer_);
    virtual ~c4_JoinPropViewer();

    virtual c4_View GetTemplate();
    virtual int GetSize();
    virtual bool GetItem(int row_, int col_, c4_Bytes &buf_);
};

c4_JoinPropViewer::c4_JoinPropViewer(c4_Sequence &seq_, const c4_ViewProp &sub_,
  bool outer_): _parent(&seq_), _sub(sub_), _subPos(_parent.FindProperty
  (sub_.GetId())), _subWidth(0) {
  d4_assert(_subPos >= 0);

  for (int k = 0; k < _parent.NumProperties(); ++k) {
    if (k != _subPos)
      _template.AddProperty(_parent.NthProperty(k));
    else
    // if there are no rows, then this join does very little anyway
    //! OOPS: if this is an unattached view, then the subviews can differ
    if (_parent.GetSize() > 0) {
      c4_View view = sub_(_parent[0]);
      for (int l = 0; l < view.NumProperties(); ++l) {
        _template.AddProperty(view.NthProperty(l));
        ++_subWidth;
      }
    }
  }

  _base.SetSize(0, 5);
  _offset.SetSize(0, 5);

  for (int i = 0; i < _parent.GetSize(); ++i) {
    c4_View v = _sub(_parent[i]);

    int n = v.GetSize();
    if (n == 0 && outer_) {
      _base.Add(i);
      _offset.Add(~(t4_i32)0); // special null entry for outer joins
    } else
    for (int j = 0; j < n; ++j) {
      _base.Add(i);
      _offset.Add(j);
    }
  }
}

c4_JoinPropViewer::~c4_JoinPropViewer(){}

c4_View c4_JoinPropViewer::GetTemplate() {
  return _template;
}

int c4_JoinPropViewer::GetSize() {
  return _base.GetSize();
}

bool c4_JoinPropViewer::GetItem(int row_, int col_, c4_Bytes &buf_) {
  c4_View v = _parent;
  int r = _base.GetAt(row_);

  if (col_ >= _subPos)
  if (col_ >= _subPos + _subWidth) {
    col_ -= _subWidth - 1;
  } else {
    v = _sub(_parent[r]);
    r = _offset.GetAt(row_);
    if (r < 0)
      return false;
    // if this is a null row in an outer join

    col_ = v.FindProperty(_template.NthProperty(col_).GetId());
    if (col_ < 0)
      return false;
    // if subview doesn't have all properties
  }

  return v.GetItem(r, col_, buf_);
}

c4_CustomViewer *f4_CustJoinProp(c4_Sequence &seq_, const c4_ViewProp &sub_,
  bool outer_) {
  return d4_new c4_JoinPropViewer(seq_, sub_, outer_);
}

/////////////////////////////////////////////////////////////////////////////

class c4_JoinViewer: public c4_CustomViewer {
    c4_View _parent, _argView, _template;
    c4_DWordArray _base, _offset;

  public:
    c4_JoinViewer(c4_Sequence &seq_, const c4_View &keys_, const c4_View &view_,
      bool outer_);
    virtual ~c4_JoinViewer();

    virtual c4_View GetTemplate();
    virtual int GetSize();
    virtual bool GetItem(int row_, int col_, c4_Bytes &buf_);
};

c4_JoinViewer::c4_JoinViewer(c4_Sequence &seq_, const c4_View &keys_, const
  c4_View &view_, bool outer_): _parent(&seq_), _argView(view_.SortOn(keys_)) {
  // why not in GetTemplate, since we don't need to know this...
  _template = _parent.Clone();
  for (int l = 0; l < _argView.NumProperties(); ++l)
    _template.AddProperty(_argView.NthProperty(l));

  c4_View sorted = _parent.SortOn(keys_).Project(keys_);
  c4_View temp = _argView.Project(keys_);

  _base.SetSize(0, 5);
  _offset.SetSize(0, 5);

  int j = 0, n = 0;

  for (int i = 0; i < sorted.GetSize(); ++i) {
    int orig = _parent.GetIndexOf(sorted[i]);
    d4_assert(orig >= 0);

    if (i > 0 && sorted[i] == sorted[i - 1]) {
      // if last key was same, repeat the same join
      int last = _offset.GetSize() - n;
      for (int k = 0; k < n; ++k) {
        _base.Add(orig);
        _offset.Add(_offset.GetAt(last + k));
      }
    } else
     { // no, this is a new combination
      bool match = false;

      // advance until the temp view entry is >= this sorted entry
      while (j < temp.GetSize())
      if (sorted[i] <= temp[j]) {
        match = sorted[i] == temp[j];
        break;
      } else
        ++j;

      n = 0;

      if (match) {
        do {
          _base.Add(orig);
          _offset.Add(j);
          ++n;
        } while (++j < temp.GetSize() && temp[j] == temp[j - 1]);
      } else if (outer_) {
        // no match, add an entry anyway if this is an outer join
        _base.Add(orig);
        _offset.Add(~(t4_i32)0); // special null entry
        ++n;
      }
    }
  }
}

c4_JoinViewer::~c4_JoinViewer(){}

c4_View c4_JoinViewer::GetTemplate() {
  return _template;
}

int c4_JoinViewer::GetSize() {
  return _base.GetSize();
}

bool c4_JoinViewer::GetItem(int row_, int col_, c4_Bytes &buf_) {
  c4_View v = _parent;
  int r = _base.GetAt(row_);

  if (col_ >= v.NumProperties()) {
    v = _argView;
    r = _offset.GetAt(row_);
    if (r < 0)
      return false;
    // if this is a null row in an outer join

    col_ = v.FindProperty(_template.NthProperty(col_).GetId());
    if (col_ < 0)
      return false;
    // if second view doesn't have all properties
  }

  return v.GetItem(r, col_, buf_);
}

#if 0
bool c4_JoinViewer::GetItem(int row_, int col_, c4_Bytes &buf_) {
  c4_View v = _parent;

  int o = 0;
  int r = _offset.GetAt(row_);

  if (r < 0) {
    o = ~r;
    if (o == 0)
      return false;
    // if this is a null row in an outer join
    r -= o;
  }

  if (col_ >= v.NumProperties()) {
    v = _argView;
    r = _o;

    col_ = v.FindProperty(_template.NthProperty(col_));
    if (col_ < 0)
      return false;
    // if second view doesn't have all properties
  }

  return v.GetItem(r, col_, buf_);
}

#endif 

c4_CustomViewer *f4_CustJoin(c4_Sequence &seq_, const c4_View &keys_, const
  c4_View &view_, bool outer_) {
  return d4_new c4_JoinViewer(seq_, keys_, view_, outer_);
}

/////////////////////////////////////////////////////////////////////////////
