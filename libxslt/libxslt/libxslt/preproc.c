/*
 * preproc.c: Preprocessing of style operations
 *
 * References:
 *   http://www.w3.org/TR/1999/REC-xslt-19991116
 *
 *   Michael Kay "XSLT Programmer's Reference" pp 637-643
 *   Writing Multiple Output Files
 *
 *   XSLT-1.1 Working Draft
 *   http://www.w3.org/TR/xslt11#multiple-output
 *
 * See Copyright for the status of this software.
 *
 * daniel@veillard.com
 */

#define IN_LIBXSLT
#include "libxslt.h"

#include <string.h>

#include <libxml/xmlmemory.h>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <libxml/valid.h>
#include <libxml/hash.h>
#include <libxml/uri.h>
#include <libxml/encoding.h>
#include <libxml/xmlerror.h>
#include "xslt.h"
#include "xsltutils.h"
#include "xsltInternals.h"
#include "transform.h"
#include "templates.h"
#include "variables.h"
#include "numbersInternals.h"
#include "preproc.h"
#include "extra.h"
#include "imports.h"
#include "extensions.h"

#ifdef WITH_XSLT_DEBUG
#define WITH_XSLT_DEBUG_PREPROC
#endif

const xmlChar *xsltExtMarker = (const xmlChar *) "Extension Element";

/************************************************************************
 *									*
 *			handling of precomputed data			*
 *									*
 ************************************************************************/

/**
 * xsltNewStylePreComp:
 * @style:  the XSLT stylesheet
 * @type:  the construct type
 *
 * Create a new XSLT Style precomputed block
 *
 * Returns the newly allocated xsltStylePreCompPtr or NULL in case of error
 */
static xsltStylePreCompPtr
xsltNewStylePreComp(xsltStylesheetPtr style, xsltStyleType type) {
    xsltStylePreCompPtr cur;

    cur = (xsltStylePreCompPtr) xmlMalloc(sizeof(xsltStylePreComp));
    if (cur == NULL) {
	xsltTransformError(NULL, style, NULL,
		"xsltNewStylePreComp : malloc failed\n");
	if (style != NULL) style->errors++;
	return(NULL);
    }
    memset(cur, 0, sizeof(xsltStylePreComp));

    cur->type = type;
    switch (cur->type) {
        case XSLT_FUNC_COPY:
            cur->func = (xsltTransformFunction) xsltCopy;break;
        case XSLT_FUNC_SORT:
            cur->func = (xsltTransformFunction) xsltSort;break;
        case XSLT_FUNC_TEXT:
            cur->func = (xsltTransformFunction) xsltText;break;
        case XSLT_FUNC_ELEMENT:
            cur->func = (xsltTransformFunction) xsltElement;break;
        case XSLT_FUNC_ATTRIBUTE:
            cur->func = (xsltTransformFunction) xsltAttribute;break;
        case XSLT_FUNC_COMMENT:
            cur->func = (xsltTransformFunction) xsltComment;break;
        case XSLT_FUNC_PI:
            cur->func = (xsltTransformFunction) xsltProcessingInstruction;
	    break;
        case XSLT_FUNC_COPYOF:
            cur->func = (xsltTransformFunction) xsltCopyOf;break;
        case XSLT_FUNC_VALUEOF:
            cur->func = (xsltTransformFunction) xsltValueOf;break;
        case XSLT_FUNC_NUMBER:
            cur->func = (xsltTransformFunction) xsltNumber;break;
        case XSLT_FUNC_APPLYIMPORTS:
            cur->func = (xsltTransformFunction) xsltApplyImports;break;
        case XSLT_FUNC_CALLTEMPLATE:
            cur->func = (xsltTransformFunction) xsltCallTemplate;break;
        case XSLT_FUNC_APPLYTEMPLATES:
            cur->func = (xsltTransformFunction) xsltApplyTemplates;break;
        case XSLT_FUNC_CHOOSE:
            cur->func = (xsltTransformFunction) xsltChoose;break;
        case XSLT_FUNC_IF:
            cur->func = (xsltTransformFunction) xsltIf;break;
        case XSLT_FUNC_FOREACH:
            cur->func = (xsltTransformFunction) xsltForEach;break;
        case XSLT_FUNC_DOCUMENT:
            cur->func = (xsltTransformFunction) xsltDocumentElem;break;
	case XSLT_FUNC_WITHPARAM:
	    cur->func = NULL;break;
	case XSLT_FUNC_PARAM:
	    cur->func = NULL;break;
	case XSLT_FUNC_VARIABLE:
	    cur->func = NULL;break;
	case XSLT_FUNC_WHEN:
	    cur->func = NULL;break;
	default:
	if (cur->func == NULL) {
	    xsltTransformError(NULL, style, NULL,
		    "xsltNewStylePreComp : no function for type %d\n", type);
	    if (style != NULL) style->errors++;
	}
    }
    cur->next = style->preComps;
    style->preComps = (xsltElemPreCompPtr) cur;

    return(cur);
}

/**
 * xsltFreeStylePreComp:
 * @comp:  an XSLT Style precomputed block
 *
 * Free up the memory allocated by @comp
 */
static void
xsltFreeStylePreComp(xsltStylePreCompPtr comp) {
    if (comp == NULL)
	return;

    if (comp->comp != NULL)
	xmlXPathFreeCompExpr(comp->comp);
    if (comp->nsList != NULL)
	xmlFree(comp->nsList);

    xmlFree(comp);
}


/************************************************************************
 *									*
 *		    XSLT-1.1 extensions					*
 *									*
 ************************************************************************/

