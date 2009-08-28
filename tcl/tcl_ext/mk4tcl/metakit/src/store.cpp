// store.cpp --
// $Id: store.cpp 1230 2007-03-09 15:58:53Z jcw $
// This is part of Metakit, the homepage is http://www.equi4.com/metakit.html

/** @file
 * Storage management and several other loose ends
 */

#include "header.h"
#include "handler.h"  // 19990906
#include "store.h"
#include "field.h"
#include "persist.h"
#include "format.h"   // 19990906

#include "mk4io.h"    // 19991104

#if !q4_INLINE
#include "store.inl"
#endif 

/////////////////////////////////////////////////////////////////////////////

c4_Dependencies::c4_Dependencies() {
  _refs.SetSize(0, 3); // a little optimization
}

c4_Dependencies::~c4_Dependencies(){}

void c4_Dependencies::Add(c4_Sequence *seq_) {
  for (int i = 0; i < _refs.GetSize(); ++i)
    d4_assert(_refs.GetAt(i) != seq_);

  _refs.Add(seq_);
}

bool c4_Dependencies::Remove(c4_Sequence *seq_) {
  int n = _refs.GetSize() - 1;
  d4_assert(n >= 0);

  for (int i = 0; i <= n; ++i)
  if (_refs.GetAt(i) == seq_) {
    _refs.SetAt(i, _refs.GetAt(n));
    _refs.SetSize(n);
    return n > 0;
  }

  d4_assert(0); // dependency not found
  return true;
}

/////////////////////////////////////////////////////////////////////////////

c4_Notifier::~c4_Notifier() {
  if (_type > kNone && _origin->GetDependencies()) {
    c4_PtrArray &refs = _origin->GetDependencies()->_refs;

    for (int i = 0; i < refs.GetSize(); ++i) {
      c4_Sequence *seq = (c4_Sequence*)refs.GetAt(i);
      d4_assert(seq != 0);

      seq->PostChange(*this);

      if (_chain && _chain->_origin == seq) {
        c4_Notifier *next = _chain->_next;
        _chain->_next = 0;

        delete _chain;

        _chain = next;
      }
    }
  }

  d4_assert(!_chain);
  d4_assert(!_next);
}

void c4_Notifier::StartSetAt(int index_, c4_Cursor &cursor_) {
  _type = kSetAt;
  _index = index_;
  _cursor = &cursor_;

  Notify();
}

void c4_Notifier::StartInsertAt(int i_, c4_Cursor &cursor_, int n_) {
  _type = kInsertAt;
  _index = i_;
  _cursor = &cursor_;
  _count = n_;

  Notify();
}

void c4_Notifier::StartRemoveAt(int index_, int count_) {
  _type = kRemoveAt;
  _index = index_;
  _count = count_;

  Notify();
}

void c4_Notifier::StartMove(int from_, int to_) {
  _type = kMove;
  _index = from_;
  _count = to_;

  Notify();
}

void c4_Notifier::StartSet(int i_, int propId_, const c4_Bytes &buf_) {
  _type = kSet;
  _index = i_;
  _propId = propId_;
  _bytes = &buf_;

  Notify();
}

void c4_Notifier::Notify() {
  d4_assert(_origin->GetDependencies() != 0);
  c4_PtrArray &refs = _origin->GetDependencies()->_refs;

  int n = refs.GetSize();
  d4_assert(n > 0);

  c4_Notifier **rover = &_chain;

  for (int i = 0; i < n; ++i) {
    c4_Sequence *seq = (c4_Sequence*)refs.GetAt(i);
    d4_assert(seq != 0);

    c4_Notifier *ptr = seq->PreChange(*this);
    if (ptr) {
      d4_assert(ptr->_origin == seq);

      d4_assert(! *rover);
      *rover = ptr;
      rover = &ptr->_next;
    }
  }
}

/////////////////////////////////////////////////////////////////////////////

