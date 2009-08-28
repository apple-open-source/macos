// PyView.cpp --
// $Id: PyView.cpp 1230 2007-03-09 15:58:53Z jcw $
// This is part of MetaKit, the homepage is http://www.equi4.com/metakit.html
// Copyright (C) 1999-2004 Gordon McMillan and Jean-Claude Wippler.
//
//  View class implementation
//  setsize method added by J. Barnard

#include "PyView.h"
#include "PyProperty.h"
#include "PyRowRef.h"
#include <PWOMSequence.h>
#include <PWONumber.h>
#include <PWOMapping.h>
#include <PWOCallable.h>

// see pep 353 at http://www.python.org/dev/peps/pep-0353/
#if PY_VERSION_HEX < 0x02050000
typedef int Py_ssize_t;
#define PY_SSIZE_T_MAX INT_MAX
#define PY_SSIZE_T_MIN INT_MIN
#endif 

static void MustBeView(PyObject *o) {
  if (!PyGenericView_Check(o))
    Fail(PyExc_TypeError, "Arg must be a view object");
}

static char *setsize__doc = 
  "setsize(nrows) -- adjust the number of rows in a view";

static PyObject *PyView_setsize(PyView *o, PyObject *_args) {
  try {
    PWOSequence args(_args);
    if (args.len() != 1)
      Fail(PyExc_TypeError, "setsize() takes exactly one argument");
    PWONumber nrows = PWONumber(args[0]);
    o->SetSize((int)nrows);
    return nrows.disOwn();
  } catch (...) {
    return 0;
  }
}

static char *structure__doc = "structure() -- return list of properties";

static PyObject *PyView_structure(PyView *o, PyObject *_args) {
  try {
    PWOSequence args(_args);
    if (args.len() != 0)
      Fail(PyExc_TypeError, "method takes no arguments");
    return o->structure();
  } catch (...) {
    return 0;
  }
}

static char *properties__doc = 
  "properties() -- return a dictionary mapping property names to property objects";

static PyObject *PyView_properties(PyView *o, PyObject *_args) {
  try {
    PWOSequence args(_args);
    if (args.len() != 0)
      Fail(PyExc_TypeError, "method takes no arguments");
    return o->properties();
  } catch (...) {
    return 0;
  }
}

static char *insert__doc = 
  "insert(position, obj) -- coerce obj (or keyword args) to row and insert before position";

static PyObject *PyView_insert(PyView *o, PyObject *_args, PyObject *kwargs) {
  try {
    PWOSequence args(_args);
    int argcount = args.len();
    if (argcount == 0 || argcount > 2) {
      Fail(PyExc_TypeError, 
        "insert() takes exactly two arguments, or one argument and keyword arguments");
    } else {
      int size = PWONumber(o->GetSize()), ndx = PWONumber(args[0]);
      if (ndx < 0) {
        ndx += size;
        if (ndx < 0) {
          ndx = 0;
        }
      } else if (ndx > size) {
        ndx = size;
      }
      if (argcount == 1)
        o->insertAt(ndx, kwargs);
      else if (argcount == 2)
        o->insertAt(ndx, args[1]);
      Py_INCREF(Py_None);
      return Py_None;
    }
  } catch (...){}
  return 0; /* satisfy compiler */
}

static char *append__doc = 
  "append(obj) -- coerce obj (or keyword args) to row and append, returns position";

static PyObject *PyView_append(PyView *o, PyObject *_args, PyObject *kwargs) {
  try {
    PWOSequence args(_args);
    PWONumber ndx(o->GetSize());
    int argcount = args.len();
    if (argcount == 0)
      o->insertAt(ndx, kwargs);
    else if (argcount == 1)
      o->insertAt(ndx, args[0]);
    else
      Fail(PyExc_TypeError, 
        "append() takes exactly one argument, or multiple keyword arguments");
    return ndx.disOwn();
  } catch (...) {
    return 0;
  }
}

static char *delete__doc = 
  "delete(position) -- delete row at specified position";

static PyObject *PyView_delete(PyView *o, PyObject *_args) {
  try {
    PWOSequence args(_args);
    int ndx = PWONumber(args[0]);
    PWOTuple seq;
    o->setSlice(ndx, ndx + 1, seq);
    Py_INCREF(Py_None);
    return Py_None;
  } catch (...) {
    return 0;
  }
}

static char *addproperty__doc = 
  "addproperty(property) -- add temp column to view (use getas() for persistent columns)";

static PyObject *PyView_addproperty(PyView *o, PyObject *_args) {
  try {
    PWOSequence args(_args);
    PWOBase prop(args[0]);
    if (!PyProperty_Check((PyObject*)prop))
      Fail(PyExc_TypeError, "Not a Property object");
    PWONumber rslt(o->AddProperty(*(PyProperty*)(PyObject*)prop));
    return rslt.disOwn();
  } catch (...) {
    return 0;
  }
}

static char *select__doc = 
  "select(criteria) -- return virtual view with selected rows\n"
  "select(crit_lo, crit_hi) -- select rows in specified range (inclusive)\n"
  "  criteria may be keyword args or dictionary";

static PyObject *PyView_select(PyView *o, PyObject *_args, PyObject *kwargs) {
  try {
    c4_Row temp;
    PWOSequence args(_args);
    if (args.len() == 0) {
      o->makeRow(temp, kwargs, false);
      return new PyView(o->Select(temp), o, o->computeState(NOTIFIABLE));
    }
    if (args.len() == 1) {
      o->makeRow(temp, args[0], false);
      return new PyView(o->Select(temp), o, o->computeState(NOTIFIABLE));
    }

    if (PyObject_Length(args[0]) > 0)
      o->makeRow(temp, args[0], false);

    c4_Row temp2; // force an error if neither boundary has useful values
    if (temp.Container().NumProperties() == 0 || PyObject_Length(args[1]) > 0)
      o->makeRow(temp2, args[1], false);

    return new PyView(o->SelectRange(temp, temp2), o, o->computeState
      (NOTIFIABLE));
  } catch (...) {
    return 0;
  }
}

static char *sort__doc = 
  "sort() -- return virtual sorted view (native key order)\n"
  "sort(property...) -- sort on the specified properties";

static PyObject *PyView_sort(PyView *o, PyObject *_args) {
  try {
    PWOSequence args(_args);
    if (args.len()) {
      PyView crit;
      crit.addProperties(args);
      return new PyView(o->SortOn(crit), o, o->computeState(FINALNOTIFIABLE));
    }
    return new PyView(o->Sort(), o, o->computeState(FINALNOTIFIABLE));
  } catch (...) {
    return 0;
  }
}