/**
 * xsltDocumentComp:
 * @style:  the XSLT stylesheet
 * @inst:  the instruction in the stylesheet
 * @function:  unused
 *
 * Pre process an XSLT-1.1 document element
 *
 * Returns a precompiled data structure for the element
 */
xsltElemPreCompPtr
xsltDocumentComp(xsltStylesheetPtr style, xmlNodePtr inst,
		 xsltTransformFunction function ATTRIBUTE_UNUSED) {
    xsltStylePreCompPtr comp;
    const xmlChar *filename = NULL;

    comp = xsltNewStylePreComp(style, XSLT_FUNC_DOCUMENT);
    if (comp == NULL)
	return (NULL);
    comp->inst = inst;
    comp->ver11 = 0;

    if (xmlStrEqual(inst->name, (const xmlChar *) "output")) {
#ifdef WITH_XSLT_DEBUG_EXTRA
	xsltGenericDebug(xsltGenericDebugContext,
	    "Found saxon:output extension\n");
#endif
	filename = xsltEvalStaticAttrValueTemplate(style, inst,
			 (const xmlChar *)"file",
			 XSLT_SAXON_NAMESPACE, &comp->has_filename);
    } else if (xmlStrEqual(inst->name, (const xmlChar *) "write")) {
#ifdef WITH_XSLT_DEBUG_EXTRA
	xsltGenericDebug(xsltGenericDebugContext,
	    "Found xalan:write extension\n");
#endif
	comp->ver11 = 0; /* the filename need to be interpreted */
    } else if (xmlStrEqual(inst->name, (const xmlChar *) "document")) {
	filename = xsltEvalStaticAttrValueTemplate(style, inst,
			 (const xmlChar *)"href",
			 XSLT_XT_NAMESPACE, &comp->has_filename);
	if (comp->has_filename == 0) {
#ifdef WITH_XSLT_DEBUG_EXTRA
	    xsltGenericDebug(xsltGenericDebugContext,
		"Found xslt11:document construct\n");
#endif
	    filename = xsltEvalStaticAttrValueTemplate(style, inst,
			     (const xmlChar *)"href",
			     XSLT_NAMESPACE, &comp->has_filename);
	    comp->ver11 = 1;
	} else {
#ifdef WITH_XSLT_DEBUG_EXTRA
	    xsltGenericDebug(xsltGenericDebugContext,
		"Found xt:document extension\n");
#endif
	    comp->ver11 = 0;
	}
    }
    if (!comp->has_filename) {
	goto error;
    }
    comp->filename = filename;

error:
    return ((xsltElemPreCompPtr) comp);
}

/************************************************************************
 *									*
 *		Most of the XSLT-1.0 transformations			*
 *									*
 ************************************************************************/

/**
 * xsltSortComp:
 * @style:  the XSLT stylesheet
 * @inst:  the xslt sort node
 *
 * Process the xslt sort node on the source node
 */
static void
xsltSortComp(xsltStylesheetPtr style, xmlNodePtr inst) {
    xsltStylePreCompPtr comp;

    if ((style == NULL) || (inst == NULL))
	return;
    comp = xsltNewStylePreComp(style, XSLT_FUNC_SORT);
    if (comp == NULL)
	return;
    inst->psvi = comp;
    comp->inst = inst;

    comp->stype = xsltEvalStaticAttrValueTemplate(style, inst,
			 (const xmlChar *)"data-type",
			 XSLT_NAMESPACE, &comp->has_stype);
    if (comp->stype != NULL) {
	if (xmlStrEqual(comp->stype, (const xmlChar *) "text"))
	    comp->number = 0;
	else if (xmlStrEqual(comp->stype, (const xmlChar *) "number"))
	    comp->number = 1;
	else {
	    xsltTransformError(NULL, style, inst,
		 "xsltSortComp: no support for data-type = %s\n", comp->stype);
	    comp->number = 0; /* use default */
	    if (style != NULL) style->warnings++;
	}
    }
    comp->order = xsltEvalStaticAttrValueTemplate(style, inst,
			      (const xmlChar *)"order",
			      XSLT_NAMESPACE, &comp->has_order);
    if (comp->order != NULL) {
	if (xmlStrEqual(comp->order, (const xmlChar *) "ascending"))
	    comp->descending = 0;
	else if (xmlStrEqual(comp->order, (const xmlChar *) "descending"))
	    comp->descending = 1;
	else {
	    xsltTransformError(NULL, style, inst,
		 "xsltSortComp: invalid value %s for order\n", comp->order);
	    comp->descending = 0; /* use default */
	    if (style != NULL) style->warnings++;
	}
    }
    comp->case_order = xsltEvalStaticAttrValueTemplate(style, inst,
			      (const xmlChar *)"case-order",
			      XSLT_NAMESPACE, &comp->has_use);
    if (comp->case_order != NULL) {
	if (xmlStrEqual(comp->case_order, (const xmlChar *) "upper-first"))
	    comp->lower_first = 0;
	else if (xmlStrEqual(comp->case_order, (const xmlChar *) "lower-first"))
	    comp->lower_first = 1;
	else {
	    xsltTransformError(NULL, style, inst,
		 "xsltSortComp: invalid value %s for order\n", comp->order);
	    comp->lower_first = 0; /* use default */
	    if (style != NULL) style->warnings++;
	}
    }

    comp->lang = xsltEvalStaticAttrValueTemplate(style, inst,
				 (const xmlChar *)"lang",
				 XSLT_NAMESPACE, &comp->has_lang);

    comp->select = xsltGetCNsProp(style, inst,(const xmlChar *)"select", XSLT_NAMESPACE);
    if (comp->select == NULL) {
	/*
	 * The default value of the select attribute is ., which will
	 * cause the string-value of the current node to be used as
	 * the sort key.
	 */
	comp->select = xmlDictLookup(style->dict, BAD_CAST ".", 1);
    }
    comp->comp = xsltXPathCompile(style, comp->select);
    if (comp->comp == NULL) {
	xsltTransformError(NULL, style, inst,
	     "xsltSortComp: could not compile select expression '%s'\n",
	                 comp->select);
	if (style != NULL) style->errors++;
    }
    if (inst->children != NULL) {
	xsltTransformError(NULL, style, inst,
	"xsl:sort : is not empty\n");
	if (style != NULL) style->errors++;
    }
}

