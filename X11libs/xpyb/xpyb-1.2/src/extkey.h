#ifndef XPYB_EXTKEY_H
#define XPYB_EXTKEY_H

typedef struct {
    PyObject_HEAD
    PyStringObject *name;
    xcb_extension_t key;
} xpybExtkey;

extern PyTypeObject xpybExtkey_type;

int xpybExtkey_modinit(PyObject *m);

#endif
