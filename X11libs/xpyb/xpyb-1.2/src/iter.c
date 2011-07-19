#include "module.h"
#include "except.h"
#include "iter.h"

/*
 * Helpers
 */

static void
xpybIter_err(xpybIter *self)
{
    if (self->is_list)
	PyErr_Format(xpybExcept_base,
		     "Extra items in '%s' list (expect multiple of %d).",
		     PyString_AS_STRING(self->name), self->groupsize);
    else
	PyErr_Format(xpybExcept_base,
		     "Too few items in '%s' list (expect %d).",
		     PyString_AS_STRING(self->name), self->groupsize);
}

static PyObject *
xpybIter_pop(xpybIter *self)
{
    PyObject *cur, *next, *item;

    cur = PyList_GET_ITEM(self->stack, self->top);
    item = PyIter_Next(cur);

    if (item == NULL) {
	if (PyErr_Occurred() || self->top < 1)
	    return NULL;
	if (PyList_SetSlice(self->stack, self->top, self->top + 1, NULL) < 0)
	    return NULL;
	self->top--;
	return xpybIter_pop(self);
    }

    if (PySequence_Check(item)) {
	next = PyObject_GetIter(item);
	if (next == NULL)
	    goto err1;
	if (PyList_Append(self->stack, next) < 0)
	    goto err2;

	self->top++;
	Py_DECREF(next);
	Py_DECREF(item);
	return xpybIter_pop(self);
    }

    return item;
err2:
    Py_DECREF(next);
err1:
    Py_DECREF(item);
    return NULL;
}


/*
 * Infrastructure
 */

static PyObject *
xpybIter_new(PyTypeObject *self, PyObject *args, PyObject *kw)
{
    return PyType_GenericNew(self, args, kw);
}

static int
xpybIter_init(xpybIter *self, PyObject *args, PyObject *kw)
{
    PyObject *name, *list, *bool;
    Py_ssize_t groupsize;

    if (!PyArg_ParseTuple(args, "OnSO", &list, &groupsize, &name, &bool))
	return -1;
    
    Py_INCREF(self->name = name);
    Py_INCREF(self->list = list);
    self->groupsize = groupsize;
    self->is_list = PyObject_IsTrue(bool);
    return 0;
}

static PyObject *
xpybIter_get(xpybIter *self)
{
    PyObject *iterator;

    Py_CLEAR(self->stack);

    self->stack = PyList_New(1);
    if (self->stack == NULL)
	return NULL;

    iterator = PyObject_GetIter(self->list);
    if (iterator == NULL)
	return NULL;

    PyList_SET_ITEM(self->stack, 0, iterator);
    self->top = 0;

    Py_INCREF(self);
    return (PyObject *)self;
}

static PyObject *
xpybIter_next(xpybIter *self)
{
    PyObject *tuple, *tmp;
    Py_ssize_t i;

    tuple = PyTuple_New(self->groupsize);
    if (tuple == NULL)
	return NULL;

    for (i = 0; i < self->groupsize; i++) {
	tmp = xpybIter_pop(self);
	if (tmp == NULL) {
	    if (i > 0 && !PyErr_Occurred())
		xpybIter_err(self);
	    goto end;
	}
	PyTuple_SET_ITEM(tuple, i, tmp);
    }

    return tuple;
end:
    Py_DECREF(tuple);
    return NULL;
}

static void
xpybIter_dealloc(xpybIter *self)
{
    Py_CLEAR(self->stack);
    Py_CLEAR(self->list);
    Py_CLEAR(self->name);

    xpybIter_type.tp_base->tp_dealloc((PyObject *)self);
}


/*
 * Members
 */


/*
 * Definition
 */

PyTypeObject xpybIter_type = {
    PyObject_HEAD_INIT(NULL)
    .tp_name = "xcb.Iterator",
    .tp_basicsize = sizeof(xpybIter),
    .tp_init = (initproc)xpybIter_init,
    .tp_new = xpybIter_new,
    .tp_dealloc = (destructor)xpybIter_dealloc,
    .tp_flags = Py_TPFLAGS_DEFAULT,
    .tp_doc = "XCB flattening-iterator object",
    .tp_iter = (getiterfunc)xpybIter_get,
    .tp_iternext = (iternextfunc)xpybIter_next
};


/*
 * Module init
 */
int xpybIter_modinit(PyObject *m)
{
    if (PyType_Ready(&xpybIter_type) < 0)
        return -1;
    Py_INCREF(&xpybIter_type);
    if (PyModule_AddObject(m, "Iterator", (PyObject *)&xpybIter_type) < 0)
	return -1;

    return 0;
}
