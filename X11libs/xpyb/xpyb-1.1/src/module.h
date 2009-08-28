#ifndef XPYB_MODULE_H
#define XPYB_MODULE_H

#include <Python.h>
#include <structmember.h>

#include <xcb/xcb.h>
#include <xcb/xcbext.h>

extern PyTypeObject *xpybModule_core;
extern PyTypeObject *xpybModule_setup;
extern PyObject *xpybModule_core_events;
extern PyObject *xpybModule_core_errors;

extern PyObject *xpybModule_extdict;
extern PyObject *xpybModule_ext_events;
extern PyObject *xpybModule_ext_errors;

PyMODINIT_FUNC initxcb(void);

#endif
