/*
 * attributes.c: Implementation of the XSLT attributes handling
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
#include "attributes.h"
#include "namespaces.h"
#include "templates.h"
#include "imports.h"
#include "transform.h"

#define WITH_XSLT_DEBUG_ATTRIBUTES
#ifdef WITH_XSLT_DEBUG
#define WITH_XSLT_DEBUG_ATTRIBUTES
#endif

/*
 * TODO: merge attribute sets from different import precedence.
 *       all this should be precomputed just before the transformation
 *       starts or at first hit with a cache in the context.
 *       The simple way for now would be to not allow redefinition of
 *       attributes once generated in the output tree, possibly costlier.
 */

/*
 * Useful macros
 */

#define IS_BLANK(c) (((c) == 0x20) || ((c) == 0x09) || ((c) == 0xA) ||	\
                     ((c) == 0x0D))

#define IS_BLANK_NODE(n)						\
    (((n)->type == XML_TEXT_NODE) && (xsltIsBlank((n)->content)))


/*
 * The in-memory structure corresponding to an XSLT Attribute in
 * an attribute set
 */


typedef struct _xsltAttrElem xsltAttrElem;
typedef xsltAttrElem *xsltAttrElemPtr;
struct _xsltAttrElem {
    struct _xsltAttrElem *next;/* chained list */
    xmlNodePtr attr;	/* the xsl:attribute definition */
    const xmlChar *set; /* or the attribute set */
    const xmlChar *ns;  /* and its namespace */
};

/************************************************************************
 *									*
 *			XSLT Attribute handling				*
 *									*
 ************************************************************************/

/**
 * xsltNewAttrElem:
 * @attr:  the new xsl:attribute node
 *
 * Create a new XSLT AttrElem
 *
 * Returns the newly allocated xsltAttrElemPtr or NULL in case of error
 */
static xsltAttrElemPtr
xsltNewAttrElem(xmlNodePtr attr) {
    xsltAttrElemPtr cur;

    cur = (xsltAttrElemPtr) xmlMalloc(sizeof(xsltAttrElem));
    if (cur == NULL) {
        xsltGenericError(xsltGenericErrorContext,
		"xsltNewAttrElem : malloc failed\n");
	return(NULL);
    }
    memset(cur, 0, sizeof(xsltAttrElem));
    cur->attr = attr;
    return(cur);
}

/**
 * xsltFreeAttrElem:
 * @attr:  an XSLT AttrElem
 *
 * Free up the memory allocated by @attr
 */
static void
xsltFreeAttrElem(xsltAttrElemPtr attr) {
    xmlFree(attr);
}

/**
 * xsltFreeAttrElemList:
 * @list:  an XSLT AttrElem list
 *
 * Free up the memory allocated by @list
 */
static void
xsltFreeAttrElemList(xsltAttrElemPtr list) {
    xsltAttrElemPtr next;
    
    while (list != NULL) {
	next = list->next;
	xsltFreeAttrElem(list);
	list = next;
    }
}

/**
 * xsltAddAttrElemList:
 * @list:  an XSLT AttrElem list
 * @attr:  the new xsl:attribute node
 *
 * Add the new attribute to the list.
 *
 * Returns the new list pointer
 */
static xsltAttrElemPtr
xsltAddAttrElemList(xsltAttrElemPtr list, xmlNodePtr attr) {
    xsltAttrElemPtr next, cur;

    if (attr == NULL)
	return(list);
    if (list == NULL)
	return(xsltNewAttrElem(attr));
    cur = list;
    while (cur != NULL) {
	next = cur->next;
	if (cur->attr == attr)
	    return(cur);
	if (cur->next == NULL) {
	    cur->next = xsltNewAttrElem(attr);
	    return(list);
	}
	cur = next;
    }
    return(list);
}

/**
 * xsltMergeAttrElemList:
 * @list:  an XSLT AttrElem list
 * @old:  another XSLT AttrElem list
 *
 * Add all the attributes from list @old to list @list,
 * but drop redefinition of existing values.
 *
 * Returns the new list pointer
 */