static char *sortrev__doc = 
  "sortrev(props,propsdown) -- return sorted view, with optional reversed order\n"" arguments are lists of properties";

static PyObject *PyView_sortrev(PyView *o, PyObject *_args) {
  try {
    PWOSequence args(_args);

    PWOSequence all(args[0]);
    PyView propsAll;
    propsAll.addProperties(all);

    PWOSequence down(args[1]);
    PyView propsDown;
    propsDown.addProperties(down);

    return new PyView(o->SortOnReverse(propsAll, propsDown), 0, o->computeState
      (FINALNOTIFIABLE));
  } catch (...) {
    return 0;
  }
}

static char *project__doc = 
  "project(property...) -- returns virtual view with only the named columns";

static PyObject *PyView_project(PyView *o, PyObject *_args) {
  try {
    PWOSequence args(_args);
    PyView crit;
    crit.addProperties(args);
    return new PyView(o->Project(crit), 0, o->computeState(NOTIFIABLE));
  } catch (...) {
    return 0;
  }
}

static char *flatten__doc = 
  "flatten(subview_property, outer) -- produces 'flat' view from nested view\n"
  " outer defaults to 0";

static PyObject *PyView_flatten(PyView *o, PyObject *_args, PyObject *_kwargs) {
  try {
    PWOSequence args(_args);
    PWOMapping kwargs;
    if (_kwargs)
      kwargs = PWOBase(_kwargs);
    if (!PyProperty_Check((PyObject*)args[0]))
      Fail(PyExc_TypeError, 
        "First arg must be a property object identifying the subview");
    const c4_Property &subview = *(PyProperty*)(PyObject*)args[0];
    bool outer = false;
    if (args.len() > 1) {
      PWONumber flag(args[1]);
      if ((int)flag > 0)
        outer = true;
    }
    if (kwargs.hasKey("outer")) {
      if (int(PWONumber(kwargs["outer"])))
        outer = true;
    }
    return new PyView(o->JoinProp((const c4_ViewProp &)subview, outer), 0, o
      ->computeState(ROVIEWER));
  } catch (...) {
    return 0;
  }
}

static char *join__doc = 
  "join(otherview, property..., outer) -- join views on properties of same name and type\n"" outer defaults to 0";

static PyObject *PyView_join(PyView *o, PyObject *_args, PyObject *_kwargs) {
  PWOMapping kwargs;
  try {
    PWOSequence args(_args);
    if (_kwargs)
      kwargs = PWOBase(_kwargs);
    MustBeView(args[0]);
    PyView *other = (PyView*)(PyObject*)args[0];
    bool outer = false;
    int last = args.len();
    if (PyInt_Check((PyObject*)args[last - 1])) {
      PWONumber flag(args[--last]);
      if ((int)flag > 0)
        outer = true;
    }
    if (kwargs.hasKey("outer")) {
      if (int(PWONumber(kwargs["outer"])))
        outer = true;
    }
    PyView crit;
    crit.addProperties(args.getSlice(1, last));
    return new PyView(o->Join(crit,  *other, outer), 0, o->computeState
      (ROVIEWER));
  } catch (...) {
    return 0;
  }
}

static char *groupby__doc = 
  "groupby(property..., 'subname') -- group by given properties, creating subviews";

static PyObject *PyView_groupby(PyView *o, PyObject *_args) {
  try {
    PWOSequence args(_args);
    int last = args.len();
    PWOString subname(args[--last]);
    PyView crit;
    crit.addProperties(args.getSlice(0, last));
    c4_ViewProp sub(subname);
    return new PyView(o->GroupBy(crit, sub), 0, o->computeState(ROVIEWER));
  } catch (...) {
    return 0;
  }
}

static char *counts__doc = 
  "counts(property..., 'name') -- group by given properties, adding a count property";

static PyObject *PyView_counts(PyView *o, PyObject *_args) {
  try {
    PWOSequence args(_args);
    int last = args.len();
    PWOString name(args[--last]);
    PyView crit;
    crit.addProperties(args.getSlice(0, last));
    c4_IntProp count(name);
    return new PyView(o->Counts(crit, count), 0, o->computeState(ROVIEWER));
  } catch (...) {
    return 0;
  }
}

static char *rename__doc = 
  "rename('oldname', 'newname') -- derive a view with one property renamed";

static PyObject *PyView_rename(PyView *o, PyObject *_args) {
  try {
    PWOSequence args(_args);

    PWOString oldName(args[0]);
    int n = o->FindPropIndexByName(oldName);
    if (n < 0)
      Fail(PyExc_TypeError, "Property not found in view");
    const c4_Property &oProp = o->NthProperty(n);

    PWOString newName(args[1]);
    c4_Property nProp(oProp.Type(), newName);

    return new PyView(o->Rename(oProp, nProp), 0, o->computeState(RWVIEWER));
  } catch (...) {
    return 0;
  }
}

static char *unique__doc = 
  "unique() -- returns a view without duplicate rows, i.e. a set";

static PyObject *PyView_unique(PyView *o, PyObject *_args) {
  try {
    return new PyView(o->Unique(), 0, o->computeState(ROVIEWER));
  } catch (...) {
    return 0;
  }
}

static char *product__doc = 
  "product(view2) -- produce the cartesian product of both views";

static PyObject *PyView_product(PyView *o, PyObject *_args) {
  try {
    PWOSequence args(_args);
    MustBeView(args[0]);
    return new PyView(o->Product(*(PyView*)(PyObject*)args[0]), 0, o
      ->computeState(ROVIEWER));
  } catch (...) {
    return 0;
  }
}

static char *union__doc = "union(view2) -- produce the set union of both views";

static PyObject *PyView_union(PyView *o, PyObject *_args) {
  try {
    PWOSequence args(_args);
    MustBeView(args[0]);
    return new PyView(o->Union(*(PyView*)(PyObject*)args[0]), 0, o
      ->computeState(ROVIEWER));
  } catch (...) {
    return 0;
  }
}

static char *intersect__doc = 
  "intersect(view2) -- produce the set intersection of both views";

static PyObject *PyView_intersect(PyView *o, PyObject *_args) {
  try {
    PWOSequence args(_args);
    MustBeView(args[0]);
    return new PyView(o->Intersect(*(PyView*)(PyObject*)args[0]), 0, o
      ->computeState(ROVIEWER));
  } catch (...) {
    return 0;
  }
}

static char *different__doc = 
  "different(view2) -- produce the set difference of both views (XOR)";

