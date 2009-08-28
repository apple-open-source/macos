// view.cpp --
// $Id: view.cpp 1230 2007-03-09 15:58:53Z jcw $
// This is part of Metakit, the homepage is http://www.equi4.com/metakit.html

/** @file
 * Implementation of main classes not involved in persistence
 */

#include "header.h"
#include "derived.h"
#include "custom.h"
#include "store.h"    // for RelocateRows
#include "field.h"    // for RelocateRows
#include "persist.h"
#include "remap.h"

#if !q4_INLINE
#include "mk4.inl"
#endif 

/////////////////////////////////////////////////////////////////////////////
// c4_ThreadLock

class c4_ThreadLock {
  public:
    c4_ThreadLock();
    ~c4_ThreadLock();

    class Hold {
      public:
        Hold();
        ~Hold();
    };
};

/////////////////////////////////////////////////////////////////////////////

#if q4_MULTI

#if q4_WIN32

/*
 *  On Win32, use a critical section to protect the global symbol table.
 *  Also uses special thread-safe calls to inc/dec all reference counts.
 *
 *  This implementation replaces the previous use of TLS, which cannot
 *  be used without special tricks in dynamically loaded DLL's, as is
 *  required for OCX/ActiveX use (which uses LoadLibrary).
 *
 *  Note: Could have used MFC's CCriticalSection and CSingleLock classes,
 *      but the code below is so trivial that it hardly matters.
 */

#if q4_MSVC && !q4_STRICT
#pragma warning(disable: 4201) // nonstandard extension used : ...
#endif 
#include <windows.h>

static CRITICAL_SECTION gCritSect;

c4_ThreadLock::c4_ThreadLock() {
  InitializeCriticalSection(&gCritSect);
}

c4_ThreadLock::~c4_ThreadLock() {
  DeleteCriticalSection(&gCritSect);
}

c4_ThreadLock::Hold::Hold() {
  EnterCriticalSection(&gCritSect);
}

c4_ThreadLock::Hold::~Hold() {
  LeaveCriticalSection(&gCritSect);
}

#else /* q4_WIN32 */

#include <pthread.h>

static pthread_mutex_t gMutex;

d4_inline c4_ThreadLock::c4_ThreadLock() {
  pthread_mutex_init(&gMutex, 0);
}

d4_inline c4_ThreadLock::~c4_ThreadLock() {
  pthread_mutex_destroy(&gMutex);
}

d4_inline c4_ThreadLock::Hold::Hold() {
  d4_dbgdef(int r = )pthread_mutex_lock(&gMutex);
  d4_assert(r == 0);
}

d4_inline c4_ThreadLock::Hold::~Hold() {
  d4_dbgdef(int r = )pthread_mutex_unlock(&gMutex);
  d4_assert(r == 0);
}

#endif /* q4_WIN32 */

#else /* q4_MULTI */

//  All other implementations revert to the simple "thread-less" case.

d4_inline c4_ThreadLock::c4_ThreadLock(){}

d4_inline c4_ThreadLock::~c4_ThreadLock(){}

d4_inline c4_ThreadLock::Hold::Hold(){}

d4_inline c4_ThreadLock::Hold::~Hold(){}

#endif 

/////////////////////////////////////////////////////////////////////////////

#if q4_LOGPROPMODS

static FILE *sPropModsFile = 0;
static int sPropModsProp =  - 1;

FILE *f4_LogPropMods(FILE *fp_, int propId_) {
  FILE *prevfp = sPropModsFile;
  sPropModsFile = fp_;
  sPropModsProp = propId_;
  return prevfp;
}

void f4_DoLogProp(const c4_Handler *hp_, int id_, const char *fmt_, int arg_) {
  if (sPropModsFile != 0 && (sPropModsProp < 0 || sPropModsProp == id_)) {
    fprintf(sPropModsFile, "handler 0x%x id %d: ", hp_, id_);
    fprintf(sPropModsFile, fmt_, arg_);
  }
}

#endif 

/////////////////////////////////////////////////////////////////////////////

/** @class c4_View
 *
 *  A collection of data rows.  This is the central public data structure of
 *  Metakit (often called "table", "array", or "relation" in other systems).
 *
 *  Views are smart pointers to the actual collections, setting a view to a new
 *  value does not alter the collection to which this view pointed previously.
 *
 *  The elements of views can be referred to by their 0-based index, which
 *  produces a row-reference of type c4_RowRef.  These row references can
 *  be copied, used to get or set properties, or dereferenced (in which case
 *  an object of class c4_Row is returned).  Taking the address of a row
 *  reference produces a c4_Cursor, which acts very much like a pointer.
 *
 *  The following code creates a view with 1 row and 2 properties:
 * @code
 *    c4_StringProp pName ("name");
 *    c4_IntProp pAge ("age");
 *
 *    c4_Row data;
 *    pName (data) = "John Williams";
 *    pAge (data) = 43;
 *
 *    c4_View myView;
 *    myView.Add(row);
 * @endcode
 */