/**
 * xsltCopyComp:
 * @style:  the XSLT stylesheet
 * @inst:  the xslt copy node
 *
 * Process the xslt copy node on the source node
 */
static void
xsltCopyComp(xsltStylesheetPtr style, xmlNodePtr inst) {
    xsltStylePreCompPtr comp;


    if ((style == NULL) || (inst == NULL))
	return;
    comp = xsltNewStylePreComp(style, XSLT_FUNC_COPY);
    if (comp == NULL)
	return;
    inst->psvi = comp;
    comp->inst = inst;


    comp->use = xsltGetCNsProp(style, inst, (const xmlChar *)"use-attribute-sets",
				    XSLT_NAMESPACE);
    if (comp->use == NULL)
	comp->has_use = 0;
    else
	comp->has_use = 1;
}

/**
 * xsltTextComp:
 * @style: an XSLT compiled stylesheet
 * @inst:  the xslt text node
 *
 * Process the xslt text node on the source node
 */
static void
xsltTextComp(xsltStylesheetPtr style, xmlNodePtr inst) {
    xsltStylePreCompPtr comp;
    const xmlChar *prop;

    if ((style == NULL) || (inst == NULL))
	return;
    comp = xsltNewStylePreComp(style, XSLT_FUNC_TEXT);
    if (comp == NULL)
	return;
    inst->psvi = comp;
    comp->inst = inst;
    comp->noescape = 0;

    prop = xsltGetCNsProp(style, inst,
	    (const xmlChar *)"disable-output-escaping",
			XSLT_NAMESPACE);
    if (prop != NULL) {
	if (xmlStrEqual(prop, (const xmlChar *)"yes")) {
	    comp->noescape = 1;
	} else if (!xmlStrEqual(prop,
				(const xmlChar *)"no")){
	    xsltTransformError(NULL, style, inst,
"xsl:text: disable-output-escaping allows only yes or no\n");
	    if (style != NULL) style->warnings++;
	}
    }
}

/**
 * xsltElementComp:
 * @style: an XSLT compiled stylesheet
 * @inst:  the xslt element node
 *
 * Process the xslt element node on the source node
 */
static void
xsltElementComp(xsltStylesheetPtr style, xmlNodePtr inst) {
    xsltStylePreCompPtr comp;

    if ((style == NULL) || (inst == NULL))
	return;
    comp = xsltNewStylePreComp(style, XSLT_FUNC_ELEMENT);
    if (comp == NULL)
	return;
    inst->psvi = comp;
    comp->inst = inst;

    comp->name = xsltEvalStaticAttrValueTemplate(style, inst,
				 (const xmlChar *)"name",
				 XSLT_NAMESPACE, &comp->has_name);
    if (comp->name != NULL) {
	if (xmlValidateQName(comp->name, 0)) {
	    xsltTransformError(NULL, style, inst,
		    "xsl:element : invalid name\n");
	    if (style != NULL) style->errors++;
	}
    }
    comp->ns = xsltEvalStaticAttrValueTemplate(style, inst,
			 (const xmlChar *)"namespace",
			 XSLT_NAMESPACE, &comp->has_ns);
    if (comp->has_ns == 0) {
	xmlNsPtr defaultNs;

	defaultNs = xmlSearchNs(inst->doc, inst, NULL);
	if (defaultNs != NULL) {
	    comp->ns = xmlDictLookup(style->dict, defaultNs->href, -1);
	    comp->has_ns = 1;
	}
    }
    comp->use = xsltEvalStaticAttrValueTemplate(style, inst,
		       (const xmlChar *)"use-attribute-sets",
		       XSLT_NAMESPACE, &comp->has_use);
}

/**
 * xsltAttributeComp:
 * @style: an XSLT compiled stylesheet
 * @inst:  the xslt attribute node
 *
 * Process the xslt attribute node on the source node
 */
static void
xsltAttributeComp(xsltStylesheetPtr style, xmlNodePtr inst) {
    xsltStylePreCompPtr comp;

    if ((style == NULL) || (inst == NULL))
	return;
    comp = xsltNewStylePreComp(style, XSLT_FUNC_ATTRIBUTE);
    if (comp == NULL)
	return;
    inst->psvi = comp;
    comp->inst = inst;

    /*
     * TODO: more computation can be done there, especially namespace lookup
     */
    comp->name = xsltEvalStaticAttrValueTemplate(style, inst,
				 (const xmlChar *)"name",
				 XSLT_NAMESPACE, &comp->has_name);
    if (comp->name != NULL) {
	if (xmlValidateQName(comp->name, 0)) {
	    xsltTransformError(NULL, style, inst,
			    "xsl:attribute : invalid QName\n");
	    if (style != NULL) style->errors++;
	}
    }
    comp->ns = xsltEvalStaticAttrValueTemplate(style, inst,
			 (const xmlChar *)"namespace",
			 XSLT_NAMESPACE, &comp->has_ns);

}

/**
 * xsltCommentComp:
 * @style: an XSLT compiled stylesheet
 * @inst:  the xslt comment node
 *
 * Process the xslt comment node on the source node
 */
