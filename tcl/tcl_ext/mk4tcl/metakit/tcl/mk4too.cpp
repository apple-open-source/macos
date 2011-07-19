// mk4too.cpp -- Tcl object command interface to Metakit
// $Id: mk4too.cpp 4452 2008-12-10 22:57:54Z patthoyts $
// This is part of Metakit, see http://www.equi4.com/metakit.html
// Copyright (C) 2000-2004 by Matt Newman and Jean-Claude Wippler.

#include "mk4tcl.h"
#include <stdio.h>
#include <string.h>

#if 10 * TCL_MAJOR_VERSION + TCL_MINOR_VERSION < 86
#define Tcl_GetErrorLine(interp) (interp)->errorLine
#endif

///////////////////////////////////////////////////////////////////////////////
// Defined in this file:

class MkView;

///////////////////////////////////////////////////////////////////////////////
// The MkView class adds Metakit-specific utilities and all the command procs.

int MkView::Dispatcher(ClientData cd, Tcl_Interp *ip, int oc, Tcl_Obj *const *
  ov) {
    MkView *self = (MkView*)cd;

    if (self == 0 || self->interp != ip) {
        Tcl_SetResult(ip, "Initialization error in dispatcher", TCL_STATIC);
        return TCL_ERROR;
    }
    return self->Execute(oc, ov);
}

void MkView::DeleteProc(ClientData cd) {
  MkView *self = (MkView*)cd;
  delete self;
}

MkView::MkView(Tcl_Interp *ip_, c4_View view_, const char *name): Tcl(ip_),
  work(*(MkWorkspace*)Tcl_GetAssocData(interp, "mk4tcl", 0)), view(view_) {
  Register(name);
}

MkView::MkView(Tcl_Interp *ip_, const char *name): Tcl(ip_), work(*
  (MkWorkspace*)Tcl_GetAssocData(interp, "mk4tcl", 0)) {
  Register(name);
}

MkView::~MkView(){}

void MkView::Register(const char *name) {
  static int uid = 0;
  char buf[32];

  if (name == 0 ||  *name == 0) {
    sprintf(buf, "%d", uid++);
    cmd = "view" + (c4_String)buf;
  } else {
    cmd = name;
  }
  // save token... so I can delete cmd later (even if renamed)
  cmdToken = Tcl_CreateObjCommand(interp, (char*)(const char*)cmd, MkView
    ::Dispatcher, this, MkView::DeleteProc);
}

c4_View MkView::View(Tcl_Interp *interp, Tcl_Obj *obj) {
  const char *name = Tcl_GetStringFromObj(obj, 0);
  Tcl_CmdInfo ci;

  if (!Tcl_GetCommandInfo(interp, (char*)name, &ci) || ci.objProc != MkView
    ::Dispatcher) {
    //Fail("no such view");
    c4_View temp;
    return temp;
  } else {
    MkView *v = (MkView*)ci.objClientData;
    return v->view;
  }
}

int MkView::asIndex(c4_View &view, Tcl_Obj *obj_, bool mayExceed_) {
  int size = view.GetSize();
  int index;

  if (Tcl_GetIntFromObj(interp, obj_, &index) != TCL_OK) {
    const char *step = Tcl_GetStringFromObj(obj_, 0);
    if (step != 0 && strcmp(step, "end") == 0) {
      index = !mayExceed_ ? size - 1: size;
      Tcl_ResetResult(interp); // clear error
      _error = TCL_OK;
    } else {
      index =  - 1;
    }
  }

  if (mayExceed_) {
    if (index > size)
      Fail("view index is too large");
    else if (index < 0)
      Fail("view index is negative");
  } else if (index < 0 || index >= size)
    Fail("view index is out of range");

  return index;
}

int MkView::SetValues(const c4_RowRef &row_, int objc, Tcl_Obj *const * objv,
  c4_View &view_) {
  if (objc % 2)
    Fail("bad args: must be prop value pairs");

  while (objc > 0 && !_error) {
    _error = SetAsObj(interp, row_, AsProperty(objv[0], view_), objv[1]);

    objc -= 2;
    objv += 2;
  }

  return _error;
}