/// Construct a view based on a sequence
c4_View::c4_View(c4_Sequence *seq_): _seq(seq_) {
  if (!_seq)
    _seq = d4_new c4_HandlerSeq(0);

  _IncSeqRef();
}

/// Construct a view based on a custom viewer
c4_View::c4_View(c4_CustomViewer *viewer_): _seq(0) {
  d4_assert(viewer_);

  _seq = d4_new c4_CustomSeq(viewer_);

  _IncSeqRef();
}

/// Construct a view based on an input stream
c4_View::c4_View(c4_Stream *stream_): _seq(c4_Persist::Load(stream_)) {
  if (_seq == 0)
    _seq = d4_new c4_HandlerSeq(0);

  _IncSeqRef();
}

/// Construct an empty view with one property
c4_View::c4_View(const c4_Property &prop_): _seq(d4_new c4_HandlerSeq(0)) {
  _IncSeqRef();

  _seq->PropIndex(prop_);
}

/// Copy constructor
c4_View::c4_View(const c4_View &view_): _seq(view_._seq) {
  _IncSeqRef();
}

/// Makes this view the same as another one.
c4_View &c4_View::operator = (const c4_View &view_) {
  if (_seq != view_._seq) {
    _DecSeqRef();
    _seq = view_._seq;
    _IncSeqRef();
  }
  return  *this;
}

/** Get a single data item in a generic way
 *
 * This can be used to access view data in a generalized way.
 * Useful for c4_CustomViewers which are based on other views.
 * @return true if the item is non-empty
 */
bool c4_View::GetItem(int row_, int col_, c4_Bytes &buf_)const {
  const c4_Property &prop = NthProperty(col_);
  return prop(GetAt(row_)).GetData(buf_);
}

/// Set a single data item in a generic way
void c4_View::SetItem(int row_, int col_, const c4_Bytes &buf_)const {
  const c4_Property &prop = NthProperty(col_);
  prop(GetAt(row_)).SetData(buf_);
}

/// Set an entry, growing the view if needed
void c4_View::SetAtGrow(int index_, const c4_RowRef &newElem_) {
  if (index_ >= GetSize())
    SetSize(index_ + 1);

  _seq->SetAt(index_, &newElem_);
}

/** Add a new entry, same as "SetAtGrow(GetSize(), ...)"
 * @return the index of the newly added row
 */
int c4_View::Add(const c4_RowRef &newElem_) {
  int i = GetSize();
  InsertAt(i, newElem_);
  return i;
}

/** Construct a new view with a copy of the data
 *
 * The copy is a deep copy, because subviews are always copied in full.
 */
c4_View c4_View::Duplicate()const {
  // insert all rows, sharing any subviews as needed
  c4_View result = Clone();
  result.InsertAt(0, _seq);
  return result;
}

/** Constructs a new view with the same structure but no data
 *
 * Structural information can only be maintain for the top level,
 * subviews will be included but without any properties themselves.
 */
c4_View c4_View::Clone()const {
  c4_View view;

  for (int i = 0; i < NumProperties(); ++i)
    view._seq->PropIndex(NthProperty(i));

  return view;
}

/** Adds a property column to a view if not already present
 * @return 0-based column position of the property
 */
int c4_View::AddProperty(const c4_Property &prop_) {
  return _seq->PropIndex(prop_);
}

/** Returns the N-th property (using zero-based indexing)
 * @return reference to the specified property
 */
const c4_Property &c4_View::NthProperty(int index_  
  ///< the zero-based property index
)const {
  return _seq->NthHandler(index_).Property();
}

/** Find the index of a property, given its name
 * @return 0-based column index
 * @retval -1 property not present in this view
 */
int c4_View::FindPropIndexByName(const char *name_  
  ///< property name (case insensitive)
)const {
  // use a slow linear scan to find the untyped property by name
  for (int i = 0; i < NumProperties(); ++i) {
    c4_String s = NthProperty(i).Name();
    if (s.CompareNoCase(name_) == 0)
      return i;
  }

  return  - 1;
}

/** Defines a column for a property.
 *
 * The following code defines an empty view with three properties:
 * @code
 *  c4_IntProp p1, p2, p3;
 *  c4_View myView = (p1, p2, p3);
 * @endcode
 * @return the new view object (without any data rows)
 * @sa c4_Property
 */
c4_View c4_View::operator, (const c4_Property &prop_)const {
  c4_View view = Clone();
  view.AddProperty(prop_);
  return view;
}

/// Insert copies of all rows of the specified view
void c4_View::InsertAt(int index_, const c4_View &view_) {
  int n = view_.GetSize();
  if (n > 0) {
    c4_Row empty;

    InsertAt(index_, empty, n);

    for (int i = 0; i < n; ++i)
      SetAt(index_ + i, view_[i]);
  }
}

