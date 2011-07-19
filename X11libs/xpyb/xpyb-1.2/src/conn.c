#include "module.h"
#include "except.h"
#include "cookie.h"
#include "response.h"
#include "event.h"
#include "error.h"
#include "extkey.h"
#include "ext.h"
#include "conn.h"

/*
 * Helpers
 */
int
xpybConn_invalid(xpybConn *self)
{
    if (self->conn == NULL) {
	PyErr_SetString(xpybExcept_base, "Invalid connection.");
	return 1;
    }
    if (xcb_connection_has_error(self->conn)) {
	PyErr_SetString(xpybExcept_base, "An error has occurred on the connection.");
	return 1;
    }
    return 0;
}

xpybConn *
xpybConn_create(PyObject *core_type)
{
    xpybConn *self;

    self = PyObject_New(xpybConn, &xpybConn_type);
    if (self == NULL)
	return NULL;

    self->core = PyObject_CallFunctionObjArgs(core_type, self, NULL);
    if (self->core == NULL)
	goto err;

    self->dict = PyDict_New();
    if (self->dict == NULL)
	goto err;

    self->extcache = PyDict_New();
    if (self->extcache == NULL)
	goto err;

    self->wrapped = 0;
    self->setup = NULL;
    self->events = NULL;
    self->events_len = 0;
    self->errors = NULL;
    self->errors_len = 0;
    return self;

err:
    Py_DECREF(self);
    return NULL;
}

static xpybExt *
xpybConn_load_ext(xpybConn *self, PyObject *key)
{
    PyObject *type;
    xpybExt *ext;
    const xcb_query_extension_reply_t *reply;

    ext = (xpybExt *)PyDict_GetItem(self->extcache, key);
    Py_XINCREF(ext);

    if (ext == NULL) {
	/* Look up the extension type in the global dictionary. */
	type = PyDict_GetItem(xpybModule_extdict, key);
	if (type == NULL) {
	    PyErr_SetString(xpybExcept_ext, "No extension found for that key.");
	    return NULL;
	}

	/* Call the type object to get a new xcb.Extension object. */
	ext = (xpybExt *)PyObject_CallFunctionObjArgs(type, self, key, NULL);
	if (ext == NULL)
	    return NULL;

	/* Get the opcode and base numbers. */
	reply = xcb_get_extension_data(self->conn, &((xpybExtkey *)key)->key);
	ext->present = reply->present;
	ext->major_opcode = reply->major_opcode;
	ext->first_event = reply->first_event;
	ext->first_error = reply->first_error;

	if (PyDict_SetItem(self->extcache, key, (PyObject *)ext) < 0)
	    return NULL;
    }

    return ext;
}

static int
xpybConn_setup_helper(xpybConn *self, xpybExt *ext, PyObject *events, PyObject *errors)
{
    Py_ssize_t j = 0;
    unsigned char opcode, newlen;
    PyObject *num, *type, **newmem;

    while (PyDict_Next(events, &j, &num, &type)) {
	opcode = ext->first_event + PyInt_AS_LONG(num);
	if (opcode >= self->events_len) {
	    newlen = opcode + 1;
	    newmem = realloc(self->events, newlen * sizeof(PyObject *));
	    if (newmem == NULL)
		return -1;
	    memset(newmem + self->events_len, 0, (newlen - self->events_len) * sizeof(PyObject *));
	    self->events = newmem;
	    self->events_len = newlen;
	}
	Py_INCREF(self->events[opcode] = type);
    }

    j = 0;
    while (PyDict_Next(errors, &j, &num, &type)) {
	opcode = ext->first_error + PyInt_AS_LONG(num);
	if (opcode >= self->errors_len) {
	    newlen = opcode + 1;
	    newmem = realloc(self->errors, newlen * sizeof(PyObject *));
	    if (newmem == NULL)
		return -1;
	    memset(newmem + self->errors_len, 0, (newlen - self->errors_len) * sizeof(PyObject *));
	    self->errors = newmem;
	    self->errors_len = newlen;
	}
	Py_INCREF(self->errors[opcode] = type);
    }

    return 0;
}

int
xpybConn_setup(xpybConn *self)
{
    PyObject *key, *events, *errors;
    xpybExt *ext;
    Py_ssize_t i = 0;
    int rc = -1;

    ext = (xpybExt *)self->core;
    events = xpybModule_core_events;
    errors = xpybModule_core_errors;
    if (xpybConn_setup_helper(self, ext, events, errors) < 0)
	return -1;

    ext = NULL;
    while (PyDict_Next(xpybModule_ext_events, &i, &key, &events)) {
	errors = PyDict_GetItem(xpybModule_ext_errors, key);
	if (errors == NULL)
	    goto out;

	Py_XDECREF(ext);
	ext = xpybConn_load_ext(self, key);
	if (ext == NULL)
	    goto out;
	if (ext->present)
	    if (xpybConn_setup_helper(self, ext, events, errors) < 0)
		goto out;
    }

    rc = 0;
out:
    Py_XDECREF(ext);
    return rc;
}

/*
 * Infrastructure
 */

