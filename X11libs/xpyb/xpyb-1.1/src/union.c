#include "module.h"
#include "except.h"
#include "union.h"

/*
 * Helpers
 */


/*
 * Infraunionure
 */


/*
 * Members
 */


/*
 * Definition
 */

PyTypeObject xpybUnion_type = {
    PyObject_HEAD_INIT(NULL)
    .tp_name = "xcb.Union",
    .tp_basicsize = sizeof(xpybUnion),
    .tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
    .tp_doc = "XCB generic union object",
    .tp_base = &xpybProtobj_type,
};


/*
 * Module init
 */
int xpybUnion_modinit(PyObject *m)
{
    if (PyType_Ready(&xpybUnion_type) < 0)
        return -1;
    Py_INCREF(&xpybUnion_type);
    if (PyModule_AddObject(m, "Union", (PyObject *)&xpybUnion_type) < 0)
	return -1;

    return 0;
}
