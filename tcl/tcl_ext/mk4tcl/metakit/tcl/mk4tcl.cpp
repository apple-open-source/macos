// mk4tcl.cpp --
// $Id: mk4tcl.cpp 4435 2008-08-01 19:58:42Z patthoyts $
// This is part of Metakit, see http://www.equi4.com/metakit.html

#include "mk4tcl.h"
#include "mk4io.h"

#ifndef _WIN32_WCE
#include <errno.h>
#endif 

#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#ifndef EINVAL
#define EINVAL 9
#endif 

// stub interface code, removes the need to link with libtclstub*.a
//#ifdef USE_TCL_STUBS
//#include "stubtcl.h"
//#else 
//#define MyInitStubs(x) 1
//#endif 

// definition of valid property name - alpha numerics, underscore, percent,
// or any extended utf-8 character
#define ISNAME(c)       (isalnum((c)) || (c) == '_' || (c) == '%' || (c) & 0x80)
///////////////////////////////////////////////////////////////////////////////
// Defined in this file:

class MkPath;
class MkWorkspace;
class Tcl;
class MkTcl;

///////////////////////////////////////////////////////////////////////////////

// inc'ed whenever a datafile is closed, forces relookup of all paths
static int generation;

#ifdef TCL_THREADS

// There is a single monolithic mutex for protecting all of Mk4tcl, but it has
// to be a bit more advanced than Tcl's one since it has to support recursion,
// i.e. re-entering this code from the *same* thread needs to be allowed.  The
// recursion can happen in Tcl's type callbacks, see "Tcl_ObjType mkCursorType".
//
// We know the current interpreter in all cases, it can be used as mutex owner.
// So we can be multiple times inside the mutex, but ONLY from a simgle interp.
// No deadlock is possible, locking is always in the order mkMutex -> infoMutex.

TCL_DECLARE_MUTEX(mkMutex)    // use a single monolithic mutex for now
TCL_DECLARE_MUTEX(infoMutex)  // use a second mutex to manage the info below

// set to the interp holding the mutex, or to zero when not locked
static Tcl_Interp *mutex_owner;

// set to the reursion level, > 1 means we've re-entered from same interp
static int mutex_level;
  
static void EnterMutex(Tcl_Interp *ip_) {
    d4_assert(ip_ != 0);
    Tcl_MutexLock(&infoMutex);
    if (ip_ != mutex_owner) {
        Tcl_MutexUnlock(&infoMutex);
        Tcl_MutexLock(&mkMutex);
        Tcl_MutexLock(&infoMutex);
        d4_assert(mutex_owner == 0);
        mutex_owner = ip_;
    }
    ++mutex_level;
    Tcl_MutexUnlock(&infoMutex);
}

static void LeaveMutex() {
    Tcl_MutexLock(&infoMutex);
    d4_assert(mutex_owner != 0 && mutex_level > 0);
    if (--mutex_level == 0) {
      mutex_owner = 0;
      Tcl_MutexUnlock(&mkMutex);
    }
    Tcl_MutexUnlock(&infoMutex);
}

#else

#define EnterMutex(x)
#define LeaveMutex()

#endif
 
// put code in this file as a mutex is static in Windows
int Mk_EvalObj(Tcl_Interp *ip_, Tcl_Obj *cmd_) {
    LeaveMutex();
    int e = Tcl_EvalObj(ip_, cmd_);
    EnterMutex(ip_);
    return e;
}

// moved out of member func scope to please HP-UX's aCC:

static const char *getCmds[] =  {
  "-size", 0
};

static const char *viewCmds[] =  {
  "layout", "delete", "size", "properties", "locate", "restrict", "open", "new",
    "info", 0
};

static const char *cursorCmds[] =  {
  "create", "position", "incr", 0
};

static const char *channelCmds[] =  {
  "read", "write", "append", 0
};

/////////////////////////////////////////////////////////////////////////////
// Utility code: return next token up to char < '0', and
// advance the string pointer past following character.

c4_String f4_GetToken(const char * &str_) {
  d4_assert(str_);

  const char *p = str_;
  while (ISNAME(*p) ||  *p == ':')
    ++p;

  c4_String result(str_, p - str_);

  if (*p)
    ++p;
  // advance over seperator - but no check!
  str_ = p;

  return result;
}

///////////////////////////////////////////////////////////////////////////////
// Utility code: true if value contains a word starting with the given prefix

bool MatchOneKeyword(const char *value_, const c4_String &crit_) {
  int n = crit_.GetLength();
  if (n == 0)
    return true;

  char cu = (char)toupper(crit_[0]);
  char cl = (char)tolower(crit_[0]);

  const char *limit = value_ + strlen(value_) - n;
  while (value_ <= limit) {
    c4_String s(value_, n);
    if (s.CompareNoCase(crit_) == 0)
      return true;

    while (*++value_)
      if ((*value_ == cu ||  *value_ == cl) && !isalnum(value_[ - 1]))
        break;
  }

  return false;
}

///////////////////////////////////////////////////////////////////////////////
// A "storage in a storage" strategy class for Metakit
// Adapted from MkWrap, the Python interface

class SiasStrategy: public c4_Strategy {
  public:
    c4_Storage _storage;
    c4_View _view;
    c4_BytesProp _memo;
    int _row;
    t4_i32 _position;
    Tcl_Channel _chan;
    int _validMask;
    int _watchMask;
    Tcl_Interp *_interp;

    SiasStrategy(c4_Storage &storage_, const c4_View &view_, const c4_BytesProp
      &memo_, int row_): _storage(storage_), _view(view_), _memo(memo_), _row
      (row_), _position(0), _interp(0) {
        // set up mapping if the memo itself is mapped in its entirety
        c4_Strategy &strat = storage_.Strategy();
        if (strat._mapStart != 0) {
            c4_RowRef r = _view[_row];
            c4_Bytes data = _memo(r).Access(0);
            const t4_byte *ptr = data.Contents();
            if (data.Size() == _memo(r).GetSize() && strat._mapStart != 0 &&
              ptr >= strat._mapStart && ptr - strat._mapStart < strat._dataSize)
              {
                _mapStart = ptr;
                _dataSize = data.Size();
            }
        }
    }

    virtual ~SiasStrategy() {
        _view = c4_View();
        _mapStart = 0;
        _dataSize = 0;

        if (_chan != 0)
          Tcl_UnregisterChannel(_interp, _chan);
    }

    virtual void DataSeek(t4_i32 position_) {
        _position = position_;
    }

    virtual int DataRead(t4_i32 pos_, void *buffer_, int length_) {
        if (pos_ != ~0)
          _position = pos_;

        int i = 0;

        while (i < length_) {
            c4_Bytes data = _memo(_view[_row]).Access(_position + i, length_ -
              i);
            int n = data.Size();
            if (n <= 0)
              break;
            memcpy((char*)buffer_ + i, data.Contents(), n);
            i += n;
        }

        _position += i;
        return i;
    }

    virtual void DataWrite(t4_i32 pos_, const void *buffer_, int length_) {
        if (pos_ != ~0)
          _position = pos_;

        c4_Bytes data(buffer_, length_);
        if (_memo(_view[_row]).Modify(data, _position))
          _position += length_;
        else
          ++_failure;
    }

    virtual void DataCommit(t4_i32 newSize_) {
        if (newSize_ > 0)
          _memo(_view[_row]).Modify(c4_Bytes(), newSize_);
    }

    virtual void ResetFileMapping() {
        _mapStart = 0; // never called, but just in case
    }
};

///////////////////////////////////////////////////////////////////////////////
// New in 1.2: channel interface to memo fields

typedef SiasStrategy MkChannel;

typedef struct {
  Tcl_Event header;
  MkChannel *chan;
} MkEvent;

static int mkEventProc(Tcl_Event *evPtr, int flags) {
  MkEvent *me = (MkEvent*)evPtr;

  if (!(flags &TCL_FILE_EVENTS))
    return 0;

  Tcl_NotifyChannel(me->chan->_chan, me->chan->_watchMask);
  return 1;
}

static int mkEventFilter(Tcl_Event *evPtr, ClientData instanceData) {
  MkEvent *me = (MkEvent*)evPtr;
  MkChannel *chan = (MkChannel*)instanceData;
  return evPtr->proc == mkEventProc && me->chan == chan;
}

static int mkClose(ClientData instanceData, Tcl_Interp *interp) {
  MkChannel *chan = (MkChannel*)instanceData;

  Tcl_DeleteEvents(mkEventFilter, (ClientData)chan);
  chan->_chan = 0;
  delete chan;

  return TCL_OK;
}

static int mkInput(ClientData instanceData, char *buf, int toRead, int
  *errorCodePtr) {
  MkChannel *chan = (MkChannel*)instanceData;
  return chan->DataRead(~0, buf, toRead);
}

static int mkOutput(ClientData instanceData, const char *buf, int toWrite, int
  *errorCodePtr) {
  MkChannel *chan = (MkChannel*)instanceData;
  chan->DataWrite(~0, buf, toWrite);
  if (chan->_failure == 0)
    return toWrite;

  *errorCodePtr = EINVAL; // hm, bad choice of error code
  return  - 1;
}

static int mkSeek(ClientData instanceData, long offset, int seekMode, int
  *errorCodePtr) {
  MkChannel *chan = (MkChannel*)instanceData;

  switch (seekMode) {
    default:
       *errorCodePtr = EINVAL; // hm, bad choice of error code
      return  - 1;
    case 0:
      break;
    case 1:
      offset += chan->_position;
      break;
    case 2:
      offset += chan->_memo(chan->_view[chan->_row]).GetSize();
      break;
  }

  chan->DataSeek(offset);
  return offset;
}

