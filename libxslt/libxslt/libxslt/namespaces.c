/*
 * namespaces.c: Implementation of the XSLT namespaces handling
 *
 * Reference:
 *   http://www.w3.org/TR/1999/REC-xslt-19991116
 *
 * See Copyright for the status of this software.
 *
 * daniel@veillard.com
 */

#define IN_LIBXSLT
#include "libxslt.h"

#include <string.h>

#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_MATH_H
#include <math.h>
#endif
#ifdef HAVE_FLOAT_H
#include <float.h>
#endif
#ifdef HAVE_IEEEFP_H
#include <ieeefp.h>
#endif
#ifdef HAVE_NAN_H
#include <nan.h>
#endif
#ifdef HAVE_CTYPE_H
#include <ctype.h>
#endif

#include <libxml/xmlmemory.h>
#include <libxml/tree.h>
#include <libxml/hash.h>
#include <libxml/xmlerror.h>
#include <libxml/uri.h>
#include "xslt.h"
#include "xsltInternals.h"
#include "xsltutils.h"
#include "namespaces.h"
#include "imports.h"



/************************************************************************
 *									*
 *			Module interfaces				*
 *									*
 ************************************************************************/

/**
 * xsltNamespaceAlias:
 * @style:  the XSLT stylesheet
 * @node:  the xsl:namespace-alias node
 *
 * Read the stylesheet-prefix and result-prefix attributes, register
 * them as well as the corresponding namespace.
 */
void
xsltNamespaceAlias(xsltStylesheetPtr style, xmlNodePtr node) {
    xmlChar *sprefix;
    xmlNsPtr sNs;
    const xmlChar *shref;
    xmlChar *rprefix;
    xmlNsPtr rNs;
    const xmlChar *rhref;

    sprefix = xsltGetNsProp(node, (const xmlChar *)"stylesheet-prefix",
	                   XSLT_NAMESPACE);
    if (sprefix == NULL) {
	xsltTransformError(NULL, style, node,
	    "namespace-alias: stylesheet-prefix attribute missing\n");
	return;
    }
    rprefix = xsltGetNsProp(node, (const xmlChar *)"result-prefix",
	                   XSLT_NAMESPACE);
    if (rprefix == NULL) {
	xsltTransformError(NULL, style, node,
	    "namespace-alias: result-prefix attribute missing\n");
	goto error;
    }
    
    if (xmlStrEqual(sprefix, (const xmlChar *)"#default")) {
        /*
	 * Do we have a default namespace previously declared?
	 */
	sNs = xmlSearchNs(node->doc, node, NULL);
	if (sNs == NULL)
	    shref = NULL;	/* No - set NULL */
	else
	    shref = sNs->href;	/* Yes - set for nsAlias table */
    } else {
	sNs = xmlSearchNs(node->doc, node, sprefix);
 
	if ((sNs == NULL) || (sNs->href == NULL)) {
	    xsltTransformError(NULL, style, node,
	        "namespace-alias: prefix %s not bound to any namespace\n",
					sprefix);
	    goto error;
	} else
	    shref = sNs->href;
    }

    /*
     * When "#default" is used for result, if a default namespace has not
     * been explicitly declared the special value UNDEFINED_DEFAULT_NS is
     * put into the nsAliases table
     */
    if (xmlStrEqual(rprefix, (const xmlChar *)"#default")) {
	rNs = xmlSearchNs(node->doc, node, NULL);
	if (rNs == NULL)
	    rhref = UNDEFINED_DEFAULT_NS;
	else
	    rhref = rNs->href;
    } else {
	rNs = xmlSearchNs(node->doc, node, rprefix);

        if ((rNs == NULL) || (rNs->href == NULL)) {
	    xsltTransformError(NULL, style, node,
	        "namespace-alias: prefix %s not bound to any namespace\n",
					rprefix);
	    goto error;
	} else
	    rhref = rNs->href;
    }
    /*
     * Special case if #default is used for stylesheet and no default has
     * been explicitly declared.  We use style->defaultAlias for this
     */
    if (shref == NULL) {
        if (rNs != NULL)
            style->defaultAlias = rNs->href;
    } else {
        if (style->nsAliases == NULL)
	    style->nsAliases = xmlHashCreate(10);
        if (style->nsAliases == NULL) {
	    xsltTransformError(NULL, style, node,
	        "namespace-alias: cannot create hash table\n");
	    goto error;
        }
        xmlHashAddEntry((xmlHashTablePtr) style->nsAliases,
	            shref, (void *) rhref);
    }

error:
    if (sprefix != NULL)
	xmlFree(sprefix);
    if (rprefix != NULL)
	xmlFree(rprefix);
}