int MkView::Execute(int oc, Tcl_Obj *const * ov) {
  struct CmdDef {
    int(MkView:: *proc)();
    int min;
    int max;
    const char *desc;
  };

  static const char *subCmds[] =  {
    "close", "delete", "exists", "find", "get", "properties", "insert", "open",
      "search", "select", "set", "size", "loop", "view", "info",  
      // will be deprecated (use "properties" instead)
    0
  };
  static CmdDef defTab[] =  {
    // the "&MkView::" stuff is required for Mac cwpro2
     {
       &MkView::CloseCmd, 2, 2, "close"
    }
    ,  {
       &MkView::DeleteCmd, 3, 4, "delete cursor ?cursor2?"
    }
    ,  {
       &MkView::ExistsCmd, 3, 0, "exists cursor ?prop ...?"
    }
    ,  {
       &MkView::FindCmd, 2, 0, "find ?prop value ...?"
    }
    ,  {
       &MkView::GetCmd, 3, 0, "get cursor ?prop ...?"
    }
    ,  {
       &MkView::InfoCmd, 2, 2, "properties"
    }
    ,  {
       &MkView::InsertCmd, 3, 0, "insert cursor ?prop ...?"
    }
    ,  {
       &MkView::OpenCmd, 4, 4, "open cursor prop"
    }
    ,  {
       &MkView::SearchCmd, 4, 4, "search prop value"
    }
    ,  {
       &MkView::SelectCmd, 2, 0, "select ?..?"
    }
    ,  {
       &MkView::SetCmd, 3, 0, "set cursor prop ?value prop value ...?"
    }
    ,  {
       &MkView::SizeCmd, 2, 3, "size ?newsize?"
    }
    ,  {
       &MkView::LoopCmd, 3, 0, "loop cursor ?first? ?limit? ?step? body"
    }
    ,  {
       &MkView::ViewCmd, 3, 0, "view option ?args?"
    }
    ,  {
       &MkView::InfoCmd, 2, 2, "info"
    }
    ,  {
      0, 0, 0, 0
    }
    , 
  };
  _error = TCL_OK;

  int id = tcl_GetIndexFromObj(ov[1], subCmds);

  if (id ==  - 1)
    return TCL_ERROR;

  CmdDef &cd = defTab[id];

  objc = oc;
  objv = ov;

  if (oc < cd.min || (cd.max > 0 && oc > cd.max)) {
    msg = "wrong # args: should be \"$obj ";
    msg += cd.desc;
    msg += "\"";

    return Fail(msg);
  }

  return (this->*cd.proc)();
}

//
// Tcl command methods
//

int MkView::CloseCmd() {
  // remove command instance... this will call delete...
  Tcl_DeleteCommandFromToken(interp, cmdToken);
  return TCL_OK;
}

int MkView::DeleteCmd() {
  int count = 1;
  int index = asIndex(view, objv[2], true);

  if (_error)
    return _error;

  if (objc > 3) {
    int index2 = asIndex(view, objv[3], true);

    if (_error)
      return _error;

    count = index2 - index + 1;
  }

  if (count > view.GetSize() - index)
    count = view.GetSize() - index;

  if (count >= 1) {
    view.RemoveAt(index, count);
  }
  return TCL_OK;
}

int MkView::ExistsCmd() {
  asIndex(view, objv[2], false);
  int r = _error ? 0 : 1;
  _error = 0;
  return tcl_SetObjResult(Tcl_NewIntObj(r));
}

int MkView::FindCmd() {
  c4_Row row;
  int idx = 2;

  while (idx < objc && !_error) {
    _error = SetAsObj(interp, row, AsProperty(objv[idx], view), objv[idx + 1]);
    idx += 2;
  }
  if (_error)
    return _error;

  idx = view.Find(row, 0);
  if (idx ==  - 1) {
    Fail("not found");
    return TCL_ERROR;
  }
  return tcl_SetObjResult(Tcl_NewIntObj(idx));
}

