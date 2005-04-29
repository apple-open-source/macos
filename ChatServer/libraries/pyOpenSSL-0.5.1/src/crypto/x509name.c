/*
 * x509name.c
 *
 * Copyright (C) AB Strakt 2001, All rights reserved
 *
 * X.509 Name handling, mostly thin wrapping.
 * See the file RATIONALE for a short explanation of why this module was written.
 *
 * Reviewed 2001-07-23
 */
#include <Python.h>
#define crypto_MODULE
#include "crypto.h"

static char *CVSid = "@(#) $Id: x509name.c,v 1.2 2004/09/23 14:25:28 murata Exp $";


/*
 * Constructor for X509Name, never called by Python code directly
 *
 * Arguments: name    - A "real" X509_NAME object
 *            dealloc - Boolean value to specify whether the destructor should
 *                      free the "real" X509_NAME object
 * Returns:   The newly created X509Name object
 */
crypto_X509NameObj *
crypto_X509Name_New(X509_NAME *name, int dealloc)
{
    crypto_X509NameObj *self;

    self = PyObject_New(crypto_X509NameObj, &crypto_X509Name_Type);

    if (self == NULL)
        return NULL;

    self->x509_name = name;
    self->dealloc = dealloc;
    self->parent_cert = NULL;

    return self;
}

/*
 * Deallocate the memory used by the X509Name object
 *
 * Arguments: self - The X509Name object
 * Returns:   None
 */
static void
crypto_X509Name_dealloc(crypto_X509NameObj *self)
{
    /* Sometimes we don't have to dealloc this */
    if (self->dealloc)
        X509_NAME_free(self->x509_name);

    Py_XDECREF(self->parent_cert);

    PyObject_Del(self);
}

/*
 * Return a name string given a X509_NAME object and a name identifier. Used
 * by the getattr function.
 *
 * Arguments: name - The X509_NAME object
 *            nid  - The name identifier
 * Returns:   The name as a Python string object
 */
static PyObject *
get_name_by_nid(X509_NAME *name, int nid)
{
    void *buf;
    int len, xlen;
    PyObject *str;

    if ((len = X509_NAME_get_text_by_NID(name, nid, NULL, 0)) == -1)
    {
        Py_INCREF(Py_None);
        return Py_None;
    }

    len++;
    buf = PyMem_Malloc(len);
    if (buf == NULL)
        return NULL;

    xlen = X509_NAME_get_text_by_NID(name, nid, buf, len);
    str = PyString_FromStringAndSize(buf, xlen);
    PyMem_Free(buf);

    return str;
}

/*
 * Given a X509_NAME object and a name identifier, set the corresponding
 * attribute to the given string. Used by the setattr function.
 *
 * Arguments: name  - The X509_NAME object
 *            nid   - The name identifier
 *            value - The string to set
 * Returns:   0 for success, -1 on failure
 */
static int
set_name_by_nid(X509_NAME *name, int nid, PyObject *value)
{
    X509_NAME_ENTRY *ne;
    int i, entry_count, temp_nid;

    /* If there's an old entry for this NID, remove it */
    entry_count = X509_NAME_entry_count(name);
    for (i = 0; i < entry_count; i++)
    {
        ne = X509_NAME_get_entry(name, i);
        temp_nid = OBJ_obj2nid(X509_NAME_ENTRY_get_object(ne));
        if (temp_nid == nid)
        {
            ne = X509_NAME_delete_entry(name, i);
            X509_NAME_ENTRY_free(ne);
            break;
        }
    }

    /* Add the new entry */
    if (!X509_NAME_add_entry_by_NID(name, nid, MBSTRING_ASC,
            PyString_AsString(value), -1, -1, 0))
    {
        exception_from_error_queue();
        return -1;
    }
    return 0;
}


/*
 * Find attribute. An X509Name object has the following attributes:
 * countryName (alias C), stateOrProvince (alias ST), locality (alias L),
 * organization (alias O), organizationalUnit (alias OU), commonName (alias
 * CN) and more...
 *
 * Arguments: self - The X509Name object
 *            name - The attribute name
 * Returns:   A Python object for the attribute, or NULL if something went
 *            wrong
 */
static PyObject *
crypto_X509Name_getattr(crypto_X509NameObj *self, char *name)
{
    int nid;

    if ((nid = OBJ_txt2nid(name)) == NID_undef)
    {
        PyErr_SetString(PyExc_AttributeError, "No such attribute");
        return NULL;
    }

    return get_name_by_nid(self->x509_name, nid);
}

/*
 * Set attribute
 *
 * Arguments: self  - The X509Name object
 *            name  - The attribute name
 *            value - The value to set
 */
static int
crypto_X509Name_setattr(crypto_X509NameObj *self, char *name, PyObject *value)
{
    PyObject *strval;
    int nid;

    if ((nid = OBJ_txt2nid(name)) == NID_undef)
    {
        PyErr_SetString(PyExc_AttributeError, "No such attribute");
        return -1;
    }
    
    strval = PyObject_Str(value);
    if (strval == NULL)
        return -1;

    return set_name_by_nid(self->x509_name, nid, strval);
}

/*
 * Compare two X509Name structures.
 *
 * Arguments: n - The first X509Name
 *            m - The second X509Name
 * Returns:   <0 if n < m, 0 if n == m and >0 if n > m
 */
static int
crypto_X509Name_compare(crypto_X509NameObj *n, crypto_X509NameObj *m)
{
    return X509_NAME_cmp(n->x509_name, m->x509_name);
}

/*
 * String representation of an X509Name
 *
 * Arguments: self - The X509Name object
 * Returns:   A string representation of the object
 */
static PyObject *
crypto_X509Name_repr(crypto_X509NameObj *self)
{
    char tmpbuf[512] = "";
    char realbuf[512+64];

    if (X509_NAME_oneline(self->x509_name, tmpbuf, 512) == NULL)
    {
        exception_from_error_queue();
        return NULL;
    }
    else
    {
        /* This is safe because tmpbuf is max 512 characters */
        sprintf(realbuf, "<X509Name object '%s'>", tmpbuf);
        return PyString_FromString(realbuf);
    }
}

PyTypeObject crypto_X509Name_Type = {
    PyObject_HEAD_INIT(NULL)
    0,
    "X509Name",
    sizeof(crypto_X509NameObj),
    0,
    (destructor)crypto_X509Name_dealloc,
    NULL, /* print */
    (getattrfunc)crypto_X509Name_getattr,
    (setattrfunc)crypto_X509Name_setattr,
    (cmpfunc)crypto_X509Name_compare,
    (reprfunc)crypto_X509Name_repr,
    NULL, /* as_number */
    NULL, /* as_sequence */
    NULL, /* as_mapping */
    NULL  /* hash */
};


/*
 * Initialize the X509Name part of the crypto module
 *
 * Arguments: dict - The crypto module dictionary
 * Returns:   None
 */
int
init_crypto_x509name(PyObject *dict)
{
    crypto_X509Name_Type.ob_type = &PyType_Type;
    Py_INCREF(&crypto_X509Name_Type);
    PyDict_SetItemString(dict, "X509NameType", (PyObject *)&crypto_X509Name_Type);
    return 1;
}
