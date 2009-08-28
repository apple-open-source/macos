// PyProperty.cpp --
// $Id: PyProperty.cpp 1230 2007-03-09 15:58:53Z jcw $
// This is part of MetaKit, the homepage is http://www.equi4.com/metakit.html
// Copyright (C) 1999-2004 Gordon McMillan and Jean-Claude Wippler.
//
//  Property class implementation

#include "PyProperty.h"
#include <PWONumber.h>
#include <PWOSequence.h>

static PyMethodDef PropertyMethods[] =  {
   {
    0, 0, 0, 0
  }
};

static void PyProperty_dealloc(PyProperty *o) {
  delete o;
}

static int PyProperty_print(PyProperty *o, FILE *f, int) {
  fprintf(f, "Property('%c', '%s')", o->Type(), o->Name());
  return 0;
}

static PyObject *PyProperty_getattr(PyProperty *o, char *nm) {
  try {
    if (nm[0] == 'n' && strcmp(nm, "name") == 0) {
      PWOString rslt(o->Name());
      return rslt.disOwn();
    }
    if (nm[0] == 't' && strcmp(nm, "type") == 0) {
      char s = o->Type();
      PWOString rslt(&s, 1);
      return rslt.disOwn();
    }
    if (nm[0] == 'i' && strcmp(nm, "id") == 0) {
      PWONumber rslt(o->GetId());
      return rslt.disOwn();
    }
    return Py_FindMethod(PropertyMethods, o, nm);
  } catch (...) {
    return 0;
  }
}

static int PyProperty_compare(PyProperty *o, PyObject *ob) {
  PyProperty *other;
  int myid, hisid;
  try {
    if (!PyProperty_Check(ob))
      return  - 1;
    other = (PyProperty*)ob;
    myid = o->GetId();
    hisid = other->GetId();
    if (myid < hisid)
      return  - 1;
    else if (myid == hisid)
      return 0;
    return 1;
  } catch (...) {
    return  - 1;
  }
}

PyTypeObject PyPropertytype =  {
  PyObject_HEAD_INIT(&PyType_Type)0, "PyProperty", sizeof(PyProperty), 0, 
    (destructor)PyProperty_dealloc,  /*tp_dealloc*/
  (printfunc)PyProperty_print,  /*tp_print*/
  (getattrfunc)PyProperty_getattr,  /*tp_getattr*/
  0,  /*tp_setattr*/
  (cmpfunc)PyProperty_compare,  /*tp_compare*/
  0,  /*tp_repr*/
  0,  /*tp_as_number*/
  0,  /*tp_as_sequence*/
  0,  /*tp_as_mapping*/
};

PyObject *PyProperty_new(PyObject *o, PyObject *_args) {
  try {
    PWOSequence args(_args);
    PWOString typ(args[0]);
    PWOString nam(args[1]);
    return new PyProperty(*(const char*)typ, nam);
  } catch (...) {
    return 0;
  }
}