static void mkWatchChannel(ClientData instanceData, int mask) {
  MkChannel *chan = (MkChannel*)instanceData;
  Tcl_Time blockTime =  {
    0, 0
  };

  /*
   * Since the file is always ready for events, we set the block time
   * to zero so we will poll.
   */

  chan->_watchMask = mask &chan->_validMask;
  if (chan->_watchMask) {
    Tcl_SetMaxBlockTime(&blockTime);
  }
}

static int mkGetFile(ClientData instanceData, int direction, ClientData
  *handlePtr) {
  return TCL_ERROR;
}

static Tcl_ChannelType mkChannelType =  {
  "mk",  /* Type name.                  */
  0,  /* Set blocking/nonblocking behaviour. NULL'able */
  mkClose,  /* Close channel, clean instance data      */
  mkInput,  /* Handle read request               */
  (Tcl_DriverOutputProc*)mkOutput,  /* Handle write request              */
  (Tcl_DriverSeekProc*)mkSeek,  /* Move location of access point.    NULL'able
    */
  0,  /* Set options.              NULL'able */
  0,  /* Get options.              NULL'able */
  (Tcl_DriverWatchProc*)mkWatchChannel,  /* Initialize notifier               */
  mkGetFile /* Get OS handle from the channel.         */
};

///////////////////////////////////////////////////////////////////////////////
// Utility code: get a Metakit item and convert it to a Tcl object

Tcl_Obj *GetAsObj(const c4_RowRef &row_, const c4_Property &prop_, Tcl_Obj
  *obj_) {
  if (obj_ == 0)
    obj_ = Tcl_NewObj();

  switch (prop_.Type()) {
    case 'S':
       {
        const char *p = ((c4_StringProp &)prop_)(row_);
        Tcl_SetStringObj(obj_, (char*)p,  - 1);
      }
      break;

    case 'B':
       {
        c4_Bytes temp;
        prop_(row_).GetData(temp);
        Tcl_SetByteArrayObj(obj_, (t4_byte*)temp.Contents(), temp.Size());
      }
      break;

    case 'F':
      Tcl_SetDoubleObj(obj_, ((c4_FloatProp &)prop_)(row_));
      break;

    case 'D':
      Tcl_SetDoubleObj(obj_, ((c4_DoubleProp &)prop_)(row_));
      break;

#ifdef TCL_WIDE_INT_TYPE
    case 'L':
      Tcl_SetWideIntObj(obj_, ((c4_LongProp &)prop_)(row_));
      break;
#endif 

    case 'I':
      Tcl_SetLongObj(obj_, ((c4_IntProp &)prop_)(row_));
      break;

    case 'V':
       {
        c4_View view = ((c4_ViewProp &)prop_)(row_);
        Tcl_SetIntObj(obj_, view.GetSize());
      }
      break;

    default:
       {
        KeepRef keeper(obj_); // a funny way to release the value
      }
      return 0;
  }

  return obj_;
}

///////////////////////////////////////////////////////////////////////////////
// Utility code: set a Metakit item and convert it from a Tcl object

int SetAsObj(Tcl_Interp *interp, const c4_RowRef &row_, const c4_Property
  &prop_, Tcl_Obj *obj_) {
  int e = TCL_OK;

  switch (prop_.Type()) {
    case 'S':
       {
        int len;
        const char *ptr = Tcl_GetStringFromObj(obj_, &len);
        prop_(row_).SetData(c4_Bytes(ptr, len + 1));
      }
      break;

    case 'B':
       {
        int len;
        const t4_byte *ptr = Tcl_GetByteArrayFromObj(obj_, &len);
        prop_(row_).SetData(c4_Bytes(ptr, len));
      }
      break;

    case 'F':
       {
        double value = 0;
        e = Tcl_GetDoubleFromObj(interp, obj_, &value);
        if (e == TCL_OK)
          ((c4_FloatProp &)prop_)(row_) = (float)value;
      }
      break;

    case 'D':
       {
        double value = 0;
        e = Tcl_GetDoubleFromObj(interp, obj_, &value);
        if (e == TCL_OK)
          ((c4_DoubleProp &)prop_)(row_) = value;
      }
      break;

#ifdef TCL_WIDE_INT_TYPE
    case 'L':
       {
        Tcl_WideInt value = 0;
        e = Tcl_GetWideIntFromObj(interp, obj_, &value);
        if (e == TCL_OK)
          ((c4_LongProp &)prop_)(row_) = value;
      }
      break;
#endif 

    case 'I':
       {
        long value = 0;
        e = Tcl_GetLongFromObj(interp, obj_, &value);
        if (e == TCL_OK)
          ((c4_IntProp &)prop_)(row_) = value;
      }
      break;

    default:
      Tcl_SetResult(interp, (char*)"unsupported property type", TCL_STATIC);
      e = TCL_ERROR;
  }

  return e;
}

///////////////////////////////////////////////////////////////////////////////
// In Tcl, streaming I/O uses the Tcl channel interface for loading/saving.

class c4_TclStream: public c4_Stream {
    Tcl_Channel _stream;

  public:
    c4_TclStream(Tcl_Channel stream_);
    virtual ~c4_TclStream();

    virtual int Read(void *buffer_, int length_);
    virtual bool Write(const void *buffer_, int length_);
};

c4_TclStream::c4_TclStream(Tcl_Channel stream_): _stream(stream_){}

c4_TclStream::~c4_TclStream(){}

int c4_TclStream::Read(void *buffer_, int length_) {
  return Tcl_Read(_stream, (char*)buffer_, length_);
}

bool c4_TclStream::Write(const void *buffer_, int length_) {
  return Tcl_Write(_stream, (char*)buffer_, length_) >= 0;
}

///////////////////////////////////////////////////////////////////////////////

MkPath::MkPath(MkWorkspace &ws_, const char * &path_, Tcl_Interp *interp):
  _refs(1), _ws(&ws_), _path(path_), _currGen(generation) {
  // if this view is not part of any storage, make a new temporary row
  if (_path.IsEmpty()) {
    ws_.AllocTempRow(_path);
    AttachView(interp);
  } else {
    int n = AttachView(interp);
    path_ += n; // move past all processed characters

    // but trim white space and unprocessed tail from stored path
    while (n > 0 && _path[n - 1] < '0')
      --n;
    if (n < _path.GetLength())
      _path = _path.Left(n);
  }
}

MkPath::~MkPath() {
  // 24-01-2003: paths should not clean up workspaces once exiting
  if (_currGen != -1)
    _ws->ForgetPath(this);
}

#if 0
static c4_View OpenMapped(c4_View v_, int col_, int row_) {
  if (col_ < 0)
    return c4_View();

  const c4_Property &prop = v_.NthProperty(col_);
  d4_assert(prop.Type() == 'V');
  if (prop.Type() != 'V')
    return c4_View();

  c4_View vw = ((c4_ViewProp &)prop)(v_[row_]);

  c4_String name = prop.Name();
  int h = v_.FindPropIndexByName(name + "_H1");
  if (h >= 0) {
    const c4_Property &proph = v_.NthProperty(h);
    if (proph.Type() == 'V') {
      c4_View vwh = ((c4_ViewProp &)proph)(v_[row_]);
      vw = vw.Hash(vwh, 1);
    }
  }

  return vw;
}

#endif 

int MkPath::AttachView(Tcl_Interp * /*interp*/) {
  const char *base = _path;
  const char *p = base;

  //  The format of a path description is:
  //
  //    storage '.' viewname [ '!' row# '.' viewprop ]*
  //  or
  //    storage '.' viewname [ '!' row# '.' viewprop ]* '!' row#
  //
  //  In the second case, the trailing row# is ignored.

  MkWorkspace::Item *ip = _ws != 0 ? _ws->Find(f4_GetToken(p)): 0;
  if (ip != 0) {
    // 16-1-2003: allow path reference to root view (i.e. storage itself)
    if (*p == 0) {
      _view = ip->_storage;
      return p - base;
    }
#if 0
    c4_View root =  *ip->_storage;
    int col = root.FindPropIndexByName(f4_GetToken(p));
    _view = OpenMapped(root, col, 0);
#else 
    _view = ip->_storage.View(f4_GetToken(p));
#endif 
    while (*p) {
      if (!isdigit(*p)) {
        _view = c4_View(); // bad stuff, bail out with an empty view
        break;
      }

      const char *q = p;

      int r = atoi(f4_GetToken(p));

      if (! *p)
        return q - base;
      // return partial number of chars processed

      //  A future version could parse derived view expressions here.
      //  Perhaps this could be done as Metakit property expressions.

      int n = _view.FindPropIndexByName(f4_GetToken(p));
      if (n < 0)
        return q - base;
      // make sure the property exists

      const c4_Property &prop = _view.NthProperty(n);
      if (prop.Type() != 'V')
        return q - base;
      // make sure it's a subview

#if 0
      _view = OpenMapped(_view, n, r);
#else 
      _view = ((c4_ViewProp &)prop)(_view[r]);
      ;
#endif 
    }
  } else
    _view = c4_View();

  return p - base; // return pointer to ending null byte
}

int MkPath::Refs(int diff_) {
  d4_assert( - 1 <= diff_ && diff_ <=  + 1);

  _refs += diff_;

  d4_assert(_refs >= 0);

  if (_refs == 0 && diff_ < 0) {
    delete this;
    return 0;
  }

  return _refs;
}