static void
xsltCommentComp(xsltStylesheetPtr style, xmlNodePtr inst) {
    xsltStylePreCompPtr comp;

    if ((style == NULL) || (inst == NULL))
	return;
    comp = xsltNewStylePreComp(style, XSLT_FUNC_COMMENT);
    if (comp == NULL)
	return;
    inst->psvi = comp;
    comp->inst = inst;
}

/**
 * xsltProcessingInstructionComp:
 * @style: an XSLT compiled stylesheet
 * @inst:  the xslt processing-instruction node
 *
 * Process the xslt processing-instruction node on the source node
 */
static void
xsltProcessingInstructionComp(xsltStylesheetPtr style, xmlNodePtr inst) {
    xsltStylePreCompPtr comp;

    if ((style == NULL) || (inst == NULL))
	return;
    comp = xsltNewStylePreComp(style, XSLT_FUNC_PI);
    if (comp == NULL)
	return;
    inst->psvi = comp;
    comp->inst = inst;

    comp->name = xsltEvalStaticAttrValueTemplate(style, inst,
				 (const xmlChar *)"name",
				 XSLT_NAMESPACE, &comp->has_name);
}

/**
 * xsltCopyOfComp:
 * @style: an XSLT compiled stylesheet
 * @inst:  the xslt copy-of node
 *
 * Process the xslt copy-of node on the source node
 */
static void
xsltCopyOfComp(xsltStylesheetPtr style, xmlNodePtr inst) {
    xsltStylePreCompPtr comp;

    if ((style == NULL) || (inst == NULL))
	return;
    comp = xsltNewStylePreComp(style, XSLT_FUNC_COPYOF);
    if (comp == NULL)
	return;
    inst->psvi = comp;
    comp->inst = inst;

    comp->select = xsltGetCNsProp(style, inst, (const xmlChar *)"select",
	                        XSLT_NAMESPACE);
    if (comp->select == NULL) {
	xsltTransformError(NULL, style, inst,
	     "xsl:copy-of : select is missing\n");
	if (style != NULL) style->errors++;
	return;
    }
    comp->comp = xsltXPathCompile(style, comp->select);
    if (comp->comp == NULL) {
	xsltTransformError(NULL, style, inst,
	     "xsl:copy-of : could not compile select expression '%s'\n",
	                 comp->select);
	if (style != NULL) style->errors++;
    }
}

/**
 * xsltValueOfComp:
 * @style: an XSLT compiled stylesheet
 * @inst:  the xslt value-of node
 *
 * Process the xslt value-of node on the source node
 */
static void
xsltValueOfComp(xsltStylesheetPtr style, xmlNodePtr inst) {
    xsltStylePreCompPtr comp;
    const xmlChar *prop;

    if ((style == NULL) || (inst == NULL))
	return;
    comp = xsltNewStylePreComp(style, XSLT_FUNC_VALUEOF);
    if (comp == NULL)
	return;
    inst->psvi = comp;
    comp->inst = inst;

    prop = xsltGetCNsProp(style, inst,
	    (const xmlChar *)"disable-output-escaping",
			XSLT_NAMESPACE);
    if (prop != NULL) {
	if (xmlStrEqual(prop, (const xmlChar *)"yes")) {
	    comp->noescape = 1;
	} else if (!xmlStrEqual(prop,
				(const xmlChar *)"no")){
	    xsltTransformError(NULL, style, inst,
"xsl:value-of : disable-output-escaping allows only yes or no\n");
	    if (style != NULL) style->warnings++;
	}
    }
    comp->select = xsltGetCNsProp(style, inst, (const xmlChar *)"select",
	                        XSLT_NAMESPACE);
    if (comp->select == NULL) {
	xsltTransformError(NULL, style, inst,
	     "xsl:value-of : select is missing\n");
	if (style != NULL) style->errors++;
	return;
    }
    comp->comp = xsltXPathCompile(style, comp->select);
    if (comp->comp == NULL) {
	xsltTransformError(NULL, style, inst,
	     "xsl:value-of : could not compile select expression '%s'\n",
	                 comp->select);
	if (style != NULL) style->errors++;
    }
}

/**
 * xsltWithParamComp:
 * @style: an XSLT compiled stylesheet
 * @inst:  the xslt with-param node
 *
 * Process the xslt with-param node on the source node
 */
static void
xsltWithParamComp(xsltStylesheetPtr style, xmlNodePtr inst) {
    xsltStylePreCompPtr comp;
    const xmlChar *prop;

    if ((style == NULL) || (inst == NULL))
	return;
    comp = xsltNewStylePreComp(style, XSLT_FUNC_WITHPARAM);
    if (comp == NULL)
	return;
    inst->psvi = comp;
    comp->inst = inst;

    /*
     * The full namespace resolution can be done statically
     */
    prop = xsltGetCNsProp(style, inst, (const xmlChar *)"name", XSLT_NAMESPACE);
    if (prop == NULL) {
	xsltTransformError(NULL, style, inst,
	     "xsl:with-param : name is missing\n");
	if (style != NULL) style->errors++;
    } else {
        const xmlChar *URI;

	URI = xsltGetQNameURI2(style, inst, &prop);
	if (prop == NULL) {
	    if (style != NULL) style->errors++;
	} else {
	    comp->name = prop;
	    comp->has_name = 1;
	    if (URI != NULL) {
		comp->ns = xmlStrdup(URI);
		comp->has_ns = 1;
	    } else {
		comp->has_ns = 0;
	    }
	}
    }

    comp->select = xsltGetCNsProp(style, inst, (const xmlChar *)"select",
	                        XSLT_NAMESPACE);
    if (comp->select != NULL) {
	comp->comp = xsltXPathCompile(style, comp->select);
	if (comp->comp == NULL) {
	    xsltTransformError(NULL, style, inst,
		 "xsl:param : could not compile select expression '%s'\n",
			     comp->select);
	    if (style != NULL) style->errors++;
	}
	if (inst->children != NULL) {
	    xsltTransformError(NULL, style, inst,
	    "xsl:param : content should be empty since select is present \n");
	    if (style != NULL) style->warnings++;
	}
    }
}