bool c4_View::IsCompatibleWith(const c4_View &dest_)const {
  // can't determine table without handlers (and can't be a table)
  if (NumProperties() == 0 || dest_.NumProperties() == 0)
    return false;

  c4_Sequence *s1 = _seq;
  c4_Sequence *s2 = dest_._seq;
  c4_HandlerSeq *h1 = (c4_HandlerSeq*)s1->HandlerContext(0);
  c4_HandlerSeq *h2 = (c4_HandlerSeq*)s2->HandlerContext(0);

  // both must be real handler views, not derived ones
  if (h1 != s1 || h2 != s2)
    return false;

  // both must not contain any temporary handlers
  if (s1->NumHandlers() != h1->NumFields() || s2->NumHandlers() != h2
    ->NumFields())
    return false;

  // both must be in the same storage
  if (h1->Persist() == 0 || h1->Persist() != h2->Persist())
    return false;

  // both must have the same structure (is this expensive?)
  c4_String d1 = h1->Definition().Description(true);
  c4_String d2 = h2->Definition().Description(true);
  return d1 == d2; // ignores all names
}

/** Move attached rows to somewhere else in same storage
 *
 * There is a lot of trickery going on here.  The whole point of this
 * code is that moving rows between (compatible!) subviews should not
 * use copying when potentially large memo's and subviews are involved.
 * In that case, the best solution is really to move pointers, not data.
 */
void c4_View::RelocateRows(int from_, int count_, c4_View &dest_, int pos_) {
  if (count_ < 0)
    count_ = GetSize() - from_;
  if (pos_ < 0)
    pos_ = dest_.GetSize();

  d4_assert(0 <= from_ && from_ <= GetSize());
  d4_assert(0 <= count_ && from_ + count_ <= GetSize());
  d4_assert(0 <= pos_ && pos_ <= dest_.GetSize());

  if (count_ > 0) {
    // the destination must not be inside the source rows
    d4_assert(&dest_ != this || from_ > pos_ || pos_ >= from_ + count_);

    // this test is slow, so do it only as a debug check
    d4_assert(IsCompatibleWith(dest_));

    // make space, swap rows, drop originals
    c4_Row empty;
    dest_.InsertAt(pos_, empty, count_);

    // careful if insert moves origin
    if (&dest_ == this && pos_ <= from_)
      from_ += count_;

    for (int i = 0; i < count_; ++i)
      ((c4_HandlerSeq*)_seq)->ExchangeEntries(from_ + i, *(c4_HandlerSeq*)
        dest_._seq, pos_ + i);

    RemoveAt(from_, count_);
  }
}

/** Create view with all rows in natural (property-wise) order
 *
 * The result is virtual, it merely maintains a permutation to access the
 * underlying view.  This "derived" view uses change notification to track
 * changes to the underlying view, but unfortunately there are some major
 * limitations with this scheme - one of them being that deriving another
 * view from this sorted one will not properly track changes.
 */
c4_View c4_View::Sort()const {
  return f4_CreateSort(*_seq);
}

/** Create view sorted according to the specified properties
 *
 * The result is virtual, it merely maintains a permutation to access the
 * underlying view.  This "derived" view uses change notification to track
 * changes to the underlying view, but unfortunately there are some major
 * limitations with this scheme - one of them being that deriving another
 * view from this sorted one will not properly track changes.
 */
c4_View c4_View::SortOn(const c4_View &up_)const {
  c4_Sequence *seq = f4_CreateProject(*_seq,  *up_._seq, true);

  return f4_CreateSort(*seq);
}

/** Create sorted view, with some properties sorted in reverse
 *
 * The result is virtual, it merely maintains a permutation to access the
 * underlying view.  This "derived" view uses change notification to track
 * changes to the underlying view, but unfortunately there are some major
 * limitations with this scheme - one of them being that deriving another
 * view from this sorted one will not properly track changes.
 */
c4_View c4_View::SortOnReverse(const c4_View &up_,  
  ///< the view which defines the sort order
const c4_View &down_  ///< subset of up_, defines reverse order
)const {
  c4_Sequence *seq = f4_CreateProject(*_seq,  *up_._seq, true);

  return f4_CreateSort(*seq, down_._seq);
}

/** Create view with rows matching the specified value
 *
 * The result is virtual, it merely maintains a permutation to access the
 * underlying view.  This "derived" view uses change notification to track
 * changes to the underlying view, but this only works when based on views
 * which properly generate change notifications (.e. raw views, other
 * selections, and projections).
 */
c4_View c4_View::Select(const c4_RowRef &crit_)const {
  return f4_CreateFilter(*_seq, &crit_, &crit_);
}