static PyObject *PyView_different(PyView *o, PyObject *_args) {
  try {
    PWOSequence args(_args);
    MustBeView(args[0]);
    return new PyView(o->Different(*(PyView*)(PyObject*)args[0]), 0, o
      ->computeState(ROVIEWER));
  } catch (...) {
    return 0;
  }
}

static char *minus__doc = "minus(view2) -- all rows in view, but not in view2";

static PyObject *PyView_minus(PyView *o, PyObject *_args) {
  try {
    PWOSequence args(_args);
    MustBeView(args[0]);
    return new PyView(o->Minus(*(PyView*)(PyObject*)args[0]), 0, o
      ->computeState(ROVIEWER));
  } catch (...) {
    return 0;
  }
}

static char *remapwith__doc = 
  "remapwith(view2) -- remap rows according to first (int) prop in view2";

static PyObject *PyView_remapwith(PyView *o, PyObject *_args) {
  try {
    PWOSequence args(_args);
    MustBeView(args[0]);
    return new PyView(o->RemapWith(*(PyView*)(PyObject*)args[0]), 0, o
      ->computeState(RWVIEWER));
  } catch (...) {
    return 0;
  }
}

static char *pair__doc = 
  "pair(view2) -- concatenate rows pairwise, side by side";

static PyObject *PyView_pair(PyView *o, PyObject *_args) {
  try {
    PWOSequence args(_args);
    MustBeView(args[0]);
    return new PyView(o->Pair(*(PyView*)(PyObject*)args[0]), 0, o->computeState
      (MVIEWER));
  } catch (...) {
    return 0;
  }
}

static char *hash__doc = 
  "hash(mapview,numkeys) -- create a hashed view mapping\n"
  " numkeys defaults to 1\n"
  " without args, creates a temporary hash on one key";

static PyObject *PyView_hash(PyView *o, PyObject *_args) {
  try {
    PWOSequence args(_args);

    c4_View map;
    if (args.len() > 0) {
      MustBeView(args[0]);
      map = *(PyView*)(PyObject*)args[0];
    }
    int numkeys = args.len() <= 1 ? 1 : (int)PWONumber(args[1]);
    return new PyView(o->Hash(map, numkeys), 0, o->computeState(MVIEWER));
  } catch (...) {
    return 0;
  }
}

static char *blocked__doc = 
  "blocked() -- create a blocked/balanced view mapping";

static PyObject *PyView_blocked(PyView *o, PyObject *_args) {
  try {
    return new PyView(o->Blocked(), 0, o->computeState(MVIEWER));
  } catch (...) {
    return 0;
  }
}

static char *ordered__doc = 
  "ordered(numkeys) -- create a order-maintaining view mapping\n"
  " numkeys defaults to 1";

static PyObject *PyView_ordered(PyView *o, PyObject *_args) {
  try {
    PWOSequence args(_args);
    int numkeys = args.len() <= 0 ? 1 : (int)PWONumber(args[0]);
    return new PyView(o->Ordered(numkeys), 0, o->computeState(MVIEWER));
  } catch (...) {
    return 0;
  }
}

static char *indexed__doc = 
  "indexed(map, property..., unique) -- create a mapped view which manages an index\n"" unique defaults to 0 (not unique)";

static PyObject *PyView_indexed(PyView *o, PyObject *_args) {
  try {
    PWOSequence args(_args);
    MustBeView(args[0]);
    PyView *other = (PyView*)(PyObject*)args[0];
    bool unique = false;
    int last = args.len();
    if (PyInt_Check((PyObject*)args[last - 1])) {
      PWONumber flag(args[--last]); //XXX kwargs?
      if ((int)flag > 0)
        unique = true;
    }
    PyView crit;
    crit.addProperties(args.getSlice(1, last));
    return new PyView(o->Indexed(crit,  *other, unique), 0, o->computeState
      (MVIEWER));
  } catch (...) {
    return 0;
  }
}

static char *find__doc = 
  "find(criteria, start) -- return index of row found, matching criteria\n"
  " criteria maybe keyword args, or a dictionary";

static PyObject *PyView_find(PyView *o, PyObject *_args, PyObject *_kwargs) {
  PWONumber start(0);
  PWOMapping crit;
  try {
    PWOSequence args(_args);
    if (_kwargs) {
      PWOMapping kwargs(_kwargs);
      if (kwargs.hasKey("start")) {
        start = kwargs["start"];
        kwargs.delItem("start");
      }
      crit = kwargs;
    }
    int numargs = args.len();
    for (int i = 0; i < numargs; ++i) {
      if (PyNumber_Check((PyObject*)args[i]))
        start = args[i];
      else
        crit = args[i];
    }
    c4_Row temp;
    o->makeRow(temp, crit, false);
    return PWONumber(o->Find(temp, start)).disOwn();
  } catch (...) {
    return 0;
  }
}

static char *search__doc = 
  "search(criteria) -- binary search (native view order), returns match or insert pos";

static PyObject *PyView_search(PyView *o, PyObject *_args, PyObject *kwargs) {
  try {
    PWOSequence args(_args);
    if (args.len() != 0)
      kwargs = args[0];
    c4_Row temp;
    o->makeRow(temp, kwargs, false);
    return PWONumber(o->Search(temp)).disOwn();
  } catch (...) {
    return 0;
  }
}

static char *locate__doc = 
  "locate(criteria) -- binary search, returns tuple with pos and count";

static PyObject *PyView_locate(PyView *o, PyObject *_args, PyObject *kwargs) {
  try {
    PWOSequence args(_args);
    if (args.len() != 0)
      kwargs = args[0];
    c4_Row temp;
    o->makeRow(temp, kwargs, false);
    int pos = 0;
    PWONumber n(o->Locate(temp, &pos));
    PWONumber r(pos);
    PWOTuple tmp(2);
    tmp.setItem(0, r);
    tmp.setItem(1, n);
    return tmp.disOwn();
  } catch (...) {
    return 0;
  }
}

static char *access__doc = 
  "access(memoprop, rownum, offset, length=0) -- get (partial) memo property contents";