/**
 * xsltNumberComp:
 * @style: an XSLT compiled stylesheet
 * @cur:   the xslt number node
 *
 * Process the xslt number node on the source node
 */
static void
xsltNumberComp(xsltStylesheetPtr style, xmlNodePtr cur) {
    xsltStylePreCompPtr comp;
    const xmlChar *prop;

    if ((style == NULL) || (cur == NULL))
	return;
    comp = xsltNewStylePreComp(style, XSLT_FUNC_NUMBER);
    if (comp == NULL)
	return;
    cur->psvi = comp;

    if ((style == NULL) || (cur == NULL))
	return;

    comp->numdata.doc = cur->doc;
    comp->numdata.node = cur;
    comp->numdata.value = xsltGetCNsProp(style, cur, (const xmlChar *)"value",
	                                XSLT_NAMESPACE);
    
    prop = xsltEvalStaticAttrValueTemplate(style, cur,
			 (const xmlChar *)"format",
			 XSLT_NAMESPACE, &comp->numdata.has_format);
    if (comp->numdata.has_format == 0) {
	comp->numdata.format = xmlDictLookup(style->dict, BAD_CAST "" , 0);
    } else {
	comp->numdata.format = prop;
    }

    comp->numdata.count = xsltGetCNsProp(style, cur, (const xmlChar *)"count",
					XSLT_NAMESPACE);
    comp->numdata.from = xsltGetCNsProp(style, cur, (const xmlChar *)"from",
					XSLT_NAMESPACE);
    
    prop = xsltGetCNsProp(style, cur, (const xmlChar *)"level", XSLT_NAMESPACE);
    if (prop != NULL) {
	if (xmlStrEqual(prop, BAD_CAST("single")) ||
	    xmlStrEqual(prop, BAD_CAST("multiple")) ||
	    xmlStrEqual(prop, BAD_CAST("any"))) {
	    comp->numdata.level = prop;
	} else {
	    xsltTransformError(NULL, style, cur,
			 "xsl:number : invalid value %s for level\n", prop);
	    if (style != NULL) style->warnings++;
	    xmlFree((void *)(prop));
	}
    }
    
    prop = xsltGetCNsProp(style, cur, (const xmlChar *)"lang", XSLT_NAMESPACE);
    if (prop != NULL) {
	XSLT_TODO; /* xsl:number lang attribute */
	xmlFree((void *)prop);
    }
    
    prop = xsltGetCNsProp(style, cur, (const xmlChar *)"letter-value", XSLT_NAMESPACE);
    if (prop != NULL) {
	if (xmlStrEqual(prop, BAD_CAST("alphabetic"))) {
	    xsltTransformError(NULL, style, cur,
		 "xsl:number : letter-value 'alphabetic' not implemented\n");
	    if (style != NULL) style->warnings++;
	    XSLT_TODO; /* xsl:number letter-value attribute alphabetic */
	} else if (xmlStrEqual(prop, BAD_CAST("traditional"))) {
	    xsltTransformError(NULL, style, cur,
		 "xsl:number : letter-value 'traditional' not implemented\n");
	    if (style != NULL) style->warnings++;
	    XSLT_TODO; /* xsl:number letter-value attribute traditional */
	} else {
	    xsltTransformError(NULL, style, cur,
		     "xsl:number : invalid value %s for letter-value\n", prop);
	    if (style != NULL) style->warnings++;
	}
    }
    
    prop = xsltGetCNsProp(style, cur, (const xmlChar *)"grouping-separator",
	                XSLT_NAMESPACE);
    if (prop != NULL) {
        comp->numdata.groupingCharacterLen = xmlStrlen(prop);
	comp->numdata.groupingCharacter =
	    xsltGetUTF8Char(prop, &(comp->numdata.groupingCharacterLen));
    }
    
    prop = xsltGetCNsProp(style, cur, (const xmlChar *)"grouping-size", XSLT_NAMESPACE);
    if (prop != NULL) {
	sscanf((char *)prop, "%d", &comp->numdata.digitsPerGroup);
    } else {
	comp->numdata.groupingCharacter = 0;
    }

    /* Set default values */
    if (comp->numdata.value == NULL) {
	if (comp->numdata.level == NULL) {
	    comp->numdata.level = xmlDictLookup(style->dict,
	                                        BAD_CAST"single", 6);
	}
    }
    
}

/**
 * xsltApplyImportsComp:
 * @style: an XSLT compiled stylesheet
 * @inst:  the xslt apply-imports node
 *
 * Process the xslt apply-imports node on the source node
 */
static void
xsltApplyImportsComp(xsltStylesheetPtr style, xmlNodePtr inst) {
    xsltStylePreCompPtr comp;

    if ((style == NULL) || (inst == NULL))
	return;
    comp = xsltNewStylePreComp(style, XSLT_FUNC_APPLYIMPORTS);
    if (comp == NULL)
	return;
    inst->psvi = comp;
    comp->inst = inst;
}

/**
 * xsltCallTemplateComp:
 * @style: an XSLT compiled stylesheet
 * @inst:  the xslt call-template node
 *
 * Process the xslt call-template node on the source node
 */