static xsltAttrElemPtr
xsltMergeAttrElemList(xsltAttrElemPtr list, xsltAttrElemPtr old) {
    xsltAttrElemPtr cur;
    int add;

    while (old != NULL) {
	if ((old->attr == NULL) && (old->set == NULL)) {
	    old = old->next;
	    continue;
	}
	/*
	 * Check that the attribute is not yet in the list
	 */
	cur = list;
	add = 1;
	while (cur != NULL) {
	    if ((cur->attr == NULL) && (cur->set == NULL)) {
		if (cur->next == NULL)
		    break;
		cur = cur->next;
		continue;
	    }
	    if ((cur->set != NULL) && (cur->set == old->set)) {
		add = 0;
		break;
	    }
	    if (cur->set != NULL) {
		if (cur->next == NULL)
		    break;
		cur = cur->next;
		continue;
	    }
	    if (old->set != NULL) {
		if (cur->next == NULL)
		    break;
		cur = cur->next;
		continue;
	    }
	    if (cur->attr == old->attr) {
		xsltGenericError(xsltGenericErrorContext,
	     "xsl:attribute-set : use-attribute-sets recursion detected\n");
		return(list);
	    }
	    if (cur->next == NULL)
		break;
            cur = cur->next;
	}

	if (add == 1) {
	    if (cur == NULL) {
		list = xsltNewAttrElem(old->attr);
		if (old->set != NULL) {
		    list->set = xmlStrdup(old->set);
		    if (old->ns != NULL)
			list->ns = xmlStrdup(old->ns);
		}
	    } else if (add) {
		cur->next = xsltNewAttrElem(old->attr);
		if (old->set != NULL) {
		    cur->next->set = xmlStrdup(old->set);
		    if (old->ns != NULL)
			cur->next->ns = xmlStrdup(old->ns);
		}
	    }
	}

	old = old->next;
    }
    return(list);
}

/************************************************************************
 *									*
 *			Module interfaces				*
 *									*
 ************************************************************************/

/**
 * xsltParseStylesheetAttributeSet:
 * @style:  the XSLT stylesheet
 * @cur:  the "attribute-set" element
 *
 * parse an XSLT stylesheet attribute-set element
 */