/** @class c4_Storage
 *
 *  Manager for persistent storage of view structures.
 *
 *  The storage class uses a view, with additional functionality to be able 
 *  to store and reload the data it contains (including nested subviews).
 *
 *  By default, data is loaded on demand, i.e. whenever data which has
 *  not yet been referenced is used for the first time.  Loading is limited
 *  to the lifetime of this storage object, since the storage object carries
 *  the file descriptor with it that is needed to access the data file.
 *
 *  To save changes, call the Commit member.  This is the only time
 *  that data is written to file - when using a read-only file simply avoid
 *  calling Commit.
 *
 *  The LoadFromStream and SaveToStream members can be used to
 *  serialize the contents of this storage row using only sequential I/O
 *  (no seeking, only read or write calls).
 *
 *  The data storage mechanism implementation provides fail-safe operation:
 *  if anything prevents Commit from completing its task, the last
 *  succesfully committed version of the saved data will be recovered on
 *  the next open. This also includes changes made to the table structure. 
 *
 *  The following code creates a view with 1 row and stores it on file:
 * @code
 *    c4_StringProp pName ("Name");
 *    c4_IntProp pAge ("Age");
 *
 *    c4_Storage storage ("myfile.dat", true);
 *    c4_View myView = storage.GetAs("Musicians[Name:S,Age:I]");
 *
 *    myView.Add(pName ["John Williams"] + pAge [43]);
 *
 *    storage.Commit();
 * @endcode
 */

c4_Storage::c4_Storage() {
  // changed to r/o, now that commits don't crash on it anymore
  Initialize(*d4_new c4_Strategy, true, 0);
}

c4_Storage::c4_Storage(c4_Strategy &strategy_, bool owned_, int mode_) {
  Initialize(strategy_, owned_, mode_);
  Persist()->LoadAll();
}

c4_Storage::c4_Storage(const char *fname_, int mode_) {
  c4_FileStrategy *strat = d4_new c4_FileStrategy;
  strat->DataOpen(fname_, mode_);

  Initialize(*strat, true, mode_);
  if (strat->IsValid())
    Persist()->LoadAll();
}

c4_Storage::c4_Storage(const c4_View &root_) {
  if (root_.Persist() != 0)
  // only restore if view was indeed persistent
    *(c4_View*)this = root_;
  else
  // if this was not possible, start with a fresh empty storage
    Initialize(*d4_new c4_Strategy, true, 0);
}

c4_Storage::~c4_Storage() {
  // cannot unmap here, because there may still be an autocommit pending
  //((c4_HandlerSeq*) _seq)->UnmapAll();
}

void c4_Storage::Initialize(c4_Strategy &strategy_, bool owned_, int mode_) {
  c4_Persist *pers = d4_new c4_Persist(strategy_, owned_, mode_);
  c4_HandlerSeq *seq = d4_new c4_HandlerSeq(pers);
  seq->DefineRoot();
  *(c4_View*)this = seq;
  pers->SetRoot(seq);
}

/// Get or set a named view in this storage object
c4_ViewRef c4_Storage::View(const char *name_) {
  /*
  The easy solution would seem to be:

  c4_ViewProp prop (name_);
  return prop (Contents());

  But this does not work, because the return value would point to
  an object allocated on the stack.

  Instead, make sure the view *has* such a property, and use the
  one inside the c4_Handler for it (since this will stay around).
   */

  //  int n = _root->PropIndex(c4_ViewProp (name_));

  c4_ViewProp prop(name_);
  int n = AddProperty(prop);
  d4_assert(n >= 0);

  // the following is an expression of the form "property (rowref)"
  return NthProperty(n)(GetAt(0));
}

