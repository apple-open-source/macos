// mk4tcl.h --
// $Id: mk4tcl.h 4435 2008-08-01 19:58:42Z patthoyts $
// This is part of Metakit, the homepage is http://www.equi4.com/metakit.html

#include "config.h"
#include "mk4.h"
#include "mk4str.h"
#include "../src/univ.h"

#include <tcl.h>

#ifdef BUILD_Mk4tcl
#undef TCL_STORAGE_CLASS
#define TCL_STORAGE_CLASS DLLEXPORT
#endif

#ifndef d4_assert
#if q4_INLINE && !q4_CHECK
// if inlining is on, assume it's release code and disable assertions
#define d4_assert(x)
#elif defined (ASSERT)
#define d4_assert(x) ASSERT(x)
#else 
#include <assert.h>
#define d4_assert(x) assert(x)
#endif 
#endif 

#ifndef CONST84
#define CONST84
#endif 

#ifndef CONST86
#define CONST86
#endif

#ifndef TCL_DECLARE_MUTEX
#define TCL_DECLARE_MUTEX(v)
#define Tcl_MutexLock(v)
#define Tcl_MutexUnlock(v)
#endif 

///////////////////////////////////////////////////////////////////////////////
// Defined in this file:

class MkPath;
class MkWorkspace;
class Tcl;
class MkTcl;

///////////////////////////////////////////////////////////////////////////////
// Utility code: return next token up to char < '0', and
// advance the string pointer past following character.

c4_String f4_GetToken(const char * &str_);

///////////////////////////////////////////////////////////////////////////////
// Utility code: true if value contains a word starting with the given prefix

bool MatchOneKeyword(const char *value_, const c4_String &crit_);

///////////////////////////////////////////////////////////////////////////////
// Utility class: increments and decrements reference count for auto cleanup

class KeepRef {
    Tcl_Obj *_obj;
  public:
    KeepRef(Tcl_Obj *obj_): _obj(obj_) {
        Tcl_IncrRefCount(_obj);
    }
    ~KeepRef() {
        Tcl_DecrRefCount(_obj);
    }

    operator Tcl_Obj *()const {
        return _obj;
    }
};

///////////////////////////////////////////////////////////////////////////////
// Utility code: get a Metakit item and convert it to a Tcl object

Tcl_Obj *GetAsObj(const c4_RowRef &row_, const c4_Property &prop_, Tcl_Obj
  *obj_ = 0);

///////////////////////////////////////////////////////////////////////////////
// Utility code: set a Metakit item and convert it from a Tcl object

int SetAsObj(Tcl_Interp *interp, const c4_RowRef &row_, const c4_Property
  &prop_, Tcl_Obj *obj_);

///////////////////////////////////////////////////////////////////////////////
// A path is a view which knows its place, and what workspace it belongs to.
// Since it contains a string version, its tag can be used to find the item.

class MkPath {
    int _refs; // reference count

  public:
    MkPath(MkWorkspace &ws_, const char * &path_, Tcl_Interp *interp);
    ~MkPath(); // don't use explicit destruction, use Refs(-1)

    int AttachView(Tcl_Interp *interp);
    int Refs(int diff_);

    MkWorkspace *_ws; // avoid globals, but there is usually just one
    c4_View _view; // the view corresponding to this path
    c4_String _path; // describes view, starting with storage tag
    int _currGen; // tracks the generation to force reloads
};

///////////////////////////////////////////////////////////////////////////////
// A workspace manages a number of storage objects and their associated paths.

class MkWorkspace {
    c4_PtrArray _items; // items, or null if released
    c4_Bytes _usedBuffer; // buffer, using 1 byte per entry
    t4_byte *_usedRows; // 1 if that row in item 0 is currently in use
    c4_PtrArray _commands;

  public:
    Tcl_Interp *_interp;

    struct Item {
        const c4_String _name; // the alias for this storage
        const c4_String _fileName;
        c4_Storage _storage; // the storage object
        c4_PtrArray _paths; // the paths associated with this entry
        c4_PtrArray &_items; // array from which this item is referenced
        int _index; // position in the _items array

        //Item ();        // special first entry initializer
        Item(const char *name_, const char *fileName_, int mode_, c4_PtrArray
          &items_, int index_, bool share_ = false);
        ~Item();

        void ForceRefresh(); // bump the generation to recreate views

        static c4_PtrArray *_shared; // shared items are also listed here
    };

    MkWorkspace(Tcl_Interp *ip_);
    ~MkWorkspace();

