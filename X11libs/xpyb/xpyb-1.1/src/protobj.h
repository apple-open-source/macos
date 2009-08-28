#ifndef XPYB_PROTOBJ_H
#define XPYB_PROTOBJ_H

typedef struct {
    PyObject_HEAD
    PyObject *buf;
    void *data;
} xpybProtobj;

extern PyTypeObject xpybProtobj_type;

int xpybProtobj_modinit(PyObject *m);

#endif