///////////////////////////////////////////////////////////////////////////////

c4_PtrArray *MkWorkspace::Item::_shared = 0;

MkWorkspace::Item::Item(const char *name_, const char *fileName_, int mode_,
  c4_PtrArray &items_, int index_, bool share_): _name(name_), _fileName
  (fileName_), _items(items_), _index(index_) {
  ++generation; // make sure all cached paths refresh on next access

  if (*fileName_) {
    c4_Storage s(fileName_, mode_);
    if (!s.Strategy().IsValid())
      return ;
    _storage = s;
  }

  if (_index >= _items.GetSize())
    _items.SetSize(_index + 1);

  _items.SetAt(_index, this);

  if (share_) {
    if (_shared == 0)
      _shared = new c4_PtrArray;
    _shared->Add(this);
  }
}

MkWorkspace::Item::~Item() {
  //! ForceRefresh();
  // all views referring to this datafile are made invalid
  for (int i = 0; i < _paths.GetSize(); ++i) {
    MkPath *path = (MkPath*)_paths.GetAt(i);
    if (_index > 0)
      path->_view = c4_View();
    path->_path = "?"; // make sure it never matches
    path->_currGen = -1; // make sure lookup is retried on next use
    // TODO: get rid of generations, use a "_valid" flag instead
  }
  ++generation; // make sure all cached paths refresh on next access

  if (_index < _items.GetSize()) {
    d4_assert(_items.GetAt(_index) == this || _items.GetAt(_index) == 0);
    _items.SetAt(_index, 0);
  }

  if (_shared != 0) {
    for (int i = 0; i < _shared->GetSize(); ++i)
    if (_shared->GetAt(i) == this) {
      _shared->RemoveAt(i);
      break;
    }

    if (_shared->GetSize() == 0) {
      delete _shared;
      _shared = 0;
    }
  }
}

void MkWorkspace::Item::ForceRefresh() {
  // all views referring to this datafile are cleared
  for (int i = 0; i < _paths.GetSize(); ++i) {
    MkPath *path = (MkPath*)_paths.GetAt(i);
    path->_view = c4_View();
  }

  ++generation; // make sure all cached paths refresh on next access
}

MkWorkspace::MkWorkspace(Tcl_Interp *ip_): _interp(ip_) {
  new Item("", "", 0, _items, 0);

  // never uses entry zero (so atoi failure in ForgetPath is harmless)
  _usedRows = _usedBuffer.SetBufferClear(16); 
    // no realloc for first 16 temp rows
}

MkWorkspace::~MkWorkspace() {
  CleanupCommands();

  for (int i = _items.GetSize(); --i >= 0;)
    delete Nth(i);

  // need this to prevent recursion in Tcl_DeleteAssocData in 8.2 (not 8.0!)
  Tcl_SetAssocData(_interp, "mk4tcl", 0, 0);
  Tcl_DeleteAssocData(_interp, "mk4tcl");
}

void MkWorkspace::DefCmd(MkTcl *cmd_) {
  _commands.Add(cmd_);
}

MkWorkspace::Item *MkWorkspace::Define(const char *name_, const char *fileName_,
  int mode_, bool share_) {
  Item *ip = Find(name_);

  if (ip == 0) {
    int n =  - 1;
    while (++n < _items.GetSize())
      if (Nth(n) == 0)
        break;

    ip = new Item(name_, fileName_, mode_, _items, n, share_);
    if (*fileName_ != 0 && !ip->_storage.Strategy().IsValid()) {
      delete ip;
      return 0;
    }
  }

  return ip;
}

MkWorkspace::Item *MkWorkspace::Find(const char *name_)const {
  for (int i = 0; i < _items.GetSize(); ++i) {
    Item *ip = Nth(i);
    if (ip && ip->_name.Compare(name_) == 0)
      return ip;
  }

  if (Item::_shared != 0)
   { // look in the shared pool, if there is one
    for (int j = 0; j < Item::_shared->GetSize(); ++j) {
      Item *ip = (Item*)Item::_shared->GetAt(j);
      if (ip && ip->_name == name_)
        return ip;
    }
  }

  return 0;
}

int MkWorkspace::NumItems()const {
  return _items.GetSize();
}

MkWorkspace::Item *MkWorkspace::Nth(int index_)const {
  return (Item*)_items.GetAt(index_);
}

MkPath *MkWorkspace::AddPath(const char * &name_, Tcl_Interp *interp) {
  const char *p = name_;

  Item *ip = Find(f4_GetToken(p));
  if (ip == 0) {
    ip = Nth(0);
    d4_assert(ip != 0);
    name_ = ""; // no such tag, assign a temporary one instead
  } else
  for (int i = 0; i < ip->_paths.GetSize(); ++i) {
    MkPath *path = (MkPath*)ip->_paths.GetAt(i);
    d4_assert(path != 0);

    if (path->_path.CompareNoCase(name_) == 0 && path->_currGen == generation) {
      path->Refs( + 1);
      return path;
    }
  }

  MkPath *newPath = new MkPath(*this, name_, interp);
  ip->_paths.Add(newPath);

  return newPath;
}

void MkWorkspace::AllocTempRow(c4_String &result_) {
  int i;

  // find an unused row
  for (i = 1; i < _usedBuffer.Size(); ++i)
    if (_usedRows[i] == 0)
      break;

  // allocate new vec if old one is too small, doubling it in size
  if (i >= _usedBuffer.Size()) {
    c4_Bytes temp;
    t4_byte *tempPtr = temp.SetBufferClear(2 *i + 1);
    memcpy(tempPtr, _usedRows, _usedBuffer.Size());

    _usedBuffer.Swap(temp);
    _usedRows = tempPtr;

    c4_View v = Nth(0)->_storage.View("");
    v.SetSize(_usedBuffer.Size());
  }

  // flag it as being in use
  _usedRows[i] = 1;

  // temporary rows have special names
  char buf[20];
  sprintf(buf, "._!%d._", i);
  result_ = buf;
}

void MkWorkspace::ForgetPath(const MkPath *path_) {
  const char *p = path_->_path;

  Item *ip = Find(f4_GetToken(p));
  if (ip != 0) {
    for (int j = 0; j < ip->_paths.GetSize(); ++j)
    if ((MkPath*)ip->_paths.GetAt(j) == path_) {
      ip->_paths.RemoveAt(j);
      break;
    }

    // last ref to a temporary row determines when to release it
    if (ip == Nth(0)) {
      int n = atoi(((const char*)path_->_path) + 3);
      d4_assert(_usedRows[n] != 0);
      _usedRows[n] = 0;
    }
  }
}

void MkWorkspace::Invalidate(const MkPath &path_) {
  const char *p = path_._path;

  c4_String prefix = path_._path + "!";
  int n = prefix.GetLength();

  Item *ip = Find(f4_GetToken(p));
  if (ip != 0) {
    for (int j = 0; j < ip->_paths.GetSize(); ++j) {
      MkPath *entry = (MkPath*)ip->_paths.GetAt(j);
      if (strncmp(entry->_path, prefix, n) == 0)
        entry->_currGen =  - 1;
      // the next use will reattach
    }
  }
}

///////////////////////////////////////////////////////////////////////////////
// Translate between the Metakit and Tcl-style datafile structure descriptions

static c4_String KitToTclDesc(const char *desc_) {
  c4_Bytes temp;
  char *p = (char*)temp.SetBuffer(3 *strlen(desc_) + 100);

  while (*desc_) {
    char *q = p;

    // assume normal property
    while (ISNAME(*desc_) ||  *desc_ == ':')
      *p++ =  *desc_++;

    // strip a trailing ':S'
    if (p[ - 2] == ':' && p[ - 1] == 'S')
      p -= 2;

    // at end of property, process commas and brackets
    switch (*desc_++) {
      // defensive coding, this cannot happen
      case 0:
        --desc_;
        break;

        // opening bracket "xxx[" --> "{xxx {"
      case '[':
         {
          c4_String name(q, p - q);
          *q++ = '{';
          strcpy(q, name);
          ++p;

          *p++ = ' ';
          *p++ = '{';
        }
        break;

        // opening bracket "]" --> "}}"
      case ']':
         {
          *p++ = '}';
          *p++ = '}';
        }
        break;

        // comma separator "," --> " "
      case ',':
        *p++ = ' ';
    }
  }

  *p++ = 0;
  return (const char*)temp.Contents();
}

///////////////////////////////////////////////////////////////////////////////
//
//  Interface to Tcl 8.0 type mechanism, defines a new "mkProperty" datatype
//
//  Since properties are immutable, we don't need most of the calls.

static void FreePropertyInternalRep(Tcl_Obj *propPtr);

static void DupPropertyInternalRep(Tcl_Obj *, Tcl_Obj*) {
  d4_assert(false);
}

static void UpdateStringOfProperty(Tcl_Obj*) {
  d4_assert(false);
}

static int SetPropertyFromAny(Tcl_Interp *, Tcl_Obj*) {
  d4_assert(false);
  return TCL_OK;
}

static Tcl_ObjType mkPropertyType =  {
  (char*)"mkProperty",  // name
  FreePropertyInternalRep,  // freeIntRepProc
  DupPropertyInternalRep,  // dupIntRepProc
  UpdateStringOfProperty,  // updateStringProc
  SetPropertyFromAny  // setFromAnyProc
};

///////////////////////////////////////////////////////////////////////////////