int MkView::GetCmd() {
  int index = asIndex(view, objv[2], false);
  if (_error)
    return _error;

  Tcl_Obj *result = tcl_GetObjResult();
  c4_RowRef row = view[index];

  if (objc < 4) {
    for (int i = 0; i < view.NumProperties() && !_error; ++i) {
      const c4_Property &prop = view.NthProperty(i);
      c4_String name = prop.Name();

      if (prop.Type() == 'V')
        continue;
      // omit subviews

      tcl_ListObjAppendElement(result, tcl_NewStringObj(name));
      tcl_ListObjAppendElement(result, GetValue(row, prop));
    }
  } else if (objc == 4) {
    GetValue(row, AsProperty(objv[3], view), result);
  } else {
    for (int i = 3; i < objc && !_error; ++i) {
      const c4_Property &prop = AsProperty(objv[i], view);
      tcl_ListObjAppendElement(result, GetValue(row, prop));
    }
  }
  return _error;
}

int MkView::InfoCmd() {
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
  return tcl_SetObjResult(result);
}

int MkView::InsertCmd() {
  int index = asIndex(view, objv[2], true);
  if (_error)
    return _error;

  c4_Row temp;
  SetValues(temp, objc - 3, objv + 3, view);
  view.InsertAt(index, temp, 1);
  //SetValues(view[index], objc - 3, objv + 3);

  if (_error) {
    view.RemoveAt(index, 1); // remove new row on errors
    return _error;
  }
  return tcl_SetObjResult(Tcl_NewIntObj(index));
}

int MkView::OpenCmd() {
  int index = asIndex(view, objv[2], false);

  if (_error)
    return _error;

  const c4_Property &prop = AsProperty(objv[3], view);
  if (_error)
    return _error;

  if (prop.Type() != 'V') {
    Fail("bad property: must be a view");
    return TCL_ERROR;
  }
  MkView *ncmd = new MkView(interp, ((const c4_ViewProp &)prop)(view[index]));

  return tcl_SetObjResult(tcl_NewStringObj(ncmd->CmdName()));
}

int MkView::SearchCmd() {
  Tcl_Obj *obj_ = objv[3];
  const c4_Property &prop = AsProperty(objv[2], view);
  char type = prop.Type();
  double dblVal = 0, dtmp;
  long longVal = 0;
#ifdef TCL_WIDE_INT_TYPE
  Tcl_WideInt wideVal = 0, wtmp;
#endif 
  c4_String strVal;

  int size = view.GetSize();
  int first = 0, last = size;
  int row, rc, e;

  switch (type) {
    case 'S':
       {
        strVal = Tcl_GetStringFromObj(obj_, 0);
      }
      break;

    case 'F':
    case 'D':
       {
        e = Tcl_GetDoubleFromObj(interp, obj_, &dblVal);
        if (e != TCL_OK)
          return e;
      }
      break;

#ifdef TCL_WIDE_INT_TYPE
    case 'L':
       {
        e = Tcl_GetWideIntFromObj(interp, obj_, &wideVal);
        if (e != TCL_OK)
          return e;
      }
      break;
#endif 

    case 'I':
       {
        e = Tcl_GetLongFromObj(interp, obj_, &longVal);
        if (e != TCL_OK)
          return e;
      }
      break;

    default:
      Tcl_SetResult(interp, "unsupported property type", TCL_STATIC);
      return TCL_ERROR;
  }

  while (first <= last) {
    row = (first + last) / 2;

    if (row >= size)
      break;

    switch (type) {
      case 'S':
        rc = strVal.CompareNoCase(((c4_StringProp &)prop)(view[row]));
        break;
      case 'F':
        dtmp = dblVal - ((c4_FloatProp &)prop)(view[row]);
        rc = (dtmp < 0 ?  - 1: (dtmp > 0));
        break;
      case 'D':
        dtmp = dblVal - ((c4_DoubleProp &)prop)(view[row]);
        rc = (dtmp < 0 ?  - 1: (dtmp > 0));
        break;
#ifdef TCL_WIDE_INT_TYPE
      case 'L':
        wtmp = wideVal - ((c4_LongProp &)prop)(view[row]);
        rc = (wtmp < 0 ?  - 1: (wtmp > 0));
        break;
#endif 
      case 'I':
        rc = longVal - ((c4_IntProp &)prop)(view[row]);
        break;
      default:
        rc = 0; // 27-09-2001, to satisfy MSVC6 warn level 4
    }

    if (rc == 0) {
      goto done;
    } else if (rc > 0) {
      first = row + 1;
    } else {
      last = row - 1;
    }
  }
  // Not found
  row =  - 1;
  done: return tcl_SetObjResult(Tcl_NewIntObj(row));
}

