#include "module.h"
#include "except.h"
#include "response.h"
#include "event.h"

/*
 * Helpers
 */

PyObject *
xpybEvent_create(xpybConn *conn, xcb_generic_event_t *e)
{
    unsigned char opcode = e->response_type;
    PyObject *shim, *event, *type = (PyObject *)&xpybEvent_type;

    if (opcode < conn->events_len && conn->events[opcode] != NULL)
	type = conn->events[opcode];

    shim = PyBuffer_FromMemory(e, sizeof(*e));
    if (shim == NULL)
	return NULL;

    event = PyObject_CallFunctionObjArgs(type, shim, NULL);
    Py_DECREF(shim);
    return event;
}


/*
 * Infrastructure
 */


/*
 * Members
 */


/*
 * Definition
 */

PyTypeObject xpybEvent_type = {
    PyObject_HEAD_INIT(NULL)
    .tp_name = "xcb.Event",
    .tp_basicsize = sizeof(xpybEvent),
    .tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
    .tp_doc = "XCB generic event object",
    .tp_base = &xpybResponse_type
};


/*
 * Module init
 */
int xpybEvent_modinit(PyObject *m)
{
    if (PyType_Ready(&xpybEvent_type) < 0)
        return -1;
    Py_INCREF(&xpybEvent_type);
    if (PyModule_AddObject(m, "Event", (PyObject *)&xpybEvent_type) < 0)
	return -1;

    return 0;
}