static PyObject *PyView_access(PyView *o, PyObject *_args) {
  try {
    PWOSequence args(_args);
    if (!PyProperty_Check((PyObject*)args[0]))
      Fail(PyExc_TypeError, "First arg must be a property");

    c4_BytesProp &prop = *(c4_BytesProp*)(c4_Property*)(PyProperty*)(PyObject*)
      args[0];

    int index = PyInt_AsLong(args[1]);
    if (index < 0 || index >= o->GetSize())
      Fail(PyExc_IndexError, "Index out of range");

    c4_RowRef row = o->GetAt(index);

    long offset = PyInt_AsLong(args[2]);
    int length = args.len() == 3 ? 0 : PyInt_AsLong(args[3]);
    if (length <= 0) {
      length = prop(row).GetSize() - offset;
      if (length < 0)
        length = 0;
    }

    PyObject *buffer = PyString_FromStringAndSize(0, length);
    int o = 0;

    while (o < length) {
      c4_Bytes buf = prop(row).Access(offset + o, length - o);
      int n = buf.Size();
      if (n == 0)
        break;
      memcpy(PyString_AS_STRING(buffer) + o, buf.Contents(), n);
      o += n;
    }

    if (o < length)
      _PyString_Resize(&buffer, o);

    return buffer;
  } catch (...) {
    return 0;
  }
}

static char *modify__doc = 
  "modify(memoprop, rownum, string, offset, diff=0) -- store (partial) memo contents\n""diff removes (<0) or inserts (>0) bytes, and is adjusted to within sensible range";

static PyObject *PyView_modify(PyView *o, PyObject *_args) {
  try {
    PWOSequence args(_args);
    if (!PyProperty_Check((PyObject*)args[0]))
      Fail(PyExc_TypeError, "First arg must be a property");

    c4_BytesProp &prop = *(c4_BytesProp*)(c4_Property*)(PyProperty*)(PyObject*)
      args[0];

    int index = PWONumber(args[1]);
    if (index < 0 || index >= o->GetSize())
      Fail(PyExc_IndexError, "Index out of range");

    c4_RowRef row = o->GetAt(index);

    PWOString buffer(args[2]);
    c4_Bytes data((void*)(const char*)buffer, buffer.len());

    long offset = PWONumber(args[3]);
    int diff = args.len() == 4 ? 0 : (int)PWONumber(args[4]);

    if (!prop(row).Modify(data, offset, diff))
      Fail(PyExc_TypeError, "Failed to modify memo field");

    Py_INCREF(Py_None);
    return Py_None;
  } catch (...) {
    return 0;
  }
}

static char *itemsize__doc = 
  "itemsize(prop, rownum=0) -- return size of item (rownum only needed for S/B/M types)\n""with integer fields, a result of -1/-2/-4 means 1/2/4 bits per value, respectively";

static PyObject *PyView_itemsize(PyView *o, PyObject *_args) {
  try {
    PWOSequence args(_args);
    if (!PyProperty_Check((PyObject*)args[0]))
      Fail(PyExc_TypeError, "First arg must be a property");

    c4_BytesProp &prop = *(c4_BytesProp*)(c4_Property*)(PyProperty*)(PyObject*)
      args[0];
    int index = args.len() == 1 ? 0 : (int)PWONumber(args[1]);
    if (index < 0 || index >= o->GetSize())
      Fail(PyExc_IndexError, "Index out of range");

    return PWONumber(prop(o->GetAt(index)).GetSize()).disOwn();
  } catch (...) {
    return 0;
  }
}

static char *relocrows__doc = 
  "relocrows(from, count, dest, pos) -- relocate rows within views of same storage\n""from is source offset, count is number of rows, pos is destination offset\n""both views must have a compatible structure (field names may differ)";

static PyObject *PyView_relocrows(PyView *o, PyObject *_args) {
  try {
    PWOSequence args(_args);
    if (!PyView_Check((PyObject*)args[2]))
      Fail(PyExc_TypeError, "Third arg must be a view object");

    PyView &dest = *(PyView*)(PyObject*)args[2];

    int from = PWONumber(args[0]);
    if (from < 0)
      from += o->GetSize();
    int count = PWONumber(args[1]);
    if (from < 0 || count < 0 || from + count > o->GetSize())
      Fail(PyExc_IndexError, "Source index out of range");

    int pos = PWONumber(args[3]);
    if (pos < 0)
      pos += dest.GetSize();
    if (pos < 0 || pos > dest.GetSize())
      Fail(PyExc_IndexError, "Destination index out of range");

    if (!o->IsCompatibleWith(dest))
      Fail(PyExc_TypeError, "Views are not compatible");

    o->RelocateRows(from, count, dest, pos);

    Py_INCREF(Py_None);
    return Py_None;
  } catch (...) {
    return 0;
  }
}

static char *map__doc = 
  "map(func, subset=None) -- apply func to each row of view,\n"
  "or (if subset specified) to each row in view that is also in subset.\n"
  "Returns None: view is mutated\n"
  "func must have the signature func(row), and may mutate row.\n"
  "subset must be a subset of view: eg, customers.map(func, customers.select(....)).\n";

static PyObject *PyView_map(PyView *o, PyObject *_args) {
  try {
    PWOSequence args(_args);
    PWOCallable func(args[0]);
    if (args.len() > 1) {
      if (!PyView_Check((PyObject*)args[1]))
        Fail(PyExc_TypeError, "Second arg must be a view object");

      PyView &subset = *(PyView*)(PyObject*)args[1];

      o->map(func, subset);
    } else
      o->map(func);

    Py_INCREF(Py_None);
    return Py_None;
  } catch (...) {
    return 0;
  }
}

static char *filter__doc = 
  "filter(func) -- return a new view containing the indices of those rows satisfying func.\n""  func must have the signature func(row), and should return a false value to omit row.";

static PyObject *PyView_filter(PyView *o, PyObject *_args) {
  try {
    PWOSequence args(_args);
    PWOCallable func(args[0]);
    return o->filter(func);
  } catch (...) {
    return 0;
  }
}

static char *reduce__doc = 
  "reduce(func, start=0) -- return the result of applying func(row, lastresult) to\n""each row in view.\n";

static PyObject *PyView_reduce(PyView *o, PyObject *_args) {
  try {
    PWOSequence args(_args);
    PWOCallable func(args[0]);
    PWONumber start(0);
    if (args.len() > 1)
      start = args[1];
    return o->reduce(func, start);
  } catch (...) {
    return 0;
  }
}

static char *remove__doc = 
  "remove(indices) -- remove all rows whose indices are in subset from view\n"
  "Not the same as minus, because unique is not required, and view is not reordered.\n";

static PyObject *PyView_remove(PyView *o, PyObject *_args) {
  try {
    PWOSequence args(_args);
    MustBeView(args[0]);

    PyView &subset = *(PyView*)(PyObject*)args[0];
    o->remove(subset);
    Py_INCREF(Py_None);
    return Py_None;
  } catch (...) {
    return 0;
  }
}