const c4_Property &AsProperty(Tcl_Obj *objPtr, const c4_View &view_) {
  void *tag = (&view_[0])._seq; // horrific hack to get at c4_Sequence pointer
  if (objPtr->typePtr !=  &mkPropertyType || objPtr
    ->internalRep.twoPtrValue.ptr1 != tag) {
    CONST86 Tcl_ObjType *oldTypePtr = objPtr->typePtr;

    char type = 'S';

    int length;
    char *string = Tcl_GetStringFromObj(objPtr, &length);
    c4_Property *prop;

    if (length > 2 && string[length - 2] == ':') {
      type = string[length - 1];
      prop = new c4_Property(type, c4_String(string, length - 2));
    } else
     { // look into the view to try to determine the type
      int n = view_.FindPropIndexByName(string);
      if (n >= 0)
        type = view_.NthProperty(n).Type();
      prop = new c4_Property(type, string);
    }

    if (oldTypePtr && oldTypePtr->freeIntRepProc)
      oldTypePtr->freeIntRepProc(objPtr);

    objPtr->typePtr = &mkPropertyType;
    // use a (char*), because the Mac wants it, others use (void*)
    objPtr->internalRep.twoPtrValue.ptr1 = tag;
    objPtr->internalRep.twoPtrValue.ptr2 = (char*)prop;
  }

  return *(c4_Property*)objPtr->internalRep.twoPtrValue.ptr2;
}

static void FreePropertyInternalRep(Tcl_Obj *propPtr) {
  // no mutex protection needed here, MK's own C++ locking is sufficient
  delete (c4_Property*)propPtr->internalRep.twoPtrValue.ptr2;
}

///////////////////////////////////////////////////////////////////////////////
//
//  Interface to Tcl 8.0 type mechanism, defines a new "mkCursor" datatype

static void FreeCursorInternalRep(Tcl_Obj *propPtr);
static void DupCursorInternalRep(Tcl_Obj *srcPtr, Tcl_Obj *copyPtr);
//static int SetCursorFromAny(Tcl_Interp* interp, Tcl_Obj* objPtr);
static void UpdateStringOfCursor(Tcl_Obj *propPtr);

static Tcl_ObjType mkCursorType =  {
  (char*)"mkCursor",  // name
  FreeCursorInternalRep,  // freeIntRepProc
  DupCursorInternalRep,  // dupIntRepProc
  UpdateStringOfCursor,  // updateStringProc
  SetCursorFromAny  // setFromAnyProc
};

///////////////////////////////////////////////////////////////////////////////
//
//  Cursors in Tcl are implemented as a pointer to an MkPath plus an index.

MkPath &AsPath(Tcl_Obj *obj_) {
  d4_assert(obj_->typePtr ==  &mkCursorType);
  d4_assert(obj_->internalRep.twoPtrValue.ptr2 != 0);

  return *(MkPath*)obj_->internalRep.twoPtrValue.ptr2;
}

int &AsIndex(Tcl_Obj *obj_) {
  d4_assert(obj_->typePtr ==  &mkCursorType);
  d4_assert(obj_->internalRep.twoPtrValue.ptr2 != 0);

  return (int &)obj_->internalRep.twoPtrValue.ptr1;
}

static void FreeCursorInternalRep(Tcl_Obj *cursorPtr) {
  MkPath &path = AsPath(cursorPtr);
  EnterMutex(path._ws->_interp);
  path.Refs( - 1);
  LeaveMutex();
}

static void DupCursorInternalRep(Tcl_Obj *srcPtr, Tcl_Obj *copyPtr) {
  MkPath &path = AsPath(srcPtr);
  EnterMutex(path._ws->_interp);
  path.Refs( + 1);
  copyPtr->internalRep = srcPtr->internalRep;
  copyPtr->typePtr = &mkCursorType;
  LeaveMutex();
}

int SetCursorFromAny(Tcl_Interp *interp, Tcl_Obj *objPtr) {
  d4_assert(interp != 0);
  EnterMutex(interp);

  // force a relookup if the this object is of the wrong generation
  if (objPtr->typePtr == &mkCursorType && AsPath(objPtr)._currGen !=
    generation) {
    // make sure we have a string representation around
    if (objPtr->bytes == 0)
      UpdateStringOfCursor(objPtr);

    // get rid of the object form
    FreeCursorInternalRep(objPtr);
    objPtr->typePtr = 0;
  }

  if (objPtr->typePtr !=  &mkCursorType) {
    CONST86 Tcl_ObjType *oldTypePtr = objPtr->typePtr;

    const char *string = Tcl_GetStringFromObj(objPtr, 0);

    // dig up the workspace used in this interpreter
    MkWorkspace *work = (MkWorkspace*)Tcl_GetAssocData(interp, "mk4tcl", 0);
    // cast required for Mac
    char *s = (char*)(void*)work->AddPath(string, interp);
    int i = isdigit(*string) ? atoi(string):  - 1;

    if (oldTypePtr && oldTypePtr->freeIntRepProc)
      oldTypePtr->freeIntRepProc(objPtr);

    objPtr->typePtr = &mkCursorType;
    objPtr->internalRep.twoPtrValue.ptr1 = (void*)i;
    objPtr->internalRep.twoPtrValue.ptr2 = s;
  }

  LeaveMutex();
  return TCL_OK;
}

static void UpdateStringOfCursor(Tcl_Obj *cursorPtr) {
  MkPath &path = AsPath(cursorPtr);
  EnterMutex(path._ws->_interp);
  c4_String s = path._path;

  int index = AsIndex(cursorPtr);
  if (index >= 0) {
    char buf[20];
    sprintf(buf, "%s%d", s.IsEmpty() ? "" : "!", index);
    s += buf;
  }

  cursorPtr->length = s.GetLength();
  cursorPtr->bytes = strcpy(Tcl_Alloc(cursorPtr->length + 1), s);
  LeaveMutex();
}

static Tcl_Obj *AllocateNewTempRow(MkWorkspace &work_) {
  Tcl_Obj *result = Tcl_NewObj();

  const char *empty = "";
  MkPath *path = work_.AddPath(empty, 0);
  //  path->_view.SetSize(1);

  result->typePtr = &mkCursorType;
  result->internalRep.twoPtrValue.ptr2 = (char*)(void*)path;
  AsIndex(result) = 0;

  Tcl_InvalidateStringRep(result);

  return result;
}

///////////////////////////////////////////////////////////////////////////////
// Helper class for the mk::select command, stores params and performs select

TclSelector::TclSelector(Tcl_Interp *interp_, const c4_View &view_): _interp
  (interp_), _view(view_), _temp(0), _first(0), _count( - 1){}

TclSelector::~TclSelector() {
  for (int i = 0; i < _conditions.GetSize(); ++i)
    delete (Condition*)_conditions.GetAt(i);
}

// convert a property (or list of properties) to an empty view
c4_View TclSelector::GetAsProps(Tcl_Obj *obj_) {
  c4_View result;

  Tcl_Obj *o;

  for (int i = 0; Tcl_ListObjIndex(_interp, obj_, i, &o) == TCL_OK && o != 0; 
    ++i)
    result.AddProperty(AsProperty(o, _view));

  return result;
}

int TclSelector::AddCondition(int id_, Tcl_Obj *props_, Tcl_Obj *value_) {
  c4_View props = GetAsProps(props_);
  if (props.NumProperties() > 0)
    _conditions.Add(new Condition(id_, props, value_));

  return TCL_OK;
}

bool TclSelector::MatchOneString(int id_, const char *value_, const char *crit_)
  {
  switch (id_) {
    case 2:
      // -exact prop value : exact case-sensitive match
      return strcmp(value_, crit_) == 0;

    case 3:
      // -glob prop pattern : match "glob" expression wildcard
      return Tcl_StringMatch(value_, crit_) > 0;

    case 4:
      // -regexp prop pattern : match specified regular expression
      return Tcl_RegExpMatch(_interp, (CONST84 char*)value_, (CONST84 char*)
        crit_) > 0;
    case 5:
      // -keyword prop prefix : match keyword in given property
      return MatchOneKeyword(value_, crit_);

    case 10:
      // -globnc prop pattern : match "glob", but not case sensitive
      return Tcl_StringCaseMatch(value_, crit_, 1) > 0;
  }

  return false;
}

bool TclSelector::Match(const c4_RowRef &row_) {
  // go through each condition and make sure they all match
  for (int i = 0; i < _conditions.GetSize(); ++i) {
    const Condition &cond = *(const Condition*)_conditions.GetAt(i);

    bool matched = false;

    // go through each property until one matches
    for (int j = 0; j < cond._view.NumProperties(); ++j) {
      const c4_Property &prop = cond._view.NthProperty(j);

      if (cond._id < 2)
       { // use typed comparison as defined by Metakit
        c4_Row data; // this is *very* slow in Metakit 1.8
        if (SetAsObj(_interp, data, prop, cond._crit) != TCL_OK)
          return false;

        // data is now a row with the criterium as single property
        matched = cond._id < 0 && data == row_ || cond._id == 0 && data <= row_
          || cond._id > 0 && data >= row_;
      } else
       { // use item value as a string
        GetAsObj(row_, prop, _temp);
        matched = MatchOneString(cond._id, Tcl_GetStringFromObj(_temp, NULL),
          Tcl_GetStringFromObj(cond._crit, NULL));
        if (matched)
          break;
      }
    }

    if (!matched)
      return false;
  }

  return true;
}

