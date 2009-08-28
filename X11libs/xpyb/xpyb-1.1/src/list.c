#include "module.h"
#include "except.h"
#include "list.h"

/*
 * Helpers
 */

static PyObject *
xpybList_build(PyObject *str, Py_ssize_t size, const char *data)
{
    switch (PyString_AS_STRING(str)[0]) {
    case 'b':
	return Py_BuildValue("b", *data);
    case 'B':
	return Py_BuildValue("B", *(unsigned char *)data);
    case 'h':
	return Py_BuildValue("h", *(short *)data);
    case 'H':
	return Py_BuildValue("H", *(unsigned short *)data);
    case 'i':
	return Py_BuildValue("i", *(int *)data);
    case 'I':
	return Py_BuildValue("I", *(unsigned int *)data);
    case 'L':
	return Py_BuildValue("L", *(long long *)data);
    case 'K':
	return Py_BuildValue("K", *(unsigned long long *)data);
    case 'f':
	return Py_BuildValue("f", *(float *)data);
    case 'd':
	return Py_BuildValue("d", *(double *)data);
    default:
	PyErr_SetString(xpybExcept_base, "Invalid format character.");
    }

    return NULL;
}


/*
 * Infrastructure
 */

static PyObject *
xpybList_new(PyTypeObject *self, PyObject *args, PyObject *kw)
{
    return PyType_GenericNew(self, args, kw);
}

static int
xpybList_init(xpybList *self, PyObject *args, PyObject *kw)
{
    static char *kwlist[] = { "parent", "offset", "length", "type", "size", NULL };
    Py_ssize_t i, datalen, cur, offset, length, size = -1;
    PyObject *parent, *type, *obj, *arglist;
    const char *data;

    if (!PyArg_ParseTupleAndKeywords(args, kw, "OnnO|n", kwlist, &parent,
				     &offset, &length, &type, &size))
	return -1;

    self->list = PyList_New(0);
    if (self->list == NULL)
	return -1;

    if (PyObject_AsReadBuffer(parent, (const void **)&data, &datalen) < 0)
	return -1;
    if (size > 0 && length * size + offset > datalen) {
	PyErr_Format(xpybExcept_base, "Protocol object buffer too short "
		     "(expected %zd got %zd).", length * size + offset, datalen);
	return -1;
    }

    cur = offset;

    for (i = 0; i < length; i++) {
	if (PyString_CheckExact(type)) {
	    obj = xpybList_build(type, length, data + cur);
	    if (obj == NULL)
		return -1;
	    cur += size;
	} else if (size > 0) {
	    arglist = Py_BuildValue("(Onn)", parent, cur, size);
	    obj = PyEval_CallObject(type, arglist);
	    Py_DECREF(arglist);
	    if (obj == NULL)
		return -1;
	    cur += size;
	} else {
	    arglist = Py_BuildValue("(On)", parent, cur);
	    obj = PyEval_CallObject(type, arglist);
	    Py_DECREF(arglist);
	    if (obj == NULL)
		return -1;
	    datalen = PySequence_Size(obj);
	    if (datalen < 0)
		return -1;
	    cur += datalen;
	}

	if (PyList_Append(self->list, obj) < 0)
	    return -1;
    }

    self->buf = PyBuffer_FromObject(parent, offset, cur - offset);
    if (self->buf == NULL)
	return -1;

    return 0;
}

static void
xpybList_dealloc(xpybList *self)
{
    Py_CLEAR(self->list);
    Py_CLEAR(self->buf);
    xpybList_type.tp_base->tp_dealloc((PyObject *)self);
}

static Py_ssize_t
xpybList_length(xpybList *self)
{
    return PyList_Type.tp_as_sequence->sq_length(self->list);
}

static PyObject *
xpybList_concat(xpybList *self, PyObject *arg)
{
    return PyList_Type.tp_as_sequence->sq_concat(self->list, arg);
}

static PyObject *
xpybList_repeat(xpybList *self, Py_ssize_t arg)
{
    return PyList_Type.tp_as_sequence->sq_repeat(self->list, arg);
}

static PyObject *
xpybList_item(xpybList *self, Py_ssize_t arg)
{
    return PyList_Type.tp_as_sequence->sq_item(self->list, arg);
}

static PyObject *
xpybList_slice(xpybList *self, Py_ssize_t arg1, Py_ssize_t arg2)
{
    return PyList_Type.tp_as_sequence->sq_slice(self->list, arg1, arg2);
}

static int
xpybList_ass_item(xpybList *self, Py_ssize_t arg1, PyObject *arg2)
{
    return PyList_Type.tp_as_sequence->sq_ass_item(self->list, arg1, arg2);
}

static int
xpybList_ass_slice(xpybList *self, Py_ssize_t arg1, Py_ssize_t arg2, PyObject *arg3)
{
    return PyList_Type.tp_as_sequence->sq_ass_slice(self->list, arg1, arg2, arg3);
}

static int
xpybList_contains(xpybList *self, PyObject *arg)
{
    return PyList_Type.tp_as_sequence->sq_contains(self->list, arg);
}

static PyObject *
xpybList_inplace_concat(xpybList *self, PyObject *arg)
{
    return PyList_Type.tp_as_sequence->sq_inplace_concat(self->list, arg);
}

static PyObject *
xpybList_inplace_repeat(xpybList *self, Py_ssize_t arg)
{
    return PyList_Type.tp_as_sequence->sq_inplace_repeat(self->list, arg);
}


/*
 * Members
 */


/*
 * Methods
 */

static PyObject *
xpybList_buf(xpybList *self, PyObject *args)
{
    Py_INCREF(self->buf);
    return self->buf;
}

static PyMethodDef xpybList_methods[] = {
    { "buf",
      (PyCFunction)xpybList_buf,
      METH_NOARGS,
      "Return the list's underlying buffer." },

    { NULL } /* terminator */
};


/*
 * Definition
 */

static PySequenceMethods xpybList_seqops = {
    .sq_length = (lenfunc)xpybList_length,
    .sq_concat = (binaryfunc)xpybList_concat,
    .sq_repeat = (ssizeargfunc)xpybList_repeat,
    .sq_item = (ssizeargfunc)xpybList_item,
    .sq_slice = (ssizessizeargfunc)xpybList_slice,
    .sq_ass_item = (ssizeobjargproc)xpybList_ass_item,
    .sq_ass_slice = (ssizessizeobjargproc)xpybList_ass_slice,
    .sq_contains = (objobjproc)xpybList_contains,
    .sq_inplace_concat = (binaryfunc)xpybList_inplace_concat,
    .sq_inplace_repeat = (ssizeargfunc)xpybList_inplace_repeat
};

PyTypeObject xpybList_type = {
    PyObject_HEAD_INIT(NULL)
    .tp_name = "xcb.List",
    .tp_basicsize = sizeof(xpybList),
    .tp_new = xpybList_new,
    .tp_init = (initproc)xpybList_init,
    .tp_dealloc = (destructor)xpybList_dealloc,
    .tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
    .tp_doc = "XCB generic list object",
    .tp_methods = xpybList_methods,
    .tp_as_sequence = &xpybList_seqops
};


/*
 * Module init
 */
int xpybList_modinit(PyObject *m)
{
    if (PyType_Ready(&xpybList_type) < 0)
        return -1;
    Py_INCREF(&xpybList_type);
    if (PyModule_AddObject(m, "List", (PyObject *)&xpybList_type) < 0)
	return -1;

    return 0;
}
