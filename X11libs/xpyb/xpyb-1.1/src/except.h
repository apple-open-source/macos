#ifndef XPYB_EXCEPT_H
#define XPYB_EXCEPT_H

extern PyObject *xpybExcept_base;
extern PyObject *xpybExcept_conn;
extern PyObject *xpybExcept_ext;
extern PyObject *xpybExcept_proto;

int xpybExcept_modinit(PyObject *m);

#endif