void
xsltParseStylesheetAttributeSet(xsltStylesheetPtr style, xmlNodePtr cur) {
    const xmlChar *ncname;
    const xmlChar *prefix;
    const xmlChar *attrib, *endattr;
    xmlChar *prop;
    xmlChar *attributes;
    xmlNodePtr list;
    xsltAttrElemPtr values;

    if ((cur == NULL) || (style == NULL))
	return;

    prop = xsltGetNsProp(cur, (const xmlChar *)"name", XSLT_NAMESPACE);
    if (prop == NULL) {
	xsltGenericError(xsltGenericErrorContext,
	     "xsl:attribute-set : name is missing\n");
	return;
    }

    ncname = xsltSplitQName(style->dict, prop, &prefix);
    xmlFree(prop);

    if (style->attributeSets == NULL) {
#ifdef WITH_XSLT_DEBUG_ATTRIBUTES
	xsltGenericDebug(xsltGenericDebugContext,
	    "creating attribute set table\n");
#endif
	style->attributeSets = xmlHashCreate(10);
    }
    if (style->attributeSets == NULL)
	return;

    values = xmlHashLookup2(style->attributeSets, ncname, prefix);

    /*
     * check the children list
     */
    list = cur->children;
    while (list != NULL) {
	if (IS_XSLT_ELEM(list)) {
	    if (!IS_XSLT_NAME(list, "attribute")) {
		xsltGenericError(xsltGenericErrorContext,
		    "xsl:attribute-set : unexpected child xsl:%s\n",
		                 list->name);
	    } else {
#ifdef WITH_XSLT_DEBUG_ATTRIBUTES
		xsltGenericDebug(xsltGenericDebugContext,
		    "add attribute to list %s\n", ncname);
#endif
                values = xsltAddAttrElemList(values, list);
	    }
	} else {
	    xsltGenericError(xsltGenericErrorContext,
		"xsl:attribute-set : unexpected child %s\n", list->name);
	}
	list = list->next;
    }

    /*
     * Check a possible use-attribute-sets definition
     */
    /* TODO check recursion */

    attributes = xsltGetNsProp(cur, (const xmlChar *)"use-attribute-sets",
	                      XSLT_NAMESPACE);
    if (attributes == NULL) {
	goto done;
    }

    attrib = attributes;
    while (*attrib != 0) {
	while (IS_BLANK(*attrib)) attrib++;
	if (*attrib == 0)
	    break;
        endattr = attrib;
	while ((*endattr != 0) && (!IS_BLANK(*endattr))) endattr++;
	attrib = xmlDictLookup(style->dict, attrib, endattr - attrib);
	if (attrib) {
	    const xmlChar *ncname2 = NULL;
	    const xmlChar *prefix2 = NULL;
	    xsltAttrElemPtr values2;

#ifdef WITH_XSLT_DEBUG_ATTRIBUTES
	    xsltGenericDebug(xsltGenericDebugContext,
		"xsl:attribute-set : %s adds use %s\n", ncname, attrib);
#endif
	    ncname2 = xsltSplitQName(style->dict, attrib, &prefix2);
	    values2 = xsltNewAttrElem(NULL);
	    if (values2 != NULL) {
		values2->set = ncname2;
		values2->ns = prefix2;
		values = xsltMergeAttrElemList(values, values2);
		xsltFreeAttrElem(values2);
	    }
	}
	attrib = endattr;
    }
    xmlFree(attributes);

done:
    /*
     * Update the value
     */
    if (values == NULL)
	values = xsltNewAttrElem(NULL);
    xmlHashUpdateEntry2(style->attributeSets, ncname, prefix, values, NULL);
#ifdef WITH_XSLT_DEBUG_ATTRIBUTES
    xsltGenericDebug(xsltGenericDebugContext,
	"updated attribute list %s\n", ncname);
#endif
}

/**
 * xsltGetSAS:
 * @style:  the XSLT stylesheet
 * @name:  the attribute list name
 * @ns:  the attribute list namespace
 *
 * lookup an attribute set based on the style cascade
 *
 * Returns the attribute set or NULL
 */
static xsltAttrElemPtr
xsltGetSAS(xsltStylesheetPtr style, const xmlChar *name, const xmlChar *ns) {
    xsltAttrElemPtr values;

    while (style != NULL) {
	values = xmlHashLookup2(style->attributeSets, name, ns);
	if (values != NULL)
	    return(values);
	style = xsltNextImport(style);
    }
    return(NULL);
}

/**
 * xsltResolveSASCallback,:
 * @style:  the XSLT stylesheet
 *
 * resolve the references in an attribute set.
 */
static void
xsltResolveSASCallback(xsltAttrElemPtr values, xsltStylesheetPtr style,
	               const xmlChar *name, const xmlChar *ns,
		       ATTRIBUTE_UNUSED const xmlChar *ignored) {
    xsltAttrElemPtr tmp;
    xsltAttrElemPtr refs;

    tmp = values;
    while (tmp != NULL) {
	if (tmp->set != NULL) {
	    /*
	     * Check against cycles !
	     */
	    if ((xmlStrEqual(name, tmp->set)) && (xmlStrEqual(ns, tmp->ns))) {
		xsltGenericError(xsltGenericErrorContext,
     "xsl:attribute-set : use-attribute-sets recursion detected on %s\n",
                                 name);
	    } else {
#ifdef WITH_XSLT_DEBUG_ATTRIBUTES
		xsltGenericDebug(xsltGenericDebugContext,
			"Importing attribute list %s\n", tmp->set);
#endif

		refs = xsltGetSAS(style, tmp->set, tmp->ns);
		if (refs == NULL) {
		    xsltGenericError(xsltGenericErrorContext,
     "xsl:attribute-set : use-attribute-sets %s reference missing %s\n",
				     name, tmp->set);
		} else {
		    /*
		     * recurse first for cleanup
		     */
		    xsltResolveSASCallback(refs, style, name, ns, NULL);
		    /*
		     * Then merge
		     */
		    xsltMergeAttrElemList(values, refs);
		    /*
		     * Then suppress the reference
		     */
		    xmlFree((char *)tmp->set);
		    tmp->set = NULL;
		    if (tmp->ns != NULL) {
			xmlFree((char *)tmp->ns);
		    }
		}
	    }
	}
	tmp = tmp->next;
    }
}