static PyObject *
xpybConn_new(PyTypeObject *self, PyObject *args, PyObject *kw)
{
    return PyType_GenericNew(self, args, kw);
}

static void
xpybConn_dealloc(xpybConn *self)
{
    int i;

    Py_CLEAR(self->dict);
    Py_CLEAR(self->core);
    Py_CLEAR(self->setup);
    Py_CLEAR(self->extcache);

    if (self->conn && !self->wrapped)
	xcb_disconnect(self->conn);

    for (i = 0; i < self->events_len; i++)
	Py_XDECREF(self->events[i]);
    for (i = 0; i < self->errors_len; i++)
	Py_XDECREF(self->errors[i]);

    free(self->events);
    free(self->errors);
    self->ob_type->tp_free((PyObject *)self);
}

static PyObject *
xpybConn_getattro(xpybConn *self, PyObject *obj)
{
    const char *name = PyString_AS_STRING(obj);
    PyMethodDef *mptr = xpybConn_type.tp_methods;
    PyMemberDef *sptr = xpybConn_type.tp_members;
    PyObject *result;

    while (mptr && mptr->ml_name)
	if (strcmp(name, (mptr++)->ml_name) == 0)
	    goto out2;
    while (sptr && sptr->name)
	if (strcmp(name, (sptr++)->name) == 0)
	    goto out2;
	
    Py_XINCREF(result = PyDict_GetItem(self->dict, obj));
    if (result != NULL || PyErr_Occurred())
	return result;

    return xpybConn_type.tp_base->tp_getattro((PyObject *)self, obj);
out2:
    return PyObject_GenericGetAttr((PyObject *)self, obj);
}

static int
xpybConn_setattro(xpybConn *self, PyObject *obj, PyObject *val)
{
    const char *name = PyString_AS_STRING(obj);
    PyMethodDef *mptr = xpybConn_type.tp_methods;
    PyMemberDef *sptr = xpybConn_type.tp_members;

    while (mptr && mptr->ml_name)
	if (strcmp(name, (mptr++)->ml_name) == 0)
	    goto out2;
    while (sptr && sptr->name)
	if (strcmp(name, (sptr++)->name) == 0)
	    goto out2;

    return val ? PyDict_SetItem(self->dict, obj, val) : PyDict_DelItem(self->dict, obj);
out2:
    return PyObject_GenericSetAttr((PyObject *)self, obj, val);
}

static PyObject *
xpybConn_call(xpybConn *self, PyObject *args, PyObject *kw)
{
    static char *kwlist[] = { "key", NULL };
    PyObject *key;
    xpybExt *ext;

    /* Parse the extension key argument and check connection. */
    if (!PyArg_ParseTupleAndKeywords(args, kw, "O!", kwlist, &xpybExtkey_type, &key))
	return NULL;
    if (xpybConn_invalid(self))
	return NULL;

    /* Check our dictionary of cached values */
    ext = xpybConn_load_ext(self, key);
    if (!ext->present) {
	PyErr_SetString(xpybExcept_ext, "Extension not present on server.");
	Py_DECREF(ext);
	return NULL;
    }

    return (PyObject *)ext;
}


/*
 * Members
 */

static PyMemberDef xpybConn_members[] = {
    { "pref_screen",
      T_INT,
      offsetof(xpybConn, pref_screen),
      READONLY,
      "Preferred display screen" },

    { "core",
      T_OBJECT,
      offsetof(xpybConn, core),
      READONLY,
      "Core protocol object" },

    { "__dict__",
      T_OBJECT,
      offsetof(xpybConn, dict),
      READONLY,
      "Instance dictionary object" },

    { NULL } /* terminator */
};


/*
 * Methods
 */

static PyObject *
xpybConn_has_error(xpybConn *self, PyObject *args)
{
    if (self->conn == NULL) {
	PyErr_SetString(xpybExcept_base, "Invalid connection.");
	return NULL;
    }

    return Py_BuildValue("i", xcb_connection_has_error(self->conn));
}

static PyObject *
xpybConn_get_file_descriptor(xpybConn *self, PyObject *args)
{
    if (xpybConn_invalid(self))
	return NULL;

    return Py_BuildValue("i", xcb_get_file_descriptor(self->conn));
}

static PyObject *
xpybConn_get_maximum_request_length(xpybConn *self, PyObject *args)
{
    if (xpybConn_invalid(self))
	return NULL;

    return Py_BuildValue("I", xcb_get_maximum_request_length(self->conn));
}

static PyObject *
xpybConn_prefetch_maximum_request_length(xpybConn *self, PyObject *args)
{
    if (xpybConn_invalid(self))
	return NULL;

    xcb_prefetch_maximum_request_length(self->conn);
    Py_RETURN_NONE;
}