static void
xsltCallTemplateComp(xsltStylesheetPtr style, xmlNodePtr inst) {
    xsltStylePreCompPtr comp;
    const xmlChar *prop;

    if ((style == NULL) || (inst == NULL))
	return;
    comp = xsltNewStylePreComp(style, XSLT_FUNC_CALLTEMPLATE);
    if (comp == NULL)
	return;
    inst->psvi = comp;
    comp->inst = inst;

    /*
     * The full template resolution can be done statically
     */
    prop = xsltGetCNsProp(style, inst, (const xmlChar *)"name", XSLT_NAMESPACE);
    if (prop == NULL) {
	xsltTransformError(NULL, style, inst,
	     "xsl:call-template : name is missing\n");
	if (style != NULL) style->errors++;
    } else {
        const xmlChar *URI;

	URI = xsltGetQNameURI2(style, inst, &prop);
	if (prop == NULL) {
	    if (style != NULL) style->errors++;
	} else {
	    comp->name = prop;
	    comp->has_name = 1;
	    if (URI != NULL) {
		comp->ns = URI;
		comp->has_ns = 1;
	    } else {
		comp->has_ns = 0;
	    }
	}
	comp->templ = NULL;
    }
}

/**
 * xsltApplyTemplatesComp:
 * @style: an XSLT compiled stylesheet
 * @inst:  the apply-templates node
 *
 * Process the apply-templates node on the source node
 */
static void
xsltApplyTemplatesComp(xsltStylesheetPtr style, xmlNodePtr inst) {
    xsltStylePreCompPtr comp;
    const xmlChar *prop;

    if ((style == NULL) || (inst == NULL))
	return;
    comp = xsltNewStylePreComp(style, XSLT_FUNC_APPLYTEMPLATES);
    if (comp == NULL)
	return;
    inst->psvi = comp;
    comp->inst = inst;

    /*
     * Get mode if any
     */
    prop = xsltGetCNsProp(style, inst, (const xmlChar *)"mode", XSLT_NAMESPACE);
    if (prop != NULL) {
        const xmlChar *URI;

	URI = xsltGetQNameURI2(style, inst, &prop);
	if (prop == NULL) {
	    if (style != NULL) style->errors++;
	} else {
	    comp->mode = prop;
	    if (URI != NULL) {
		comp->modeURI = xmlStrdup(URI);
	    } else {
		comp->modeURI = NULL;
	    }
	}
    }
    comp->select = xsltGetCNsProp(style, inst, (const xmlChar *)"select",
	                        XSLT_NAMESPACE);
    if (comp->select != NULL) {
	comp->comp = xsltXPathCompile(style, comp->select);
	if (comp->comp == NULL) {
	    xsltTransformError(NULL, style, inst,
     "xsl:apply-templates : could not compile select expression '%s'\n",
			     comp->select);
	    if (style != NULL) style->errors++;
	}
    }

    /* TODO: handle (or skip) the xsl:sort and xsl:with-param */
}

/**
 * xsltChooseComp:
 * @style: an XSLT compiled stylesheet
 * @inst:  the xslt choose node
 *
 * Process the xslt choose node on the source node
 */
static void
xsltChooseComp(xsltStylesheetPtr style, xmlNodePtr inst) {
    xsltStylePreCompPtr comp;

    if ((style == NULL) || (inst == NULL))
	return;
    comp = xsltNewStylePreComp(style, XSLT_FUNC_CHOOSE);
    if (comp == NULL)
	return;
    inst->psvi = comp;
    comp->inst = inst;
}

/**
 * xsltIfComp:
 * @style: an XSLT compiled stylesheet
 * @inst:  the xslt if node
 *
 * Process the xslt if node on the source node
 */
static void
xsltIfComp(xsltStylesheetPtr style, xmlNodePtr inst) {
    xsltStylePreCompPtr comp;

    if ((style == NULL) || (inst == NULL))
	return;
    comp = xsltNewStylePreComp(style, XSLT_FUNC_IF);
    if (comp == NULL)
	return;
    inst->psvi = comp;
    comp->inst = inst;

    comp->test = xsltGetCNsProp(style, inst, (const xmlChar *)"test", XSLT_NAMESPACE);
    if (comp->test == NULL) {
	xsltTransformError(NULL, style, inst,
	     "xsl:if : test is not defined\n");
	if (style != NULL) style->errors++;
	return;
    }
    comp->comp = xsltXPathCompile(style, comp->test);
    if (comp->comp == NULL) {
	xsltTransformError(NULL, style, inst,
	     "xsl:if : could not compile test expression '%s'\n",
	                 comp->test);
	if (style != NULL) style->errors++;
    }
}

/**
 * xsltWhenComp:
 * @style: an XSLT compiled stylesheet
 * @inst:  the xslt if node
 *
 * Process the xslt if node on the source node
 */
static void
xsltWhenComp(xsltStylesheetPtr style, xmlNodePtr inst) {
    xsltStylePreCompPtr comp;

    if ((style == NULL) || (inst == NULL))
	return;
    comp = xsltNewStylePreComp(style, XSLT_FUNC_WHEN);
    if (comp == NULL)
	return;
    inst->psvi = comp;
    comp->inst = inst;

    comp->test = xsltGetCNsProp(style, inst, (const xmlChar *)"test", XSLT_NAMESPACE);
    if (comp->test == NULL) {
	xsltTransformError(NULL, style, inst,
	     "xsl:when : test is not defined\n");
	if (style != NULL) style->errors++;
	return;
    }
    comp->comp = xsltXPathCompile(style, comp->test);
    if (comp->comp == NULL) {
	xsltTransformError(NULL, style, inst,
	     "xsl:when : could not compile test expression '%s'\n",
	                 comp->test);
	if (style != NULL) style->errors++;
    }
}