/**
 * xsltMergeSASCallback,:
 * @style:  the XSLT stylesheet
 *
 * Merge an attribute set from an imported stylesheet.
 */
static void
xsltMergeSASCallback(xsltAttrElemPtr values, xsltStylesheetPtr style,
	               const xmlChar *name, const xmlChar *ns,
		       ATTRIBUTE_UNUSED const xmlChar *ignored) {
    int ret;
    xsltAttrElemPtr topSet;

    ret = xmlHashAddEntry2(style->attributeSets, name, ns, values);
    if (ret < 0) {
	/*
	 * Add failed, this attribute set can be removed.
	 */
#ifdef WITH_XSLT_DEBUG_ATTRIBUTES
	xsltGenericDebug(xsltGenericDebugContext,
		"attribute set %s present already in top stylesheet"
		" - merging\n", name);
#endif
	topSet = xmlHashLookup2(style->attributeSets, name, ns);
	if (topSet==NULL) {
	    xsltGenericError(xsltGenericErrorContext,
	        "xsl:attribute-set : logic error merging from imports for"
		" attribute-set %s\n", name);
	} else {
	    topSet = xsltMergeAttrElemList(topSet, values);
	    xmlHashUpdateEntry2(style->attributeSets, name, ns, topSet, NULL);
	}
	xsltFreeAttrElemList(values);
#ifdef WITH_XSLT_DEBUG_ATTRIBUTES
    } else {
	xsltGenericDebug(xsltGenericDebugContext,
		"attribute set %s moved to top stylesheet\n",
		         name);
#endif
    }
}

/**
 * xsltResolveStylesheetAttributeSet:
 * @style:  the XSLT stylesheet
 *
 * resolve the references between attribute sets.
 */
void
xsltResolveStylesheetAttributeSet(xsltStylesheetPtr style) {
    xsltStylesheetPtr cur;

#ifdef WITH_XSLT_DEBUG_ATTRIBUTES
    xsltGenericDebug(xsltGenericDebugContext,
	    "Resolving attribute sets references\n");
#endif
    /*
     * First aggregate all the attribute sets definitions from the imports
     */
    cur = xsltNextImport(style);
    while (cur != NULL) {
	if (cur->attributeSets != NULL) {
	    if (style->attributeSets == NULL) {
#ifdef WITH_XSLT_DEBUG_ATTRIBUTES
		xsltGenericDebug(xsltGenericDebugContext,
		    "creating attribute set table\n");
#endif
		style->attributeSets = xmlHashCreate(10);
	    }
	    xmlHashScanFull(cur->attributeSets, 
		(xmlHashScannerFull) xsltMergeSASCallback, style);
	    /*
	     * the attribute lists have either been migrated to style
	     * or freed directly in xsltMergeSASCallback()
	     */
	    xmlHashFree(cur->attributeSets, NULL);
	    cur->attributeSets = NULL;
	}
	cur = xsltNextImport(cur);
    }

    /*
     * Then resolve all the references and computes the resulting sets
     */
    if (style->attributeSets != NULL) {
	xmlHashScanFull(style->attributeSets, 
		(xmlHashScannerFull) xsltResolveSASCallback, style);
    }
}

