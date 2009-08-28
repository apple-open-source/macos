// PyStorage.cpp --
// $Id: PyStorage.cpp 1669 2007-06-16 00:23:25Z jcw $
// This is part of MetaKit, the homepage is http://www.equi4.com/metakit.html
// Copyright (C) 1999-2004 Gordon McMillan and Jean-Claude Wippler.
//
//  Storage class implementation and main entry point

#include <Python.h>
#include "PyStorage.h"
#include <PWOSequence.h>
#include <PWONumber.h>
#include "PyView.h"
#include "PyProperty.h"
#include "PyRowRef.h"
#include "mk4str.h"
#include "mk4io.h"

#if !defined _WIN32
#define __declspec(x)
#endif 

static char mk4py_module_documentation[] = 
  "This is the Python interface of the embeddable MetaKit database library.\n"
  "Example of use:\n""\n""  import Mk4py\n""  mk = Mk4py\n""\n"
  "  s = mk.storage('demo.dat', 1)\n"
  "  v = s.getas('people[first:S,last:S,shoesize:I]')\n""\n"
  "  v.append(first='John',last='Lennon',shoesize=44)\n"
  "  v.append(first='Flash',last='Gordon',shoesize=42)\n""  s.commit()\n""\n"
  "  def dump(v):\n""    print len(v)\n"
  "    for r in v: print r.first, r.last, r.shoesize\n""\n"
  "  v2 = v.sort(v.last)\n""  dump(v2)\n""  v[0].last = 'Doe'\n""  dump(v2)\n"
  "  v2 = v.select(last='Doe')\n""  dump(v2)\n""  del s\n""\n"
  "See the website at http://www.equi4.com/metakit.html for full details.\n";

///////////////////////////////////////////////////////////////////////////////

class c4_PyStream: public c4_Stream {
    PyObject *_stream;

  public:
    c4_PyStream(PyObject *stream_);

    virtual int Read(void *buffer_, int length_);
    virtual bool Write(const void *buffer_, int length_);
};

c4_PyStream::c4_PyStream(PyObject *stream_): _stream(stream_){}

int c4_PyStream::Read(void *buffer_, int length_) {
  PyObject *o = PyObject_CallMethod(_stream, "read", "i", length_);
  int n = o != 0 ? PyString_Size(o): 0;
  if (n > 0)
    memcpy(buffer_, PyString_AsString(o), n);
  return n;
}

bool c4_PyStream::Write(const void *buffer_, int length_) {
  PyObject_CallMethod(_stream, "write", "s#", buffer_, length_);
  return true; //!! how do we detect write errors here?
}

///////////////////////////////////////////////////////////////////////////////
// A "storage in a storage" strategy class for MetaKit

class SiasStrategy: public c4_Strategy {
    c4_Storage &_storage;
    c4_View _view;
    c4_BytesProp _memo;
    int _row;

  public:
    SiasStrategy(c4_Storage &storage_, const c4_View &view_, const c4_BytesProp
      &memo_, int row_): _storage(storage_), _view(view_), _memo(memo_), _row
      (row_) {
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
    }

    virtual int DataRead(t4_i32 pos_, void *buffer_, int length_) {
        int i = 0;

        while (i < length_) {
            c4_Bytes data = _memo(_view[_row]).Access(pos_ + i, length_ - i);
            int n = data.Size();
            if (n <= 0)
              break;
            memcpy((char*)buffer_ + i, data.Contents(), n);
            i += n;
        }

        return i;
    }