/**
 * xsltForEachComp:
 * @style: an XSLT compiled stylesheet
 * @inst:  the xslt for-each node
 *
 * Process the xslt for-each node on the source node
 */
static void
xsltForEachComp(xsltStylesheetPtr style, xmlNodePtr inst) {
    xsltStylePreCompPtr comp;

    if ((style == NULL) || (inst == NULL))
	return;
    comp = xsltNewStylePreComp(style, XSLT_FUNC_FOREACH);
    if (comp == NULL)
	return;
    inst->psvi = comp;
    comp->inst = inst;

    comp->select = xsltGetCNsProp(style, inst, (const xmlChar *)"select",
	                        XSLT_NAMESPACE);
    if (comp->select == NULL) {
	xsltTransformError(NULL, style, inst,
		"xsl:for-each : select is missing\n");
	if (style != NULL) style->errors++;
    } else {
	comp->comp = xsltXPathCompile(style, comp->select);
	if (comp->comp == NULL) {
	    xsltTransformError(NULL, style, inst,
     "xsl:for-each : could not compile select expression '%s'\n",
			     comp->select);
	    if (style != NULL) style->errors++;
	}
    }
    /* TODO: handle and skip the xsl:sort */
}

/**
 * xsltVariableComp:
 * @style: an XSLT compiled stylesheet
 * @inst:  the xslt variable node
 *
 * Process the xslt variable node on the source node
 */
static void
xsltVariableComp(xsltStylesheetPtr style, xmlNodePtr inst) {
    xsltStylePreCompPtr comp;
    const xmlChar *prop;

    if ((style == NULL) || (inst == NULL))
	return;
    comp = xsltNewStylePreComp(style, XSLT_FUNC_VARIABLE);
    if (comp == NULL)
	return;
    inst->psvi = comp;
    comp->inst = inst;

    /*
     * The full template resolution can be done statically
     */
    prop = xsltGetCNsProp(style, inst, (const xmlChar *)"name", XSLT_NAMESPACE);
    if (prop == NULL) {
	xsltTransformError(NULL, style, inst,
	     "xsl:variable : name is missing\n");
	if (style != NULL) style->errors++;
    } else {
        const xmlChar *URI;

	URI = xsltGetQNameURI2(style, inst, &prop);
	if (prop == NULL) {
	    if (style != NULL) style->errors++;
	} else {
	    comp->name = prop;
	    comp->has_name = 1;
	    if (URI != NULL) {
		comp->ns = xmlStrdup(URI);
		comp->has_ns = 1;
	    } else {
		comp->has_ns = 0;
	    }
	}
    }

    comp->select = xsltGetCNsProp(style, inst, (const xmlChar *)"select",
	                        XSLT_NAMESPACE);
    if (comp->select != NULL) {
	comp->comp = xsltXPathCompile(style, comp->select);
	if (comp->comp == NULL) {
	    xsltTransformError(NULL, style, inst,
		 "xsl:variable : could not compile select expression '%s'\n",
			     comp->select);
	    if (style != NULL) style->errors++;
	}
	if (inst->children != NULL) {
	    xsltTransformError(NULL, style, inst,
	"xsl:variable : content should be empty since select is present \n");
	    if (style != NULL) style->warnings++;
	}
    }
}

/**
 * xsltParamComp:
 * @style: an XSLT compiled stylesheet
 * @inst:  the xslt param node
 *
 * Process the xslt param node on the source node
 */
static void
xsltParamComp(xsltStylesheetPtr style, xmlNodePtr inst) {
    xsltStylePreCompPtr comp;
    const xmlChar *prop;

    if ((style == NULL) || (inst == NULL))
	return;
    comp = xsltNewStylePreComp(style, XSLT_FUNC_PARAM);
    if (comp == NULL)
	return;
    inst->psvi = comp;
    comp->inst = inst;

    /*
     * The full template resolution can be done statically
     */
    prop = xsltGetCNsProp(style, inst, (const xmlChar *)"name", XSLT_NAMESPACE);
    if (prop == NULL) {
	xsltTransformError(NULL, style, inst,
	     "xsl:param : name is missing\n");
	if (style != NULL) style->errors++;
    } else {
        const xmlChar *URI;

	URI = xsltGetQNameURI2(style, inst, &prop);
	if (prop == NULL) {
	    if (style != NULL) style->errors++;
	} else {
	    comp->name = prop;
	    comp->has_name = 1;
	    if (URI != NULL) {
		comp->ns = xmlStrdup(URI);
		comp->has_ns = 1;
	    } else {
		comp->has_ns = 0;
	    }
	}
    }

    comp->select = xsltGetCNsProp(style, inst, (const xmlChar *)"select",
	                        XSLT_NAMESPACE);
    if (comp->select != NULL) {
	comp->comp = xsltXPathCompile(style, comp->select);
	if (comp->comp == NULL) {
	    xsltTransformError(NULL, style, inst,
		 "xsl:param : could not compile select expression '%s'\n",
			     comp->select);
	    if (style != NULL) style->errors++;
	}
	if (inst->children != NULL) {
	    xsltTransformError(NULL, style, inst,
	"xsl:param : content should be empty since select is present \n");
	    if (style != NULL) style->warnings++;
	}
    }
}


/************************************************************************
 *									*
 *		    Generic interface					*
 *									*
 ************************************************************************/

/**
 * xsltFreeStylePreComps:
 * @style:  an XSLT transformation context
 *
 * Free up the memory allocated by all precomputed blocks
 */
void
xsltFreeStylePreComps(xsltStylesheetPtr style) {
    xsltElemPreCompPtr cur, next;

    if (style == NULL)
	return;
    cur = style->preComps;
    while (cur != NULL) {
	next = cur->next;
	if (cur->type == XSLT_FUNC_EXTENSION)
	    cur->free(cur);
	else
	    xsltFreeStylePreComp((xsltStylePreCompPtr) cur);
	cur = next;
    }
}

