#include "module.h"
#include "except.h"
#include "constant.h"
#include "cookie.h"
#include "protobj.h"
#include "response.h"
#include "event.h"
#include "error.h"
#include "reply.h"
#include "request.h"
#include "struct.h"
#include "union.h"
#include "list.h"
#include "iter.h"
#include "conn.h"
#include "extkey.h"
#include "ext.h"
#include "void.h"


/*
 * Globals
 */

PyTypeObject *xpybModule_core;
PyTypeObject *xpybModule_setup;
PyObject *xpybModule_core_events;
PyObject *xpybModule_core_errors;

PyObject *xpybModule_extdict;
PyObject *xpybModule_ext_events;
PyObject *xpybModule_ext_errors;


/*
 * Helpers
 */

static int
xpyb_parse_auth(const char *authstr, int authlen, xcb_auth_info_t *auth)
{
    int i = 0;

    while (i < authlen && authstr[i] != ':')
	i++;

    if (i >= authlen) {
	PyErr_SetString(xpybExcept_base, "Auth string must take the form '<name>:<data>'.");
	return -1;
    }

    auth->name = (char *)authstr;
    auth->namelen = i++;
    auth->data = (char *)authstr + i;
    auth->datalen = authlen - i;
    return 0;
}

/*
 * Module functions
 */

static PyObject *
xpyb_connect(PyObject *self, PyObject *args, PyObject *kw)
{
    static char *kwlist[] = { "display", "fd", "auth", NULL };
    const char *displayname = NULL, *authstr = NULL;
    xcb_auth_info_t auth, *authptr = NULL;
    xpybConn *conn;
    int authlen, fd = -1;

    /* Make sure core was set. */
    if (xpybModule_core == NULL) {
	PyErr_SetString(xpybExcept_base, "No core protocol object has been set.  Did you import xcb.xproto?");
	return NULL;
    }

    /* Parse arguments and allocate new connection object */
    if (!PyArg_ParseTupleAndKeywords(args, kw, "|ziz#", kwlist, &displayname,
				     &fd, &authstr, &authlen))
	return NULL;

    conn = xpybConn_create((PyObject *)xpybModule_core);
    if (conn == NULL)
	return NULL;

    /* Set up authorization */
    if (authstr != NULL) {
	if (xpyb_parse_auth(authstr, authlen, &auth) < 0)
	    goto err;
	authptr = &auth;
    }

    /* Connect to display */
    if (fd >= 0)
	conn->conn = xcb_connect_to_fd(fd, authptr);
    else if (authptr)
	conn->conn = xcb_connect_to_display_with_auth_info(displayname, authptr, &conn->pref_screen);
    else
	conn->conn = xcb_connect(displayname, &conn->pref_screen);

    if (xcb_connection_has_error(conn->conn)) {
	PyErr_SetString(xpybExcept_conn, "Failed to connect to X server.");
	goto err;
    }

    /* Load extensions */
    if (xpybConn_setup(conn) < 0)
	goto err;

    return (PyObject *)conn;
err:
    Py_DECREF(conn);
    return NULL;
}

static PyObject *
xpyb_wrap(PyObject *self, PyObject *args)
{
    PyObject *obj;
    void *raw;
    xpybConn *conn;

    /* Make sure core was set. */
    if (xpybModule_core == NULL) {
	PyErr_SetString(xpybExcept_base, "No core protocol object has been set.  Did you import xcb.xproto?");
	return NULL;
    }

    /* Parse arguments and allocate new connection object */
    if (!PyArg_ParseTuple(args, "O", &obj))
	return NULL;

    conn = xpybConn_create((PyObject *)xpybModule_core);
    if (conn == NULL)
	return NULL;

    /* Get our pointer */
    raw = PyLong_AsVoidPtr(obj);
    if (!raw || PyErr_Occurred()) {
	PyErr_SetString(xpybExcept_base, "Bad pointer value passed to wrap().");
	goto err;
    }

    conn->conn = raw;
    conn->wrapped = 1;

    /* Load extensions */
    if (xpybConn_setup(conn) < 0)
	goto err;

    return (PyObject *)conn;
err:
    Py_DECREF(conn);
    return NULL;
}
static PyObject *
xpyb_add_core(PyObject *self, PyObject *args)
{
    PyTypeObject *value, *setup;
    PyObject *events, *errors;

    if (xpybModule_core != NULL)
	Py_RETURN_NONE;

    if (!PyArg_ParseTuple(args, "O!O!O!O!", &PyType_Type, &value, &PyType_Type, &setup,
			  &PyDict_Type, &events, &PyDict_Type, &errors))
	return NULL;

    if (!PyType_IsSubtype(value, &xpybExt_type)) {
	PyErr_SetString(xpybExcept_base, "Extension type not derived from xcb.Extension.");
	return NULL;
    }
    if (!PyType_IsSubtype(setup, &xpybStruct_type)) {
	PyErr_SetString(xpybExcept_base, "Setup type not derived from xcb.Struct.");
	return NULL;
    }

    Py_INCREF(xpybModule_core = value);
    Py_INCREF(xpybModule_core_events = events);
    Py_INCREF(xpybModule_core_errors = errors);
    Py_INCREF(xpybModule_setup = setup);
    Py_RETURN_NONE;
}