static PyObject *
xpybConn_get_setup(xpybConn *self, PyObject *args)
{
    const xcb_setup_t *s;
    PyObject *shim, *type;

    if (xpybConn_invalid(self))
	return NULL;

    if (self->setup == NULL) {
	s = xcb_get_setup(self->conn);
	shim = PyBuffer_FromMemory((void *)s, 8 + s->length * 4);
	if (shim == NULL)
	    return NULL;
	type = (PyObject *)xpybModule_setup;
	self->setup = PyObject_CallFunctionObjArgs(type, shim, Py_False, NULL);
	Py_DECREF(shim);
    }

    Py_XINCREF(self->setup);
    return self->setup;
}

static PyObject *
xpybConn_wait_for_event(xpybConn *self, PyObject *args)
{
    xcb_generic_event_t *data;

    if (xpybConn_invalid(self))
	return NULL;

    data = xcb_wait_for_event(self->conn);

    if (data == NULL) {
	PyErr_SetString(PyExc_IOError, "I/O error on X server connection.");
	return NULL;
    }

    if (data->response_type == 0) {
	xpybError_set(self, (xcb_generic_error_t *)data);
	return NULL;
    }

    return xpybEvent_create(self, data);
}

static PyObject *
xpybConn_poll_for_event(xpybConn *self, PyObject *args)
{
    xcb_generic_event_t *data;

    if (xpybConn_invalid(self))
	return NULL;

    data = xcb_poll_for_event(self->conn);

    if (data == NULL) {
	PyErr_SetString(PyExc_IOError, "I/O error on X server connection.");
	return NULL;
    }

    if (data->response_type == 0) {
	xpybError_set(self, (xcb_generic_error_t *)data);
	return NULL;
    }

    return xpybEvent_create(self, data);
}

static PyObject *
xpybConn_flush(xpybConn *self, PyObject *args)
{
    if (xpybConn_invalid(self))
	return NULL;

    xcb_flush(self->conn);
    Py_RETURN_NONE;
}

static PyObject *
xpybConn_generate_id(xpybConn *self)
{
    unsigned int xid;

    if (xpybConn_invalid(self))
	return NULL;

    xid = xcb_generate_id(self->conn);
    if (xid == (unsigned int)-1) {
	PyErr_SetString(xpybExcept_base, "No more free XID's available.");
	return NULL;
    }

    return Py_BuildValue("I", xid);
}

static PyObject *
xpybConn_disconnect(xpybConn *self, PyObject *args)
{
    if (self->conn)
	xcb_disconnect(self->conn);
    self->conn = NULL;
    Py_RETURN_NONE;
}

static PyMethodDef xpybConn_methods[] = {
    { "has_error",
      (PyCFunction)xpybConn_has_error,
      METH_NOARGS,
      "Test whether the connection has shut down due to a fatal error." },

    { "get_file_descriptor",
      (PyCFunction)xpybConn_get_file_descriptor,
      METH_NOARGS,
      "Access the file descriptor of the connection." },

    { "get_maximum_request_length",
      (PyCFunction)xpybConn_get_maximum_request_length,
      METH_NOARGS,
      "Returns the maximum request length that this server accepts." },

    { "prefetch_maximum_request_length",
      (PyCFunction)xpybConn_prefetch_maximum_request_length,
      METH_NOARGS,
      "Prefetch the maximum request length without blocking." },

    { "get_setup",
      (PyCFunction)xpybConn_get_setup,
      METH_NOARGS,
      "Accessor for the connection information returned by the server." },

    { "wait_for_event",
      (PyCFunction)xpybConn_wait_for_event,
      METH_NOARGS,
      "Returns the next event or raises the next error from the server." },

    { "poll_for_event",
      (PyCFunction)xpybConn_poll_for_event,
      METH_NOARGS,
      "Returns the next event or raises the next error from the server." },

    { "flush",
      (PyCFunction)xpybConn_flush,
      METH_NOARGS,
      "Forces any buffered output to be written to the server." },

    { "generate_id",
      (PyCFunction)xpybConn_generate_id,
      METH_NOARGS,
      "Allocates an XID for a new object." },

    { "disconnect",
      (PyCFunction)xpybConn_disconnect,
      METH_NOARGS,
      "Disconnects from the X server." },

    { NULL } /* terminator */
};


/*
 * Definition
 */

PyTypeObject xpybConn_type = {
    PyObject_HEAD_INIT(NULL)
    .tp_name = "xcb.Connection",
    .tp_basicsize = sizeof(xpybConn),
    .tp_new = xpybConn_new,
    .tp_dealloc = (destructor)xpybConn_dealloc,
    .tp_flags = Py_TPFLAGS_DEFAULT,
    .tp_doc = "XCB connection object",
    .tp_methods = xpybConn_methods,
    .tp_members = xpybConn_members,
    .tp_call = (ternaryfunc)xpybConn_call,
    .tp_getattro = (getattrofunc)xpybConn_getattro,
    .tp_setattro = (setattrofunc)xpybConn_setattro
};


/*
 * Module init
 */
int xpybConn_modinit(PyObject *m)
{
    if (PyType_Ready(&xpybConn_type) < 0)
        return -1;
    Py_INCREF(&xpybConn_type);
    if (PyModule_AddObject(m, "Connection", (PyObject *)&xpybConn_type) < 0)
	return -1;

    return 0;
}
