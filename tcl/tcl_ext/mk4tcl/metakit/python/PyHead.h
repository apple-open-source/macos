// PyHead.h --
// $Id: PyHead.h,v 1.8 2004/01/21 01:46:06 jcw Exp $
// This is part of MetaKit, see http://www.equi4.com/metakit/
// Copyright (C) 1999-2004 Gordon McMillan and Jean-Claude Wippler.
//
//  Common object header class

#if !defined INCLUDE_PYHEAD_H
#define INCLUDE_PYHEAD_H

#include <Python.h>

class PyHead : public PyObject {
public:
    PyHead(PyTypeObject& t)
    {
#ifdef Py_TRACE_REFS
        _ob_next = 0;
        _ob_prev = 0;
#endif
        ob_refcnt = 1;
        ob_type = &t;
    }
};

#endif