/**
 * xsltAttributeInternal:
 * @ctxt:  a XSLT process context
 * @node:  the node in the source tree.
 * @inst:  the xslt attribute node
 * @comp:  precomputed information
 * @fromset:  the attribute comes from an attribute-set
 *
 * Process the xslt attribute node on the source node
 */
static void
xsltAttributeInternal(xsltTransformContextPtr ctxt, xmlNodePtr node,
                      xmlNodePtr inst, xsltStylePreCompPtr comp,
                      int fromset)
{
    xmlChar *prop = NULL;
    xmlChar *namespace;
    const xmlChar *name = NULL;
    const xmlChar *prefix = NULL;
    xmlChar *value = NULL;
    xmlNsPtr ns = NULL;
    xmlAttrPtr attr;
    const xmlChar *URL = NULL;


    if (ctxt->insert == NULL)
        return;
    if (comp == NULL) {
        xsltTransformError(ctxt, NULL, inst,
                         "xsl:attribute : compilation failed\n");
        return;
    }

    if ((ctxt == NULL) || (node == NULL) || (inst == NULL)
        || (comp == NULL))
        return;
    if (!comp->has_name) {
        return;
    }
    if (ctxt->insert->children != NULL) {
        xsltTransformError(ctxt, NULL, inst,
                         "xsl:attribute : node already has children\n");
        return;
    }
#ifdef WITH_DEBUGGER
    if (ctxt->debugStatus != XSLT_DEBUG_NONE) {
        xslHandleDebugger(inst, node, NULL, ctxt);
    }
#endif

    if (comp->name == NULL) {
        prop =
            xsltEvalAttrValueTemplate(ctxt, inst, (const xmlChar *) "name",
                                      XSLT_NAMESPACE);
        if (prop == NULL) {
            xsltTransformError(ctxt, NULL, inst,
                             "xsl:attribute : name is missing\n");
            goto error;
        }
	if (xmlValidateQName(prop, 0)) {
	    xsltTransformError(ctxt, NULL, inst,
			    "xsl:attribute : invalid QName\n");
	    /* we fall through to catch any further errors, if possible */
	}
	name = xsltSplitQName(ctxt->dict, prop, &prefix);
	xmlFree(prop);
    } else {
	name = xsltSplitQName(ctxt->dict, comp->name, &prefix);
    }

    if (!xmlStrncasecmp(prefix, (xmlChar *) "xmlns", 5)) {
#ifdef WITH_XSLT_DEBUG_PARSING
        xsltGenericDebug(xsltGenericDebugContext,
                         "xsltAttribute: xmlns prefix forbidden\n");
#endif
        goto error;
    }
    if ((comp->ns == NULL) && (comp->has_ns)) {
        namespace = xsltEvalAttrValueTemplate(ctxt, inst,
                                              (const xmlChar *)
                                              "namespace", XSLT_NAMESPACE);
        if (namespace != NULL) {
            ns = xsltGetSpecialNamespace(ctxt, inst, namespace, prefix,
                                         ctxt->insert);
            xmlFree(namespace);
        } else {
            if (prefix != NULL) {
                ns = xmlSearchNs(inst->doc, inst, prefix);
                if (ns == NULL) {
                    xsltTransformError(ctxt, NULL, inst,
			 "xsl:attribute : no namespace bound to prefix %s\n",
                                     prefix);
                } else {
                    ns = xsltGetNamespace(ctxt, inst, ns, ctxt->insert);
                }
            }
        }
    } else if (comp->ns != NULL) {
        ns = xsltGetSpecialNamespace(ctxt, inst, comp->ns, prefix,
                                     ctxt->insert);
    } else if (prefix != NULL) {
	xmlNsPtr tmp;
	tmp = xmlSearchNs(inst->doc, inst, prefix);
	if (tmp != NULL) {
	    ns = xsltGetNamespace(ctxt, inst, tmp, ctxt->insert);
	}
    }

    if ((fromset) && (ns != NULL))
        URL = ns->href;

    if (fromset) {
	if (URL != NULL)
	    attr = xmlHasNsProp(ctxt->insert, name, URL);
	else
	    attr = xmlHasProp(ctxt->insert, name);
	if (attr != NULL)
	    return;
    }
    value = xsltEvalTemplateString(ctxt, node, inst);
    if (value == NULL) {
        if (ns) {
            attr = xmlSetNsProp(ctxt->insert, ns, name,
                                (const xmlChar *) "");
        } else {
            attr =
                xmlSetProp(ctxt->insert, name, (const xmlChar *) "");
        }
    } else {
        if (ns) {
            attr = xmlSetNsProp(ctxt->insert, ns, name, value);
        } else {
            attr = xmlSetProp(ctxt->insert, name, value);
        }
    }

error:
    if (value != NULL)
        xmlFree(value);
}