int MkView::SelectCmd() {
  TclSelector sel(interp, view);

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

  c4_View nview;
  sel.DoSelect(0, &nview);
  MkView *ncmd = new MkView(interp, nview);
  return tcl_SetObjResult(tcl_NewStringObj(ncmd->CmdName()));
}

int MkView::SetCmd() {
  if (objc < 4)
    return GetCmd();

  int index = asIndex(view, objv[2], false);
  if (_error)
    return _error;

  return SetValues(view[index], objc - 3, objv + 3, view);
}

int MkView::SizeCmd() {
  if (objc > 2) {
    int i = tcl_GetIntFromObj(objv[2]);
    if (_error)
      return _error;
    view.SetSize(i);
  }

  return tcl_SetObjResult(Tcl_NewIntObj(view.GetSize()));
}

int MkView::LoopCmd() {
  long first = 0;
  long limit = view.GetSize();
  long incr = 1;

  if (objc >= 5)
    first = tcl_ExprLongObj(objv[3]);

  if (objc >= 6)
    limit = tcl_ExprLongObj(objv[4]);

  if (objc >= 7) {
    incr = tcl_ExprLongObj(objv[5]);
    if (incr == 0)
      Fail("increment has to be nonzero");
  }

  if (_error)
    return _error;

  Tcl_Obj *vname = objv[2];
  Tcl_Obj *cmd = objv[objc - 1];

  for (int i = first; i < limit && incr > 0 || i > limit && incr < 0; i += incr)
    {
    Tcl_Obj *var = Tcl_ObjSetVar2(interp, vname, 0, Tcl_NewIntObj(i),
      TCL_LEAVE_ERR_MSG);
    if (var == 0)
      return Fail();

    _error = Mk_EvalObj(interp, cmd);

    if (_error) {
      if (_error == TCL_CONTINUE)
        _error = TCL_OK;
      else {
        if (_error == TCL_BREAK)
          _error = TCL_OK;
        else if (_error == TCL_ERROR) {
          char msg[100];
          sprintf(msg, "\n  (\"mk::loop\" body line %d)", Tcl_GetErrorLine(interp));
          Tcl_AddObjErrorInfo(interp, msg,  - 1);
        }
        break;
      }
    }
  }

  if (_error == TCL_OK)
    Tcl_ResetResult(interp);

  return _error;
}

