// PyRowRef.cpp --
// $Id: PyRowRef.cpp 1230 2007-03-09 15:58:53Z jcw $
// This is part of MetaKit, the homepage is http://www.equi4.com/metakit.html
// Copyright (C) 1999-2004 Gordon McMillan and Jean-Claude Wippler.
//
//  RowRef class implementation

#include <PWOSequence.h>
#include <PWONumber.h>
#include "PyRowRef.h"
#include "PyView.h"

#if defined(PY_LONG_LONG) && !defined(LONG_LONG)
#define LONG_LONG PY_LONG_LONG
#endif 

static PyMethodDef RowRefMethods[] =  {
   {
    0, 0, 0, 0
  }
};

static void PyRowRef_dealloc(PyRowRef *o) {
  //o->~PyRowRef();
  delete o;
}

static int PyRowRef_print(PyRowRef *o, FILE *f, int) {
  fprintf(f, "<PyRowRef object at %p>", (void*)o);
  return 0;
}

static int PyRORowRef_print(PyRowRef *o, FILE *f, int) {
  fprintf(f, "<PyRORowRef object at %p>", (void*)o);
  return 0;
}

static PyObject *PyRowRef_getattr(PyRowRef *o, char *nm) {
  try {
    if (nm[0] == '_' && nm[1] == '_') {
      if (strcmp(nm, "__attrs__") == 0) {
        c4_View parent = o->Container();
        int nprops = parent.NumProperties();
        PyObject *out = PyList_New(nprops);
        for (int i = 0; i < nprops; i++) {
          PyList_SetItem(out, i, new PyProperty(parent.NthProperty(i)));
        }
        return out;
      } else if (strcmp(nm, "__view__") == 0) {
        return new PyView(o->Container());
      } else if (strcmp(nm, "__index__") == 0) {
        return PyInt_FromLong((&(*o))._index);
      }
    }

    PyObject *attr = o->getPropertyValue(nm);
    if (attr)
      return attr;
    PyErr_Clear();
    return Py_FindMethod(RowRefMethods, (PyObject*)o, nm);
  } catch (...) {
    return NULL;
  }
}

static int PyRowRef_setattr(PyRowRef *o, char *nm, PyObject *v) {
  try {
    PyProperty *p = o->getProperty(nm);
    if (p) {
      if (v)
        PyRowRef::setFromPython(*o,  *p, v);
      else
        PyRowRef::setDefault(*o,  *p);
      Py_DECREF(p);
      return 0;
    }
    PyErr_SetString(PyExc_AttributeError, "delete of nonexistent attribute");
    return  - 1;
  } catch (...) {
    return  - 1;
  }
}

PyTypeObject PyRowReftype =  {
  PyObject_HEAD_INIT(&PyType_Type)0, "PyRowRef", sizeof(PyRowRef), 0, 
    (destructor)PyRowRef_dealloc,  /*tp_dealloc*/
  (printfunc)PyRowRef_print,  /*tp_print*/
  (getattrfunc)PyRowRef_getattr,  /*tp_getattr*/
  (setattrfunc)PyRowRef_setattr,  /*tp_setattr*/
  (cmpfunc)0,  /*tp_compare*/
  (reprfunc)0,  /*tp_repr*/
  0,  /*tp_as_number*/
  0,  /*tp_as_sequence*/
  0,  /*tp_as_mapping*/
};
PyTypeObject PyRORowReftype =  {
  PyObject_HEAD_INIT(&PyType_Type)0, "PyRORowRef", sizeof(PyRowRef), 0, 
    (destructor)PyRowRef_dealloc,  /*tp_dealloc*/
  (printfunc)PyRORowRef_print,  /*tp_print*/
  (getattrfunc)PyRowRef_getattr,  /*tp_getattr*/
  (setattrfunc)0,  /*tp_setattr*/
  (cmpfunc)0,  /*tp_compare*/
  (reprfunc)0,  /*tp_repr*/
  0,  /*tp_as_number*/
  0,  /*tp_as_sequence*/
  0,  /*tp_as_mapping*/
};


PyRowRef::PyRowRef(const c4_RowRef &o, int immutable): PyHead(PyRowReftype),
  c4_RowRef(o) {
  if (immutable)
    ob_type = &PyRORowReftype;
  c4_Cursor c = &(*(c4_RowRef*)this);
  c._seq->IncRef();
}

