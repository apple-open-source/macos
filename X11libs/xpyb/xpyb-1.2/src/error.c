#include "module.h"
#include "except.h"
#include "response.h"
#include "error.h"

/*
 * Helpers
 */

int
xpybError_set(xpybConn *conn, xcb_generic_error_t *e)
{
    unsigned char opcode;
    PyObject *shim, *error, *type, *except;

    type = (PyObject *)&xpybError_type;
    except = xpybExcept_proto;

    if (e) {
	opcode = e->error_code;
	if (opcode < conn->errors_len && conn->errors[opcode] != NULL) {
	    type = PyTuple_GET_ITEM(conn->errors[opcode], 0);
	    except = PyTuple_GET_ITEM(conn->errors[opcode], 1);
	}

	shim = PyBuffer_FromMemory(e, sizeof(*e));
	if (shim == NULL)
	    return 1;

	error = PyObject_CallFunctionObjArgs(type, shim, NULL);
	if (error != NULL)
	    PyErr_SetObject(except, error);
	Py_DECREF(shim);
	return 1;
    }
    return 0;
}


/*
 * Infrastructure
 */


/*
 * Members
 */

static PyObject *
xpybError_getattro(PyObject *self, PyObject *obj)
{
    const char *name = PyString_AS_STRING(obj);
    const xcb_generic_error_t *data;
    Py_ssize_t size;

    if (PyObject_AsReadBuffer(self, (const void **)&data, &size) < 0)
	return NULL;

    if (strcmp(name, "code") == 0)
	return Py_BuildValue("B", data->error_code);

    return xpybError_type.tp_base->tp_getattro(self, obj);
}


/*
 * Definition
 */

PyTypeObject xpybError_type = {
    PyObject_HEAD_INIT(NULL)
    .tp_name = "xcb.Error",
    .tp_basicsize = sizeof(xpybError),
    .tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
    .tp_doc = "XCB generic error object",
    .tp_base = &xpybResponse_type,
    .tp_getattro = xpybError_getattro
};


/*
 * Module init
 */
int xpybError_modinit(PyObject *m)
{
    if (PyType_Ready(&xpybError_type) < 0)
        return -1;
    Py_INCREF(&xpybError_type);
    if (PyModule_AddObject(m, "Error", (PyObject *)&xpybError_type) < 0)
	return -1;

    return 0;
}
