/* Generated */

#include <Python.h>
#include <libxml/xmlversion.h>
#include <libxml/tree.h>
#include "libxml_wrap.h"
#include "libxml2-py.h"

PyObject *
libxml_xmlUCSIsBlockElements(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    int c_retval;
    int code;

    if (!PyArg_ParseTuple(args, (char *)"i:xmlUCSIsBlockElements", &code))
        return(NULL);

    c_retval = xmlUCSIsBlockElements(code);
    py_retval = libxml_intWrap((int) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlLoadCatalogs(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    char * pathss;

    if (!PyArg_ParseTuple(args, (char *)"z:xmlLoadCatalogs", &pathss))
        return(NULL);

    xmlLoadCatalogs(pathss);
    Py_INCREF(Py_None);
    return(Py_None);
}

PyObject *
libxml_xmlGetDocEntity(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    xmlEntityPtr c_retval;
    xmlDocPtr doc;
    PyObject *pyobj_doc;
    xmlChar * name;

    if (!PyArg_ParseTuple(args, (char *)"Oz:xmlGetDocEntity", &pyobj_doc, &name))
        return(NULL);
    doc = (xmlDocPtr) PyxmlNode_Get(pyobj_doc);

    c_retval = xmlGetDocEntity(doc, name);
    py_retval = libxml_xmlNodePtrWrap((xmlNodePtr) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlUCSIsBopomofo(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    int c_retval;
    int code;

    if (!PyArg_ParseTuple(args, (char *)"i:xmlUCSIsBopomofo", &code))
        return(NULL);

    c_retval = xmlUCSIsBopomofo(code);
    py_retval = libxml_intWrap((int) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlNodeGetBase(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    xmlChar * c_retval;
    xmlDocPtr doc;
    PyObject *pyobj_doc;
    xmlNodePtr cur;
    PyObject *pyobj_cur;

    if (!PyArg_ParseTuple(args, (char *)"OO:xmlNodeGetBase", &pyobj_doc, &pyobj_cur))
        return(NULL);
    doc = (xmlDocPtr) PyxmlNode_Get(pyobj_doc);
    cur = (xmlNodePtr) PyxmlNode_Get(pyobj_cur);

    c_retval = xmlNodeGetBase(doc, cur);
    py_retval = libxml_xmlCharPtrWrap((xmlChar *) c_retval);
    return(py_retval);
}

#ifdef LIBXML_XPATH_ENABLED
PyObject *
libxml_xmlXPathNextAncestorOrSelf(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    xmlNodePtr c_retval;
    xmlXPathParserContextPtr ctxt;
    PyObject *pyobj_ctxt;
    xmlNodePtr cur;
    PyObject *pyobj_cur;

    if (!PyArg_ParseTuple(args, (char *)"OO:xmlXPathNextAncestorOrSelf", &pyobj_ctxt, &pyobj_cur))
        return(NULL);
    ctxt = (xmlXPathParserContextPtr) PyxmlXPathParserContext_Get(pyobj_ctxt);
    cur = (xmlNodePtr) PyxmlNode_Get(pyobj_cur);

    c_retval = xmlXPathNextAncestorOrSelf(ctxt, cur);
    py_retval = libxml_xmlNodePtrWrap((xmlNodePtr) c_retval);
    return(py_retval);
}

#endif /* LIBXML_XPATH_ENABLED */
PyObject *
libxml_xmlStrstr(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    const xmlChar * c_retval;
    xmlChar * str;
    xmlChar * val;

    if (!PyArg_ParseTuple(args, (char *)"zz:xmlStrstr", &str, &val))
        return(NULL);

    c_retval = xmlStrstr(str, val);
    py_retval = libxml_xmlCharPtrConstWrap((const xmlChar *) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlGetCompressMode(ATTRIBUTE_UNUSED PyObject *self,ATTRIBUTE_UNUSED  PyObject *args) {
    PyObject *py_retval;
    int c_retval;

    c_retval = xmlGetCompressMode();
    py_retval = libxml_intWrap((int) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlUCSIsCJKUnifiedIdeographsExtensionB(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    int c_retval;
    int code;

    if (!PyArg_ParseTuple(args, (char *)"i:xmlUCSIsCJKUnifiedIdeographsExtensionB", &code))
        return(NULL);

    c_retval = xmlUCSIsCJKUnifiedIdeographsExtensionB(code);
    py_retval = libxml_intWrap((int) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlParseChunk(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    int c_retval;
    xmlParserCtxtPtr ctxt;
    PyObject *pyobj_ctxt;
    char * chunk;
    int size;
    int terminate;

    if (!PyArg_ParseTuple(args, (char *)"Ozii:xmlParseChunk", &pyobj_ctxt, &chunk, &size, &terminate))
        return(NULL);
    ctxt = (xmlParserCtxtPtr) PyparserCtxt_Get(pyobj_ctxt);

    c_retval = xmlParseChunk(ctxt, chunk, size, terminate);
    py_retval = libxml_intWrap((int) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlTextReaderCurrentDoc(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    xmlDocPtr c_retval;
    xmlTextReaderPtr reader;
    PyObject *pyobj_reader;

    if (!PyArg_ParseTuple(args, (char *)"O:xmlTextReaderCurrentDoc", &pyobj_reader))
        return(NULL);
    reader = (xmlTextReaderPtr) PyxmlTextReader_Get(pyobj_reader);

    c_retval = xmlTextReaderCurrentDoc(reader);
    py_retval = libxml_xmlDocPtrWrap((xmlDocPtr) c_retval);
    return(py_retval);
}

#ifdef LIBXML_HTML_ENABLED
PyObject *
libxml_htmlNewDocNoDtD(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    htmlDocPtr c_retval;
    xmlChar * URI;
    xmlChar * ExternalID;

    if (!PyArg_ParseTuple(args, (char *)"zz:htmlNewDocNoDtD", &URI, &ExternalID))
        return(NULL);

    c_retval = htmlNewDocNoDtD(URI, ExternalID);
    py_retval = libxml_xmlDocPtrWrap((xmlDocPtr) c_retval);
    return(py_retval);
}

#endif /* LIBXML_HTML_ENABLED */
#ifdef LIBXML_XPATH_ENABLED
PyObject *
libxml_xmlXPathNewString(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    xmlXPathObjectPtr c_retval;
    xmlChar * val;

    if (!PyArg_ParseTuple(args, (char *)"z:xmlXPathNewString", &val))
        return(NULL);

    c_retval = xmlXPathNewString(val);
    py_retval = libxml_xmlXPathObjectPtrWrap((xmlXPathObjectPtr) c_retval);
    return(py_retval);
}

#endif /* LIBXML_XPATH_ENABLED */
PyObject *
libxml_xmlUCSIsHangulJamo(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    int c_retval;
    int code;

    if (!PyArg_ParseTuple(args, (char *)"i:xmlUCSIsHangulJamo", &code))
        return(NULL);

    c_retval = xmlUCSIsHangulJamo(code);
    py_retval = libxml_intWrap((int) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlTextReaderXmlLang(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    xmlChar * c_retval;
    xmlTextReaderPtr reader;
    PyObject *pyobj_reader;

    if (!PyArg_ParseTuple(args, (char *)"O:xmlTextReaderXmlLang", &pyobj_reader))
        return(NULL);
    reader = (xmlTextReaderPtr) PyxmlTextReader_Get(pyobj_reader);

    c_retval = xmlTextReaderXmlLang(reader);
    py_retval = libxml_xmlCharPtrWrap((xmlChar *) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlAddSibling(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    xmlNodePtr c_retval;
    xmlNodePtr cur;
    PyObject *pyobj_cur;
    xmlNodePtr elem;
    PyObject *pyobj_elem;

    if (!PyArg_ParseTuple(args, (char *)"OO:xmlAddSibling", &pyobj_cur, &pyobj_elem))
        return(NULL);
    cur = (xmlNodePtr) PyxmlNode_Get(pyobj_cur);
    elem = (xmlNodePtr) PyxmlNode_Get(pyobj_elem);

    c_retval = xmlAddSibling(cur, elem);
    py_retval = libxml_xmlNodePtrWrap((xmlNodePtr) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlScanName(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    xmlChar * c_retval;
    xmlParserCtxtPtr ctxt;
    PyObject *pyobj_ctxt;

    if (!PyArg_ParseTuple(args, (char *)"O:xmlScanName", &pyobj_ctxt))
        return(NULL);
    ctxt = (xmlParserCtxtPtr) PyparserCtxt_Get(pyobj_ctxt);

    c_retval = xmlScanName(ctxt);
    py_retval = libxml_xmlCharPtrWrap((xmlChar *) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlParseElementDecl(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    int c_retval;
    xmlParserCtxtPtr ctxt;
    PyObject *pyobj_ctxt;

    if (!PyArg_ParseTuple(args, (char *)"O:xmlParseElementDecl", &pyobj_ctxt))
        return(NULL);
    ctxt = (xmlParserCtxtPtr) PyparserCtxt_Get(pyobj_ctxt);

    c_retval = xmlParseElementDecl(ctxt);
    py_retval = libxml_intWrap((int) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlFreeParserInputBuffer(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    xmlParserInputBufferPtr in;
    PyObject *pyobj_in;

    if (!PyArg_ParseTuple(args, (char *)"O:xmlFreeParserInputBuffer", &pyobj_in))
        return(NULL);
    in = (xmlParserInputBufferPtr) PyinputBuffer_Get(pyobj_in);

    xmlFreeParserInputBuffer(in);
    Py_INCREF(Py_None);
    return(Py_None);
}

#ifdef LIBXML_HTML_ENABLED
PyObject *
libxml_htmlDocContentDumpFormatOutput(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    xmlOutputBufferPtr buf;
    PyObject *pyobj_buf;
    xmlDocPtr cur;
    PyObject *pyobj_cur;
    char * encoding;
    int format;

    if (!PyArg_ParseTuple(args, (char *)"OOzi:htmlDocContentDumpFormatOutput", &pyobj_buf, &pyobj_cur, &encoding, &format))
        return(NULL);
    buf = (xmlOutputBufferPtr) PyoutputBuffer_Get(pyobj_buf);
    cur = (xmlDocPtr) PyxmlNode_Get(pyobj_cur);

    htmlDocContentDumpFormatOutput(buf, cur, encoding, format);
    Py_INCREF(Py_None);
    return(Py_None);
}

#endif /* LIBXML_HTML_ENABLED */
#ifdef LIBXML_XPATH_ENABLED
PyObject *
libxml_xmlXPathNextDescendantOrSelf(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    xmlNodePtr c_retval;
    xmlXPathParserContextPtr ctxt;
    PyObject *pyobj_ctxt;
    xmlNodePtr cur;
    PyObject *pyobj_cur;

    if (!PyArg_ParseTuple(args, (char *)"OO:xmlXPathNextDescendantOrSelf", &pyobj_ctxt, &pyobj_cur))
        return(NULL);
    ctxt = (xmlXPathParserContextPtr) PyxmlXPathParserContext_Get(pyobj_ctxt);
    cur = (xmlNodePtr) PyxmlNode_Get(pyobj_cur);

    c_retval = xmlXPathNextDescendantOrSelf(ctxt, cur);
    py_retval = libxml_xmlNodePtrWrap((xmlNodePtr) c_retval);
    return(py_retval);
}

#endif /* LIBXML_XPATH_ENABLED */
PyObject *
libxml_xmlUCSIsMathematicalAlphanumericSymbols(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    int c_retval;
    int code;

    if (!PyArg_ParseTuple(args, (char *)"i:xmlUCSIsMathematicalAlphanumericSymbols", &code))
        return(NULL);

    c_retval = xmlUCSIsMathematicalAlphanumericSymbols(code);
    py_retval = libxml_intWrap((int) c_retval);
    return(py_retval);
}

#ifdef LIBXML_XPATH_ENABLED
PyObject *
libxml_xmlXPathNsLookup(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    const xmlChar * c_retval;
    xmlXPathContextPtr ctxt;
    PyObject *pyobj_ctxt;
    xmlChar * prefix;

    if (!PyArg_ParseTuple(args, (char *)"Oz:xmlXPathNsLookup", &pyobj_ctxt, &prefix))
        return(NULL);
    ctxt = (xmlXPathContextPtr) PyxmlXPathContext_Get(pyobj_ctxt);

    c_retval = xmlXPathNsLookup(ctxt, prefix);
    py_retval = libxml_xmlCharPtrConstWrap((const xmlChar *) c_retval);
    return(py_retval);
}

#endif /* LIBXML_XPATH_ENABLED */
PyObject *
libxml_xmlUCSIsCJKCompatibilityIdeographs(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    int c_retval;
    int code;

    if (!PyArg_ParseTuple(args, (char *)"i:xmlUCSIsCJKCompatibilityIdeographs", &code))
        return(NULL);

    c_retval = xmlUCSIsCJKCompatibilityIdeographs(code);
    py_retval = libxml_intWrap((int) c_retval);
    return(py_retval);
}

#ifdef LIBXML_DEBUG_ENABLED
PyObject *
libxml_xmlDebugDumpEntities(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    FILE * output;
    PyObject *pyobj_output;
    xmlDocPtr doc;
    PyObject *pyobj_doc;

    if (!PyArg_ParseTuple(args, (char *)"OO:xmlDebugDumpEntities", &pyobj_output, &pyobj_doc))
        return(NULL);
    output = (FILE *) PyFile_Get(pyobj_output);
    doc = (xmlDocPtr) PyxmlNode_Get(pyobj_doc);

    xmlDebugDumpEntities(output, doc);
    Py_INCREF(Py_None);
    return(Py_None);
}

#endif /* LIBXML_DEBUG_ENABLED */
PyObject *
libxml_xmlUCSIsAlphabeticPresentationForms(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    int c_retval;
    int code;

    if (!PyArg_ParseTuple(args, (char *)"i:xmlUCSIsAlphabeticPresentationForms", &code))
        return(NULL);

    c_retval = xmlUCSIsAlphabeticPresentationForms(code);
    py_retval = libxml_intWrap((int) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlURISetQuery(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    xmlURIPtr URI;
    PyObject *pyobj_URI;
    char * query;

    if (!PyArg_ParseTuple(args, (char *)"Oz:xmlURISetQuery", &pyobj_URI, &query))
        return(NULL);
    URI = (xmlURIPtr) PyURI_Get(pyobj_URI);

    if (URI->query != NULL) xmlFree(URI->query);
    URI->query = xmlStrdup((const xmlChar *)query);
    Py_INCREF(Py_None);
    return(Py_None);
}

PyObject *
libxml_xmlURISetPort(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    xmlURIPtr URI;
    PyObject *pyobj_URI;
    int port;

    if (!PyArg_ParseTuple(args, (char *)"Oi:xmlURISetPort", &pyobj_URI, &port))
        return(NULL);
    URI = (xmlURIPtr) PyURI_Get(pyobj_URI);

    URI->port = port;
    Py_INCREF(Py_None);
    return(Py_None);
}

#ifdef LIBXML_DEBUG_ENABLED
PyObject *
libxml_xmlDebugDumpNodeList(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    FILE * output;
    PyObject *pyobj_output;
    xmlNodePtr node;
    PyObject *pyobj_node;
    int depth;

    if (!PyArg_ParseTuple(args, (char *)"OOi:xmlDebugDumpNodeList", &pyobj_output, &pyobj_node, &depth))
        return(NULL);
    output = (FILE *) PyFile_Get(pyobj_output);
    node = (xmlNodePtr) PyxmlNode_Get(pyobj_node);

    xmlDebugDumpNodeList(output, node, depth);
    Py_INCREF(Py_None);
    return(Py_None);
}

#endif /* LIBXML_DEBUG_ENABLED */
#ifdef LIBXML_XPATH_ENABLED
PyObject *
libxml_xmlXPathStartsWithFunction(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    xmlXPathParserContextPtr ctxt;
    PyObject *pyobj_ctxt;
    int nargs;

    if (!PyArg_ParseTuple(args, (char *)"Oi:xmlXPathStartsWithFunction", &pyobj_ctxt, &nargs))
        return(NULL);
    ctxt = (xmlXPathParserContextPtr) PyxmlXPathParserContext_Get(pyobj_ctxt);

    xmlXPathStartsWithFunction(ctxt, nargs);
    Py_INCREF(Py_None);
    return(Py_None);
}

#endif /* LIBXML_XPATH_ENABLED */
#ifdef LIBXML_XPATH_ENABLED
PyObject *
libxml_xmlXPathNextAncestor(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    xmlNodePtr c_retval;
    xmlXPathParserContextPtr ctxt;
    PyObject *pyobj_ctxt;
    xmlNodePtr cur;
    PyObject *pyobj_cur;

    if (!PyArg_ParseTuple(args, (char *)"OO:xmlXPathNextAncestor", &pyobj_ctxt, &pyobj_cur))
        return(NULL);
    ctxt = (xmlXPathParserContextPtr) PyxmlXPathParserContext_Get(pyobj_ctxt);
    cur = (xmlNodePtr) PyxmlNode_Get(pyobj_cur);

    c_retval = xmlXPathNextAncestor(ctxt, cur);
    py_retval = libxml_xmlNodePtrWrap((xmlNodePtr) c_retval);
    return(py_retval);
}

#endif /* LIBXML_XPATH_ENABLED */
#ifdef LIBXML_XPATH_ENABLED
PyObject *
libxml_xmlXPathLocalNameFunction(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    xmlXPathParserContextPtr ctxt;
    PyObject *pyobj_ctxt;
    int nargs;

    if (!PyArg_ParseTuple(args, (char *)"Oi:xmlXPathLocalNameFunction", &pyobj_ctxt, &nargs))
        return(NULL);
    ctxt = (xmlXPathParserContextPtr) PyxmlXPathParserContext_Get(pyobj_ctxt);

    xmlXPathLocalNameFunction(ctxt, nargs);
    Py_INCREF(Py_None);
    return(Py_None);
}

#endif /* LIBXML_XPATH_ENABLED */
PyObject *
libxml_xmlTextReaderMoveToFirstAttribute(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    int c_retval;
    xmlTextReaderPtr reader;
    PyObject *pyobj_reader;

    if (!PyArg_ParseTuple(args, (char *)"O:xmlTextReaderMoveToFirstAttribute", &pyobj_reader))
        return(NULL);
    reader = (xmlTextReaderPtr) PyxmlTextReader_Get(pyobj_reader);

    c_retval = xmlTextReaderMoveToFirstAttribute(reader);
    py_retval = libxml_intWrap((int) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlUCSIsCatCs(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    int c_retval;
    int code;

    if (!PyArg_ParseTuple(args, (char *)"i:xmlUCSIsCatCs", &code))
        return(NULL);

    c_retval = xmlUCSIsCatCs(code);
    py_retval = libxml_intWrap((int) c_retval);
    return(py_retval);
}

#ifdef LIBXML_XPATH_ENABLED
PyObject *
libxml_xmlXPathAddValues(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    xmlXPathParserContextPtr ctxt;
    PyObject *pyobj_ctxt;

    if (!PyArg_ParseTuple(args, (char *)"O:xmlXPathAddValues", &pyobj_ctxt))
        return(NULL);
    ctxt = (xmlXPathParserContextPtr) PyxmlXPathParserContext_Get(pyobj_ctxt);

    xmlXPathAddValues(ctxt);
    Py_INCREF(Py_None);
    return(Py_None);
}

#endif /* LIBXML_XPATH_ENABLED */
PyObject *
libxml_xmlUCSIsCJKCompatibility(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    int c_retval;
    int code;

    if (!PyArg_ParseTuple(args, (char *)"i:xmlUCSIsCJKCompatibility", &code))
        return(NULL);

    c_retval = xmlUCSIsCJKCompatibility(code);
    py_retval = libxml_intWrap((int) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlCanonicPath(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    xmlChar * c_retval;
    xmlChar * path;

    if (!PyArg_ParseTuple(args, (char *)"z:xmlCanonicPath", &path))
        return(NULL);

    c_retval = xmlCanonicPath(path);
    py_retval = libxml_xmlCharPtrWrap((xmlChar *) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlTextReaderGetAttributeNs(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    xmlChar * c_retval;
    xmlTextReaderPtr reader;
    PyObject *pyobj_reader;
    xmlChar * localName;
    xmlChar * namespaceURI;

    if (!PyArg_ParseTuple(args, (char *)"Ozz:xmlTextReaderGetAttributeNs", &pyobj_reader, &localName, &namespaceURI))
        return(NULL);
    reader = (xmlTextReaderPtr) PyxmlTextReader_Get(pyobj_reader);

    c_retval = xmlTextReaderGetAttributeNs(reader, localName, namespaceURI);
    py_retval = libxml_xmlCharPtrWrap((xmlChar *) c_retval);
    return(py_retval);
}

#ifdef LIBXML_HTML_ENABLED
PyObject *
libxml_htmlParseDocument(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    int c_retval;
    htmlParserCtxtPtr ctxt;
    PyObject *pyobj_ctxt;

    if (!PyArg_ParseTuple(args, (char *)"O:htmlParseDocument", &pyobj_ctxt))
        return(NULL);
    ctxt = (htmlParserCtxtPtr) PyparserCtxt_Get(pyobj_ctxt);

    c_retval = htmlParseDocument(ctxt);
    py_retval = libxml_intWrap((int) c_retval);
    return(py_retval);
}

#endif /* LIBXML_HTML_ENABLED */
PyObject *
libxml_xmlTextReaderGetParserProp(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    int c_retval;
    xmlTextReaderPtr reader;
    PyObject *pyobj_reader;
    int prop;

    if (!PyArg_ParseTuple(args, (char *)"Oi:xmlTextReaderGetParserProp", &pyobj_reader, &prop))
        return(NULL);
    reader = (xmlTextReaderPtr) PyxmlTextReader_Get(pyobj_reader);

    c_retval = xmlTextReaderGetParserProp(reader, prop);
    py_retval = libxml_intWrap((int) c_retval);
    return(py_retval);
}

#ifdef LIBXML_XPATH_ENABLED
PyObject *
libxml_xmlXPathPopBoolean(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    int c_retval;
    xmlXPathParserContextPtr ctxt;
    PyObject *pyobj_ctxt;

    if (!PyArg_ParseTuple(args, (char *)"O:xmlXPathPopBoolean", &pyobj_ctxt))
        return(NULL);
    ctxt = (xmlXPathParserContextPtr) PyxmlXPathParserContext_Get(pyobj_ctxt);

    c_retval = xmlXPathPopBoolean(ctxt);
    py_retval = libxml_intWrap((int) c_retval);
    return(py_retval);
}

#endif /* LIBXML_XPATH_ENABLED */
PyObject *
libxml_xmlUCSIsCatCo(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    int c_retval;
    int code;

    if (!PyArg_ParseTuple(args, (char *)"i:xmlUCSIsCatCo", &code))
        return(NULL);

    c_retval = xmlUCSIsCatCo(code);
    py_retval = libxml_intWrap((int) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlNewTextLen(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    xmlNodePtr c_retval;
    xmlChar * content;
    int len;

    if (!PyArg_ParseTuple(args, (char *)"zi:xmlNewTextLen", &content, &len))
        return(NULL);

    c_retval = xmlNewTextLen(content, len);
    py_retval = libxml_xmlNodePtrWrap((xmlNodePtr) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlNanoFTPCleanup(ATTRIBUTE_UNUSED PyObject *self,ATTRIBUTE_UNUSED  PyObject *args) {

    xmlNanoFTPCleanup();
    Py_INCREF(Py_None);
    return(Py_None);
}

PyObject *
libxml_xmlRecoverMemory(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    xmlDocPtr c_retval;
    char * buffer;
    int size;

    if (!PyArg_ParseTuple(args, (char *)"zi:xmlRecoverMemory", &buffer, &size))
        return(NULL);

    c_retval = xmlRecoverMemory(buffer, size);
    py_retval = libxml_xmlDocPtrWrap((xmlDocPtr) c_retval);
    return(py_retval);
}

#ifdef LIBXML_XPATH_ENABLED
PyObject *
libxml_xmlXPathValueFlipSign(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    xmlXPathParserContextPtr ctxt;
    PyObject *pyobj_ctxt;

    if (!PyArg_ParseTuple(args, (char *)"O:xmlXPathValueFlipSign", &pyobj_ctxt))
        return(NULL);
    ctxt = (xmlXPathParserContextPtr) PyxmlXPathParserContext_Get(pyobj_ctxt);

    xmlXPathValueFlipSign(ctxt);
    Py_INCREF(Py_None);
    return(Py_None);
}

#endif /* LIBXML_XPATH_ENABLED */
PyObject *
libxml_xmlValidateNCName(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    int c_retval;
    xmlChar * value;
    int space;

    if (!PyArg_ParseTuple(args, (char *)"zi:xmlValidateNCName", &value, &space))
        return(NULL);

    c_retval = xmlValidateNCName(value, space);
    py_retval = libxml_intWrap((int) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlTextReaderIsDefault(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    int c_retval;
    xmlTextReaderPtr reader;
    PyObject *pyobj_reader;

    if (!PyArg_ParseTuple(args, (char *)"O:xmlTextReaderIsDefault", &pyobj_reader))
        return(NULL);
    reader = (xmlTextReaderPtr) PyxmlTextReader_Get(pyobj_reader);

    c_retval = xmlTextReaderIsDefault(reader);
    py_retval = libxml_intWrap((int) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlUCSIsGeneralPunctuation(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    int c_retval;
    int code;

    if (!PyArg_ParseTuple(args, (char *)"i:xmlUCSIsGeneralPunctuation", &code))
        return(NULL);

    c_retval = xmlUCSIsGeneralPunctuation(code);
    py_retval = libxml_intWrap((int) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlUCSIsDevanagari(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    int c_retval;
    int code;

    if (!PyArg_ParseTuple(args, (char *)"i:xmlUCSIsDevanagari", &code))
        return(NULL);

    c_retval = xmlUCSIsDevanagari(code);
    py_retval = libxml_intWrap((int) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlURIGetUser(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    const char * c_retval;
    xmlURIPtr URI;
    PyObject *pyobj_URI;

    if (!PyArg_ParseTuple(args, (char *)"O:xmlURIGetUser", &pyobj_URI))
        return(NULL);
    URI = (xmlURIPtr) PyURI_Get(pyobj_URI);

    c_retval = URI->user;
    py_retval = libxml_charPtrConstWrap((const char *) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlUCSIsControlPictures(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    int c_retval;
    int code;

    if (!PyArg_ParseTuple(args, (char *)"i:xmlUCSIsControlPictures", &code))
        return(NULL);

    c_retval = xmlUCSIsControlPictures(code);
    py_retval = libxml_intWrap((int) c_retval);
    return(py_retval);
}

#ifdef LIBXML_HTML_ENABLED
PyObject *
libxml_htmlIsBooleanAttr(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    int c_retval;
    xmlChar * name;

    if (!PyArg_ParseTuple(args, (char *)"z:htmlIsBooleanAttr", &name))
        return(NULL);

    c_retval = htmlIsBooleanAttr(name);
    py_retval = libxml_intWrap((int) c_retval);
    return(py_retval);
}

#endif /* LIBXML_HTML_ENABLED */
PyObject *
libxml_xmlNodeListGetString(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    xmlChar * c_retval;
    xmlDocPtr doc;
    PyObject *pyobj_doc;
    xmlNodePtr list;
    PyObject *pyobj_list;
    int inLine;

    if (!PyArg_ParseTuple(args, (char *)"OOi:xmlNodeListGetString", &pyobj_doc, &pyobj_list, &inLine))
        return(NULL);
    doc = (xmlDocPtr) PyxmlNode_Get(pyobj_doc);
    list = (xmlNodePtr) PyxmlNode_Get(pyobj_list);

    c_retval = xmlNodeListGetString(doc, list, inLine);
    py_retval = libxml_xmlCharPtrWrap((xmlChar *) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlUCSIsBengali(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    int c_retval;
    int code;

    if (!PyArg_ParseTuple(args, (char *)"i:xmlUCSIsBengali", &code))
        return(NULL);

    c_retval = xmlUCSIsBengali(code);
    py_retval = libxml_intWrap((int) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlParserGetWellFormed(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    int c_retval;
    xmlParserCtxtPtr ctxt;
    PyObject *pyobj_ctxt;

    if (!PyArg_ParseTuple(args, (char *)"O:xmlParserGetWellFormed", &pyobj_ctxt))
        return(NULL);
    ctxt = (xmlParserCtxtPtr) PyparserCtxt_Get(pyobj_ctxt);

    c_retval = ctxt->wellFormed;
    py_retval = libxml_intWrap((int) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlEncodeSpecialChars(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    xmlChar * c_retval;
    xmlDocPtr doc;
    PyObject *pyobj_doc;
    xmlChar * input;

    if (!PyArg_ParseTuple(args, (char *)"Oz:xmlEncodeSpecialChars", &pyobj_doc, &input))
        return(NULL);
    doc = (xmlDocPtr) PyxmlNode_Get(pyobj_doc);

    c_retval = xmlEncodeSpecialChars(doc, input);
    py_retval = libxml_xmlCharPtrWrap((xmlChar *) c_retval);
    return(py_retval);
}

#ifdef LIBXML_XPATH_ENABLED
PyObject *
libxml_xmlXPathNextPrecedingSibling(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    xmlNodePtr c_retval;
    xmlXPathParserContextPtr ctxt;
    PyObject *pyobj_ctxt;
    xmlNodePtr cur;
    PyObject *pyobj_cur;

    if (!PyArg_ParseTuple(args, (char *)"OO:xmlXPathNextPrecedingSibling", &pyobj_ctxt, &pyobj_cur))
        return(NULL);
    ctxt = (xmlXPathParserContextPtr) PyxmlXPathParserContext_Get(pyobj_ctxt);
    cur = (xmlNodePtr) PyxmlNode_Get(pyobj_cur);

    c_retval = xmlXPathNextPrecedingSibling(ctxt, cur);
    py_retval = libxml_xmlNodePtrWrap((xmlNodePtr) c_retval);
    return(py_retval);
}

#endif /* LIBXML_XPATH_ENABLED */
PyObject *
libxml_xmlStrsub(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    xmlChar * c_retval;
    xmlChar * str;
    int start;
    int len;

    if (!PyArg_ParseTuple(args, (char *)"zii:xmlStrsub", &str, &start, &len))
        return(NULL);

    c_retval = xmlStrsub(str, start, len);
    py_retval = libxml_xmlCharPtrWrap((xmlChar *) c_retval);
    return(py_retval);
}

#ifdef LIBXML_XPATH_ENABLED
PyObject *
libxml_xmlXPathStringFunction(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    xmlXPathParserContextPtr ctxt;
    PyObject *pyobj_ctxt;
    int nargs;

    if (!PyArg_ParseTuple(args, (char *)"Oi:xmlXPathStringFunction", &pyobj_ctxt, &nargs))
        return(NULL);
    ctxt = (xmlXPathParserContextPtr) PyxmlXPathParserContext_Get(pyobj_ctxt);

    xmlXPathStringFunction(ctxt, nargs);
    Py_INCREF(Py_None);
    return(Py_None);
}

#endif /* LIBXML_XPATH_ENABLED */
PyObject *
libxml_xmlAddNextSibling(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    xmlNodePtr c_retval;
    xmlNodePtr cur;
    PyObject *pyobj_cur;
    xmlNodePtr elem;
    PyObject *pyobj_elem;

    if (!PyArg_ParseTuple(args, (char *)"OO:xmlAddNextSibling", &pyobj_cur, &pyobj_elem))
        return(NULL);
    cur = (xmlNodePtr) PyxmlNode_Get(pyobj_cur);
    elem = (xmlNodePtr) PyxmlNode_Get(pyobj_elem);

    c_retval = xmlAddNextSibling(cur, elem);
    py_retval = libxml_xmlNodePtrWrap((xmlNodePtr) c_retval);
    return(py_retval);
}

#ifdef LIBXML_XPATH_ENABLED
PyObject *
libxml_xmlXPathInit(ATTRIBUTE_UNUSED PyObject *self,ATTRIBUTE_UNUSED  PyObject *args) {

    xmlXPathInit();
    Py_INCREF(Py_None);
    return(Py_None);
}

#endif /* LIBXML_XPATH_ENABLED */
PyObject *
libxml_xmlInitParserCtxt(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    xmlParserCtxtPtr ctxt;
    PyObject *pyobj_ctxt;

    if (!PyArg_ParseTuple(args, (char *)"O:xmlInitParserCtxt", &pyobj_ctxt))
        return(NULL);
    ctxt = (xmlParserCtxtPtr) PyparserCtxt_Get(pyobj_ctxt);

    xmlInitParserCtxt(ctxt);
    Py_INCREF(Py_None);
    return(Py_None);
}

#ifdef LIBXML_SCHEMAS_ENABLED
PyObject *
libxml_xmlRelaxNGFreeParserCtxt(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    xmlRelaxNGParserCtxtPtr ctxt;
    PyObject *pyobj_ctxt;

    if (!PyArg_ParseTuple(args, (char *)"O:xmlRelaxNGFreeParserCtxt", &pyobj_ctxt))
        return(NULL);
    ctxt = (xmlRelaxNGParserCtxtPtr) PyrelaxNgParserCtxt_Get(pyobj_ctxt);

    xmlRelaxNGFreeParserCtxt(ctxt);
    Py_INCREF(Py_None);
    return(Py_None);
}

#endif /* LIBXML_SCHEMAS_ENABLED */
PyObject *
libxml_xmlCheckLanguageID(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    int c_retval;
    xmlChar * lang;

    if (!PyArg_ParseTuple(args, (char *)"z:xmlCheckLanguageID", &lang))
        return(NULL);

    c_retval = xmlCheckLanguageID(lang);
    py_retval = libxml_intWrap((int) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlGetDtdElementDesc(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    xmlElementPtr c_retval;
    xmlDtdPtr dtd;
    PyObject *pyobj_dtd;
    xmlChar * name;

    if (!PyArg_ParseTuple(args, (char *)"Oz:xmlGetDtdElementDesc", &pyobj_dtd, &name))
        return(NULL);
    dtd = (xmlDtdPtr) PyxmlNode_Get(pyobj_dtd);

    c_retval = xmlGetDtdElementDesc(dtd, name);
    py_retval = libxml_xmlElementPtrWrap((xmlElementPtr) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlUCSIsCatSc(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    int c_retval;
    int code;

    if (!PyArg_ParseTuple(args, (char *)"i:xmlUCSIsCatSc", &code))
        return(NULL);

    c_retval = xmlUCSIsCatSc(code);
    py_retval = libxml_intWrap((int) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlFreeNs(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    xmlNsPtr cur;
    PyObject *pyobj_cur;

    if (!PyArg_ParseTuple(args, (char *)"O:xmlFreeNs", &pyobj_cur))
        return(NULL);
    cur = (xmlNsPtr) PyxmlNode_Get(pyobj_cur);

    xmlFreeNs(cur);
    Py_INCREF(Py_None);
    return(Py_None);
}

#ifdef LIBXML_XPATH_ENABLED
PyObject *
libxml_xmlXPatherror(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    xmlXPathParserContextPtr ctxt;
    PyObject *pyobj_ctxt;
    char * file;
    int line;
    int no;

    if (!PyArg_ParseTuple(args, (char *)"Ozii:xmlXPatherror", &pyobj_ctxt, &file, &line, &no))
        return(NULL);
    ctxt = (xmlXPathParserContextPtr) PyxmlXPathParserContext_Get(pyobj_ctxt);

    xmlXPatherror(ctxt, file, line, no);
    Py_INCREF(Py_None);
    return(Py_None);
}

#endif /* LIBXML_XPATH_ENABLED */
PyObject *
libxml_xmlNextChar(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    xmlParserCtxtPtr ctxt;
    PyObject *pyobj_ctxt;

    if (!PyArg_ParseTuple(args, (char *)"O:xmlNextChar", &pyobj_ctxt))
        return(NULL);
    ctxt = (xmlParserCtxtPtr) PyparserCtxt_Get(pyobj_ctxt);

    xmlNextChar(ctxt);
    Py_INCREF(Py_None);
    return(Py_None);
}

#ifdef LIBXML_XPATH_ENABLED
PyObject *
libxml_xmlXPathFalseFunction(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    xmlXPathParserContextPtr ctxt;
    PyObject *pyobj_ctxt;
    int nargs;

    if (!PyArg_ParseTuple(args, (char *)"Oi:xmlXPathFalseFunction", &pyobj_ctxt, &nargs))
        return(NULL);
    ctxt = (xmlXPathParserContextPtr) PyxmlXPathParserContext_Get(pyobj_ctxt);

    xmlXPathFalseFunction(ctxt, nargs);
    Py_INCREF(Py_None);
    return(Py_None);
}

#endif /* LIBXML_XPATH_ENABLED */
PyObject *
libxml_xmlGetNoNsProp(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    xmlChar * c_retval;
    xmlNodePtr node;
    PyObject *pyobj_node;
    xmlChar * name;

    if (!PyArg_ParseTuple(args, (char *)"Oz:xmlGetNoNsProp", &pyobj_node, &name))
        return(NULL);
    node = (xmlNodePtr) PyxmlNode_Get(pyobj_node);

    c_retval = xmlGetNoNsProp(node, name);
    py_retval = libxml_xmlCharPtrWrap((xmlChar *) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlNewProp(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    xmlAttrPtr c_retval;
    xmlNodePtr node;
    PyObject *pyobj_node;
    xmlChar * name;
    xmlChar * value;

    if (!PyArg_ParseTuple(args, (char *)"Ozz:xmlNewProp", &pyobj_node, &name, &value))
        return(NULL);
    node = (xmlNodePtr) PyxmlNode_Get(pyobj_node);

    c_retval = xmlNewProp(node, name, value);
    py_retval = libxml_xmlNodePtrWrap((xmlNodePtr) c_retval);
    return(py_retval);
}

#ifdef LIBXML_XPATH_ENABLED
PyObject *
libxml_xmlXPathContainsFunction(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    xmlXPathParserContextPtr ctxt;
    PyObject *pyobj_ctxt;
    int nargs;

    if (!PyArg_ParseTuple(args, (char *)"Oi:xmlXPathContainsFunction", &pyobj_ctxt, &nargs))
        return(NULL);
    ctxt = (xmlXPathParserContextPtr) PyxmlXPathParserContext_Get(pyobj_ctxt);

    xmlXPathContainsFunction(ctxt, nargs);
    Py_INCREF(Py_None);
    return(Py_None);
}

#endif /* LIBXML_XPATH_ENABLED */
PyObject *
libxml_xmlTextReaderHasValue(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    int c_retval;
    xmlTextReaderPtr reader;
    PyObject *pyobj_reader;

    if (!PyArg_ParseTuple(args, (char *)"O:xmlTextReaderHasValue", &pyobj_reader))
        return(NULL);
    reader = (xmlTextReaderPtr) PyxmlTextReader_Get(pyobj_reader);

    c_retval = xmlTextReaderHasValue(reader);
    py_retval = libxml_intWrap((int) c_retval);
    return(py_retval);
}

#ifdef LIBXML_SCHEMAS_ENABLED
PyObject *
libxml_xmlRelaxNGDumpTree(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    FILE * output;
    PyObject *pyobj_output;
    xmlRelaxNGPtr schema;
    PyObject *pyobj_schema;

    if (!PyArg_ParseTuple(args, (char *)"OO:xmlRelaxNGDumpTree", &pyobj_output, &pyobj_schema))
        return(NULL);
    output = (FILE *) PyFile_Get(pyobj_output);
    schema = (xmlRelaxNGPtr) PyrelaxNgSchema_Get(pyobj_schema);

    xmlRelaxNGDumpTree(output, schema);
    Py_INCREF(Py_None);
    return(Py_None);
}

#endif /* LIBXML_SCHEMAS_ENABLED */
PyObject *
libxml_xmlParserGetDoc(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    xmlDocPtr c_retval;
    xmlParserCtxtPtr ctxt;
    PyObject *pyobj_ctxt;

    if (!PyArg_ParseTuple(args, (char *)"O:xmlParserGetDoc", &pyobj_ctxt))
        return(NULL);
    ctxt = (xmlParserCtxtPtr) PyparserCtxt_Get(pyobj_ctxt);

    c_retval = ctxt->myDoc;
    py_retval = libxml_xmlDocPtrWrap((xmlDocPtr) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlStringLenGetNodeList(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    xmlNodePtr c_retval;
    xmlDocPtr doc;
    PyObject *pyobj_doc;
    xmlChar * value;
    int len;

    if (!PyArg_ParseTuple(args, (char *)"Ozi:xmlStringLenGetNodeList", &pyobj_doc, &value, &len))
        return(NULL);
    doc = (xmlDocPtr) PyxmlNode_Get(pyobj_doc);

    c_retval = xmlStringLenGetNodeList(doc, value, len);
    py_retval = libxml_xmlNodePtrWrap((xmlNodePtr) c_retval);
    return(py_retval);
}

#ifdef LIBXML_REGEXP_ENABLED
PyObject *
libxml_xmlRegexpPrint(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    FILE * output;
    PyObject *pyobj_output;
    xmlRegexpPtr regexp;
    PyObject *pyobj_regexp;

    if (!PyArg_ParseTuple(args, (char *)"OO:xmlRegexpPrint", &pyobj_output, &pyobj_regexp))
        return(NULL);
    output = (FILE *) PyFile_Get(pyobj_output);
    regexp = (xmlRegexpPtr) PyxmlReg_Get(pyobj_regexp);

    xmlRegexpPrint(output, regexp);
    Py_INCREF(Py_None);
    return(Py_None);
}

#endif /* LIBXML_REGEXP_ENABLED */
#ifdef LIBXML_XPATH_ENABLED
PyObject *
libxml_xmlXPathRoundFunction(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    xmlXPathParserContextPtr ctxt;
    PyObject *pyobj_ctxt;
    int nargs;

    if (!PyArg_ParseTuple(args, (char *)"Oi:xmlXPathRoundFunction", &pyobj_ctxt, &nargs))
        return(NULL);
    ctxt = (xmlXPathParserContextPtr) PyxmlXPathParserContext_Get(pyobj_ctxt);

    xmlXPathRoundFunction(ctxt, nargs);
    Py_INCREF(Py_None);
    return(Py_None);
}

#endif /* LIBXML_XPATH_ENABLED */
PyObject *
libxml_xmlNodeAddContentLen(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    xmlNodePtr cur;
    PyObject *pyobj_cur;
    xmlChar * content;
    int len;

    if (!PyArg_ParseTuple(args, (char *)"Ozi:xmlNodeAddContentLen", &pyobj_cur, &content, &len))
        return(NULL);
    cur = (xmlNodePtr) PyxmlNode_Get(pyobj_cur);

    xmlNodeAddContentLen(cur, content, len);
    Py_INCREF(Py_None);
    return(Py_None);
}

PyObject *
libxml_xmlRegisterDefaultOutputCallbacks(ATTRIBUTE_UNUSED PyObject *self,ATTRIBUTE_UNUSED  PyObject *args) {

    xmlRegisterDefaultOutputCallbacks();
    Py_INCREF(Py_None);
    return(Py_None);
}

PyObject *
libxml_xmlTextReaderMoveToAttributeNo(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    int c_retval;
    xmlTextReaderPtr reader;
    PyObject *pyobj_reader;
    int no;

    if (!PyArg_ParseTuple(args, (char *)"Oi:xmlTextReaderMoveToAttributeNo", &pyobj_reader, &no))
        return(NULL);
    reader = (xmlTextReaderPtr) PyxmlTextReader_Get(pyobj_reader);

    c_retval = xmlTextReaderMoveToAttributeNo(reader, no);
    py_retval = libxml_intWrap((int) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlCleanupCharEncodingHandlers(ATTRIBUTE_UNUSED PyObject *self,ATTRIBUTE_UNUSED  PyObject *args) {

    xmlCleanupCharEncodingHandlers();
    Py_INCREF(Py_None);
    return(Py_None);
}

PyObject *
libxml_xmlUCSIsHiragana(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    int c_retval;
    int code;

    if (!PyArg_ParseTuple(args, (char *)"i:xmlUCSIsHiragana", &code))
        return(NULL);

    c_retval = xmlUCSIsHiragana(code);
    py_retval = libxml_intWrap((int) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlParserHandlePEReference(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    xmlParserCtxtPtr ctxt;
    PyObject *pyobj_ctxt;

    if (!PyArg_ParseTuple(args, (char *)"O:xmlParserHandlePEReference", &pyobj_ctxt))
        return(NULL);
    ctxt = (xmlParserCtxtPtr) PyparserCtxt_Get(pyobj_ctxt);

    xmlParserHandlePEReference(ctxt);
    Py_INCREF(Py_None);
    return(Py_None);
}

PyObject *
libxml_xmlUCSIsCatSk(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    int c_retval;
    int code;

    if (!PyArg_ParseTuple(args, (char *)"i:xmlUCSIsCatSk", &code))
        return(NULL);

    c_retval = xmlUCSIsCatSk(code);
    py_retval = libxml_intWrap((int) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlTextReaderMoveToAttributeNs(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    int c_retval;
    xmlTextReaderPtr reader;
    PyObject *pyobj_reader;
    xmlChar * localName;
    xmlChar * namespaceURI;

    if (!PyArg_ParseTuple(args, (char *)"Ozz:xmlTextReaderMoveToAttributeNs", &pyobj_reader, &localName, &namespaceURI))
        return(NULL);
    reader = (xmlTextReaderPtr) PyxmlTextReader_Get(pyobj_reader);

    c_retval = xmlTextReaderMoveToAttributeNs(reader, localName, namespaceURI);
    py_retval = libxml_intWrap((int) c_retval);
    return(py_retval);
}

#ifdef LIBXML_HTML_ENABLED
PyObject *
libxml_htmlNodeDumpFile(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    FILE * out;
    PyObject *pyobj_out;
    xmlDocPtr doc;
    PyObject *pyobj_doc;
    xmlNodePtr cur;
    PyObject *pyobj_cur;

    if (!PyArg_ParseTuple(args, (char *)"OOO:htmlNodeDumpFile", &pyobj_out, &pyobj_doc, &pyobj_cur))
        return(NULL);
    out = (FILE *) PyFile_Get(pyobj_out);
    doc = (xmlDocPtr) PyxmlNode_Get(pyobj_doc);
    cur = (xmlNodePtr) PyxmlNode_Get(pyobj_cur);

    htmlNodeDumpFile(out, doc, cur);
    Py_INCREF(Py_None);
    return(Py_None);
}

#endif /* LIBXML_HTML_ENABLED */
#ifdef LIBXML_XPATH_ENABLED
PyObject *
libxml_xmlXPathPositionFunction(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    xmlXPathParserContextPtr ctxt;
    PyObject *pyobj_ctxt;
    int nargs;

    if (!PyArg_ParseTuple(args, (char *)"Oi:xmlXPathPositionFunction", &pyobj_ctxt, &nargs))
        return(NULL);
    ctxt = (xmlXPathParserContextPtr) PyxmlXPathParserContext_Get(pyobj_ctxt);

    xmlXPathPositionFunction(ctxt, nargs);
    Py_INCREF(Py_None);
    return(Py_None);
}

#endif /* LIBXML_XPATH_ENABLED */
PyObject *
libxml_xmlXPathGetContextNode(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    xmlNodePtr c_retval;
    xmlXPathContextPtr ctxt;
    PyObject *pyobj_ctxt;

    if (!PyArg_ParseTuple(args, (char *)"O:xmlXPathGetContextNode", &pyobj_ctxt))
        return(NULL);
    ctxt = (xmlXPathContextPtr) PyxmlXPathContext_Get(pyobj_ctxt);

    c_retval = ctxt->node;
    py_retval = libxml_xmlNodePtrWrap((xmlNodePtr) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlParserSetPedantic(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    xmlParserCtxtPtr ctxt;
    PyObject *pyobj_ctxt;
    int pedantic;

    if (!PyArg_ParseTuple(args, (char *)"Oi:xmlParserSetPedantic", &pyobj_ctxt, &pedantic))
        return(NULL);
    ctxt = (xmlParserCtxtPtr) PyparserCtxt_Get(pyobj_ctxt);

    ctxt->pedantic = pedantic;
    Py_INCREF(Py_None);
    return(Py_None);
}

PyObject *
libxml_xmlUCSIsCatLu(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    int c_retval;
    int code;

    if (!PyArg_ParseTuple(args, (char *)"i:xmlUCSIsCatLu", &code))
        return(NULL);

    c_retval = xmlUCSIsCatLu(code);
    py_retval = libxml_intWrap((int) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlParseEntityRef(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    xmlEntityPtr c_retval;
    xmlParserCtxtPtr ctxt;
    PyObject *pyobj_ctxt;

    if (!PyArg_ParseTuple(args, (char *)"O:xmlParseEntityRef", &pyobj_ctxt))
        return(NULL);
    ctxt = (xmlParserCtxtPtr) PyparserCtxt_Get(pyobj_ctxt);

    c_retval = xmlParseEntityRef(ctxt);
    py_retval = libxml_xmlNodePtrWrap((xmlNodePtr) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlCatalogIsEmpty(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    int c_retval;
    xmlCatalogPtr catal;
    PyObject *pyobj_catal;

    if (!PyArg_ParseTuple(args, (char *)"O:xmlCatalogIsEmpty", &pyobj_catal))
        return(NULL);
    catal = (xmlCatalogPtr) Pycatalog_Get(pyobj_catal);

    c_retval = xmlCatalogIsEmpty(catal);
    py_retval = libxml_intWrap((int) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlIsPubidChar(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    int c_retval;
    int c;

    if (!PyArg_ParseTuple(args, (char *)"i:xmlIsPubidChar", &c))
        return(NULL);

    c_retval = xmlIsPubidChar(c);
    py_retval = libxml_intWrap((int) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlUCSIsCatLm(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    int c_retval;
    int code;

    if (!PyArg_ParseTuple(args, (char *)"i:xmlUCSIsCatLm", &code))
        return(NULL);

    c_retval = xmlUCSIsCatLm(code);
    py_retval = libxml_intWrap((int) c_retval);
    return(py_retval);
}

PyObject *
libxml_htmlInitAutoClose(ATTRIBUTE_UNUSED PyObject *self,ATTRIBUTE_UNUSED  PyObject *args) {

    htmlInitAutoClose();
    Py_INCREF(Py_None);
    return(Py_None);
}

PyObject *
libxml_xmlCopyNamespaceList(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    xmlNsPtr c_retval;
    xmlNsPtr cur;
    PyObject *pyobj_cur;

    if (!PyArg_ParseTuple(args, (char *)"O:xmlCopyNamespaceList", &pyobj_cur))
        return(NULL);
    cur = (xmlNsPtr) PyxmlNode_Get(pyobj_cur);

    c_retval = xmlCopyNamespaceList(cur);
    py_retval = libxml_xmlNsPtrWrap((xmlNsPtr) c_retval);
    return(py_retval);
}

#ifdef LIBXML_XPTR_ENABLED
PyObject *
libxml_xmlXPtrEval(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    xmlXPathObjectPtr c_retval;
    xmlChar * str;
    xmlXPathContextPtr ctx;
    PyObject *pyobj_ctx;

    if (!PyArg_ParseTuple(args, (char *)"zO:xmlXPtrEval", &str, &pyobj_ctx))
        return(NULL);
    ctx = (xmlXPathContextPtr) PyxmlXPathContext_Get(pyobj_ctx);

    c_retval = xmlXPtrEval(str, ctx);
    py_retval = libxml_xmlXPathObjectPtrWrap((xmlXPathObjectPtr) c_retval);
    return(py_retval);
}

#endif /* LIBXML_XPTR_ENABLED */
PyObject *
libxml_xmlUCSIsOpticalCharacterRecognition(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    int c_retval;
    int code;

    if (!PyArg_ParseTuple(args, (char *)"i:xmlUCSIsOpticalCharacterRecognition", &code))
        return(NULL);

    c_retval = xmlUCSIsOpticalCharacterRecognition(code);
    py_retval = libxml_intWrap((int) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlTextReaderReadOuterXml(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    xmlChar * c_retval;
    xmlTextReaderPtr reader;
    PyObject *pyobj_reader;

    if (!PyArg_ParseTuple(args, (char *)"O:xmlTextReaderReadOuterXml", &pyobj_reader))
        return(NULL);
    reader = (xmlTextReaderPtr) PyxmlTextReader_Get(pyobj_reader);

    c_retval = xmlTextReaderReadOuterXml(reader);
    py_retval = libxml_xmlCharPtrWrap((xmlChar *) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlLoadSGMLSuperCatalog(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    xmlCatalogPtr c_retval;
    char * filename;

    if (!PyArg_ParseTuple(args, (char *)"z:xmlLoadSGMLSuperCatalog", &filename))
        return(NULL);

    c_retval = xmlLoadSGMLSuperCatalog(filename);
    py_retval = libxml_xmlCatalogPtrWrap((xmlCatalogPtr) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlUCSIsCatPc(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    int c_retval;
    int code;

    if (!PyArg_ParseTuple(args, (char *)"i:xmlUCSIsCatPc", &code))
        return(NULL);

    c_retval = xmlUCSIsCatPc(code);
    py_retval = libxml_intWrap((int) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlUCSIsTamil(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    int c_retval;
    int code;

    if (!PyArg_ParseTuple(args, (char *)"i:xmlUCSIsTamil", &code))
        return(NULL);

    c_retval = xmlUCSIsTamil(code);
    py_retval = libxml_intWrap((int) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlGetNsProp(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    xmlChar * c_retval;
    xmlNodePtr node;
    PyObject *pyobj_node;
    xmlChar * name;
    xmlChar * nameSpace;

    if (!PyArg_ParseTuple(args, (char *)"Ozz:xmlGetNsProp", &pyobj_node, &name, &nameSpace))
        return(NULL);
    node = (xmlNodePtr) PyxmlNode_Get(pyobj_node);

    c_retval = xmlGetNsProp(node, name, nameSpace);
    py_retval = libxml_xmlCharPtrWrap((xmlChar *) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlNewDocProp(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    xmlAttrPtr c_retval;
    xmlDocPtr doc;
    PyObject *pyobj_doc;
    xmlChar * name;
    xmlChar * value;

    if (!PyArg_ParseTuple(args, (char *)"Ozz:xmlNewDocProp", &pyobj_doc, &name, &value))
        return(NULL);
    doc = (xmlDocPtr) PyxmlNode_Get(pyobj_doc);

    c_retval = xmlNewDocProp(doc, name, value);
    py_retval = libxml_xmlNodePtrWrap((xmlNodePtr) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlUCSIsCatN(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    int c_retval;
    int code;

    if (!PyArg_ParseTuple(args, (char *)"i:xmlUCSIsCatN", &code))
        return(NULL);

    c_retval = xmlUCSIsCatN(code);
    py_retval = libxml_intWrap((int) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlXPathSetContextDoc(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    xmlXPathContextPtr ctxt;
    PyObject *pyobj_ctxt;
    xmlDocPtr doc;
    PyObject *pyobj_doc;

    if (!PyArg_ParseTuple(args, (char *)"OO:xmlXPathSetContextDoc", &pyobj_ctxt, &pyobj_doc))
        return(NULL);
    ctxt = (xmlXPathContextPtr) PyxmlXPathContext_Get(pyobj_ctxt);
    doc = (xmlDocPtr) PyxmlNode_Get(pyobj_doc);

    ctxt->doc = doc;
    Py_INCREF(Py_None);
    return(Py_None);
}

PyObject *
libxml_xmlUCSIsCatL(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    int c_retval;
    int code;

    if (!PyArg_ParseTuple(args, (char *)"i:xmlUCSIsCatL", &code))
        return(NULL);

    c_retval = xmlUCSIsCatL(code);
    py_retval = libxml_intWrap((int) c_retval);
    return(py_retval);
}

#ifdef LIBXML_DEBUG_ENABLED
PyObject *
libxml_xmlDebugDumpString(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    FILE * output;
    PyObject *pyobj_output;
    xmlChar * str;

    if (!PyArg_ParseTuple(args, (char *)"Oz:xmlDebugDumpString", &pyobj_output, &str))
        return(NULL);
    output = (FILE *) PyFile_Get(pyobj_output);

    xmlDebugDumpString(output, str);
    Py_INCREF(Py_None);
    return(Py_None);
}

#endif /* LIBXML_DEBUG_ENABLED */
PyObject *
libxml_xmlUTF8Strpos(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    xmlChar * c_retval;
    xmlChar * utf;
    int pos;

    if (!PyArg_ParseTuple(args, (char *)"zi:xmlUTF8Strpos", &utf, &pos))
        return(NULL);

    c_retval = xmlUTF8Strpos(utf, pos);
    py_retval = libxml_xmlCharPtrWrap((xmlChar *) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlLoadACatalog(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    xmlCatalogPtr c_retval;
    char * filename;

    if (!PyArg_ParseTuple(args, (char *)"z:xmlLoadACatalog", &filename))
        return(NULL);

    c_retval = xmlLoadACatalog(filename);
    py_retval = libxml_xmlCatalogPtrWrap((xmlCatalogPtr) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlCopyChar(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    int c_retval;
    int len;
    xmlChar * out;
    int val;

    if (!PyArg_ParseTuple(args, (char *)"izi:xmlCopyChar", &len, &out, &val))
        return(NULL);

    c_retval = xmlCopyChar(len, out, val);
    py_retval = libxml_intWrap((int) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlUCSIsIPAExtensions(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    int c_retval;
    int code;

    if (!PyArg_ParseTuple(args, (char *)"i:xmlUCSIsIPAExtensions", &code))
        return(NULL);

    c_retval = xmlUCSIsIPAExtensions(code);
    py_retval = libxml_intWrap((int) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlUCSIsCatS(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    int c_retval;
    int code;

    if (!PyArg_ParseTuple(args, (char *)"i:xmlUCSIsCatS", &code))
        return(NULL);

    c_retval = xmlUCSIsCatS(code);
    py_retval = libxml_intWrap((int) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlUCSIsCatP(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    int c_retval;
    int code;

    if (!PyArg_ParseTuple(args, (char *)"i:xmlUCSIsCatP", &code))
        return(NULL);

    c_retval = xmlUCSIsCatP(code);
    py_retval = libxml_intWrap((int) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlCatalogGetSystem(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    const xmlChar * c_retval;
    xmlChar * sysID;

    if (!PyArg_ParseTuple(args, (char *)"z:xmlCatalogGetSystem", &sysID))
        return(NULL);

    c_retval = xmlCatalogGetSystem(sysID);
    py_retval = libxml_xmlCharPtrConstWrap((const xmlChar *) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlEncodeEntities(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    const xmlChar * c_retval;
    xmlDocPtr doc;
    PyObject *pyobj_doc;
    xmlChar * input;

    if (!PyArg_ParseTuple(args, (char *)"Oz:xmlEncodeEntities", &pyobj_doc, &input))
        return(NULL);
    doc = (xmlDocPtr) PyxmlNode_Get(pyobj_doc);

    c_retval = xmlEncodeEntities(doc, input);
    py_retval = libxml_xmlCharPtrConstWrap((const xmlChar *) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlGetDtdQElementDesc(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    xmlElementPtr c_retval;
    xmlDtdPtr dtd;
    PyObject *pyobj_dtd;
    xmlChar * name;
    xmlChar * prefix;

    if (!PyArg_ParseTuple(args, (char *)"Ozz:xmlGetDtdQElementDesc", &pyobj_dtd, &name, &prefix))
        return(NULL);
    dtd = (xmlDtdPtr) PyxmlNode_Get(pyobj_dtd);

    c_retval = xmlGetDtdQElementDesc(dtd, name, prefix);
    py_retval = libxml_xmlElementPtrWrap((xmlElementPtr) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlUCSIsCatZ(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    int c_retval;
    int code;

    if (!PyArg_ParseTuple(args, (char *)"i:xmlUCSIsCatZ", &code))
        return(NULL);

    c_retval = xmlUCSIsCatZ(code);
    py_retval = libxml_intWrap((int) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlURIEscape(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    xmlChar * c_retval;
    xmlChar * str;

    if (!PyArg_ParseTuple(args, (char *)"z:xmlURIEscape", &str))
        return(NULL);

    c_retval = xmlURIEscape(str);
    py_retval = libxml_xmlCharPtrWrap((xmlChar *) c_retval);
    return(py_retval);
}

#ifdef LIBXML_REGEXP_ENABLED
PyObject *
libxml_xmlRegexpExec(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    int c_retval;
    xmlRegexpPtr comp;
    PyObject *pyobj_comp;
    xmlChar * content;

    if (!PyArg_ParseTuple(args, (char *)"Oz:xmlRegexpExec", &pyobj_comp, &content))
        return(NULL);
    comp = (xmlRegexpPtr) PyxmlReg_Get(pyobj_comp);

    c_retval = xmlRegexpExec(comp, content);
    py_retval = libxml_intWrap((int) c_retval);
    return(py_retval);
}

#endif /* LIBXML_REGEXP_ENABLED */
PyObject *
libxml_xmlRegisterDefaultInputCallbacks(ATTRIBUTE_UNUSED PyObject *self,ATTRIBUTE_UNUSED  PyObject *args) {

    xmlRegisterDefaultInputCallbacks();
    Py_INCREF(Py_None);
    return(Py_None);
}

PyObject *
libxml_xmlStrncasecmp(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    int c_retval;
    xmlChar * str1;
    xmlChar * str2;
    int len;

    if (!PyArg_ParseTuple(args, (char *)"zzi:xmlStrncasecmp", &str1, &str2, &len))
        return(NULL);

    c_retval = xmlStrncasecmp(str1, str2, len);
    py_retval = libxml_intWrap((int) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlUCSIsCatPe(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    int c_retval;
    int code;

    if (!PyArg_ParseTuple(args, (char *)"i:xmlUCSIsCatPe", &code))
        return(NULL);

    c_retval = xmlUCSIsCatPe(code);
    py_retval = libxml_intWrap((int) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlParserHandleReference(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    xmlParserCtxtPtr ctxt;
    PyObject *pyobj_ctxt;

    if (!PyArg_ParseTuple(args, (char *)"O:xmlParserHandleReference", &pyobj_ctxt))
        return(NULL);
    ctxt = (xmlParserCtxtPtr) PyparserCtxt_Get(pyobj_ctxt);

    xmlParserHandleReference(ctxt);
    Py_INCREF(Py_None);
    return(Py_None);
}

PyObject *
libxml_xmlUCSIsCatPd(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    int c_retval;
    int code;

    if (!PyArg_ParseTuple(args, (char *)"i:xmlUCSIsCatPd", &code))
        return(NULL);

    c_retval = xmlUCSIsCatPd(code);
    py_retval = libxml_intWrap((int) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlHasProp(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    xmlAttrPtr c_retval;
    xmlNodePtr node;
    PyObject *pyobj_node;
    xmlChar * name;

    if (!PyArg_ParseTuple(args, (char *)"Oz:xmlHasProp", &pyobj_node, &name))
        return(NULL);
    node = (xmlNodePtr) PyxmlNode_Get(pyobj_node);

    c_retval = xmlHasProp(node, name);
    py_retval = libxml_xmlNodePtrWrap((xmlNodePtr) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlUCSIsArmenian(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    int c_retval;
    int code;

    if (!PyArg_ParseTuple(args, (char *)"i:xmlUCSIsArmenian", &code))
        return(NULL);

    c_retval = xmlUCSIsArmenian(code);
    py_retval = libxml_intWrap((int) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlCatalogCleanup(ATTRIBUTE_UNUSED PyObject *self,ATTRIBUTE_UNUSED  PyObject *args) {

    xmlCatalogCleanup();
    Py_INCREF(Py_None);
    return(Py_None);
}

PyObject *
libxml_xmlUCSIsCatPi(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    int c_retval;
    int code;

    if (!PyArg_ParseTuple(args, (char *)"i:xmlUCSIsCatPi", &code))
        return(NULL);

    c_retval = xmlUCSIsCatPi(code);
    py_retval = libxml_intWrap((int) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlDecodeEntities(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    xmlChar * c_retval;
    xmlParserCtxtPtr ctxt;
    PyObject *pyobj_ctxt;
    int len;
    int what;
    xmlChar end;
    xmlChar end2;
    xmlChar end3;

    if (!PyArg_ParseTuple(args, (char *)"Oiiccc:xmlDecodeEntities", &pyobj_ctxt, &len, &what, &end, &end2, &end3))
        return(NULL);
    ctxt = (xmlParserCtxtPtr) PyparserCtxt_Get(pyobj_ctxt);

    c_retval = xmlDecodeEntities(ctxt, len, what, end, end2, end3);
    py_retval = libxml_xmlCharPtrWrap((xmlChar *) c_retval);
    return(py_retval);
}

#ifdef LIBXML_XPATH_ENABLED
PyObject *
libxml_xmlXPathCastNodeToString(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    xmlChar * c_retval;
    xmlNodePtr node;
    PyObject *pyobj_node;

    if (!PyArg_ParseTuple(args, (char *)"O:xmlXPathCastNodeToString", &pyobj_node))
        return(NULL);
    node = (xmlNodePtr) PyxmlNode_Get(pyobj_node);

    c_retval = xmlXPathCastNodeToString(node);
    py_retval = libxml_xmlCharPtrWrap((xmlChar *) c_retval);
    return(py_retval);
}

#endif /* LIBXML_XPATH_ENABLED */
PyObject *
libxml_xmlNanoHTTPInit(ATTRIBUTE_UNUSED PyObject *self,ATTRIBUTE_UNUSED  PyObject *args) {

    xmlNanoHTTPInit();
    Py_INCREF(Py_None);
    return(Py_None);
}

PyObject *
libxml_xmlUCSIsHighPrivateUseSurrogates(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    int c_retval;
    int code;

    if (!PyArg_ParseTuple(args, (char *)"i:xmlUCSIsHighPrivateUseSurrogates", &code))
        return(NULL);

    c_retval = xmlUCSIsHighPrivateUseSurrogates(code);
    py_retval = libxml_intWrap((int) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlIsID(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    int c_retval;
    xmlDocPtr doc;
    PyObject *pyobj_doc;
    xmlNodePtr elem;
    PyObject *pyobj_elem;
    xmlAttrPtr attr;
    PyObject *pyobj_attr;

    if (!PyArg_ParseTuple(args, (char *)"OOO:xmlIsID", &pyobj_doc, &pyobj_elem, &pyobj_attr))
        return(NULL);
    doc = (xmlDocPtr) PyxmlNode_Get(pyobj_doc);
    elem = (xmlNodePtr) PyxmlNode_Get(pyobj_elem);
    attr = (xmlAttrPtr) PyxmlNode_Get(pyobj_attr);

    c_retval = xmlIsID(doc, elem, attr);
    py_retval = libxml_intWrap((int) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlGetDtdQAttrDesc(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    xmlAttributePtr c_retval;
    xmlDtdPtr dtd;
    PyObject *pyobj_dtd;
    xmlChar * elem;
    xmlChar * name;
    xmlChar * prefix;

    if (!PyArg_ParseTuple(args, (char *)"Ozzz:xmlGetDtdQAttrDesc", &pyobj_dtd, &elem, &name, &prefix))
        return(NULL);
    dtd = (xmlDtdPtr) PyxmlNode_Get(pyobj_dtd);

    c_retval = xmlGetDtdQAttrDesc(dtd, elem, name, prefix);
    py_retval = libxml_xmlAttributePtrWrap((xmlAttributePtr) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlUCSIsSpecials(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    int c_retval;
    int code;

    if (!PyArg_ParseTuple(args, (char *)"i:xmlUCSIsSpecials", &code))
        return(NULL);

    c_retval = xmlUCSIsSpecials(code);
    py_retval = libxml_intWrap((int) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlParseCDSect(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    xmlParserCtxtPtr ctxt;
    PyObject *pyobj_ctxt;

    if (!PyArg_ParseTuple(args, (char *)"O:xmlParseCDSect", &pyobj_ctxt))
        return(NULL);
    ctxt = (xmlParserCtxtPtr) PyparserCtxt_Get(pyobj_ctxt);

    xmlParseCDSect(ctxt);
    Py_INCREF(Py_None);
    return(Py_None);
}

PyObject *
libxml_xmlSetNs(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    xmlNodePtr node;
    PyObject *pyobj_node;
    xmlNsPtr ns;
    PyObject *pyobj_ns;

    if (!PyArg_ParseTuple(args, (char *)"OO:xmlSetNs", &pyobj_node, &pyobj_ns))
        return(NULL);
    node = (xmlNodePtr) PyxmlNode_Get(pyobj_node);
    ns = (xmlNsPtr) PyxmlNode_Get(pyobj_ns);

    xmlSetNs(node, ns);
    Py_INCREF(Py_None);
    return(Py_None);
}

PyObject *
libxml_xmlUCSIsLatinExtendedB(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    int c_retval;
    int code;

    if (!PyArg_ParseTuple(args, (char *)"i:xmlUCSIsLatinExtendedB", &code))
        return(NULL);

    c_retval = xmlUCSIsLatinExtendedB(code);
    py_retval = libxml_intWrap((int) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlUCSIsLatinExtendedA(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    int c_retval;
    int code;

    if (!PyArg_ParseTuple(args, (char *)"i:xmlUCSIsLatinExtendedA", &code))
        return(NULL);

    c_retval = xmlUCSIsLatinExtendedA(code);
    py_retval = libxml_intWrap((int) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlUCSIsDeseret(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    int c_retval;
    int code;

    if (!PyArg_ParseTuple(args, (char *)"i:xmlUCSIsDeseret", &code))
        return(NULL);

    c_retval = xmlUCSIsDeseret(code);
    py_retval = libxml_intWrap((int) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlNamespaceParseNCName(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    xmlChar * c_retval;
    xmlParserCtxtPtr ctxt;
    PyObject *pyobj_ctxt;

    if (!PyArg_ParseTuple(args, (char *)"O:xmlNamespaceParseNCName", &pyobj_ctxt))
        return(NULL);
    ctxt = (xmlParserCtxtPtr) PyparserCtxt_Get(pyobj_ctxt);

    c_retval = xmlNamespaceParseNCName(ctxt);
    py_retval = libxml_xmlCharPtrWrap((xmlChar *) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlParseExtParsedEnt(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    int c_retval;
    xmlParserCtxtPtr ctxt;
    PyObject *pyobj_ctxt;

    if (!PyArg_ParseTuple(args, (char *)"O:xmlParseExtParsedEnt", &pyobj_ctxt))
        return(NULL);
    ctxt = (xmlParserCtxtPtr) PyparserCtxt_Get(pyobj_ctxt);

    c_retval = xmlParseExtParsedEnt(ctxt);
    py_retval = libxml_intWrap((int) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlCopyDtd(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    xmlDtdPtr c_retval;
    xmlDtdPtr dtd;
    PyObject *pyobj_dtd;

    if (!PyArg_ParseTuple(args, (char *)"O:xmlCopyDtd", &pyobj_dtd))
        return(NULL);
    dtd = (xmlDtdPtr) PyxmlNode_Get(pyobj_dtd);

    c_retval = xmlCopyDtd(dtd);
    py_retval = libxml_xmlNodePtrWrap((xmlNodePtr) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlGetDtdEntity(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    xmlEntityPtr c_retval;
    xmlDocPtr doc;
    PyObject *pyobj_doc;
    xmlChar * name;

    if (!PyArg_ParseTuple(args, (char *)"Oz:xmlGetDtdEntity", &pyobj_doc, &name))
        return(NULL);
    doc = (xmlDocPtr) PyxmlNode_Get(pyobj_doc);

    c_retval = xmlGetDtdEntity(doc, name);
    py_retval = libxml_xmlNodePtrWrap((xmlNodePtr) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlUCSIsEnclosedCJKLettersandMonths(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    int c_retval;
    int code;

    if (!PyArg_ParseTuple(args, (char *)"i:xmlUCSIsEnclosedCJKLettersandMonths", &code))
        return(NULL);

    c_retval = xmlUCSIsEnclosedCJKLettersandMonths(code);
    py_retval = libxml_intWrap((int) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlNodeListGetRawString(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    xmlChar * c_retval;
    xmlDocPtr doc;
    PyObject *pyobj_doc;
    xmlNodePtr list;
    PyObject *pyobj_list;
    int inLine;

    if (!PyArg_ParseTuple(args, (char *)"OOi:xmlNodeListGetRawString", &pyobj_doc, &pyobj_list, &inLine))
        return(NULL);
    doc = (xmlDocPtr) PyxmlNode_Get(pyobj_doc);
    list = (xmlNodePtr) PyxmlNode_Get(pyobj_list);

    c_retval = xmlNodeListGetRawString(doc, list, inLine);
    py_retval = libxml_xmlCharPtrWrap((xmlChar *) c_retval);
    return(py_retval);
}

#ifdef LIBXML_XPATH_ENABLED
PyObject *
libxml_xmlXPathModValues(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    xmlXPathParserContextPtr ctxt;
    PyObject *pyobj_ctxt;

    if (!PyArg_ParseTuple(args, (char *)"O:xmlXPathModValues", &pyobj_ctxt))
        return(NULL);
    ctxt = (xmlXPathParserContextPtr) PyxmlXPathParserContext_Get(pyobj_ctxt);

    xmlXPathModValues(ctxt);
    Py_INCREF(Py_None);
    return(Py_None);
}

#endif /* LIBXML_XPATH_ENABLED */
PyObject *
libxml_xmlIsXHTML(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    int c_retval;
    xmlChar * systemID;
    xmlChar * publicID;

    if (!PyArg_ParseTuple(args, (char *)"zz:xmlIsXHTML", &systemID, &publicID))
        return(NULL);

    c_retval = xmlIsXHTML(systemID, publicID);
    py_retval = libxml_intWrap((int) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlUCSIsCJKUnifiedIdeographsExtensionA(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    int c_retval;
    int code;

    if (!PyArg_ParseTuple(args, (char *)"i:xmlUCSIsCJKUnifiedIdeographsExtensionA", &code))
        return(NULL);

    c_retval = xmlUCSIsCJKUnifiedIdeographsExtensionA(code);
    py_retval = libxml_intWrap((int) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlUCSIsRunic(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    int c_retval;
    int code;

    if (!PyArg_ParseTuple(args, (char *)"i:xmlUCSIsRunic", &code))
        return(NULL);

    c_retval = xmlUCSIsRunic(code);
    py_retval = libxml_intWrap((int) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlNewDocNode(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    xmlNodePtr c_retval;
    xmlDocPtr doc;
    PyObject *pyobj_doc;
    xmlNsPtr ns;
    PyObject *pyobj_ns;
    xmlChar * name;
    xmlChar * content;

    if (!PyArg_ParseTuple(args, (char *)"OOzz:xmlNewDocNode", &pyobj_doc, &pyobj_ns, &name, &content))
        return(NULL);
    doc = (xmlDocPtr) PyxmlNode_Get(pyobj_doc);
    ns = (xmlNsPtr) PyxmlNode_Get(pyobj_ns);

    c_retval = xmlNewDocNode(doc, ns, name, content);
    py_retval = libxml_xmlNodePtrWrap((xmlNodePtr) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlIsDigit(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    int c_retval;
    int c;

    if (!PyArg_ParseTuple(args, (char *)"i:xmlIsDigit", &c))
        return(NULL);

    c_retval = xmlIsDigit(c);
    py_retval = libxml_intWrap((int) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlNanoHTTPCleanup(ATTRIBUTE_UNUSED PyObject *self,ATTRIBUTE_UNUSED  PyObject *args) {

    xmlNanoHTTPCleanup();
    Py_INCREF(Py_None);
    return(Py_None);
}

#ifdef LIBXML_HTML_ENABLED
PyObject *
libxml_htmlParseDoc(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    htmlDocPtr c_retval;
    xmlChar * cur;
    char * encoding;

    if (!PyArg_ParseTuple(args, (char *)"zz:htmlParseDoc", &cur, &encoding))
        return(NULL);

    c_retval = htmlParseDoc(cur, encoding);
    py_retval = libxml_xmlDocPtrWrap((xmlDocPtr) c_retval);
    return(py_retval);
}

#endif /* LIBXML_HTML_ENABLED */
PyObject *
libxml_xmlParserGetDirectory(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    char * c_retval;
    char * filename;

    if (!PyArg_ParseTuple(args, (char *)"z:xmlParserGetDirectory", &filename))
        return(NULL);

    c_retval = xmlParserGetDirectory(filename);
    py_retval = libxml_charPtrWrap((char *) c_retval);
    return(py_retval);
}

#ifdef LIBXML_XPATH_ENABLED
PyObject *
libxml_xmlXPathFreeParserContext(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    xmlXPathParserContextPtr ctxt;
    PyObject *pyobj_ctxt;

    if (!PyArg_ParseTuple(args, (char *)"O:xmlXPathFreeParserContext", &pyobj_ctxt))
        return(NULL);
    ctxt = (xmlXPathParserContextPtr) PyxmlXPathParserContext_Get(pyobj_ctxt);

    xmlXPathFreeParserContext(ctxt);
    Py_INCREF(Py_None);
    return(Py_None);
}

#endif /* LIBXML_XPATH_ENABLED */
#ifdef LIBXML_XPATH_ENABLED
PyObject *
libxml_xmlXPathRegisteredNsCleanup(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    xmlXPathContextPtr ctxt;
    PyObject *pyobj_ctxt;

    if (!PyArg_ParseTuple(args, (char *)"O:xmlXPathRegisteredNsCleanup", &pyobj_ctxt))
        return(NULL);
    ctxt = (xmlXPathContextPtr) PyxmlXPathContext_Get(pyobj_ctxt);

    xmlXPathRegisteredNsCleanup(ctxt);
    Py_INCREF(Py_None);
    return(Py_None);
}

#endif /* LIBXML_XPATH_ENABLED */
PyObject *
libxml_xmlNewDocTextLen(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    xmlNodePtr c_retval;
    xmlDocPtr doc;
    PyObject *pyobj_doc;
    xmlChar * content;
    int len;

    if (!PyArg_ParseTuple(args, (char *)"Ozi:xmlNewDocTextLen", &pyobj_doc, &content, &len))
        return(NULL);
    doc = (xmlDocPtr) PyxmlNode_Get(pyobj_doc);

    c_retval = xmlNewDocTextLen(doc, content, len);
    py_retval = libxml_xmlNodePtrWrap((xmlNodePtr) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlFreeNsList(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    xmlNsPtr cur;
    PyObject *pyobj_cur;

    if (!PyArg_ParseTuple(args, (char *)"O:xmlFreeNsList", &pyobj_cur))
        return(NULL);
    cur = (xmlNsPtr) PyxmlNode_Get(pyobj_cur);

    xmlFreeNsList(cur);
    Py_INCREF(Py_None);
    return(Py_None);
}

PyObject *
libxml_xmlParseEntityDecl(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    xmlParserCtxtPtr ctxt;
    PyObject *pyobj_ctxt;

    if (!PyArg_ParseTuple(args, (char *)"O:xmlParseEntityDecl", &pyobj_ctxt))
        return(NULL);
    ctxt = (xmlParserCtxtPtr) PyparserCtxt_Get(pyobj_ctxt);

    xmlParseEntityDecl(ctxt);
    Py_INCREF(Py_None);
    return(Py_None);
}

PyObject *
libxml_xmlDocCopyNode(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    xmlNodePtr c_retval;
    xmlNodePtr node;
    PyObject *pyobj_node;
    xmlDocPtr doc;
    PyObject *pyobj_doc;
    int recursive;

    if (!PyArg_ParseTuple(args, (char *)"OOi:xmlDocCopyNode", &pyobj_node, &pyobj_doc, &recursive))
        return(NULL);
    node = (xmlNodePtr) PyxmlNode_Get(pyobj_node);
    doc = (xmlDocPtr) PyxmlNode_Get(pyobj_doc);

    c_retval = xmlDocCopyNode(node, doc, recursive);
    py_retval = libxml_xmlNodePtrWrap((xmlNodePtr) c_retval);
    return(py_retval);
}

PyObject *
libxml_nodePop(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    xmlNodePtr c_retval;
    xmlParserCtxtPtr ctxt;
    PyObject *pyobj_ctxt;

    if (!PyArg_ParseTuple(args, (char *)"O:nodePop", &pyobj_ctxt))
        return(NULL);
    ctxt = (xmlParserCtxtPtr) PyparserCtxt_Get(pyobj_ctxt);

    c_retval = nodePop(ctxt);
    py_retval = libxml_xmlNodePtrWrap((xmlNodePtr) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlCreateURI(ATTRIBUTE_UNUSED PyObject *self,ATTRIBUTE_UNUSED  PyObject *args) {
    PyObject *py_retval;
    xmlURIPtr c_retval;

    c_retval = xmlCreateURI();
    py_retval = libxml_xmlURIPtrWrap((xmlURIPtr) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlGetProp(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    xmlChar * c_retval;
    xmlNodePtr node;
    PyObject *pyobj_node;
    xmlChar * name;

    if (!PyArg_ParseTuple(args, (char *)"Oz:xmlGetProp", &pyobj_node, &name))
        return(NULL);
    node = (xmlNodePtr) PyxmlNode_Get(pyobj_node);

    c_retval = xmlGetProp(node, name);
    py_retval = libxml_xmlCharPtrWrap((xmlChar *) c_retval);
    return(py_retval);
}

#ifdef LIBXML_XPATH_ENABLED
PyObject *
libxml_xmlXPathNextDescendant(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    xmlNodePtr c_retval;
    xmlXPathParserContextPtr ctxt;
    PyObject *pyobj_ctxt;
    xmlNodePtr cur;
    PyObject *pyobj_cur;

    if (!PyArg_ParseTuple(args, (char *)"OO:xmlXPathNextDescendant", &pyobj_ctxt, &pyobj_cur))
        return(NULL);
    ctxt = (xmlXPathParserContextPtr) PyxmlXPathParserContext_Get(pyobj_ctxt);
    cur = (xmlNodePtr) PyxmlNode_Get(pyobj_cur);

    c_retval = xmlXPathNextDescendant(ctxt, cur);
    py_retval = libxml_xmlNodePtrWrap((xmlNodePtr) c_retval);
    return(py_retval);
}

#endif /* LIBXML_XPATH_ENABLED */
PyObject *
libxml_xmlStrcat(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    xmlChar * c_retval;
    xmlChar * cur;
    xmlChar * add;

    if (!PyArg_ParseTuple(args, (char *)"zz:xmlStrcat", &cur, &add))
        return(NULL);

    c_retval = xmlStrcat(cur, add);
    py_retval = libxml_xmlCharPtrWrap((xmlChar *) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlSearchNsByHref(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    xmlNsPtr c_retval;
    xmlDocPtr doc;
    PyObject *pyobj_doc;
    xmlNodePtr node;
    PyObject *pyobj_node;
    xmlChar * href;

    if (!PyArg_ParseTuple(args, (char *)"OOz:xmlSearchNsByHref", &pyobj_doc, &pyobj_node, &href))
        return(NULL);
    doc = (xmlDocPtr) PyxmlNode_Get(pyobj_doc);
    node = (xmlNodePtr) PyxmlNode_Get(pyobj_node);

    c_retval = xmlSearchNsByHref(doc, node, href);
    py_retval = libxml_xmlNsPtrWrap((xmlNsPtr) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlACatalogResolveURI(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    xmlChar * c_retval;
    xmlCatalogPtr catal;
    PyObject *pyobj_catal;
    xmlChar * URI;

    if (!PyArg_ParseTuple(args, (char *)"Oz:xmlACatalogResolveURI", &pyobj_catal, &URI))
        return(NULL);
    catal = (xmlCatalogPtr) Pycatalog_Get(pyobj_catal);

    c_retval = xmlACatalogResolveURI(catal, URI);
    py_retval = libxml_xmlCharPtrWrap((xmlChar *) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlURIGetAuthority(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    const char * c_retval;
    xmlURIPtr URI;
    PyObject *pyobj_URI;

    if (!PyArg_ParseTuple(args, (char *)"O:xmlURIGetAuthority", &pyobj_URI))
        return(NULL);
    URI = (xmlURIPtr) PyURI_Get(pyobj_URI);

    c_retval = URI->authority;
    py_retval = libxml_charPtrConstWrap((const char *) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlInitializeCatalog(ATTRIBUTE_UNUSED PyObject *self,ATTRIBUTE_UNUSED  PyObject *args) {

    xmlInitializeCatalog();
    Py_INCREF(Py_None);
    return(Py_None);
}

PyObject *
libxml_xmlLoadCatalog(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    int c_retval;
    char * filename;

    if (!PyArg_ParseTuple(args, (char *)"z:xmlLoadCatalog", &filename))
        return(NULL);

    c_retval = xmlLoadCatalog(filename);
    py_retval = libxml_intWrap((int) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlParseEntity(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    xmlDocPtr c_retval;
    char * filename;

    if (!PyArg_ParseTuple(args, (char *)"z:xmlParseEntity", &filename))
        return(NULL);

    c_retval = xmlParseEntity(filename);
    py_retval = libxml_xmlDocPtrWrap((xmlDocPtr) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlURIGetScheme(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    const char * c_retval;
    xmlURIPtr URI;
    PyObject *pyobj_URI;

    if (!PyArg_ParseTuple(args, (char *)"O:xmlURIGetScheme", &pyobj_URI))
        return(NULL);
    URI = (xmlURIPtr) PyURI_Get(pyobj_URI);

    c_retval = URI->scheme;
    py_retval = libxml_charPtrConstWrap((const char *) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlDocGetRootElement(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    xmlNodePtr c_retval;
    xmlDocPtr doc;
    PyObject *pyobj_doc;

    if (!PyArg_ParseTuple(args, (char *)"O:xmlDocGetRootElement", &pyobj_doc))
        return(NULL);
    doc = (xmlDocPtr) PyxmlNode_Get(pyobj_doc);

    c_retval = xmlDocGetRootElement(doc);
    py_retval = libxml_xmlNodePtrWrap((xmlNodePtr) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlStringGetNodeList(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    xmlNodePtr c_retval;
    xmlDocPtr doc;
    PyObject *pyobj_doc;
    xmlChar * value;

    if (!PyArg_ParseTuple(args, (char *)"Oz:xmlStringGetNodeList", &pyobj_doc, &value))
        return(NULL);
    doc = (xmlDocPtr) PyxmlNode_Get(pyobj_doc);

    c_retval = xmlStringGetNodeList(doc, value);
    py_retval = libxml_xmlNodePtrWrap((xmlNodePtr) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlParseMarkupDecl(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    xmlParserCtxtPtr ctxt;
    PyObject *pyobj_ctxt;

    if (!PyArg_ParseTuple(args, (char *)"O:xmlParseMarkupDecl", &pyobj_ctxt))
        return(NULL);
    ctxt = (xmlParserCtxtPtr) PyparserCtxt_Get(pyobj_ctxt);

    xmlParseMarkupDecl(ctxt);
    Py_INCREF(Py_None);
    return(Py_None);
}

PyObject *
libxml_xmlUCSIsMongolian(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    int c_retval;
    int code;

    if (!PyArg_ParseTuple(args, (char *)"i:xmlUCSIsMongolian", &code))
        return(NULL);

    c_retval = xmlUCSIsMongolian(code);
    py_retval = libxml_intWrap((int) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlUCSIsEthiopic(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    int c_retval;
    int code;

    if (!PyArg_ParseTuple(args, (char *)"i:xmlUCSIsEthiopic", &code))
        return(NULL);

    c_retval = xmlUCSIsEthiopic(code);
    py_retval = libxml_intWrap((int) c_retval);
    return(py_retval);
}

PyObject *
libxml_htmlCreateFileParserCtxt(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    htmlParserCtxtPtr c_retval;
    char * filename;
    char * encoding;

    if (!PyArg_ParseTuple(args, (char *)"zz:htmlCreateFileParserCtxt", &filename, &encoding))
        return(NULL);

    c_retval = htmlCreateFileParserCtxt(filename, encoding);
    py_retval = libxml_xmlParserCtxtPtrWrap((xmlParserCtxtPtr) c_retval);
    return(py_retval);
}

#ifdef LIBXML_XPATH_ENABLED
PyObject *
libxml_xmlXPathSumFunction(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    xmlXPathParserContextPtr ctxt;
    PyObject *pyobj_ctxt;
    int nargs;

    if (!PyArg_ParseTuple(args, (char *)"Oi:xmlXPathSumFunction", &pyobj_ctxt, &nargs))
        return(NULL);
    ctxt = (xmlXPathParserContextPtr) PyxmlXPathParserContext_Get(pyobj_ctxt);

    xmlXPathSumFunction(ctxt, nargs);
    Py_INCREF(Py_None);
    return(Py_None);
}

#endif /* LIBXML_XPATH_ENABLED */
PyObject *
libxml_xmlHasNsProp(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    xmlAttrPtr c_retval;
    xmlNodePtr node;
    PyObject *pyobj_node;
    xmlChar * name;
    xmlChar * nameSpace;

    if (!PyArg_ParseTuple(args, (char *)"Ozz:xmlHasNsProp", &pyobj_node, &name, &nameSpace))
        return(NULL);
    node = (xmlNodePtr) PyxmlNode_Get(pyobj_node);

    c_retval = xmlHasNsProp(node, name, nameSpace);
    py_retval = libxml_xmlNodePtrWrap((xmlNodePtr) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlUCSIsTags(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    int c_retval;
    int code;

    if (!PyArg_ParseTuple(args, (char *)"i:xmlUCSIsTags", &code))
        return(NULL);

    c_retval = xmlUCSIsTags(code);
    py_retval = libxml_intWrap((int) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlAddPrevSibling(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    xmlNodePtr c_retval;
    xmlNodePtr cur;
    PyObject *pyobj_cur;
    xmlNodePtr elem;
    PyObject *pyobj_elem;

    if (!PyArg_ParseTuple(args, (char *)"OO:xmlAddPrevSibling", &pyobj_cur, &pyobj_elem))
        return(NULL);
    cur = (xmlNodePtr) PyxmlNode_Get(pyobj_cur);
    elem = (xmlNodePtr) PyxmlNode_Get(pyobj_elem);

    c_retval = xmlAddPrevSibling(cur, elem);
    py_retval = libxml_xmlNodePtrWrap((xmlNodePtr) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlNewPI(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    xmlNodePtr c_retval;
    xmlChar * name;
    xmlChar * content;

    if (!PyArg_ParseTuple(args, (char *)"zz:xmlNewPI", &name, &content))
        return(NULL);

    c_retval = xmlNewPI(name, content);
    py_retval = libxml_xmlNodePtrWrap((xmlNodePtr) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlUCSIsLowSurrogates(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    int c_retval;
    int code;

    if (!PyArg_ParseTuple(args, (char *)"i:xmlUCSIsLowSurrogates", &code))
        return(NULL);

    c_retval = xmlUCSIsLowSurrogates(code);
    py_retval = libxml_intWrap((int) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlTextReaderMoveToAttribute(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    int c_retval;
    xmlTextReaderPtr reader;
    PyObject *pyobj_reader;
    xmlChar * name;

    if (!PyArg_ParseTuple(args, (char *)"Oz:xmlTextReaderMoveToAttribute", &pyobj_reader, &name))
        return(NULL);
    reader = (xmlTextReaderPtr) PyxmlTextReader_Get(pyobj_reader);

    c_retval = xmlTextReaderMoveToAttribute(reader, name);
    py_retval = libxml_intWrap((int) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlUCSIsBoxDrawing(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    int c_retval;
    int code;

    if (!PyArg_ParseTuple(args, (char *)"i:xmlUCSIsBoxDrawing", &code))
        return(NULL);

    c_retval = xmlUCSIsBoxDrawing(code);
    py_retval = libxml_intWrap((int) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlGetDtdAttrDesc(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    xmlAttributePtr c_retval;
    xmlDtdPtr dtd;
    PyObject *pyobj_dtd;
    xmlChar * elem;
    xmlChar * name;

    if (!PyArg_ParseTuple(args, (char *)"Ozz:xmlGetDtdAttrDesc", &pyobj_dtd, &elem, &name))
        return(NULL);
    dtd = (xmlDtdPtr) PyxmlNode_Get(pyobj_dtd);

    c_retval = xmlGetDtdAttrDesc(dtd, elem, name);
    py_retval = libxml_xmlAttributePtrWrap((xmlAttributePtr) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlUCSIsCJKCompatibilityForms(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    int c_retval;
    int code;

    if (!PyArg_ParseTuple(args, (char *)"i:xmlUCSIsCJKCompatibilityForms", &code))
        return(NULL);

    c_retval = xmlUCSIsCJKCompatibilityForms(code);
    py_retval = libxml_intWrap((int) c_retval);
    return(py_retval);
}

#ifdef LIBXML_HTML_ENABLED
PyObject *
libxml_htmlGetMetaEncoding(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    const xmlChar * c_retval;
    htmlDocPtr doc;
    PyObject *pyobj_doc;

    if (!PyArg_ParseTuple(args, (char *)"O:htmlGetMetaEncoding", &pyobj_doc))
        return(NULL);
    doc = (htmlDocPtr) PyxmlNode_Get(pyobj_doc);

    c_retval = htmlGetMetaEncoding(doc);
    py_retval = libxml_xmlCharPtrConstWrap((const xmlChar *) c_retval);
    return(py_retval);
}

#endif /* LIBXML_HTML_ENABLED */
#ifdef LIBXML_HTML_ENABLED
PyObject *
libxml_htmlNodeDumpFileFormat(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    int c_retval;
    FILE * out;
    PyObject *pyobj_out;
    xmlDocPtr doc;
    PyObject *pyobj_doc;
    xmlNodePtr cur;
    PyObject *pyobj_cur;
    char * encoding;
    int format;

    if (!PyArg_ParseTuple(args, (char *)"OOOzi:htmlNodeDumpFileFormat", &pyobj_out, &pyobj_doc, &pyobj_cur, &encoding, &format))
        return(NULL);
    out = (FILE *) PyFile_Get(pyobj_out);
    doc = (xmlDocPtr) PyxmlNode_Get(pyobj_doc);
    cur = (xmlNodePtr) PyxmlNode_Get(pyobj_cur);

    c_retval = htmlNodeDumpFileFormat(out, doc, cur, encoding, format);
    py_retval = libxml_intWrap((int) c_retval);
    return(py_retval);
}

#endif /* LIBXML_HTML_ENABLED */
PyObject *
libxml_xmlUnsetNsProp(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    int c_retval;
    xmlNodePtr node;
    PyObject *pyobj_node;
    xmlNsPtr ns;
    PyObject *pyobj_ns;
    xmlChar * name;

    if (!PyArg_ParseTuple(args, (char *)"OOz:xmlUnsetNsProp", &pyobj_node, &pyobj_ns, &name))
        return(NULL);
    node = (xmlNodePtr) PyxmlNode_Get(pyobj_node);
    ns = (xmlNsPtr) PyxmlNode_Get(pyobj_ns);

    c_retval = xmlUnsetNsProp(node, ns, name);
    py_retval = libxml_intWrap((int) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlElemDump(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    FILE * f;
    PyObject *pyobj_f;
    xmlDocPtr doc;
    PyObject *pyobj_doc;
    xmlNodePtr cur;
    PyObject *pyobj_cur;

    if (!PyArg_ParseTuple(args, (char *)"OOO:xmlElemDump", &pyobj_f, &pyobj_doc, &pyobj_cur))
        return(NULL);
    f = (FILE *) PyFile_Get(pyobj_f);
    doc = (xmlDocPtr) PyxmlNode_Get(pyobj_doc);
    cur = (xmlNodePtr) PyxmlNode_Get(pyobj_cur);

    xmlElemDump(f, doc, cur);
    Py_INCREF(Py_None);
    return(Py_None);
}

PyObject *
libxml_xmlUCSIsByzantineMusicalSymbols(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    int c_retval;
    int code;

    if (!PyArg_ParseTuple(args, (char *)"i:xmlUCSIsByzantineMusicalSymbols", &code))
        return(NULL);

    c_retval = xmlUCSIsByzantineMusicalSymbols(code);
    py_retval = libxml_intWrap((int) c_retval);
    return(py_retval);
}

#ifdef LIBXML_XPATH_ENABLED
PyObject *
libxml_xmlXPathTrueFunction(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    xmlXPathParserContextPtr ctxt;
    PyObject *pyobj_ctxt;
    int nargs;

    if (!PyArg_ParseTuple(args, (char *)"Oi:xmlXPathTrueFunction", &pyobj_ctxt, &nargs))
        return(NULL);
    ctxt = (xmlXPathParserContextPtr) PyxmlXPathParserContext_Get(pyobj_ctxt);

    xmlXPathTrueFunction(ctxt, nargs);
    Py_INCREF(Py_None);
    return(Py_None);
}

#endif /* LIBXML_XPATH_ENABLED */
#ifdef LIBXML_XPATH_ENABLED
PyObject *
libxml_xmlXPathConcatFunction(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    xmlXPathParserContextPtr ctxt;
    PyObject *pyobj_ctxt;
    int nargs;

    if (!PyArg_ParseTuple(args, (char *)"Oi:xmlXPathConcatFunction", &pyobj_ctxt, &nargs))
        return(NULL);
    ctxt = (xmlXPathParserContextPtr) PyxmlXPathParserContext_Get(pyobj_ctxt);

    xmlXPathConcatFunction(ctxt, nargs);
    Py_INCREF(Py_None);
    return(Py_None);
}

#endif /* LIBXML_XPATH_ENABLED */
PyObject *
libxml_xmlXPathGetContextPosition(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    int c_retval;
    xmlXPathContextPtr ctxt;
    PyObject *pyobj_ctxt;

    if (!PyArg_ParseTuple(args, (char *)"O:xmlXPathGetContextPosition", &pyobj_ctxt))
        return(NULL);
    ctxt = (xmlXPathContextPtr) PyxmlXPathContext_Get(pyobj_ctxt);

    c_retval = ctxt->proximityPosition;
    py_retval = libxml_intWrap((int) c_retval);
    return(py_retval);
}

#ifdef LIBXML_DEBUG_ENABLED
PyObject *
libxml_xmlShellPrintXPathError(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    int errorType;
    char * arg;

    if (!PyArg_ParseTuple(args, (char *)"iz:xmlShellPrintXPathError", &errorType, &arg))
        return(NULL);

    xmlShellPrintXPathError(errorType, arg);
    Py_INCREF(Py_None);
    return(Py_None);
}

#endif /* LIBXML_DEBUG_ENABLED */
PyObject *
libxml_xmlCatalogResolve(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    xmlChar * c_retval;
    xmlChar * pubID;
    xmlChar * sysID;

    if (!PyArg_ParseTuple(args, (char *)"zz:xmlCatalogResolve", &pubID, &sysID))
        return(NULL);

    c_retval = xmlCatalogResolve(pubID, sysID);
    py_retval = libxml_xmlCharPtrWrap((xmlChar *) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlCleanupInputCallbacks(ATTRIBUTE_UNUSED PyObject *self,ATTRIBUTE_UNUSED  PyObject *args) {

    xmlCleanupInputCallbacks();
    Py_INCREF(Py_None);
    return(Py_None);
}

PyObject *
libxml_xmlTextReaderLocatorLineNumber(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    int c_retval;
    xmlTextReaderLocatorPtr locator;
    PyObject *pyobj_locator;

    if (!PyArg_ParseTuple(args, (char *)"O:xmlTextReaderLocatorLineNumber", &pyobj_locator))
        return(NULL);
    locator = (xmlTextReaderLocatorPtr) PyxmlTextReaderLocator_Get(pyobj_locator);

    c_retval = xmlTextReaderLocatorLineNumber(locator);
    py_retval = libxml_intWrap((int) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlUCSIsCherokee(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    int c_retval;
    int code;

    if (!PyArg_ParseTuple(args, (char *)"i:xmlUCSIsCherokee", &code))
        return(NULL);

    c_retval = xmlUCSIsCherokee(code);
    py_retval = libxml_intWrap((int) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlTextConcat(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    xmlNodePtr node;
    PyObject *pyobj_node;
    xmlChar * content;
    int len;

    if (!PyArg_ParseTuple(args, (char *)"Ozi:xmlTextConcat", &pyobj_node, &content, &len))
        return(NULL);
    node = (xmlNodePtr) PyxmlNode_Get(pyobj_node);

    xmlTextConcat(node, content, len);
    Py_INCREF(Py_None);
    return(Py_None);
}

PyObject *
libxml_xmlNormalizeURIPath(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    int c_retval;
    char * path;

    if (!PyArg_ParseTuple(args, (char *)"z:xmlNormalizeURIPath", &path))
        return(NULL);

    c_retval = xmlNormalizeURIPath(path);
    py_retval = libxml_intWrap((int) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlNodeSetContent(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    xmlNodePtr cur;
    PyObject *pyobj_cur;
    xmlChar * content;

    if (!PyArg_ParseTuple(args, (char *)"Oz:xmlNodeSetContent", &pyobj_cur, &content))
        return(NULL);
    cur = (xmlNodePtr) PyxmlNode_Get(pyobj_cur);

    xmlNodeSetContent(cur, content);
    Py_INCREF(Py_None);
    return(Py_None);
}

PyObject *
libxml_xmlNewTextReader(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    xmlTextReaderPtr c_retval;
    xmlParserInputBufferPtr input;
    PyObject *pyobj_input;
    char * URI;

    if (!PyArg_ParseTuple(args, (char *)"Oz:xmlNewTextReader", &pyobj_input, &URI))
        return(NULL);
    input = (xmlParserInputBufferPtr) PyinputBuffer_Get(pyobj_input);

    c_retval = xmlNewTextReader(input, URI);
    py_retval = libxml_xmlTextReaderPtrWrap((xmlTextReaderPtr) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlParseCharRef(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    int c_retval;
    xmlParserCtxtPtr ctxt;
    PyObject *pyobj_ctxt;

    if (!PyArg_ParseTuple(args, (char *)"O:xmlParseCharRef", &pyobj_ctxt))
        return(NULL);
    ctxt = (xmlParserCtxtPtr) PyparserCtxt_Get(pyobj_ctxt);

    c_retval = xmlParseCharRef(ctxt);
    py_retval = libxml_intWrap((int) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlFreeProp(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    xmlAttrPtr cur;
    PyObject *pyobj_cur;

    if (!PyArg_ParseTuple(args, (char *)"O:xmlFreeProp", &pyobj_cur))
        return(NULL);
    cur = (xmlAttrPtr) PyxmlNode_Get(pyobj_cur);

    xmlFreeProp(cur);
    Py_INCREF(Py_None);
    return(Py_None);
}

#ifdef LIBXML_DEBUG_ENABLED
PyObject *
libxml_xmlDebugDumpAttrList(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    FILE * output;
    PyObject *pyobj_output;
    xmlAttrPtr attr;
    PyObject *pyobj_attr;
    int depth;

    if (!PyArg_ParseTuple(args, (char *)"OOi:xmlDebugDumpAttrList", &pyobj_output, &pyobj_attr, &depth))
        return(NULL);
    output = (FILE *) PyFile_Get(pyobj_output);
    attr = (xmlAttrPtr) PyxmlNode_Get(pyobj_attr);

    xmlDebugDumpAttrList(output, attr, depth);
    Py_INCREF(Py_None);
    return(Py_None);
}

#endif /* LIBXML_DEBUG_ENABLED */
PyObject *
libxml_xmlTextReaderGetAttributeNo(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    xmlChar * c_retval;
    xmlTextReaderPtr reader;
    PyObject *pyobj_reader;
    int no;

    if (!PyArg_ParseTuple(args, (char *)"Oi:xmlTextReaderGetAttributeNo", &pyobj_reader, &no))
        return(NULL);
    reader = (xmlTextReaderPtr) PyxmlTextReader_Get(pyobj_reader);

    c_retval = xmlTextReaderGetAttributeNo(reader, no);
    py_retval = libxml_xmlCharPtrWrap((xmlChar *) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlParserInputBufferRead(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    int c_retval;
    xmlParserInputBufferPtr in;
    PyObject *pyobj_in;
    int len;

    if (!PyArg_ParseTuple(args, (char *)"Oi:xmlParserInputBufferRead", &pyobj_in, &len))
        return(NULL);
    in = (xmlParserInputBufferPtr) PyinputBuffer_Get(pyobj_in);

    c_retval = xmlParserInputBufferRead(in, len);
    py_retval = libxml_intWrap((int) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlCatalogAdd(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    int c_retval;
    xmlChar * type;
    xmlChar * orig;
    xmlChar * replace;

    if (!PyArg_ParseTuple(args, (char *)"zzz:xmlCatalogAdd", &type, &orig, &replace))
        return(NULL);

    c_retval = xmlCatalogAdd(type, orig, replace);
    py_retval = libxml_intWrap((int) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlCopyCharMultiByte(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    int c_retval;
    xmlChar * out;
    int val;

    if (!PyArg_ParseTuple(args, (char *)"zi:xmlCopyCharMultiByte", &out, &val))
        return(NULL);

    c_retval = xmlCopyCharMultiByte(out, val);
    py_retval = libxml_intWrap((int) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlParseVersionNum(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    xmlChar * c_retval;
    xmlParserCtxtPtr ctxt;
    PyObject *pyobj_ctxt;

    if (!PyArg_ParseTuple(args, (char *)"O:xmlParseVersionNum", &pyobj_ctxt))
        return(NULL);
    ctxt = (xmlParserCtxtPtr) PyparserCtxt_Get(pyobj_ctxt);

    c_retval = xmlParseVersionNum(ctxt);
    py_retval = libxml_xmlCharPtrWrap((xmlChar *) c_retval);
    return(py_retval);
}

#ifdef LIBXML_DEBUG_ENABLED
PyObject *
libxml_xmlBoolToText(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    const char * c_retval;
    int boolval;

    if (!PyArg_ParseTuple(args, (char *)"i:xmlBoolToText", &boolval))
        return(NULL);

    c_retval = xmlBoolToText(boolval);
    py_retval = libxml_charPtrConstWrap((const char *) c_retval);
    return(py_retval);
}

#endif /* LIBXML_DEBUG_ENABLED */
PyObject *
libxml_xmlUCSIsGurmukhi(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    int c_retval;
    int code;

    if (!PyArg_ParseTuple(args, (char *)"i:xmlUCSIsGurmukhi", &code))
        return(NULL);

    c_retval = xmlUCSIsGurmukhi(code);
    py_retval = libxml_intWrap((int) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlParseCharData(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    xmlParserCtxtPtr ctxt;
    PyObject *pyobj_ctxt;
    int cdata;

    if (!PyArg_ParseTuple(args, (char *)"Oi:xmlParseCharData", &pyobj_ctxt, &cdata))
        return(NULL);
    ctxt = (xmlParserCtxtPtr) PyparserCtxt_Get(pyobj_ctxt);

    xmlParseCharData(ctxt, cdata);
    Py_INCREF(Py_None);
    return(Py_None);
}

PyObject *
libxml_xmlUCSIsCatCf(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    int c_retval;
    int code;

    if (!PyArg_ParseTuple(args, (char *)"i:xmlUCSIsCatCf", &code))
        return(NULL);

    c_retval = xmlUCSIsCatCf(code);
    py_retval = libxml_intWrap((int) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlUCSIsThai(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    int c_retval;
    int code;

    if (!PyArg_ParseTuple(args, (char *)"i:xmlUCSIsThai", &code))
        return(NULL);

    c_retval = xmlUCSIsThai(code);
    py_retval = libxml_intWrap((int) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlSetCompressMode(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    int mode;

    if (!PyArg_ParseTuple(args, (char *)"i:xmlSetCompressMode", &mode))
        return(NULL);

    xmlSetCompressMode(mode);
    Py_INCREF(Py_None);
    return(Py_None);
}

PyObject *
libxml_xmlCatalogResolveSystem(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    xmlChar * c_retval;
    xmlChar * sysID;

    if (!PyArg_ParseTuple(args, (char *)"z:xmlCatalogResolveSystem", &sysID))
        return(NULL);

    c_retval = xmlCatalogResolveSystem(sysID);
    py_retval = libxml_xmlCharPtrWrap((xmlChar *) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlNodeAddContent(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    xmlNodePtr cur;
    PyObject *pyobj_cur;
    xmlChar * content;

    if (!PyArg_ParseTuple(args, (char *)"Oz:xmlNodeAddContent", &pyobj_cur, &content))
        return(NULL);
    cur = (xmlNodePtr) PyxmlNode_Get(pyobj_cur);

    xmlNodeAddContent(cur, content);
    Py_INCREF(Py_None);
    return(Py_None);
}

PyObject *
libxml_xmlStrlen(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    int c_retval;
    xmlChar * str;

    if (!PyArg_ParseTuple(args, (char *)"z:xmlStrlen", &str))
        return(NULL);

    c_retval = xmlStrlen(str);
    py_retval = libxml_intWrap((int) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlTextReaderNodeType(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    int c_retval;
    xmlTextReaderPtr reader;
    PyObject *pyobj_reader;

    if (!PyArg_ParseTuple(args, (char *)"O:xmlTextReaderNodeType", &pyobj_reader))
        return(NULL);
    reader = (xmlTextReaderPtr) PyxmlTextReader_Get(pyobj_reader);

    c_retval = xmlTextReaderNodeType(reader);
    py_retval = libxml_intWrap((int) c_retval);
    return(py_retval);
}

#ifdef LIBXML_XPATH_ENABLED
PyObject *
libxml_xmlXPathSubstringFunction(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    xmlXPathParserContextPtr ctxt;
    PyObject *pyobj_ctxt;
    int nargs;

    if (!PyArg_ParseTuple(args, (char *)"Oi:xmlXPathSubstringFunction", &pyobj_ctxt, &nargs))
        return(NULL);
    ctxt = (xmlXPathParserContextPtr) PyxmlXPathParserContext_Get(pyobj_ctxt);

    xmlXPathSubstringFunction(ctxt, nargs);
    Py_INCREF(Py_None);
    return(Py_None);
}

#endif /* LIBXML_XPATH_ENABLED */
PyObject *
libxml_xmlIsBlankNode(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    int c_retval;
    xmlNodePtr node;
    PyObject *pyobj_node;

    if (!PyArg_ParseTuple(args, (char *)"O:xmlIsBlankNode", &pyobj_node))
        return(NULL);
    node = (xmlNodePtr) PyxmlNode_Get(pyobj_node);

    c_retval = xmlIsBlankNode(node);
    py_retval = libxml_intWrap((int) c_retval);
    return(py_retval);
}

#ifdef LIBXML_SCHEMAS_ENABLED
PyObject *
libxml_xmlRelaxNGFree(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    xmlRelaxNGPtr schema;
    PyObject *pyobj_schema;

    if (!PyArg_ParseTuple(args, (char *)"O:xmlRelaxNGFree", &pyobj_schema))
        return(NULL);
    schema = (xmlRelaxNGPtr) PyrelaxNgSchema_Get(pyobj_schema);

    xmlRelaxNGFree(schema);
    Py_INCREF(Py_None);
    return(Py_None);
}

#endif /* LIBXML_SCHEMAS_ENABLED */
PyObject *
libxml_xmlURISetServer(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    xmlURIPtr URI;
    PyObject *pyobj_URI;
    char * server;

    if (!PyArg_ParseTuple(args, (char *)"Oz:xmlURISetServer", &pyobj_URI, &server))
        return(NULL);
    URI = (xmlURIPtr) PyURI_Get(pyobj_URI);

    if (URI->server != NULL) xmlFree(URI->server);
    URI->server = xmlStrdup((const xmlChar *)server);
    Py_INCREF(Py_None);
    return(Py_None);
}

PyObject *
libxml_xmlCopyDoc(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    xmlDocPtr c_retval;
    xmlDocPtr doc;
    PyObject *pyobj_doc;
    int recursive;

    if (!PyArg_ParseTuple(args, (char *)"Oi:xmlCopyDoc", &pyobj_doc, &recursive))
        return(NULL);
    doc = (xmlDocPtr) PyxmlNode_Get(pyobj_doc);

    c_retval = xmlCopyDoc(doc, recursive);
    py_retval = libxml_xmlDocPtrWrap((xmlDocPtr) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlUCSIsSpacingModifierLetters(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    int c_retval;
    int code;

    if (!PyArg_ParseTuple(args, (char *)"i:xmlUCSIsSpacingModifierLetters", &code))
        return(NULL);

    c_retval = xmlUCSIsSpacingModifierLetters(code);
    py_retval = libxml_intWrap((int) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlUCSIsCat(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    int c_retval;
    int code;
    char * cat;

    if (!PyArg_ParseTuple(args, (char *)"iz:xmlUCSIsCat", &code, &cat))
        return(NULL);

    c_retval = xmlUCSIsCat(code, cat);
    py_retval = libxml_intWrap((int) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlNewNsProp(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    xmlAttrPtr c_retval;
    xmlNodePtr node;
    PyObject *pyobj_node;
    xmlNsPtr ns;
    PyObject *pyobj_ns;
    xmlChar * name;
    xmlChar * value;

    if (!PyArg_ParseTuple(args, (char *)"OOzz:xmlNewNsProp", &pyobj_node, &pyobj_ns, &name, &value))
        return(NULL);
    node = (xmlNodePtr) PyxmlNode_Get(pyobj_node);
    ns = (xmlNsPtr) PyxmlNode_Get(pyobj_ns);

    c_retval = xmlNewNsProp(node, ns, name, value);
    py_retval = libxml_xmlNodePtrWrap((xmlNodePtr) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlTextReaderReadAttributeValue(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    int c_retval;
    xmlTextReaderPtr reader;
    PyObject *pyobj_reader;

    if (!PyArg_ParseTuple(args, (char *)"O:xmlTextReaderReadAttributeValue", &pyobj_reader))
        return(NULL);
    reader = (xmlTextReaderPtr) PyxmlTextReader_Get(pyobj_reader);

    c_retval = xmlTextReaderReadAttributeValue(reader);
    py_retval = libxml_intWrap((int) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlNewCatalog(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    xmlCatalogPtr c_retval;
    int sgml;

    if (!PyArg_ParseTuple(args, (char *)"i:xmlNewCatalog", &sgml))
        return(NULL);

    c_retval = xmlNewCatalog(sgml);
    py_retval = libxml_xmlCatalogPtrWrap((xmlCatalogPtr) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlStrcasecmp(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    int c_retval;
    xmlChar * str1;
    xmlChar * str2;

    if (!PyArg_ParseTuple(args, (char *)"zz:xmlStrcasecmp", &str1, &str2))
        return(NULL);

    c_retval = xmlStrcasecmp(str1, str2);
    py_retval = libxml_intWrap((int) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlUCSIsGothic(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    int c_retval;
    int code;

    if (!PyArg_ParseTuple(args, (char *)"i:xmlUCSIsGothic", &code))
        return(NULL);

    c_retval = xmlUCSIsGothic(code);
    py_retval = libxml_intWrap((int) c_retval);
    return(py_retval);
}

#ifdef LIBXML_XPTR_ENABLED
PyObject *
libxml_xmlXPtrNewContext(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    xmlXPathContextPtr c_retval;
    xmlDocPtr doc;
    PyObject *pyobj_doc;
    xmlNodePtr here;
    PyObject *pyobj_here;
    xmlNodePtr origin;
    PyObject *pyobj_origin;

    if (!PyArg_ParseTuple(args, (char *)"OOO:xmlXPtrNewContext", &pyobj_doc, &pyobj_here, &pyobj_origin))
        return(NULL);
    doc = (xmlDocPtr) PyxmlNode_Get(pyobj_doc);
    here = (xmlNodePtr) PyxmlNode_Get(pyobj_here);
    origin = (xmlNodePtr) PyxmlNode_Get(pyobj_origin);

    c_retval = xmlXPtrNewContext(doc, here, origin);
    py_retval = libxml_xmlXPathContextPtrWrap((xmlXPathContextPtr) c_retval);
    return(py_retval);
}

#endif /* LIBXML_XPTR_ENABLED */
PyObject *
libxml_xmlUCSIsBraillePatterns(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    int c_retval;
    int code;

    if (!PyArg_ParseTuple(args, (char *)"i:xmlUCSIsBraillePatterns", &code))
        return(NULL);

    c_retval = xmlUCSIsBraillePatterns(code);
    py_retval = libxml_intWrap((int) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlParseSystemLiteral(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    xmlChar * c_retval;
    xmlParserCtxtPtr ctxt;
    PyObject *pyobj_ctxt;

    if (!PyArg_ParseTuple(args, (char *)"O:xmlParseSystemLiteral", &pyobj_ctxt))
        return(NULL);
    ctxt = (xmlParserCtxtPtr) PyparserCtxt_Get(pyobj_ctxt);

    c_retval = xmlParseSystemLiteral(ctxt);
    py_retval = libxml_xmlCharPtrWrap((xmlChar *) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlNanoFTPProxy(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    char * host;
    int port;
    char * user;
    char * passwd;
    int type;

    if (!PyArg_ParseTuple(args, (char *)"zizzi:xmlNanoFTPProxy", &host, &port, &user, &passwd, &type))
        return(NULL);

    xmlNanoFTPProxy(host, port, user, passwd, type);
    Py_INCREF(Py_None);
    return(Py_None);
}

PyObject *
libxml_xmlParseAttValue(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    xmlChar * c_retval;
    xmlParserCtxtPtr ctxt;
    PyObject *pyobj_ctxt;

    if (!PyArg_ParseTuple(args, (char *)"O:xmlParseAttValue", &pyobj_ctxt))
        return(NULL);
    ctxt = (xmlParserCtxtPtr) PyparserCtxt_Get(pyobj_ctxt);

    c_retval = xmlParseAttValue(ctxt);
    py_retval = libxml_xmlCharPtrWrap((xmlChar *) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlStrcmp(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    int c_retval;
    xmlChar * str1;
    xmlChar * str2;

    if (!PyArg_ParseTuple(args, (char *)"zz:xmlStrcmp", &str1, &str2))
        return(NULL);

    c_retval = xmlStrcmp(str1, str2);
    py_retval = libxml_intWrap((int) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlCreateMemoryParserCtxt(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    xmlParserCtxtPtr c_retval;
    char * buffer;
    int size;

    if (!PyArg_ParseTuple(args, (char *)"zi:xmlCreateMemoryParserCtxt", &buffer, &size))
        return(NULL);

    c_retval = xmlCreateMemoryParserCtxt(buffer, size);
    py_retval = libxml_xmlParserCtxtPtrWrap((xmlParserCtxtPtr) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlParseAttributeListDecl(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    xmlParserCtxtPtr ctxt;
    PyObject *pyobj_ctxt;

    if (!PyArg_ParseTuple(args, (char *)"O:xmlParseAttributeListDecl", &pyobj_ctxt))
        return(NULL);
    ctxt = (xmlParserCtxtPtr) PyparserCtxt_Get(pyobj_ctxt);

    xmlParseAttributeListDecl(ctxt);
    Py_INCREF(Py_None);
    return(Py_None);
}

PyObject *
libxml_xmlCheckVersion(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    int version;

    if (!PyArg_ParseTuple(args, (char *)"i:xmlCheckVersion", &version))
        return(NULL);

    xmlCheckVersion(version);
    Py_INCREF(Py_None);
    return(Py_None);
}

#ifdef LIBXML_HTML_ENABLED
PyObject *
libxml_htmlHandleOmittedElem(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    int c_retval;
    int val;

    if (!PyArg_ParseTuple(args, (char *)"i:htmlHandleOmittedElem", &val))
        return(NULL);

    c_retval = htmlHandleOmittedElem(val);
    py_retval = libxml_intWrap((int) c_retval);
    return(py_retval);
}

#endif /* LIBXML_HTML_ENABLED */
PyObject *
libxml_xmlParseName(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    xmlChar * c_retval;
    xmlParserCtxtPtr ctxt;
    PyObject *pyobj_ctxt;

    if (!PyArg_ParseTuple(args, (char *)"O:xmlParseName", &pyobj_ctxt))
        return(NULL);
    ctxt = (xmlParserCtxtPtr) PyparserCtxt_Get(pyobj_ctxt);

    c_retval = xmlParseName(ctxt);
    py_retval = libxml_xmlCharPtrWrap((xmlChar *) c_retval);
    return(py_retval);
}

#ifdef LIBXML_REGEXP_ENABLED
PyObject *
libxml_xmlRegFreeRegexp(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    xmlRegexpPtr regexp;
    PyObject *pyobj_regexp;

    if (!PyArg_ParseTuple(args, (char *)"O:xmlRegFreeRegexp", &pyobj_regexp))
        return(NULL);
    regexp = (xmlRegexpPtr) PyxmlReg_Get(pyobj_regexp);

    xmlRegFreeRegexp(regexp);
    Py_INCREF(Py_None);
    return(Py_None);
}

#endif /* LIBXML_REGEXP_ENABLED */
PyObject *
libxml_nodePush(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    int c_retval;
    xmlParserCtxtPtr ctxt;
    PyObject *pyobj_ctxt;
    xmlNodePtr value;
    PyObject *pyobj_value;

    if (!PyArg_ParseTuple(args, (char *)"OO:nodePush", &pyobj_ctxt, &pyobj_value))
        return(NULL);
    ctxt = (xmlParserCtxtPtr) PyparserCtxt_Get(pyobj_ctxt);
    value = (xmlNodePtr) PyxmlNode_Get(pyobj_value);

    c_retval = nodePush(ctxt, value);
    py_retval = libxml_intWrap((int) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlURIUnescapeString(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    char * c_retval;
    char * str;
    int len;
    char * target;

    if (!PyArg_ParseTuple(args, (char *)"ziz:xmlURIUnescapeString", &str, &len, &target))
        return(NULL);

    c_retval = xmlURIUnescapeString(str, len, target);
    py_retval = libxml_charPtrWrap((char *) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlTextReaderLookupNamespace(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    xmlChar * c_retval;
    xmlTextReaderPtr reader;
    PyObject *pyobj_reader;
    xmlChar * prefix;

    if (!PyArg_ParseTuple(args, (char *)"Oz:xmlTextReaderLookupNamespace", &pyobj_reader, &prefix))
        return(NULL);
    reader = (xmlTextReaderPtr) PyxmlTextReader_Get(pyobj_reader);

    c_retval = xmlTextReaderLookupNamespace(reader, prefix);
    py_retval = libxml_xmlCharPtrWrap((xmlChar *) c_retval);
    return(py_retval);
}

#ifdef LIBXML_REGEXP_ENABLED
PyObject *
libxml_xmlRegexpIsDeterminist(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    int c_retval;
    xmlRegexpPtr comp;
    PyObject *pyobj_comp;

    if (!PyArg_ParseTuple(args, (char *)"O:xmlRegexpIsDeterminist", &pyobj_comp))
        return(NULL);
    comp = (xmlRegexpPtr) PyxmlReg_Get(pyobj_comp);

    c_retval = xmlRegexpIsDeterminist(comp);
    py_retval = libxml_intWrap((int) c_retval);
    return(py_retval);
}

#endif /* LIBXML_REGEXP_ENABLED */
#ifdef LIBXML_HTML_ENABLED
PyObject *
libxml_htmlNewDoc(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    htmlDocPtr c_retval;
    xmlChar * URI;
    xmlChar * ExternalID;

    if (!PyArg_ParseTuple(args, (char *)"zz:htmlNewDoc", &URI, &ExternalID))
        return(NULL);

    c_retval = htmlNewDoc(URI, ExternalID);
    py_retval = libxml_xmlDocPtrWrap((xmlDocPtr) c_retval);
    return(py_retval);
}

#endif /* LIBXML_HTML_ENABLED */
PyObject *
libxml_xmlUCSIsCombiningDiacriticalMarks(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    int c_retval;
    int code;

    if (!PyArg_ParseTuple(args, (char *)"i:xmlUCSIsCombiningDiacriticalMarks", &code))
        return(NULL);

    c_retval = xmlUCSIsCombiningDiacriticalMarks(code);
    py_retval = libxml_intWrap((int) c_retval);
    return(py_retval);
}

#ifdef LIBXML_XPATH_ENABLED
PyObject *
libxml_xmlXPathIsInf(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    int c_retval;
    double val;

    if (!PyArg_ParseTuple(args, (char *)"d:xmlXPathIsInf", &val))
        return(NULL);

    c_retval = xmlXPathIsInf(val);
    py_retval = libxml_intWrap((int) c_retval);
    return(py_retval);
}

#endif /* LIBXML_XPATH_ENABLED */
PyObject *
libxml_xmlCopyNode(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    xmlNodePtr c_retval;
    xmlNodePtr node;
    PyObject *pyobj_node;
    int recursive;

    if (!PyArg_ParseTuple(args, (char *)"Oi:xmlCopyNode", &pyobj_node, &recursive))
        return(NULL);
    node = (xmlNodePtr) PyxmlNode_Get(pyobj_node);

    c_retval = xmlCopyNode(node, recursive);
    py_retval = libxml_xmlNodePtrWrap((xmlNodePtr) c_retval);
    return(py_retval);
}

#ifdef LIBXML_XPATH_ENABLED
PyObject *
libxml_xmlXPathCastStringToBoolean(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    int c_retval;
    xmlChar * val;

    if (!PyArg_ParseTuple(args, (char *)"z:xmlXPathCastStringToBoolean", &val))
        return(NULL);

    c_retval = xmlXPathCastStringToBoolean(val);
    py_retval = libxml_intWrap((int) c_retval);
    return(py_retval);
}

#endif /* LIBXML_XPATH_ENABLED */
#ifdef LIBXML_XPATH_ENABLED
PyObject *
libxml_xmlXPathEqualValues(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    int c_retval;
    xmlXPathParserContextPtr ctxt;
    PyObject *pyobj_ctxt;

    if (!PyArg_ParseTuple(args, (char *)"O:xmlXPathEqualValues", &pyobj_ctxt))
        return(NULL);
    ctxt = (xmlXPathParserContextPtr) PyxmlXPathParserContext_Get(pyobj_ctxt);

    c_retval = xmlXPathEqualValues(ctxt);
    py_retval = libxml_intWrap((int) c_retval);
    return(py_retval);
}

#endif /* LIBXML_XPATH_ENABLED */
PyObject *
libxml_xmlTextReaderClose(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    int c_retval;
    xmlTextReaderPtr reader;
    PyObject *pyobj_reader;

    if (!PyArg_ParseTuple(args, (char *)"O:xmlTextReaderClose", &pyobj_reader))
        return(NULL);
    reader = (xmlTextReaderPtr) PyxmlTextReader_Get(pyobj_reader);

    c_retval = xmlTextReaderClose(reader);
    py_retval = libxml_intWrap((int) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlURISetUser(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    xmlURIPtr URI;
    PyObject *pyobj_URI;
    char * user;

    if (!PyArg_ParseTuple(args, (char *)"Oz:xmlURISetUser", &pyobj_URI, &user))
        return(NULL);
    URI = (xmlURIPtr) PyURI_Get(pyobj_URI);

    if (URI->user != NULL) xmlFree(URI->user);
    URI->user = xmlStrdup((const xmlChar *)user);
    Py_INCREF(Py_None);
    return(Py_None);
}

PyObject *
libxml_xmlTextReaderMoveToElement(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    int c_retval;
    xmlTextReaderPtr reader;
    PyObject *pyobj_reader;

    if (!PyArg_ParseTuple(args, (char *)"O:xmlTextReaderMoveToElement", &pyobj_reader))
        return(NULL);
    reader = (xmlTextReaderPtr) PyxmlTextReader_Get(pyobj_reader);

    c_retval = xmlTextReaderMoveToElement(reader);
    py_retval = libxml_intWrap((int) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlInitializePredefinedEntities(ATTRIBUTE_UNUSED PyObject *self,ATTRIBUTE_UNUSED  PyObject *args) {

    xmlInitializePredefinedEntities();
    Py_INCREF(Py_None);
    return(Py_None);
}

#ifdef LIBXML_XPATH_ENABLED
PyObject *
libxml_xmlXPathNextAttribute(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    xmlNodePtr c_retval;
    xmlXPathParserContextPtr ctxt;
    PyObject *pyobj_ctxt;
    xmlNodePtr cur;
    PyObject *pyobj_cur;

    if (!PyArg_ParseTuple(args, (char *)"OO:xmlXPathNextAttribute", &pyobj_ctxt, &pyobj_cur))
        return(NULL);
    ctxt = (xmlXPathParserContextPtr) PyxmlXPathParserContext_Get(pyobj_ctxt);
    cur = (xmlNodePtr) PyxmlNode_Get(pyobj_cur);

    c_retval = xmlXPathNextAttribute(ctxt, cur);
    py_retval = libxml_xmlNodePtrWrap((xmlNodePtr) c_retval);
    return(py_retval);
}

#endif /* LIBXML_XPATH_ENABLED */
PyObject *
libxml_xmlUCSIsMiscellaneousTechnical(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    int c_retval;
    int code;

    if (!PyArg_ParseTuple(args, (char *)"i:xmlUCSIsMiscellaneousTechnical", &code))
        return(NULL);

    c_retval = xmlUCSIsMiscellaneousTechnical(code);
    py_retval = libxml_intWrap((int) c_retval);
    return(py_retval);
}

#ifdef LIBXML_HTML_ENABLED
PyObject *
libxml_htmlIsAutoClosed(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    int c_retval;
    htmlDocPtr doc;
    PyObject *pyobj_doc;
    htmlNodePtr elem;
    PyObject *pyobj_elem;

    if (!PyArg_ParseTuple(args, (char *)"OO:htmlIsAutoClosed", &pyobj_doc, &pyobj_elem))
        return(NULL);
    doc = (htmlDocPtr) PyxmlNode_Get(pyobj_doc);
    elem = (htmlNodePtr) PyxmlNode_Get(pyobj_elem);

    c_retval = htmlIsAutoClosed(doc, elem);
    py_retval = libxml_intWrap((int) c_retval);
    return(py_retval);
}

#endif /* LIBXML_HTML_ENABLED */
PyObject *
libxml_xmlSearchNs(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    xmlNsPtr c_retval;
    xmlDocPtr doc;
    PyObject *pyobj_doc;
    xmlNodePtr node;
    PyObject *pyobj_node;
    xmlChar * nameSpace;

    if (!PyArg_ParseTuple(args, (char *)"OOz:xmlSearchNs", &pyobj_doc, &pyobj_node, &nameSpace))
        return(NULL);
    doc = (xmlDocPtr) PyxmlNode_Get(pyobj_doc);
    node = (xmlNodePtr) PyxmlNode_Get(pyobj_node);

    c_retval = xmlSearchNs(doc, node, nameSpace);
    py_retval = libxml_xmlNsPtrWrap((xmlNsPtr) c_retval);
    return(py_retval);
}

#ifdef LIBXML_XPATH_ENABLED
PyObject *
libxml_xmlXPathVariableLookupNS(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    xmlXPathObjectPtr c_retval;
    xmlXPathContextPtr ctxt;
    PyObject *pyobj_ctxt;
    xmlChar * name;
    xmlChar * ns_uri;

    if (!PyArg_ParseTuple(args, (char *)"Ozz:xmlXPathVariableLookupNS", &pyobj_ctxt, &name, &ns_uri))
        return(NULL);
    ctxt = (xmlXPathContextPtr) PyxmlXPathContext_Get(pyobj_ctxt);

    c_retval = xmlXPathVariableLookupNS(ctxt, name, ns_uri);
    py_retval = libxml_xmlXPathObjectPtrWrap((xmlXPathObjectPtr) c_retval);
    return(py_retval);
}

#endif /* LIBXML_XPATH_ENABLED */
PyObject *
libxml_xmlUCSIsCJKCompatibilityIdeographsSupplement(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    int c_retval;
    int code;

    if (!PyArg_ParseTuple(args, (char *)"i:xmlUCSIsCJKCompatibilityIdeographsSupplement", &code))
        return(NULL);

    c_retval = xmlUCSIsCJKCompatibilityIdeographsSupplement(code);
    py_retval = libxml_intWrap((int) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlUCSIsCatCc(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    int c_retval;
    int code;

    if (!PyArg_ParseTuple(args, (char *)"i:xmlUCSIsCatCc", &code))
        return(NULL);

    c_retval = xmlUCSIsCatCc(code);
    py_retval = libxml_intWrap((int) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlUCSIsHebrew(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    int c_retval;
    int code;

    if (!PyArg_ParseTuple(args, (char *)"i:xmlUCSIsHebrew", &code))
        return(NULL);

    c_retval = xmlUCSIsHebrew(code);
    py_retval = libxml_intWrap((int) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlReconciliateNs(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    int c_retval;
    xmlDocPtr doc;
    PyObject *pyobj_doc;
    xmlNodePtr tree;
    PyObject *pyobj_tree;

    if (!PyArg_ParseTuple(args, (char *)"OO:xmlReconciliateNs", &pyobj_doc, &pyobj_tree))
        return(NULL);
    doc = (xmlDocPtr) PyxmlNode_Get(pyobj_doc);
    tree = (xmlNodePtr) PyxmlNode_Get(pyobj_tree);

    c_retval = xmlReconciliateNs(doc, tree);
    py_retval = libxml_intWrap((int) c_retval);
    return(py_retval);
}

#ifdef LIBXML_XPATH_ENABLED
PyObject *
libxml_xmlXPathLangFunction(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    xmlXPathParserContextPtr ctxt;
    PyObject *pyobj_ctxt;
    int nargs;

    if (!PyArg_ParseTuple(args, (char *)"Oi:xmlXPathLangFunction", &pyobj_ctxt, &nargs))
        return(NULL);
    ctxt = (xmlXPathParserContextPtr) PyxmlXPathParserContext_Get(pyobj_ctxt);

    xmlXPathLangFunction(ctxt, nargs);
    Py_INCREF(Py_None);
    return(Py_None);
}

#endif /* LIBXML_XPATH_ENABLED */
PyObject *
libxml_xmlUCSIsCatLl(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    int c_retval;
    int code;

    if (!PyArg_ParseTuple(args, (char *)"i:xmlUCSIsCatLl", &code))
        return(NULL);

    c_retval = xmlUCSIsCatLl(code);
    py_retval = libxml_intWrap((int) c_retval);
    return(py_retval);
}

#ifdef LIBXML_XPATH_ENABLED
PyObject *
libxml_xmlXPathEvalExpression(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    xmlXPathObjectPtr c_retval;
    xmlChar * str;
    xmlXPathContextPtr ctxt;
    PyObject *pyobj_ctxt;

    if (!PyArg_ParseTuple(args, (char *)"zO:xmlXPathEvalExpression", &str, &pyobj_ctxt))
        return(NULL);
    ctxt = (xmlXPathContextPtr) PyxmlXPathContext_Get(pyobj_ctxt);

    c_retval = xmlXPathEvalExpression(str, ctxt);
    py_retval = libxml_xmlXPathObjectPtrWrap((xmlXPathObjectPtr) c_retval);
    return(py_retval);
}

#endif /* LIBXML_XPATH_ENABLED */
#ifdef LIBXML_DEBUG_ENABLED
PyObject *
libxml_xmlShellPrintNode(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    xmlNodePtr node;
    PyObject *pyobj_node;

    if (!PyArg_ParseTuple(args, (char *)"O:xmlShellPrintNode", &pyobj_node))
        return(NULL);
    node = (xmlNodePtr) PyxmlNode_Get(pyobj_node);

    xmlShellPrintNode(node);
    Py_INCREF(Py_None);
    return(Py_None);
}

#endif /* LIBXML_DEBUG_ENABLED */
#ifdef LIBXML_SCHEMAS_ENABLED
PyObject *
libxml_xmlRelaxNGValidateDoc(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    int c_retval;
    xmlRelaxNGValidCtxtPtr ctxt;
    PyObject *pyobj_ctxt;
    xmlDocPtr doc;
    PyObject *pyobj_doc;

    if (!PyArg_ParseTuple(args, (char *)"OO:xmlRelaxNGValidateDoc", &pyobj_ctxt, &pyobj_doc))
        return(NULL);
    ctxt = (xmlRelaxNGValidCtxtPtr) PyrelaxNgValidCtxt_Get(pyobj_ctxt);
    doc = (xmlDocPtr) PyxmlNode_Get(pyobj_doc);

    c_retval = xmlRelaxNGValidateDoc(ctxt, doc);
    py_retval = libxml_intWrap((int) c_retval);
    return(py_retval);
}

#endif /* LIBXML_SCHEMAS_ENABLED */
PyObject *
libxml_xmlConvertSGMLCatalog(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    int c_retval;
    xmlCatalogPtr catal;
    PyObject *pyobj_catal;

    if (!PyArg_ParseTuple(args, (char *)"O:xmlConvertSGMLCatalog", &pyobj_catal))
        return(NULL);
    catal = (xmlCatalogPtr) Pycatalog_Get(pyobj_catal);

    c_retval = xmlConvertSGMLCatalog(catal);
    py_retval = libxml_intWrap((int) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlNewChild(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    xmlNodePtr c_retval;
    xmlNodePtr parent;
    PyObject *pyobj_parent;
    xmlNsPtr ns;
    PyObject *pyobj_ns;
    xmlChar * name;
    xmlChar * content;

    if (!PyArg_ParseTuple(args, (char *)"OOzz:xmlNewChild", &pyobj_parent, &pyobj_ns, &name, &content))
        return(NULL);
    parent = (xmlNodePtr) PyxmlNode_Get(pyobj_parent);
    ns = (xmlNsPtr) PyxmlNode_Get(pyobj_ns);

    c_retval = xmlNewChild(parent, ns, name, content);
    py_retval = libxml_xmlNodePtrWrap((xmlNodePtr) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlUCSIsGeorgian(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    int c_retval;
    int code;

    if (!PyArg_ParseTuple(args, (char *)"i:xmlUCSIsGeorgian", &code))
        return(NULL);

    c_retval = xmlUCSIsGeorgian(code);
    py_retval = libxml_intWrap((int) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlUCSIsKangxiRadicals(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    int c_retval;
    int code;

    if (!PyArg_ParseTuple(args, (char *)"i:xmlUCSIsKangxiRadicals", &code))
        return(NULL);

    c_retval = xmlUCSIsKangxiRadicals(code);
    py_retval = libxml_intWrap((int) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlUTF8Strsub(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    xmlChar * c_retval;
    xmlChar * utf;
    int start;
    int len;

    if (!PyArg_ParseTuple(args, (char *)"zii:xmlUTF8Strsub", &utf, &start, &len))
        return(NULL);

    c_retval = xmlUTF8Strsub(utf, start, len);
    py_retval = libxml_xmlCharPtrWrap((xmlChar *) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlValidNormalizeAttributeValue(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    xmlChar * c_retval;
    xmlDocPtr doc;
    PyObject *pyobj_doc;
    xmlNodePtr elem;
    PyObject *pyobj_elem;
    xmlChar * name;
    xmlChar * value;

    if (!PyArg_ParseTuple(args, (char *)"OOzz:xmlValidNormalizeAttributeValue", &pyobj_doc, &pyobj_elem, &name, &value))
        return(NULL);
    doc = (xmlDocPtr) PyxmlNode_Get(pyobj_doc);
    elem = (xmlNodePtr) PyxmlNode_Get(pyobj_elem);

    c_retval = xmlValidNormalizeAttributeValue(doc, elem, name, value);
    py_retval = libxml_xmlCharPtrWrap((xmlChar *) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlNodeSetSpacePreserve(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    xmlNodePtr cur;
    PyObject *pyobj_cur;
    int val;

    if (!PyArg_ParseTuple(args, (char *)"Oi:xmlNodeSetSpacePreserve", &pyobj_cur, &val))
        return(NULL);
    cur = (xmlNodePtr) PyxmlNode_Get(pyobj_cur);

    xmlNodeSetSpacePreserve(cur, val);
    Py_INCREF(Py_None);
    return(Py_None);
}

#ifdef LIBXML_XPATH_ENABLED
PyObject *
libxml_xmlXPathNewContext(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    xmlXPathContextPtr c_retval;
    xmlDocPtr doc;
    PyObject *pyobj_doc;

    if (!PyArg_ParseTuple(args, (char *)"O:xmlXPathNewContext", &pyobj_doc))
        return(NULL);
    doc = (xmlDocPtr) PyxmlNode_Get(pyobj_doc);

    c_retval = xmlXPathNewContext(doc);
    py_retval = libxml_xmlXPathContextPtrWrap((xmlXPathContextPtr) c_retval);
    return(py_retval);
}

#endif /* LIBXML_XPATH_ENABLED */
PyObject *
libxml_xmlTextReaderLocalName(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    xmlChar * c_retval;
    xmlTextReaderPtr reader;
    PyObject *pyobj_reader;

    if (!PyArg_ParseTuple(args, (char *)"O:xmlTextReaderLocalName", &pyobj_reader))
        return(NULL);
    reader = (xmlTextReaderPtr) PyxmlTextReader_Get(pyobj_reader);

    c_retval = xmlTextReaderLocalName(reader);
    py_retval = libxml_xmlCharPtrWrap((xmlChar *) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlUCSIsHalfwidthandFullwidthForms(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    int c_retval;
    int code;

    if (!PyArg_ParseTuple(args, (char *)"i:xmlUCSIsHalfwidthandFullwidthForms", &code))
        return(NULL);

    c_retval = xmlUCSIsHalfwidthandFullwidthForms(code);
    py_retval = libxml_intWrap((int) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlParsePubidLiteral(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    xmlChar * c_retval;
    xmlParserCtxtPtr ctxt;
    PyObject *pyobj_ctxt;

    if (!PyArg_ParseTuple(args, (char *)"O:xmlParsePubidLiteral", &pyobj_ctxt))
        return(NULL);
    ctxt = (xmlParserCtxtPtr) PyparserCtxt_Get(pyobj_ctxt);

    c_retval = xmlParsePubidLiteral(ctxt);
    py_retval = libxml_xmlCharPtrWrap((xmlChar *) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlCreateIntSubset(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    xmlDtdPtr c_retval;
    xmlDocPtr doc;
    PyObject *pyobj_doc;
    xmlChar * name;
    xmlChar * ExternalID;
    xmlChar * SystemID;

    if (!PyArg_ParseTuple(args, (char *)"Ozzz:xmlCreateIntSubset", &pyobj_doc, &name, &ExternalID, &SystemID))
        return(NULL);
    doc = (xmlDocPtr) PyxmlNode_Get(pyobj_doc);

    c_retval = xmlCreateIntSubset(doc, name, ExternalID, SystemID);
    py_retval = libxml_xmlNodePtrWrap((xmlNodePtr) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlNewCharRef(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    xmlNodePtr c_retval;
    xmlDocPtr doc;
    PyObject *pyobj_doc;
    xmlChar * name;

    if (!PyArg_ParseTuple(args, (char *)"Oz:xmlNewCharRef", &pyobj_doc, &name))
        return(NULL);
    doc = (xmlDocPtr) PyxmlNode_Get(pyobj_doc);

    c_retval = xmlNewCharRef(doc, name);
    py_retval = libxml_xmlNodePtrWrap((xmlNodePtr) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlUCSIsArabic(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    int c_retval;
    int code;

    if (!PyArg_ParseTuple(args, (char *)"i:xmlUCSIsArabic", &code))
        return(NULL);

    c_retval = xmlUCSIsArabic(code);
    py_retval = libxml_intWrap((int) c_retval);
    return(py_retval);
}

#ifdef LIBXML_XPATH_ENABLED
PyObject *
libxml_xmlXPathSubValues(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    xmlXPathParserContextPtr ctxt;
    PyObject *pyobj_ctxt;

    if (!PyArg_ParseTuple(args, (char *)"O:xmlXPathSubValues", &pyobj_ctxt))
        return(NULL);
    ctxt = (xmlXPathParserContextPtr) PyxmlXPathParserContext_Get(pyobj_ctxt);

    xmlXPathSubValues(ctxt);
    Py_INCREF(Py_None);
    return(Py_None);
}

#endif /* LIBXML_XPATH_ENABLED */
PyObject *
libxml_xmlUCSIsArabicPresentationFormsA(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    int c_retval;
    int code;

    if (!PyArg_ParseTuple(args, (char *)"i:xmlUCSIsArabicPresentationFormsA", &code))
        return(NULL);

    c_retval = xmlUCSIsArabicPresentationFormsA(code);
    py_retval = libxml_intWrap((int) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlIsMixedElement(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    int c_retval;
    xmlDocPtr doc;
    PyObject *pyobj_doc;
    xmlChar * name;

    if (!PyArg_ParseTuple(args, (char *)"Oz:xmlIsMixedElement", &pyobj_doc, &name))
        return(NULL);
    doc = (xmlDocPtr) PyxmlNode_Get(pyobj_doc);

    c_retval = xmlIsMixedElement(doc, name);
    py_retval = libxml_intWrap((int) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlUCSIsGeometricShapes(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    int c_retval;
    int code;

    if (!PyArg_ParseTuple(args, (char *)"i:xmlUCSIsGeometricShapes", &code))
        return(NULL);

    c_retval = xmlUCSIsGeometricShapes(code);
    py_retval = libxml_intWrap((int) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlGetParameterEntity(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    xmlEntityPtr c_retval;
    xmlDocPtr doc;
    PyObject *pyobj_doc;
    xmlChar * name;

    if (!PyArg_ParseTuple(args, (char *)"Oz:xmlGetParameterEntity", &pyobj_doc, &name))
        return(NULL);
    doc = (xmlDocPtr) PyxmlNode_Get(pyobj_doc);

    c_retval = xmlGetParameterEntity(doc, name);
    py_retval = libxml_xmlNodePtrWrap((xmlNodePtr) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlParseQuotedString(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    xmlChar * c_retval;
    xmlParserCtxtPtr ctxt;
    PyObject *pyobj_ctxt;

    if (!PyArg_ParseTuple(args, (char *)"O:xmlParseQuotedString", &pyobj_ctxt))
        return(NULL);
    ctxt = (xmlParserCtxtPtr) PyparserCtxt_Get(pyobj_ctxt);

    c_retval = xmlParseQuotedString(ctxt);
    py_retval = libxml_xmlCharPtrWrap((xmlChar *) c_retval);
    return(py_retval);
}

#ifdef LIBXML_XPATH_ENABLED
PyObject *
libxml_xmlXPathCastNodeToNumber(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    double c_retval;
    xmlNodePtr node;
    PyObject *pyobj_node;

    if (!PyArg_ParseTuple(args, (char *)"O:xmlXPathCastNodeToNumber", &pyobj_node))
        return(NULL);
    node = (xmlNodePtr) PyxmlNode_Get(pyobj_node);

    c_retval = xmlXPathCastNodeToNumber(node);
    py_retval = libxml_doubleWrap((double) c_retval);
    return(py_retval);
}

#endif /* LIBXML_XPATH_ENABLED */
PyObject *
libxml_xmlGetPredefinedEntity(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    xmlEntityPtr c_retval;
    xmlChar * name;

    if (!PyArg_ParseTuple(args, (char *)"z:xmlGetPredefinedEntity", &name))
        return(NULL);

    c_retval = xmlGetPredefinedEntity(name);
    py_retval = libxml_xmlNodePtrWrap((xmlNodePtr) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlNewText(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    xmlNodePtr c_retval;
    xmlChar * content;

    if (!PyArg_ParseTuple(args, (char *)"z:xmlNewText", &content))
        return(NULL);

    c_retval = xmlNewText(content);
    py_retval = libxml_xmlNodePtrWrap((xmlNodePtr) c_retval);
    return(py_retval);
}

#ifdef LIBXML_XPATH_ENABLED
PyObject *
libxml_xmlXPathRegisterAllFunctions(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    xmlXPathContextPtr ctxt;
    PyObject *pyobj_ctxt;

    if (!PyArg_ParseTuple(args, (char *)"O:xmlXPathRegisterAllFunctions", &pyobj_ctxt))
        return(NULL);
    ctxt = (xmlXPathContextPtr) PyxmlXPathContext_Get(pyobj_ctxt);

    xmlXPathRegisterAllFunctions(ctxt);
    Py_INCREF(Py_None);
    return(Py_None);
}

#endif /* LIBXML_XPATH_ENABLED */
PyObject *
libxml_xmlSaveFile(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    int c_retval;
    char * filename;
    xmlDocPtr cur;
    PyObject *pyobj_cur;

    if (!PyArg_ParseTuple(args, (char *)"zO:xmlSaveFile", &filename, &pyobj_cur))
        return(NULL);
    cur = (xmlDocPtr) PyxmlNode_Get(pyobj_cur);

    c_retval = xmlSaveFile(filename, cur);
    py_retval = libxml_intWrap((int) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlNewNs(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    xmlNsPtr c_retval;
    xmlNodePtr node;
    PyObject *pyobj_node;
    xmlChar * href;
    xmlChar * prefix;

    if (!PyArg_ParseTuple(args, (char *)"Ozz:xmlNewNs", &pyobj_node, &href, &prefix))
        return(NULL);
    node = (xmlNodePtr) PyxmlNode_Get(pyobj_node);

    c_retval = xmlNewNs(node, href, prefix);
    py_retval = libxml_xmlNsPtrWrap((xmlNsPtr) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlACatalogResolvePublic(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    xmlChar * c_retval;
    xmlCatalogPtr catal;
    PyObject *pyobj_catal;
    xmlChar * pubID;

    if (!PyArg_ParseTuple(args, (char *)"Oz:xmlACatalogResolvePublic", &pyobj_catal, &pubID))
        return(NULL);
    catal = (xmlCatalogPtr) Pycatalog_Get(pyobj_catal);

    c_retval = xmlACatalogResolvePublic(catal, pubID);
    py_retval = libxml_xmlCharPtrWrap((xmlChar *) c_retval);
    return(py_retval);
}

#ifdef LIBXML_XPATH_ENABLED
PyObject *
libxml_xmlXPathNextNamespace(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    xmlNodePtr c_retval;
    xmlXPathParserContextPtr ctxt;
    PyObject *pyobj_ctxt;
    xmlNodePtr cur;
    PyObject *pyobj_cur;

    if (!PyArg_ParseTuple(args, (char *)"OO:xmlXPathNextNamespace", &pyobj_ctxt, &pyobj_cur))
        return(NULL);
    ctxt = (xmlXPathParserContextPtr) PyxmlXPathParserContext_Get(pyobj_ctxt);
    cur = (xmlNodePtr) PyxmlNode_Get(pyobj_cur);

    c_retval = xmlXPathNextNamespace(ctxt, cur);
    py_retval = libxml_xmlNodePtrWrap((xmlNodePtr) c_retval);
    return(py_retval);
}

#endif /* LIBXML_XPATH_ENABLED */
PyObject *
libxml_xmlStopParser(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    xmlParserCtxtPtr ctxt;
    PyObject *pyobj_ctxt;

    if (!PyArg_ParseTuple(args, (char *)"O:xmlStopParser", &pyobj_ctxt))
        return(NULL);
    ctxt = (xmlParserCtxtPtr) PyparserCtxt_Get(pyobj_ctxt);

    xmlStopParser(ctxt);
    Py_INCREF(Py_None);
    return(Py_None);
}

PyObject *
libxml_xmlNewDocText(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    xmlNodePtr c_retval;
    xmlDocPtr doc;
    PyObject *pyobj_doc;
    xmlChar * content;

    if (!PyArg_ParseTuple(args, (char *)"Oz:xmlNewDocText", &pyobj_doc, &content))
        return(NULL);
    doc = (xmlDocPtr) PyxmlNode_Get(pyobj_doc);

    c_retval = xmlNewDocText(doc, content);
    py_retval = libxml_xmlNodePtrWrap((xmlNodePtr) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlACatalogResolveSystem(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    xmlChar * c_retval;
    xmlCatalogPtr catal;
    PyObject *pyobj_catal;
    xmlChar * sysID;

    if (!PyArg_ParseTuple(args, (char *)"Oz:xmlACatalogResolveSystem", &pyobj_catal, &sysID))
        return(NULL);
    catal = (xmlCatalogPtr) Pycatalog_Get(pyobj_catal);

    c_retval = xmlACatalogResolveSystem(catal, sysID);
    py_retval = libxml_xmlCharPtrWrap((xmlChar *) c_retval);
    return(py_retval);
}

#ifdef LIBXML_XPATH_ENABLED
PyObject *
libxml_xmlXPathNewFloat(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    xmlXPathObjectPtr c_retval;
    double val;

    if (!PyArg_ParseTuple(args, (char *)"d:xmlXPathNewFloat", &val))
        return(NULL);

    c_retval = xmlXPathNewFloat(val);
    py_retval = libxml_xmlXPathObjectPtrWrap((xmlXPathObjectPtr) c_retval);
    return(py_retval);
}

#endif /* LIBXML_XPATH_ENABLED */
PyObject *
libxml_xmlCatalogConvert(ATTRIBUTE_UNUSED PyObject *self,ATTRIBUTE_UNUSED  PyObject *args) {
    PyObject *py_retval;
    int c_retval;

    c_retval = xmlCatalogConvert();
    py_retval = libxml_intWrap((int) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlNewReference(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    xmlNodePtr c_retval;
    xmlDocPtr doc;
    PyObject *pyobj_doc;
    xmlChar * name;

    if (!PyArg_ParseTuple(args, (char *)"Oz:xmlNewReference", &pyobj_doc, &name))
        return(NULL);
    doc = (xmlDocPtr) PyxmlNode_Get(pyobj_doc);

    c_retval = xmlNewReference(doc, name);
    py_retval = libxml_xmlNodePtrWrap((xmlNodePtr) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlUCSIsCJKSymbolsandPunctuation(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    int c_retval;
    int code;

    if (!PyArg_ParseTuple(args, (char *)"i:xmlUCSIsCJKSymbolsandPunctuation", &code))
        return(NULL);

    c_retval = xmlUCSIsCJKSymbolsandPunctuation(code);
    py_retval = libxml_intWrap((int) c_retval);
    return(py_retval);
}

#ifdef LIBXML_HTML_ENABLED
PyObject *
libxml_htmlDocDump(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    int c_retval;
    FILE * f;
    PyObject *pyobj_f;
    xmlDocPtr cur;
    PyObject *pyobj_cur;

    if (!PyArg_ParseTuple(args, (char *)"OO:htmlDocDump", &pyobj_f, &pyobj_cur))
        return(NULL);
    f = (FILE *) PyFile_Get(pyobj_f);
    cur = (xmlDocPtr) PyxmlNode_Get(pyobj_cur);

    c_retval = htmlDocDump(f, cur);
    py_retval = libxml_intWrap((int) c_retval);
    return(py_retval);
}

#endif /* LIBXML_HTML_ENABLED */
PyObject *
libxml_xmlUCSIsMusicalSymbols(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    int c_retval;
    int code;

    if (!PyArg_ParseTuple(args, (char *)"i:xmlUCSIsMusicalSymbols", &code))
        return(NULL);

    c_retval = xmlUCSIsMusicalSymbols(code);
    py_retval = libxml_intWrap((int) c_retval);
    return(py_retval);
}

#ifdef LIBXML_XPATH_ENABLED
PyObject *
libxml_xmlXPathParseName(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    xmlChar * c_retval;
    xmlXPathParserContextPtr ctxt;
    PyObject *pyobj_ctxt;

    if (!PyArg_ParseTuple(args, (char *)"O:xmlXPathParseName", &pyobj_ctxt))
        return(NULL);
    ctxt = (xmlXPathParserContextPtr) PyxmlXPathParserContext_Get(pyobj_ctxt);

    c_retval = xmlXPathParseName(ctxt);
    py_retval = libxml_xmlCharPtrWrap((xmlChar *) c_retval);
    return(py_retval);
}

#endif /* LIBXML_XPATH_ENABLED */
PyObject *
libxml_xmlNanoFTPInit(ATTRIBUTE_UNUSED PyObject *self,ATTRIBUTE_UNUSED  PyObject *args) {

    xmlNanoFTPInit();
    Py_INCREF(Py_None);
    return(Py_None);
}

PyObject *
libxml_xmlFreeNodeList(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    xmlNodePtr cur;
    PyObject *pyobj_cur;

    if (!PyArg_ParseTuple(args, (char *)"O:xmlFreeNodeList", &pyobj_cur))
        return(NULL);
    cur = (xmlNodePtr) PyxmlNode_Get(pyobj_cur);

    xmlFreeNodeList(cur);
    Py_INCREF(Py_None);
    return(Py_None);
}

PyObject *
libxml_xmlValidateNMToken(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    int c_retval;
    xmlChar * value;
    int space;

    if (!PyArg_ParseTuple(args, (char *)"zi:xmlValidateNMToken", &value, &space))
        return(NULL);

    c_retval = xmlValidateNMToken(value, space);
    py_retval = libxml_intWrap((int) c_retval);
    return(py_retval);
}

#ifdef LIBXML_XPATH_ENABLED
PyObject *
libxml_xmlXPathDivValues(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    xmlXPathParserContextPtr ctxt;
    PyObject *pyobj_ctxt;

    if (!PyArg_ParseTuple(args, (char *)"O:xmlXPathDivValues", &pyobj_ctxt))
        return(NULL);
    ctxt = (xmlXPathParserContextPtr) PyxmlXPathParserContext_Get(pyobj_ctxt);

    xmlXPathDivValues(ctxt);
    Py_INCREF(Py_None);
    return(Py_None);
}

#endif /* LIBXML_XPATH_ENABLED */
PyObject *
libxml_xmlUCSIsCatNd(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    int c_retval;
    int code;

    if (!PyArg_ParseTuple(args, (char *)"i:xmlUCSIsCatNd", &code))
        return(NULL);

    c_retval = xmlUCSIsCatNd(code);
    py_retval = libxml_intWrap((int) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlUCSIsTelugu(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    int c_retval;
    int code;

    if (!PyArg_ParseTuple(args, (char *)"i:xmlUCSIsTelugu", &code))
        return(NULL);

    c_retval = xmlUCSIsTelugu(code);
    py_retval = libxml_intWrap((int) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlOutputBufferWriteString(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    int c_retval;
    xmlOutputBufferPtr out;
    PyObject *pyobj_out;
    char * str;

    if (!PyArg_ParseTuple(args, (char *)"Oz:xmlOutputBufferWriteString", &pyobj_out, &str))
        return(NULL);
    out = (xmlOutputBufferPtr) PyoutputBuffer_Get(pyobj_out);

    c_retval = xmlOutputBufferWriteString(out, str);
    py_retval = libxml_intWrap((int) c_retval);
    return(py_retval);
}

#ifdef LIBXML_DEBUG_ENABLED
PyObject *
libxml_xmlLsCountNode(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    int c_retval;
    xmlNodePtr node;
    PyObject *pyobj_node;

    if (!PyArg_ParseTuple(args, (char *)"O:xmlLsCountNode", &pyobj_node))
        return(NULL);
    node = (xmlNodePtr) PyxmlNode_Get(pyobj_node);

    c_retval = xmlLsCountNode(node);
    py_retval = libxml_intWrap((int) c_retval);
    return(py_retval);
}

#endif /* LIBXML_DEBUG_ENABLED */
PyObject *
libxml_xmlParseCatalogFile(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    xmlDocPtr c_retval;
    char * filename;

    if (!PyArg_ParseTuple(args, (char *)"z:xmlParseCatalogFile", &filename))
        return(NULL);

    c_retval = xmlParseCatalogFile(filename);
    py_retval = libxml_xmlDocPtrWrap((xmlDocPtr) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlSetListDoc(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    xmlNodePtr list;
    PyObject *pyobj_list;
    xmlDocPtr doc;
    PyObject *pyobj_doc;

    if (!PyArg_ParseTuple(args, (char *)"OO:xmlSetListDoc", &pyobj_list, &pyobj_doc))
        return(NULL);
    list = (xmlNodePtr) PyxmlNode_Get(pyobj_list);
    doc = (xmlDocPtr) PyxmlNode_Get(pyobj_doc);

    xmlSetListDoc(list, doc);
    Py_INCREF(Py_None);
    return(Py_None);
}

PyObject *
libxml_xmlURISetPath(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    xmlURIPtr URI;
    PyObject *pyobj_URI;
    char * path;

    if (!PyArg_ParseTuple(args, (char *)"Oz:xmlURISetPath", &pyobj_URI, &path))
        return(NULL);
    URI = (xmlURIPtr) PyURI_Get(pyobj_URI);

    if (URI->path != NULL) xmlFree(URI->path);
    URI->path = xmlStrdup((const xmlChar *)path);
    Py_INCREF(Py_None);
    return(Py_None);
}

PyObject *
libxml_xmlXPathGetFunctionURI(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    const xmlChar * c_retval;
    xmlXPathContextPtr ctxt;
    PyObject *pyobj_ctxt;

    if (!PyArg_ParseTuple(args, (char *)"O:xmlXPathGetFunctionURI", &pyobj_ctxt))
        return(NULL);
    ctxt = (xmlXPathContextPtr) PyxmlXPathContext_Get(pyobj_ctxt);

    c_retval = ctxt->functionURI;
    py_retval = libxml_xmlCharPtrConstWrap((const xmlChar *) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlOutputBufferWrite(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    int c_retval;
    xmlOutputBufferPtr out;
    PyObject *pyobj_out;
    int len;
    char * buf;

    if (!PyArg_ParseTuple(args, (char *)"Oiz:xmlOutputBufferWrite", &pyobj_out, &len, &buf))
        return(NULL);
    out = (xmlOutputBufferPtr) PyoutputBuffer_Get(pyobj_out);

    c_retval = xmlOutputBufferWrite(out, len, buf);
    py_retval = libxml_intWrap((int) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlUCSIsLao(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    int c_retval;
    int code;

    if (!PyArg_ParseTuple(args, (char *)"i:xmlUCSIsLao", &code))
        return(NULL);

    c_retval = xmlUCSIsLao(code);
    py_retval = libxml_intWrap((int) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlNanoHTTPScanProxy(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    char * URL;

    if (!PyArg_ParseTuple(args, (char *)"z:xmlNanoHTTPScanProxy", &URL))
        return(NULL);

    xmlNanoHTTPScanProxy(URL);
    Py_INCREF(Py_None);
    return(Py_None);
}

#ifdef LIBXML_SCHEMAS_ENABLED
PyObject *
libxml_xmlRelaxNGCleanupTypes(ATTRIBUTE_UNUSED PyObject *self,ATTRIBUTE_UNUSED  PyObject *args) {

    xmlRelaxNGCleanupTypes();
    Py_INCREF(Py_None);
    return(Py_None);
}

#endif /* LIBXML_SCHEMAS_ENABLED */
#ifdef LIBXML_SCHEMAS_ENABLED
PyObject *
libxml_xmlRelaxNGNewParserCtxt(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    xmlRelaxNGParserCtxtPtr c_retval;
    char * URL;

    if (!PyArg_ParseTuple(args, (char *)"z:xmlRelaxNGNewParserCtxt", &URL))
        return(NULL);

    c_retval = xmlRelaxNGNewParserCtxt(URL);
    py_retval = libxml_xmlRelaxNGParserCtxtPtrWrap((xmlRelaxNGParserCtxtPtr) c_retval);
    return(py_retval);
}

#endif /* LIBXML_SCHEMAS_ENABLED */
PyObject *
libxml_xmlGetID(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    xmlAttrPtr c_retval;
    xmlDocPtr doc;
    PyObject *pyobj_doc;
    xmlChar * ID;

    if (!PyArg_ParseTuple(args, (char *)"Oz:xmlGetID", &pyobj_doc, &ID))
        return(NULL);
    doc = (xmlDocPtr) PyxmlNode_Get(pyobj_doc);

    c_retval = xmlGetID(doc, ID);
    py_retval = libxml_xmlNodePtrWrap((xmlNodePtr) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlGetLastChild(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    xmlNodePtr c_retval;
    xmlNodePtr parent;
    PyObject *pyobj_parent;

    if (!PyArg_ParseTuple(args, (char *)"O:xmlGetLastChild", &pyobj_parent))
        return(NULL);
    parent = (xmlNodePtr) PyxmlNode_Get(pyobj_parent);

    c_retval = xmlGetLastChild(parent);
    py_retval = libxml_xmlNodePtrWrap((xmlNodePtr) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlGetEncodingAlias(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    const char * c_retval;
    char * alias;

    if (!PyArg_ParseTuple(args, (char *)"z:xmlGetEncodingAlias", &alias))
        return(NULL);

    c_retval = xmlGetEncodingAlias(alias);
    py_retval = libxml_charPtrConstWrap((const char *) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlACatalogAdd(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    int c_retval;
    xmlCatalogPtr catal;
    PyObject *pyobj_catal;
    xmlChar * type;
    xmlChar * orig;
    xmlChar * replace;

    if (!PyArg_ParseTuple(args, (char *)"Ozzz:xmlACatalogAdd", &pyobj_catal, &type, &orig, &replace))
        return(NULL);
    catal = (xmlCatalogPtr) Pycatalog_Get(pyobj_catal);

    c_retval = xmlACatalogAdd(catal, type, orig, replace);
    py_retval = libxml_intWrap((int) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlAddDtdEntity(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    xmlEntityPtr c_retval;
    xmlDocPtr doc;
    PyObject *pyobj_doc;
    xmlChar * name;
    int type;
    xmlChar * ExternalID;
    xmlChar * SystemID;
    xmlChar * content;

    if (!PyArg_ParseTuple(args, (char *)"Ozizzz:xmlAddDtdEntity", &pyobj_doc, &name, &type, &ExternalID, &SystemID, &content))
        return(NULL);
    doc = (xmlDocPtr) PyxmlNode_Get(pyobj_doc);

    c_retval = xmlAddDtdEntity(doc, name, type, ExternalID, SystemID, content);
    py_retval = libxml_xmlNodePtrWrap((xmlNodePtr) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlNewNsPropEatName(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    xmlAttrPtr c_retval;
    xmlNodePtr node;
    PyObject *pyobj_node;
    xmlNsPtr ns;
    PyObject *pyobj_ns;
    xmlChar * name;
    xmlChar * value;

    if (!PyArg_ParseTuple(args, (char *)"OOzz:xmlNewNsPropEatName", &pyobj_node, &pyobj_ns, &name, &value))
        return(NULL);
    node = (xmlNodePtr) PyxmlNode_Get(pyobj_node);
    ns = (xmlNsPtr) PyxmlNode_Get(pyobj_ns);

    c_retval = xmlNewNsPropEatName(node, ns, name, value);
    py_retval = libxml_xmlNodePtrWrap((xmlNodePtr) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlSaveFormatFileTo(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    int c_retval;
    xmlOutputBufferPtr buf;
    PyObject *pyobj_buf;
    xmlDocPtr cur;
    PyObject *pyobj_cur;
    char * encoding;
    int format;

    if (!PyArg_ParseTuple(args, (char *)"OOzi:xmlSaveFormatFileTo", &pyobj_buf, &pyobj_cur, &encoding, &format))
        return(NULL);
    buf = (xmlOutputBufferPtr) PyoutputBuffer_Get(pyobj_buf);
    cur = (xmlDocPtr) PyxmlNode_Get(pyobj_cur);

    c_retval = xmlSaveFormatFileTo(buf, cur, encoding, format);
    py_retval = libxml_intWrap((int) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlNodeSetBase(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    xmlNodePtr cur;
    PyObject *pyobj_cur;
    xmlChar * uri;

    if (!PyArg_ParseTuple(args, (char *)"Oz:xmlNodeSetBase", &pyobj_cur, &uri))
        return(NULL);
    cur = (xmlNodePtr) PyxmlNode_Get(pyobj_cur);

    xmlNodeSetBase(cur, uri);
    Py_INCREF(Py_None);
    return(Py_None);
}

PyObject *
libxml_xmlUCSIsCatNl(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    int c_retval;
    int code;

    if (!PyArg_ParseTuple(args, (char *)"i:xmlUCSIsCatNl", &code))
        return(NULL);

    c_retval = xmlUCSIsCatNl(code);
    py_retval = libxml_intWrap((int) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlUCSIsCombiningHalfMarks(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    int c_retval;
    int code;

    if (!PyArg_ParseTuple(args, (char *)"i:xmlUCSIsCombiningHalfMarks", &code))
        return(NULL);

    c_retval = xmlUCSIsCombiningHalfMarks(code);
    py_retval = libxml_intWrap((int) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlParseNotationDecl(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    xmlParserCtxtPtr ctxt;
    PyObject *pyobj_ctxt;

    if (!PyArg_ParseTuple(args, (char *)"O:xmlParseNotationDecl", &pyobj_ctxt))
        return(NULL);
    ctxt = (xmlParserCtxtPtr) PyparserCtxt_Get(pyobj_ctxt);

    xmlParseNotationDecl(ctxt);
    Py_INCREF(Py_None);
    return(Py_None);
}

PyObject *
libxml_xmlUCSIsTibetan(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    int c_retval;
    int code;

    if (!PyArg_ParseTuple(args, (char *)"i:xmlUCSIsTibetan", &code))
        return(NULL);

    c_retval = xmlUCSIsTibetan(code);
    py_retval = libxml_intWrap((int) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlUCSIsYiRadicals(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    int c_retval;
    int code;

    if (!PyArg_ParseTuple(args, (char *)"i:xmlUCSIsYiRadicals", &code))
        return(NULL);

    c_retval = xmlUCSIsYiRadicals(code);
    py_retval = libxml_intWrap((int) c_retval);
    return(py_retval);
}

#ifdef LIBXML_XPTR_ENABLED
PyObject *
libxml_xmlXPtrNewLocationSetNodes(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    xmlXPathObjectPtr c_retval;
    xmlNodePtr start;
    PyObject *pyobj_start;
    xmlNodePtr end;
    PyObject *pyobj_end;

    if (!PyArg_ParseTuple(args, (char *)"OO:xmlXPtrNewLocationSetNodes", &pyobj_start, &pyobj_end))
        return(NULL);
    start = (xmlNodePtr) PyxmlNode_Get(pyobj_start);
    end = (xmlNodePtr) PyxmlNode_Get(pyobj_end);

    c_retval = xmlXPtrNewLocationSetNodes(start, end);
    py_retval = libxml_xmlXPathObjectPtrWrap((xmlXPathObjectPtr) c_retval);
    return(py_retval);
}

#endif /* LIBXML_XPTR_ENABLED */
PyObject *
libxml_xmlUCSIsCombiningMarksforSymbols(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    int c_retval;
    int code;

    if (!PyArg_ParseTuple(args, (char *)"i:xmlUCSIsCombiningMarksforSymbols", &code))
        return(NULL);

    c_retval = xmlUCSIsCombiningMarksforSymbols(code);
    py_retval = libxml_intWrap((int) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlUCSIsBasicLatin(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    int c_retval;
    int code;

    if (!PyArg_ParseTuple(args, (char *)"i:xmlUCSIsBasicLatin", &code))
        return(NULL);

    c_retval = xmlUCSIsBasicLatin(code);
    py_retval = libxml_intWrap((int) c_retval);
    return(py_retval);
}

#ifdef LIBXML_XPATH_ENABLED
PyObject *
libxml_xmlXPathNewCString(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    xmlXPathObjectPtr c_retval;
    char * val;

    if (!PyArg_ParseTuple(args, (char *)"z:xmlXPathNewCString", &val))
        return(NULL);

    c_retval = xmlXPathNewCString(val);
    py_retval = libxml_xmlXPathObjectPtrWrap((xmlXPathObjectPtr) c_retval);
    return(py_retval);
}

#endif /* LIBXML_XPATH_ENABLED */
PyObject *
libxml_xmlParseMisc(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    xmlParserCtxtPtr ctxt;
    PyObject *pyobj_ctxt;

    if (!PyArg_ParseTuple(args, (char *)"O:xmlParseMisc", &pyobj_ctxt))
        return(NULL);
    ctxt = (xmlParserCtxtPtr) PyparserCtxt_Get(pyobj_ctxt);

    xmlParseMisc(ctxt);
    Py_INCREF(Py_None);
    return(Py_None);
}

PyObject *
libxml_xmlParserInputBufferGrow(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    int c_retval;
    xmlParserInputBufferPtr in;
    PyObject *pyobj_in;
    int len;

    if (!PyArg_ParseTuple(args, (char *)"Oi:xmlParserInputBufferGrow", &pyobj_in, &len))
        return(NULL);
    in = (xmlParserInputBufferPtr) PyinputBuffer_Get(pyobj_in);

    c_retval = xmlParserInputBufferGrow(in, len);
    py_retval = libxml_intWrap((int) c_retval);
    return(py_retval);
}

#ifdef LIBXML_XINCLUDE_ENABLED
PyObject *
libxml_xmlXIncludeProcess(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    int c_retval;
    xmlDocPtr doc;
    PyObject *pyobj_doc;

    if (!PyArg_ParseTuple(args, (char *)"O:xmlXIncludeProcess", &pyobj_doc))
        return(NULL);
    doc = (xmlDocPtr) PyxmlNode_Get(pyobj_doc);

    c_retval = xmlXIncludeProcess(doc);
    py_retval = libxml_intWrap((int) c_retval);
    return(py_retval);
}

#endif /* LIBXML_XINCLUDE_ENABLED */
PyObject *
libxml_xmlUCSIsCatSo(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    int c_retval;
    int code;

    if (!PyArg_ParseTuple(args, (char *)"i:xmlUCSIsCatSo", &code))
        return(NULL);

    c_retval = xmlUCSIsCatSo(code);
    py_retval = libxml_intWrap((int) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlNewDocFragment(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    xmlNodePtr c_retval;
    xmlDocPtr doc;
    PyObject *pyobj_doc;

    if (!PyArg_ParseTuple(args, (char *)"O:xmlNewDocFragment", &pyobj_doc))
        return(NULL);
    doc = (xmlDocPtr) PyxmlNode_Get(pyobj_doc);

    c_retval = xmlNewDocFragment(doc);
    py_retval = libxml_xmlNodePtrWrap((xmlNodePtr) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlValidateName(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    int c_retval;
    xmlChar * value;
    int space;

    if (!PyArg_ParseTuple(args, (char *)"zi:xmlValidateName", &value, &space))
        return(NULL);

    c_retval = xmlValidateName(value, space);
    py_retval = libxml_intWrap((int) c_retval);
    return(py_retval);
}

#ifdef LIBXML_XPATH_ENABLED
PyObject *
libxml_xmlXPathFreeContext(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    xmlXPathContextPtr ctxt;
    PyObject *pyobj_ctxt;

    if (!PyArg_ParseTuple(args, (char *)"O:xmlXPathFreeContext", &pyobj_ctxt))
        return(NULL);
    ctxt = (xmlXPathContextPtr) PyxmlXPathContext_Get(pyobj_ctxt);

    xmlXPathFreeContext(ctxt);
    Py_INCREF(Py_None);
    return(Py_None);
}

#endif /* LIBXML_XPATH_ENABLED */
PyObject *
libxml_xmlStrdup(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    xmlChar * c_retval;
    xmlChar * cur;

    if (!PyArg_ParseTuple(args, (char *)"z:xmlStrdup", &cur))
        return(NULL);

    c_retval = xmlStrdup(cur);
    py_retval = libxml_xmlCharPtrWrap((xmlChar *) c_retval);
    return(py_retval);
}

#ifdef LIBXML_XPATH_ENABLED
PyObject *
libxml_xmlXPathNamespaceURIFunction(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    xmlXPathParserContextPtr ctxt;
    PyObject *pyobj_ctxt;
    int nargs;

    if (!PyArg_ParseTuple(args, (char *)"Oi:xmlXPathNamespaceURIFunction", &pyobj_ctxt, &nargs))
        return(NULL);
    ctxt = (xmlXPathParserContextPtr) PyxmlXPathParserContext_Get(pyobj_ctxt);

    xmlXPathNamespaceURIFunction(ctxt, nargs);
    Py_INCREF(Py_None);
    return(Py_None);
}

#endif /* LIBXML_XPATH_ENABLED */
PyObject *
libxml_xmlCopyPropList(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    xmlAttrPtr c_retval;
    xmlNodePtr target;
    PyObject *pyobj_target;
    xmlAttrPtr cur;
    PyObject *pyobj_cur;

    if (!PyArg_ParseTuple(args, (char *)"OO:xmlCopyPropList", &pyobj_target, &pyobj_cur))
        return(NULL);
    target = (xmlNodePtr) PyxmlNode_Get(pyobj_target);
    cur = (xmlAttrPtr) PyxmlNode_Get(pyobj_cur);

    c_retval = xmlCopyPropList(target, cur);
    py_retval = libxml_xmlNodePtrWrap((xmlNodePtr) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlUCSIsCatMe(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    int c_retval;
    int code;

    if (!PyArg_ParseTuple(args, (char *)"i:xmlUCSIsCatMe", &code))
        return(NULL);

    c_retval = xmlUCSIsCatMe(code);
    py_retval = libxml_intWrap((int) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlRemoveRef(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    int c_retval;
    xmlDocPtr doc;
    PyObject *pyobj_doc;
    xmlAttrPtr attr;
    PyObject *pyobj_attr;

    if (!PyArg_ParseTuple(args, (char *)"OO:xmlRemoveRef", &pyobj_doc, &pyobj_attr))
        return(NULL);
    doc = (xmlDocPtr) PyxmlNode_Get(pyobj_doc);
    attr = (xmlAttrPtr) PyxmlNode_Get(pyobj_attr);

    c_retval = xmlRemoveRef(doc, attr);
    py_retval = libxml_intWrap((int) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlSubstituteEntitiesDefault(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    int c_retval;
    int val;

    if (!PyArg_ParseTuple(args, (char *)"i:xmlSubstituteEntitiesDefault", &val))
        return(NULL);

    c_retval = xmlSubstituteEntitiesDefault(val);
    py_retval = libxml_intWrap((int) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlStrncat(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    xmlChar * c_retval;
    xmlChar * cur;
    xmlChar * add;
    int len;

    if (!PyArg_ParseTuple(args, (char *)"zzi:xmlStrncat", &cur, &add, &len))
        return(NULL);

    c_retval = xmlStrncat(cur, add, len);
    py_retval = libxml_xmlCharPtrWrap((xmlChar *) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlUCSIsMiscellaneousSymbols(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    int c_retval;
    int code;

    if (!PyArg_ParseTuple(args, (char *)"i:xmlUCSIsMiscellaneousSymbols", &code))
        return(NULL);

    c_retval = xmlUCSIsMiscellaneousSymbols(code);
    py_retval = libxml_intWrap((int) c_retval);
    return(py_retval);
}

#ifdef LIBXML_HTML_ENABLED
PyObject *
libxml_htmlSaveFileEnc(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    int c_retval;
    char * filename;
    xmlDocPtr cur;
    PyObject *pyobj_cur;
    char * encoding;

    if (!PyArg_ParseTuple(args, (char *)"zOz:htmlSaveFileEnc", &filename, &pyobj_cur, &encoding))
        return(NULL);
    cur = (xmlDocPtr) PyxmlNode_Get(pyobj_cur);

    c_retval = htmlSaveFileEnc(filename, cur, encoding);
    py_retval = libxml_intWrap((int) c_retval);
    return(py_retval);
}

#endif /* LIBXML_HTML_ENABLED */
PyObject *
libxml_xmlTextReaderQuoteChar(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    int c_retval;
    xmlTextReaderPtr reader;
    PyObject *pyobj_reader;

    if (!PyArg_ParseTuple(args, (char *)"O:xmlTextReaderQuoteChar", &pyobj_reader))
        return(NULL);
    reader = (xmlTextReaderPtr) PyxmlTextReader_Get(pyobj_reader);

    c_retval = xmlTextReaderQuoteChar(reader);
    py_retval = libxml_intWrap((int) c_retval);
    return(py_retval);
}

PyObject *
libxml_namePop(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    xmlChar * c_retval;
    xmlParserCtxtPtr ctxt;
    PyObject *pyobj_ctxt;

    if (!PyArg_ParseTuple(args, (char *)"O:namePop", &pyobj_ctxt))
        return(NULL);
    ctxt = (xmlParserCtxtPtr) PyparserCtxt_Get(pyobj_ctxt);

    c_retval = namePop(ctxt);
    py_retval = libxml_xmlCharPtrWrap((xmlChar *) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlInitCharEncodingHandlers(ATTRIBUTE_UNUSED PyObject *self,ATTRIBUTE_UNUSED  PyObject *args) {

    xmlInitCharEncodingHandlers();
    Py_INCREF(Py_None);
    return(Py_None);
}

PyObject *
libxml_xmlACatalogResolve(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    xmlChar * c_retval;
    xmlCatalogPtr catal;
    PyObject *pyobj_catal;
    xmlChar * pubID;
    xmlChar * sysID;

    if (!PyArg_ParseTuple(args, (char *)"Ozz:xmlACatalogResolve", &pyobj_catal, &pubID, &sysID))
        return(NULL);
    catal = (xmlCatalogPtr) Pycatalog_Get(pyobj_catal);

    c_retval = xmlACatalogResolve(catal, pubID, sysID);
    py_retval = libxml_xmlCharPtrWrap((xmlChar *) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlParseContent(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    xmlParserCtxtPtr ctxt;
    PyObject *pyobj_ctxt;

    if (!PyArg_ParseTuple(args, (char *)"O:xmlParseContent", &pyobj_ctxt))
        return(NULL);
    ctxt = (xmlParserCtxtPtr) PyparserCtxt_Get(pyobj_ctxt);

    xmlParseContent(ctxt);
    Py_INCREF(Py_None);
    return(Py_None);
}

#ifdef LIBXML_REGEXP_ENABLED
PyObject *
libxml_xmlRegexpCompile(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    xmlRegexpPtr c_retval;
    xmlChar * regexp;

    if (!PyArg_ParseTuple(args, (char *)"z:xmlRegexpCompile", &regexp))
        return(NULL);

    c_retval = xmlRegexpCompile(regexp);
    py_retval = libxml_xmlRegexpPtrWrap((xmlRegexpPtr) c_retval);
    return(py_retval);
}

#endif /* LIBXML_REGEXP_ENABLED */
PyObject *
libxml_xmlParseMemory(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    xmlDocPtr c_retval;
    char * buffer;
    int size;

    if (!PyArg_ParseTuple(args, (char *)"zi:xmlParseMemory", &buffer, &size))
        return(NULL);

    c_retval = xmlParseMemory(buffer, size);
    py_retval = libxml_xmlDocPtrWrap((xmlDocPtr) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlStrcasestr(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    const xmlChar * c_retval;
    xmlChar * str;
    xmlChar * val;

    if (!PyArg_ParseTuple(args, (char *)"zz:xmlStrcasestr", &str, &val))
        return(NULL);

    c_retval = xmlStrcasestr(str, val);
    py_retval = libxml_xmlCharPtrConstWrap((const xmlChar *) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlUCSIsKannada(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    int c_retval;
    int code;

    if (!PyArg_ParseTuple(args, (char *)"i:xmlUCSIsKannada", &code))
        return(NULL);

    c_retval = xmlUCSIsKannada(code);
    py_retval = libxml_intWrap((int) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlCleanupEncodingAliases(ATTRIBUTE_UNUSED PyObject *self,ATTRIBUTE_UNUSED  PyObject *args) {

    xmlCleanupEncodingAliases();
    Py_INCREF(Py_None);
    return(Py_None);
}

PyObject *
libxml_xmlUCSIsCJKRadicalsSupplement(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    int c_retval;
    int code;

    if (!PyArg_ParseTuple(args, (char *)"i:xmlUCSIsCJKRadicalsSupplement", &code))
        return(NULL);

    c_retval = xmlUCSIsCJKRadicalsSupplement(code);
    py_retval = libxml_intWrap((int) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlSetNsProp(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    xmlAttrPtr c_retval;
    xmlNodePtr node;
    PyObject *pyobj_node;
    xmlNsPtr ns;
    PyObject *pyobj_ns;
    xmlChar * name;
    xmlChar * value;

    if (!PyArg_ParseTuple(args, (char *)"OOzz:xmlSetNsProp", &pyobj_node, &pyobj_ns, &name, &value))
        return(NULL);
    node = (xmlNodePtr) PyxmlNode_Get(pyobj_node);
    ns = (xmlNsPtr) PyxmlNode_Get(pyobj_ns);

    c_retval = xmlSetNsProp(node, ns, name, value);
    py_retval = libxml_xmlNodePtrWrap((xmlNodePtr) c_retval);
    return(py_retval);
}

#ifdef LIBXML_XPATH_ENABLED
PyObject *
libxml_xmlXPathCeilingFunction(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    xmlXPathParserContextPtr ctxt;
    PyObject *pyobj_ctxt;
    int nargs;

    if (!PyArg_ParseTuple(args, (char *)"Oi:xmlXPathCeilingFunction", &pyobj_ctxt, &nargs))
        return(NULL);
    ctxt = (xmlXPathParserContextPtr) PyxmlXPathParserContext_Get(pyobj_ctxt);

    xmlXPathCeilingFunction(ctxt, nargs);
    Py_INCREF(Py_None);
    return(Py_None);
}

#endif /* LIBXML_XPATH_ENABLED */
PyObject *
libxml_xmlFreePropList(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    xmlAttrPtr cur;
    PyObject *pyobj_cur;

    if (!PyArg_ParseTuple(args, (char *)"O:xmlFreePropList", &pyobj_cur))
        return(NULL);
    cur = (xmlAttrPtr) PyxmlNode_Get(pyobj_cur);

    xmlFreePropList(cur);
    Py_INCREF(Py_None);
    return(Py_None);
}

PyObject *
libxml_xmlUCSIsSmallFormVariants(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    int c_retval;
    int code;

    if (!PyArg_ParseTuple(args, (char *)"i:xmlUCSIsSmallFormVariants", &code))
        return(NULL);

    c_retval = xmlUCSIsSmallFormVariants(code);
    py_retval = libxml_intWrap((int) c_retval);
    return(py_retval);
}

#ifdef LIBXML_HTML_ENABLED
PyObject *
libxml_htmlParseFile(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    htmlDocPtr c_retval;
    char * filename;
    char * encoding;

    if (!PyArg_ParseTuple(args, (char *)"zz:htmlParseFile", &filename, &encoding))
        return(NULL);

    c_retval = htmlParseFile(filename, encoding);
    py_retval = libxml_xmlDocPtrWrap((xmlDocPtr) c_retval);
    return(py_retval);
}

#endif /* LIBXML_HTML_ENABLED */
PyObject *
libxml_xmlInitParser(ATTRIBUTE_UNUSED PyObject *self,ATTRIBUTE_UNUSED  PyObject *args) {

    xmlInitParser();
    Py_INCREF(Py_None);
    return(Py_None);
}

PyObject *
libxml_xmlSaveFileTo(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    int c_retval;
    xmlOutputBufferPtr buf;
    PyObject *pyobj_buf;
    xmlDocPtr cur;
    PyObject *pyobj_cur;
    char * encoding;

    if (!PyArg_ParseTuple(args, (char *)"OOz:xmlSaveFileTo", &pyobj_buf, &pyobj_cur, &encoding))
        return(NULL);
    buf = (xmlOutputBufferPtr) PyoutputBuffer_Get(pyobj_buf);
    cur = (xmlDocPtr) PyxmlNode_Get(pyobj_cur);

    c_retval = xmlSaveFileTo(buf, cur, encoding);
    py_retval = libxml_intWrap((int) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlTextReaderReadState(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    int c_retval;
    xmlTextReaderPtr reader;
    PyObject *pyobj_reader;

    if (!PyArg_ParseTuple(args, (char *)"O:xmlTextReaderReadState", &pyobj_reader))
        return(NULL);
    reader = (xmlTextReaderPtr) PyxmlTextReader_Get(pyobj_reader);

    c_retval = xmlTextReaderReadState(reader);
    py_retval = libxml_intWrap((int) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlCreateFileParserCtxt(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    xmlParserCtxtPtr c_retval;
    char * filename;

    if (!PyArg_ParseTuple(args, (char *)"z:xmlCreateFileParserCtxt", &filename))
        return(NULL);

    c_retval = xmlCreateFileParserCtxt(filename);
    py_retval = libxml_xmlParserCtxtPtrWrap((xmlParserCtxtPtr) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlNodeDumpOutput(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    xmlOutputBufferPtr buf;
    PyObject *pyobj_buf;
    xmlDocPtr doc;
    PyObject *pyobj_doc;
    xmlNodePtr cur;
    PyObject *pyobj_cur;
    int level;
    int format;
    char * encoding;

    if (!PyArg_ParseTuple(args, (char *)"OOOiiz:xmlNodeDumpOutput", &pyobj_buf, &pyobj_doc, &pyobj_cur, &level, &format, &encoding))
        return(NULL);
    buf = (xmlOutputBufferPtr) PyoutputBuffer_Get(pyobj_buf);
    doc = (xmlDocPtr) PyxmlNode_Get(pyobj_doc);
    cur = (xmlNodePtr) PyxmlNode_Get(pyobj_cur);

    xmlNodeDumpOutput(buf, doc, cur, level, format, encoding);
    Py_INCREF(Py_None);
    return(Py_None);
}

PyObject *
libxml_xmlCopyNamespace(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    xmlNsPtr c_retval;
    xmlNsPtr cur;
    PyObject *pyobj_cur;

    if (!PyArg_ParseTuple(args, (char *)"O:xmlCopyNamespace", &pyobj_cur))
        return(NULL);
    cur = (xmlNsPtr) PyxmlNode_Get(pyobj_cur);

    c_retval = xmlCopyNamespace(cur);
    py_retval = libxml_xmlNsPtrWrap((xmlNsPtr) c_retval);
    return(py_retval);
}

#ifdef LIBXML_HTML_ENABLED
PyObject *
libxml_htmlIsScriptAttribute(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    int c_retval;
    xmlChar * name;

    if (!PyArg_ParseTuple(args, (char *)"z:htmlIsScriptAttribute", &name))
        return(NULL);

    c_retval = htmlIsScriptAttribute(name);
    py_retval = libxml_intWrap((int) c_retval);
    return(py_retval);
}

#endif /* LIBXML_HTML_ENABLED */
#ifdef LIBXML_DEBUG_ENABLED
PyObject *
libxml_xmlDebugDumpNode(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    FILE * output;
    PyObject *pyobj_output;
    xmlNodePtr node;
    PyObject *pyobj_node;
    int depth;

    if (!PyArg_ParseTuple(args, (char *)"OOi:xmlDebugDumpNode", &pyobj_output, &pyobj_node, &depth))
        return(NULL);
    output = (FILE *) PyFile_Get(pyobj_output);
    node = (xmlNodePtr) PyxmlNode_Get(pyobj_node);

    xmlDebugDumpNode(output, node, depth);
    Py_INCREF(Py_None);
    return(Py_None);
}

#endif /* LIBXML_DEBUG_ENABLED */
PyObject *
libxml_xmlParseTextDecl(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    xmlParserCtxtPtr ctxt;
    PyObject *pyobj_ctxt;

    if (!PyArg_ParseTuple(args, (char *)"O:xmlParseTextDecl", &pyobj_ctxt))
        return(NULL);
    ctxt = (xmlParserCtxtPtr) PyparserCtxt_Get(pyobj_ctxt);

    xmlParseTextDecl(ctxt);
    Py_INCREF(Py_None);
    return(Py_None);
}

#ifdef LIBXML_XPATH_ENABLED
PyObject *
libxml_xmlXPathNextPreceding(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    xmlNodePtr c_retval;
    xmlXPathParserContextPtr ctxt;
    PyObject *pyobj_ctxt;
    xmlNodePtr cur;
    PyObject *pyobj_cur;

    if (!PyArg_ParseTuple(args, (char *)"OO:xmlXPathNextPreceding", &pyobj_ctxt, &pyobj_cur))
        return(NULL);
    ctxt = (xmlXPathParserContextPtr) PyxmlXPathParserContext_Get(pyobj_ctxt);
    cur = (xmlNodePtr) PyxmlNode_Get(pyobj_cur);

    c_retval = xmlXPathNextPreceding(ctxt, cur);
    py_retval = libxml_xmlNodePtrWrap((xmlNodePtr) c_retval);
    return(py_retval);
}

#endif /* LIBXML_XPATH_ENABLED */
PyObject *
libxml_xmlURISetScheme(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    xmlURIPtr URI;
    PyObject *pyobj_URI;
    char * scheme;

    if (!PyArg_ParseTuple(args, (char *)"Oz:xmlURISetScheme", &pyobj_URI, &scheme))
        return(NULL);
    URI = (xmlURIPtr) PyURI_Get(pyobj_URI);

    if (URI->scheme != NULL) xmlFree(URI->scheme);
    URI->scheme = xmlStrdup((const xmlChar *)scheme);
    Py_INCREF(Py_None);
    return(Py_None);
}

#ifdef LIBXML_XPATH_ENABLED
PyObject *
libxml_xmlXPathRegisteredFuncsCleanup(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    xmlXPathContextPtr ctxt;
    PyObject *pyobj_ctxt;

    if (!PyArg_ParseTuple(args, (char *)"O:xmlXPathRegisteredFuncsCleanup", &pyobj_ctxt))
        return(NULL);
    ctxt = (xmlXPathContextPtr) PyxmlXPathContext_Get(pyobj_ctxt);

    xmlXPathRegisteredFuncsCleanup(ctxt);
    Py_INCREF(Py_None);
    return(Py_None);
}

#endif /* LIBXML_XPATH_ENABLED */
PyObject *
libxml_xmlUCSIsBlock(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    int c_retval;
    int code;
    char * block;

    if (!PyArg_ParseTuple(args, (char *)"iz:xmlUCSIsBlock", &code, &block))
        return(NULL);

    c_retval = xmlUCSIsBlock(code, block);
    py_retval = libxml_intWrap((int) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlTextReaderMoveToNextAttribute(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    int c_retval;
    xmlTextReaderPtr reader;
    PyObject *pyobj_reader;

    if (!PyArg_ParseTuple(args, (char *)"O:xmlTextReaderMoveToNextAttribute", &pyobj_reader))
        return(NULL);
    reader = (xmlTextReaderPtr) PyxmlTextReader_Get(pyobj_reader);

    c_retval = xmlTextReaderMoveToNextAttribute(reader);
    py_retval = libxml_intWrap((int) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlKeepBlanksDefault(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    int c_retval;
    int val;

    if (!PyArg_ParseTuple(args, (char *)"i:xmlKeepBlanksDefault", &val))
        return(NULL);

    c_retval = xmlKeepBlanksDefault(val);
    py_retval = libxml_intWrap((int) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlCheckFilename(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    int c_retval;
    char * path;

    if (!PyArg_ParseTuple(args, (char *)"z:xmlCheckFilename", &path))
        return(NULL);

    c_retval = xmlCheckFilename(path);
    py_retval = libxml_intWrap((int) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlParseSDDecl(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    int c_retval;
    xmlParserCtxtPtr ctxt;
    PyObject *pyobj_ctxt;

    if (!PyArg_ParseTuple(args, (char *)"O:xmlParseSDDecl", &pyobj_ctxt))
        return(NULL);
    ctxt = (xmlParserCtxtPtr) PyparserCtxt_Get(pyobj_ctxt);

    c_retval = xmlParseSDDecl(ctxt);
    py_retval = libxml_intWrap((int) c_retval);
    return(py_retval);
}

#ifdef LIBXML_XPATH_ENABLED
PyObject *
libxml_xmlXPathFloorFunction(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    xmlXPathParserContextPtr ctxt;
    PyObject *pyobj_ctxt;
    int nargs;

    if (!PyArg_ParseTuple(args, (char *)"Oi:xmlXPathFloorFunction", &pyobj_ctxt, &nargs))
        return(NULL);
    ctxt = (xmlXPathParserContextPtr) PyxmlXPathParserContext_Get(pyobj_ctxt);

    xmlXPathFloorFunction(ctxt, nargs);
    Py_INCREF(Py_None);
    return(Py_None);
}

#endif /* LIBXML_XPATH_ENABLED */
PyObject *
libxml_xmlURISetFragment(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    xmlURIPtr URI;
    PyObject *pyobj_URI;
    char * fragment;

    if (!PyArg_ParseTuple(args, (char *)"Oz:xmlURISetFragment", &pyobj_URI, &fragment))
        return(NULL);
    URI = (xmlURIPtr) PyURI_Get(pyobj_URI);

    if (URI->fragment != NULL) xmlFree(URI->fragment);
    URI->fragment = xmlStrdup((const xmlChar *)fragment);
    Py_INCREF(Py_None);
    return(Py_None);
}

PyObject *
libxml_xmlUCSIsCatNo(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    int c_retval;
    int code;

    if (!PyArg_ParseTuple(args, (char *)"i:xmlUCSIsCatNo", &code))
        return(NULL);

    c_retval = xmlUCSIsCatNo(code);
    py_retval = libxml_intWrap((int) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlHandleEntity(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    xmlParserCtxtPtr ctxt;
    PyObject *pyobj_ctxt;
    xmlEntityPtr entity;
    PyObject *pyobj_entity;

    if (!PyArg_ParseTuple(args, (char *)"OO:xmlHandleEntity", &pyobj_ctxt, &pyobj_entity))
        return(NULL);
    ctxt = (xmlParserCtxtPtr) PyparserCtxt_Get(pyobj_ctxt);
    entity = (xmlEntityPtr) PyxmlNode_Get(pyobj_entity);

    xmlHandleEntity(ctxt, entity);
    Py_INCREF(Py_None);
    return(Py_None);
}

PyObject *
libxml_xmlSkipBlankChars(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    int c_retval;
    xmlParserCtxtPtr ctxt;
    PyObject *pyobj_ctxt;

    if (!PyArg_ParseTuple(args, (char *)"O:xmlSkipBlankChars", &pyobj_ctxt))
        return(NULL);
    ctxt = (xmlParserCtxtPtr) PyparserCtxt_Get(pyobj_ctxt);

    c_retval = xmlSkipBlankChars(ctxt);
    py_retval = libxml_intWrap((int) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlValidateNmtokenValue(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    int c_retval;
    xmlChar * value;

    if (!PyArg_ParseTuple(args, (char *)"z:xmlValidateNmtokenValue", &value))
        return(NULL);

    c_retval = xmlValidateNmtokenValue(value);
    py_retval = libxml_intWrap((int) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlAddChildList(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    xmlNodePtr c_retval;
    xmlNodePtr parent;
    PyObject *pyobj_parent;
    xmlNodePtr cur;
    PyObject *pyobj_cur;

    if (!PyArg_ParseTuple(args, (char *)"OO:xmlAddChildList", &pyobj_parent, &pyobj_cur))
        return(NULL);
    parent = (xmlNodePtr) PyxmlNode_Get(pyobj_parent);
    cur = (xmlNodePtr) PyxmlNode_Get(pyobj_cur);

    c_retval = xmlAddChildList(parent, cur);
    py_retval = libxml_xmlNodePtrWrap((xmlNodePtr) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlGetNodePath(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    xmlChar * c_retval;
    xmlNodePtr node;
    PyObject *pyobj_node;

    if (!PyArg_ParseTuple(args, (char *)"O:xmlGetNodePath", &pyobj_node))
        return(NULL);
    node = (xmlNodePtr) PyxmlNode_Get(pyobj_node);

    c_retval = xmlGetNodePath(node);
    py_retval = libxml_xmlCharPtrWrap((xmlChar *) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlURIGetOpaque(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    const char * c_retval;
    xmlURIPtr URI;
    PyObject *pyobj_URI;

    if (!PyArg_ParseTuple(args, (char *)"O:xmlURIGetOpaque", &pyobj_URI))
        return(NULL);
    URI = (xmlURIPtr) PyURI_Get(pyobj_URI);

    c_retval = URI->opaque;
    py_retval = libxml_charPtrConstWrap((const char *) c_retval);
    return(py_retval);
}

#ifdef LIBXML_HTML_ENABLED
PyObject *
libxml_htmlDocContentDumpOutput(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    xmlOutputBufferPtr buf;
    PyObject *pyobj_buf;
    xmlDocPtr cur;
    PyObject *pyobj_cur;
    char * encoding;

    if (!PyArg_ParseTuple(args, (char *)"OOz:htmlDocContentDumpOutput", &pyobj_buf, &pyobj_cur, &encoding))
        return(NULL);
    buf = (xmlOutputBufferPtr) PyoutputBuffer_Get(pyobj_buf);
    cur = (xmlDocPtr) PyxmlNode_Get(pyobj_cur);

    htmlDocContentDumpOutput(buf, cur, encoding);
    Py_INCREF(Py_None);
    return(Py_None);
}

#endif /* LIBXML_HTML_ENABLED */
#ifdef LIBXML_HTML_ENABLED
PyObject *
libxml_htmlParseChunk(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    int c_retval;
    htmlParserCtxtPtr ctxt;
    PyObject *pyobj_ctxt;
    char * chunk;
    int size;
    int terminate;

    if (!PyArg_ParseTuple(args, (char *)"Ozii:htmlParseChunk", &pyobj_ctxt, &chunk, &size, &terminate))
        return(NULL);
    ctxt = (htmlParserCtxtPtr) PyparserCtxt_Get(pyobj_ctxt);

    c_retval = htmlParseChunk(ctxt, chunk, size, terminate);
    py_retval = libxml_intWrap((int) c_retval);
    return(py_retval);
}

#endif /* LIBXML_HTML_ENABLED */
PyObject *
libxml_xmlParseDTD(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    xmlDtdPtr c_retval;
    xmlChar * ExternalID;
    xmlChar * SystemID;

    if (!PyArg_ParseTuple(args, (char *)"zz:xmlParseDTD", &ExternalID, &SystemID))
        return(NULL);

    c_retval = xmlParseDTD(ExternalID, SystemID);
    py_retval = libxml_xmlNodePtrWrap((xmlNodePtr) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlNewGlobalNs(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    xmlNsPtr c_retval;
    xmlDocPtr doc;
    PyObject *pyobj_doc;
    xmlChar * href;
    xmlChar * prefix;

    if (!PyArg_ParseTuple(args, (char *)"Ozz:xmlNewGlobalNs", &pyobj_doc, &href, &prefix))
        return(NULL);
    doc = (xmlDocPtr) PyxmlNode_Get(pyobj_doc);

    c_retval = xmlNewGlobalNs(doc, href, prefix);
    py_retval = libxml_xmlNsPtrWrap((xmlNsPtr) c_retval);
    return(py_retval);
}

#ifdef LIBXML_XPATH_ENABLED
PyObject *
libxml_xmlXPathIdFunction(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    xmlXPathParserContextPtr ctxt;
    PyObject *pyobj_ctxt;
    int nargs;

    if (!PyArg_ParseTuple(args, (char *)"Oi:xmlXPathIdFunction", &pyobj_ctxt, &nargs))
        return(NULL);
    ctxt = (xmlXPathParserContextPtr) PyxmlXPathParserContext_Get(pyobj_ctxt);

    xmlXPathIdFunction(ctxt, nargs);
    Py_INCREF(Py_None);
    return(Py_None);
}

#endif /* LIBXML_XPATH_ENABLED */
PyObject *
libxml_xmlTextReaderRead(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    int c_retval;
    xmlTextReaderPtr reader;
    PyObject *pyobj_reader;

    if (!PyArg_ParseTuple(args, (char *)"O:xmlTextReaderRead", &pyobj_reader))
        return(NULL);
    reader = (xmlTextReaderPtr) PyxmlTextReader_Get(pyobj_reader);

    c_retval = xmlTextReaderRead(reader);
    py_retval = libxml_intWrap((int) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlIsLetter(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    int c_retval;
    int c;

    if (!PyArg_ParseTuple(args, (char *)"i:xmlIsLetter", &c))
        return(NULL);

    c_retval = xmlIsLetter(c);
    py_retval = libxml_intWrap((int) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlTextMerge(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    xmlNodePtr c_retval;
    xmlNodePtr first;
    PyObject *pyobj_first;
    xmlNodePtr second;
    PyObject *pyobj_second;

    if (!PyArg_ParseTuple(args, (char *)"OO:xmlTextMerge", &pyobj_first, &pyobj_second))
        return(NULL);
    first = (xmlNodePtr) PyxmlNode_Get(pyobj_first);
    second = (xmlNodePtr) PyxmlNode_Get(pyobj_second);

    c_retval = xmlTextMerge(first, second);
    py_retval = libxml_xmlNodePtrWrap((xmlNodePtr) c_retval);
    return(py_retval);
}

#ifdef LIBXML_XPATH_ENABLED
PyObject *
libxml_xmlXPathStringLengthFunction(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    xmlXPathParserContextPtr ctxt;
    PyObject *pyobj_ctxt;
    int nargs;

    if (!PyArg_ParseTuple(args, (char *)"Oi:xmlXPathStringLengthFunction", &pyobj_ctxt, &nargs))
        return(NULL);
    ctxt = (xmlXPathParserContextPtr) PyxmlXPathParserContext_Get(pyobj_ctxt);

    xmlXPathStringLengthFunction(ctxt, nargs);
    Py_INCREF(Py_None);
    return(Py_None);
}

#endif /* LIBXML_XPATH_ENABLED */
PyObject *
libxml_xmlIsChar(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    int c_retval;
    int c;

    if (!PyArg_ParseTuple(args, (char *)"i:xmlIsChar", &c))
        return(NULL);

    c_retval = xmlIsChar(c);
    py_retval = libxml_intWrap((int) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlUCSIsCatC(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    int c_retval;
    int code;

    if (!PyArg_ParseTuple(args, (char *)"i:xmlUCSIsCatC", &code))
        return(NULL);

    c_retval = xmlUCSIsCatC(code);
    py_retval = libxml_intWrap((int) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlIsIdeographic(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    int c_retval;
    int c;

    if (!PyArg_ParseTuple(args, (char *)"i:xmlIsIdeographic", &c))
        return(NULL);

    c_retval = xmlIsIdeographic(c);
    py_retval = libxml_intWrap((int) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlTextReaderSetParserProp(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    int c_retval;
    xmlTextReaderPtr reader;
    PyObject *pyobj_reader;
    int prop;
    int value;

    if (!PyArg_ParseTuple(args, (char *)"Oii:xmlTextReaderSetParserProp", &pyobj_reader, &prop, &value))
        return(NULL);
    reader = (xmlTextReaderPtr) PyxmlTextReader_Get(pyobj_reader);

    c_retval = xmlTextReaderSetParserProp(reader, prop, value);
    py_retval = libxml_intWrap((int) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlPedanticParserDefault(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    int c_retval;
    int val;

    if (!PyArg_ParseTuple(args, (char *)"i:xmlPedanticParserDefault", &val))
        return(NULL);

    c_retval = xmlPedanticParserDefault(val);
    py_retval = libxml_intWrap((int) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlUCSIsOriya(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    int c_retval;
    int code;

    if (!PyArg_ParseTuple(args, (char *)"i:xmlUCSIsOriya", &code))
        return(NULL);

    c_retval = xmlUCSIsOriya(code);
    py_retval = libxml_intWrap((int) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlUCSIsLetterlikeSymbols(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    int c_retval;
    int code;

    if (!PyArg_ParseTuple(args, (char *)"i:xmlUCSIsLetterlikeSymbols", &code))
        return(NULL);

    c_retval = xmlUCSIsLetterlikeSymbols(code);
    py_retval = libxml_intWrap((int) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlParserSetLoadSubset(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    xmlParserCtxtPtr ctxt;
    PyObject *pyobj_ctxt;
    int loadsubset;

    if (!PyArg_ParseTuple(args, (char *)"Oi:xmlParserSetLoadSubset", &pyobj_ctxt, &loadsubset))
        return(NULL);
    ctxt = (xmlParserCtxtPtr) PyparserCtxt_Get(pyobj_ctxt);

    ctxt->loadsubset = loadsubset;
    Py_INCREF(Py_None);
    return(Py_None);
}

PyObject *
libxml_xmlParseDoc(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    xmlDocPtr c_retval;
    xmlChar * cur;

    if (!PyArg_ParseTuple(args, (char *)"z:xmlParseDoc", &cur))
        return(NULL);

    c_retval = xmlParseDoc(cur);
    py_retval = libxml_xmlDocPtrWrap((xmlDocPtr) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlTextReaderBaseUri(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    xmlChar * c_retval;
    xmlTextReaderPtr reader;
    PyObject *pyobj_reader;

    if (!PyArg_ParseTuple(args, (char *)"O:xmlTextReaderBaseUri", &pyobj_reader))
        return(NULL);
    reader = (xmlTextReaderPtr) PyxmlTextReader_Get(pyobj_reader);

    c_retval = xmlTextReaderBaseUri(reader);
    py_retval = libxml_xmlCharPtrWrap((xmlChar *) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlParseURI(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    xmlURIPtr c_retval;
    char * str;

    if (!PyArg_ParseTuple(args, (char *)"z:xmlParseURI", &str))
        return(NULL);

    c_retval = xmlParseURI(str);
    py_retval = libxml_xmlURIPtrWrap((xmlURIPtr) c_retval);
    return(py_retval);
}

#ifdef LIBXML_XPATH_ENABLED
PyObject *
libxml_xmlXPathParseNCName(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    xmlChar * c_retval;
    xmlXPathParserContextPtr ctxt;
    PyObject *pyobj_ctxt;

    if (!PyArg_ParseTuple(args, (char *)"O:xmlXPathParseNCName", &pyobj_ctxt))
        return(NULL);
    ctxt = (xmlXPathParserContextPtr) PyxmlXPathParserContext_Get(pyobj_ctxt);

    c_retval = xmlXPathParseNCName(ctxt);
    py_retval = libxml_xmlCharPtrWrap((xmlChar *) c_retval);
    return(py_retval);
}

#endif /* LIBXML_XPATH_ENABLED */
PyObject *
libxml_xmlCopyProp(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    xmlAttrPtr c_retval;
    xmlNodePtr target;
    PyObject *pyobj_target;
    xmlAttrPtr cur;
    PyObject *pyobj_cur;

    if (!PyArg_ParseTuple(args, (char *)"OO:xmlCopyProp", &pyobj_target, &pyobj_cur))
        return(NULL);
    target = (xmlNodePtr) PyxmlNode_Get(pyobj_target);
    cur = (xmlAttrPtr) PyxmlNode_Get(pyobj_cur);

    c_retval = xmlCopyProp(target, cur);
    py_retval = libxml_xmlNodePtrWrap((xmlNodePtr) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlSaveUri(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    xmlChar * c_retval;
    xmlURIPtr uri;
    PyObject *pyobj_uri;

    if (!PyArg_ParseTuple(args, (char *)"O:xmlSaveUri", &pyobj_uri))
        return(NULL);
    uri = (xmlURIPtr) PyURI_Get(pyobj_uri);

    c_retval = xmlSaveUri(uri);
    py_retval = libxml_xmlCharPtrWrap((xmlChar *) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlReplaceNode(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    xmlNodePtr c_retval;
    xmlNodePtr old;
    PyObject *pyobj_old;
    xmlNodePtr cur;
    PyObject *pyobj_cur;

    if (!PyArg_ParseTuple(args, (char *)"OO:xmlReplaceNode", &pyobj_old, &pyobj_cur))
        return(NULL);
    old = (xmlNodePtr) PyxmlNode_Get(pyobj_old);
    cur = (xmlNodePtr) PyxmlNode_Get(pyobj_cur);

    c_retval = xmlReplaceNode(old, cur);
    py_retval = libxml_xmlNodePtrWrap((xmlNodePtr) c_retval);
    return(py_retval);
}

#ifdef LIBXML_XPATH_ENABLED
PyObject *
libxml_xmlXPathNewBoolean(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    xmlXPathObjectPtr c_retval;
    int val;

    if (!PyArg_ParseTuple(args, (char *)"i:xmlXPathNewBoolean", &val))
        return(NULL);

    c_retval = xmlXPathNewBoolean(val);
    py_retval = libxml_xmlXPathObjectPtrWrap((xmlXPathObjectPtr) c_retval);
    return(py_retval);
}

#endif /* LIBXML_XPATH_ENABLED */
#ifdef LIBXML_XPATH_ENABLED
PyObject *
libxml_xmlXPathEvalExpr(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    xmlXPathParserContextPtr ctxt;
    PyObject *pyobj_ctxt;

    if (!PyArg_ParseTuple(args, (char *)"O:xmlXPathEvalExpr", &pyobj_ctxt))
        return(NULL);
    ctxt = (xmlXPathParserContextPtr) PyxmlXPathParserContext_Get(pyobj_ctxt);

    xmlXPathEvalExpr(ctxt);
    Py_INCREF(Py_None);
    return(Py_None);
}

#endif /* LIBXML_XPATH_ENABLED */
PyObject *
libxml_xmlLineNumbersDefault(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    int c_retval;
    int val;

    if (!PyArg_ParseTuple(args, (char *)"i:xmlLineNumbersDefault", &val))
        return(NULL);

    c_retval = xmlLineNumbersDefault(val);
    py_retval = libxml_intWrap((int) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlSetDocCompressMode(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    xmlDocPtr doc;
    PyObject *pyobj_doc;
    int mode;

    if (!PyArg_ParseTuple(args, (char *)"Oi:xmlSetDocCompressMode", &pyobj_doc, &mode))
        return(NULL);
    doc = (xmlDocPtr) PyxmlNode_Get(pyobj_doc);

    xmlSetDocCompressMode(doc, mode);
    Py_INCREF(Py_None);
    return(Py_None);
}

PyObject *
libxml_xmlCatalogSetDebug(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    int c_retval;
    int level;

    if (!PyArg_ParseTuple(args, (char *)"i:xmlCatalogSetDebug", &level))
        return(NULL);

    c_retval = xmlCatalogSetDebug(level);
    py_retval = libxml_intWrap((int) c_retval);
    return(py_retval);
}

#ifdef LIBXML_XPTR_ENABLED
PyObject *
libxml_xmlXPtrNewRange(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    xmlXPathObjectPtr c_retval;
    xmlNodePtr start;
    PyObject *pyobj_start;
    int startindex;
    xmlNodePtr end;
    PyObject *pyobj_end;
    int endindex;

    if (!PyArg_ParseTuple(args, (char *)"OiOi:xmlXPtrNewRange", &pyobj_start, &startindex, &pyobj_end, &endindex))
        return(NULL);
    start = (xmlNodePtr) PyxmlNode_Get(pyobj_start);
    end = (xmlNodePtr) PyxmlNode_Get(pyobj_end);

    c_retval = xmlXPtrNewRange(start, startindex, end, endindex);
    py_retval = libxml_xmlXPathObjectPtrWrap((xmlXPathObjectPtr) c_retval);
    return(py_retval);
}

#endif /* LIBXML_XPTR_ENABLED */
PyObject *
libxml_xmlEncodeEntitiesReentrant(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    xmlChar * c_retval;
    xmlDocPtr doc;
    PyObject *pyobj_doc;
    xmlChar * input;

    if (!PyArg_ParseTuple(args, (char *)"Oz:xmlEncodeEntitiesReentrant", &pyobj_doc, &input))
        return(NULL);
    doc = (xmlDocPtr) PyxmlNode_Get(pyobj_doc);

    c_retval = xmlEncodeEntitiesReentrant(doc, input);
    py_retval = libxml_xmlCharPtrWrap((xmlChar *) c_retval);
    return(py_retval);
}

#ifdef LIBXML_XPATH_ENABLED
PyObject *
libxml_xmlXPathMultValues(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    xmlXPathParserContextPtr ctxt;
    PyObject *pyobj_ctxt;

    if (!PyArg_ParseTuple(args, (char *)"O:xmlXPathMultValues", &pyobj_ctxt))
        return(NULL);
    ctxt = (xmlXPathParserContextPtr) PyxmlXPathParserContext_Get(pyobj_ctxt);

    xmlXPathMultValues(ctxt);
    Py_INCREF(Py_None);
    return(Py_None);
}

#endif /* LIBXML_XPATH_ENABLED */
PyObject *
libxml_xmlUCSIsCJKUnifiedIdeographs(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    int c_retval;
    int code;

    if (!PyArg_ParseTuple(args, (char *)"i:xmlUCSIsCJKUnifiedIdeographs", &code))
        return(NULL);

    c_retval = xmlUCSIsCJKUnifiedIdeographs(code);
    py_retval = libxml_intWrap((int) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlRemoveProp(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    int c_retval;
    xmlAttrPtr cur;
    PyObject *pyobj_cur;

    if (!PyArg_ParseTuple(args, (char *)"O:xmlRemoveProp", &pyobj_cur))
        return(NULL);
    cur = (xmlAttrPtr) PyxmlNode_Get(pyobj_cur);

    c_retval = xmlRemoveProp(cur);
    py_retval = libxml_intWrap((int) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlACatalogDump(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    xmlCatalogPtr catal;
    PyObject *pyobj_catal;
    FILE * out;
    PyObject *pyobj_out;

    if (!PyArg_ParseTuple(args, (char *)"OO:xmlACatalogDump", &pyobj_catal, &pyobj_out))
        return(NULL);
    catal = (xmlCatalogPtr) Pycatalog_Get(pyobj_catal);
    out = (FILE *) PyFile_Get(pyobj_out);

    xmlACatalogDump(catal, out);
    Py_INCREF(Py_None);
    return(Py_None);
}

PyObject *
libxml_xmlURISetAuthority(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    xmlURIPtr URI;
    PyObject *pyobj_URI;
    char * authority;

    if (!PyArg_ParseTuple(args, (char *)"Oz:xmlURISetAuthority", &pyobj_URI, &authority))
        return(NULL);
    URI = (xmlURIPtr) PyURI_Get(pyobj_URI);

    if (URI->authority != NULL) xmlFree(URI->authority);
    URI->authority = xmlStrdup((const xmlChar *)authority);
    Py_INCREF(Py_None);
    return(Py_None);
}

PyObject *
libxml_xmlURIGetPort(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    int c_retval;
    xmlURIPtr URI;
    PyObject *pyobj_URI;

    if (!PyArg_ParseTuple(args, (char *)"O:xmlURIGetPort", &pyobj_URI))
        return(NULL);
    URI = (xmlURIPtr) PyURI_Get(pyobj_URI);

    c_retval = URI->port;
    py_retval = libxml_intWrap((int) c_retval);
    return(py_retval);
}

#ifdef LIBXML_XPATH_ENABLED
PyObject *
libxml_xmlXPathNewParserContext(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    xmlXPathParserContextPtr c_retval;
    xmlChar * str;
    xmlXPathContextPtr ctxt;
    PyObject *pyobj_ctxt;

    if (!PyArg_ParseTuple(args, (char *)"zO:xmlXPathNewParserContext", &str, &pyobj_ctxt))
        return(NULL);
    ctxt = (xmlXPathContextPtr) PyxmlXPathContext_Get(pyobj_ctxt);

    c_retval = xmlXPathNewParserContext(str, ctxt);
    py_retval = libxml_xmlXPathParserContextPtrWrap((xmlXPathParserContextPtr) c_retval);
    return(py_retval);
}

#endif /* LIBXML_XPATH_ENABLED */
PyObject *
libxml_xmlValidateNamesValue(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    int c_retval;
    xmlChar * value;

    if (!PyArg_ParseTuple(args, (char *)"z:xmlValidateNamesValue", &value))
        return(NULL);

    c_retval = xmlValidateNamesValue(value);
    py_retval = libxml_intWrap((int) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlParseFile(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    xmlDocPtr c_retval;
    char * filename;

    if (!PyArg_ParseTuple(args, (char *)"z:xmlParseFile", &filename))
        return(NULL);

    c_retval = xmlParseFile(filename);
    py_retval = libxml_xmlDocPtrWrap((xmlDocPtr) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlUCSIsNumberForms(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    int c_retval;
    int code;

    if (!PyArg_ParseTuple(args, (char *)"i:xmlUCSIsNumberForms", &code))
        return(NULL);

    c_retval = xmlUCSIsNumberForms(code);
    py_retval = libxml_intWrap((int) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlParseDocument(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    int c_retval;
    xmlParserCtxtPtr ctxt;
    PyObject *pyobj_ctxt;

    if (!PyArg_ParseTuple(args, (char *)"O:xmlParseDocument", &pyobj_ctxt))
        return(NULL);
    ctxt = (xmlParserCtxtPtr) PyparserCtxt_Get(pyobj_ctxt);

    c_retval = xmlParseDocument(ctxt);
    py_retval = libxml_intWrap((int) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlStrncmp(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    int c_retval;
    xmlChar * str1;
    xmlChar * str2;
    int len;

    if (!PyArg_ParseTuple(args, (char *)"zzi:xmlStrncmp", &str1, &str2, &len))
        return(NULL);

    c_retval = xmlStrncmp(str1, str2, len);
    py_retval = libxml_intWrap((int) c_retval);
    return(py_retval);
}

#ifdef LIBXML_HTML_ENABLED
PyObject *
libxml_htmlSaveFile(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    int c_retval;
    char * filename;
    xmlDocPtr cur;
    PyObject *pyobj_cur;

    if (!PyArg_ParseTuple(args, (char *)"zO:htmlSaveFile", &filename, &pyobj_cur))
        return(NULL);
    cur = (xmlDocPtr) PyxmlNode_Get(pyobj_cur);

    c_retval = htmlSaveFile(filename, cur);
    py_retval = libxml_intWrap((int) c_retval);
    return(py_retval);
}

#endif /* LIBXML_HTML_ENABLED */
PyObject *
libxml_xmlPrintURI(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    FILE * stream;
    PyObject *pyobj_stream;
    xmlURIPtr uri;
    PyObject *pyobj_uri;

    if (!PyArg_ParseTuple(args, (char *)"OO:xmlPrintURI", &pyobj_stream, &pyobj_uri))
        return(NULL);
    stream = (FILE *) PyFile_Get(pyobj_stream);
    uri = (xmlURIPtr) PyURI_Get(pyobj_uri);

    xmlPrintURI(stream, uri);
    Py_INCREF(Py_None);
    return(Py_None);
}

PyObject *
libxml_xmlCatalogGetPublic(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    const xmlChar * c_retval;
    xmlChar * pubID;

    if (!PyArg_ParseTuple(args, (char *)"z:xmlCatalogGetPublic", &pubID))
        return(NULL);

    c_retval = xmlCatalogGetPublic(pubID);
    py_retval = libxml_xmlCharPtrConstWrap((const xmlChar *) c_retval);
    return(py_retval);
}

#ifdef LIBXML_XPATH_ENABLED
PyObject *
libxml_xmlXPathTranslateFunction(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    xmlXPathParserContextPtr ctxt;
    PyObject *pyobj_ctxt;
    int nargs;

    if (!PyArg_ParseTuple(args, (char *)"Oi:xmlXPathTranslateFunction", &pyobj_ctxt, &nargs))
        return(NULL);
    ctxt = (xmlXPathParserContextPtr) PyxmlXPathParserContext_Get(pyobj_ctxt);

    xmlXPathTranslateFunction(ctxt, nargs);
    Py_INCREF(Py_None);
    return(Py_None);
}

#endif /* LIBXML_XPATH_ENABLED */
PyObject *
libxml_xmlParseEndTag(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    xmlParserCtxtPtr ctxt;
    PyObject *pyobj_ctxt;

    if (!PyArg_ParseTuple(args, (char *)"O:xmlParseEndTag", &pyobj_ctxt))
        return(NULL);
    ctxt = (xmlParserCtxtPtr) PyparserCtxt_Get(pyobj_ctxt);

    xmlParseEndTag(ctxt);
    Py_INCREF(Py_None);
    return(Py_None);
}

PyObject *
libxml_xmlDocDump(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    int c_retval;
    FILE * f;
    PyObject *pyobj_f;
    xmlDocPtr cur;
    PyObject *pyobj_cur;

    if (!PyArg_ParseTuple(args, (char *)"OO:xmlDocDump", &pyobj_f, &pyobj_cur))
        return(NULL);
    f = (FILE *) PyFile_Get(pyobj_f);
    cur = (xmlDocPtr) PyxmlNode_Get(pyobj_cur);

    c_retval = xmlDocDump(f, cur);
    py_retval = libxml_intWrap((int) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlURIGetFragment(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    const char * c_retval;
    xmlURIPtr URI;
    PyObject *pyobj_URI;

    if (!PyArg_ParseTuple(args, (char *)"O:xmlURIGetFragment", &pyobj_URI))
        return(NULL);
    URI = (xmlURIPtr) PyURI_Get(pyobj_URI);

    c_retval = URI->fragment;
    py_retval = libxml_charPtrConstWrap((const char *) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlUCSIsSinhala(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    int c_retval;
    int code;

    if (!PyArg_ParseTuple(args, (char *)"i:xmlUCSIsSinhala", &code))
        return(NULL);

    c_retval = xmlUCSIsSinhala(code);
    py_retval = libxml_intWrap((int) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlParserInputBufferPush(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    int c_retval;
    xmlParserInputBufferPtr in;
    PyObject *pyobj_in;
    int len;
    char * buf;

    if (!PyArg_ParseTuple(args, (char *)"Oiz:xmlParserInputBufferPush", &pyobj_in, &len, &buf))
        return(NULL);
    in = (xmlParserInputBufferPtr) PyinputBuffer_Get(pyobj_in);

    c_retval = xmlParserInputBufferPush(in, len, buf);
    py_retval = libxml_intWrap((int) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlSaveFormatFile(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    int c_retval;
    char * filename;
    xmlDocPtr cur;
    PyObject *pyobj_cur;
    int format;

    if (!PyArg_ParseTuple(args, (char *)"zOi:xmlSaveFormatFile", &filename, &pyobj_cur, &format))
        return(NULL);
    cur = (xmlDocPtr) PyxmlNode_Get(pyobj_cur);

    c_retval = xmlSaveFormatFile(filename, cur, format);
    py_retval = libxml_intWrap((int) c_retval);
    return(py_retval);
}

#ifdef LIBXML_XPATH_ENABLED
PyObject *
libxml_xmlXPathEval(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    xmlXPathObjectPtr c_retval;
    xmlChar * str;
    xmlXPathContextPtr ctx;
    PyObject *pyobj_ctx;

    if (!PyArg_ParseTuple(args, (char *)"zO:xmlXPathEval", &str, &pyobj_ctx))
        return(NULL);
    ctx = (xmlXPathContextPtr) PyxmlXPathContext_Get(pyobj_ctx);

    c_retval = xmlXPathEval(str, ctx);
    py_retval = libxml_xmlXPathObjectPtrWrap((xmlXPathObjectPtr) c_retval);
    return(py_retval);
}

#endif /* LIBXML_XPATH_ENABLED */
#ifdef LIBXML_XPATH_ENABLED
PyObject *
libxml_xmlXPathNextSelf(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    xmlNodePtr c_retval;
    xmlXPathParserContextPtr ctxt;
    PyObject *pyobj_ctxt;
    xmlNodePtr cur;
    PyObject *pyobj_cur;

    if (!PyArg_ParseTuple(args, (char *)"OO:xmlXPathNextSelf", &pyobj_ctxt, &pyobj_cur))
        return(NULL);
    ctxt = (xmlXPathParserContextPtr) PyxmlXPathParserContext_Get(pyobj_ctxt);
    cur = (xmlNodePtr) PyxmlNode_Get(pyobj_cur);

    c_retval = xmlXPathNextSelf(ctxt, cur);
    py_retval = libxml_xmlNodePtrWrap((xmlNodePtr) c_retval);
    return(py_retval);
}

#endif /* LIBXML_XPATH_ENABLED */
PyObject *
libxml_xmlTextReaderHasAttributes(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    int c_retval;
    xmlTextReaderPtr reader;
    PyObject *pyobj_reader;

    if (!PyArg_ParseTuple(args, (char *)"O:xmlTextReaderHasAttributes", &pyobj_reader))
        return(NULL);
    reader = (xmlTextReaderPtr) PyxmlTextReader_Get(pyobj_reader);

    c_retval = xmlTextReaderHasAttributes(reader);
    py_retval = libxml_intWrap((int) c_retval);
    return(py_retval);
}

#ifdef LIBXML_XPATH_ENABLED
PyObject *
libxml_xmlXPathPopString(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    xmlChar * c_retval;
    xmlXPathParserContextPtr ctxt;
    PyObject *pyobj_ctxt;

    if (!PyArg_ParseTuple(args, (char *)"O:xmlXPathPopString", &pyobj_ctxt))
        return(NULL);
    ctxt = (xmlXPathParserContextPtr) PyxmlXPathParserContext_Get(pyobj_ctxt);

    c_retval = xmlXPathPopString(ctxt);
    py_retval = libxml_xmlCharPtrWrap((xmlChar *) c_retval);
    return(py_retval);
}

#endif /* LIBXML_XPATH_ENABLED */
PyObject *
libxml_xmlFileMatch(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    int c_retval;
    char * filename;

    if (!PyArg_ParseTuple(args, (char *)"z:xmlFileMatch", &filename))
        return(NULL);

    c_retval = xmlFileMatch(filename);
    py_retval = libxml_intWrap((int) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlUCSIsGreekExtended(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    int c_retval;
    int code;

    if (!PyArg_ParseTuple(args, (char *)"i:xmlUCSIsGreekExtended", &code))
        return(NULL);

    c_retval = xmlUCSIsGreekExtended(code);
    py_retval = libxml_intWrap((int) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlParsePITarget(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    xmlChar * c_retval;
    xmlParserCtxtPtr ctxt;
    PyObject *pyobj_ctxt;

    if (!PyArg_ParseTuple(args, (char *)"O:xmlParsePITarget", &pyobj_ctxt))
        return(NULL);
    ctxt = (xmlParserCtxtPtr) PyparserCtxt_Get(pyobj_ctxt);

    c_retval = xmlParsePITarget(ctxt);
    py_retval = libxml_xmlCharPtrWrap((xmlChar *) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlURISetOpaque(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    xmlURIPtr URI;
    PyObject *pyobj_URI;
    char * opaque;

    if (!PyArg_ParseTuple(args, (char *)"Oz:xmlURISetOpaque", &pyobj_URI, &opaque))
        return(NULL);
    URI = (xmlURIPtr) PyURI_Get(pyobj_URI);

    if (URI->opaque != NULL) xmlFree(URI->opaque);
    URI->opaque = xmlStrdup((const xmlChar *)opaque);
    Py_INCREF(Py_None);
    return(Py_None);
}

PyObject *
libxml_xmlStrEqual(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    int c_retval;
    xmlChar * str1;
    xmlChar * str2;

    if (!PyArg_ParseTuple(args, (char *)"zz:xmlStrEqual", &str1, &str2))
        return(NULL);

    c_retval = xmlStrEqual(str1, str2);
    py_retval = libxml_intWrap((int) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlParserSetLineNumbers(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    xmlParserCtxtPtr ctxt;
    PyObject *pyobj_ctxt;
    int linenumbers;

    if (!PyArg_ParseTuple(args, (char *)"Oi:xmlParserSetLineNumbers", &pyobj_ctxt, &linenumbers))
        return(NULL);
    ctxt = (xmlParserCtxtPtr) PyparserCtxt_Get(pyobj_ctxt);

    ctxt->linenumbers = linenumbers;
    Py_INCREF(Py_None);
    return(Py_None);
}

PyObject *
libxml_xmlParsePEReference(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    xmlParserCtxtPtr ctxt;
    PyObject *pyobj_ctxt;

    if (!PyArg_ParseTuple(args, (char *)"O:xmlParsePEReference", &pyobj_ctxt))
        return(NULL);
    ctxt = (xmlParserCtxtPtr) PyparserCtxt_Get(pyobj_ctxt);

    xmlParsePEReference(ctxt);
    Py_INCREF(Py_None);
    return(Py_None);
}

#ifdef LIBXML_XPATH_ENABLED
PyObject *
libxml_xmlXPathNormalizeFunction(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    xmlXPathParserContextPtr ctxt;
    PyObject *pyobj_ctxt;
    int nargs;

    if (!PyArg_ParseTuple(args, (char *)"Oi:xmlXPathNormalizeFunction", &pyobj_ctxt, &nargs))
        return(NULL);
    ctxt = (xmlXPathParserContextPtr) PyxmlXPathParserContext_Get(pyobj_ctxt);

    xmlXPathNormalizeFunction(ctxt, nargs);
    Py_INCREF(Py_None);
    return(Py_None);
}

#endif /* LIBXML_XPATH_ENABLED */
PyObject *
libxml_xmlURIGetQuery(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    const char * c_retval;
    xmlURIPtr URI;
    PyObject *pyobj_URI;

    if (!PyArg_ParseTuple(args, (char *)"O:xmlURIGetQuery", &pyobj_URI))
        return(NULL);
    URI = (xmlURIPtr) PyURI_Get(pyobj_URI);

    c_retval = URI->query;
    py_retval = libxml_charPtrConstWrap((const char *) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlDelEncodingAlias(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    int c_retval;
    char * alias;

    if (!PyArg_ParseTuple(args, (char *)"z:xmlDelEncodingAlias", &alias))
        return(NULL);

    c_retval = xmlDelEncodingAlias(alias);
    py_retval = libxml_intWrap((int) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlNewNodeEatName(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    xmlNodePtr c_retval;
    xmlNsPtr ns;
    PyObject *pyobj_ns;
    xmlChar * name;

    if (!PyArg_ParseTuple(args, (char *)"Oz:xmlNewNodeEatName", &pyobj_ns, &name))
        return(NULL);
    ns = (xmlNsPtr) PyxmlNode_Get(pyobj_ns);

    c_retval = xmlNewNodeEatName(ns, name);
    py_retval = libxml_xmlNodePtrWrap((xmlNodePtr) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlParseXMLDecl(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    xmlParserCtxtPtr ctxt;
    PyObject *pyobj_ctxt;

    if (!PyArg_ParseTuple(args, (char *)"O:xmlParseXMLDecl", &pyobj_ctxt))
        return(NULL);
    ctxt = (xmlParserCtxtPtr) PyparserCtxt_Get(pyobj_ctxt);

    xmlParseXMLDecl(ctxt);
    Py_INCREF(Py_None);
    return(Py_None);
}

PyObject *
libxml_xmlTextReaderNormalization(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    int c_retval;
    xmlTextReaderPtr reader;
    PyObject *pyobj_reader;

    if (!PyArg_ParseTuple(args, (char *)"O:xmlTextReaderNormalization", &pyobj_reader))
        return(NULL);
    reader = (xmlTextReaderPtr) PyxmlTextReader_Get(pyobj_reader);

    c_retval = xmlTextReaderNormalization(reader);
    py_retval = libxml_intWrap((int) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlIsCombining(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    int c_retval;
    int c;

    if (!PyArg_ParseTuple(args, (char *)"i:xmlIsCombining", &c))
        return(NULL);

    c_retval = xmlIsCombining(c);
    py_retval = libxml_intWrap((int) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlNewComment(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    xmlNodePtr c_retval;
    xmlChar * content;

    if (!PyArg_ParseTuple(args, (char *)"z:xmlNewComment", &content))
        return(NULL);

    c_retval = xmlNewComment(content);
    py_retval = libxml_xmlNodePtrWrap((xmlNodePtr) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlUCSIsCatM(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    int c_retval;
    int code;

    if (!PyArg_ParseTuple(args, (char *)"i:xmlUCSIsCatM", &code))
        return(NULL);

    c_retval = xmlUCSIsCatM(code);
    py_retval = libxml_intWrap((int) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlUCSIsLatinExtendedAdditional(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    int c_retval;
    int code;

    if (!PyArg_ParseTuple(args, (char *)"i:xmlUCSIsLatinExtendedAdditional", &code))
        return(NULL);

    c_retval = xmlUCSIsLatinExtendedAdditional(code);
    py_retval = libxml_intWrap((int) c_retval);
    return(py_retval);
}

#ifdef LIBXML_SCHEMAS_ENABLED
PyObject *
libxml_xmlRelaxNGNewValidCtxt(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    xmlRelaxNGValidCtxtPtr c_retval;
    xmlRelaxNGPtr schema;
    PyObject *pyobj_schema;

    if (!PyArg_ParseTuple(args, (char *)"O:xmlRelaxNGNewValidCtxt", &pyobj_schema))
        return(NULL);
    schema = (xmlRelaxNGPtr) PyrelaxNgSchema_Get(pyobj_schema);

    c_retval = xmlRelaxNGNewValidCtxt(schema);
    py_retval = libxml_xmlRelaxNGValidCtxtPtrWrap((xmlRelaxNGValidCtxtPtr) c_retval);
    return(py_retval);
}

#endif /* LIBXML_SCHEMAS_ENABLED */
PyObject *
libxml_xmlUCSIsThaana(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    int c_retval;
    int code;

    if (!PyArg_ParseTuple(args, (char *)"i:xmlUCSIsThaana", &code))
        return(NULL);

    c_retval = xmlUCSIsThaana(code);
    py_retval = libxml_intWrap((int) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlUCSIsKatakana(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    int c_retval;
    int code;

    if (!PyArg_ParseTuple(args, (char *)"i:xmlUCSIsKatakana", &code))
        return(NULL);

    c_retval = xmlUCSIsKatakana(code);
    py_retval = libxml_intWrap((int) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlUnsetProp(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    int c_retval;
    xmlNodePtr node;
    PyObject *pyobj_node;
    xmlChar * name;

    if (!PyArg_ParseTuple(args, (char *)"Oz:xmlUnsetProp", &pyobj_node, &name))
        return(NULL);
    node = (xmlNodePtr) PyxmlNode_Get(pyobj_node);

    c_retval = xmlUnsetProp(node, name);
    py_retval = libxml_intWrap((int) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlAddEncodingAlias(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    int c_retval;
    char * name;
    char * alias;

    if (!PyArg_ParseTuple(args, (char *)"zz:xmlAddEncodingAlias", &name, &alias))
        return(NULL);

    c_retval = xmlAddEncodingAlias(name, alias);
    py_retval = libxml_intWrap((int) c_retval);
    return(py_retval);
}

#ifdef LIBXML_XPATH_ENABLED
PyObject *
libxml_xmlXPathCastStringToNumber(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    double c_retval;
    xmlChar * val;

    if (!PyArg_ParseTuple(args, (char *)"z:xmlXPathCastStringToNumber", &val))
        return(NULL);

    c_retval = xmlXPathCastStringToNumber(val);
    py_retval = libxml_doubleWrap((double) c_retval);
    return(py_retval);
}

#endif /* LIBXML_XPATH_ENABLED */
PyObject *
libxml_xmlUCSIsCatSm(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    int c_retval;
    int code;

    if (!PyArg_ParseTuple(args, (char *)"i:xmlUCSIsCatSm", &code))
        return(NULL);

    c_retval = xmlUCSIsCatSm(code);
    py_retval = libxml_intWrap((int) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlCatalogResolvePublic(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    xmlChar * c_retval;
    xmlChar * pubID;

    if (!PyArg_ParseTuple(args, (char *)"z:xmlCatalogResolvePublic", &pubID))
        return(NULL);

    c_retval = xmlCatalogResolvePublic(pubID);
    py_retval = libxml_xmlCharPtrWrap((xmlChar *) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlNewCDataBlock(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    xmlNodePtr c_retval;
    xmlDocPtr doc;
    PyObject *pyobj_doc;
    xmlChar * content;
    int len;

    if (!PyArg_ParseTuple(args, (char *)"Ozi:xmlNewCDataBlock", &pyobj_doc, &content, &len))
        return(NULL);
    doc = (xmlDocPtr) PyxmlNode_Get(pyobj_doc);

    c_retval = xmlNewCDataBlock(doc, content, len);
    py_retval = libxml_xmlNodePtrWrap((xmlNodePtr) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlOutputBufferFlush(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    int c_retval;
    xmlOutputBufferPtr out;
    PyObject *pyobj_out;

    if (!PyArg_ParseTuple(args, (char *)"O:xmlOutputBufferFlush", &pyobj_out))
        return(NULL);
    out = (xmlOutputBufferPtr) PyoutputBuffer_Get(pyobj_out);

    c_retval = xmlOutputBufferFlush(out);
    py_retval = libxml_intWrap((int) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlUTF8Strlen(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    int c_retval;
    xmlChar * utf;

    if (!PyArg_ParseTuple(args, (char *)"z:xmlUTF8Strlen", &utf))
        return(NULL);

    c_retval = xmlUTF8Strlen(utf);
    py_retval = libxml_intWrap((int) c_retval);
    return(py_retval);
}

#ifdef LIBXML_XPATH_ENABLED
PyObject *
libxml_xmlXPathRoot(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    xmlXPathParserContextPtr ctxt;
    PyObject *pyobj_ctxt;

    if (!PyArg_ParseTuple(args, (char *)"O:xmlXPathRoot", &pyobj_ctxt))
        return(NULL);
    ctxt = (xmlXPathParserContextPtr) PyxmlXPathParserContext_Get(pyobj_ctxt);

    xmlXPathRoot(ctxt);
    Py_INCREF(Py_None);
    return(Py_None);
}

#endif /* LIBXML_XPATH_ENABLED */
PyObject *
libxml_xmlCharStrdup(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    xmlChar * c_retval;
    char * cur;

    if (!PyArg_ParseTuple(args, (char *)"z:xmlCharStrdup", &cur))
        return(NULL);

    c_retval = xmlCharStrdup(cur);
    py_retval = libxml_xmlCharPtrWrap((xmlChar *) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlURIGetServer(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    const char * c_retval;
    xmlURIPtr URI;
    PyObject *pyobj_URI;

    if (!PyArg_ParseTuple(args, (char *)"O:xmlURIGetServer", &pyobj_URI))
        return(NULL);
    URI = (xmlURIPtr) PyURI_Get(pyobj_URI);

    c_retval = URI->server;
    py_retval = libxml_charPtrConstWrap((const char *) c_retval);
    return(py_retval);
}

#ifdef LIBXML_HTML_ENABLED
PyObject *
libxml_htmlSaveFileFormat(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    int c_retval;
    char * filename;
    xmlDocPtr cur;
    PyObject *pyobj_cur;
    char * encoding;
    int format;

    if (!PyArg_ParseTuple(args, (char *)"zOzi:htmlSaveFileFormat", &filename, &pyobj_cur, &encoding, &format))
        return(NULL);
    cur = (xmlDocPtr) PyxmlNode_Get(pyobj_cur);

    c_retval = htmlSaveFileFormat(filename, cur, encoding, format);
    py_retval = libxml_intWrap((int) c_retval);
    return(py_retval);
}

#endif /* LIBXML_HTML_ENABLED */
PyObject *
libxml_xmlIOHTTPMatch(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    int c_retval;
    char * filename;

    if (!PyArg_ParseTuple(args, (char *)"z:xmlIOHTTPMatch", &filename))
        return(NULL);

    c_retval = xmlIOHTTPMatch(filename);
    py_retval = libxml_intWrap((int) c_retval);
    return(py_retval);
}

PyObject *
libxml_namePush(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    int c_retval;
    xmlParserCtxtPtr ctxt;
    PyObject *pyobj_ctxt;
    xmlChar * value;

    if (!PyArg_ParseTuple(args, (char *)"Oz:namePush", &pyobj_ctxt, &value))
        return(NULL);
    ctxt = (xmlParserCtxtPtr) PyparserCtxt_Get(pyobj_ctxt);

    c_retval = namePush(ctxt, value);
    py_retval = libxml_intWrap((int) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlUCSIsMalayalam(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    int c_retval;
    int code;

    if (!PyArg_ParseTuple(args, (char *)"i:xmlUCSIsMalayalam", &code))
        return(NULL);

    c_retval = xmlUCSIsMalayalam(code);
    py_retval = libxml_intWrap((int) c_retval);
    return(py_retval);
}

#ifdef LIBXML_XPATH_ENABLED
PyObject *
libxml_xmlXPathRegisterNs(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    int c_retval;
    xmlXPathContextPtr ctxt;
    PyObject *pyobj_ctxt;
    xmlChar * prefix;
    xmlChar * ns_uri;

    if (!PyArg_ParseTuple(args, (char *)"Ozz:xmlXPathRegisterNs", &pyobj_ctxt, &prefix, &ns_uri))
        return(NULL);
    ctxt = (xmlXPathContextPtr) PyxmlXPathContext_Get(pyobj_ctxt);

    c_retval = xmlXPathRegisterNs(ctxt, prefix, ns_uri);
    py_retval = libxml_intWrap((int) c_retval);
    return(py_retval);
}

#endif /* LIBXML_XPATH_ENABLED */
PyObject *
libxml_xmlNodeIsText(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    int c_retval;
    xmlNodePtr node;
    PyObject *pyobj_node;

    if (!PyArg_ParseTuple(args, (char *)"O:xmlNodeIsText", &pyobj_node))
        return(NULL);
    node = (xmlNodePtr) PyxmlNode_Get(pyobj_node);

    c_retval = xmlNodeIsText(node);
    py_retval = libxml_intWrap((int) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlParserSetReplaceEntities(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    xmlParserCtxtPtr ctxt;
    PyObject *pyobj_ctxt;
    int replaceEntities;

    if (!PyArg_ParseTuple(args, (char *)"Oi:xmlParserSetReplaceEntities", &pyobj_ctxt, &replaceEntities))
        return(NULL);
    ctxt = (xmlParserCtxtPtr) PyparserCtxt_Get(pyobj_ctxt);

    ctxt->replaceEntities = replaceEntities;
    Py_INCREF(Py_None);
    return(Py_None);
}

PyObject *
libxml_xmlUCSIsCurrencySymbols(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    int c_retval;
    int code;

    if (!PyArg_ParseTuple(args, (char *)"i:xmlUCSIsCurrencySymbols", &code))
        return(NULL);

    c_retval = xmlUCSIsCurrencySymbols(code);
    py_retval = libxml_intWrap((int) c_retval);
    return(py_retval);
}

#ifdef LIBXML_XPATH_ENABLED
PyObject *
libxml_xmlXPathVariableLookup(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    xmlXPathObjectPtr c_retval;
    xmlXPathContextPtr ctxt;
    PyObject *pyobj_ctxt;
    xmlChar * name;

    if (!PyArg_ParseTuple(args, (char *)"Oz:xmlXPathVariableLookup", &pyobj_ctxt, &name))
        return(NULL);
    ctxt = (xmlXPathContextPtr) PyxmlXPathContext_Get(pyobj_ctxt);

    c_retval = xmlXPathVariableLookup(ctxt, name);
    py_retval = libxml_xmlXPathObjectPtrWrap((xmlXPathObjectPtr) c_retval);
    return(py_retval);
}

#endif /* LIBXML_XPATH_ENABLED */
#ifdef LIBXML_XPATH_ENABLED
PyObject *
libxml_xmlXPathStringEvalNumber(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    double c_retval;
    xmlChar * str;

    if (!PyArg_ParseTuple(args, (char *)"z:xmlXPathStringEvalNumber", &str))
        return(NULL);

    c_retval = xmlXPathStringEvalNumber(str);
    py_retval = libxml_doubleWrap((double) c_retval);
    return(py_retval);
}

#endif /* LIBXML_XPATH_ENABLED */
PyObject *
libxml_xmlUCSIsKanbun(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    int c_retval;
    int code;

    if (!PyArg_ParseTuple(args, (char *)"i:xmlUCSIsKanbun", &code))
        return(NULL);

    c_retval = xmlUCSIsKanbun(code);
    py_retval = libxml_intWrap((int) c_retval);
    return(py_retval);
}

#ifdef LIBXML_XPATH_ENABLED
PyObject *
libxml_xmlXPathCmpNodes(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    int c_retval;
    xmlNodePtr node1;
    PyObject *pyobj_node1;
    xmlNodePtr node2;
    PyObject *pyobj_node2;

    if (!PyArg_ParseTuple(args, (char *)"OO:xmlXPathCmpNodes", &pyobj_node1, &pyobj_node2))
        return(NULL);
    node1 = (xmlNodePtr) PyxmlNode_Get(pyobj_node1);
    node2 = (xmlNodePtr) PyxmlNode_Get(pyobj_node2);

    c_retval = xmlXPathCmpNodes(node1, node2);
    py_retval = libxml_intWrap((int) c_retval);
    return(py_retval);
}

#endif /* LIBXML_XPATH_ENABLED */
#ifdef LIBXML_DEBUG_ENABLED
PyObject *
libxml_xmlDebugDumpAttr(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    FILE * output;
    PyObject *pyobj_output;
    xmlAttrPtr attr;
    PyObject *pyobj_attr;
    int depth;

    if (!PyArg_ParseTuple(args, (char *)"OOi:xmlDebugDumpAttr", &pyobj_output, &pyobj_attr, &depth))
        return(NULL);
    output = (FILE *) PyFile_Get(pyobj_output);
    attr = (xmlAttrPtr) PyxmlNode_Get(pyobj_attr);

    xmlDebugDumpAttr(output, attr, depth);
    Py_INCREF(Py_None);
    return(Py_None);
}

#endif /* LIBXML_DEBUG_ENABLED */
PyObject *
libxml_xmlUTF8Strsize(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    int c_retval;
    xmlChar * utf;
    int len;

    if (!PyArg_ParseTuple(args, (char *)"zi:xmlUTF8Strsize", &utf, &len))
        return(NULL);

    c_retval = xmlUTF8Strsize(utf, len);
    py_retval = libxml_intWrap((int) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlCleanupOutputCallbacks(ATTRIBUTE_UNUSED PyObject *self,ATTRIBUTE_UNUSED  PyObject *args) {

    xmlCleanupOutputCallbacks();
    Py_INCREF(Py_None);
    return(Py_None);
}

PyObject *
libxml_xmlUCSIsLatin1Supplement(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    int c_retval;
    int code;

    if (!PyArg_ParseTuple(args, (char *)"i:xmlUCSIsLatin1Supplement", &code))
        return(NULL);

    c_retval = xmlUCSIsLatin1Supplement(code);
    py_retval = libxml_intWrap((int) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlXPathSetContextNode(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    xmlXPathContextPtr ctxt;
    PyObject *pyobj_ctxt;
    xmlNodePtr node;
    PyObject *pyobj_node;

    if (!PyArg_ParseTuple(args, (char *)"OO:xmlXPathSetContextNode", &pyobj_ctxt, &pyobj_node))
        return(NULL);
    ctxt = (xmlXPathContextPtr) PyxmlXPathContext_Get(pyobj_ctxt);
    node = (xmlNodePtr) PyxmlNode_Get(pyobj_node);

    ctxt->node = node;
    Py_INCREF(Py_None);
    return(Py_None);
}

PyObject *
libxml_xmlSaveFileEnc(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    int c_retval;
    char * filename;
    xmlDocPtr cur;
    PyObject *pyobj_cur;
    char * encoding;

    if (!PyArg_ParseTuple(args, (char *)"zOz:xmlSaveFileEnc", &filename, &pyobj_cur, &encoding))
        return(NULL);
    cur = (xmlDocPtr) PyxmlNode_Get(pyobj_cur);

    c_retval = xmlSaveFileEnc(filename, cur, encoding);
    py_retval = libxml_intWrap((int) c_retval);
    return(py_retval);
}

#ifdef LIBXML_HTML_ENABLED
PyObject *
libxml_htmlFreeParserCtxt(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    htmlParserCtxtPtr ctxt;
    PyObject *pyobj_ctxt;

    if (!PyArg_ParseTuple(args, (char *)"O:htmlFreeParserCtxt", &pyobj_ctxt))
        return(NULL);
    ctxt = (htmlParserCtxtPtr) PyparserCtxt_Get(pyobj_ctxt);

    htmlFreeParserCtxt(ctxt);
    Py_INCREF(Py_None);
    return(Py_None);
}

#endif /* LIBXML_HTML_ENABLED */
PyObject *
libxml_xmlXPathGetFunction(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    const xmlChar * c_retval;
    xmlXPathContextPtr ctxt;
    PyObject *pyobj_ctxt;

    if (!PyArg_ParseTuple(args, (char *)"O:xmlXPathGetFunction", &pyobj_ctxt))
        return(NULL);
    ctxt = (xmlXPathContextPtr) PyxmlXPathContext_Get(pyobj_ctxt);

    c_retval = ctxt->function;
    py_retval = libxml_xmlCharPtrConstWrap((const xmlChar *) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlNodeSetName(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    xmlNodePtr cur;
    PyObject *pyobj_cur;
    xmlChar * name;

    if (!PyArg_ParseTuple(args, (char *)"Oz:xmlNodeSetName", &pyobj_cur, &name))
        return(NULL);
    cur = (xmlNodePtr) PyxmlNode_Get(pyobj_cur);

    xmlNodeSetName(cur, name);
    Py_INCREF(Py_None);
    return(Py_None);
}

PyObject *
libxml_xmlGetIntSubset(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    xmlDtdPtr c_retval;
    xmlDocPtr doc;
    PyObject *pyobj_doc;

    if (!PyArg_ParseTuple(args, (char *)"O:xmlGetIntSubset", &pyobj_doc))
        return(NULL);
    doc = (xmlDocPtr) PyxmlNode_Get(pyobj_doc);

    c_retval = xmlGetIntSubset(doc);
    py_retval = libxml_xmlNodePtrWrap((xmlNodePtr) c_retval);
    return(py_retval);
}

#ifdef LIBXML_DEBUG_ENABLED
PyObject *
libxml_xmlDebugDumpOneNode(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    FILE * output;
    PyObject *pyobj_output;
    xmlNodePtr node;
    PyObject *pyobj_node;
    int depth;

    if (!PyArg_ParseTuple(args, (char *)"OOi:xmlDebugDumpOneNode", &pyobj_output, &pyobj_node, &depth))
        return(NULL);
    output = (FILE *) PyFile_Get(pyobj_output);
    node = (xmlNodePtr) PyxmlNode_Get(pyobj_node);

    xmlDebugDumpOneNode(output, node, depth);
    Py_INCREF(Py_None);
    return(Py_None);
}

#endif /* LIBXML_DEBUG_ENABLED */
PyObject *
libxml_xmlUTF8Strloc(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    int c_retval;
    xmlChar * utf;
    xmlChar * utfchar;

    if (!PyArg_ParseTuple(args, (char *)"zz:xmlUTF8Strloc", &utf, &utfchar))
        return(NULL);

    c_retval = xmlUTF8Strloc(utf, utfchar);
    py_retval = libxml_intWrap((int) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlParseStartTag(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    xmlChar * c_retval;
    xmlParserCtxtPtr ctxt;
    PyObject *pyobj_ctxt;

    if (!PyArg_ParseTuple(args, (char *)"O:xmlParseStartTag", &pyobj_ctxt))
        return(NULL);
    ctxt = (xmlParserCtxtPtr) PyparserCtxt_Get(pyobj_ctxt);

    c_retval = xmlParseStartTag(ctxt);
    py_retval = libxml_xmlCharPtrWrap((xmlChar *) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlSetupParserForBuffer(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    xmlParserCtxtPtr ctxt;
    PyObject *pyobj_ctxt;
    xmlChar * buffer;
    char * filename;

    if (!PyArg_ParseTuple(args, (char *)"Ozz:xmlSetupParserForBuffer", &pyobj_ctxt, &buffer, &filename))
        return(NULL);
    ctxt = (xmlParserCtxtPtr) PyparserCtxt_Get(pyobj_ctxt);

    xmlSetupParserForBuffer(ctxt, buffer, filename);
    Py_INCREF(Py_None);
    return(Py_None);
}

PyObject *
libxml_xmlCreateDocParserCtxt(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    xmlParserCtxtPtr c_retval;
    xmlChar * cur;

    if (!PyArg_ParseTuple(args, (char *)"z:xmlCreateDocParserCtxt", &cur))
        return(NULL);

    c_retval = xmlCreateDocParserCtxt(cur);
    py_retval = libxml_xmlParserCtxtPtrWrap((xmlParserCtxtPtr) c_retval);
    return(py_retval);
}

#ifdef LIBXML_XPATH_ENABLED
PyObject *
libxml_xmlXPathSubstringAfterFunction(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    xmlXPathParserContextPtr ctxt;
    PyObject *pyobj_ctxt;
    int nargs;

    if (!PyArg_ParseTuple(args, (char *)"Oi:xmlXPathSubstringAfterFunction", &pyobj_ctxt, &nargs))
        return(NULL);
    ctxt = (xmlXPathParserContextPtr) PyxmlXPathParserContext_Get(pyobj_ctxt);

    xmlXPathSubstringAfterFunction(ctxt, nargs);
    Py_INCREF(Py_None);
    return(Py_None);
}

#endif /* LIBXML_XPATH_ENABLED */
PyObject *
libxml_xmlNewTextReaderFilename(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    xmlTextReaderPtr c_retval;
    char * URI;

    if (!PyArg_ParseTuple(args, (char *)"z:xmlNewTextReaderFilename", &URI))
        return(NULL);

    c_retval = xmlNewTextReaderFilename(URI);
    py_retval = libxml_xmlTextReaderPtrWrap((xmlTextReaderPtr) c_retval);
    return(py_retval);
}

#ifdef LIBXML_XPATH_ENABLED
PyObject *
libxml_xmlXPathNumberFunction(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    xmlXPathParserContextPtr ctxt;
    PyObject *pyobj_ctxt;
    int nargs;

    if (!PyArg_ParseTuple(args, (char *)"Oi:xmlXPathNumberFunction", &pyobj_ctxt, &nargs))
        return(NULL);
    ctxt = (xmlXPathParserContextPtr) PyxmlXPathParserContext_Get(pyobj_ctxt);

    xmlXPathNumberFunction(ctxt, nargs);
    Py_INCREF(Py_None);
    return(Py_None);
}

#endif /* LIBXML_XPATH_ENABLED */
PyObject *
libxml_xmlUCSIsDingbats(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    int c_retval;
    int code;

    if (!PyArg_ParseTuple(args, (char *)"i:xmlUCSIsDingbats", &code))
        return(NULL);

    c_retval = xmlUCSIsDingbats(code);
    py_retval = libxml_intWrap((int) c_retval);
    return(py_retval);
}

#ifdef LIBXML_HTML_ENABLED
PyObject *
libxml_htmlNodeDumpFormatOutput(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    xmlOutputBufferPtr buf;
    PyObject *pyobj_buf;
    xmlDocPtr doc;
    PyObject *pyobj_doc;
    xmlNodePtr cur;
    PyObject *pyobj_cur;
    char * encoding;
    int format;

    if (!PyArg_ParseTuple(args, (char *)"OOOzi:htmlNodeDumpFormatOutput", &pyobj_buf, &pyobj_doc, &pyobj_cur, &encoding, &format))
        return(NULL);
    buf = (xmlOutputBufferPtr) PyoutputBuffer_Get(pyobj_buf);
    doc = (xmlDocPtr) PyxmlNode_Get(pyobj_doc);
    cur = (xmlNodePtr) PyxmlNode_Get(pyobj_cur);

    htmlNodeDumpFormatOutput(buf, doc, cur, encoding, format);
    Py_INCREF(Py_None);
    return(Py_None);
}

#endif /* LIBXML_HTML_ENABLED */
#ifdef LIBXML_DEBUG_ENABLED
PyObject *
libxml_xmlLsOneNode(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    FILE * output;
    PyObject *pyobj_output;
    xmlNodePtr node;
    PyObject *pyobj_node;

    if (!PyArg_ParseTuple(args, (char *)"OO:xmlLsOneNode", &pyobj_output, &pyobj_node))
        return(NULL);
    output = (FILE *) PyFile_Get(pyobj_output);
    node = (xmlNodePtr) PyxmlNode_Get(pyobj_node);

    xmlLsOneNode(output, node);
    Py_INCREF(Py_None);
    return(Py_None);
}

#endif /* LIBXML_DEBUG_ENABLED */
PyObject *
libxml_xmlParseURIReference(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    int c_retval;
    xmlURIPtr uri;
    PyObject *pyobj_uri;
    char * str;

    if (!PyArg_ParseTuple(args, (char *)"Oz:xmlParseURIReference", &pyobj_uri, &str))
        return(NULL);
    uri = (xmlURIPtr) PyURI_Get(pyobj_uri);

    c_retval = xmlParseURIReference(uri, str);
    py_retval = libxml_intWrap((int) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlParseExternalSubset(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    xmlParserCtxtPtr ctxt;
    PyObject *pyobj_ctxt;
    xmlChar * ExternalID;
    xmlChar * SystemID;

    if (!PyArg_ParseTuple(args, (char *)"Ozz:xmlParseExternalSubset", &pyobj_ctxt, &ExternalID, &SystemID))
        return(NULL);
    ctxt = (xmlParserCtxtPtr) PyparserCtxt_Get(pyobj_ctxt);

    xmlParseExternalSubset(ctxt, ExternalID, SystemID);
    Py_INCREF(Py_None);
    return(Py_None);
}

PyObject *
libxml_xmlNewDocNodeEatName(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    xmlNodePtr c_retval;
    xmlDocPtr doc;
    PyObject *pyobj_doc;
    xmlNsPtr ns;
    PyObject *pyobj_ns;
    xmlChar * name;
    xmlChar * content;

    if (!PyArg_ParseTuple(args, (char *)"OOzz:xmlNewDocNodeEatName", &pyobj_doc, &pyobj_ns, &name, &content))
        return(NULL);
    doc = (xmlDocPtr) PyxmlNode_Get(pyobj_doc);
    ns = (xmlNsPtr) PyxmlNode_Get(pyobj_ns);

    c_retval = xmlNewDocNodeEatName(doc, ns, name, content);
    py_retval = libxml_xmlNodePtrWrap((xmlNodePtr) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlUCSIsHangulSyllables(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    int c_retval;
    int code;

    if (!PyArg_ParseTuple(args, (char *)"i:xmlUCSIsHangulSyllables", &code))
        return(NULL);

    c_retval = xmlUCSIsHangulSyllables(code);
    py_retval = libxml_intWrap((int) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlStrndup(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    xmlChar * c_retval;
    xmlChar * cur;
    int len;

    if (!PyArg_ParseTuple(args, (char *)"zi:xmlStrndup", &cur, &len))
        return(NULL);

    c_retval = xmlStrndup(cur, len);
    py_retval = libxml_xmlCharPtrWrap((xmlChar *) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlXPathParserGetContext(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    xmlXPathContextPtr c_retval;
    xmlXPathParserContextPtr ctxt;
    PyObject *pyobj_ctxt;

    if (!PyArg_ParseTuple(args, (char *)"O:xmlXPathParserGetContext", &pyobj_ctxt))
        return(NULL);
    ctxt = (xmlXPathParserContextPtr) PyxmlXPathParserContext_Get(pyobj_ctxt);

    c_retval = ctxt->context;
    py_retval = libxml_xmlXPathContextPtrWrap((xmlXPathContextPtr) c_retval);
    return(py_retval);
}

#ifdef LIBXML_XPATH_ENABLED
PyObject *
libxml_xmlXPathBooleanFunction(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    xmlXPathParserContextPtr ctxt;
    PyObject *pyobj_ctxt;
    int nargs;

    if (!PyArg_ParseTuple(args, (char *)"Oi:xmlXPathBooleanFunction", &pyobj_ctxt, &nargs))
        return(NULL);
    ctxt = (xmlXPathParserContextPtr) PyxmlXPathParserContext_Get(pyobj_ctxt);

    xmlXPathBooleanFunction(ctxt, nargs);
    Py_INCREF(Py_None);
    return(Py_None);
}

#endif /* LIBXML_XPATH_ENABLED */
PyObject *
libxml_xmlTextReaderValue(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    xmlChar * c_retval;
    xmlTextReaderPtr reader;
    PyObject *pyobj_reader;

    if (!PyArg_ParseTuple(args, (char *)"O:xmlTextReaderValue", &pyobj_reader))
        return(NULL);
    reader = (xmlTextReaderPtr) PyxmlTextReader_Get(pyobj_reader);

    c_retval = xmlTextReaderValue(reader);
    py_retval = libxml_xmlCharPtrWrap((xmlChar *) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlRecoverFile(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    xmlDocPtr c_retval;
    char * filename;

    if (!PyArg_ParseTuple(args, (char *)"z:xmlRecoverFile", &filename))
        return(NULL);

    c_retval = xmlRecoverFile(filename);
    py_retval = libxml_xmlDocPtrWrap((xmlDocPtr) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlURIEscapeStr(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    xmlChar * c_retval;
    xmlChar * str;
    xmlChar * list;

    if (!PyArg_ParseTuple(args, (char *)"zz:xmlURIEscapeStr", &str, &list))
        return(NULL);

    c_retval = xmlURIEscapeStr(str, list);
    py_retval = libxml_xmlCharPtrWrap((xmlChar *) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlTextReaderReadInnerXml(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    xmlChar * c_retval;
    xmlTextReaderPtr reader;
    PyObject *pyobj_reader;

    if (!PyArg_ParseTuple(args, (char *)"O:xmlTextReaderReadInnerXml", &pyobj_reader))
        return(NULL);
    reader = (xmlTextReaderPtr) PyxmlTextReader_Get(pyobj_reader);

    c_retval = xmlTextReaderReadInnerXml(reader);
    py_retval = libxml_xmlCharPtrWrap((xmlChar *) c_retval);
    return(py_retval);
}

#ifdef LIBXML_XPATH_ENABLED
PyObject *
libxml_xmlXPathNextFollowingSibling(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    xmlNodePtr c_retval;
    xmlXPathParserContextPtr ctxt;
    PyObject *pyobj_ctxt;
    xmlNodePtr cur;
    PyObject *pyobj_cur;

    if (!PyArg_ParseTuple(args, (char *)"OO:xmlXPathNextFollowingSibling", &pyobj_ctxt, &pyobj_cur))
        return(NULL);
    ctxt = (xmlXPathParserContextPtr) PyxmlXPathParserContext_Get(pyobj_ctxt);
    cur = (xmlNodePtr) PyxmlNode_Get(pyobj_cur);

    c_retval = xmlXPathNextFollowingSibling(ctxt, cur);
    py_retval = libxml_xmlNodePtrWrap((xmlNodePtr) c_retval);
    return(py_retval);
}

#endif /* LIBXML_XPATH_ENABLED */
PyObject *
libxml_xmlIsExtender(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    int c_retval;
    int c;

    if (!PyArg_ParseTuple(args, (char *)"i:xmlIsExtender", &c))
        return(NULL);

    c_retval = xmlIsExtender(c);
    py_retval = libxml_intWrap((int) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlAddDocEntity(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    xmlEntityPtr c_retval;
    xmlDocPtr doc;
    PyObject *pyobj_doc;
    xmlChar * name;
    int type;
    xmlChar * ExternalID;
    xmlChar * SystemID;
    xmlChar * content;

    if (!PyArg_ParseTuple(args, (char *)"Ozizzz:xmlAddDocEntity", &pyobj_doc, &name, &type, &ExternalID, &SystemID, &content))
        return(NULL);
    doc = (xmlDocPtr) PyxmlNode_Get(pyobj_doc);

    c_retval = xmlAddDocEntity(doc, name, type, ExternalID, SystemID, content);
    py_retval = libxml_xmlNodePtrWrap((xmlNodePtr) c_retval);
    return(py_retval);
}

#ifdef LIBXML_XPATH_ENABLED
PyObject *
libxml_xmlXPathCastNumberToBoolean(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    int c_retval;
    double val;

    if (!PyArg_ParseTuple(args, (char *)"d:xmlXPathCastNumberToBoolean", &val))
        return(NULL);

    c_retval = xmlXPathCastNumberToBoolean(val);
    py_retval = libxml_intWrap((int) c_retval);
    return(py_retval);
}

#endif /* LIBXML_XPATH_ENABLED */
PyObject *
libxml_xmlValidateQName(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    int c_retval;
    xmlChar * value;
    int space;

    if (!PyArg_ParseTuple(args, (char *)"zi:xmlValidateQName", &value, &space))
        return(NULL);

    c_retval = xmlValidateQName(value, space);
    py_retval = libxml_intWrap((int) c_retval);
    return(py_retval);
}

#ifdef LIBXML_XPATH_ENABLED
PyObject *
libxml_xmlXPathCompareValues(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    int c_retval;
    xmlXPathParserContextPtr ctxt;
    PyObject *pyobj_ctxt;
    int inf;
    int strict;

    if (!PyArg_ParseTuple(args, (char *)"Oii:xmlXPathCompareValues", &pyobj_ctxt, &inf, &strict))
        return(NULL);
    ctxt = (xmlXPathParserContextPtr) PyxmlXPathParserContext_Get(pyobj_ctxt);

    c_retval = xmlXPathCompareValues(ctxt, inf, strict);
    py_retval = libxml_intWrap((int) c_retval);
    return(py_retval);
}

#endif /* LIBXML_XPATH_ENABLED */
PyObject *
libxml_xmlUCSIsMyanmar(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    int c_retval;
    int code;

    if (!PyArg_ParseTuple(args, (char *)"i:xmlUCSIsMyanmar", &code))
        return(NULL);

    c_retval = xmlUCSIsMyanmar(code);
    py_retval = libxml_intWrap((int) c_retval);
    return(py_retval);
}

#ifdef LIBXML_XPATH_ENABLED
PyObject *
libxml_xmlXPathCountFunction(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    xmlXPathParserContextPtr ctxt;
    PyObject *pyobj_ctxt;
    int nargs;

    if (!PyArg_ParseTuple(args, (char *)"Oi:xmlXPathCountFunction", &pyobj_ctxt, &nargs))
        return(NULL);
    ctxt = (xmlXPathParserContextPtr) PyxmlXPathParserContext_Get(pyobj_ctxt);

    xmlXPathCountFunction(ctxt, nargs);
    Py_INCREF(Py_None);
    return(Py_None);
}

#endif /* LIBXML_XPATH_ENABLED */
#ifdef LIBXML_HTML_ENABLED
PyObject *
libxml_htmlParseCharRef(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    int c_retval;
    htmlParserCtxtPtr ctxt;
    PyObject *pyobj_ctxt;

    if (!PyArg_ParseTuple(args, (char *)"O:htmlParseCharRef", &pyobj_ctxt))
        return(NULL);
    ctxt = (htmlParserCtxtPtr) PyparserCtxt_Get(pyobj_ctxt);

    c_retval = htmlParseCharRef(ctxt);
    py_retval = libxml_intWrap((int) c_retval);
    return(py_retval);
}

#endif /* LIBXML_HTML_ENABLED */
#ifdef LIBXML_XPATH_ENABLED
PyObject *
libxml_xmlXPathIsNodeType(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    int c_retval;
    xmlChar * name;

    if (!PyArg_ParseTuple(args, (char *)"z:xmlXPathIsNodeType", &name))
        return(NULL);

    c_retval = xmlXPathIsNodeType(name);
    py_retval = libxml_intWrap((int) c_retval);
    return(py_retval);
}

#endif /* LIBXML_XPATH_ENABLED */
PyObject *
libxml_xmlUTF8Strndup(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    xmlChar * c_retval;
    xmlChar * utf;
    int len;

    if (!PyArg_ParseTuple(args, (char *)"zi:xmlUTF8Strndup", &utf, &len))
        return(NULL);

    c_retval = xmlUTF8Strndup(utf, len);
    py_retval = libxml_xmlCharPtrWrap((xmlChar *) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlBuildURI(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    xmlChar * c_retval;
    xmlChar * URI;
    xmlChar * base;

    if (!PyArg_ParseTuple(args, (char *)"zz:xmlBuildURI", &URI, &base))
        return(NULL);

    c_retval = xmlBuildURI(URI, base);
    py_retval = libxml_xmlCharPtrWrap((xmlChar *) c_retval);
    return(py_retval);
}

#ifdef LIBXML_XPATH_ENABLED
PyObject *
libxml_xmlXPathCastBooleanToString(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    xmlChar * c_retval;
    int val;

    if (!PyArg_ParseTuple(args, (char *)"i:xmlXPathCastBooleanToString", &val))
        return(NULL);

    c_retval = xmlXPathCastBooleanToString(val);
    py_retval = libxml_xmlCharPtrWrap((xmlChar *) c_retval);
    return(py_retval);
}

#endif /* LIBXML_XPATH_ENABLED */
#ifdef LIBXML_XPATH_ENABLED
PyObject *
libxml_xmlXPathSubstringBeforeFunction(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    xmlXPathParserContextPtr ctxt;
    PyObject *pyobj_ctxt;
    int nargs;

    if (!PyArg_ParseTuple(args, (char *)"Oi:xmlXPathSubstringBeforeFunction", &pyobj_ctxt, &nargs))
        return(NULL);
    ctxt = (xmlXPathParserContextPtr) PyxmlXPathParserContext_Get(pyobj_ctxt);

    xmlXPathSubstringBeforeFunction(ctxt, nargs);
    Py_INCREF(Py_None);
    return(Py_None);
}

#endif /* LIBXML_XPATH_ENABLED */
#ifdef LIBXML_XPATH_ENABLED
PyObject *
libxml_xmlXPathNextFollowing(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    xmlNodePtr c_retval;
    xmlXPathParserContextPtr ctxt;
    PyObject *pyobj_ctxt;
    xmlNodePtr cur;
    PyObject *pyobj_cur;

    if (!PyArg_ParseTuple(args, (char *)"OO:xmlXPathNextFollowing", &pyobj_ctxt, &pyobj_cur))
        return(NULL);
    ctxt = (xmlXPathParserContextPtr) PyxmlXPathParserContext_Get(pyobj_ctxt);
    cur = (xmlNodePtr) PyxmlNode_Get(pyobj_cur);

    c_retval = xmlXPathNextFollowing(ctxt, cur);
    py_retval = libxml_xmlNodePtrWrap((xmlNodePtr) c_retval);
    return(py_retval);
}

#endif /* LIBXML_XPATH_ENABLED */
PyObject *
libxml_xmlValidateNameValue(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    int c_retval;
    xmlChar * value;

    if (!PyArg_ParseTuple(args, (char *)"z:xmlValidateNameValue", &value))
        return(NULL);

    c_retval = xmlValidateNameValue(value);
    py_retval = libxml_intWrap((int) c_retval);
    return(py_retval);
}

#ifdef LIBXML_XPATH_ENABLED
PyObject *
libxml_valuePop(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    xmlXPathObjectPtr c_retval;
    xmlXPathParserContextPtr ctxt;
    PyObject *pyobj_ctxt;

    if (!PyArg_ParseTuple(args, (char *)"O:valuePop", &pyobj_ctxt))
        return(NULL);
    ctxt = (xmlXPathParserContextPtr) PyxmlXPathParserContext_Get(pyobj_ctxt);

    c_retval = valuePop(ctxt);
    py_retval = libxml_xmlXPathObjectPtrWrap((xmlXPathObjectPtr) c_retval);
    return(py_retval);
}

#endif /* LIBXML_XPATH_ENABLED */
PyObject *
libxml_xmlCleanupPredefinedEntities(ATTRIBUTE_UNUSED PyObject *self,ATTRIBUTE_UNUSED  PyObject *args) {

    xmlCleanupPredefinedEntities();
    Py_INCREF(Py_None);
    return(Py_None);
}

PyObject *
libxml_xmlUCSIsHangulCompatibilityJamo(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    int c_retval;
    int code;

    if (!PyArg_ParseTuple(args, (char *)"i:xmlUCSIsHangulCompatibilityJamo", &code))
        return(NULL);

    c_retval = xmlUCSIsHangulCompatibilityJamo(code);
    py_retval = libxml_intWrap((int) c_retval);
    return(py_retval);
}

#ifdef LIBXML_XPATH_ENABLED
PyObject *
libxml_xmlXPathNewNodeSet(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    xmlXPathObjectPtr c_retval;
    xmlNodePtr val;
    PyObject *pyobj_val;

    if (!PyArg_ParseTuple(args, (char *)"O:xmlXPathNewNodeSet", &pyobj_val))
        return(NULL);
    val = (xmlNodePtr) PyxmlNode_Get(pyobj_val);

    c_retval = xmlXPathNewNodeSet(val);
    py_retval = libxml_xmlXPathObjectPtrWrap((xmlXPathObjectPtr) c_retval);
    return(py_retval);
}

#endif /* LIBXML_XPATH_ENABLED */
PyObject *
libxml_xmlNewTextChild(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    xmlNodePtr c_retval;
    xmlNodePtr parent;
    PyObject *pyobj_parent;
    xmlNsPtr ns;
    PyObject *pyobj_ns;
    xmlChar * name;
    xmlChar * content;

    if (!PyArg_ParseTuple(args, (char *)"OOzz:xmlNewTextChild", &pyobj_parent, &pyobj_ns, &name, &content))
        return(NULL);
    parent = (xmlNodePtr) PyxmlNode_Get(pyobj_parent);
    ns = (xmlNsPtr) PyxmlNode_Get(pyobj_ns);

    c_retval = xmlNewTextChild(parent, ns, name, content);
    py_retval = libxml_xmlNodePtrWrap((xmlNodePtr) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlStringDecodeEntities(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    xmlChar * c_retval;
    xmlParserCtxtPtr ctxt;
    PyObject *pyobj_ctxt;
    xmlChar * str;
    int what;
    xmlChar end;
    xmlChar end2;
    xmlChar end3;

    if (!PyArg_ParseTuple(args, (char *)"Oziccc:xmlStringDecodeEntities", &pyobj_ctxt, &str, &what, &end, &end2, &end3))
        return(NULL);
    ctxt = (xmlParserCtxtPtr) PyparserCtxt_Get(pyobj_ctxt);

    c_retval = xmlStringDecodeEntities(ctxt, str, what, end, end2, end3);
    py_retval = libxml_xmlCharPtrWrap((xmlChar *) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlFreeCatalog(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    xmlCatalogPtr catal;
    PyObject *pyobj_catal;

    if (!PyArg_ParseTuple(args, (char *)"O:xmlFreeCatalog", &pyobj_catal))
        return(NULL);
    catal = (xmlCatalogPtr) Pycatalog_Get(pyobj_catal);

    xmlFreeCatalog(catal);
    Py_INCREF(Py_None);
    return(Py_None);
}

#ifdef LIBXML_XPATH_ENABLED
PyObject *
libxml_xmlXPathNodeSetFreeNs(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    xmlNsPtr ns;
    PyObject *pyobj_ns;

    if (!PyArg_ParseTuple(args, (char *)"O:xmlXPathNodeSetFreeNs", &pyobj_ns))
        return(NULL);
    ns = (xmlNsPtr) PyxmlNode_Get(pyobj_ns);

    xmlXPathNodeSetFreeNs(ns);
    Py_INCREF(Py_None);
    return(Py_None);
}

#endif /* LIBXML_XPATH_ENABLED */
PyObject *
libxml_xmlParseElement(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    xmlParserCtxtPtr ctxt;
    PyObject *pyobj_ctxt;

    if (!PyArg_ParseTuple(args, (char *)"O:xmlParseElement", &pyobj_ctxt))
        return(NULL);
    ctxt = (xmlParserCtxtPtr) PyparserCtxt_Get(pyobj_ctxt);

    xmlParseElement(ctxt);
    Py_INCREF(Py_None);
    return(Py_None);
}

PyObject *
libxml_xmlAddChild(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    xmlNodePtr c_retval;
    xmlNodePtr parent;
    PyObject *pyobj_parent;
    xmlNodePtr cur;
    PyObject *pyobj_cur;

    if (!PyArg_ParseTuple(args, (char *)"OO:xmlAddChild", &pyobj_parent, &pyobj_cur))
        return(NULL);
    parent = (xmlNodePtr) PyxmlNode_Get(pyobj_parent);
    cur = (xmlNodePtr) PyxmlNode_Get(pyobj_cur);

    c_retval = xmlAddChild(parent, cur);
    py_retval = libxml_xmlNodePtrWrap((xmlNodePtr) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlUCSIsArabicPresentationFormsB(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    int c_retval;
    int code;

    if (!PyArg_ParseTuple(args, (char *)"i:xmlUCSIsArabicPresentationFormsB", &code))
        return(NULL);

    c_retval = xmlUCSIsArabicPresentationFormsB(code);
    py_retval = libxml_intWrap((int) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlTextReaderDepth(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    int c_retval;
    xmlTextReaderPtr reader;
    PyObject *pyobj_reader;

    if (!PyArg_ParseTuple(args, (char *)"O:xmlTextReaderDepth", &pyobj_reader))
        return(NULL);
    reader = (xmlTextReaderPtr) PyxmlTextReader_Get(pyobj_reader);

    c_retval = xmlTextReaderDepth(reader);
    py_retval = libxml_intWrap((int) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlUCSIsOgham(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    int c_retval;
    int code;

    if (!PyArg_ParseTuple(args, (char *)"i:xmlUCSIsOgham", &code))
        return(NULL);

    c_retval = xmlUCSIsOgham(code);
    py_retval = libxml_intWrap((int) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlNewDocRawNode(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    xmlNodePtr c_retval;
    xmlDocPtr doc;
    PyObject *pyobj_doc;
    xmlNsPtr ns;
    PyObject *pyobj_ns;
    xmlChar * name;
    xmlChar * content;

    if (!PyArg_ParseTuple(args, (char *)"OOzz:xmlNewDocRawNode", &pyobj_doc, &pyobj_ns, &name, &content))
        return(NULL);
    doc = (xmlDocPtr) PyxmlNode_Get(pyobj_doc);
    ns = (xmlNsPtr) PyxmlNode_Get(pyobj_ns);

    c_retval = xmlNewDocRawNode(doc, ns, name, content);
    py_retval = libxml_xmlNodePtrWrap((xmlNodePtr) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlUCSIsBopomofoExtended(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    int c_retval;
    int code;

    if (!PyArg_ParseTuple(args, (char *)"i:xmlUCSIsBopomofoExtended", &code))
        return(NULL);

    c_retval = xmlUCSIsBopomofoExtended(code);
    py_retval = libxml_intWrap((int) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlTextReaderNamespaceUri(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    xmlChar * c_retval;
    xmlTextReaderPtr reader;
    PyObject *pyobj_reader;

    if (!PyArg_ParseTuple(args, (char *)"O:xmlTextReaderNamespaceUri", &pyobj_reader))
        return(NULL);
    reader = (xmlTextReaderPtr) PyxmlTextReader_Get(pyobj_reader);

    c_retval = xmlTextReaderNamespaceUri(reader);
    py_retval = libxml_xmlCharPtrWrap((xmlChar *) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlParseVersionInfo(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    xmlChar * c_retval;
    xmlParserCtxtPtr ctxt;
    PyObject *pyobj_ctxt;

    if (!PyArg_ParseTuple(args, (char *)"O:xmlParseVersionInfo", &pyobj_ctxt))
        return(NULL);
    ctxt = (xmlParserCtxtPtr) PyparserCtxt_Get(pyobj_ctxt);

    c_retval = xmlParseVersionInfo(ctxt);
    py_retval = libxml_xmlCharPtrWrap((xmlChar *) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlUCSIsArrows(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    int c_retval;
    int code;

    if (!PyArg_ParseTuple(args, (char *)"i:xmlUCSIsArrows", &code))
        return(NULL);

    c_retval = xmlUCSIsArrows(code);
    py_retval = libxml_intWrap((int) c_retval);
    return(py_retval);
}

#ifdef LIBXML_SCHEMAS_ENABLED
PyObject *
libxml_xmlRelaxNGDump(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    FILE * output;
    PyObject *pyobj_output;
    xmlRelaxNGPtr schema;
    PyObject *pyobj_schema;

    if (!PyArg_ParseTuple(args, (char *)"OO:xmlRelaxNGDump", &pyobj_output, &pyobj_schema))
        return(NULL);
    output = (FILE *) PyFile_Get(pyobj_output);
    schema = (xmlRelaxNGPtr) PyrelaxNgSchema_Get(pyobj_schema);

    xmlRelaxNGDump(output, schema);
    Py_INCREF(Py_None);
    return(Py_None);
}

#endif /* LIBXML_SCHEMAS_ENABLED */
PyObject *
libxml_xmlRegisterHTTPPostCallbacks(ATTRIBUTE_UNUSED PyObject *self,ATTRIBUTE_UNUSED  PyObject *args) {

    xmlRegisterHTTPPostCallbacks();
    Py_INCREF(Py_None);
    return(Py_None);
}

PyObject *
libxml_xmlFreeURI(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    xmlURIPtr uri;
    PyObject *pyobj_uri;

    if (!PyArg_ParseTuple(args, (char *)"O:xmlFreeURI", &pyobj_uri))
        return(NULL);
    uri = (xmlURIPtr) PyURI_Get(pyobj_uri);

    xmlFreeURI(uri);
    Py_INCREF(Py_None);
    return(Py_None);
}

PyObject *
libxml_xmlSetTreeDoc(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    xmlNodePtr tree;
    PyObject *pyobj_tree;
    xmlDocPtr doc;
    PyObject *pyobj_doc;

    if (!PyArg_ParseTuple(args, (char *)"OO:xmlSetTreeDoc", &pyobj_tree, &pyobj_doc))
        return(NULL);
    tree = (xmlNodePtr) PyxmlNode_Get(pyobj_tree);
    doc = (xmlDocPtr) PyxmlNode_Get(pyobj_doc);

    xmlSetTreeDoc(tree, doc);
    Py_INCREF(Py_None);
    return(Py_None);
}

PyObject *
libxml_xmlUCSIsMathematicalOperators(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    int c_retval;
    int code;

    if (!PyArg_ParseTuple(args, (char *)"i:xmlUCSIsMathematicalOperators", &code))
        return(NULL);

    c_retval = xmlUCSIsMathematicalOperators(code);
    py_retval = libxml_intWrap((int) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlCopyNodeList(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    xmlNodePtr c_retval;
    xmlNodePtr node;
    PyObject *pyobj_node;

    if (!PyArg_ParseTuple(args, (char *)"O:xmlCopyNodeList", &pyobj_node))
        return(NULL);
    node = (xmlNodePtr) PyxmlNode_Get(pyobj_node);

    c_retval = xmlCopyNodeList(node);
    py_retval = libxml_xmlNodePtrWrap((xmlNodePtr) c_retval);
    return(py_retval);
}

#ifdef LIBXML_XPATH_ENABLED
PyObject *
libxml_xmlXPathNextParent(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    xmlNodePtr c_retval;
    xmlXPathParserContextPtr ctxt;
    PyObject *pyobj_ctxt;
    xmlNodePtr cur;
    PyObject *pyobj_cur;

    if (!PyArg_ParseTuple(args, (char *)"OO:xmlXPathNextParent", &pyobj_ctxt, &pyobj_cur))
        return(NULL);
    ctxt = (xmlXPathParserContextPtr) PyxmlXPathParserContext_Get(pyobj_ctxt);
    cur = (xmlNodePtr) PyxmlNode_Get(pyobj_cur);

    c_retval = xmlXPathNextParent(ctxt, cur);
    py_retval = libxml_xmlNodePtrWrap((xmlNodePtr) c_retval);
    return(py_retval);
}

#endif /* LIBXML_XPATH_ENABLED */
#ifdef LIBXML_XPATH_ENABLED
PyObject *
libxml_xmlXPathNewValueTree(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    xmlXPathObjectPtr c_retval;
    xmlNodePtr val;
    PyObject *pyobj_val;

    if (!PyArg_ParseTuple(args, (char *)"O:xmlXPathNewValueTree", &pyobj_val))
        return(NULL);
    val = (xmlNodePtr) PyxmlNode_Get(pyobj_val);

    c_retval = xmlXPathNewValueTree(val);
    py_retval = libxml_xmlXPathObjectPtrWrap((xmlXPathObjectPtr) c_retval);
    return(py_retval);
}

#endif /* LIBXML_XPATH_ENABLED */
PyObject *
libxml_xmlUCSIsOldItalic(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    int c_retval;
    int code;

    if (!PyArg_ParseTuple(args, (char *)"i:xmlUCSIsOldItalic", &code))
        return(NULL);

    c_retval = xmlUCSIsOldItalic(code);
    py_retval = libxml_intWrap((int) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlStrchr(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    const xmlChar * c_retval;
    xmlChar * str;
    xmlChar val;

    if (!PyArg_ParseTuple(args, (char *)"zc:xmlStrchr", &str, &val))
        return(NULL);

    c_retval = xmlStrchr(str, val);
    py_retval = libxml_xmlCharPtrConstWrap((const xmlChar *) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlXPathGetContextSize(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    int c_retval;
    xmlXPathContextPtr ctxt;
    PyObject *pyobj_ctxt;

    if (!PyArg_ParseTuple(args, (char *)"O:xmlXPathGetContextSize", &pyobj_ctxt))
        return(NULL);
    ctxt = (xmlXPathContextPtr) PyxmlXPathContext_Get(pyobj_ctxt);

    c_retval = ctxt->contextSize;
    py_retval = libxml_intWrap((int) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlURIGetPath(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    const char * c_retval;
    xmlURIPtr URI;
    PyObject *pyobj_URI;

    if (!PyArg_ParseTuple(args, (char *)"O:xmlURIGetPath", &pyobj_URI))
        return(NULL);
    URI = (xmlURIPtr) PyURI_Get(pyobj_URI);

    c_retval = URI->path;
    py_retval = libxml_charPtrConstWrap((const char *) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlSetProp(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    xmlAttrPtr c_retval;
    xmlNodePtr node;
    PyObject *pyobj_node;
    xmlChar * name;
    xmlChar * value;

    if (!PyArg_ParseTuple(args, (char *)"Ozz:xmlSetProp", &pyobj_node, &name, &value))
        return(NULL);
    node = (xmlNodePtr) PyxmlNode_Get(pyobj_node);

    c_retval = xmlSetProp(node, name, value);
    py_retval = libxml_xmlNodePtrWrap((xmlNodePtr) c_retval);
    return(py_retval);
}

#ifdef LIBXML_XPTR_ENABLED
PyObject *
libxml_xmlXPtrNewRangeNodes(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    xmlXPathObjectPtr c_retval;
    xmlNodePtr start;
    PyObject *pyobj_start;
    xmlNodePtr end;
    PyObject *pyobj_end;

    if (!PyArg_ParseTuple(args, (char *)"OO:xmlXPtrNewRangeNodes", &pyobj_start, &pyobj_end))
        return(NULL);
    start = (xmlNodePtr) PyxmlNode_Get(pyobj_start);
    end = (xmlNodePtr) PyxmlNode_Get(pyobj_end);

    c_retval = xmlXPtrNewRangeNodes(start, end);
    py_retval = libxml_xmlXPathObjectPtrWrap((xmlXPathObjectPtr) c_retval);
    return(py_retval);
}

#endif /* LIBXML_XPTR_ENABLED */
PyObject *
libxml_xmlNodeGetContent(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    xmlChar * c_retval;
    xmlNodePtr cur;
    PyObject *pyobj_cur;

    if (!PyArg_ParseTuple(args, (char *)"O:xmlNodeGetContent", &pyobj_cur))
        return(NULL);
    cur = (xmlNodePtr) PyxmlNode_Get(pyobj_cur);

    c_retval = xmlNodeGetContent(cur);
    py_retval = libxml_xmlCharPtrWrap((xmlChar *) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlRemoveID(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    int c_retval;
    xmlDocPtr doc;
    PyObject *pyobj_doc;
    xmlAttrPtr attr;
    PyObject *pyobj_attr;

    if (!PyArg_ParseTuple(args, (char *)"OO:xmlRemoveID", &pyobj_doc, &pyobj_attr))
        return(NULL);
    doc = (xmlDocPtr) PyxmlNode_Get(pyobj_doc);
    attr = (xmlAttrPtr) PyxmlNode_Get(pyobj_attr);

    c_retval = xmlRemoveID(doc, attr);
    py_retval = libxml_intWrap((int) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlTextReaderName(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    xmlChar * c_retval;
    xmlTextReaderPtr reader;
    PyObject *pyobj_reader;

    if (!PyArg_ParseTuple(args, (char *)"O:xmlTextReaderName", &pyobj_reader))
        return(NULL);
    reader = (xmlTextReaderPtr) PyxmlTextReader_Get(pyobj_reader);

    c_retval = xmlTextReaderName(reader);
    py_retval = libxml_xmlCharPtrWrap((xmlChar *) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlTextReaderIsEmptyElement(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    int c_retval;
    xmlTextReaderPtr reader;
    PyObject *pyobj_reader;

    if (!PyArg_ParseTuple(args, (char *)"O:xmlTextReaderIsEmptyElement", &pyobj_reader))
        return(NULL);
    reader = (xmlTextReaderPtr) PyxmlTextReader_Get(pyobj_reader);

    c_retval = xmlTextReaderIsEmptyElement(reader);
    py_retval = libxml_intWrap((int) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlCheckUTF8(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    int c_retval;
    unsigned char * utf;

    if (!PyArg_ParseTuple(args, (char *)"z:xmlCheckUTF8", &utf))
        return(NULL);

    c_retval = xmlCheckUTF8(utf);
    py_retval = libxml_intWrap((int) c_retval);
    return(py_retval);
}

#ifdef LIBXML_XPATH_ENABLED
PyObject *
libxml_xmlXPathNotFunction(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    xmlXPathParserContextPtr ctxt;
    PyObject *pyobj_ctxt;
    int nargs;

    if (!PyArg_ParseTuple(args, (char *)"Oi:xmlXPathNotFunction", &pyobj_ctxt, &nargs))
        return(NULL);
    ctxt = (xmlXPathParserContextPtr) PyxmlXPathParserContext_Get(pyobj_ctxt);

    xmlXPathNotFunction(ctxt, nargs);
    Py_INCREF(Py_None);
    return(Py_None);
}

#endif /* LIBXML_XPATH_ENABLED */
#ifdef LIBXML_XPATH_ENABLED
PyObject *
libxml_xmlXPathRegisteredVariablesCleanup(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    xmlXPathContextPtr ctxt;
    PyObject *pyobj_ctxt;

    if (!PyArg_ParseTuple(args, (char *)"O:xmlXPathRegisteredVariablesCleanup", &pyobj_ctxt))
        return(NULL);
    ctxt = (xmlXPathContextPtr) PyxmlXPathContext_Get(pyobj_ctxt);

    xmlXPathRegisteredVariablesCleanup(ctxt);
    Py_INCREF(Py_None);
    return(Py_None);
}

#endif /* LIBXML_XPATH_ENABLED */
PyObject *
libxml_xmlUCSIsCatPf(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    int c_retval;
    int code;

    if (!PyArg_ParseTuple(args, (char *)"i:xmlUCSIsCatPf", &code))
        return(NULL);

    c_retval = xmlUCSIsCatPf(code);
    py_retval = libxml_intWrap((int) c_retval);
    return(py_retval);
}

#ifdef LIBXML_XPATH_ENABLED
PyObject *
libxml_xmlXPathLastFunction(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    xmlXPathParserContextPtr ctxt;
    PyObject *pyobj_ctxt;
    int nargs;

    if (!PyArg_ParseTuple(args, (char *)"Oi:xmlXPathLastFunction", &pyobj_ctxt, &nargs))
        return(NULL);
    ctxt = (xmlXPathParserContextPtr) PyxmlXPathParserContext_Get(pyobj_ctxt);

    xmlXPathLastFunction(ctxt, nargs);
    Py_INCREF(Py_None);
    return(Py_None);
}

#endif /* LIBXML_XPATH_ENABLED */
PyObject *
libxml_xmlUCSIsCatPo(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    int c_retval;
    int code;

    if (!PyArg_ParseTuple(args, (char *)"i:xmlUCSIsCatPo", &code))
        return(NULL);

    c_retval = xmlUCSIsCatPo(code);
    py_retval = libxml_intWrap((int) c_retval);
    return(py_retval);
}

#ifdef LIBXML_XPATH_ENABLED
PyObject *
libxml_xmlXPathNextChild(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    xmlNodePtr c_retval;
    xmlXPathParserContextPtr ctxt;
    PyObject *pyobj_ctxt;
    xmlNodePtr cur;
    PyObject *pyobj_cur;

    if (!PyArg_ParseTuple(args, (char *)"OO:xmlXPathNextChild", &pyobj_ctxt, &pyobj_cur))
        return(NULL);
    ctxt = (xmlXPathParserContextPtr) PyxmlXPathParserContext_Get(pyobj_ctxt);
    cur = (xmlNodePtr) PyxmlNode_Get(pyobj_cur);

    c_retval = xmlXPathNextChild(ctxt, cur);
    py_retval = libxml_xmlNodePtrWrap((xmlNodePtr) c_retval);
    return(py_retval);
}

#endif /* LIBXML_XPATH_ENABLED */
PyObject *
libxml_xmlUCSIsCatPs(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    int c_retval;
    int code;

    if (!PyArg_ParseTuple(args, (char *)"i:xmlUCSIsCatPs", &code))
        return(NULL);

    c_retval = xmlUCSIsCatPs(code);
    py_retval = libxml_intWrap((int) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlUCSIsHighSurrogates(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    int c_retval;
    int code;

    if (!PyArg_ParseTuple(args, (char *)"i:xmlUCSIsHighSurrogates", &code))
        return(NULL);

    c_retval = xmlUCSIsHighSurrogates(code);
    py_retval = libxml_intWrap((int) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlNodeGetSpacePreserve(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    int c_retval;
    xmlNodePtr cur;
    PyObject *pyobj_cur;

    if (!PyArg_ParseTuple(args, (char *)"O:xmlNodeGetSpacePreserve", &pyobj_cur))
        return(NULL);
    cur = (xmlNodePtr) PyxmlNode_Get(pyobj_cur);

    c_retval = xmlNodeGetSpacePreserve(cur);
    py_retval = libxml_intWrap((int) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlCatalogResolveURI(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    xmlChar * c_retval;
    xmlChar * URI;

    if (!PyArg_ParseTuple(args, (char *)"z:xmlCatalogResolveURI", &URI))
        return(NULL);

    c_retval = xmlCatalogResolveURI(URI);
    py_retval = libxml_xmlCharPtrWrap((xmlChar *) c_retval);
    return(py_retval);
}

#ifdef LIBXML_XPATH_ENABLED
PyObject *
libxml_xmlXPathPopNumber(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    double c_retval;
    xmlXPathParserContextPtr ctxt;
    PyObject *pyobj_ctxt;

    if (!PyArg_ParseTuple(args, (char *)"O:xmlXPathPopNumber", &pyobj_ctxt))
        return(NULL);
    ctxt = (xmlXPathParserContextPtr) PyxmlXPathParserContext_Get(pyobj_ctxt);

    c_retval = xmlXPathPopNumber(ctxt);
    py_retval = libxml_doubleWrap((double) c_retval);
    return(py_retval);
}

#endif /* LIBXML_XPATH_ENABLED */
PyObject *
libxml_xmlUCSIsKhmer(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    int c_retval;
    int code;

    if (!PyArg_ParseTuple(args, (char *)"i:xmlUCSIsKhmer", &code))
        return(NULL);

    c_retval = xmlUCSIsKhmer(code);
    py_retval = libxml_intWrap((int) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlUCSIsCatLt(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    int c_retval;
    int code;

    if (!PyArg_ParseTuple(args, (char *)"i:xmlUCSIsCatLt", &code))
        return(NULL);

    c_retval = xmlUCSIsCatLt(code);
    py_retval = libxml_intWrap((int) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlIsBlank(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    int c_retval;
    int c;

    if (!PyArg_ParseTuple(args, (char *)"i:xmlIsBlank", &c))
        return(NULL);

    c_retval = xmlIsBlank(c);
    py_retval = libxml_intWrap((int) c_retval);
    return(py_retval);
}

#ifdef LIBXML_XPATH_ENABLED
PyObject *
libxml_xmlXPathIsNaN(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    int c_retval;
    double val;

    if (!PyArg_ParseTuple(args, (char *)"d:xmlXPathIsNaN", &val))
        return(NULL);

    c_retval = xmlXPathIsNaN(val);
    py_retval = libxml_intWrap((int) c_retval);
    return(py_retval);
}

#endif /* LIBXML_XPATH_ENABLED */
#ifdef LIBXML_XPATH_ENABLED
PyObject *
libxml_xmlXPathNotEqualValues(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    int c_retval;
    xmlXPathParserContextPtr ctxt;
    PyObject *pyobj_ctxt;

    if (!PyArg_ParseTuple(args, (char *)"O:xmlXPathNotEqualValues", &pyobj_ctxt))
        return(NULL);
    ctxt = (xmlXPathParserContextPtr) PyxmlXPathParserContext_Get(pyobj_ctxt);

    c_retval = xmlXPathNotEqualValues(ctxt);
    py_retval = libxml_intWrap((int) c_retval);
    return(py_retval);
}

#endif /* LIBXML_XPATH_ENABLED */
PyObject *
libxml_xmlUCSIsEnclosedAlphanumerics(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    int c_retval;
    int code;

    if (!PyArg_ParseTuple(args, (char *)"i:xmlUCSIsEnclosedAlphanumerics", &code))
        return(NULL);

    c_retval = xmlUCSIsEnclosedAlphanumerics(code);
    py_retval = libxml_intWrap((int) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlParseEncName(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    xmlChar * c_retval;
    xmlParserCtxtPtr ctxt;
    PyObject *pyobj_ctxt;

    if (!PyArg_ParseTuple(args, (char *)"O:xmlParseEncName", &pyobj_ctxt))
        return(NULL);
    ctxt = (xmlParserCtxtPtr) PyparserCtxt_Get(pyobj_ctxt);

    c_retval = xmlParseEncName(ctxt);
    py_retval = libxml_xmlCharPtrWrap((xmlChar *) c_retval);
    return(py_retval);
}

#ifdef LIBXML_XPTR_ENABLED
PyObject *
libxml_xmlXPtrRangeToFunction(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    xmlXPathParserContextPtr ctxt;
    PyObject *pyobj_ctxt;
    int nargs;

    if (!PyArg_ParseTuple(args, (char *)"Oi:xmlXPtrRangeToFunction", &pyobj_ctxt, &nargs))
        return(NULL);
    ctxt = (xmlXPathParserContextPtr) PyxmlXPathParserContext_Get(pyobj_ctxt);

    xmlXPtrRangeToFunction(ctxt, nargs);
    Py_INCREF(Py_None);
    return(Py_None);
}

#endif /* LIBXML_XPTR_ENABLED */
PyObject *
libxml_xmlFreeDtd(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    xmlDtdPtr cur;
    PyObject *pyobj_cur;

    if (!PyArg_ParseTuple(args, (char *)"O:xmlFreeDtd", &pyobj_cur))
        return(NULL);
    cur = (xmlDtdPtr) PyxmlNode_Get(pyobj_cur);

    xmlFreeDtd(cur);
    Py_INCREF(Py_None);
    return(Py_None);
}

#ifdef LIBXML_XPATH_ENABLED
PyObject *
libxml_xmlXPathCastBooleanToNumber(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    double c_retval;
    int val;

    if (!PyArg_ParseTuple(args, (char *)"i:xmlXPathCastBooleanToNumber", &val))
        return(NULL);

    c_retval = xmlXPathCastBooleanToNumber(val);
    py_retval = libxml_doubleWrap((double) c_retval);
    return(py_retval);
}

#endif /* LIBXML_XPATH_ENABLED */
#ifdef LIBXML_XPTR_ENABLED
PyObject *
libxml_xmlXPtrEvalRangePredicate(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    xmlXPathParserContextPtr ctxt;
    PyObject *pyobj_ctxt;

    if (!PyArg_ParseTuple(args, (char *)"O:xmlXPtrEvalRangePredicate", &pyobj_ctxt))
        return(NULL);
    ctxt = (xmlXPathParserContextPtr) PyxmlXPathParserContext_Get(pyobj_ctxt);

    xmlXPtrEvalRangePredicate(ctxt);
    Py_INCREF(Py_None);
    return(Py_None);
}

#endif /* LIBXML_XPTR_ENABLED */
#ifdef LIBXML_HTML_ENABLED
PyObject *
libxml_htmlAutoCloseTag(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    int c_retval;
    htmlDocPtr doc;
    PyObject *pyobj_doc;
    xmlChar * name;
    htmlNodePtr elem;
    PyObject *pyobj_elem;

    if (!PyArg_ParseTuple(args, (char *)"OzO:htmlAutoCloseTag", &pyobj_doc, &name, &pyobj_elem))
        return(NULL);
    doc = (htmlDocPtr) PyxmlNode_Get(pyobj_doc);
    elem = (htmlNodePtr) PyxmlNode_Get(pyobj_elem);

    c_retval = htmlAutoCloseTag(doc, name, elem);
    py_retval = libxml_intWrap((int) c_retval);
    return(py_retval);
}

#endif /* LIBXML_HTML_ENABLED */
PyObject *
libxml_xmlUCSIsIdeographicDescriptionCharacters(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    int c_retval;
    int code;

    if (!PyArg_ParseTuple(args, (char *)"i:xmlUCSIsIdeographicDescriptionCharacters", &code))
        return(NULL);

    c_retval = xmlUCSIsIdeographicDescriptionCharacters(code);
    py_retval = libxml_intWrap((int) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlUCSIsCatLo(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    int c_retval;
    int code;

    if (!PyArg_ParseTuple(args, (char *)"i:xmlUCSIsCatLo", &code))
        return(NULL);

    c_retval = xmlUCSIsCatLo(code);
    py_retval = libxml_intWrap((int) c_retval);
    return(py_retval);
}

#ifdef LIBXML_SCHEMAS_ENABLED
PyObject *
libxml_xmlRelaxNGNewMemParserCtxt(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    xmlRelaxNGParserCtxtPtr c_retval;
    char * buffer;
    int size;

    if (!PyArg_ParseTuple(args, (char *)"zi:xmlRelaxNGNewMemParserCtxt", &buffer, &size))
        return(NULL);

    c_retval = xmlRelaxNGNewMemParserCtxt(buffer, size);
    py_retval = libxml_xmlRelaxNGParserCtxtPtrWrap((xmlRelaxNGParserCtxtPtr) c_retval);
    return(py_retval);
}

#endif /* LIBXML_SCHEMAS_ENABLED */
PyObject *
libxml_xmlOutputBufferClose(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    int c_retval;
    xmlOutputBufferPtr out;
    PyObject *pyobj_out;

    if (!PyArg_ParseTuple(args, (char *)"O:xmlOutputBufferClose", &pyobj_out))
        return(NULL);
    out = (xmlOutputBufferPtr) PyoutputBuffer_Get(pyobj_out);

    c_retval = xmlOutputBufferClose(out);
    py_retval = libxml_intWrap((int) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlTextReaderAttributeCount(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    int c_retval;
    xmlTextReaderPtr reader;
    PyObject *pyobj_reader;

    if (!PyArg_ParseTuple(args, (char *)"O:xmlTextReaderAttributeCount", &pyobj_reader))
        return(NULL);
    reader = (xmlTextReaderPtr) PyxmlTextReader_Get(pyobj_reader);

    c_retval = xmlTextReaderAttributeCount(reader);
    py_retval = libxml_intWrap((int) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlCharStrndup(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    xmlChar * c_retval;
    char * cur;
    int len;

    if (!PyArg_ParseTuple(args, (char *)"zi:xmlCharStrndup", &cur, &len))
        return(NULL);

    c_retval = xmlCharStrndup(cur, len);
    py_retval = libxml_xmlCharPtrWrap((xmlChar *) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlUCSIsYiSyllables(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    int c_retval;
    int code;

    if (!PyArg_ParseTuple(args, (char *)"i:xmlUCSIsYiSyllables", &code))
        return(NULL);

    c_retval = xmlUCSIsYiSyllables(code);
    py_retval = libxml_intWrap((int) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlGetLineNo(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    long c_retval;
    xmlNodePtr node;
    PyObject *pyobj_node;

    if (!PyArg_ParseTuple(args, (char *)"O:xmlGetLineNo", &pyobj_node))
        return(NULL);
    node = (xmlNodePtr) PyxmlNode_Get(pyobj_node);

    c_retval = xmlGetLineNo(node);
    py_retval = libxml_intWrap((int) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlParseEncodingDecl(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    xmlChar * c_retval;
    xmlParserCtxtPtr ctxt;
    PyObject *pyobj_ctxt;

    if (!PyArg_ParseTuple(args, (char *)"O:xmlParseEncodingDecl", &pyobj_ctxt))
        return(NULL);
    ctxt = (xmlParserCtxtPtr) PyparserCtxt_Get(pyobj_ctxt);

    c_retval = xmlParseEncodingDecl(ctxt);
    py_retval = libxml_xmlCharPtrWrap((xmlChar *) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlNodeGetLang(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    xmlChar * c_retval;
    xmlNodePtr cur;
    PyObject *pyobj_cur;

    if (!PyArg_ParseTuple(args, (char *)"O:xmlNodeGetLang", &pyobj_cur))
        return(NULL);
    cur = (xmlNodePtr) PyxmlNode_Get(pyobj_cur);

    c_retval = xmlNodeGetLang(cur);
    py_retval = libxml_xmlCharPtrWrap((xmlChar *) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlGetDocCompressMode(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    int c_retval;
    xmlDocPtr doc;
    PyObject *pyobj_doc;

    if (!PyArg_ParseTuple(args, (char *)"O:xmlGetDocCompressMode", &pyobj_doc))
        return(NULL);
    doc = (xmlDocPtr) PyxmlNode_Get(pyobj_doc);

    c_retval = xmlGetDocCompressMode(doc);
    py_retval = libxml_intWrap((int) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlUCSIsPrivateUse(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    int c_retval;
    int code;

    if (!PyArg_ParseTuple(args, (char *)"i:xmlUCSIsPrivateUse", &code))
        return(NULL);

    c_retval = xmlUCSIsPrivateUse(code);
    py_retval = libxml_intWrap((int) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlNewParserCtxt(ATTRIBUTE_UNUSED PyObject *self,ATTRIBUTE_UNUSED  PyObject *args) {
    PyObject *py_retval;
    xmlParserCtxtPtr c_retval;

    c_retval = xmlNewParserCtxt();
    py_retval = libxml_xmlParserCtxtPtrWrap((xmlParserCtxtPtr) c_retval);
    return(py_retval);
}

#ifdef LIBXML_DEBUG_ENABLED
PyObject *
libxml_xmlDebugDumpDocumentHead(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    FILE * output;
    PyObject *pyobj_output;
    xmlDocPtr doc;
    PyObject *pyobj_doc;

    if (!PyArg_ParseTuple(args, (char *)"OO:xmlDebugDumpDocumentHead", &pyobj_output, &pyobj_doc))
        return(NULL);
    output = (FILE *) PyFile_Get(pyobj_output);
    doc = (xmlDocPtr) PyxmlNode_Get(pyobj_doc);

    xmlDebugDumpDocumentHead(output, doc);
    Py_INCREF(Py_None);
    return(Py_None);
}

#endif /* LIBXML_DEBUG_ENABLED */
PyObject *
libxml_xmlNanoFTPScanProxy(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    char * URL;

    if (!PyArg_ParseTuple(args, (char *)"z:xmlNanoFTPScanProxy", &URL))
        return(NULL);

    xmlNanoFTPScanProxy(URL);
    Py_INCREF(Py_None);
    return(Py_None);
}

PyObject *
libxml_xmlUnlinkNode(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    xmlNodePtr cur;
    PyObject *pyobj_cur;

    if (!PyArg_ParseTuple(args, (char *)"O:xmlUnlinkNode", &pyobj_cur))
        return(NULL);
    cur = (xmlNodePtr) PyxmlNode_Get(pyobj_cur);

    xmlUnlinkNode(cur);
    Py_INCREF(Py_None);
    return(Py_None);
}

PyObject *
libxml_xmlValidateNmtokensValue(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    int c_retval;
    xmlChar * value;

    if (!PyArg_ParseTuple(args, (char *)"z:xmlValidateNmtokensValue", &value))
        return(NULL);

    c_retval = xmlValidateNmtokensValue(value);
    py_retval = libxml_intWrap((int) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlCreateEntityParserCtxt(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    xmlParserCtxtPtr c_retval;
    xmlChar * URL;
    xmlChar * ID;
    xmlChar * base;

    if (!PyArg_ParseTuple(args, (char *)"zzz:xmlCreateEntityParserCtxt", &URL, &ID, &base))
        return(NULL);

    c_retval = xmlCreateEntityParserCtxt(URL, ID, base);
    py_retval = libxml_xmlParserCtxtPtrWrap((xmlParserCtxtPtr) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlTextReaderPrefix(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    xmlChar * c_retval;
    xmlTextReaderPtr reader;
    PyObject *pyobj_reader;

    if (!PyArg_ParseTuple(args, (char *)"O:xmlTextReaderPrefix", &pyobj_reader))
        return(NULL);
    reader = (xmlTextReaderPtr) PyxmlTextReader_Get(pyobj_reader);

    c_retval = xmlTextReaderPrefix(reader);
    py_retval = libxml_xmlCharPtrWrap((xmlChar *) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlUCSIsUnifiedCanadianAboriginalSyllabics(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    int c_retval;
    int code;

    if (!PyArg_ParseTuple(args, (char *)"i:xmlUCSIsUnifiedCanadianAboriginalSyllabics", &code))
        return(NULL);

    c_retval = xmlUCSIsUnifiedCanadianAboriginalSyllabics(code);
    py_retval = libxml_intWrap((int) c_retval);
    return(py_retval);
}

#ifdef LIBXML_HTML_ENABLED
PyObject *
libxml_htmlNodeDumpOutput(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    xmlOutputBufferPtr buf;
    PyObject *pyobj_buf;
    xmlDocPtr doc;
    PyObject *pyobj_doc;
    xmlNodePtr cur;
    PyObject *pyobj_cur;
    char * encoding;

    if (!PyArg_ParseTuple(args, (char *)"OOOz:htmlNodeDumpOutput", &pyobj_buf, &pyobj_doc, &pyobj_cur, &encoding))
        return(NULL);
    buf = (xmlOutputBufferPtr) PyoutputBuffer_Get(pyobj_buf);
    doc = (xmlDocPtr) PyxmlNode_Get(pyobj_doc);
    cur = (xmlNodePtr) PyxmlNode_Get(pyobj_cur);

    htmlNodeDumpOutput(buf, doc, cur, encoding);
    Py_INCREF(Py_None);
    return(Py_None);
}

#endif /* LIBXML_HTML_ENABLED */
PyObject *
libxml_xmlClearParserCtxt(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    xmlParserCtxtPtr ctxt;
    PyObject *pyobj_ctxt;

    if (!PyArg_ParseTuple(args, (char *)"O:xmlClearParserCtxt", &pyobj_ctxt))
        return(NULL);
    ctxt = (xmlParserCtxtPtr) PyparserCtxt_Get(pyobj_ctxt);

    xmlClearParserCtxt(ctxt);
    Py_INCREF(Py_None);
    return(Py_None);
}

PyObject *
libxml_xmlTextReaderReadString(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    xmlChar * c_retval;
    xmlTextReaderPtr reader;
    PyObject *pyobj_reader;

    if (!PyArg_ParseTuple(args, (char *)"O:xmlTextReaderReadString", &pyobj_reader))
        return(NULL);
    reader = (xmlTextReaderPtr) PyxmlTextReader_Get(pyobj_reader);

    c_retval = xmlTextReaderReadString(reader);
    py_retval = libxml_xmlCharPtrWrap((xmlChar *) c_retval);
    return(py_retval);
}

#ifdef LIBXML_HTML_ENABLED
PyObject *
libxml_htmlParseElement(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    htmlParserCtxtPtr ctxt;
    PyObject *pyobj_ctxt;

    if (!PyArg_ParseTuple(args, (char *)"O:htmlParseElement", &pyobj_ctxt))
        return(NULL);
    ctxt = (htmlParserCtxtPtr) PyparserCtxt_Get(pyobj_ctxt);

    htmlParseElement(ctxt);
    Py_INCREF(Py_None);
    return(Py_None);
}

#endif /* LIBXML_HTML_ENABLED */
#ifdef LIBXML_DEBUG_ENABLED
PyObject *
libxml_xmlDebugDumpDocument(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    FILE * output;
    PyObject *pyobj_output;
    xmlDocPtr doc;
    PyObject *pyobj_doc;

    if (!PyArg_ParseTuple(args, (char *)"OO:xmlDebugDumpDocument", &pyobj_output, &pyobj_doc))
        return(NULL);
    output = (FILE *) PyFile_Get(pyobj_output);
    doc = (xmlDocPtr) PyxmlNode_Get(pyobj_doc);

    xmlDebugDumpDocument(output, doc);
    Py_INCREF(Py_None);
    return(Py_None);
}

#endif /* LIBXML_DEBUG_ENABLED */
PyObject *
libxml_xmlUCSIsGreek(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    int c_retval;
    int code;

    if (!PyArg_ParseTuple(args, (char *)"i:xmlUCSIsGreek", &code))
        return(NULL);

    c_retval = xmlUCSIsGreek(code);
    py_retval = libxml_intWrap((int) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlDocFormatDump(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    int c_retval;
    FILE * f;
    PyObject *pyobj_f;
    xmlDocPtr cur;
    PyObject *pyobj_cur;
    int format;

    if (!PyArg_ParseTuple(args, (char *)"OOi:xmlDocFormatDump", &pyobj_f, &pyobj_cur, &format))
        return(NULL);
    f = (FILE *) PyFile_Get(pyobj_f);
    cur = (xmlDocPtr) PyxmlNode_Get(pyobj_cur);

    c_retval = xmlDocFormatDump(f, cur, format);
    py_retval = libxml_intWrap((int) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlTextReaderGetAttribute(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    xmlChar * c_retval;
    xmlTextReaderPtr reader;
    PyObject *pyobj_reader;
    xmlChar * name;

    if (!PyArg_ParseTuple(args, (char *)"Oz:xmlTextReaderGetAttribute", &pyobj_reader, &name))
        return(NULL);
    reader = (xmlTextReaderPtr) PyxmlTextReader_Get(pyobj_reader);

    c_retval = xmlTextReaderGetAttribute(reader, name);
    py_retval = libxml_xmlCharPtrWrap((xmlChar *) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlNodeSetContentLen(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    xmlNodePtr cur;
    PyObject *pyobj_cur;
    xmlChar * content;
    int len;

    if (!PyArg_ParseTuple(args, (char *)"Ozi:xmlNodeSetContentLen", &pyobj_cur, &content, &len))
        return(NULL);
    cur = (xmlNodePtr) PyxmlNode_Get(pyobj_cur);

    xmlNodeSetContentLen(cur, content, len);
    Py_INCREF(Py_None);
    return(Py_None);
}

#ifdef LIBXML_HTML_ENABLED
PyObject *
libxml_htmlSetMetaEncoding(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    int c_retval;
    htmlDocPtr doc;
    PyObject *pyobj_doc;
    xmlChar * encoding;

    if (!PyArg_ParseTuple(args, (char *)"Oz:htmlSetMetaEncoding", &pyobj_doc, &encoding))
        return(NULL);
    doc = (htmlDocPtr) PyxmlNode_Get(pyobj_doc);

    c_retval = htmlSetMetaEncoding(doc, encoding);
    py_retval = libxml_intWrap((int) c_retval);
    return(py_retval);
}

#endif /* LIBXML_HTML_ENABLED */
PyObject *
libxml_xmlIsRef(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    int c_retval;
    xmlDocPtr doc;
    PyObject *pyobj_doc;
    xmlNodePtr elem;
    PyObject *pyobj_elem;
    xmlAttrPtr attr;
    PyObject *pyobj_attr;

    if (!PyArg_ParseTuple(args, (char *)"OOO:xmlIsRef", &pyobj_doc, &pyobj_elem, &pyobj_attr))
        return(NULL);
    doc = (xmlDocPtr) PyxmlNode_Get(pyobj_doc);
    elem = (xmlNodePtr) PyxmlNode_Get(pyobj_elem);
    attr = (xmlAttrPtr) PyxmlNode_Get(pyobj_attr);

    c_retval = xmlIsRef(doc, elem, attr);
    py_retval = libxml_intWrap((int) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlPopInput(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    xmlChar c_retval;
    xmlParserCtxtPtr ctxt;
    PyObject *pyobj_ctxt;

    if (!PyArg_ParseTuple(args, (char *)"O:xmlPopInput", &pyobj_ctxt))
        return(NULL);
    ctxt = (xmlParserCtxtPtr) PyparserCtxt_Get(pyobj_ctxt);

    c_retval = xmlPopInput(ctxt);
    py_retval = libxml_intWrap((int) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlXPathGetContextDoc(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    xmlDocPtr c_retval;
    xmlXPathContextPtr ctxt;
    PyObject *pyobj_ctxt;

    if (!PyArg_ParseTuple(args, (char *)"O:xmlXPathGetContextDoc", &pyobj_ctxt))
        return(NULL);
    ctxt = (xmlXPathContextPtr) PyxmlXPathContext_Get(pyobj_ctxt);

    c_retval = ctxt->doc;
    py_retval = libxml_xmlDocPtrWrap((xmlDocPtr) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlTextReaderCurrentNode(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    xmlNodePtr c_retval;
    xmlTextReaderPtr reader;
    PyObject *pyobj_reader;

    if (!PyArg_ParseTuple(args, (char *)"O:xmlTextReaderCurrentNode", &pyobj_reader))
        return(NULL);
    reader = (xmlTextReaderPtr) PyxmlTextReader_Get(pyobj_reader);

    c_retval = xmlTextReaderCurrentNode(reader);
    py_retval = libxml_xmlNodePtrWrap((xmlNodePtr) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlNewDoc(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    xmlDocPtr c_retval;
    xmlChar * version;

    if (!PyArg_ParseTuple(args, (char *)"z:xmlNewDoc", &version))
        return(NULL);

    c_retval = xmlNewDoc(version);
    py_retval = libxml_xmlDocPtrWrap((xmlDocPtr) c_retval);
    return(py_retval);
}

#ifdef LIBXML_SCHEMAS_ENABLED
PyObject *
libxml_xmlRelaxNGFreeValidCtxt(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    xmlRelaxNGValidCtxtPtr ctxt;
    PyObject *pyobj_ctxt;

    if (!PyArg_ParseTuple(args, (char *)"O:xmlRelaxNGFreeValidCtxt", &pyobj_ctxt))
        return(NULL);
    ctxt = (xmlRelaxNGValidCtxtPtr) PyrelaxNgValidCtxt_Get(pyobj_ctxt);

    xmlRelaxNGFreeValidCtxt(ctxt);
    Py_INCREF(Py_None);
    return(Py_None);
}

#endif /* LIBXML_SCHEMAS_ENABLED */
PyObject *
libxml_xmlDocSetRootElement(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    xmlNodePtr c_retval;
    xmlDocPtr doc;
    PyObject *pyobj_doc;
    xmlNodePtr root;
    PyObject *pyobj_root;

    if (!PyArg_ParseTuple(args, (char *)"OO:xmlDocSetRootElement", &pyobj_doc, &pyobj_root))
        return(NULL);
    doc = (xmlDocPtr) PyxmlNode_Get(pyobj_doc);
    root = (xmlNodePtr) PyxmlNode_Get(pyobj_root);

    c_retval = xmlDocSetRootElement(doc, root);
    py_retval = libxml_xmlNodePtrWrap((xmlNodePtr) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlUCSIsCatZp(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    int c_retval;
    int code;

    if (!PyArg_ParseTuple(args, (char *)"i:xmlUCSIsCatZp", &code))
        return(NULL);

    c_retval = xmlUCSIsCatZp(code);
    py_retval = libxml_intWrap((int) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlUCSIsCatZs(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    int c_retval;
    int code;

    if (!PyArg_ParseTuple(args, (char *)"i:xmlUCSIsCatZs", &code))
        return(NULL);

    c_retval = xmlUCSIsCatZs(code);
    py_retval = libxml_intWrap((int) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlTextReaderGetRemainder(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    xmlParserInputBufferPtr c_retval;
    xmlTextReaderPtr reader;
    PyObject *pyobj_reader;

    if (!PyArg_ParseTuple(args, (char *)"O:xmlTextReaderGetRemainder", &pyobj_reader))
        return(NULL);
    reader = (xmlTextReaderPtr) PyxmlTextReader_Get(pyobj_reader);

    c_retval = xmlTextReaderGetRemainder(reader);
    py_retval = libxml_xmlParserInputBufferPtrWrap((xmlParserInputBufferPtr) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlUCSIsCatZl(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    int c_retval;
    int code;

    if (!PyArg_ParseTuple(args, (char *)"i:xmlUCSIsCatZl", &code))
        return(NULL);

    c_retval = xmlUCSIsCatZl(code);
    py_retval = libxml_intWrap((int) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlUCSIsGujarati(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    int c_retval;
    int code;

    if (!PyArg_ParseTuple(args, (char *)"i:xmlUCSIsGujarati", &code))
        return(NULL);

    c_retval = xmlUCSIsGujarati(code);
    py_retval = libxml_intWrap((int) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlACatalogRemove(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    int c_retval;
    xmlCatalogPtr catal;
    PyObject *pyobj_catal;
    xmlChar * value;

    if (!PyArg_ParseTuple(args, (char *)"Oz:xmlACatalogRemove", &pyobj_catal, &value))
        return(NULL);
    catal = (xmlCatalogPtr) Pycatalog_Get(pyobj_catal);

    c_retval = xmlACatalogRemove(catal, value);
    py_retval = libxml_intWrap((int) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlNewDocComment(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    xmlNodePtr c_retval;
    xmlDocPtr doc;
    PyObject *pyobj_doc;
    xmlChar * content;

    if (!PyArg_ParseTuple(args, (char *)"Oz:xmlNewDocComment", &pyobj_doc, &content))
        return(NULL);
    doc = (xmlDocPtr) PyxmlNode_Get(pyobj_doc);

    c_retval = xmlNewDocComment(doc, content);
    py_retval = libxml_xmlNodePtrWrap((xmlNodePtr) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlNamespaceParseNSDef(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    xmlChar * c_retval;
    xmlParserCtxtPtr ctxt;
    PyObject *pyobj_ctxt;

    if (!PyArg_ParseTuple(args, (char *)"O:xmlNamespaceParseNSDef", &pyobj_ctxt))
        return(NULL);
    ctxt = (xmlParserCtxtPtr) PyparserCtxt_Get(pyobj_ctxt);

    c_retval = xmlNamespaceParseNSDef(ctxt);
    py_retval = libxml_xmlCharPtrWrap((xmlChar *) c_retval);
    return(py_retval);
}

#ifdef LIBXML_XPATH_ENABLED
PyObject *
libxml_xmlXPathCastNumberToString(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    xmlChar * c_retval;
    double val;

    if (!PyArg_ParseTuple(args, (char *)"d:xmlXPathCastNumberToString", &val))
        return(NULL);

    c_retval = xmlXPathCastNumberToString(val);
    py_retval = libxml_xmlCharPtrWrap((xmlChar *) c_retval);
    return(py_retval);
}

#endif /* LIBXML_XPATH_ENABLED */
PyObject *
libxml_xmlFreeNode(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    xmlNodePtr cur;
    PyObject *pyobj_cur;

    if (!PyArg_ParseTuple(args, (char *)"O:xmlFreeNode", &pyobj_cur))
        return(NULL);
    cur = (xmlNodePtr) PyxmlNode_Get(pyobj_cur);

    xmlFreeNode(cur);
    Py_INCREF(Py_None);
    return(Py_None);
}

PyObject *
libxml_xmlParserSetValidate(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    xmlParserCtxtPtr ctxt;
    PyObject *pyobj_ctxt;
    int validate;

    if (!PyArg_ParseTuple(args, (char *)"Oi:xmlParserSetValidate", &pyobj_ctxt, &validate))
        return(NULL);
    ctxt = (xmlParserCtxtPtr) PyparserCtxt_Get(pyobj_ctxt);

    ctxt->validate = validate;
    Py_INCREF(Py_None);
    return(Py_None);
}

PyObject *
libxml_xmlParseComment(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    xmlParserCtxtPtr ctxt;
    PyObject *pyobj_ctxt;

    if (!PyArg_ParseTuple(args, (char *)"O:xmlParseComment", &pyobj_ctxt))
        return(NULL);
    ctxt = (xmlParserCtxtPtr) PyparserCtxt_Get(pyobj_ctxt);

    xmlParseComment(ctxt);
    Py_INCREF(Py_None);
    return(Py_None);
}

PyObject *
libxml_xmlCatalogRemove(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    int c_retval;
    xmlChar * value;

    if (!PyArg_ParseTuple(args, (char *)"z:xmlCatalogRemove", &value))
        return(NULL);

    c_retval = xmlCatalogRemove(value);
    py_retval = libxml_intWrap((int) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlSaveFormatFileEnc(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    int c_retval;
    char * filename;
    xmlDocPtr cur;
    PyObject *pyobj_cur;
    char * encoding;
    int format;

    if (!PyArg_ParseTuple(args, (char *)"zOzi:xmlSaveFormatFileEnc", &filename, &pyobj_cur, &encoding, &format))
        return(NULL);
    cur = (xmlDocPtr) PyxmlNode_Get(pyobj_cur);

    c_retval = xmlSaveFormatFileEnc(filename, cur, encoding, format);
    py_retval = libxml_intWrap((int) c_retval);
    return(py_retval);
}

#ifdef LIBXML_SCHEMAS_ENABLED
PyObject *
libxml_xmlRelaxNGParse(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    xmlRelaxNGPtr c_retval;
    xmlRelaxNGParserCtxtPtr ctxt;
    PyObject *pyobj_ctxt;

    if (!PyArg_ParseTuple(args, (char *)"O:xmlRelaxNGParse", &pyobj_ctxt))
        return(NULL);
    ctxt = (xmlRelaxNGParserCtxtPtr) PyrelaxNgParserCtxt_Get(pyobj_ctxt);

    c_retval = xmlRelaxNGParse(ctxt);
    py_retval = libxml_xmlRelaxNGPtrWrap((xmlRelaxNGPtr) c_retval);
    return(py_retval);
}

#endif /* LIBXML_SCHEMAS_ENABLED */
PyObject *
libxml_xmlIOFTPMatch(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    int c_retval;
    char * filename;

    if (!PyArg_ParseTuple(args, (char *)"z:xmlIOFTPMatch", &filename))
        return(NULL);

    c_retval = xmlIOFTPMatch(filename);
    py_retval = libxml_intWrap((int) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlParseNmtoken(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    xmlChar * c_retval;
    xmlParserCtxtPtr ctxt;
    PyObject *pyobj_ctxt;

    if (!PyArg_ParseTuple(args, (char *)"O:xmlParseNmtoken", &pyobj_ctxt))
        return(NULL);
    ctxt = (xmlParserCtxtPtr) PyparserCtxt_Get(pyobj_ctxt);

    c_retval = xmlParseNmtoken(ctxt);
    py_retval = libxml_xmlCharPtrWrap((xmlChar *) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlParserGetIsValid(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    int c_retval;
    xmlParserCtxtPtr ctxt;
    PyObject *pyobj_ctxt;

    if (!PyArg_ParseTuple(args, (char *)"O:xmlParserGetIsValid", &pyobj_ctxt))
        return(NULL);
    ctxt = (xmlParserCtxtPtr) PyparserCtxt_Get(pyobj_ctxt);

    c_retval = ctxt->valid;
    py_retval = libxml_intWrap((int) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlParseReference(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    xmlParserCtxtPtr ctxt;
    PyObject *pyobj_ctxt;

    if (!PyArg_ParseTuple(args, (char *)"O:xmlParseReference", &pyobj_ctxt))
        return(NULL);
    ctxt = (xmlParserCtxtPtr) PyparserCtxt_Get(pyobj_ctxt);

    xmlParseReference(ctxt);
    Py_INCREF(Py_None);
    return(Py_None);
}

PyObject *
libxml_xmlUCSIsCatMn(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    int c_retval;
    int code;

    if (!PyArg_ParseTuple(args, (char *)"i:xmlUCSIsCatMn", &code))
        return(NULL);

    c_retval = xmlUCSIsCatMn(code);
    py_retval = libxml_intWrap((int) c_retval);
    return(py_retval);
}

#ifdef LIBXML_DEBUG_ENABLED
PyObject *
libxml_xmlDebugDumpDTD(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    FILE * output;
    PyObject *pyobj_output;
    xmlDtdPtr dtd;
    PyObject *pyobj_dtd;

    if (!PyArg_ParseTuple(args, (char *)"OO:xmlDebugDumpDTD", &pyobj_output, &pyobj_dtd))
        return(NULL);
    output = (FILE *) PyFile_Get(pyobj_output);
    dtd = (xmlDtdPtr) PyxmlNode_Get(pyobj_dtd);

    xmlDebugDumpDTD(output, dtd);
    Py_INCREF(Py_None);
    return(Py_None);
}

#endif /* LIBXML_DEBUG_ENABLED */
PyObject *
libxml_xmlRecoverDoc(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    xmlDocPtr c_retval;
    xmlChar * cur;

    if (!PyArg_ParseTuple(args, (char *)"z:xmlRecoverDoc", &cur))
        return(NULL);

    c_retval = xmlRecoverDoc(cur);
    py_retval = libxml_xmlDocPtrWrap((xmlDocPtr) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlNormalizeWindowsPath(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    xmlChar * c_retval;
    xmlChar * path;

    if (!PyArg_ParseTuple(args, (char *)"z:xmlNormalizeWindowsPath", &path))
        return(NULL);

    c_retval = xmlNormalizeWindowsPath(path);
    py_retval = libxml_xmlCharPtrWrap((xmlChar *) c_retval);
    return(py_retval);
}

#ifdef LIBXML_XPTR_ENABLED
PyObject *
libxml_xmlXPtrNewCollapsedRange(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    xmlXPathObjectPtr c_retval;
    xmlNodePtr start;
    PyObject *pyobj_start;

    if (!PyArg_ParseTuple(args, (char *)"O:xmlXPtrNewCollapsedRange", &pyobj_start))
        return(NULL);
    start = (xmlNodePtr) PyxmlNode_Get(pyobj_start);

    c_retval = xmlXPtrNewCollapsedRange(start);
    py_retval = libxml_xmlXPathObjectPtrWrap((xmlXPathObjectPtr) c_retval);
    return(py_retval);
}

#endif /* LIBXML_XPTR_ENABLED */
PyObject *
libxml_xmlCleanupParser(ATTRIBUTE_UNUSED PyObject *self,ATTRIBUTE_UNUSED  PyObject *args) {

    xmlCleanupParser();
    Py_INCREF(Py_None);
    return(Py_None);
}

PyObject *
libxml_xmlCatalogDump(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    FILE * out;
    PyObject *pyobj_out;

    if (!PyArg_ParseTuple(args, (char *)"O:xmlCatalogDump", &pyobj_out))
        return(NULL);
    out = (FILE *) PyFile_Get(pyobj_out);

    xmlCatalogDump(out);
    Py_INCREF(Py_None);
    return(Py_None);
}

PyObject *
libxml_xmlNodeSetLang(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    xmlNodePtr cur;
    PyObject *pyobj_cur;
    xmlChar * lang;

    if (!PyArg_ParseTuple(args, (char *)"Oz:xmlNodeSetLang", &pyobj_cur, &lang))
        return(NULL);
    cur = (xmlNodePtr) PyxmlNode_Get(pyobj_cur);

    xmlNodeSetLang(cur, lang);
    Py_INCREF(Py_None);
    return(Py_None);
}

PyObject *
libxml_xmlParseNamespace(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    xmlParserCtxtPtr ctxt;
    PyObject *pyobj_ctxt;

    if (!PyArg_ParseTuple(args, (char *)"O:xmlParseNamespace", &pyobj_ctxt))
        return(NULL);
    ctxt = (xmlParserCtxtPtr) PyparserCtxt_Get(pyobj_ctxt);

    xmlParseNamespace(ctxt);
    Py_INCREF(Py_None);
    return(Py_None);
}

PyObject *
libxml_xmlFreeDoc(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    xmlDocPtr cur;
    PyObject *pyobj_cur;

    if (!PyArg_ParseTuple(args, (char *)"O:xmlFreeDoc", &pyobj_cur))
        return(NULL);
    cur = (xmlDocPtr) PyxmlNode_Get(pyobj_cur);

    xmlFreeDoc(cur);
    Py_INCREF(Py_None);
    return(Py_None);
}

PyObject *
libxml_xmlUCSIsCatMc(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    int c_retval;
    int code;

    if (!PyArg_ParseTuple(args, (char *)"i:xmlUCSIsCatMc", &code))
        return(NULL);

    c_retval = xmlUCSIsCatMc(code);
    py_retval = libxml_intWrap((int) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlTextReaderLocatorBaseURI(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    xmlChar * c_retval;
    xmlTextReaderLocatorPtr locator;
    PyObject *pyobj_locator;

    if (!PyArg_ParseTuple(args, (char *)"O:xmlTextReaderLocatorBaseURI", &pyobj_locator))
        return(NULL);
    locator = (xmlTextReaderLocatorPtr) PyxmlTextReaderLocator_Get(pyobj_locator);

    c_retval = xmlTextReaderLocatorBaseURI(locator);
    py_retval = libxml_xmlCharPtrWrap((xmlChar *) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlUCSIsCyrillic(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    int c_retval;
    int code;

    if (!PyArg_ParseTuple(args, (char *)"i:xmlUCSIsCyrillic", &code))
        return(NULL);

    c_retval = xmlUCSIsCyrillic(code);
    py_retval = libxml_intWrap((int) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlIsBaseChar(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    int c_retval;
    int c;

    if (!PyArg_ParseTuple(args, (char *)"i:xmlIsBaseChar", &c))
        return(NULL);

    c_retval = xmlIsBaseChar(c);
    py_retval = libxml_intWrap((int) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlUCSIsSyriac(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    int c_retval;
    int code;

    if (!PyArg_ParseTuple(args, (char *)"i:xmlUCSIsSyriac", &code))
        return(NULL);

    c_retval = xmlUCSIsSyriac(code);
    py_retval = libxml_intWrap((int) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlParsePI(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    xmlParserCtxtPtr ctxt;
    PyObject *pyobj_ctxt;

    if (!PyArg_ParseTuple(args, (char *)"O:xmlParsePI", &pyobj_ctxt))
        return(NULL);
    ctxt = (xmlParserCtxtPtr) PyparserCtxt_Get(pyobj_ctxt);

    xmlParsePI(ctxt);
    Py_INCREF(Py_None);
    return(Py_None);
}

PyObject *
libxml_xmlUCSIsSuperscriptsandSubscripts(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    int c_retval;
    int code;

    if (!PyArg_ParseTuple(args, (char *)"i:xmlUCSIsSuperscriptsandSubscripts", &code))
        return(NULL);

    c_retval = xmlUCSIsSuperscriptsandSubscripts(code);
    py_retval = libxml_intWrap((int) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlNewDtd(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    PyObject *py_retval;
    xmlDtdPtr c_retval;
    xmlDocPtr doc;
    PyObject *pyobj_doc;
    xmlChar * name;
    xmlChar * ExternalID;
    xmlChar * SystemID;

    if (!PyArg_ParseTuple(args, (char *)"Ozzz:xmlNewDtd", &pyobj_doc, &name, &ExternalID, &SystemID))
        return(NULL);
    doc = (xmlDocPtr) PyxmlNode_Get(pyobj_doc);

    c_retval = xmlNewDtd(doc, name, ExternalID, SystemID);
    py_retval = libxml_xmlNodePtrWrap((xmlNodePtr) c_retval);
    return(py_retval);
}

PyObject *
libxml_xmlParseDocTypeDecl(ATTRIBUTE_UNUSED PyObject *self, PyObject *args) {
    xmlParserCtxtPtr ctxt;
    PyObject *pyobj_ctxt;

    if (!PyArg_ParseTuple(args, (char *)"O:xmlParseDocTypeDecl", &pyobj_ctxt))
        return(NULL);
    ctxt = (xmlParserCtxtPtr) PyparserCtxt_Get(pyobj_ctxt);

    xmlParseDocTypeDecl(ctxt);
    Py_INCREF(Py_None);
    return(Py_None);
}

