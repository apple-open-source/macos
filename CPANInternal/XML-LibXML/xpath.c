#include <libxml/tree.h>
#include <libxml/xpath.h>
#include <libxml/xpathInternals.h>
#include <libxml/uri.h>

#include "EXTERN.h"

#include "dom.h"

void
perlDocumentFunction(xmlXPathParserContextPtr ctxt, int nargs){
    xmlXPathObjectPtr obj = NULL, obj2 = NULL;
    xmlChar *base = NULL, *URI = NULL;


    if ((nargs < 1) || (nargs > 2)) {
        ctxt->error = XPATH_INVALID_ARITY;
        return;
    }
    if (ctxt->value == NULL) {
        ctxt->error = XPATH_INVALID_TYPE;
        return;
    }

    if (nargs == 2) {
        if (ctxt->value->type != XPATH_NODESET) {
            ctxt->error = XPATH_INVALID_TYPE;
            return;
        }

        obj2 = valuePop(ctxt);
    }


    /* first assure the XML::LibXML error handler is deactivated 
       otherwise strange things might happen
     */

    if (ctxt->value->type == XPATH_NODESET) {
        int i;
        xmlXPathObjectPtr newobj, ret;

        obj = valuePop(ctxt);
        ret = xmlXPathNewNodeSet(NULL);

        if (obj->nodesetval) {
            for (i = 0; i < obj->nodesetval->nodeNr; i++) {
                valuePush(ctxt,
                          xmlXPathNewNodeSet(obj->nodesetval->nodeTab[i]));
                xmlXPathStringFunction(ctxt, 1);
                if (nargs == 2) {
                    valuePush(ctxt, xmlXPathObjectCopy(obj2));
                } else {
                    valuePush(ctxt,
                              xmlXPathNewNodeSet(obj->nodesetval->nodeTab[i]));
                }
                perlDocumentFunction(ctxt, 2);
                newobj = valuePop(ctxt);
                ret->nodesetval = xmlXPathNodeSetMerge(ret->nodesetval,
                                                       newobj->nodesetval);
                xmlXPathFreeObject(newobj);
            }
        }

        xmlXPathFreeObject(obj);
        if (obj2 != NULL)
            xmlXPathFreeObject(obj2);
        valuePush(ctxt, ret);

        /* reset the error old error handler before leaving
         */
        return;
    }
    /*
     * Make sure it's converted to a string
     */
    xmlXPathStringFunction(ctxt, 1);
    if (ctxt->value->type != XPATH_STRING) {
        ctxt->error = XPATH_INVALID_TYPE;
        if (obj2 != NULL)
            xmlXPathFreeObject(obj2);

        /* reset the error old error handler before leaving
         */

        return;
    }
    obj = valuePop(ctxt);
    if (obj->stringval == NULL) {
        valuePush(ctxt, xmlXPathNewNodeSet(NULL));
    } else {
        if ((obj2 != NULL) && (obj2->nodesetval != NULL) &&
            (obj2->nodesetval->nodeNr > 0)) {
            xmlNodePtr target;

            target = obj2->nodesetval->nodeTab[0];
            if (target->type == XML_ATTRIBUTE_NODE) {
                target = ((xmlAttrPtr) target)->parent;
            }
            base = xmlNodeGetBase(target->doc, target);
        } else {
            base = xmlNodeGetBase(ctxt->context->node->doc, ctxt->context->node);
        }
        URI = xmlBuildURI(obj->stringval, base);
        if (base != NULL)
            xmlFree(base);
        if (URI == NULL) {
            valuePush(ctxt, xmlXPathNewNodeSet(NULL));
        } else {
            if (xmlStrEqual(ctxt->context->node->doc->URL, URI)) {
                valuePush(ctxt, xmlXPathNewNodeSet((xmlNodePtr)ctxt->context->node->doc));
            }
            else {
                xmlDocPtr doc;
                doc = xmlParseFile((const char *)URI);
                if (doc == NULL)
                    valuePush(ctxt, xmlXPathNewNodeSet(NULL));
                else {
                    /* TODO: use XPointer of HTML location for fragment ID */
                    /* pbm #xxx can lead to location sets, not nodesets :-) */
                    valuePush(ctxt, xmlXPathNewNodeSet((xmlNodePtr) doc));
                }
            }
            xmlFree(URI);
        }
    }
    xmlXPathFreeObject(obj);
    if (obj2 != NULL)
        xmlXPathFreeObject(obj2);

    /* reset the error old error handler before leaving
     */
}


/**
 * Most of the code is stolen from testXPath. 
 * The almost only thing I added, is the storeing of the data, so
 * we can access the data easily - or say more easiely than through
 * libxml2.
 **/