// with thanks to Niki Spahiev for improving conversions and error checks
void PyRowRef::setFromPython(const c4_RowRef &row, const c4_Property &prop,
  PyObject *attr) {
  switch (prop.Type()) {
    case 'I':
      if (PyInt_Check(attr)) {
        long number = PyInt_AsLong(attr);
        if (number ==  - 1 && PyErr_Occurred() != NULL)
          Fail(PyExc_ValueError, "int too large to convert to C long");
        ((const c4_IntProp &)prop)(row) = number;
      }
       else if (attr != Py_None) {
        PWONumber number(attr);
        ((const c4_IntProp &)prop)(row) = (long)number;
      }
      break;
#ifdef HAVE_LONG_LONG
    case 'L':
      if (PyLong_Check(attr)) {
        LONG_LONG number = PyLong_AsLongLong(attr);
        if (number ==  - 1 && PyErr_Occurred() != NULL)
          Fail(PyExc_ValueError, "long int too large to convert to C long long")
            ;
        ((const c4_LongProp &)prop)(row) = number;
      }
       else if (PyInt_Check(attr)) {
        long number = PyInt_AsLong(attr);
        if (number ==  - 1 && PyErr_Occurred() != NULL)
          Fail(PyExc_ValueError, "int too large to convert to C long");
        ((const c4_LongProp &)prop)(row) = number;
      }
       else if (attr != Py_None) {
        PWONumber number(attr);
        ((const c4_LongProp &)prop)(row) = (LONG_LONG)number;
      }
      break;
#endif 
    case 'F':
      if (PyFloat_Check(attr))
        ((const c4_FloatProp &)prop)(row) = PyFloat_AS_DOUBLE(attr);
      else if (attr != Py_None) {
        PWONumber number(attr);
        ((const c4_FloatProp &)prop)(row) = (double)number;
      }
      break;
    case 'D':
      if (PyFloat_Check(attr))
        ((const c4_DoubleProp &)prop)(row) = PyFloat_AS_DOUBLE(attr);
      else if (attr != Py_None) {
        PWONumber number(attr);
        ((const c4_DoubleProp &)prop)(row) = (double)number;
      }
      break;
    case 'S':
      if (attr != Py_None) {
#ifdef HAVE_UNICODEOBJECT_H
        bool is_unicode = PyUnicode_Check(attr);
        if (is_unicode)
          attr = PyUnicode_AsUTF8String(attr);
#else 
        const bool is_unicode = false;
#endif 
        PWOString string(attr);
        if (is_unicode)
          Py_DECREF(attr);

        size_t size = string.size();
        const char *cstring = string;
        if (size != strlen(cstring))
          Fail(PyExc_ValueError, "string contains embedded nulls; try 'B' type")
            ;
        c4_Bytes temp(cstring, size + 1, false);
        prop(row).SetData(temp);
      }
      break;
    case 'V':
      if (PyView_Check(attr)) {
        PyView *obj = (PyView*)attr;
        ((const c4_ViewProp &)prop)(row) =  *obj;
      }
       else {
        //((const c4_ViewProp&) prop) (row) = c4_View ();
        PyView tmp(((const c4_ViewProp &)prop)(row));
        PWOSequence lst(attr);
        tmp.SetSize(lst.len());
        for (int i = 0; i < lst.len(); i++) {
          PyObject *entry = lst[i];
          tmp.setItem(i, entry);
          Py_DECREF(entry);
        }
      }
      break;
    case 'B':
    case 'M':
      if (PyString_Check(attr)) {
        c4_Bytes temp(PyString_AS_STRING(attr), PyString_GET_SIZE(attr), false);
        prop(row).SetData(temp);
      }
       else if (attr != Py_None)
        Fail(PyExc_TypeError, "wrong type for ByteProp");
      break;
    default:
      PyErr_Format(PyExc_TypeError, "unknown property type '%c'", prop.Type());
      throw PWDPyException;
  }
}

void PyRowRef::setDefault(const c4_RowRef &row, const c4_Property &prop) {
  switch (prop.Type()) {
    case 'I':
      ((const c4_IntProp &)prop)(row) = 0;
      break;
#ifdef HAVE_LONG_LONG
    case 'L':
      ((const c4_LongProp &)prop)(row) = 0;
      break;
#endif 
    case 'F':
      ((const c4_FloatProp &)prop)(row) = 0.0;
      break;
    case 'D':
      ((const c4_DoubleProp &)prop)(row) = 0.0;
      break;
    case 'S':
      ((const c4_StringProp &)prop)(row) = "";
      break;
    case 'V':
      ((const c4_ViewProp &)prop)(row) = c4_View();
      break;
    case 'B':
    case 'M':
       {
        c4_Bytes temp;
        prop(row).SetData(temp);
      }
      break;
    default:
      PyErr_Format(PyExc_TypeError, "unknown property type '%c'", prop.Type());
      throw PWDPyException;
  }
}

PyObject *PyRowRef::asPython(const c4_Property &prop) {
  switch (prop.Type()) {
    case 'I':
       {
        PWONumber rslt(((const c4_IntProp &)prop)(*this));
        return rslt.disOwn();
      }
#ifdef HAVE_LONG_LONG
    case 'L':
       {
        return PyLong_FromLongLong((LONG_LONG)((const c4_LongProp &)prop)(*this)
          );
      }
#endif 
    case 'F':
       {
        PWONumber rslt(((const c4_FloatProp &)prop)(*this));
        return rslt.disOwn();
      }
    case 'D':
       {
        PWONumber rslt(((const c4_DoubleProp &)prop)(*this));
        return rslt.disOwn();
      }
    case 'S':
       {
        PWOString rslt(((c4_StringProp &)prop).Get(*this));
        return rslt.disOwn();
      }
    case 'V':
       {
        return new PyView(((const c4_ViewProp &)prop)(*this));
      }
    case 'B':
    case 'M':
       {
        c4_Bytes temp;
        prop(*this).GetData(temp);
        PWOString rslt((const char*)temp.Contents(), temp.Size());
        return rslt.disOwn();
      }
    default:
      return PyErr_Format(PyExc_TypeError, "unknown property type '%c'",
        prop.Type());
  }
}