static char *indices__doc = 
  "indices(subset) -- returns a view containing the indices in view of the rows of subset";

static PyObject *PyView_indices(PyView *o, PyObject *_args) {
  try {
    PWOSequence args(_args);
    MustBeView(args[0]);

    PyView &subset = *(PyView*)(PyObject*)args[0];
    return o->indices(subset);
  } catch (...) {
    return 0;
  }
}

static char *copy__doc = "copy() -- returns a copy of the view\n";

static PyObject *PyView_copy(PyView *o, PyObject *_args) {
  try {
    return new PyView(o->Duplicate());
  } catch (...) {
    return 0;
  }
}

static PyMethodDef ViewMethods[] =  {
   {
    "setsize", (PyCFunction)PyView_setsize, METH_VARARGS, setsize__doc
  }
  ,  {
    "insert", (PyCFunction)PyView_insert, METH_VARARGS | METH_KEYWORDS,
      insert__doc
  }
  ,  {
    "append", (PyCFunction)PyView_append, METH_VARARGS | METH_KEYWORDS,
      append__doc
  }
  ,  {
    "delete", (PyCFunction)PyView_delete, METH_VARARGS, delete__doc
  }
  ,  {
    "structure", (PyCFunction)PyView_structure, METH_VARARGS, structure__doc
  }
  ,  {
    "select", (PyCFunction)PyView_select, METH_VARARGS | METH_KEYWORDS,
      select__doc
  }
  ,  {
    "addproperty", (PyCFunction)PyView_addproperty, METH_VARARGS,
      addproperty__doc
  }
  ,  {
    "sort", (PyCFunction)PyView_sort, METH_VARARGS, sort__doc
  }
  ,  {
    "sortrev", (PyCFunction)PyView_sortrev, METH_VARARGS, sortrev__doc
  }
  ,  {
    "project", (PyCFunction)PyView_project, METH_VARARGS, project__doc
  }
  ,  {
    "flatten", (PyCFunction)PyView_flatten, METH_VARARGS | METH_KEYWORDS,
      flatten__doc
  }
  ,  {
    "join", (PyCFunction)PyView_join, METH_VARARGS | METH_KEYWORDS, join__doc
  }
  ,  {
    "groupby", (PyCFunction)PyView_groupby, METH_VARARGS, groupby__doc
  }
  ,  {
    "counts", (PyCFunction)PyView_counts, METH_VARARGS, counts__doc
  }
  ,  {
    "product", (PyCFunction)PyView_product, METH_VARARGS, product__doc
  }
  ,  {
    "union", (PyCFunction)PyView_union, METH_VARARGS, union__doc
  }
  ,  {
    "intersect", (PyCFunction)PyView_intersect, METH_VARARGS, intersect__doc
  }
  ,  {
    "different", (PyCFunction)PyView_different, METH_VARARGS, different__doc
  }
  ,  {
    "minus", (PyCFunction)PyView_minus, METH_VARARGS, minus__doc
  }
  ,  {
    "remapwith", (PyCFunction)PyView_remapwith, METH_VARARGS, remapwith__doc
  }
  ,  {
    "pair", (PyCFunction)PyView_pair, METH_VARARGS, pair__doc
  }
  ,  {
    "rename", (PyCFunction)PyView_rename, METH_VARARGS, rename__doc
  }
  ,  {
    "unique", (PyCFunction)PyView_unique, METH_VARARGS, unique__doc
  }
  ,  {
    "hash", (PyCFunction)PyView_hash, METH_VARARGS, hash__doc
  }
  ,  {
    "blocked", (PyCFunction)PyView_blocked, METH_VARARGS, blocked__doc
  }
  ,  {
    "ordered", (PyCFunction)PyView_ordered, METH_VARARGS, ordered__doc
  }
  ,  {
    "indexed", (PyCFunction)PyView_indexed, METH_VARARGS, indexed__doc
  }
  ,  {
    "find", (PyCFunction)PyView_find, METH_VARARGS | METH_KEYWORDS, find__doc
  }
  ,  {
    "search", (PyCFunction)PyView_search, METH_VARARGS | METH_KEYWORDS,
      search__doc
  }
  ,  {
    "locate", (PyCFunction)PyView_locate, METH_VARARGS | METH_KEYWORDS,
      locate__doc
  }
  ,  {
    "access", (PyCFunction)PyView_access, METH_VARARGS, access__doc
  }
  ,  {
    "modify", (PyCFunction)PyView_modify, METH_VARARGS, modify__doc
  }
  ,  {
    "itemsize", (PyCFunction)PyView_itemsize, METH_VARARGS, itemsize__doc
  }
  , 
  // {"relocrows", (PyCFunction)PyView_relocrows, METH_VARARGS, relocrows__doc},
   {
    "map", (PyCFunction)PyView_map, METH_VARARGS, map__doc
  }
  ,  {
    "filter", (PyCFunction)PyView_filter, METH_VARARGS, filter__doc
  }
  ,  {
    "reduce", (PyCFunction)PyView_reduce, METH_VARARGS, reduce__doc
  }
  ,  {
    "remove", (PyCFunction)PyView_remove, METH_VARARGS, remove__doc
  }
  ,  {
    "indices", (PyCFunction)PyView_indices, METH_VARARGS, indices__doc
  }
  ,  {
    "copy", (PyCFunction)PyView_copy, METH_VARARGS, copy__doc
  }
  ,  {
    "properties", (PyCFunction)PyView_properties, METH_VARARGS, properties__doc
  }
  ,  {
    0, 0, 0, 0
  }
};
static PyMethodDef ViewerMethods[] =  {
   {
    "structure", (PyCFunction)PyView_structure, METH_VARARGS, structure__doc
  }
  ,  {
    "select", (PyCFunction)PyView_select, METH_VARARGS | METH_KEYWORDS,
      select__doc
  }
  ,  {
    "addproperty", (PyCFunction)PyView_addproperty, METH_VARARGS,
      addproperty__doc
  }
  ,  {
    "sort", (PyCFunction)PyView_sort, METH_VARARGS, sort__doc
  }
  ,  {
    "sortrev", (PyCFunction)PyView_sortrev, METH_VARARGS, sortrev__doc
  }
  ,  {
    "project", (PyCFunction)PyView_project, METH_VARARGS, project__doc
  }
  ,  {
    "flatten", (PyCFunction)PyView_flatten, METH_VARARGS | METH_KEYWORDS,
      flatten__doc
  }
  ,  {
    "join", (PyCFunction)PyView_join, METH_VARARGS | METH_KEYWORDS, join__doc
  }
  ,  {
    "groupby", (PyCFunction)PyView_groupby, METH_VARARGS, groupby__doc
  }
  ,  {
    "counts", (PyCFunction)PyView_counts, METH_VARARGS, counts__doc
  }
  ,  {
    "product", (PyCFunction)PyView_product, METH_VARARGS, product__doc
  }
  ,  {
    "union", (PyCFunction)PyView_union, METH_VARARGS, union__doc
  }
  ,  {
    "intersect", (PyCFunction)PyView_intersect, METH_VARARGS, intersect__doc
  }
  ,  {
    "different", (PyCFunction)PyView_different, METH_VARARGS, different__doc
  }
  ,  {
    "minus", (PyCFunction)PyView_minus, METH_VARARGS, minus__doc
  }
  ,  {
    "remapwith", (PyCFunction)PyView_remapwith, METH_VARARGS, remapwith__doc
  }
  ,  {
    "pair", (PyCFunction)PyView_pair, METH_VARARGS, pair__doc
  }
  ,  {
    "rename", (PyCFunction)PyView_rename, METH_VARARGS, rename__doc
  }
  ,  {
    "unique", (PyCFunction)PyView_unique, METH_VARARGS, unique__doc
  }
  ,  {
    "hash", (PyCFunction)PyView_hash, METH_VARARGS, hash__doc
  }
  ,  {
    "blocked", (PyCFunction)PyView_blocked, METH_VARARGS, blocked__doc
  }
  ,  {
    "ordered", (PyCFunction)PyView_ordered, METH_VARARGS, ordered__doc
  }
  ,  {
    "indexed", (PyCFunction)PyView_indexed, METH_VARARGS, indexed__doc
  }
  ,  {
    "find", (PyCFunction)PyView_find, METH_VARARGS | METH_KEYWORDS, find__doc
  }
  ,  {
    "search", (PyCFunction)PyView_search, METH_VARARGS | METH_KEYWORDS,
      search__doc
  }
  ,  {
    "locate", (PyCFunction)PyView_locate, METH_VARARGS | METH_KEYWORDS,
      locate__doc
  }
  ,  {
    "access", (PyCFunction)PyView_access, METH_VARARGS, access__doc
  }
  ,  {
    "modify", (PyCFunction)PyView_modify, METH_VARARGS, modify__doc
  }
  ,  {
    "itemsize", (PyCFunction)PyView_itemsize, METH_VARARGS, itemsize__doc
  }
  , 
  //{"map", (PyCFunction)PyView_map, METH_VARARGS, map__doc},
   {
    "filter", (PyCFunction)PyView_filter, METH_VARARGS, filter__doc
  }
  ,  {
    "reduce", (PyCFunction)PyView_reduce, METH_VARARGS, reduce__doc
  }
  ,  {
    "indices", (PyCFunction)PyView_indices, METH_VARARGS, indices__doc
  }
  ,  {
    "copy", (PyCFunction)PyView_copy, METH_VARARGS, copy__doc
  }
  ,  {
    "properties", (PyCFunction)PyView_properties, METH_VARARGS, properties__doc
  }
  ,  {
    0, 0, 0, 0
  }
};