/**
 * xsltAttribute:
 * @ctxt:  a XSLT process context
 * @node:  the node in the source tree.
 * @inst:  the xslt attribute node
 * @comp:  precomputed information
 *
 * Process the xslt attribute node on the source node
 */
void
xsltAttribute(xsltTransformContextPtr ctxt, xmlNodePtr node,
	      xmlNodePtr inst, xsltStylePreCompPtr comp) {
    xsltAttributeInternal(ctxt, node, inst, comp, 0);
}

/**
 * xsltApplyAttributeSet:
 * @ctxt:  the XSLT stylesheet
 * @node:  the node in the source tree.
 * @inst:  the xslt attribute node
 * @attributes:  the set list.
 *
 * Apply the xsl:use-attribute-sets
 */

void
xsltApplyAttributeSet(xsltTransformContextPtr ctxt, xmlNodePtr node,
                      xmlNodePtr inst ATTRIBUTE_UNUSED,
                      const xmlChar * attributes)
{
    const xmlChar *ncname = NULL;
    const xmlChar *prefix = NULL;
    const xmlChar *attrib, *endattr;
    xsltAttrElemPtr values;
    xsltStylesheetPtr style;

    if (attributes == NULL) {
        return;
    }

    attrib = attributes;
    while (*attrib != 0) {
        while (IS_BLANK(*attrib))
            attrib++;
        if (*attrib == 0)
            break;
        endattr = attrib;
        while ((*endattr != 0) && (!IS_BLANK(*endattr)))
            endattr++;
        attrib = xmlDictLookup(ctxt->dict, attrib, endattr - attrib);
        if (attrib) {
#ifdef WITH_XSLT_DEBUG_ATTRIBUTES
            xsltGenericDebug(xsltGenericDebugContext,
                             "apply attribute set %s\n", attrib);
#endif
            ncname = xsltSplitQName(ctxt->dict, attrib, &prefix);

            style = ctxt->style;
#ifdef WITH_DEBUGGER
            if ((style != NULL) && (style->attributeSets != NULL) &&
		(ctxt->debugStatus != XSLT_DEBUG_NONE)) {
                values =
                    xmlHashLookup2(style->attributeSets, ncname, prefix);
                if ((values != NULL) && (values->attr != NULL))
                    xslHandleDebugger(values->attr->parent, node, NULL,
                                      ctxt);
            }
#endif
            while (style != NULL) {
                values =
                    xmlHashLookup2(style->attributeSets, ncname, prefix);
                while (values != NULL) {
                    if (values->attr != NULL) {
                        xsltAttributeInternal(ctxt, node, values->attr,
                                              values->attr->psvi, 1);
                    }
                    values = values->next;
                }
                style = xsltNextImport(style);
            }
        }
        attrib = endattr;
    }
}

/**
 * xsltFreeAttributeSetsHashes:
 * @style: an XSLT stylesheet
 *
 * Free up the memory used by attribute sets
 */
void
xsltFreeAttributeSetsHashes(xsltStylesheetPtr style) {
    if (style->attributeSets != NULL)
	xmlHashFree((xmlHashTablePtr) style->attributeSets,
		    (xmlHashDeallocator) xsltFreeAttrElemList);
    style->attributeSets = NULL;
}
