#include "module.h"
#include "except.h"
#include "extkey.h"

/*
 * Helpers
 */


/*
 * Infrastructure
 */

static PyObject *
xpybExtkey_new(PyTypeObject *self, PyObject *args, PyObject *kw)
{
    return PyType_GenericNew(self, args, kw);
}

static int
xpybExtkey_init(xpybExtkey *self, PyObject *args)
{
    PyStringObject *name;

    if (!PyArg_ParseTuple(args, "S", &name))
	return -1;

    Py_INCREF(self->name = name);
    self->key.name = PyString_AS_STRING(name);
    return 0;
}

static long
xpybExtkey_hash(xpybExtkey *self)
{
    return PyString_Type.tp_hash((PyObject *)self->name);
}

static void
xpybExtkey_dealloc(xpybExtkey *self)
{
    Py_CLEAR(self->name);
    self->ob_type->tp_free((PyObject *)self);
}


/*
 * Definition
 */

PyTypeObject xpybExtkey_type = {
    PyObject_HEAD_INIT(NULL)
    .tp_name = "xcb.ExtensionKey",
    .tp_basicsize = sizeof(xpybExtkey),
    .tp_init = (initproc)xpybExtkey_init,
    .tp_new = xpybExtkey_new,
    .tp_dealloc = (destructor)xpybExtkey_dealloc,
    .tp_flags = Py_TPFLAGS_DEFAULT,
    .tp_doc = "XCB extension-key object",
    .tp_hash = (hashfunc)xpybExtkey_hash
};


/*
 * Module init
 */
int xpybExtkey_modinit(PyObject *m)
{
    if (PyType_Ready(&xpybExtkey_type) < 0)
        return -1;
    Py_INCREF(&xpybExtkey_type);
    if (PyModule_AddObject(m, "ExtensionKey", (PyObject *)&xpybExtkey_type) < 0)
	return -1;

    return 0;
}