    void DefCmd(MkTcl *cmd_); // 1.2: for cleanup
    void CleanupCommands();

    Item *Define(const char *name_, const char *fileName_, int mode_, bool
      share_);

    Item *Find(const char *name_)const;
    int NumItems()const;
    Item *Nth(int index_)const;

    // create a new path if it doesn't exist, else bump the reference count
    MkPath *AddPath(const char * &name_, Tcl_Interp *interp);
    // decrease the reference count, delete path if it is no longer used
    void ForgetPath(const MkPath *path_);
    // create a path to a temporary row
    void AllocTempRow(c4_String &);

    // adjust paths of all subviews if the parent position has changed
    void Invalidate(const MkPath &path_);
};

///////////////////////////////////////////////////////////////////////////////
//
//  Interface to Tcl 8.0 type mechanism, defines a new "mkProperty" datatype
//
//  Since properties are immutable, we don't need most of the calls.
///////////////////////////////////////////////////////////////////////////////

const c4_Property &AsProperty(Tcl_Obj *objPtr, const c4_View &view_);

///////////////////////////////////////////////////////////////////////////////
//
//  Interface to Tcl 8.0 type mechanism, defines a new "mkCursor" datatype
//
///////////////////////////////////////////////////////////////////////////////
//
//  Cursors in Tcl are implemented as a pointer to an MkPath plus an index.

MkPath &AsPath(Tcl_Obj *obj_);
int &AsIndex(Tcl_Obj *obj_);
int SetCursorFromAny(Tcl_Interp *interp, Tcl_Obj *objPtr);

// 24nov02: added to support releasing mutex lock during loop eval's
int Mk_EvalObj(Tcl_Interp *ip_, Tcl_Obj *cmd_);

///////////////////////////////////////////////////////////////////////////////
// Helper class for the mk::select command, stores params and performs select

class TclSelector {
    c4_PtrArray _conditions;
    Tcl_Interp *_interp;
    c4_View _view;
    Tcl_Obj *_temp;

  public:
    class Condition {
      public:
        int _id;
        c4_View _view;
        Tcl_Obj *_crit; // no need to incref, original lifetime is guaranteed

        Condition(int id_, const c4_View &view_, Tcl_Obj *crit_): _id(id_),
          _view(view_), _crit(crit_){}
    };

    c4_View _sortProps;
    c4_View _sortRevProps;
    int _first;
    int _count;

    TclSelector(Tcl_Interp *interp_, const c4_View &view_);
    ~TclSelector();

    c4_View GetAsProps(Tcl_Obj *obj_);
    int AddCondition(int id_, Tcl_Obj *props_, Tcl_Obj *value_);
    bool MatchOneString(int id_, const char *value_, const char *crit_);
    bool Match(const c4_RowRef &row_);
    void ExactKeyProps(const c4_RowRef &row_);
    int DoSelect(Tcl_Obj *list_, c4_View *result_ = 0);
};

///////////////////////////////////////////////////////////////////////////////
// The Tcl class is a generic interface to Tcl, providing some C++ wrapping

class Tcl {
  protected:
    Tcl_Interp *interp;
    int _error;

  public:
    Tcl(Tcl_Interp *ip_);

    int Fail(const char *msg_ = 0, int err_ = TCL_ERROR);
    Tcl_Obj *tcl_GetObjResult();
    int tcl_SetObjResult(Tcl_Obj *obj_);
    int tcl_ListObjLength(Tcl_Obj *obj_);
    void tcl_ListObjAppendElement(Tcl_Obj *obj_, Tcl_Obj *value_);
    bool tcl_GetBooleanFromObj(Tcl_Obj *obj_);
    int tcl_GetIntFromObj(Tcl_Obj *obj_);
    long tcl_GetLongFromObj(Tcl_Obj *obj_);
    double tcl_GetDoubleFromObj(Tcl_Obj *obj_);
    int tcl_GetIndexFromObj(Tcl_Obj *obj_, const char **table_, const char
      *msg_ = "option");
    long tcl_ExprLongObj(Tcl_Obj *obj_);

    Tcl_Obj *GetValue(const c4_RowRef &row_, const c4_Property &prop_, Tcl_Obj
      *obj_ = 0);
    Tcl_Obj *tcl_NewStringObj(const char *str_, int len_ =  - 1);
    void list2desc(Tcl_Obj *in, Tcl_Obj *out);
};

// The MkTcl class adds Metakit-specific utilities and all the command procs.

class MkTcl: public Tcl {
    int id;
    int objc;
    Tcl_Obj *const * objv;
    c4_String msg;
    MkWorkspace &work;