/*
Duplicate(deep=0)  (__copy__ and __deepcopy__ as methods, too)
Clone()
 */
static Py_ssize_t PyView_length(PyObject *_o) {
  PyView *o = (PyView*)_o;

  try {
    return o->GetSize();
  } catch (...) {
    return  - 1;
  }
}

static PyObject *PyView_concat(PyObject *_o, PyObject *_other) {
  PyView *o = (PyView*)_o;
  PyView *other = (PyView*)_other;

  try {
    if (!PyGenericView_Check(other))
      Fail(PyExc_TypeError, "Not a PyView(er)");
    return new PyView(o->Concat(*other), 0, o->computeState(RWVIEWER));
  } catch (...) {
    return 0;
  }
}

static PyObject *PyView_repeat(PyObject *_o, Py_ssize_t n) {
  PyView *o = (PyView*)_o;

  try {
    PyView *tmp = new PyView(*o, 0, o->computeState(RWVIEWER));
    while (--n > 0) {
      //!! a huge stack of views?
      PyView *tmp1 = new PyView(tmp->Concat(*o), 0, o->computeState(RWVIEWER));
      delete tmp;
      tmp = tmp1;
    }
    return tmp;
  } catch (...) {
    return 0;
  }
}

static PyObject *PyView_getitem(PyObject *_o, Py_ssize_t n) {
  PyView *o = (PyView*)_o;

  try {
    PyObject *rslt = o->getItem(n);
    if (rslt == 0)
      PyErr_SetString(PyExc_IndexError, "row index out of range");
    return rslt;
  } catch (...) {
    return 0;
  }
}

static PyObject *PyView_getslice(PyObject *_o, Py_ssize_t s, Py_ssize_t e) {
  PyView *o = (PyView*)_o;

  try {
    return o->getSlice(s, e);
  } catch (...) {
    return 0;
  }
}

static int PyView_setitem(PyObject *_o, Py_ssize_t n, PyObject *v) {
  PyView *o = (PyView*)_o;

  try {
    if (n < 0)
      n += o->GetSize();
    if (n >= o->GetSize() || n < 0)
      Fail(PyExc_IndexError, "Index out of range");
    if (v == 0) {
      o->RemoveAt(n);
      return 0;
    }

    return o->setItem(n, v);
  } catch (...) {
    return  - 1;
  }
}

static int PyView_setslice(PyObject *_o, Py_ssize_t s, Py_ssize_t e, PyObject
  *v) {
  PyView *o = (PyView*)_o;

  try {
    if (v == 0) {
      PWOTuple seq;
      return o->setSlice(s, e, seq);
    }

    PWOSequence seq(v);
    return o->setSlice(s, e, seq);
  } catch (...) {
    return  - 1;
  }
}

static PySequenceMethods ViewAsSeq =  {
  PyView_length,  //sq_length
  PyView_concat,  //sq_concat
  PyView_repeat,  //sq_repeat
  PyView_getitem,  //sq_item
  PyView_getslice,  //sq_slice
  PyView_setitem,  //sq_ass_item
  PyView_setslice,  //sq_ass_slice
};

static PySequenceMethods ViewerAsSeq =  {
  PyView_length,  //sq_length
  PyView_concat,  //sq_concat
  PyView_repeat,  //sq_repeat
  PyView_getitem,  //sq_item
  PyView_getslice,  //sq_slice
  0,  //sq_ass_item
  0,  //sq_ass_slice
};