/** Create view with row values within the specified range
 *
 * The result is virtual, it merely maintains a permutation to access the
 * underlying view.  This "derived" view uses change notification to track
 * changes to the underlying view, but this only works when based on views
 * which properly generate change notifications (.e. raw views, other
 * selections, and projections).
 */
c4_View c4_View::SelectRange(const c4_RowRef &low_,  
  ///< values of the lower bounds (inclusive)
const c4_RowRef &high_  ///< values of the upper bounds (inclusive)
)const {
  return f4_CreateFilter(*_seq, &low_, &high_);
}

/** Create view with the specified property arrangement
 *
 * The result is virtual, it merely maintains a permutation to access the
 * underlying view.  This "derived" view uses change notification to track
 * changes to the underlying view, but this only works when based on views
 * which properly generate change notifications (.e. raw views, selections,
 * and other projections).
 */
c4_View c4_View::Project(const c4_View &in_)const {
  return f4_CreateProject(*_seq,  *in_._seq, false);
}

/** Create derived view with some properties omitted
 *
 * The result is virtual, it merely maintains a permutation to access the
 * underlying view.  This "derived" view uses change notification to track
 * changes to the underlying view, but this only works when based on views
 * which properly generate change notifications (.e. raw views, selections,
 * and other projections).
 */
c4_View c4_View::ProjectWithout(const c4_View &out_)const {
  return f4_CreateProject(*_seq,  *_seq, false, out_._seq);
}

/** Create view which is a segment/slice (default is up to end)
 *
 * Returns a view which is a subset, either a contiguous range, or
 * a "slice" with element taken from every step_ entries.  If the
 * step is negative, the same entries are returned, but in reverse
 * order (start_ is still lower index, it'll then be returned last).
 *
 * This view operation is based on a custom viewer and is modifiable.
 */
c4_View c4_View::Slice(int first_, int limit_, int step_)const {
  return f4_CustSlice(*_seq, first_, limit_, step_);
}

/** Create view which is the cartesian product with given view
 *
 * The cartesian product is defined as every combination of rows
 * in both views.  The number of entries is the product of the
 * number of entries in the two views, properties which are present
 * in both views will use the values defined in this view.
 *
 * This view operation is based on a read-only custom viewer.
 */
c4_View c4_View::Product(const c4_View &view_)const {
  return f4_CustProduct(*_seq, view_);
}

/** Create view which remaps another given view
 *
 * Remapping constructs a view with the rows indicated by another
 * view.  The first property in the order_ view must be an int
 * property with index values referring to this one.  The size of
 * the resulting view is determined by the order_ view and can
 * differ, for example to act as a subset selection (if smaller).
 *
 * This view operation is based on a custom viewer and is modifiable.
 */
c4_View c4_View::RemapWith(const c4_View &view_)const {
  return f4_CustRemapWith(*_seq, view_);
}

/** Create view which pairs each row with corresponding row
 *
 * This is like a row-by-row concatenation.  Both views must have
 * the same number of rows, the result has all properties from
 * this view plus any other properties from the other view.
 *
 * This view operation is based on a custom viewer and is modifiable.
 */
c4_View c4_View::Pair(const c4_View &view_)const {
  return f4_CustPair(*_seq, view_);
}

/** Create view with rows from another view appended
 *
 * Constructs a view which has all rows of this view, and all rows
 * of the second view appended.  The structure of the second view
 * is assumed to be identical to this one.  This operation is a bit
 * similar to appending all rows from the second view, but it does
 * not actually store the result anywhere, it just looks like it.
 *
 * This view operation is based on a custom viewer and is modifiable.
 */
c4_View c4_View::Concat(const c4_View &view_)const {
  return f4_CustConcat(*_seq, view_);
}

/** Create view with one property renamed (must be of same type)
 *
 * This view operation is based on a custom viewer and is modifiable.
 */
c4_View c4_View::Rename(const c4_Property &old_, const c4_Property &new_)const {
  return f4_CustRename(*_seq, old_, new_);
}

/** Create view with a subview, grouped by the specified properties
 *
 * This operation is similar to the SQL 'GROUP BY', but it takes
 * advantage of the fact that Metakit supports nested views.  The
 * view returned from this member has one row per distinct group,
 * with an extra view property holding the remaining properties.
 * If there are N rows in the original view matching key X, then
 * the result is a row for key X, with a subview of N rows.  The
 * properties of the subview are all the properties not in the key.
 *
 * This view operation is based on a read-only custom viewer.
 */
c4_View c4_View::GroupBy(const c4_View &keys_,  
  ///< properties in this view determine grouping
const c4_ViewProp &result_  ///< name of new subview defined in result
)const {
  return f4_CustGroupBy(*_seq, keys_, result_);
}

