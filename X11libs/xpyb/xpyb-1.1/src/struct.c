#include "module.h"
#include "except.h"
#include "struct.h"

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
 * Definition
 */

PyTypeObject xpybStruct_type = {
    PyObject_HEAD_INIT(NULL)
    .tp_name = "xcb.Struct",
    .tp_basicsize = sizeof(xpybStruct),
    .tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
    .tp_doc = "XCB generic struct object",
    .tp_base = &xpybProtobj_type,
};


/*
 * Module init
 */
int xpybStruct_modinit(PyObject *m)
{
    if (PyType_Ready(&xpybStruct_type) < 0)
        return -1;
    Py_INCREF(&xpybStruct_type);
    if (PyModule_AddObject(m, "Struct", (PyObject *)&xpybStruct_type) < 0)
	return -1;

    return 0;
}