    virtual void DataWrite(t4_i32 pos_, const void *buffer_, int length_) {
        c4_Bytes data(buffer_, length_);
        if (!_memo(_view[_row]).Modify(data, pos_))
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

static char *autocommit__doc = 
  "autocommit() -- turn on autocommit (i.e. commit when storage object is deleted)";

static PyObject *PyStorage_Autocommit(PyStorage *o, PyObject *_args) {
  try {
    o->AutoCommit();
    Py_INCREF(Py_None);
    return Py_None;
  } catch (...) {
    return 0;
  }
}

static char *contents__doc = 
  "contents() -- return view with one row, representing entire storage (internal use)";

static PyObject *PyStorage_Contents(PyStorage *o, PyObject *_args) {
  try {
    return new PyView(*o);
  } catch (...) {
    return 0;
  }
}

static char *description__doc = 
  "description(name='') -- return a description of named view, or of entire storage";

static PyObject *PyStorage_Description(PyStorage *o, PyObject *_args) {
  try {
    PWOSequence args(_args);
    PWOString nm("");
    if (args.len() > 0)
      nm = args[0];
    const char *descr = o->Description(nm);
    if (descr) {
      PWOString rslt(descr);
      return rslt.disOwn();
    }
    Fail(PyExc_KeyError, nm);
  } catch (...){}
  return 0; /* satisfy compiler */
}

static char *commit__doc = 
  "commit(full=0) -- permanently commit data and structure changes to disk";

static PyObject *PyStorage_Commit(PyStorage *o, PyObject *_args) {
  try {
    PWOSequence args(_args);
    PWONumber flag(0);
    if (args.len() > 0)
      flag = args[0];
    if (!o->Commit((int)flag != 0))
      Fail(PyExc_IOError, "commit failed");
    Py_INCREF(Py_None);
    return Py_None;
  } catch (...) {
    return 0;
  }
}

static char *rollback__doc = 
  "rollback(full=0) -- revert data and structure as was last committed to disk";

static PyObject *PyStorage_Rollback(PyStorage *o, PyObject *_args) {
  try {
    PWOSequence args(_args);
    PWONumber flag(0);
    if (args.len() > 0)
      flag = args[0];
    if (!o->Rollback((int)flag != 0))
      Fail(PyExc_IOError, "rollback failed");
    Py_INCREF(Py_None);
    return Py_None;
  } catch (...) {
    return 0;
  }
}

static char *aside__doc = 
  "aside() -- revert data and structure as was last committed to disk";

static PyObject *PyStorage_Aside(PyStorage *o, PyObject *_args) {
  try {
    PWOSequence args(_args);
    if (!PyStorage_Check((PyObject*)args[0]))
      Fail(PyExc_TypeError, "First arg must be a storage");
    c4_Storage &storage = *(PyStorage*)(PyObject*)args[0];
    if (!o->SetAside(storage))
      Fail(PyExc_IOError, "aside failed");
    Py_INCREF(Py_None);
    return Py_None;
  } catch (...) {
    return 0;
  }
}

static char *view__doc = 
  "view(viewname) -- return top-level view in storage, given its name";

static PyObject *PyStorage_View(PyStorage *o, PyObject *_args) {
  try {
    PWOSequence args(_args);
    PWOString nm(args[0]);
    return new PyView(o->View(nm));
  } catch (...) {
    return 0;
  }
}

static char *getas__doc = 
  "getas(description) -- return view, create / restructure as needed to match";

static PyObject *PyStorage_GetAs(PyStorage *o, PyObject *_args) {
  try {
    PWOSequence args(_args);
    PWOString descr(args[0]);
    return new PyView(o->GetAs(descr));
  } catch (...) {
    return 0;
  }
}

static char *load__doc = 
  "load(file) -- replace storage object contents from file (or any obj supporting read)";

static PyObject *PyStorage_load(PyStorage *o, PyObject *_args) {
  try {
    PWOSequence args(_args);
    if (args.len() != 1)
      Fail(PyExc_ValueError, "load requires a file-like object");

    c4_PyStream stream(args[0]);
    o->LoadFrom(stream);

    Py_INCREF(Py_None);
    return Py_None;
  } catch (...) {
    return 0;
  }
}

static char *save__doc = 
  "save(file) -- store storage object contents to file (or any obj supporting write)";

static PyObject *PyStorage_save(PyStorage *o, PyObject *_args) {
  try {
    PWOSequence args(_args);
    if (args.len() != 1)
      Fail(PyExc_ValueError, "save requires a file-like object");

    c4_PyStream stream(args[0]);
    o->SaveTo(stream);

    Py_INCREF(Py_None);
    return Py_None;
  } catch (...) {
    return 0;
  }
}

static PyMethodDef StorageMethods[] =  {
   {
    "getas", (PyCFunction)PyStorage_GetAs, METH_VARARGS, getas__doc
  }
  ,  {
    "view", (PyCFunction)PyStorage_View, METH_VARARGS, view__doc
  }
  ,  {
    "rollback", (PyCFunction)PyStorage_Rollback, METH_VARARGS, rollback__doc
  }
  ,  {
    "commit", (PyCFunction)PyStorage_Commit, METH_VARARGS, commit__doc
  }
  ,  {
    "aside", (PyCFunction)PyStorage_Aside, METH_VARARGS, aside__doc
  }
  ,  {
    "description", (PyCFunction)PyStorage_Description, METH_VARARGS,
      description__doc
  }
  ,  {
    "contents", (PyCFunction)PyStorage_Contents, METH_VARARGS, contents__doc
  }
  ,  {
    "autocommit", (PyCFunction)PyStorage_Autocommit, METH_VARARGS,
      autocommit__doc
  }
  ,  {
    "load", (PyCFunction)PyStorage_load, METH_VARARGS, load__doc
  }
  ,  {
    "save", (PyCFunction)PyStorage_save, METH_VARARGS, save__doc
  }
  ,  {
    0, 0, 0, 0
  }
};

static void PyStorage_dealloc(PyStorage *o) {
  //o->~PyStorage();
  delete o;
}

static int PyStorage_print(PyStorage *o, FILE *f, int) {
  fprintf(f, "<PyStorage object at %lx>", (long)o);
  return 0;
}

static PyObject *PyStorage_getattr(PyStorage *o, char *nm) {
  return Py_FindMethod(StorageMethods, o, nm);
}

PyTypeObject PyStoragetype =  {
  PyObject_HEAD_INIT(&PyType_Type)0, "PyStorage", sizeof(PyStorage), 0, 
    (destructor)PyStorage_dealloc,  /*tp_dealloc*/
  (printfunc)PyStorage_print,  /*tp_print*/
  (getattrfunc)PyStorage_getattr,  /*tp_getattr*/
  0,  /*tp_setattr*/
  (cmpfunc)0,  /*tp_compare*/
  (reprfunc)0,  /*tp_repr*/
  0,  /*tp_as_number*/
  0,  /*tp_as_sequence*/
  0,  /*tp_as_mapping*/
};

static char *storage__doc = 
  "storage() -- create a new in-memory storage (can load/save, but not commit/rollback)\n""storage(file) -- attach a storage object to manage an already opened stdio file\n""storage(filename, rw) -- open file, rw=0: r/o, rw=1: r/w, rw=2: extend";

static PyObject *PyStorage_new(PyObject *o, PyObject *_args) {
  try {
    PWOSequence args(_args);
    PyStorage *ps = 0;
    switch (args.len()) {
      case 0:
        ps = new PyStorage;
        break;
      case 1:
        if (!PyFile_Check((PyObject*)args[0])) {
          if (PyString_Check((PyObject*)args[0]))
            Fail(PyExc_TypeError, "rw parameter missing");
          else
            Fail(PyExc_TypeError, "argument not an open file");
          break;
        }
        ps = new PyStorage(*new c4_FileStrategy(PyFile_AsFile(args[0])), true);
        break;
      case 4:
         { // Rrrrrr...
          if (!PyStorage_Check((PyObject*)args[0]))
            Fail(PyExc_TypeError, "First arg must be a storage object");
          c4_Storage &storage = *(PyStorage*)(PyObject*)args[0];
          if (!PyView_Check((PyObject*)args[1]))
            Fail(PyExc_TypeError, "Second arg must be a view object");
          c4_View &view = *(PyView*)(PyObject*)args[1];
          if (!PyProperty_Check((PyObject*)args[2]))
            Fail(PyExc_TypeError, "Third arg must be a property object");
          c4_BytesProp &prop = *(c4_BytesProp*)(c4_Property*)(PyProperty*)
            (PyObject*)args[2];
          int row = PWONumber(args[3]);

          ps = new PyStorage(*new SiasStrategy(storage, view, prop, row), true);
          break;
        }
      case 2:
         {
          char *fnm;
          int mode;
          if (!PyArg_ParseTuple(args, "esi", "utf_8", &fnm, &mode))
            Fail(PyExc_TypeError, "bad argument type");
          ps = new PyStorage(fnm, mode);
          PyMem_Free(fnm);
          if (!ps->Strategy().IsValid()) {
            delete ps;
            ps = 0;
            Fail(PyExc_IOError, "can't open storage file");
          }
          break;
        }
      default:
        Fail(PyExc_ValueError, "storage() takes at most 4 arguments");
    }
    return ps;
  } catch (...) {
    return 0;
  }
}

class PyViewer: public c4_CustomViewer {
    PWOSequence _data;
    c4_View _template;
    c4_Row _tempRow;
    bool _byPos;

  public:
    PyViewer(const PWOSequence &data_, const c4_View &template_, bool byPos_);
    virtual ~PyViewer();

    virtual c4_View GetTemplate();
    virtual int GetSize();
    virtual bool GetItem(int row_, int col_, c4_Bytes &buf_);
    virtual bool SetItem(int row_, int col_, const c4_Bytes &buf_);
};

PyViewer::PyViewer(const PWOSequence &data_, const c4_View &template_, bool
  byPos_): _data(data_), _template(template_), _byPos(byPos_){}

PyViewer::~PyViewer(){}

c4_View PyViewer::GetTemplate() {
  return _template;
}

int PyViewer::GetSize() {
  return _data.len();
}

bool PyViewer::GetItem(int row_, int col_, c4_Bytes &buf_) {
  const c4_Property &prop = _template.NthProperty(col_);
  if (_byPos) {
    PWOSequence item(_data[row_]);
    PyRowRef::setFromPython(_tempRow, prop, item[col_]);
    return prop(_tempRow).GetData(buf_);
  }
  PyObject *item = _data[row_];
  if (PyInstance_Check(item)) {
    PyObject *attr = PyObject_GetAttrString(item, (char*)prop.Name());
    PyRowRef::setFromPython(_tempRow, prop, attr);
    return prop(_tempRow).GetData(buf_);
  }
  if (PyDict_Check(item)) {
    PyObject *attr = PyDict_GetItemString(item, (char*)prop.Name());
    PyRowRef::setFromPython(_tempRow, prop, attr);
    return prop(_tempRow).GetData(buf_);
  }
  if (_template.NumProperties() == 1) {
    PyRowRef::setFromPython(_tempRow, prop, _data[row_]);
    return prop(_tempRow).GetData(buf_);
  }
  Fail(PyExc_ValueError, "Object has no usable attributes");
  return false;
  // create a row with just this single property value
  // this detour handles dicts and objects, because makeRow does
  /*  c4_Row one;
  PyView v (prop); // nasty, stack-based temp to get at makeRow
  v.makeRow(one, _data[row_]);
  return prop (one).GetData(buf_); */
}

bool PyViewer::SetItem(int row_, int col_, const c4_Bytes &buf_) {
  const c4_Property &prop = _template.NthProperty(col_);
  c4_Row one;
  prop(one).SetData(buf_);

  PyRowRef r(one); // careful, stack-based temp
  PyObject *item = r.asPython(prop);

  if (_byPos) {
    PWOSequence item(_data[row_]);
    item[col_] = item;
  } else if (PyDict_Check((PyObject*)_data))
    PyDict_SetItemString(_data, (char*)prop.Name(), item);
  else
    PyObject_SetAttrString(_data, (char*)prop.Name(), item);

  Py_DECREF(item);
  return true;
}

static PyObject *PyView_wrap(PyObject *o, PyObject *_args) {
  try {
    PWOSequence args(_args);
    PWOSequence seq(args[0]);
    PWOSequence types(args[1]);
    PWONumber usetuples(0);
    if (args.len() > 2)
      usetuples = args[2];

    c4_View templ;
    for (int i = 0; i < types.len(); ++i) {
      const c4_Property &prop = *(PyProperty*)(PyObject*)types[i];
      templ.AddProperty(prop);
    }

    c4_View cv = new PyViewer(seq, templ, (int)usetuples != 0);
    return new PyView(cv, 0, ROVIEWER);
  } catch (...) {
    return 0;
  }
}

static PyMethodDef Mk4Methods[] =  {
   {
    "view", PyView_new, METH_VARARGS, "view() - create a new unattached view"
  }
  ,  {
    "storage", PyStorage_new, METH_VARARGS, storage__doc
  }
  ,  {
    "property", PyProperty_new, METH_VARARGS, 
      "property(type, name) -- create a property of given type and name"
  }
  ,  {
    "wrap", PyView_wrap, METH_VARARGS, 
      "wrap(seq,props,usetuples=0) - wrap a sequence as a read-only view"
  }
  ,  {
    0, 0, 0, 0
  }
};

extern "C"__declspec(dllexport)
void initMk4py() {
  PyObject *m = Py_InitModule4("Mk4py", Mk4Methods, mk4py_module_documentation,
    0, PYTHON_API_VERSION);
  PyObject_SetAttrString(m, "version", PyString_FromString("2.4.9.7"));
  PyObject_SetAttrString(m, "ViewType", (PyObject*) &PyViewtype);
  PyObject_SetAttrString(m, "ViewerType", (PyObject*) &PyViewertype);
  PyObject_SetAttrString(m, "ROViewerType", (PyObject*) &PyROViewertype);
  PyObject_SetAttrString(m, "RowRefType", (PyObject*) &PyRowReftype);
  PyObject_SetAttrString(m, "RORowRefType", (PyObject*) &PyRORowReftype);
}