/**
 * xsltStylePreCompute:
 * @style:  the XSLT stylesheet
 * @inst:  the instruction in the stylesheet
 *
 * Precompute an XSLT stylesheet element
 */
void
xsltStylePreCompute(xsltStylesheetPtr style, xmlNodePtr inst) {
    if (inst->psvi != NULL) 
        return;
    if (IS_XSLT_ELEM(inst)) {
	xsltStylePreCompPtr cur;

	if (IS_XSLT_NAME(inst, "apply-templates")) {
	    xsltApplyTemplatesComp(style, inst);
	} else if (IS_XSLT_NAME(inst, "with-param")) {
	    xsltWithParamComp(style, inst);
	} else if (IS_XSLT_NAME(inst, "value-of")) {
	    xsltValueOfComp(style, inst);
	} else if (IS_XSLT_NAME(inst, "copy")) {
	    xsltCopyComp(style, inst);
	} else if (IS_XSLT_NAME(inst, "copy-of")) {
	    xsltCopyOfComp(style, inst);
	} else if (IS_XSLT_NAME(inst, "if")) {
	    xsltIfComp(style, inst);
	} else if (IS_XSLT_NAME(inst, "when")) {
	    xsltWhenComp(style, inst);
	} else if (IS_XSLT_NAME(inst, "choose")) {
	    xsltChooseComp(style, inst);
	} else if (IS_XSLT_NAME(inst, "for-each")) {
	    xsltForEachComp(style, inst);
	} else if (IS_XSLT_NAME(inst, "apply-imports")) {
	    xsltApplyImportsComp(style, inst);
	} else if (IS_XSLT_NAME(inst, "attribute")) {
	    xsltAttributeComp(style, inst);
	} else if (IS_XSLT_NAME(inst, "element")) {
	    xsltElementComp(style, inst);
	} else if (IS_XSLT_NAME(inst, "text")) {
	    xsltTextComp(style, inst);
	} else if (IS_XSLT_NAME(inst, "sort")) {
	    xsltSortComp(style, inst);
	} else if (IS_XSLT_NAME(inst, "comment")) {
	    xsltCommentComp(style, inst);
	} else if (IS_XSLT_NAME(inst, "number")) {
	    xsltNumberComp(style, inst);
	} else if (IS_XSLT_NAME(inst, "processing-instruction")) {
	    xsltProcessingInstructionComp(style, inst);
	} else if (IS_XSLT_NAME(inst, "call-template")) {
	    xsltCallTemplateComp(style, inst);
	} else if (IS_XSLT_NAME(inst, "param")) {
	    xsltParamComp(style, inst);
	} else if (IS_XSLT_NAME(inst, "variable")) {
	    xsltVariableComp(style, inst);
	} else if (IS_XSLT_NAME(inst, "otherwise")) {
	    /* no computation needed */
	    return;
	} else if (IS_XSLT_NAME(inst, "template")) {
	    /* no computation needed */
	    return;
	} else if (IS_XSLT_NAME(inst, "output")) {
	    /* no computation needed */
	    return;
	} else if (IS_XSLT_NAME(inst, "preserve-space")) {
	    /* no computation needed */
	    return;
	} else if (IS_XSLT_NAME(inst, "strip-space")) {
	    /* no computation needed */
	    return;
	} else if (IS_XSLT_NAME(inst, "stylesheet")) {
	    /* no computation needed */
	    return;
	} else if (IS_XSLT_NAME(inst, "transform")) {
	    /* no computation needed */
	    return;
	} else if (IS_XSLT_NAME(inst, "key")) {
	    /* no computation needed */
	    return;
	} else if (IS_XSLT_NAME(inst, "message")) {
	    /* no computation needed */
	    return;
	} else if (IS_XSLT_NAME(inst, "attribute-set")) {
	    /* no computation needed */
	    return;
	} else if (IS_XSLT_NAME(inst, "namespace-alias")) {
	    /* no computation needed */
	    return;
	} else if (IS_XSLT_NAME(inst, "include")) {
	    /* no computation needed */
	    return;
	} else if (IS_XSLT_NAME(inst, "import")) {
	    /* no computation needed */
	    return;
	} else if (IS_XSLT_NAME(inst, "decimal-format")) {
	    /* no computation needed */
	    return;
	} else if (IS_XSLT_NAME(inst, "fallback")) {
	    /* no computation needed */
	    return;
	} else if (IS_XSLT_NAME(inst, "document")) {
	    inst->psvi = (void *) xsltDocumentComp(style, inst,
				(xsltTransformFunction) xsltDocumentElem);
	} else {
	    xsltTransformError(NULL, style, inst,
		 "xsltStylePreCompute: unknown xsl:%s\n", inst->name);
	    if (style != NULL) style->warnings++;
	}
	/*
	 * Add the namespace lookup here, this code can be shared by
	 * all precomputations.
	 */
	cur = (xsltStylePreCompPtr) inst->psvi;
	if (cur != NULL) {
	    int i = 0;

	    cur->nsList = xmlGetNsList(inst->doc, inst);
            if (cur->nsList != NULL) {
		while (cur->nsList[i] != NULL)
		    i++;
	    }
	    cur->nsNr = i;
	}
    } else {
	inst->psvi =
	    (void *) xsltPreComputeExtModuleElement(style, inst);

	/*
	 * Unknown element, maybe registered at the context
	 * level. Mark it for later recognition.
	 */
	if (inst->psvi == NULL)
	    inst->psvi = (void *) xsltExtMarker;
    }
}