/**
 * xsltNsInScope:
 * @doc:  the document
 * @node:  the current node
 * @ancestor:  the ancestor carrying the namespace
 * @prefix:  the namespace prefix
 *
 * Copy of xmlNsInScope which is not public ...
 * 
 * Returns 1 if true, 0 if false and -1 in case of error.
 */
static int
xsltNsInScope(xmlDocPtr doc ATTRIBUTE_UNUSED, xmlNodePtr node,
             xmlNodePtr ancestor, const xmlChar * prefix)
{
    xmlNsPtr tst;

    while ((node != NULL) && (node != ancestor)) {
        if ((node->type == XML_ENTITY_REF_NODE) ||
            (node->type == XML_ENTITY_NODE) ||
            (node->type == XML_ENTITY_DECL))
            return (-1);
        if (node->type == XML_ELEMENT_NODE) {
            tst = node->nsDef;
            while (tst != NULL) {
                if ((tst->prefix == NULL)
                    && (prefix == NULL))
                    return (0);
                if ((tst->prefix != NULL)
                    && (prefix != NULL)
                    && (xmlStrEqual(tst->prefix, prefix)))
                    return (0);
                tst = tst->next;
            }
        }
        node = node->parent;
    }
    if (node != ancestor)
        return (-1);
    return (1);
}

/**
 * xsltSearchPlainNsByHref:
 * @doc:  the document
 * @node:  the current node
 * @href:  the namespace value
 *
 * Search a Ns aliasing a given URI and without a NULL prefix.
 * Recurse on the parents until it finds
 * the defined namespace or return NULL otherwise.
 * Returns the namespace pointer or NULL.
 */
static xmlNsPtr
xsltSearchPlainNsByHref(xmlDocPtr doc, xmlNodePtr node, const xmlChar * href)
{
    xmlNsPtr cur;
    xmlNodePtr orig = node;

    if ((node == NULL) || (href == NULL))
        return (NULL);

    while (node != NULL) {
        if ((node->type == XML_ENTITY_REF_NODE) ||
            (node->type == XML_ENTITY_NODE) ||
            (node->type == XML_ENTITY_DECL))
            return (NULL);
        if (node->type == XML_ELEMENT_NODE) {
            cur = node->nsDef;
            while (cur != NULL) {
                if ((cur->href != NULL) && (cur->prefix != NULL) &&
		    (href != NULL) && (xmlStrEqual(cur->href, href))) {
		    if (xsltNsInScope(doc, orig, node, cur->href) == 1)
			return (cur);
                }
                cur = cur->next;
            }
            if (orig != node) {
                cur = node->ns;
                if (cur != NULL) {
                    if ((cur->href != NULL) && (cur->prefix != NULL) &&
		        (href != NULL) && (xmlStrEqual(cur->href, href))) {
			if (xsltNsInScope(doc, orig, node, cur->href) == 1)
			    return (cur);
                    }
                }
            }    
        }
        node = node->parent;
    }
    return (NULL);
}

/**
 * xsltGetPlainNamespace:
 * @ctxt:  a transformation context
 * @cur:  the input node
 * @ns:  the namespace
 * @out:  the output node (or its parent)
 *
 * Find the right namespace value for this prefix, if needed create
 * and add a new namespace decalaration on the node
 * Handle namespace aliases and make sure the prefix is not NULL, this
 * is needed for attributes.
 *
 * Returns the namespace node to use or NULL
 */
xmlNsPtr
xsltGetPlainNamespace(xsltTransformContextPtr ctxt, xmlNodePtr cur,
                      xmlNsPtr ns, xmlNodePtr out) {
    xsltStylesheetPtr style;
    xmlNsPtr ret;
    const xmlChar *URI = NULL; /* the replacement URI */

    if ((ctxt == NULL) || (cur == NULL) || (out == NULL) || (ns == NULL))
	return(NULL);

    style = ctxt->style;
    while (style != NULL) {
	if (style->nsAliases != NULL)
	    URI = (const xmlChar *) xmlHashLookup(style->nsAliases, ns->href);
	if (URI != NULL)
	    break;

	style = xsltNextImport(style);
    }

    if (URI == UNDEFINED_DEFAULT_NS) {
        xmlNsPtr dflt;
	dflt = xmlSearchNs(cur->doc, cur, NULL);
        if (dflt == NULL)
	    return NULL;
	else
	    URI = dflt->href;
    }

    if (URI == NULL)
	URI = ns->href;

    if ((out->parent != NULL) &&
	(out->parent->type == XML_ELEMENT_NODE) &&
	(out->parent->ns != NULL) &&
	(out->parent->ns->prefix != NULL) &&
	(xmlStrEqual(out->parent->ns->href, URI)))
	ret = out->parent->ns;
    else {
	if (ns->prefix != NULL) {
	    ret = xmlSearchNs(out->doc, out, ns->prefix);
	    if ((ret == NULL) || (!xmlStrEqual(ret->href, URI)) ||
	        (ret->prefix == NULL)) {
		ret = xsltSearchPlainNsByHref(out->doc, out, URI);
	    }
	} else {
	    ret = xsltSearchPlainNsByHref(out->doc, out, URI);
	}
    }

    if (ret == NULL) {
	if (out->type == XML_ELEMENT_NODE)
	    ret = xmlNewNs(out, URI, ns->prefix);
    }
    return(ret);
}