static void PyView_dealloc(PyView *o) {
  //o->~PyView();
  delete o;
}

static int PyView_print(PyView *o, FILE *f, int) {
  fprintf(f, "<PyView object at %p>", (void*)o);
  return 0;
}

static int PyViewer_print(PyView *o, FILE *f, int) {
  fprintf(f, "<PyViewer object at %p>", (void*)o);
  return 0;
}

static int PyROViewer_print(PyView *o, FILE *f, int) {
  fprintf(f, "<PyROViewer object at %p>", (void*)o);
  return 0;
}

static PyObject *PyView_getattr(PyView *o, char *nm) {
  PyObject *rslt;
  try {
    rslt = Py_FindMethod(ViewMethods, o, nm);
    if (rslt)
      return rslt;
    PyErr_Clear();
    int ndx = o->FindPropIndexByName(nm);
    if (ndx >  - 1)
      return new PyProperty(o->NthProperty(ndx));
    Fail(PyExc_AttributeError, nm);
  } catch (...) {
    return 0;
  }
  return 0;
}

static PyObject *PyViewer_getattr(PyView *o, char *nm) {
  PyObject *rslt;
  try {
    rslt = Py_FindMethod(ViewerMethods, o, nm);
    if (rslt)
      return rslt;
    PyErr_Clear();
    int ndx = o->FindPropIndexByName(nm);
    if (ndx >  - 1)
      return new PyProperty(o->NthProperty(ndx));
    Fail(PyExc_AttributeError, nm);
  } catch (...) {
    return 0;
  }
  return 0;
}


PyTypeObject PyViewtype =  {
  PyObject_HEAD_INIT(&PyType_Type)0, "PyView", sizeof(PyView), 0, (destructor)
    PyView_dealloc,  /*tp_dealloc*/
  (printfunc)PyView_print,  /*tp_print*/
  (getattrfunc)PyView_getattr,  /*tp_getattr*/
  0,  /*tp_setattr*/
  (cmpfunc)0,  /*tp_compare*/
  (reprfunc)0,  /*tp_repr*/
  0,  /*tp_as_number*/
   &ViewAsSeq,  /*tp_as_sequence*/
  0,  /*tp_as_mapping*/
};
PyTypeObject PyViewertype =  {
  PyObject_HEAD_INIT(&PyType_Type)0, "PyViewer", sizeof(PyView), 0, (destructor)
    PyView_dealloc,  /*tp_dealloc*/
  (printfunc)PyViewer_print,  /*tp_print*/
  (getattrfunc)PyViewer_getattr,  /*tp_getattr*/
  0,  /*tp_setattr*/
  (cmpfunc)0,  /*tp_compare*/
  (reprfunc)0,  /*tp_repr*/
  0,  /*tp_as_number*/
   &ViewerAsSeq,  /*tp_as_sequence*/
  0,  /*tp_as_mapping*/
};
PyTypeObject PyROViewertype =  {
  PyObject_HEAD_INIT(&PyType_Type)0, "PyROViewer", sizeof(PyView), 0, 
    (destructor)PyView_dealloc,  /*tp_dealloc*/
  (printfunc)PyROViewer_print,  /*tp_print*/
  (getattrfunc)PyViewer_getattr,  /*tp_getattr*/
  0,  /*tp_setattr*/
  (cmpfunc)0,  /*tp_compare*/
  (reprfunc)0,  /*tp_repr*/
  0,  /*tp_as_number*/
   &ViewerAsSeq,  /*tp_as_sequence*/
  0,  /*tp_as_mapping*/
};
int PyView::computeState(int targettype) {
  int newtype = _state | targettype;
  if (newtype > FINALNOTIFIABLE)
    newtype = ROVIEWER;
  if (_state == FINALNOTIFIABLE)
    newtype = ROVIEWER;
  return newtype;
}

PyTypeObject *getTypeObject(int type) {
  switch (type) {
    case BASE:
    case MVIEWER:
      return  &PyViewtype;
      break;
    case NOTIFIABLE:
    case RWVIEWER:
    case FINALNOTIFIABLE:
      return  &PyViewertype;
    case ROVIEWER:
      return  &PyROViewertype;
  }
  return  &PyViewtype;
}


PyObject *PyView_new(PyObject *o, PyObject *_args) {
  return new PyView;
}

PyView::PyView(): PyHead(PyViewtype), _base(0), _state(BASE){}

PyView::PyView(const c4_View &o, PyView *owner, int state): PyHead(PyViewtype),
  c4_View(o), _base(owner), _state(state) {
  ob_type = getTypeObject(_state);
  if (owner && owner->_base)
    _base = owner->_base;
}

/* For dicts, use the Python names so MK's case insensitivity works */
void PyView::makeRowFromDict(c4_Row &tmp, PyObject *o, bool useDefaults) {
  PWOMapping dict(o);
  PWOList keys = dict.keys();
  for (int i = 0; i < dict.len(); ++i) {
    PWOString key = keys[i];
    int ndx = FindPropIndexByName(key);
    if (ndx >  - 1) {
      const c4_Property &prop = NthProperty(ndx);
      PyRowRef::setFromPython(tmp, prop, dict[(const char*)key]);
    }
  }
}

void PyView::makeRow(c4_Row &tmp, PyObject *o, bool useDefaults) {
  /* can't just check if mapping type; strings are mappings in Python 2.3
  (but not in 2.2 or earlier) */
  if (o && PyDict_Check(o))
    makeRowFromDict(tmp, o, useDefaults);
  else {
    enum {
      instance, sequence, none
    } pyobject_type = none;
    int n = NumProperties();

    if (!o) {
      pyobject_type = none;
    } else if (PyInstance_Check(o)) {
      /* instances of new-style classes (Python 2.2+) do not return true */
      pyobject_type = instance;
    } else if (PySequence_Check(o)) {
      int seq_length = PyObject_Length(o);
      if (seq_length > n) {
        PyErr_Format(PyExc_IndexError, 
          "Sequence has %d elements; view has %d properties", seq_length, n);
        throw PWDPyException;
      }
      n = seq_length;
      pyobject_type = sequence;
    } else {
      /* new-style class, not a number */
      if (PyObject_HasAttrString(o, "__class__") && !PyNumber_Check(o)) {
        pyobject_type = instance;
      } else {
        Fail(PyExc_TypeError, 
          "Argument is not an instance, sequence or dictionary: cannot be coerced to row");
      }
    }

    for (int i = 0; i < n; i++) {
      const c4_Property &prop = NthProperty(i);
      PyObject *attr = 0;
      if (pyobject_type == instance) {
        attr = PyObject_GetAttrString(o, (char*)prop.Name());
        if (attr == 0 && i == 0 && NumProperties() == 1) {
          PyErr_Clear();
          attr = o;
          Py_XINCREF(attr);
        }
      } else if (pyobject_type == sequence) {
        attr = PySequence_GetItem(o, i);
      }
      if (attr) {
        try {
          PyRowRef::setFromPython(tmp, prop, attr);
        } catch (...) {
          Py_DECREF(attr);
          throw;
        }
        Py_DECREF(attr);
      } else {
        PyErr_Clear();
        if (useDefaults)
          PyRowRef::setDefault(tmp, prop);
      }
    }
  }
  if (!useDefaults)
    if (tmp.Container().NumProperties() == 0)
      Fail(PyExc_ValueError, "Object has no usable attributes");
}