/** Create view with count of duplicates, when grouped by key
 *
 * This is similar to c4_View::GroupBy, but it determines only the
 * number of rows in each group and does not create a nested view.
 *
 * This view operation is based on a read-only custom viewer.
 */
c4_View c4_View::Counts(const c4_View &keys_,  
  ///< properties in this view determine grouping
const c4_IntProp &result_  ///< new count property defined in result
)const {
  return f4_CustGroupBy(*_seq, keys_, result_); // third arg is c4_IntProp
}

/** Create view with all duplicate rows omitted
 *
 * This view operation is based on a read-only custom viewer.
 */
c4_View c4_View::Unique()const {
  c4_IntProp count("#N#");
  return Counts(Clone(), count).ProjectWithout(count);
}

/** Create view which is the set union (assumes no duplicate rows)
 *
 * Calculates the set union.  This will only work if both input
 * views are sets, i.e. they have no duplicate rows in them.
 *
 * This view operation is based on a read-only custom viewer.
 */
c4_View c4_View::Union(const c4_View &view_)const {
  return Concat(view_).Unique();
}

/** Create view with all rows also in the given view (no dups)
 *
 * Calculates the set intersection.  This will only work if both
 * input views are sets, i.e. they have no duplicate rows in them.
 *
 * This view operation is based on a read-only custom viewer.
 */
c4_View c4_View::Intersect(const c4_View &view_)const {
  c4_View v = Concat(view_);

  // assume neither view has any duplicates
  c4_IntProp count("#N#");
  return v.Counts(Clone(), count).Select(count[2]).ProjectWithout(count);
}

/** Create view with all rows not in both views (no dups)
 *
 * Calculates the "XOR" of two sets.  This will only work if both
 * input views are sets, i.e. they have no duplicate rows in them.
 *
 * This view operation is based on a read-only custom viewer.
 */
c4_View c4_View::Different(const c4_View &view_)const {
  c4_View v = Concat(view_);

  // assume neither view has any duplicates
  c4_IntProp count("#N#");
  return v.Counts(Clone(), count).Select(count[1]).ProjectWithout(count);
}

/** Create view with all rows not in the given view (no dups)
 *
 * Calculates set-difference of this view minus arg view.  Result
 * is a subset, unlike c4_View::Different. Will only work if both
 * input views are sets, i.e. they have no duplicate rows in them.
 *
 * This view operation is based on a read-only custom viewer.
 */
c4_View c4_View::Minus(const c4_View &view_  ///< the second view
)const {
  // inefficient: calculate difference, then keep only those in self
  return Intersect(Different(view_));
}

/** Create view with a specific subview expanded, like a join
 *
 * This operation is the inverse of c4_View::GroupBy, expanding
 * all rows in specified subview and returning a view which looks
 * as if the rows in each subview were "expanded in place".
 *
 * This view operation is based on a read-only custom viewer.
 */
c4_View c4_View::JoinProp(const c4_ViewProp &sub_,  
  ///< name of the subview to expand
bool outer_  ///< true: keep rows with empty subviews
)const {
  return f4_CustJoinProp(*_seq, sub_, outer_);
}

/** Create view which is the relational join on the given keys
 *
 * This view operation is based on a read-only custom viewer.
 */
c4_View c4_View::Join(const c4_View &keys_,  
  ///< properties in this view determine the join
const c4_View &view_,  ///< second view participating in the join
bool outer_  ///< true: keep rows with no match in second view
)const {
  // inefficient: calculate difference, then keep only those in self
  return f4_CustJoin(*_seq, keys_, view_, outer_);
}

/** Create an identity view which only allows reading
 *
 * This view operation is based on a custom viewer.
 */
c4_View c4_View::ReadOnly()const {
  return f4_CreateReadOnly(*_seq);
}

/** Create mapped view which adds a hash lookup layer
 *
 * This view creates and manages a special hash map view, to implement a
 * fast find on the key.  The key is defined to consist of the first
 * numKeys_ properties of the underlying view.
 *
 * The map_ view must be empty the first time this hash view is used, so
 * that Metakit can fill it based on whatever rows are already present in
 * the underlying view.  After that, neither the underlying view nor the
 * map view may be modified other than through this hash mapping layer.
 * The defined structure of the map view must be "_H:I,_R:I".
 *
 * This view is modifiable.  Insertions and changes to key field properties
 * can cause rows to be repositioned to maintain hash uniqueness.  Careful:
 * when a row is changed in such a way that its key is the same as in another
 * row, that other row will be deleted from the view.
 *
 * Example of use:
 * @code
 *  c4_View data = storage.GetAs("people[name:S,age:I]");
 *  c4_View datah = storage.GetAs("people_H[_H:I,_R:I]");
 *  c4_View hash = raw.Hash(datah, 1);
 *  ... hash.GetSize() ...
 *  hash.Add(...)
 * @endcode
 */
