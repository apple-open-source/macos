#include "module.h"
#include "except.h"

PyObject *xpybExcept_ext;
PyObject *xpybExcept_base;
PyObject *xpybExcept_conn;
PyObject *xpybExcept_proto;

int xpybExcept_modinit(PyObject *m)
{
    xpybExcept_base = PyErr_NewException("xcb.Exception", NULL, NULL);
    if (xpybExcept_base == NULL)
	return -1;
    Py_INCREF(xpybExcept_base);
    if (PyModule_AddObject(m, "Exception", xpybExcept_base) < 0)
	return -1;

    xpybExcept_conn = PyErr_NewException("xcb.ConnectException", xpybExcept_base, NULL);
    if (xpybExcept_conn == NULL)
	return -1;
    Py_INCREF(xpybExcept_conn);
    if (PyModule_AddObject(m, "ConnectException", xpybExcept_conn) < 0)
	return -1;

    xpybExcept_ext = PyErr_NewException("xcb.ExtensionException", xpybExcept_base, NULL);
    if (xpybExcept_ext == NULL)
	return -1;
    Py_INCREF(xpybExcept_ext);
    if (PyModule_AddObject(m, "ExtensionException", xpybExcept_ext) < 0)
	return -1;

    xpybExcept_proto = PyErr_NewException("xcb.ProtocolException", xpybExcept_base, NULL);
    if (xpybExcept_proto == NULL)
	return -1;
    Py_INCREF(xpybExcept_proto);
    if (PyModule_AddObject(m, "ProtocolException", xpybExcept_proto) < 0)
	return -1;

    return 0;
}

