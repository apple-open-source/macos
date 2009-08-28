#include "module.h"
#include "except.h"
#include "cookie.h"
#include "void.h"

/*
 * Helpers
 */


/*
 * Infrastructure
 */


/*
 * Members
 */


/*
 * Methods
 */


/*
 * Definition
 */

PyTypeObject xpybVoid_type = {
    PyObject_HEAD_INIT(NULL)
    .tp_name = "xcb.VoidCookie",
    .tp_basicsize = sizeof(xpybVoid),
    .tp_flags = Py_TPFLAGS_DEFAULT,
    .tp_doc = "XCB void cookie object",
    .tp_base = &xpybCookie_type
};


/*
 * Module init
 */
int xpybVoid_modinit(PyObject *m)
{
    if (PyType_Ready(&xpybVoid_type) < 0)
        return -1;
    Py_INCREF(&xpybVoid_type);
    if (PyModule_AddObject(m, "VoidCookie", (PyObject *)&xpybVoid_type) < 0)
	return -1;

    return 0;
}