c4_View c4_View::Hash(const c4_View &map_, int numKeys_)const {
  return f4_CreateHash(*_seq, numKeys_, map_._seq);
}

/** Create mapped view which blocks its rows in two levels
 *
 * This view acts like a large flat view, even though the actual rows are
 * stored in blocks, which are rebalanced automatically to maintain a good 
 * trade-off between block size and number of blocks.
 *
 * The underlying view must be defined with a single view property, with
 * the structure of the subview being as needed.  An example of a blocked
 * view definition which will act like a single one containing 2 properties:
 * @code
 *  c4_View raw = storage.GetAs("people[_B[name:S,age:I]]");
 *  c4_View flat = raw.Blocked();
 *  ... flat.GetSize() ...
 *  flat.InsertAt(...)
 * @endcode
 * 
 * This view operation is based on a custom viewer and is modifiable.
 */
c4_View c4_View::Blocked()const {
  return f4_CreateBlocked(*_seq);
}

/** Create mapped view which keeps its rows ordered
 *
 * This is an identity view, which has as only use to inform Metakit that
 * the underlying view can be considered to be sorted on its first numKeys_
 * properties.  The effect is that c4_View::Find will try to use binary
 * search when the search includes key properties (results will be identical
 * to unordered views, the find will just be more efficient).
 *
 * This view is modifiable.  Insertions and changes to key field properties
 * can cause rows to be repositioned to maintain the sort order.  Careful:
 * when a row is changed in such a way that its key is the same as in another
 * row, that other row will be deleted from the view.
 *
 * This view can be combined with c4_View::Blocked, to create a 2-level
 * btree structure.
 */
c4_View c4_View::Ordered(int numKeys_)const {
  return f4_CreateOrdered(*_seq, numKeys_);
}

/** Create mapped view which maintains an index permutation
 *
 * This is an identity view which somewhat resembles the ordered view, it
 * maintains a secondary "map" view to contain the permutation to act as
 * an index.  The indexed view presents the same order of rows as the
 * underlying view, but the index map is set up in such a way that binary
 * search is possible on the keys specified.  When the "unique" parameter
 * is true, insertions which would create a duplicate key are ignored.
 *
 * This view is modifiable.  Careful: when a row is changed in such a way
 * that its key is the same as in another row, that other row will be
 * deleted from the view.
 */
c4_View c4_View::Indexed(const c4_View &map_, const c4_View &props_, bool
  unique_)const {
  return f4_CreateIndexed(*_seq,  *map_._seq, props_, unique_);
}

/** Return the index of the specified row in this view (or -1)
 *
 * This function can be used to "unmap" an index of a derived view back
 * to the original underlying view.
 */
int c4_View::GetIndexOf(const c4_RowRef &row_)const {
  c4_Cursor cursor = &row_;

  return cursor._seq->RemapIndex(cursor._index, _seq);
}

/// Restrict the search range for rows
int c4_View::RestrictSearch(const c4_RowRef &c_, int &pos_, int &count_) {
  return _seq->RestrictSearch(&c_, pos_, count_) ? 0 : ~0;
}

/** Find index of the the next entry matching the specified key.
 *
 * Defaults to linear search, but hash- and ordered-views will use a better
 * algorithm if possible.  Only the properties present in the search key
 * are used to determine whether a row matches the key.
 * @return position where match occurred
 * @retval -1 if not found
 */
int c4_View::Find(const c4_RowRef &crit_,  ///< the value to look for
int start_  ///< the index to start with
)const {
  d4_assert(start_ >= 0);

  c4_Row copy = crit_; // the lazy (and slow) solution: make a copy

  int count = GetSize() - start_;
  if (_seq->RestrictSearch(&copy, start_, count)) {
    c4_View refView = copy.Container();
    c4_Sequence *refSeq = refView._seq;
    d4_assert(refSeq != 0);

    c4_Bytes data;

    for (int j = 0; j < count; ++j) {
      int i;

      for (i = 0; i < refSeq->NumHandlers(); ++i) {
        c4_Handler &h = refSeq->NthHandler(i); // no context issues

        if (!_seq->Get(start_ + j, h.PropId(), data))
          h.ClearBytes(data);

        if (h.Compare(0, data) != 0)
        // always row 0
          break;
      }

      if (i == refSeq->NumHandlers())
        return start_ + j;
    }
  }

  return  - 1;
}

/** Search for a key, using the native sort order of the view
 * @return position where found, or where it may be inserted,
 *  this position can also be just past the last row
 */
int c4_View::Search(const c4_RowRef &crit_)const {
  int l =  - 1, u = GetSize();
  while (l + 1 != u) {
    const int m = (l + u) >> 1;
    if (_seq->Compare(m, &crit_) < 0)
    //if (crit_ > (*this)[m]) // Dec 2001: see comments below
      l = m;
    else
      u = m;
  }

  return u;
}