int MkView::ViewCmd() {
  struct CmdDef {
    int(MkView:: *proc)();
    int min;
    int max;
    const char *desc;
  };

  static const char *subCmds[] =  {
    "blocked", "clone", "concat", "copy", "different", "dup", "flatten", 
      "groupby", "hash", "indexed", "intersect", "join", "map", "minus", 
      "ordered", "pair", "product", "project", "range", "readonly", "rename", 
      "restrict", "union", "unique", 
#if 0
    "==", "!=", "<", ">", "<=", ">=", 
#endif 
    0
  };
  static CmdDef defTab[] =  {
    // the "&MkView::" stuff is required for Mac cwpro2
     {
       &MkView::BlockedCmd, 2, 2, "blocked"
    }
    ,  {
       &MkView::CloneCmd, 2, 2, "clone"
    }
    ,  {
       &MkView::ConcatCmd, 3, 3, "concat view"
    }
    ,  {
       &MkView::CopyCmd, 2, 3, "copy"
    }
    ,  {
       &MkView::DifferentCmd, 3, 3, "different view"
    }
    ,  {
       &MkView::DupCmd, 2, 2, "dup"
    }
    ,  {
       &MkView::FlattenCmd, 3, 3, "flatten prop"
    }
    ,  {
       &MkView::GroupByCmd, 4, 0, "groupby subview prop ?prop ...?"
    }
    ,  {
       &MkView::HashCmd, 3, 4, "hash map ?numkeys?"
    }
    ,  {
       &MkView::IndexedCmd, 5, 0, "indexed map unique prop ?prop ...?"
    }
    ,  {
       &MkView::IntersectCmd, 3, 3, "intersect view"
    }
    ,  {
       &MkView::JoinCmd, 4, 0, "join view prop ?prop ...?"
    }
    ,  {
       &MkView::MapCmd, 3, 3, "map view"
    }
    ,  {
       &MkView::MinusCmd, 3, 3, "minus view"
    }
    ,  {
       &MkView::OrderedCmd, 2, 3, "ordered ?numkeys?"
    }
    ,  {
       &MkView::PairCmd, 3, 3, "pair view"
    }
    ,  {
       &MkView::ProductCmd, 3, 3, "product view"
    }
    ,  {
       &MkView::ProjectCmd, 3, 0, "project prop ?prop ...?"
    }
    ,  {
       &MkView::RangeCmd, 4, 0, "range start finish ?step?"
    }
    ,  {
       &MkView::ReadOnlyCmd, 2, 2, "readonly"
    }
    ,  {
       &MkView::RenameCmd, 4, 4, "rename oprop nprop"
    }
    ,  {
       &MkView::RestrictCmd, 2, 0, "restrict...."
    }
    ,  {
       &MkView::UnionCmd, 3, 3, "union view"
    }
    ,  {
       &MkView::UniqueCmd, 2, 2, "unique"
    }
    , 
#if 0
     {
       &MkView::OperatorCmd, 3, 3, "== view"
    }
    ,  {
       &MkView::OperatorCmd, 3, 3, "!= view"
    }
    ,  {
       &MkView::OperatorCmd, 3, 3, "< view"
    }
    ,  {
       &MkView::OperatorCmd, 3, 3, "> view"
    }
    ,  {
       &MkView::OperatorCmd, 3, 3, "<= view"
    }
    ,  {
       &MkView::OperatorCmd, 3, 3, ">= view"
    }
    , 
#endif 
     {
      0, 0, 0, 0
    }
    , 
  };
  _error = TCL_OK;

  objc--;
  objv++;

  int id = tcl_GetIndexFromObj(objv[1], subCmds);

  if (id ==  - 1)
    return TCL_ERROR;

  CmdDef &cd = defTab[id];

  if (objc < cd.min || (cd.max > 0 && objc > cd.max)) {
    msg = "wrong # args: should be \"$obj view ";
    msg += cd.desc;
    msg += "\"";

    return Fail(msg);
  }

  return (this->*cd.proc)();
}

//
// View-based methods (typically return a new view)
//
int MkView::BlockedCmd() {
  MkView *ncmd = new MkView(interp, view.Blocked());

  return tcl_SetObjResult(tcl_NewStringObj(ncmd->CmdName()));
}

int MkView::DupCmd() {
  MkView *cmd = new MkView(interp, view);

  return tcl_SetObjResult(tcl_NewStringObj(cmd->CmdName()));
}

int MkView::CloneCmd() {
  MkView *cmd = new MkView(interp, view.Clone());

  return tcl_SetObjResult(tcl_NewStringObj(cmd->CmdName()));
}

int MkView::ConcatCmd() {
  c4_View nview = View(interp, objv[2]);
  MkView *ncmd = new MkView(interp, view.Concat(nview));

  return tcl_SetObjResult(tcl_NewStringObj(ncmd->CmdName()));
}