/**
 * xsltGetSpecialNamespace:
 * @ctxt:  a transformation context
 * @cur:  the input node
 * @URI:  the namespace URI
 * @prefix:  the suggested prefix
 * @out:  the output node (or its parent)
 *
 * Find the right namespace value for this URI, if needed create
 * and add a new namespace decalaration on the node
 *
 * Returns the namespace node to use or NULL
 */
xmlNsPtr
xsltGetSpecialNamespace(xsltTransformContextPtr ctxt, xmlNodePtr cur,
		const xmlChar *URI, const xmlChar *prefix, xmlNodePtr out) {
    xmlNsPtr ret;
    static int prefixno = 1;
    char nprefix[10];

    if ((ctxt == NULL) || (cur == NULL) || (out == NULL) || (URI == NULL))
	return(NULL);

    if ((prefix == NULL) && (URI[0] == 0)) {
	ret = xmlSearchNs(out->doc, out, NULL);
	if (ret != NULL) {
	    ret = xmlNewNs(out, URI, prefix);
	    return(ret);
	}
	return(NULL);
    }

    if ((out->parent != NULL) &&
	(out->parent->type == XML_ELEMENT_NODE) &&
	(out->parent->ns != NULL) &&
	(xmlStrEqual(out->parent->ns->href, URI)))
	ret = out->parent->ns;
    else 
	ret = xmlSearchNsByHref(out->doc, out, URI);

    if ((ret == NULL) || (ret->prefix == NULL)) {
	if (prefix == NULL) {
	    do {
		sprintf(nprefix, "ns%d", prefixno++);
		ret = xmlSearchNs(out->doc, out, (xmlChar *)nprefix);
	    } while (ret != NULL);
	    prefix = (const xmlChar *) &nprefix[0];
	} else if ((ret != NULL) && (ret->prefix == NULL)) {
	    /* found ns but no prefix - search for the prefix */
	    ret = xmlSearchNs(out->doc, out, prefix);
	    if (ret != NULL)
	        return(ret);	/* found it */
	}
	if (out->type == XML_ELEMENT_NODE)
	    ret = xmlNewNs(out, URI, prefix);
    }
    return(ret);
}

/**
 * xsltGetNamespace:
 * @ctxt:  a transformation context
 * @cur:  the input node
 * @ns:  the namespace
 * @out:  the output node (or its parent)
 *
 * Find the right namespace value for this prefix, if needed create
 * and add a new namespace decalaration on the node
 * Handle namespace aliases
 *
 * Returns the namespace node to use or NULL
 */
xmlNsPtr
xsltGetNamespace(xsltTransformContextPtr ctxt, xmlNodePtr cur, xmlNsPtr ns,
	         xmlNodePtr out) {
    xsltStylesheetPtr style;
    xmlNsPtr ret;
    const xmlChar *URI = NULL; /* the replacement URI */

    if ((ctxt == NULL) || (cur == NULL) || (out == NULL) || (ns == NULL))
	return(NULL);

    style = ctxt->style;
    while (style != NULL) {
	if (style->nsAliases != NULL)
	    URI = (const xmlChar *) 
		xmlHashLookup(style->nsAliases, ns->href);
	if (URI != NULL)
	    break;

	style = xsltNextImport(style);
    }

    if (URI == UNDEFINED_DEFAULT_NS) {
        xmlNsPtr dflt;
	dflt = xmlSearchNs(cur->doc, cur, NULL);
	if (dflt != NULL)
	    URI = dflt->href;
	else
	    return NULL;
    } else if (URI == NULL)
	URI = ns->href;

    /*
     * If the parent is an XML_ELEMENT_NODE, and has the "equivalent"
     * namespace as ns (either both default, or both with a prefix
     * with the same href) then return the parent's ns record
     */
    if ((out->parent != NULL) &&
	(out->parent->type == XML_ELEMENT_NODE) &&
	(out->parent->ns != NULL) &&
	(((out->parent->ns->prefix == NULL) && (ns->prefix == NULL)) ||
	 ((out->parent->ns->prefix != NULL) && (ns->prefix != NULL))) &&
	(xmlStrEqual(out->parent->ns->href, URI)))
	ret = out->parent->ns;
    else {
        /*
	 * do a standard namespace search for ns in the output doc
	 */
        ret = xmlSearchNs(out->doc, out, ns->prefix);
	if ((ret != NULL) && (!xmlStrEqual(ret->href, URI)))
	    ret = NULL;

	/*
	 * if the search fails and it's not for the default prefix
	 * do a search by href
	 */
	if ((ret == NULL) && (ns->prefix != NULL))
	    ret = xmlSearchNsByHref(out->doc, out, URI);
	}

    if (ret == NULL) {	/* if no success and an element node, create the ns */
	if (out->type == XML_ELEMENT_NODE)
	    ret = xmlNewNs(out, URI, ns->prefix);
    }
    return(ret);
}