void PyView::insertAt(int i, PyObject *o) {
  if (PyGenericView_Check(o))
    InsertAt(i, *(PyView*)o);
  else {
    c4_Row temp;
    makeRow(temp, o);
    InsertAt(i, temp);
  }
}

PyObject *PyView::structure() {
  int n = NumProperties();
  //  PyObject* list=PyList_New(n);
  //  for (int i = 0; i < n; i++)
  //    PyList_SET_ITEM(list, i, new PyProperty(NthProperty(i)));
  //  return list;
  PWOList rslt(n);
  for (int i = 0; i < n; i++) {
    PyProperty *prop = new PyProperty(NthProperty(i));
    rslt.setItem(i, prop);
  }
  return rslt.disOwn();
}


PyObject *PyView::properties() {
  int n = NumProperties();
  PWOMapping rslt;
  for (int i = 0; i < n; i++) {
    PyProperty *item = new PyProperty(NthProperty(i));
    rslt.setItem(item->Name(), item);
    Py_DECREF(item);
  }
  return rslt.disOwn();
}


PyView *PyView::getSlice(int s, int e) {
  int sz = GetSize();
  if (s < 0)
    s += sz;
  if (e < 0)
    e += sz;
  if (e > sz)
    e = sz;
  if (s >= 0 && s < sz)
    if (e > s && e <= sz)
      return new PyView(Slice(s, e), 0, computeState(RWVIEWER));
  return new PyView(Clone());
}

int PyView::setSlice(int s, int e, const PWOSequence &lst) {
  int sz = GetSize();
  if (s < 0)
    s += sz;
  if (e < 0)
    e += sz;
  if (e > sz)
    e = sz;
  int i = 0;
  for (; i < lst.len() && s < e; i++, s++)
    setItem(s, lst[i]);
  for (; i < lst.len(); i++, s++) {
    if (_base)
      Fail(PyExc_RuntimeError, "Can't insert in this view");
    insertAt(s, lst[i]);
  }
  if (s < e)
    if (_base)
  while (s < e) {
    int ndx = _base->GetIndexOf(GetAt(s));
    _base->RemoveAt(ndx, 1);
    --e;
  } else
    RemoveAt(s, e-s);
  return 0;
}

PyRowRef *PyView::getItem(int i) {
  if (i < 0)
    i += GetSize();
  if (i >= GetSize() || i < 0)
    return 0;
  if (_base && !(_state &IMMUTABLEROWS)) {
    c4_RowRef derived = GetAt(i);
    int ndx = _base->GetIndexOf(derived);
    if (ndx >= 0)
      return new PyRowRef(_base->GetAt(ndx), _state &IMMUTABLEROWS);
  }
  return new PyRowRef(GetAt(i), _state &IMMUTABLEROWS);
}

int PyView::setItem(int i, PyObject *v) {
  if (PyGenericRowRef_Check(v))
    return setItemRow(i, *(PyRowRef*)v);
  c4_Row temp;
  makeRow(temp, v, false);
  return setItemRow(i, temp);
}

void PyView::addProperties(const PWOSequence &lst) {
  for (int i = 0; i < lst.len(); i++) {
    if (PyProperty_Check((PyObject*)lst[i])) {
      AddProperty(*(PyProperty*)(PyObject*)lst[i]);
    }
  }
}

void PyView::map(const PWOCallable &func) {
  PWOTuple tmp(1);
  for (int i = 0; i < GetSize(); ++i) {
    PyRowRef *row = new PyRowRef(GetAt(i));
    PWOBase r2(row);
    tmp.setItem(0, r2);
    func.call(tmp);
    Py_DECREF(row);
  }
}

void PyView::map(const PWOCallable &func, const PyView &subset) {
  int sz = subset.GetSize();
  PWOTuple tmp(1);
  for (int i = 0; i < sz; ++i) {
    PyRowRef *row = new PyRowRef(GetAt(GetIndexOf(subset.GetAt(i))));
    PWOBase r2(row);
    tmp.setItem(0, r2);
    func.call(tmp);
    Py_DECREF(row);
  }
}

static c4_IntProp _index("index");

PyView *PyView::indices(const PyView &subset) {
  c4_View tmp(_index);
  tmp.SetSize(subset.GetSize());
  c4_Row row;
  for (int i = 0; i < subset.GetSize(); ++i) {
    _index(row) = GetIndexOf(subset.GetAt(i));
    tmp.SetAt(i, row);
  }
  return new PyView(tmp);
}

void PyView::remove(const PyView &indices) {
  c4_View tmp = indices.Sort();
  for (int i = indices.GetSize() - 1; i >= 0; --i)
    RemoveAt(_index(tmp.GetAt(i)));
}

PyView *PyView::filter(const PWOCallable &func) {
  c4_View indices(_index);
  c4_Row ndx;
  PWOTuple tmp(1);
  for (int i = 0; i < GetSize(); ++i) {
    PyRowRef *row = new PyRowRef(GetAt(i));
    PWOBase r2(row);
    tmp.setItem(0, r2);
    PWOBase rslt(func.call(tmp));
    if (rslt.isTrue()) {
      _index(ndx) = i;
      indices.Add(ndx);
    }
    Py_DECREF(row);
  }
  return new PyView(indices);
}

PyObject *PyView::reduce(const PWOCallable &func, PWONumber &start) {
  PWONumber accum = start;
  PWOTuple tmp(2);
  for (int i = 0; i < GetSize(); ++i) {
    PyRowRef *row = new PyRowRef(GetAt(i));
    PWOBase r2(row);
    tmp.setItem(0, r2);
    tmp.setItem(1, accum);
    PWOBase rslt(func.call(tmp));
    accum = rslt;
    Py_DECREF(row);
  }
  return accum;
}
