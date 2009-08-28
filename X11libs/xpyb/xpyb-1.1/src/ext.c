#include "module.h"
#include "except.h"
#include "extkey.h"
#include "ext.h"
#include "conn.h"
#include "cookie.h"
#include "reply.h"
#include "request.h"

/*
 * Helpers
 */


/*
 * Infrastructure
 */

static PyObject *
xpybExt_new(PyTypeObject *self, PyObject *args, PyObject *kw)
{
    return PyType_GenericNew(self, args, kw);
}

static int
xpybExt_init(xpybExt *self, PyObject *args)
{
    xpybConn *conn;
    xpybExtkey *key = (xpybExtkey *)Py_None;

    if (!PyArg_ParseTuple(args, "O!|O!", &xpybConn_type, &conn, &xpybExtkey_type, &key))
	return -1;

    Py_INCREF(self->key = key);
    Py_INCREF(self->conn = conn);
    return 0;
}

static void
xpybExt_dealloc(xpybExt *self)
{
    Py_CLEAR(self->key);
    Py_CLEAR(self->conn);
}

/*
 * Members
 */

static PyMemberDef xpybExt_members[] = {
    { "key",
      T_OBJECT,
      offsetof(xpybExt, key),
      READONLY,
      "Extension identifier key object" },

    { "conn",
      T_OBJECT,
      offsetof(xpybExt, conn),
      READONLY,
      "Connection object open to X server" },

    { "major_opcode",
      T_UBYTE,
      offsetof(xpybExt, major_opcode),
      READONLY,
      "Major opcode of the extension" },

    { "first_event",
      T_UBYTE,
      offsetof(xpybExt, first_event),
      READONLY,
      "Event base of the extension" },

    { "first_error",
      T_UBYTE,
      offsetof(xpybExt, first_error),
      READONLY,
      "Error base of the extension" },

    { NULL } /* terminator */
};


/*
 * Methods
 */

static PyObject *
xpybExt_send_request(xpybExt *self, PyObject *args, PyObject *kw)
{
    static char *kwlist[] = { "request", "cookie", "reply", NULL };
    xpybRequest *request;
    xpybCookie *cookie;
    PyTypeObject *reply = NULL;
    xcb_protocol_request_t xcb_req;
    struct iovec xcb_parts[4];
    unsigned int seq;
    int flags;
    const void *data;
    Py_ssize_t size;

    /* Parse and check arguments */
    if (!PyArg_ParseTupleAndKeywords(args, kw, "O!O!|O!", kwlist,
				     &xpybRequest_type, &request,
				     &xpybCookie_type, &cookie,
				     &PyType_Type, &reply))
	return NULL;

    if (!request->is_void)
	if (reply == NULL || !PyType_IsSubtype(reply, &xpybReply_type)) {
	    PyErr_SetString(xpybExcept_base, "Reply type missing or not derived from xcb.Reply.");
	    return NULL;
	}

    /* Set up request structure */
    xcb_req.count = 2;
    xcb_req.ext = (self->key != (xpybExtkey *)Py_None) ? &self->key->key : 0;
    xcb_req.opcode = request->opcode;
    xcb_req.isvoid = request->is_void;

    /* Allocate and fill in data strings */
    if (PyObject_AsReadBuffer(((xpybProtobj *)request)->buf, &data, &size) < 0)
	return NULL;
    xcb_parts[2].iov_base = (void *)data;
    xcb_parts[2].iov_len = size;
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    /* Make request call */
    flags = request->is_checked ? XCB_REQUEST_CHECKED : 0;
    seq = xcb_send_request(self->conn->conn, flags, xcb_parts + 2, &xcb_req);

    /* Set up cookie */
    Py_INCREF(cookie->conn = self->conn);
    Py_INCREF((PyObject *)(cookie->request = request));
    Py_XINCREF(cookie->reply = reply);
    cookie->cookie.sequence = seq;

    Py_INCREF(cookie);
    return (PyObject *)cookie;
}

static PyMethodDef xpybExt_methods[] = {
    { "send_request",
      (PyCFunction)xpybExt_send_request,
      METH_VARARGS | METH_KEYWORDS,
      "Sends a request to the X server." },

    { NULL } /* terminator */
};


/*
 * Definition
 */

PyTypeObject xpybExt_type = {
    PyObject_HEAD_INIT(NULL)
    .tp_name = "xcb.Extension",
    .tp_basicsize = sizeof(xpybExt),
    .tp_init = (initproc)xpybExt_init,
    .tp_new = xpybExt_new,
    .tp_dealloc = (destructor)xpybExt_dealloc,
    .tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
    .tp_doc = "XCB extension object",
    .tp_members = xpybExt_members,
    .tp_methods = xpybExt_methods
};


/*
 * Module init
 */
int xpybExt_modinit(PyObject *m)
{
    if (PyType_Ready(&xpybExt_type) < 0)
        return -1;
    Py_INCREF(&xpybExt_type);
    if (PyModule_AddObject(m, "Extension", (PyObject *)&xpybExt_type) < 0)
	return -1;

    return 0;
}