// pick out criteria which specify an exact match
void TclSelector::ExactKeyProps(const c4_RowRef &row_) {
  for (int i = 0; i < _conditions.GetSize(); ++i) {
    const Condition &cond = *(const Condition*)_conditions.GetAt(i);
    if (cond._id ==  - 1 || cond._id == 2) {
      for (int j = 0; j < cond._view.NumProperties(); ++j) {
        const c4_Property &prop = cond._view.NthProperty(j);
        SetAsObj(_interp, row_, prop, cond._crit);
      }
    }
  }
}

int TclSelector::DoSelect(Tcl_Obj *list_, c4_View *result_) {
  c4_IntProp pIndex("index");

  // normalize _first and _count to be in allowable range
  int n = _view.GetSize();
  if (_first < 0)
    _first = 0;
  if (_first > n)
    _first = n;
  if (_count < 0)
    _count = n;
  if (_first + _count > n)
    _count = n - _first;

  c4_View result;
  result.SetSize(_count); // upper bound

  // keep a temporary around during the comparison loop
  _temp = Tcl_NewObj();
  KeepRef keeper(_temp);

  // try to take advantage of key lookup structures
  c4_Row exact;
  ExactKeyProps(exact);
  if (exact.Container().NumProperties() > 0)
    _view.RestrictSearch(exact, _first, _count);

  // the matching loop where all the hard work is done
  for (n = 0; _first < _view.GetSize() && n < _count; ++_first)
    if (Match(_view[_first]))
      pIndex(result[n++]) = _first;

  result.SetSize(n);

  // set up sorting, this references/loads a lot of extra Metakit code
  const bool sorted = n > 0 && _sortProps.NumProperties() > 0;

  c4_View mapView;
  c4_View sortResult;
  if (sorted) {
    mapView = _view.RemapWith(result);
    sortResult = mapView.SortOnReverse(_sortProps, _sortRevProps);
  }

  // convert result to a Tcl list of ints
  if (list_ != 0)
  for (int i = 0; i < n; ++i) {
    // sorting means we have to lookup the index of the original again
    int pos = i;
    if (sorted)
      pos = mapView.GetIndexOf(sortResult[i]);

    // set up a Tcl integer which holds the selected row index
    KeepRef o = Tcl_NewIntObj(pIndex(result[pos]));

    if (Tcl_ListObjAppendElement(_interp, list_, o) != TCL_OK)
      return TCL_ERROR;
  }

  // added 2003/02/14: return intermediate view, if requested
  if (result_ != 0)
    *result_ = sorted ? sortResult : result;

  return TCL_OK;
}

///////////////////////////////////////////////////////////////////////////////
// The Tcl class is a generic interface to Tcl, providing some C++ wrapping

Tcl::Tcl(Tcl_Interp *ip_): interp(ip_){}

int Tcl::Fail(const char *msg_, int err_) {
  if (!_error) {
    if (msg_)
      Tcl_SetResult(interp, (char*)msg_, TCL_VOLATILE);
    _error = err_;
  }

  return _error;
}

Tcl_Obj *Tcl::tcl_GetObjResult() {
  return Tcl_GetObjResult(interp);
}

int Tcl::tcl_SetObjResult(Tcl_Obj *obj_) {
  Tcl_SetObjResult(interp, obj_);
  return _error;
}

int Tcl::tcl_ListObjLength(Tcl_Obj *obj_) {
  int result;
  _error = Tcl_ListObjLength(interp, obj_, &result);
  return _error ?  - 1: result;
}

void Tcl::tcl_ListObjAppendElement(Tcl_Obj *obj_, Tcl_Obj *value_) {
  if (!_error)
    if (value_ == 0)
      Fail();
    else
      _error = Tcl_ListObjAppendElement(interp, obj_, value_);
}

bool Tcl::tcl_GetBooleanFromObj(Tcl_Obj *obj_) {
  int value = 0;
  if (!_error)
    _error = Tcl_GetBooleanFromObj(interp, obj_, &value);
  return value != 0;
}

int Tcl::tcl_GetIntFromObj(Tcl_Obj *obj_) {
  int value = 0;
  if (!_error)
    _error = Tcl_GetIntFromObj(interp, obj_, &value);
  return value;
}

long Tcl::tcl_GetLongFromObj(Tcl_Obj *obj_) {
  long value = 0;
  if (!_error)
    _error = Tcl_GetLongFromObj(interp, obj_, &value);
  return value;
}

double Tcl::tcl_GetDoubleFromObj(Tcl_Obj *obj_) {
  double value = 0;
  if (!_error)
    _error = Tcl_GetDoubleFromObj(interp, obj_, &value);
  return value;
}

int Tcl::tcl_GetIndexFromObj(Tcl_Obj *obj_, const char **table_, const char
  *msg_) {
  int index =  - 1;
  if (!_error)
    _error = Tcl_GetIndexFromObj(interp, obj_, (CONST84 char **)table_, msg_, 0,
      &index);
  return _error == TCL_OK ? index :  - 1;
}

long Tcl::tcl_ExprLongObj(Tcl_Obj *obj_) {
  long result = 0;
  if (!_error)
    _error = Tcl_ExprLongObj(interp, obj_, &result);
  return result;
}

Tcl_Obj *Tcl::GetValue(const c4_RowRef &row_, const c4_Property &prop_, Tcl_Obj
  *obj_) {
  obj_ = GetAsObj(row_, prop_, obj_);

  if (!obj_)
    Fail("unsupported property type");

  return obj_;
}

Tcl_Obj *Tcl::tcl_NewStringObj(const char *str_, int len_) {
  return Tcl_NewStringObj((char*)str_, len_);
}

void Tcl::list2desc(Tcl_Obj *in_, Tcl_Obj *out_) {
  Tcl_Obj *o,  **ov;
  int oc;
  if (Tcl_ListObjGetElements(0, in_, &oc, &ov) == TCL_OK && oc > 0) {
    char sep = '[';
    for (int i = 0; i < oc; ++i) {
      Tcl_AppendToObj(out_, &sep, 1);
      sep = ',';
      Tcl_ListObjIndex(0, ov[i], 0, &o);
      if (o != 0)
        Tcl_AppendObjToObj(out_, o);
      Tcl_ListObjIndex(0, ov[i], 1, &o);
      if (o != 0)
        list2desc(o, out_);
    }
    Tcl_AppendToObj(out_, "]", 1);
  }
}

///////////////////////////////////////////////////////////////////////////////
// The MkTcl class adds Metakit-specific utilities and all the command procs.

int MkTcl::Dispatcher(ClientData cd, Tcl_Interp *ip, int oc, Tcl_Obj *const *
  ov) {
  MkTcl *self = (MkTcl*)cd;

  if (self == 0 || self->interp != ip) {
    Tcl_SetResult(ip, (char*)"Initialization error in dispatcher", TCL_STATIC);
    return TCL_ERROR;
  }

  return self->Execute(oc, ov);
}

MkTcl::MkTcl(MkWorkspace *ws_, Tcl_Interp *ip_, int id_, const char *cmd_): Tcl
  (ip_), id(id_), work(*ws_) {
  Tcl_CreateObjCommand(ip_, (char*)cmd_, Dispatcher, this, 0);
}

MkTcl::~MkTcl(){}

c4_View MkTcl::asView(Tcl_Obj *obj_) {
  SetCursorFromAny(interp, obj_);
  return AsPath(obj_)._view;
}

int &MkTcl::changeIndex(Tcl_Obj *obj_) {
  SetCursorFromAny(interp, obj_);
  Tcl_InvalidateStringRep(obj_);
  return AsIndex(obj_);
}

c4_RowRef MkTcl::asRowRef(Tcl_Obj *obj_, int type_) {
  c4_View view = asView(obj_);
  int index = AsIndex(obj_);
  int size = view.GetSize();

  switch (type_) {
    case kExtendRow:
      if (index >= size)
        view.SetSize(size = index + 1);
    case kLimitRow:
      if (index > size)
        Fail("view index is too large");
      else if (index < 0)
        Fail("view index is negative");
      break;

    case kExistingRow:
      if (index < 0 || index >= size) {
        Fail("view index is out of range");
        break;
      }
    case kAnyRow:
      ;
  }

  return view[index];
}

int MkTcl::GetCmd() {
  c4_RowRef row = asRowRef(objv[1], kExistingRow);

  if (!_error) {
    const bool returnSize = objc > 2 &&  // fixed 1999-11-19
    tcl_GetIndexFromObj(objv[2], getCmds) >= 0;
    if (returnSize) {
      --objc;
      ++objv;
    } else {
      _error = TCL_OK; // ignore missing option
      KeepRef o = Tcl_NewObj();
      tcl_SetObjResult(o);
    }

    Tcl_Obj *result = tcl_GetObjResult();

    if (objc < 3) {
      c4_View view = row.Container();
      for (int i = 0; i < view.NumProperties() && !_error; ++i) {
        const c4_Property &prop = view.NthProperty(i);
        if (prop.Type() == 'V')
          continue;
        // omit subviews

        tcl_ListObjAppendElement(result, tcl_NewStringObj(prop.Name()));
        tcl_ListObjAppendElement(result, returnSize ? Tcl_NewIntObj(prop(row)
          .GetSize()): GetValue(row, prop));
      }
    } else if (objc == 3) {
      const c4_Property &prop = AsProperty(objv[2], row.Container());
      if (returnSize)
        Tcl_SetIntObj(result, prop(row).GetSize());
      else
        GetValue(row, prop, result);
    } else {
      for (int i = 2; i < objc && !_error; ++i) {
        const c4_Property &prop = AsProperty(objv[i], row.Container());
        tcl_ListObjAppendElement(result, returnSize ? Tcl_NewIntObj(prop(row)
          .GetSize()): GetValue(row, prop));
      }
    }
  }

  return _error;
}

