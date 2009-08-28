#ifndef XPYB_ITER_H
#define XPYB_ITER_H

typedef struct {
    PyObject_HEAD
    PyObject *name;
    PyObject *list;
    PyObject *stack;
    Py_ssize_t top;
    Py_ssize_t groupsize;
    int is_list;
} xpybIter;

extern PyTypeObject xpybIter_type;

int xpybIter_modinit(PyObject *m);

#endif