int MkView::DifferentCmd() {
  c4_View nview = View(interp, objv[2]);
  MkView *ncmd = new MkView(interp, view.Different(nview));

  return tcl_SetObjResult(tcl_NewStringObj(ncmd->CmdName()));
}

int MkView::CopyCmd() {
  MkView *cmd = new MkView(interp, view.Duplicate());

  return tcl_SetObjResult(tcl_NewStringObj(cmd->CmdName()));
}

int MkView::FlattenCmd() {
  c4_View nview;

  const c4_Property &prop = AsProperty(objv[2], view);
  if (_error)
    return _error;

  if (prop.Type() != 'V') {
    Fail("bad property: must be a view");
    return TCL_ERROR;
  }
  MkView *ncmd = new MkView(interp, view.JoinProp((const c4_ViewProp &)prop));

  return tcl_SetObjResult(tcl_NewStringObj(ncmd->CmdName()));
}

int MkView::GroupByCmd() {
  const c4_Property &prop = AsProperty(objv[2], view);
  if (_error)
    return _error;

  if (prop.Type() != 'V') {
    Fail("bad property: must be a view");
    return TCL_ERROR;
  }
  c4_View nview;

  for (int i = 3; i < objc && !_error; ++i) {
    const c4_Property &prop = AsProperty(objv[i], view);
    nview.AddProperty(prop);
  }
  if (_error)
    return _error;

  MkView *ncmd = new MkView(interp, view.GroupBy(nview, (const c4_ViewProp &)
    prop));

  return tcl_SetObjResult(tcl_NewStringObj(ncmd->CmdName()));
}

int MkView::HashCmd() {
  c4_View nview = View(interp, objv[2]);
  int nkeys = objc > 3 ? tcl_GetIntFromObj(objv[3]): 1;
  MkView *ncmd = new MkView(interp, view.Hash(nview, nkeys));

  return tcl_SetObjResult(tcl_NewStringObj(ncmd->CmdName()));
}

int MkView::IndexedCmd() {
  c4_View map = View(interp, objv[2]);
  bool unique = tcl_GetIntFromObj(objv[3]) != 0;

  c4_View props;
  for (int i = 4; i < objc && !_error; ++i) {
    const c4_Property &prop = AsProperty(objv[i], view);
    props.AddProperty(prop);
  }
  if (_error)
    return _error;

  MkView *ncmd = new MkView(interp, view.Indexed(map, props, unique));

  return tcl_SetObjResult(tcl_NewStringObj(ncmd->CmdName()));
}

int MkView::IntersectCmd() {
  c4_View nview = View(interp, objv[2]);
  MkView *ncmd = new MkView(interp, view.Intersect(nview));

  return tcl_SetObjResult(tcl_NewStringObj(ncmd->CmdName()));
}

int MkView::JoinCmd() {
  c4_View nview = View(interp, objv[2]);
  c4_View props;

  for (int i = 3; i < objc && !_error; ++i) {
    const c4_Property &prop = AsProperty(objv[i], view);
    props.AddProperty(prop);
  }
  if (_error)
    return _error;

  MkView *ncmd = new MkView(interp, view.Join(props, nview));

  return tcl_SetObjResult(tcl_NewStringObj(ncmd->CmdName()));
}

int MkView::MapCmd() {
  c4_View nview = View(interp, objv[2]);
  MkView *ncmd = new MkView(interp, view.RemapWith(nview));

  return tcl_SetObjResult(tcl_NewStringObj(ncmd->CmdName()));
}

int MkView::MinusCmd() {
  c4_View nview = View(interp, objv[2]);
  MkView *ncmd = new MkView(interp, view.Minus(nview));

  return tcl_SetObjResult(tcl_NewStringObj(ncmd->CmdName()));
}