/// Return number of matching keys, and pos of first one as arg
int c4_View::Locate(const c4_RowRef &crit_, int *pos_)const {
  // Dec 2001: fixed a problem with searching of partial rows.
  //
  // There is an *extremely* tricky issue in here, in that the
  // comparison operator for rows is not symmetric.  So in the
  // general case, "a == b" is not euivalent to "b == a".  This
  // is without doubt a design mistake (and should have at least
  // been named differently).
  //
  // The reason is that the number of properties in both rowrefs
  // need not be the same.  Only the properties of the leftmost
  // rowref are compared against the other one.  This also applies
  // to the other comparisons, i.e. !=, <, >, <=, and >=.
  //
  // All Compare calls below have been changed to use comparisons
  // in the proper order and now use "rowref <op> rowref" syntax.

  c4_Cursor curr(*(c4_Sequence*)_seq, 0); // loses const

  int l =  - 1, u = GetSize();
  while (l + 1 != u) {
    curr._index = (l + u) >> 1;
    if (crit_ >  *curr)
      l = curr._index;
    else
      u = curr._index;
  }

  if (pos_ != 0)
    *pos_ = u;

  // only look for more if the search hit an exact match
  curr._index = u;
  if (u == GetSize() || crit_ !=  *curr)
    return 0;

  // as Jon Bentley wrote in DDJ Apr 2000, setting l2 to -1 is better than u
  int l2 =  - 1, u2 = GetSize();
  while (l2 + 1 != u2) {
    curr._index = (l2 + u2) >> 1;
    if (crit_ >=  *curr)
      l2 = curr._index;
    else
      u2 = curr._index;
  }

  return u2 - u;
}

/// Compare two views lexicographically (rows 0..N-1).
int c4_View::Compare(const c4_View &view_)const {
  if (_seq == view_._seq)
    return 0;

  int na = GetSize();
  int nb = view_.GetSize();
  int i;

  for (i = 0; i < na && i < nb; ++i)
    if (GetAt(i) != view_.GetAt(i))
      return GetAt(i) < view_.GetAt(i) ?  - 1:  + 1;

  return na == nb ? 0 : i < na ?  + 1:  - 1;
}

/////////////////////////////////////////////////////////////////////////////

/** @class c4_Cursor
 *
 *  An iterator for collections of rows (views).
 *
 *  Cursor objects can be used to point to specific entries in a view.
 *  A cursor acts very much like a pointer to a row in a view, and is 
 *  returned when taking the address of a c4_RowRef.  Dereferencing
 *  a cursor leads to the original row reference again.  You can construct a
 *  cursor for a c4_Row, but since such rows are not part of a collection,
 *  incrementing or decrementing these cursors is meaningless (and wrong). 
 *
 *  The usual range of pointer operations can be applied to these objects:
 *  pre/post-increment and decrement, adding or subtracting integer offsets,
 *  as well as the full range of comparison operators.  If two cursors
 *  point to entries in the same view, their difference can be calculated.
 *
 *  As with regular pointers, care must be taken to avoid running off of
 *  either end of a view (the debug build includes assertions to check this).
 */

/** @class c4_RowRef
 *
 *  Reference to a data row, can be used on either side of an assignment.
 *
 *  Row references are created when dereferencing a c4_Cursor or when
 *  indexing an element of a c4_View.  Assignment will change the
 *  corresponding item.  Rows (objects of type c4_Row) are a special
 *  case of row references, consisting of a view with exactly one item.
 *
 *  Internally, row references are very similar to cursors, in fact they are
 *  little more than a wrapper around them.  The essential difference is one
 *  of semantics: comparing row references compares contents, copying row
 *  references copies the contents, whereas cursor comparison and copying
 *  deals with the pointer to the row, not its contents.
 */

/////////////////////////////////////////////////////////////////////////////
// c4_Row

c4_Row::c4_Row(): c4_RowRef(*Allocate()){}

c4_Row::c4_Row(const c4_Row &row_): c4_RowRef(*Allocate()) {
  operator = (row_);
}

c4_Row::c4_Row(const c4_RowRef &rowRef_): c4_RowRef(*Allocate()) {
  operator = (rowRef_);
}

c4_Row::~c4_Row() {
  Release(_cursor);
}

c4_Row &c4_Row::operator = (const c4_Row &row_) {
  return operator = ((const c4_RowRef &)row_);
}

/// Assignment from a reference to a row.
c4_Row &c4_Row::operator = (const c4_RowRef &rowRef_) {
  d4_assert(_cursor._seq != 0);

  if (_cursor !=  &rowRef_) {
    d4_assert(_cursor._index == 0);
    _cursor._seq->SetAt(0, &rowRef_);
  }

  return  *this;
}

