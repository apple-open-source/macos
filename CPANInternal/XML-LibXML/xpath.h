#ifndef __LIBXML_XPATH_H__
#define __LIBXML_XPATH_H__

#include <libxml/tree.h>
#include <libxml/xpath.h>


xmlNodeSetPtr
domXPathSelect( xmlNodePtr refNode, xmlChar * xpathstring );

xmlXPathObjectPtr
domXPathFind( xmlNodePtr refNode, xmlChar * xpathstring );

#endif