int MkTcl::SetValues(const c4_RowRef &row_, int objc, Tcl_Obj *const * objv) {
  while (objc >= 2 && !_error) {
    _error = SetAsObj(interp, row_, AsProperty(objv[0], row_.Container()),
      objv[1]);

    objc -= 2;
    objv += 2;
  }

  return _error;
}

int MkTcl::SetCmd() {
  if (objc < 4)
    return GetCmd();

  int size = asView(objv[1]).GetSize();
  c4_RowRef row = asRowRef(objv[1], kExtendRow);

  int e = SetValues(row, objc - 2, objv + 2);
  if (e != TCL_OK)
    asView(objv[1]).SetSize(size);
  // 1.1: restore old size on errors

  if (_error)
    return _error;

  return tcl_SetObjResult(objv[1]);
}

int MkTcl::RowCmd() {
  static const char *cmds[] =  {
    "create", "append", "delete", "insert", "replace", 0
  };

  // "create" is optional if there are no further args
  int id = objc <= 1 ? 0 : tcl_GetIndexFromObj(objv[1], cmds);
  if (id < 0)
    return _error;

  switch (id) {
    case 0:
       {
        Tcl_Obj *var = AllocateNewTempRow(work);
        KeepRef keeper(var);

        SetValues(asRowRef(var, kExtendRow), objc - 2, objv + 2);
        return tcl_SetObjResult(var); // different result
      }

    case 1:
       {
        Tcl_Obj *var = Tcl_DuplicateObj(objv[2]);
        tcl_SetObjResult(var);

        // used to be a single stmt, avoids bug in gcc 2.7.2 on Linux?
        int size = asView(var).GetSize();
        changeIndex(var) = size;

        int oc = objc - 3;
        Tcl_Obj **ov = (Tcl_Obj **)objv + 3;

        // 2003-03-16, allow giving all pairs as list
        if (oc == 1 && Tcl_ListObjGetElements(interp, objv[3], &oc, &ov) !=
          TCL_OK)
          return TCL_ERROR;

        // 2000-06-15: this will not work with custom viewers which
        // take over ordering or uniqueness, because such views can
        // not be resized to create emtpy rows, which get filled in
        int e = SetValues(asRowRef(var, kExtendRow), oc, ov);
        if (e != TCL_OK)
          asView(var).SetSize(size);
        // 1.1: restore old size on errors

        return e;
      }

    case 2:
       {
        c4_RowRef row = asRowRef(objv[2]);
        if (_error)
          return _error;

        c4_View view = row.Container();
        int index = AsIndex(objv[2]);

        int count = objc > 3 ? tcl_GetIntFromObj(objv[3]): 1;
        if (count > view.GetSize() - index)
          count = view.GetSize() - index;

        if (count >= 1) {
          view.RemoveAt(index, count);
          work.Invalidate(AsPath(objv[2]));
        }
      }
      break;

    case 3:
       {
        c4_RowRef toRow = asRowRef(objv[2], kLimitRow);
        if (_error)
          return _error;

        c4_View view = toRow.Container();
        int n = AsIndex(objv[2]);

        int count = objc > 3 ? tcl_GetIntFromObj(objv[3]): 1;
        if (count >= 1) {
          c4_Row temp;
          view.InsertAt(n, temp, count);

          if (objc > 4) {
            c4_RowRef fromRow = asRowRef(objv[4]);
            if (_error)
              return _error;

            while (--count >= 0)
              view[n++] = fromRow;
          }
          work.Invalidate(AsPath(objv[2]));
        }
      }
      break;

    case 4:
       {
        c4_RowRef row = asRowRef(objv[2]);
        if (_error)
          return _error;

        if (objc > 3)
          row = asRowRef(objv[3]);
        else
          row = c4_Row();
      }
      break;
  }

  if (_error)
    return _error;

  return tcl_SetObjResult(objv[2]);
}

int MkTcl::FileCmd() {
  static const char *cmds[] =  {
    "open", "end", "close", "commit", "rollback", "load", "save", "views", 
      "aside", "autocommit", "space", 0
  };

  int id = tcl_GetIndexFromObj(objv[1], cmds);
  if (id < 0)
    return _error;

  if (id == 0 && objc == 2)
   { // new in 1.1: return list of db's
    Tcl_Obj *result = tcl_GetObjResult();

    // skip first entry, which is for temp rows
    for (int i = 1; i < work.NumItems() && !_error; ++i) {
      MkWorkspace::Item *ip = work.Nth(i);

      if (ip != 0) {
        tcl_ListObjAppendElement(result, tcl_NewStringObj(ip->_name));
        tcl_ListObjAppendElement(result, tcl_NewStringObj(ip->_fileName));
      }
    }

    return _error;
  }

  const char *string = Tcl_GetStringFromObj(objv[2], 0);

  MkWorkspace::Item *np = work.Find(f4_GetToken(string));
  if (np == 0 && id > 1)
    return Fail("no storage with this name");

  switch (id) {
    case 0:
       { // open
        if (np != 0)
          return Fail("file already open");

        int mode = 1;
        bool nocommit = false, shared = false;
        static const char *options[] =  {
          "-readonly", "-extend", "-nocommit", "-shared", 0
        }
        ;

        while (objc > 2 &&  *Tcl_GetStringFromObj(objv[objc - 1], 0) == '-')
        switch (tcl_GetIndexFromObj(objv[--objc], options)) {
        case 0:
          mode = 0;
          break;
        case 1:
          mode = 2;
          break;
        case 2:
          nocommit = true;
          break;
        case 3:
          shared = true;
          break;
        default:
          return _error;
        }

        const char *name = Tcl_GetStringFromObj(objv[2], 0);
        int len = 0;
        const char *file = objc < 4 ? "": Tcl_GetStringFromObj(objv[3], &len);
#ifdef WIN32
        np = work.Define(name, file, mode, shared);
#else 
        Tcl_DString ds;
        const char *native = Tcl_UtfToExternalDString(NULL, file, len, &ds);
        np = work.Define(name, native, mode, shared);
        Tcl_DStringFree(&ds);
#endif 
        if (np == 0)
          return Fail("file open failed");

        if (*file && mode != 0 && !nocommit)
          np->_storage.AutoCommit();
      }
      break;

    case 1:
       { // end
        int len;
        const char *name = Tcl_GetStringFromObj(objv[2], &len);
        c4_FileStrategy strat;
#ifdef WIN32
        int err = strat.DataOpen(name, false);
#else 
        Tcl_DString ds;
        const char *native = Tcl_UtfToExternalDString(NULL, name, len, &ds);
        int err = strat.DataOpen(native, false);
        Tcl_DStringFree(&ds);
#endif 
        if (!err || !strat.IsValid())
          return Fail("no such file");
        t4_i32 end = strat.EndOfData();
        if (end < 0)
          return Fail("not a Metakit datafile");

        Tcl_SetIntObj(tcl_GetObjResult(), end);
        return _error;
      }
      break;

    case 2:
       { // close
        delete np;
      }
      break;

    case 3:
       { // commit
        if (!np->_storage.Strategy().IsValid())
          return Fail("cannot commit temporary dataset");

        np->ForceRefresh(); // detach first

        // 1-Mar-1999: check commit success
        bool full = objc > 3 && strcmp(Tcl_GetStringFromObj(objv[3], 0), 
          "-full") == 0;
        if (!np->_storage.Commit(full))
          return Fail("I/O error during commit");
      }
      break;

    case 4:
       { // rollback
        if (!np->_storage.Strategy().IsValid())
          return Fail("cannot rollback temporary dataset");

        np->ForceRefresh(); // detach first

        bool full = objc > 3 && strcmp(Tcl_GetStringFromObj(objv[3], 0), 
          "-full") == 0;
        np->_storage.Rollback(full);
      }
      break;

    case 5:
       { // load
        char *channel = Tcl_GetStringFromObj(objv[3], 0);

        int mode;
        Tcl_Channel cp = Tcl_GetChannel(interp, channel, &mode);
        if (cp == 0 || !(mode &TCL_READABLE))
          return Fail("load from channel failed");

        if (Tcl_SetChannelOption(interp, cp, "-translation", "binary"))
          return Fail();

        np->ForceRefresh(); // detach first

        c4_TclStream stream(cp);
        if (!np->_storage.LoadFrom(stream))
          return Fail("load error");
      }
      break;

    case 6:
       { // save
        char *channel = Tcl_GetStringFromObj(objv[3], 0);

        int mode;
        Tcl_Channel cp = Tcl_GetChannel(interp, channel, &mode);
        if (cp == 0 || !(mode &TCL_WRITABLE))
          return Fail("save to channel failed");

        if (Tcl_SetChannelOption(interp, cp, "-translation", "binary"))
          return Fail();

        c4_TclStream stream(cp);
        np->_storage.SaveTo(stream);
      }
      break;

    case 7:
       { // views
        c4_View view = np->_storage;
        Tcl_Obj *result = tcl_GetObjResult();

        for (int i = 0; i < view.NumProperties() && !_error; ++i) {
          const c4_Property &prop = view.NthProperty(i);
          tcl_ListObjAppendElement(result, tcl_NewStringObj(prop.Name()));
        }

        return _error; // different result
      }

    case 8:
       { // aside
        if (objc != 4)
          return Fail("mk::file aside: needs 2 storage args");

        const char *as = Tcl_GetStringFromObj(objv[3], 0);
        MkWorkspace::Item *np2 = work.Find(f4_GetToken(as));
        if (np2 == 0)
          return Fail("no storage with this name");

        np->_storage.SetAside(np2->_storage);
      }
      break;

    case 9:
       { // autocommit
        if (objc != 3)
          return Fail("mk::file autocommit: too many args");

        np->_storage.AutoCommit();
      }
      break;

    case 10:
       { // space, new on 30-11-2001:  returns allocator used space pairs
        // nasty hack to obtain the storage's sequence pointer
        c4_View v = np->_storage;
        c4_Cursor c = &v[0];
        c4_Sequence *s = c._seq;

        // even more horrible (i.e. brittle) hack to get the space vector
        c4_Persist *p = s->Persist();
        c4_PtrArray *a = p != 0 ? *(c4_PtrArray **)p: 0; // first field
        if (a == 0)
          return Fail("storage is not persistent");

        // now return the values as a list
        Tcl_Obj *r = tcl_GetObjResult();
        for (int i = 1; i < a->GetSize() - 1 && !_error; ++i)
          tcl_ListObjAppendElement(r, Tcl_NewLongObj((long)a->GetAt(i)));
        return _error;
      }
  }

  if (_error)
    return _error;

  return tcl_SetObjResult(objv[2]);
}