/**
 * xsltCopyNamespaceList:
 * @ctxt:  a transformation context
 * @node:  the target node
 * @cur:  the first namespace
 *
 * Do a copy of an namespace list. If @node is non-NULL the
 * new namespaces are added automatically. This handles namespaces
 * aliases
 *
 * Returns: a new xmlNsPtr, or NULL in case of error.
 */
xmlNsPtr
xsltCopyNamespaceList(xsltTransformContextPtr ctxt, xmlNodePtr node,
	              xmlNsPtr cur) {
    xmlNsPtr ret = NULL, tmp;
    xmlNsPtr p = NULL,q;
    const xmlChar *URI;

    if (cur == NULL)
	return(NULL);
    if (cur->type != XML_NAMESPACE_DECL)
	return(NULL);

    /*
     * One can add namespaces only on element nodes
     */
    if ((node != NULL) && (node->type != XML_ELEMENT_NODE))
	node = NULL;

    while (cur != NULL) {
	if (cur->type != XML_NAMESPACE_DECL)
	    break;

	/*
	 * Avoid duplicating namespace declrations on the tree
	 */
	if (node != NULL) {
	    if ((node->ns != NULL) &&
        	(xmlStrEqual(node->ns->href, cur->href)) &&
        	(xmlStrEqual(node->ns->prefix, cur->prefix))) {
		cur = cur->next;
		continue;
	    }
	    tmp = xmlSearchNs(node->doc, node, cur->prefix);
	    if ((tmp != NULL) && (xmlStrEqual(tmp->href, cur->href))) {
		cur = cur->next;
		continue;
	    }
	}
	
	if (!xmlStrEqual(cur->href, XSLT_NAMESPACE)) {
	    /* TODO apply cascading */
	    URI = (const xmlChar *) xmlHashLookup(ctxt->style->nsAliases,
		                                  cur->href);
	    if (URI == UNDEFINED_DEFAULT_NS)
	        continue;
	    if (URI != NULL) {
		q = xmlNewNs(node, URI, cur->prefix);
	    } else {
		q = xmlNewNs(node, cur->href, cur->prefix);
	    }
	    if (p == NULL) {
		ret = p = q;
	    } else {
		p->next = q;
		p = q;
	    }
	}
	cur = cur->next;
    }
    return(ret);
}

/**
 * xsltCopyNamespace:
 * @ctxt:  a transformation context
 * @node:  the target node
 * @cur:  the namespace node
 *
 * Do a copy of an namespace node. If @node is non-NULL the
 * new namespaces are added automatically. This handles namespaces
 * aliases
 *
 * Returns: a new xmlNsPtr, or NULL in case of error.
 */
xmlNsPtr
xsltCopyNamespace(xsltTransformContextPtr ctxt, xmlNodePtr node,
	          xmlNsPtr cur) {
    xmlNsPtr ret = NULL;
    const xmlChar *URI;

    if (cur == NULL)
	return(NULL);
    if (cur->type != XML_NAMESPACE_DECL)
	return(NULL);

    /*
     * One can add namespaces only on element nodes
     */
    if ((node != NULL) && (node->type != XML_ELEMENT_NODE))
	node = NULL;

    if (!xmlStrEqual(cur->href, XSLT_NAMESPACE)) {
	URI = (const xmlChar *) xmlHashLookup(ctxt->style->nsAliases,
					      cur->href);
	if (URI == UNDEFINED_DEFAULT_NS)
	    return(NULL);
	if (URI != NULL) {
	    ret = xmlNewNs(node, URI, cur->prefix);
	} else {
	    ret = xmlNewNs(node, cur->href, cur->prefix);
	}
    }
    return(ret);
}


/**
 * xsltFreeNamespaceAliasHashes:
 * @style: an XSLT stylesheet
 *
 * Free up the memory used by namespaces aliases
 */
void
xsltFreeNamespaceAliasHashes(xsltStylesheetPtr style) {
    if (style->nsAliases != NULL)
	xmlHashFree((xmlHashTablePtr) style->nsAliases, NULL);
    style->nsAliases = NULL;
}
