#include "module.h"
#include "except.h"
#include "request.h"

/*
 * Helpers
 */


/*
 * Infrastructure
 */

static int
xpybRequest_init(xpybRequest *self, PyObject *args, PyObject *kw)
{
    static char *kwlist[] = {"buffer", "opcode", "void", "checked", NULL };
    PyObject *is_void, *is_checked, *buf;
    unsigned char opcode;
    const void *data;
    Py_ssize_t size;

    if (!PyArg_ParseTupleAndKeywords(args, kw, "OBOO", kwlist, &buf,
				     &opcode, &is_void, &is_checked))
	return -1;

    if (PyObject_AsReadBuffer(buf, &data, &size) < 0)
	return -1;
    if (size < 4) {
	PyErr_SetString(PyExc_ValueError, "Request buffer too short.");
	return -1;
    }

    self->opcode = opcode;
    self->is_void = PyObject_IsTrue(is_void);
    self->is_checked = PyObject_IsTrue(is_checked);
    Py_INCREF(((xpybProtobj *)self)->buf = buf);
    return 0;
}

/*
 * Members
 */


/*
 * Definition
 */

PyTypeObject xpybRequest_type = {
    PyObject_HEAD_INIT(NULL)
    .tp_name = "xcb.Request",
    .tp_basicsize = sizeof(xpybRequest),
    .tp_init = (initproc)xpybRequest_init,
    .tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
    .tp_doc = "XCB generic request object",
    .tp_base = &xpybProtobj_type,
};


/*
 * Module init
 */
int xpybRequest_modinit(PyObject *m)
{
    if (PyType_Ready(&xpybRequest_type) < 0)
        return -1;
    Py_INCREF(&xpybRequest_type);
    if (PyModule_AddObject(m, "Request", (PyObject *)&xpybRequest_type) < 0)
	return -1;

    return 0;
}
