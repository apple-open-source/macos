#ifndef XPYB_LIST_H
#define XPYB_LIST_H

#include "protobj.h"

typedef struct {
    PyObject_HEAD
    PyObject *buf;
    PyObject *list;
} xpybList;

extern PyTypeObject xpybList_type;

int xpybList_modinit(PyObject *m);

#endif
