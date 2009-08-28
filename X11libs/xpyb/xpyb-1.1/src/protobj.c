#include "module.h"
#include "except.h"
#include "protobj.h"


/*
 * Infrastructure
 */

static PyObject *
xpybProtobj_new(PyTypeObject *self, PyObject *args, PyObject *kw)
{
    return PyType_GenericNew(self, args, kw);
}

static int
xpybProtobj_init(xpybProtobj *self, PyObject *args, PyObject *kw)
{
    static char *kwlist[] = { "parent", "offset", "size", NULL };
    Py_ssize_t offset = 0, size = Py_END_OF_BUFFER;
    PyObject *parent;

    if (!PyArg_ParseTupleAndKeywords(args, kw, "O|nn", kwlist,
				     &parent, &offset, &size))
	return -1;

    self->buf = PyBuffer_FromObject(parent, offset, size);
    if (self->buf == NULL)
	return -1;

    return 0;
}

static void
xpybProtobj_dealloc(xpybProtobj *self)
{
    Py_CLEAR(self->buf);
    free(self->data);
    self->ob_type->tp_free((PyObject *)self);
}

static Py_ssize_t
xpybProtobj_readbuf(xpybProtobj *self, Py_ssize_t s, void **p)
{
    return PyBuffer_Type.tp_as_buffer->bf_getreadbuffer(self->buf, s, p);
}

static Py_ssize_t
xpybProtobj_segcount(xpybProtobj *self, Py_ssize_t *s)
{
    return PyBuffer_Type.tp_as_buffer->bf_getsegcount(self->buf, s);
}

static Py_ssize_t
xpybProtobj_charbuf(xpybProtobj *self, Py_ssize_t s, char **p)
{
    return PyBuffer_Type.tp_as_buffer->bf_getcharbuffer(self->buf, s, p);
}

static Py_ssize_t
xpybProtobj_length(xpybProtobj *self)
{
    return PyBuffer_Type.tp_as_sequence->sq_length(self->buf);
}

static PyObject *
xpybProtobj_concat(xpybProtobj *self, PyObject *arg)
{
    return PyBuffer_Type.tp_as_sequence->sq_concat(self->buf, arg);
}

static PyObject *
xpybProtobj_repeat(xpybProtobj *self, Py_ssize_t arg)
{
    return PyBuffer_Type.tp_as_sequence->sq_repeat(self->buf, arg);
}

static PyObject *
xpybProtobj_item(xpybProtobj *self, Py_ssize_t arg)
{
    return PyBuffer_Type.tp_as_sequence->sq_item(self->buf, arg);
}

static PyObject *
xpybProtobj_slice(xpybProtobj *self, Py_ssize_t arg1, Py_ssize_t arg2)
{
    return PyBuffer_Type.tp_as_sequence->sq_slice(self->buf, arg1, arg2);
}

static int
xpybProtobj_ass_item(xpybProtobj *self, Py_ssize_t arg1, PyObject *arg2)
{
    return PyBuffer_Type.tp_as_sequence->sq_ass_item(self->buf, arg1, arg2);
}

static int
xpybProtobj_ass_slice(xpybProtobj *self, Py_ssize_t arg1, Py_ssize_t arg2, PyObject *arg3)
{
    return PyBuffer_Type.tp_as_sequence->sq_ass_slice(self->buf, arg1, arg2, arg3);
}

static int
xpybProtobj_contains(xpybProtobj *self, PyObject *arg)
{
    return PyBuffer_Type.tp_as_sequence->sq_contains(self->buf, arg);
}

static PyObject *
xpybProtobj_inplace_concat(xpybProtobj *self, PyObject *arg)
{
    return PyBuffer_Type.tp_as_sequence->sq_inplace_concat(self->buf, arg);
}

static PyObject *
xpybProtobj_inplace_repeat(xpybProtobj *self, Py_ssize_t arg)
{
    return PyBuffer_Type.tp_as_sequence->sq_inplace_repeat(self->buf, arg);
}


/*
 * Members
 */


/*
 * Methods
 */


/*
 * Definition
 */

static PyBufferProcs xpybProtobj_bufops = {
    .bf_getreadbuffer = (readbufferproc)xpybProtobj_readbuf,
    .bf_getsegcount = (segcountproc)xpybProtobj_segcount,
    .bf_getcharbuffer = (charbufferproc)xpybProtobj_charbuf
};

static PySequenceMethods xpybProtobj_seqops = {
    .sq_length = (lenfunc)xpybProtobj_length,
    .sq_concat = (binaryfunc)xpybProtobj_concat,
    .sq_repeat = (ssizeargfunc)xpybProtobj_repeat,
    .sq_item = (ssizeargfunc)xpybProtobj_item,
    .sq_slice = (ssizessizeargfunc)xpybProtobj_slice,
    .sq_ass_item = (ssizeobjargproc)xpybProtobj_ass_item,
    .sq_ass_slice = (ssizessizeobjargproc)xpybProtobj_ass_slice,
    .sq_contains = (objobjproc)xpybProtobj_contains,
    .sq_inplace_concat = (binaryfunc)xpybProtobj_inplace_concat,
    .sq_inplace_repeat = (ssizeargfunc)xpybProtobj_inplace_repeat
};

PyTypeObject xpybProtobj_type = {
    PyObject_HEAD_INIT(NULL)
    .tp_name = "xcb.Protobj",
    .tp_basicsize = sizeof(xpybProtobj),
    .tp_init = (initproc)xpybProtobj_init,
    .tp_new = xpybProtobj_new,
    .tp_dealloc = (destructor)xpybProtobj_dealloc,
    .tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
    .tp_doc = "XCB generic X protocol object",
    .tp_as_buffer = &xpybProtobj_bufops,
    .tp_as_sequence = &xpybProtobj_seqops
};


/*
 * Module init
 */
int xpybProtobj_modinit(PyObject *m)
{
    if (PyType_Ready(&xpybProtobj_type) < 0)
        return -1;
    Py_INCREF(&xpybProtobj_type);
    if (PyModule_AddObject(m, "Protobj", (PyObject *)&xpybProtobj_type) < 0)
	return -1;

    return 0;
}
