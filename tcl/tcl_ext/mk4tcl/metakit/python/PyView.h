// PyView.h --
// $Id: PyView.h 1230 2007-03-09 15:58:53Z jcw $
// This is part of MetaKit, see http://www.equi4.com/metakit.html
// Copyright (C) 1999-2004 Gordon McMillan and Jean-Claude Wippler.
//
//  View class header

#if !defined INCLUDE_PYVIEW_H
#define INCLUDE_PYVIEW_H

#include <mk4.h>
#include <PWOSequence.h>
#include <PWOCallable.h>
#include <PWONumber.h>
#include "PyHead.h"

#define PyView_Check(v) ((v)->ob_type==&PyViewtype)
#define PyViewer_Check(v) ((v)->ob_type==&PyViewertype)
#define PyROViewer_Check(v) ((v)->ob_type==&PyROViewertype)
#define PyGenericView_Check(v) (PyView_Check(v) || PyViewer_Check(v) || \
  PyROViewer_Check(v))

class PyView;
class PyRowRef;

extern PyTypeObject PyViewtype;
extern PyTypeObject PyViewertype;
extern PyTypeObject PyROViewertype;

#define BASE 0              //0000
#define MVIEWER 4           //0100
#define RWVIEWER 5          //0101
#define NOTIFIABLE 1        //0001
#define FINALNOTIFIABLE 9   //1001
#define ROVIEWER 7          //0111
#define IMMUTABLEROWS 2

class PyView: public PyHead, public c4_View {
    PyView *_base;
    int _state;
  public:
    PyView();
    PyView(const c4_View &o, PyView *owner = 0, int state = BASE);
    ~PyView(){}
    void insertAt(int i, PyObject *o);
    PyRowRef *getItem(int i);
    PyView *getSlice(int s, int e);
    int setItemRow(int i, const c4_RowRef &v) {
        if (i < 0)
          i += GetSize();
        if (i > GetSize() || i < 0)
          Fail(PyExc_IndexError, "Index out of range");
        SetAt(i, v);
        return 0;
    };
    int setItem(int i, PyObject *v);
    void addProperties(const PWOSequence &lst);
    int setSlice(int s, int e, const PWOSequence &lst);
    PyObject *structure();
    void makeRow(c4_Row &temp, PyObject *o, bool useDefaults = true);
    void makeRowFromDict(c4_Row &temp, PyObject *o, bool useDefaults = true);
    void map(const PWOCallable &func);
    void map(const PWOCallable &func, const PyView &subset);
    PyView *filter(const PWOCallable &func);
    PyObject *reduce(const PWOCallable &func, PWONumber &start);
    void remove(const PyView &indices);
    PyView *indices(const PyView &subset);
    int computeState(int targetstate);
    PyObject *properties();
};

PyObject *PyView_new(PyObject *o, PyObject *_args);

#endif