/// Get a named view, redefining it to match the given structure
c4_View c4_Storage::GetAs(const char *description_) {
  d4_assert(description_ != 0);

  // Dec 2001: now that GetAs is being used so much more frequently, 
  // add a quick check to see whether restructuring is needed at all
  const char *q = strchr(description_, '[');
  if (q != 0) {
    c4_String vname(description_, q - description_);
    const char *d = Description(vname);
    if (d != 0) {
      c4_String desc(d);
      if (("[" + desc + "]").CompareNoCase(q) == 0)
        return View(vname);
    }
  }

  c4_Field *field = d4_new c4_Field(description_);
  d4_assert(field != 0);

  d4_assert(! *description_);

  c4_String name = field->Name();
  d4_assert(!name.IsEmpty());

  c4_Field &curr = Persist()->Root().Definition();

  c4_String newField = "," + field->Description();
  bool keep = newField.Find('[') >= 0;

  c4_String newDef;

  // go through all subfields
  for (int i = 0; i < curr.NumSubFields(); ++i) {
    c4_Field &of = curr.SubField(i);
    if (of.Name().CompareNoCase(name) == 0) {
      if (field->IsRepeating())
        newDef += newField;
      // else new is not a repeating entry, so drop this entire field

      newField.Empty(); // don't append it later on
      continue;
    }

    newDef += "," + of.Description(); // keep original field
  }

  if (keep)
  // added 19990824 ignore if deletion
    newDef += newField;
  // appends new definition if not found earlier

  delete field;

  const char *p = newDef;
  SetStructure(*p ? ++p: p); // skip the leading comma

  if (!keep)
  // 19990916: avoid adding an empty view again
    return c4_View();

  return View(name);
}

/// Define the complete view structure of the storage
void c4_Storage::SetStructure(const char *description_) {
  d4_assert(description_ != 0);

  if (description_ != Description()) {
    c4_String s = "[" + c4_String(description_) + "]";
    description_ = s;

    c4_Field *field = d4_new c4_Field(description_);
    d4_assert(! *description_);

    d4_assert(field != 0);
    Persist()->Root().Restructure(*field, false);
  }
}

/// Return the strategy object associated with this storage
c4_Strategy &c4_Storage::Strategy()const {
  return Persist()->Strategy();
}

/// Return a description of the view structure (default is all)
const char *c4_Storage::Description(const char *name_) {
  if (name_ == 0 ||  *name_ == 0)
    return c4_View::Description();

  c4_View v = View(name_);
  return v.Description();
}

/// Define the storage to use for differential commits
bool c4_Storage::SetAside(c4_Storage &aside_) {
  c4_Persist *pers = Persist();
  bool f = pers->SetAside(aside_);
  // adjust our copy when the root view has been replaced
  *(c4_View*)this = &pers->Root();
  return f;
}

/// Return storage used for differential commits, or null
c4_Storage *c4_Storage::GetAside()const {
  return Persist()->GetAside();
}

/// Flush pending changes to file right now
bool c4_Storage::Commit(bool full_) {
  return Strategy().IsValid() && Persist()->Commit(full_);
}

/** (Re)initialize for on-demand loading
 *
 *  Calling Rollback will cancel all uncommitted changes.
 */
bool c4_Storage::Rollback(bool full_) {
  c4_Persist *pers = Persist();
  bool f = Strategy().IsValid() && pers->Rollback(full_);
  // adjust our copy when the root view has been replaced
  *(c4_View*)this = &pers->Root();
  return f;
}

/// Set storage up to always call Commit in the destructor
bool c4_Storage::AutoCommit(bool flag_) {
  return Persist()->AutoCommit(flag_);
}

/// Load contents from the specified input stream
bool c4_Storage::LoadFrom(c4_Stream &stream_) {
  c4_HandlerSeq *newRoot = c4_Persist::Load(&stream_);
  if (newRoot == 0)
    return false;

  // fix commit-after-load bug, by using a full view copy
  // this is inefficient, but avoids mapping/strategy problems
  c4_View temp(newRoot);

  SetSize(0);
  SetStructure(temp.Description());
  InsertAt(0, temp);

  return true;
}

/// Save contents to the specified output stream
void c4_Storage::SaveTo(c4_Stream &stream_) {
  c4_Persist::Save(&stream_, Persist()->Root());
}

t4_i32 c4_Storage::FreeSpace(t4_i32 *bytes_) {
  return Persist()->FreeBytes(bytes_);
}

/////////////////////////////////////////////////////////////////////////////

c4_DerivedSeq::c4_DerivedSeq(c4_Sequence &seq_): _seq(seq_) {
  _seq.Attach(this);
}

