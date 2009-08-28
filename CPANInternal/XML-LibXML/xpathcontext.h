#ifndef __LIBXML_XPATHCONTEXT_H__
#define __LIBXML_XPATHCONTEXT_H__

/*
 * xpathcontext.h
 * 
 * This file is directly included into LibXML.xs.
 *
 */

struct _XPathContextData {
    SV* node;
    HV* pool;  
    SV* varLookup;
    SV* varData;
};
typedef struct _XPathContextData XPathContextData;
typedef XPathContextData* XPathContextDataPtr;

#define XPathContextDATA(ctxt) ((XPathContextDataPtr) ctxt->user)

#endif