static PyObject *
xpyb_add_ext(PyObject *self, PyObject *args)
{
    PyTypeObject *value;
    PyObject *key, *events, *errors;

    if (!PyArg_ParseTuple(args, "O!O!O!O!", &xpybExtkey_type, &key, &PyType_Type, &value,
			  &PyDict_Type, &events, &PyDict_Type, &errors))
	return NULL;

    if (!PyType_IsSubtype(value, &xpybExt_type)) {
	PyErr_SetString(xpybExcept_base, "Extension type not derived from xcb.Extension.");
	return NULL;
    }

    if (PyDict_SetItem(xpybModule_extdict, key, (PyObject *)value) < 0)
	return NULL;
    if (PyDict_SetItem(xpybModule_ext_events, key, events) < 0)
	return NULL;
    if (PyDict_SetItem(xpybModule_ext_errors, key, errors) < 0)
	return NULL;

    Py_RETURN_NONE;
}

static PyObject *
xpyb_resize_obj(PyObject *self, PyObject *args)
{
    xpybProtobj *obj;
    Py_ssize_t size;
    PyObject *buf;

    if (!PyArg_ParseTuple(args, "O!n", &xpybProtobj_type, &obj, &size))
	return NULL;

    buf = PyBuffer_FromObject(obj->buf, 0, size);
    if (buf == NULL)
	return NULL;

    Py_CLEAR(obj->buf);
    obj->buf = buf;

    Py_RETURN_NONE;
}

static PyObject *
xpyb_popcount(PyObject *self, PyObject *args)
{
    unsigned int i;

    if (!PyArg_ParseTuple(args, "I", &i))
	return NULL;

    return Py_BuildValue("I", xcb_popcount(i));
}

static PyObject *
xpyb_type_pad(PyObject *self, PyObject *args)
{
    unsigned int i, t;

    if (!PyArg_ParseTuple(args, "II", &t, &i))
	return NULL;

    return Py_BuildValue("I", -i & (t > 4 ? 3 : t - 1));
}


static PyMethodDef XCBMethods[] = {
    { "connect",
      (PyCFunction)xpyb_connect,
      METH_VARARGS | METH_KEYWORDS,
      "Connects to the X server." },

    { "wrap",
      (PyCFunction)xpyb_wrap,
      METH_VARARGS,
      "Wraps an existing XCB connection pointer." },

    { "popcount",
      (PyCFunction)xpyb_popcount,
      METH_VARARGS,
      "Counts number of bits set in a bitmask." },

    { "type_pad",
      (PyCFunction)xpyb_type_pad,
      METH_VARARGS,
      "Returns number of padding bytes needed for a type size." },

    { "_add_core",
      (PyCFunction)xpyb_add_core,
      METH_VARARGS,
      "Registers the core protocol class.  Not meant for end users." },

    { "_add_ext",
      (PyCFunction)xpyb_add_ext,
      METH_VARARGS,
      "Registers a new extension protocol class.  Not meant for end users." },

    { "_resize_obj",
      (PyCFunction)xpyb_resize_obj,
      METH_VARARGS,
      "Sizes a protocol object after size determination.  Not meant for end users." },

    { NULL } /* terminator */
};


/*
 * Module init
 */

PyMODINIT_FUNC
initxcb(void) 
{
    /* Create module object */
    PyObject *m = Py_InitModule3("xcb", XCBMethods, "XCB Python Binding.");
    if (m == NULL)
	return;

    /* Create other internal objects */
    if ((xpybModule_extdict = PyDict_New()) == NULL)
	return;
    if ((xpybModule_ext_events = PyDict_New()) == NULL)
	return;
    if ((xpybModule_ext_errors = PyDict_New()) == NULL)
	return;

    /* Add integer constants */
    if (xpybConstant_modinit(m) < 0)
	return;

    /* Set up all the types */
    if (xpybExcept_modinit(m) < 0)
	return;
    if (xpybConn_modinit(m) < 0)
	return;
    if (xpybCookie_modinit(m) < 0)
	return;

    if (xpybExtkey_modinit(m) < 0)
	return;
    if (xpybExt_modinit(m) < 0)
	return;

    if (xpybProtobj_modinit(m) < 0)
	return;
    if (xpybResponse_modinit(m) < 0)
	return;
    if (xpybEvent_modinit(m) < 0)
	return;
    if (xpybError_modinit(m) < 0)
	return;
    if (xpybReply_modinit(m) < 0)
	return;
    if (xpybRequest_modinit(m) < 0)
	return;
    if (xpybStruct_modinit(m) < 0)
	return;
    if (xpybUnion_modinit(m) < 0)
	return;

    if (xpybList_modinit(m) < 0)
	return;
    if (xpybIter_modinit(m) < 0)
	return;

    if (xpybVoid_modinit(m) < 0)
	return;
}