    static int Dispatcher(ClientData cd, Tcl_Interp *ip, int oc, Tcl_Obj *const
      * ov);

  public:
    enum {
        kAnyRow, kExistingRow, kLimitRow, kExtendRow
    };

    MkTcl(MkWorkspace *ws_, Tcl_Interp *ip_, int id_, const char *cmd_);
    ~MkTcl();

    c4_View asView(Tcl_Obj *obj_);
    int &changeIndex(Tcl_Obj *obj_);
    c4_RowRef asRowRef(Tcl_Obj *obj_, int type_ = kExistingRow);
    int GetCmd();
    int SetValues(const c4_RowRef &row_, int objc, Tcl_Obj *const * objv);
    int SetCmd();
    int RowCmd();
    int FileCmd();
    int ViewCmd();
    int LoopCmd();
    int CursorCmd();
    int SelectCmd();
    int ChannelCmd();
    int NewCmd();
    int Try1Cmd();
    int Try2Cmd();
    int Try3Cmd();
#if MKSQL
    int SqlAuxCmd();
#endif 
    int Execute(int oc, Tcl_Obj *const * ov);
};

///////////////////////////////////////////////////////////////////////////////

class MkView: public Tcl {
    int objc;
    Tcl_Obj *const * objv;
    Tcl_Command cmdToken;
    c4_String msg;
    MkWorkspace &work;
    c4_View view;
    c4_String cmd;

    static int Dispatcher(ClientData cd, Tcl_Interp *ip, int oc, Tcl_Obj *const
      * ov);
    static void DeleteProc(ClientData cd);

  public:

    MkView(Tcl_Interp *ip_, c4_View view_, const char *name = 0);
    MkView(Tcl_Interp *ip_, const char *name = 0);
    ~MkView();

    void Register(const char *name);

    static c4_View View(Tcl_Interp *interp, Tcl_Obj *obj);

    c4_String CmdName() {
        return cmd;
    }

    int asIndex(c4_View &view, Tcl_Obj *obj_, bool mayExceed_);
    int SetValues(const c4_RowRef &row_, int objc, Tcl_Obj *const * objv,
      c4_View &);

    c4_View &View() {
        return view;
    }

    int CloseCmd(); // $obj close
    int DeleteCmd(); // $obj delete cursor ?count?
    int FindCmd(); // $obj find ?prop value ...?
    int GetCmd(); // $obj get cursor ?prop prop ...?
    int ExistsCmd(); // $obj exists cursor
    int InfoCmd(); // $obj info
    int InsertCmd(); // $obj insert cursor ?prop prop ...?
    int OpenCmd(); // $obj open cursor prop
    int SearchCmd(); // $obj search prop value
    int SelectCmd(); // $obj select ....
    int SetCmd(); // $obj set cursor ?prop value ...?
    int SizeCmd(); // $obj size ?newsize?
    int LoopCmd(); // $obj loop cursor ?first? ?limit? ?step? {cmds}
    int ViewCmd(); // $obj view option ?args?

    int CloneCmd(); // $obj view clone
    int ConcatCmd(); // $obj view concat view
    int CopyCmd(); // $obj view copy
    int DifferentCmd(); // $obj view different view
    int DupCmd(); // $obj view dup
    int BlockedCmd(); // $obj view blocked
    int FlattenCmd(); // $obj view flatten prop
    int GroupByCmd(); // $obj view groupby subview prop ?prop ...?
    int HashCmd(); // $obj view hash view ?numkeys?
    int IndexedCmd(); // $obj view indexed map unique prop ?prop ...?
    int IntersectCmd(); // $obj view intersect view
    int JoinCmd(); // $obj view join view prop ?prop ...?
    int MapCmd(); // $obj view map view
    int MinusCmd(); // $obj view minus view
    int OrderedCmd(); // $obj view ordered ?numKeys?
    int PairCmd(); // $obj view pair view
    int ProductCmd(); // $obj view product view
    int ProjectCmd(); // $obj view project prop ?prop ...?
    int RangeCmd(); // $obj view range start ?limit? ?step?
    int ReadOnlyCmd(); // $obj view readonly
    int RenameCmd(); // $obj view rename oprop nprop
    int RestrictCmd(); // $obj view restrict cursor pos count
    int UnionCmd(); // $obj view union view
    int UniqueCmd(); // $obj view unique

    int Execute(int oc, Tcl_Obj *const * ov);
};

///////////////////////////////////////////////////////////////////////////////