#if 0
int MkView::OperatorCmd() {
  c4_String op = (const char*)Tcl_GetStringFromObj(objv[1], 0);
  c4_View nview = View(interp, objv[2]);
  bool rc;

  if (op == "==")
    rc = (view == nview);
  else if (op == "!=")
    rc = (view != nview);
  else if (op == "<")
    rc = (view < nview);
  else if (op == ">")
    rc = (view > nview);
  else if (op == ">=")
    rc = (view >= nview);
  else if (op == "<=")
    rc = (view <= nview);
  else
    return Fail("bad operator: must be one of ==, !=, <, >, <=, >=");

  return tcl_SetObjResult(Tcl_NewBooleanObj(rc ? 1 : 0));
}

#endif 

int MkView::OrderedCmd() {
  int nkeys = objc > 2 ? tcl_GetIntFromObj(objv[2]): 1;
  MkView *ncmd = new MkView(interp, view.Ordered(nkeys));

  return tcl_SetObjResult(tcl_NewStringObj(ncmd->CmdName()));
}

int MkView::PairCmd() {
  c4_View nview = View(interp, objv[2]);

  MkView *ncmd = new MkView(interp, view.Pair(nview));

  return tcl_SetObjResult(tcl_NewStringObj(ncmd->CmdName()));
}

int MkView::ProductCmd() {
  c4_View nview = View(interp, objv[2]);
  MkView *ncmd = new MkView(interp, view.Product(nview));

  return tcl_SetObjResult(tcl_NewStringObj(ncmd->CmdName()));
}

int MkView::ProjectCmd() {
  c4_View nview;

  for (int i = 2; i < objc; i++) {
    const c4_Property &prop = AsProperty(objv[i], view);

    nview.AddProperty(prop);
  }
  MkView *ncmd = new MkView(interp, view.Project(nview));

  return tcl_SetObjResult(tcl_NewStringObj(ncmd->CmdName()));
}

int MkView::RangeCmd() {
  int start = asIndex(view, objv[2], false);
  if (_error)
    return _error;

  int finish = objc > 3 ? asIndex(view, objv[3], false) + 1: start + 1;
  if (_error)
    return _error;

  int step = objc > 4 ? tcl_GetIntFromObj(objv[4]): 1;
  if (_error)
    return _error;

  MkView *ncmd = new MkView(interp, view.Slice(start, finish, step));

  return tcl_SetObjResult(tcl_NewStringObj(ncmd->CmdName()));
}

int MkView::ReadOnlyCmd() {
  MkView *ncmd = new MkView(interp, view.ReadOnly());

  return tcl_SetObjResult(tcl_NewStringObj(ncmd->CmdName()));
}

int MkView::RenameCmd() {
  const c4_Property &oprop = AsProperty(objv[2], view);
  if (_error)
    return _error;

  const c4_Property &nprop = AsProperty(objv[3], view);
  if (_error)
    return _error;

  MkView *ncmd = new MkView(interp, view.Rename(oprop, nprop));

  return tcl_SetObjResult(tcl_NewStringObj(ncmd->CmdName()));
}

int MkView::RestrictCmd() {
  int index = asIndex(view, objv[2], false);
  int pos = tcl_GetIntFromObj(objv[3]);
  int count = tcl_GetIntFromObj(objv[4]);

  int result = view.RestrictSearch(view[index], pos, count);

  Tcl_Obj *r = tcl_GetObjResult();
  tcl_ListObjAppendElement(r, Tcl_NewIntObj(result));
  tcl_ListObjAppendElement(r, Tcl_NewIntObj(pos));
  tcl_ListObjAppendElement(r, Tcl_NewIntObj(count));
  return _error;
}

int MkView::UnionCmd() {
  c4_View nview = View(interp, objv[2]);
  MkView *ncmd = new MkView(interp, view.Union(nview));

  return tcl_SetObjResult(tcl_NewStringObj(ncmd->CmdName()));
}

int MkView::UniqueCmd() {
  MkView *ncmd = new MkView(interp, view.Unique());

  return tcl_SetObjResult(tcl_NewStringObj(ncmd->CmdName()));
}