/// Adds all properties and values into this row.
void c4_Row::ConcatRow(const c4_RowRef &rowRef_) {
  d4_assert(_cursor._seq != 0);

  c4_Cursor cursor = &rowRef_; // trick to access private rowRef_._cursor
  d4_assert(cursor._seq != 0);

  c4_Sequence &rhSeq =  *cursor._seq;

  c4_Bytes data;

  for (int i = 0; i < rhSeq.NumHandlers(); ++i) {
    c4_Handler &h = rhSeq.NthHandler(i);

    h.GetBytes(cursor._index, data);
    _cursor._seq->Set(_cursor._index, h.Property(), data);
  }
}

c4_Row operator + (const c4_RowRef &a_, const c4_RowRef &b_) {
  c4_Row row = a_;
  row.ConcatRow(b_);
  return row;
}

c4_Cursor c4_Row::Allocate() {
  c4_Sequence *seq = d4_new c4_HandlerSeq(0);
  seq->IncRef();

  seq->Resize(1);

  return c4_Cursor(*seq, 0);
}

void c4_Row::Release(c4_Cursor row_) {
  d4_assert(row_._seq != 0);
  d4_assert(row_._index == 0);

  row_._seq->DecRef();
}

/////////////////////////////////////////////////////////////////////////////

/** @class c4_Property
 *
 *  Base class for the basic data types.
 *
 *  Property objects exist independently of view, row, and storage objects.
 *  They have a name and type, and can appear in any number of views.
 *  You will normally only use derived classes, to maintain strong typing.
 */

// This is a workaround for the fact that the initialization order of
// static objects is not always adequate (implementation dependent).
// Extremely messy solution, to allow statically declared properties.
//
// These are the only static variables in the entire Metakit core lib.

static c4_ThreadLock *sThreadLock = 0;
static c4_StringArray *sPropNames = 0;
static c4_DWordArray *sPropCounts = 0;

/// Call this to get rid of some internal datastructues (on exit)
void c4_Property::CleanupInternalData() {
  delete sPropNames;
  sPropNames = 0; // race

  delete sPropCounts;
  sPropCounts = 0; // race

  delete sThreadLock;
  sThreadLock = 0; // race
}

c4_Property::c4_Property(char type_, const char *name_): _type(type_) {
  if (sThreadLock == 0)
    sThreadLock = d4_new c4_ThreadLock;

  c4_ThreadLock::Hold lock; // grabs the lock until end of scope

  if (sPropNames == 0)
    sPropNames = d4_new c4_StringArray;

  if (sPropCounts == 0)
    sPropCounts = d4_new c4_DWordArray;

  c4_String temp = name_;

  _id = sPropNames->GetSize();
  while (--_id >= 0) {
    const char *p = sPropNames->GetAt(_id);
    // optimize for first char case-insensitive match
    if (((*p ^  *name_) &~0x20) == 0 && temp.CompareNoCase(p) == 0)
      break;
  }

  if (_id < 0) {
    int size = sPropCounts->GetSize();

    for (_id = 0; _id < size; ++_id)
      if (sPropCounts->GetAt(_id) == 0)
        break;

    if (_id >= size) {
      sPropCounts->SetSize(_id + 1);
      sPropNames->SetSize(_id + 1);
    }

    sPropCounts->SetAt(_id, 0);
    sPropNames->SetAt(_id, name_);
  }

  Refs( + 1);
}

c4_Property::c4_Property(const c4_Property &prop_): _id(prop_.GetId()), _type
  (prop_.Type()) {
  c4_ThreadLock::Hold lock;

  d4_assert(sPropCounts != 0);
  d4_assert(sPropCounts->GetAt(_id) > 0);

  Refs( + 1);
}

c4_Property::~c4_Property() {
  c4_ThreadLock::Hold lock;

  Refs( - 1);
}

void c4_Property::operator = (const c4_Property &prop_) {
  c4_ThreadLock::Hold lock;

  prop_.Refs( + 1);
  Refs( - 1);

  _id = prop_.GetId();
  _type = prop_.Type();
}

/// Return the name of this property
const char *c4_Property::Name()const {
  c4_ThreadLock::Hold lock;

  d4_assert(sPropNames != 0);
  return sPropNames->GetAt(_id);
}

/** Adjust the reference count
 *
 *  This is part of the implementation and shouldn't normally be called.
 *  This code is only called with the lock held, and always thread-safe.
 */
void c4_Property::Refs(int diff_)const {
  d4_assert(diff_ ==  - 1 || diff_ ==  + 1);

  d4_assert(sPropCounts != 0);
  sPropCounts->ElementAt(_id) += diff_;

#if q4_CHECK
  // get rid of the cache when the last property goes away
  static t4_i32 sPropTotals;

  sPropTotals += diff_;
  if (sPropTotals == 0)
    CleanupInternalData();
#endif 
}

/////////////////////////////////////////////////////////////////////////////