xmlXPathObjectPtr
domXPathFind( xmlNodePtr refNode, xmlChar * path ) {
    xmlXPathObjectPtr res = NULL;
  
    if ( refNode != NULL && path != NULL ) {
        xmlXPathContextPtr ctxt;
        xmlXPathCompExprPtr comp;

        xmlDocPtr tdoc = NULL;
        xmlNodePtr froot = refNode;

        comp = xmlXPathCompile( path );
        if ( comp == NULL ) {
            return NULL;
        }
        
        if ( refNode->doc == NULL ) {
            /* if one XPaths a node from a fragment, libxml2 will
               refuse the lookup. this is not very usefull for XML
               scripters. thus we need to create a temporary document
               to make libxml2 do it's job correctly.
             */
            tdoc = xmlNewDoc( NULL );

            /* find refnode's root node */
            while ( froot != NULL ) {
                if ( froot->parent == NULL ) {
                    break;
                }
                froot = froot->parent;
            }
            xmlAddChild((xmlNodePtr)tdoc, froot);

            refNode->doc = tdoc;
        }

        /* prepare the xpath context */
        ctxt = xmlXPathNewContext( refNode->doc );
        ctxt->node = refNode;
        /* get the namespace information */
        if (refNode->type == XML_DOCUMENT_NODE) {
            ctxt->namespaces = xmlGetNsList( refNode->doc,
                                             xmlDocGetRootElement( refNode->doc ) );
        }
        else {
            ctxt->namespaces = xmlGetNsList(refNode->doc, refNode);
        }
        ctxt->nsNr = 0;
        if (ctxt->namespaces != NULL) {
            while (ctxt->namespaces[ctxt->nsNr] != NULL)
            ctxt->nsNr++;
        }

        xmlXPathRegisterFunc(ctxt,
                             (const xmlChar *) "document",
                             perlDocumentFunction);
       
        res = xmlXPathCompiledEval(comp, ctxt);

        xmlXPathFreeCompExpr(comp);

        if (ctxt->namespaces != NULL) {
            xmlFree( ctxt->namespaces );
        }

        xmlXPathFreeContext(ctxt);

        if ( tdoc != NULL ) {
            /* after looking through a fragment, we need to drop the
               fake document again */
            xmlSetTreeDoc(froot, NULL);
            tdoc->children = NULL;
            tdoc->last     = NULL;
            /* next line is not required anymore */
            /* refNode->doc = NULL; */

            xmlFreeDoc( tdoc );
        }
    }
    return res;
}

xmlNodeSetPtr
domXPathSelect( xmlNodePtr refNode, xmlChar * path ) {
    xmlNodeSetPtr rv = NULL;
    xmlXPathObjectPtr res = NULL;
  
    res = domXPathFind( refNode, path );
    
    if (res != NULL) {
            /* here we have to transfer the result from the internal
               structure to the return value */
        	/* get the result from the query */
        	/* we have to unbind the nodelist, so free object can 
        	   not kill it */
        rv = res->nodesetval;  
        res->nodesetval = 0 ;
    }

    xmlXPathFreeObject(res);

    return rv;
}

/**
 * Most of the code is stolen from testXPath. 
 * The almost only thing I added, is the storeing of the data, so
 * we can access the data easily - or say more easiely than through
 * libxml2.
 **/

xmlXPathObjectPtr
domXPathFindCtxt( xmlXPathContextPtr ctxt, xmlChar * path ) {
    xmlXPathObjectPtr res = NULL;
  
    if ( ctxt->node != NULL && path != NULL ) {
        xmlXPathCompExprPtr comp;

        xmlDocPtr tdoc = NULL;
        xmlNodePtr froot = ctxt->node;

        comp = xmlXPathCompile( path );
        if ( comp == NULL ) {
            return NULL;
        }
        
        if ( ctxt->node->doc == NULL ) {
            /* if one XPaths a node from a fragment, libxml2 will
               refuse the lookup. this is not very usefull for XML
               scripters. thus we need to create a temporary document
               to make libxml2 do it's job correctly.
             */
            tdoc = xmlNewDoc( NULL );

            /* find refnode's root node */
            while ( froot != NULL ) {
                if ( froot->parent == NULL ) {
                    break;
                }
                froot = froot->parent;
            }
            xmlAddChild((xmlNodePtr)tdoc, froot);

            ctxt->node->doc = tdoc;
        }
       
        res = xmlXPathCompiledEval(comp, ctxt);

        xmlXPathFreeCompExpr(comp);

        if ( tdoc != NULL ) {
            /* after looking through a fragment, we need to drop the
               fake document again */
	    xmlSetTreeDoc(froot,NULL);
            froot->doc = NULL;
            tdoc->children = NULL;
            tdoc->last     = NULL;
            froot->parent  = NULL;
            ctxt->node->doc = NULL;

            xmlFreeDoc( tdoc );
        }
    }
    return res;
}

xmlNodeSetPtr
domXPathSelectCtxt( xmlXPathContextPtr ctxt, xmlChar * path ) {
    xmlNodeSetPtr rv = NULL;
    xmlXPathObjectPtr res = NULL;
  
    res = domXPathFindCtxt( ctxt, path );
    
    if (res != NULL) {
            /* here we have to transfer the result from the internal
               structure to the return value */
        	/* get the result from the query */
        	/* we have to unbind the nodelist, so free object can 
        	   not kill it */
        rv = res->nodesetval;  
        res->nodesetval = 0 ;
    }

    xmlXPathFreeObject(res);

    return rv;
}