int MkTcl::ViewCmd() {
  int id = tcl_GetIndexFromObj(objv[1], viewCmds);
  if (id < 0)
    return _error;

  switch (id) {
    case 0:
      // layout
      if (objc == 3) {
        const char *string = Tcl_GetStringFromObj(objv[2], 0);

        MkWorkspace::Item *np = work.Find(f4_GetToken(string));
        if (np == 0)
          return Fail("no storage with this name");

        c4_Storage &s = np->_storage;

        const char *p = s.Description(f4_GetToken(string));
        if (p == 0)
          return Fail("no view with this name");

        c4_String desc = KitToTclDesc(p);
        KeepRef o = tcl_NewStringObj(desc);
        return tcl_SetObjResult(o); // different result
      }
      // else fall through
    case 1:
       { // delete
        const char *string = Tcl_GetStringFromObj(objv[2], 0);

        MkWorkspace::Item *np = work.Find(f4_GetToken(string));
        if (np == 0 && id != 4)
          return Fail("no storage with this name");

        c4_String s = f4_GetToken(string);
        if (s.IsEmpty() ||  *string != 0)
          return Fail("unrecognized view name");

        if (id == 0) {
          KeepRef o = tcl_NewStringObj(s);
          list2desc(objv[3], o);
          const char *desc = Tcl_GetStringFromObj(o, 0);
          if (desc &&  *desc)
            np->_storage.GetAs(desc);
        }
         else {
          c4_View v = np->_storage;
          if (v.FindPropIndexByName(s) < 0)
            return Fail("no view with this name");

          np->_storage.GetAs(s);
        }

        np->ForceRefresh(); // make sure views are re-attached
      }
      break;

    case 2:
       { // size
        c4_View view = asView(objv[2]);

        if (objc > 3) {
          int i = tcl_GetIntFromObj(objv[3]);
          if (_error)
            return _error;
          view.SetSize(i);
        }

        Tcl_SetIntObj(tcl_GetObjResult(), view.GetSize());
        return _error; // different result
      }
      break;

    case 3:
      // properties
    case 8:
       { // info (will be deprecated)
        c4_View view = asView(objv[2]);
        Tcl_Obj *result = tcl_GetObjResult();

        for (int i = 0; i < view.NumProperties() && !_error; ++i) {
          const c4_Property &prop = view.NthProperty(i);

          c4_String s = prop.Name();
          if (prop.Type() != 'S') {
            s += ":";
            s += prop.Type();
          }

          tcl_ListObjAppendElement(result, tcl_NewStringObj(s));
        }

        return _error;
      }

    case 4:
       { // locate
        c4_View view = asView(objv[2]);

        bool force = strcmp(Tcl_GetStringFromObj(objv[3], 0), "-force") == 0;
        int k = force ? 4 : 3;

        if (k >= objc)
          return Fail("no key specified");

        c4_Row key;

        for (int i = 0; k + i < objc; ++i) {
          const c4_Property &prop = view.NthProperty(i);
          _error = SetAsObj(interp, key, prop, objv[k + i]);
          if (_error)
            return _error;
        }

        int pos;
        if (view.Locate(key, &pos) == 0)
          if (force)
            view.InsertAt(pos, key);
          else
            return Fail("key not found");

        Tcl_SetIntObj(tcl_GetObjResult(), pos);
        return _error;
      }

    case 5:
       { // restrict
        if (objc <= 5)
          return Fail("too few args");

        c4_View view = asView(objv[2]);
        c4_View hview = asView(objv[3]);
        int nkeys = tcl_GetIntFromObj(objv[4]);
        view = view.Hash(hview, nkeys);

        c4_Row key;

        for (int i = 0; i + 5 < objc; ++i) {
          const c4_Property &prop = view.NthProperty(i);
          _error = SetAsObj(interp, key, prop, objv[i + 5]);
          if (_error)
            return _error;
        }

        int pos = 0;
        int count = view.GetSize();
        int result = view.RestrictSearch(key, pos, count);

        Tcl_Obj *r = tcl_GetObjResult();
        tcl_ListObjAppendElement(r, Tcl_NewIntObj(result));
        tcl_ListObjAppendElement(r, Tcl_NewIntObj(pos));
        tcl_ListObjAppendElement(r, Tcl_NewIntObj(count));
        return _error;
      }

    case 6:
       { // open
        if (objc < 3 || objc > 4)
          return Fail("wrong number of args");

        c4_View view = asView(objv[2]);
        const char *name = objc > 3 ? Tcl_GetStringFromObj(objv[3], 0): "";

        MkView *cmd = new MkView(interp, view, name);
        Tcl_SetStringObj(tcl_GetObjResult(), (char*)(const char*)cmd->CmdName(),
          - 1);
        return _error;
      }

    case 7:
       { // new ?name?
        if (objc < 2 || objc > 3)
          return Fail("wrong number of args");

        c4_View view;
        const char *name = objc > 3 ? Tcl_GetStringFromObj(objv[2], 0): "";

        MkView *cmd = new MkView(interp, view, name);
        Tcl_SetStringObj(tcl_GetObjResult(), (char*)(const char*)cmd->CmdName(),
          - 1);
        return _error;
      }
  }

  if (_error)
    return _error;

  return tcl_SetObjResult(objv[2]);
}

int MkTcl::LoopCmd() {
  Tcl_Obj *value = objc >= 4 ? Tcl_ObjSetVar2(interp, objv[1], 0, objv[2],
    TCL_LEAVE_ERR_MSG): Tcl_ObjGetVar2(interp, objv[1], 0, TCL_LEAVE_ERR_MSG);
  if (value == 0)
    return Fail();
  // has to exist, can't be valid otherwise

  long first = objc >= 5 ? tcl_ExprLongObj(objv[3]): 0;
  long limit = objc >= 6 ? tcl_ExprLongObj(objv[4]): asView(value).GetSize();
  long incr = objc >= 7 ? tcl_ExprLongObj(objv[5]): 1;

  if (incr == 0)
    Fail("increment must be nonzero");

  if (_error)
    return _error;

  Tcl_Obj *var = objv[1];
  Tcl_Obj *cmd = objv[objc - 1];

  for (int i = first;; i += incr) {
    if (Tcl_IsShared(value))
      value = Tcl_DuplicateObj(value);

    changeIndex(value) = i;

    if (Tcl_ObjSetVar2(interp, var, 0, value, TCL_LEAVE_ERR_MSG) == 0)
      return Fail();

    if (!(i < limit && incr > 0 || i > limit && incr < 0))
      break;

    LeaveMutex();
    _error = Tcl_EvalObj(interp, cmd);
    EnterMutex(interp);

    if (_error == TCL_CONTINUE)
      _error = TCL_OK;

    if (_error) {
      if (_error == TCL_BREAK)
        _error = TCL_OK;
      else if (_error == TCL_ERROR) {
        char msg[100];
        sprintf(msg, "\n  (\"mk::loop\" body line %d)", interp->errorLine);
        Tcl_AddObjErrorInfo(interp, msg,  - 1);
      }
      break;
    }
  }

  if (_error == TCL_OK)
    Tcl_ResetResult(interp);

  return _error;
}

int MkTcl::CursorCmd() {
  int id = tcl_GetIndexFromObj(objv[1], cursorCmds);
  if (id < 0)
    return _error;

  Tcl_Obj *name = objv[2];

  Tcl_Obj *var = 0;

  if (id == 0) {
    var = objc < 4 ? AllocateNewTempRow(work): objv[3]; // create expects a path

    --objc; // shift so the index will be picked up if present
    ++objv;
  } else
   { // alter an existing cursor
    var = Tcl_ObjGetVar2(interp, name, 0, TCL_LEAVE_ERR_MSG);
    if (var == 0)
      return Fail();
    // has to exist, can't be valid otherwise
  }

  // about to modify, so make sure we are sole owners
  Tcl_Obj *original = 0;
  if (Tcl_IsShared(var)) {
    original = var;
    var = Tcl_DuplicateObj(var);
  }

  KeepRef keeper(var);

  c4_View view = asView(var);

  int value;
  if (objc <= 3) {
    if (id == 1)
     { // position without value returns current value
      Tcl_SetIntObj(tcl_GetObjResult(), AsIndex(var));
      return _error;
    }

    value = id == 0 ? 0 : 1; // create defaults to 0, incr defaults to 1
  } else if (Tcl_GetIntFromObj(interp, objv[3], &value) != TCL_OK) {
    const char *step = Tcl_GetStringFromObj(objv[3], 0);
    if (strcmp(step, "end") == 0)
      value = view.GetSize() - 1;
    else {
      if (original)
        Tcl_DecrRefCount(original);
      return Fail();
    }
  }

  if (id < 2)
    changeIndex(var) = value;
  else
    changeIndex(var) += value;

  Tcl_Obj *result = Tcl_ObjSetVar2(interp, name, 0, var, TCL_LEAVE_ERR_MSG);
  if (result == 0)
    return Fail();

  return tcl_SetObjResult(result);
}