c4_DerivedSeq::~c4_DerivedSeq() {
  _seq.Detach(this);
}

int c4_DerivedSeq::RemapIndex(int index_, const c4_Sequence *seq_)const {
  return seq_ == this ? index_ : _seq.RemapIndex(index_, seq_);
}

int c4_DerivedSeq::NumRows()const {
  return _seq.NumRows();
}

int c4_DerivedSeq::NumHandlers()const {
  return _seq.NumHandlers();
}

c4_Handler &c4_DerivedSeq::NthHandler(int colNum_)const {
  return _seq.NthHandler(colNum_);
}

const c4_Sequence *c4_DerivedSeq::HandlerContext(int colNum_)const {
  return _seq.HandlerContext(colNum_);
}

int c4_DerivedSeq::AddHandler(c4_Handler *handler_) {
  return _seq.AddHandler(handler_);
}

c4_Handler *c4_DerivedSeq::CreateHandler(const c4_Property &prop_) {
  return _seq.CreateHandler(prop_);
}

void c4_DerivedSeq::SetNumRows(int size_) {
  _seq.SetNumRows(size_);
}

c4_Notifier *c4_DerivedSeq::PreChange(c4_Notifier &nf_) {
  if (!GetDependencies())
    return 0;

  c4_Notifier *chg = d4_new c4_Notifier(this);

  switch (nf_._type) {
    case c4_Notifier::kSetAt: chg->StartSetAt(nf_._index,  *nf_._cursor);
    break;

    case c4_Notifier::kSet: chg->StartSet(nf_._index, nf_._propId,  *nf_._bytes)
      ;
    break;

    case c4_Notifier::kInsertAt: chg->StartInsertAt(nf_._index,  *nf_._cursor,
      nf_._count);
    break;

    case c4_Notifier::kRemoveAt: chg->StartRemoveAt(nf_._index, nf_._count);
    break;

    case c4_Notifier::kMove: chg->StartMove(nf_._index, nf_._count);
    break;
  }

  return chg;
}

/////////////////////////////////////////////////////////////////////////////

c4_StreamStrategy::c4_StreamStrategy(t4_i32 buflen_): _stream(0), _buffer
  (d4_new t4_byte[buflen_]), _buflen(buflen_), _position(0) {
  _mapStart = _buffer;
  _dataSize = buflen_;
}

c4_StreamStrategy::c4_StreamStrategy(c4_Stream *stream_): _stream(stream_),
  _buffer(0), _buflen(0), _position(0){}

c4_StreamStrategy::~c4_StreamStrategy() {
  _mapStart = 0;
  _dataSize = 0;

  if (_buffer != 0)
    delete [] _buffer;
}

bool c4_StreamStrategy::IsValid()const {
  return true;
}

int c4_StreamStrategy::DataRead(t4_i32 pos_, void *buffer_, int length_) {
  if (_buffer != 0) {
    d4_assert(pos_ <= _buflen);
    _position = pos_ + _baseOffset;

    if (length_ > _buflen - _position)
      length_ = _buflen - _position;
    if (length_ > 0)
      memcpy(buffer_, _buffer + _position, length_);
  } else {
    d4_assert(_position == pos_ + _baseOffset);
    length_ = _stream != 0 ? _stream->Read(buffer_, length_): 0;
  }

  _position += length_;
  return length_;
}

void c4_StreamStrategy::DataWrite(t4_i32 pos_, const void *buffer_, int length_)
  {
  if (_buffer != 0) {
    d4_assert(pos_ <= _buflen);
    _position = pos_ + _baseOffset;

    int n = length_;
    if (n > _buflen - _position)
      n = _buflen - _position;
    if (n > 0)
      memcpy(_buffer + _position, buffer_, n);
  } else {
    d4_assert(_position == pos_ + _baseOffset);
    if (_stream != 0 && !_stream->Write(buffer_, length_))
      ++_failure;
  }

  _position += length_;
}

t4_i32 c4_StreamStrategy::FileSize() {
  return _position;
}

/////////////////////////////////////////////////////////////////////////////