int MkTcl::SelectCmd() {
  TclSelector sel(interp, asView(objv[1]));

  static const char *opts[] =  {
    "-min",  // 0
    "-max",  // 1
    "-exact",  // 2
    "-glob",  // 3
    "-regexp",  // 4
    "-keyword",  // 5
    "-first",  // 6
    "-count",  // 7
    "-sort",  // 8
    "-rsort",  // 9
    "-globnc",  // 10
    0
  };

  while (objc >= 4) {
    objc -= 2; // gobble next two arguments
    objv += 2;

    // at this point, *objv is the next option, and objc >= 2

    int id =  - 1;

    const char *p = Tcl_GetStringFromObj(*objv, 0);
    if (p &&  *p == '-') {
      id = tcl_GetIndexFromObj(*objv, opts);
      if (id < 0)
        return _error;
    }

    switch (id) {
      case  - 1:  { // prop value : case-insensitive match
        _error = sel.AddCondition( - 1, objv[0], objv[1]);
      }
      break;

      case 0:
        // -min prop value : property must be greater or equal to value
      case 1:
        // -max prop value : property must be less or equal to value
      case 2:
        // -exact prop value : exact case-sensitive match
      case 3:
        // -glob prop pattern : match "glob" expression wildcard
      case 4:
        // -regexp prop pattern : match specified regular expression
      case 5:
        // -keyword prop prefix : match keyword in given property
      case 10:
         { // -globnc prop pattern : match "glob", but ignore case
          if (objc < 3)
            return Fail("not enough arguments");

          _error = sel.AddCondition(id, objv[1], objv[2]);

          --objc; // gobble a third argument
          ++objv;
        }
        break;

      case 6:
        // -first pos : searching starts at specified row index
      case 7:
         { // -count num : return no more than this many results
          int n = tcl_GetIntFromObj(objv[1]);
          if (_error)
            return _error;

          if (id == 6)
            sel._first = n;
          else
            sel._count = n;
        }
        break;

      case 8:
        // -sort prop : sort on one or more properties, ascending
      case 9:
         { // -rsort prop : sort on one or more properties, descending
          c4_View props = sel.GetAsProps(objv[1]);
          for (int i = 0; i < props.NumProperties(); ++i) {
            const c4_Property &prop = props.NthProperty(i);

            sel._sortProps.AddProperty(prop);
            if (id == 9)
              sel._sortRevProps.AddProperty(prop);
          }
        }
        break;
    }
  }

  if (_error)
    return _error;

  return sel.DoSelect(tcl_GetObjResult());
}

int MkTcl::ChannelCmd() {
  c4_RowRef row = asRowRef(objv[1]);
  MkPath &path = AsPath(objv[1]);
  int index = AsIndex(objv[1]);

  if (_error)
    return _error;

  const c4_BytesProp &memo = (const c4_BytesProp &)AsProperty(objv[2],
    path._view);

  int id = objc < 4 ? 0 : tcl_GetIndexFromObj(objv[3], channelCmds);
  if (id < 0)
    return _error;

  const char *p = path._path;
  MkWorkspace::Item *ip = work.Find(f4_GetToken(p));
  if (ip == 0)
    return Fail("no storage with this name");

  if (id == 1)
    memo(row).SetData(c4_Bytes());
  // truncate the existing contents

  int mode = id == 0 ? TCL_READABLE : id == 1 ? TCL_WRITABLE : TCL_READABLE |
    TCL_WRITABLE;

  MkChannel *mkChan = new MkChannel(ip->_storage, path._view, memo, index);
  d4_assert(mkChan != 0);

  static int mkChanSeq = 0;
  char buffer[10];
  sprintf(buffer, "mk%d", ++mkChanSeq);

  mkChan->_watchMask = 0;
  mkChan->_validMask = mode;
  mkChan->_interp = interp;
  mkChan->_chan = Tcl_CreateChannel(&mkChannelType, buffer, (ClientData)mkChan,
    mode);

  if (id == 2)
    Tcl_Seek(mkChan->_chan, 0, SEEK_END);

  Tcl_RegisterChannel(interp, mkChan->_chan);

  if (_error)
    return _error;

  KeepRef o = tcl_NewStringObj(buffer);
  return tcl_SetObjResult(o);
}

int MkTcl::Execute(int oc, Tcl_Obj *const * ov) {
  struct CmdDef {
    int min;
    int max;
    const char *desc;
  };

  static CmdDef defTab[] =  {
     {
      2, 0, "get cursor ?prop ...?"
    }
    ,  {
      3, 0, "set cursor prop ?value prop value ...?"
    }
    ,  {
      3, 5, "cursor option cursorname ?...?"
    }
    ,  {
      2, 0, "row option ?cursor ...?"
    }
    ,  {
      2, 0, "view option view ?arg?"
    }
    ,  {
      2, 6, "file option ?tag ...?"
    }
    ,  {
      3, 7, "loop cursor ?path first limit incr? {cmds}"
    }
    ,  {
      2, 0, "select path ?...?"
    }
    ,  {
      3, 4, "channel path prop ?mode?"
    }
    , 
    {
      0, 0, 0
    }
    , 
  };

  _error = TCL_OK;

  CmdDef &cd = defTab[id];

  objc = oc;
  objv = ov;

  if (oc < cd.min || (cd.max > 0 && oc > cd.max)) {
    msg = "wrong # args: should be \"mk::";
    msg += cd.desc;
    msg += "\"";

    return Fail(msg);
  }

  EnterMutex(interp);
  int result = 0;
  switch (id) {
    case 0:
      result = GetCmd();
      break;
    case 1:
      result = SetCmd();
      break;
    case 2:
      result = CursorCmd();
      break;
    case 3:
      result = RowCmd();
      break;
    case 4:
      result = ViewCmd();
      break;
    case 5:
      result = FileCmd();
      break;
    case 6:
      result = LoopCmd();
      break;
    case 7:
      result = SelectCmd();
      break;
    case 8:
      result = ChannelCmd();
      break;
  }
  LeaveMutex();
  return result;
}

///////////////////////////////////////////////////////////////////////////////

void MkWorkspace::CleanupCommands() {
  for (int i = 0; i < _commands.GetSize(); ++i)
    delete (MkTcl*)_commands.GetAt(i);
  _commands.SetSize(0);
}

static void ExitProc(ClientData cd_) {
  delete (MkWorkspace*)cd_;
}

static void DelProc(ClientData cd_, Tcl_Interp *ip_) {
  // got here through assoc's delproc, don't trigger again on exit
  Tcl_DeleteExitHandler(ExitProc, cd_);
  ExitProc(cd_);
}

static int Mktcl_Cmds(Tcl_Interp *interp, bool /*safe*/) {
  if (Tcl_InitStubs(interp, "8.1", 0) == 0)
    return TCL_ERROR;

  // Create workspace if not present.
  MkWorkspace *ws = (MkWorkspace*)Tcl_GetAssocData(interp, "mk4tcl", 0);
  if (ws == 0) {
    Tcl_RegisterObjType(&mkPropertyType);
    Tcl_RegisterObjType(&mkCursorType);

    ws = new MkWorkspace(interp);
    // add an association with delproc to catch "interp delete",
    // since that does not seem to trigger exitproc handling (!)
    Tcl_SetAssocData(interp, "mk4tcl", DelProc, ws);
    Tcl_CreateExitHandler(ExitProc, ws);
  }

  // this list must match the "CmdDef defTab []" above.
  static const char *cmds[] =  {
    "get", "set", "cursor", "row", "view", "file", "loop", "select", "channel", 
    0
  };

  c4_String prefix = "mk::";

  for (int i = 0; cmds[i]; ++i)
    ws->DefCmd(new MkTcl(ws, interp, i, prefix + cmds[i]));

  return Tcl_PkgProvide(interp, "Mk4tcl", "2.4.9.7");
}

///////////////////////////////////////////////////////////////////////////////
// The proper way to load this extension is with "load mk4tcl.{so,dll} mk4tcl",
// but 8.0.2 load guesses module "mk" instead of "mk4tcl" (it stops at digits)
// when the third argument is omitted, allow that too: "load mk4tcl.{so,dll}".

EXTERN int Mk4tcl_Init(Tcl_Interp *interp) {
  return Mktcl_Cmds(interp, false);
}

EXTERN int Mk_Init(Tcl_Interp *interp) {
  return Mktcl_Cmds(interp, false);
}

EXTERN int Mk4tcl_SafeInit(Tcl_Interp *interp) {
  return Mktcl_Cmds(interp, true);
}

EXTERN int Mk_SafeInit(Tcl_Interp *interp) {
  return Mktcl_Cmds(interp, true);
}

///////////////////////////////////////////////////////////////////////////////
