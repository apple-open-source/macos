/*
 * xslt.c: Implemetation of an XSL Transformation 1.0 engine
 *
 * Reference:
 *   XSLT specification
 *   http://www.w3.org/TR/1999/REC-xslt-19991116
 *
 *   Associating Style Sheets with XML documents
 *   http://www.w3.org/1999/06/REC-xml-stylesheet-19990629
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
#include <libxml/xmlerror.h>
#include <libxml/parserInternals.h>
#include <libxml/xpathInternals.h>
#include "xslt.h"
#include "xsltInternals.h"
#include "pattern.h"
#include "variables.h"
#include "namespaces.h"
#include "attributes.h"
#include "xsltutils.h"
#include "imports.h"
#include "keys.h"
#include "documents.h"
#include "extensions.h"
#include "preproc.h"
#include "extra.h"
#include "security.h"

#ifdef WITH_XSLT_DEBUG
#define WITH_XSLT_DEBUG_PARSING
/* #define WITH_XSLT_DEBUG_BLANKS */
#endif

const char *xsltEngineVersion = LIBXSLT_VERSION_STRING LIBXSLT_VERSION_EXTRA;
const int xsltLibxsltVersion = LIBXSLT_VERSION;
const int xsltLibxmlVersion = LIBXML_VERSION;

/*
 * Harmless but avoiding a problem when compiling against a
 * libxml <= 2.3.11 without LIBXML_DEBUG_ENABLED
 */
#ifndef LIBXML_DEBUG_ENABLED
double xmlXPathStringEvalNumber(const xmlChar *str);
#endif
/*
 * Useful macros
 */

#ifdef  IS_BLANK
#undef	IS_BLANK
#endif
#define IS_BLANK(c) (((c) == 0x20) || ((c) == 0x09) || ((c) == 0xA) ||	\
                     ((c) == 0x0D))

#ifdef	IS_BLANK_NODE
#undef	IS_BLANK_NODE
#endif
#define IS_BLANK_NODE(n)						\
    (((n)->type == XML_TEXT_NODE) && (xsltIsBlank((n)->content)))

/**
 * exclPrefixPush:
 * @style: the transformation stylesheet
 * @value:  the excluded prefix to push on the stack
 *
 * Push an excluded prefix on the stack
 *
 * Returns the new index in the stack or 0 in case of error
 */
static int
exclPrefixPush(xsltStylesheetPtr style, xmlChar * value)
{
    if (style->exclPrefixMax == 0) {
        style->exclPrefixMax = 4;
        style->exclPrefixTab =
            (xmlChar * *)xmlMalloc(style->exclPrefixMax *
                                   sizeof(style->exclPrefixTab[0]));
        if (style->exclPrefixTab == NULL) {
            xmlGenericError(xmlGenericErrorContext, "malloc failed !\n");
            return (0);
        }
    }
    if (style->exclPrefixNr >= style->exclPrefixMax) {
        style->exclPrefixMax *= 2;
        style->exclPrefixTab =
            (xmlChar * *)xmlRealloc(style->exclPrefixTab,
                                    style->exclPrefixMax *
                                    sizeof(style->exclPrefixTab[0]));
        if (style->exclPrefixTab == NULL) {
            xmlGenericError(xmlGenericErrorContext, "realloc failed !\n");
            return (0);
        }
    }
    style->exclPrefixTab[style->exclPrefixNr] = value;
    style->exclPrefix = value;
    return (style->exclPrefixNr++);
}
/**
 * exclPrefixPop:
 * @style: the transformation stylesheet
 *
 * Pop an excluded prefix value from the stack
 *
 * Returns the stored excluded prefix value
 */
static xmlChar *
exclPrefixPop(xsltStylesheetPtr style)
{
    xmlChar *ret;

    if (style->exclPrefixNr <= 0)
        return (0);
    style->exclPrefixNr--;
    if (style->exclPrefixNr > 0)
        style->exclPrefix = style->exclPrefixTab[style->exclPrefixNr - 1];
    else
        style->exclPrefix = NULL;
    ret = style->exclPrefixTab[style->exclPrefixNr];
    style->exclPrefixTab[style->exclPrefixNr] = 0;
    return (ret);
}

/************************************************************************
 *									*
 *			Helper functions				*
 *									*
 ************************************************************************/

/**
 * xsltInit:
 *
 * Initializes the processor (e.g. registers built-in extensions,
 * etc.)
 */
static void
xsltInit (void) {
    static int initialized = 0;

    if (initialized == 0) {
	initialized = 1;
	xsltRegisterAllExtras();
    }
}

/**
 * xsltIsBlank:
 * @str:  a string
 *
 * Check if a string is ignorable
 *
 * Returns 1 if the string is NULL or made of blanks chars, 0 otherwise
 */
int
xsltIsBlank(xmlChar *str) {
    if (str == NULL)
	return(1);
    while (*str != 0) {
	if (!(IS_BLANK(*str))) return(0);
	str++;
    }
    return(1);
}

/************************************************************************
 *									*
 *		Routines to handle XSLT data structures			*
 *									*
 ************************************************************************/
static xsltDecimalFormatPtr
xsltNewDecimalFormat(xmlChar *name)
{
    xsltDecimalFormatPtr self;

    self = xmlMalloc(sizeof(xsltDecimalFormat));
    if (self != NULL) {
	self->next = NULL;
	self->name = name;
	
	/* Default values */
	self->digit = xmlStrdup(BAD_CAST("#"));
	self->patternSeparator = xmlStrdup(BAD_CAST(";"));
	self->decimalPoint = xmlStrdup(BAD_CAST("."));
	self->grouping = xmlStrdup(BAD_CAST(","));
	self->percent = xmlStrdup(BAD_CAST("%"));
	self->permille = xmlStrdup(BAD_CAST("?"));
	self->zeroDigit = xmlStrdup(BAD_CAST("0"));
	self->minusSign = xmlStrdup(BAD_CAST("-"));
	self->infinity = xmlStrdup(BAD_CAST("Infinity"));
	self->noNumber = xmlStrdup(BAD_CAST("NaN"));
    }
    return self;
}

static void
xsltFreeDecimalFormat(xsltDecimalFormatPtr self)
{
    if (self != NULL) {
	if (self->digit)
	    xmlFree(self->digit);
	if (self->patternSeparator)
	    xmlFree(self->patternSeparator);
	if (self->decimalPoint)
	    xmlFree(self->decimalPoint);
	if (self->grouping)
	    xmlFree(self->grouping);
	if (self->percent)
	    xmlFree(self->percent);
	if (self->permille)
	    xmlFree(self->permille);
	if (self->zeroDigit)
	    xmlFree(self->zeroDigit);
	if (self->minusSign)
	    xmlFree(self->minusSign);
	if (self->infinity)
	    xmlFree(self->infinity);
	if (self->noNumber)
	    xmlFree(self->noNumber);
	if (self->name)
	    xmlFree(self->name);
	xmlFree(self);
    }
}

static void
xsltFreeDecimalFormatList(xsltStylesheetPtr self)
{
    xsltDecimalFormatPtr iter;
    xsltDecimalFormatPtr tmp;

    if (self == NULL)
	return;
    
    iter = self->decimalFormat;
    while (iter != NULL) {
	tmp = iter->next;
	xsltFreeDecimalFormat(iter);
	iter = tmp;
    }
}

/**
 * xsltDecimalFormatGetByName:
 * @sheet: the XSLT stylesheet
 * @name: the decimal-format name to find
 *
 * Find decimal-format by name
 *
 * Returns the xsltDecimalFormatPtr
 */
xsltDecimalFormatPtr
xsltDecimalFormatGetByName(xsltStylesheetPtr sheet, xmlChar *name)
{
    xsltDecimalFormatPtr result = NULL;

    if (name == NULL)
	return sheet->decimalFormat;

    while (sheet != NULL) {
	for (result = sheet->decimalFormat->next;
	     result != NULL;
	     result = result->next) {
	    if (xmlStrEqual(name, result->name))
		return result;
	}
	sheet = xsltNextImport(sheet);
    }
    return result;
}


/**
 * xsltNewTemplate:
 *
 * Create a new XSLT Template
 *
 * Returns the newly allocated xsltTemplatePtr or NULL in case of error
 */
static xsltTemplatePtr
xsltNewTemplate(void) {
    xsltTemplatePtr cur;

    cur = (xsltTemplatePtr) xmlMalloc(sizeof(xsltTemplate));
    if (cur == NULL) {
	xsltTransformError(NULL, NULL, NULL,
		"xsltNewTemplate : malloc failed\n");
	return(NULL);
    }
    memset(cur, 0, sizeof(xsltTemplate));
    cur->priority = XSLT_PAT_NO_PRIORITY;
    return(cur);
}

/**
 * xsltFreeTemplate:
 * @template:  an XSLT template
 *
 * Free up the memory allocated by @template
 */
static void
xsltFreeTemplate(xsltTemplatePtr template) {
    if (template == NULL)
	return;
    if (template->match) xmlFree(template->match);
    if (template->name) xmlFree(template->name);
    if (template->nameURI) xmlFree(template->nameURI);
    if (template->mode) xmlFree(template->mode);
    if (template->modeURI) xmlFree(template->modeURI);
    if (template->inheritedNs) xmlFree(template->inheritedNs);
    memset(template, -1, sizeof(xsltTemplate));
    xmlFree(template);
}

/**
 * xsltFreeTemplateList:
 * @template:  an XSLT template list
 *
 * Free up the memory allocated by all the elements of @template
 */
static void
xsltFreeTemplateList(xsltTemplatePtr template) {
    xsltTemplatePtr cur;

    while (template != NULL) {
	cur = template;
	template = template->next;
	xsltFreeTemplate(cur);
    }
}

/**
 * xsltNewStylesheet:
 *
 * Create a new XSLT Stylesheet
 *
 * Returns the newly allocated xsltStylesheetPtr or NULL in case of error
 */
xsltStylesheetPtr
xsltNewStylesheet(void) {
    xsltStylesheetPtr cur;

    cur = (xsltStylesheetPtr) xmlMalloc(sizeof(xsltStylesheet));
    if (cur == NULL) {
	xsltTransformError(NULL, NULL, NULL,
		"xsltNewStylesheet : malloc failed\n");
	return(NULL);
    }
    memset(cur, 0, sizeof(xsltStylesheet));
    cur->omitXmlDeclaration = -1;
    cur->standalone = -1;
    cur->decimalFormat = xsltNewDecimalFormat(NULL);
    cur->indent = -1;
    cur->errors = 0;
    cur->warnings = 0;
    cur->exclPrefixNr = 0;
    cur->exclPrefixMax = 0;
    cur->exclPrefixTab = NULL;
    cur->extInfos = NULL;
    cur->extrasNr = 0;
    cur->dict = xmlDictCreate();
#ifdef WITH_XSLT_DEBUG
    xsltGenericDebug(xsltGenericDebugContext,
                     "creating dictionary for stylesheet\n");
#endif

    xsltInit();

    return(cur);
}

/**
 * xsltAllocateExtra:
 * @style:  an XSLT stylesheet
 *
 * Allocate an extra runtime information slot statically while compiling
 * the stylesheet and return its number
 *
 * Returns the number of the slot
 */
int
xsltAllocateExtra(xsltStylesheetPtr style)
{
    return(style->extrasNr++);
}

/**
 * xsltAllocateExtraCtxt:
 * @ctxt:  an XSLT transformation context
 *
 * Allocate an extra runtime information slot at run-time
 * and return its number
 * This make sure there is a slot ready in the transformation context
 *
 * Returns the number of the slot
 */
int
xsltAllocateExtraCtxt(xsltTransformContextPtr ctxt)
{
    if (ctxt->extrasNr >= ctxt->extrasMax) {
	int i;
	if (ctxt->extrasNr == 0) {
	    ctxt->extrasMax = 20;
	    ctxt->extras = (xsltRuntimeExtraPtr)
		xmlMalloc(ctxt->extrasMax * sizeof(xsltRuntimeExtra));
	    if (ctxt->extras == NULL) {
		xmlGenericError(xmlGenericErrorContext,
			"xsltAllocateExtraCtxt: out of memory\n");
		ctxt->state = XSLT_STATE_ERROR;
		return(0);
	    }
	    for (i = 0;i < ctxt->extrasMax;i++) {
		ctxt->extras[i].info = NULL;
		ctxt->extras[i].deallocate = NULL;
		ctxt->extras[i].val = NULL;
	    }

	} else {
	    xsltRuntimeExtraPtr tmp;

	    ctxt->extrasMax += 100;
	    tmp = (xsltRuntimeExtraPtr) xmlRealloc(ctxt->extras,
		            ctxt->extrasMax * sizeof(xsltRuntimeExtra));
	    if (tmp == NULL) {
		xmlGenericError(xmlGenericErrorContext,
			"xsltAllocateExtraCtxt: out of memory\n");
		ctxt->state = XSLT_STATE_ERROR;
		return(0);
	    }
	    ctxt->extras = tmp;
	    for (i = ctxt->extrasNr;i < ctxt->extrasMax;i++) {
		ctxt->extras[i].info = NULL;
		ctxt->extras[i].deallocate = NULL;
		ctxt->extras[i].val = NULL;
	    }
	}
    }
    return(ctxt->extrasNr++);
}

/**
 * xsltFreeStylesheetList:
 * @sheet:  an XSLT stylesheet list
 *
 * Free up the memory allocated by the list @sheet
 */
static void
xsltFreeStylesheetList(xsltStylesheetPtr sheet) {
    xsltStylesheetPtr next;

    while (sheet != NULL) {
	next = sheet->next;
	xsltFreeStylesheet(sheet);
	sheet = next;
    }
}

/**
 * xsltFreeStylesheet:
 * @sheet:  an XSLT stylesheet
 *
 * Free up the memory allocated by @sheet
 */
void
xsltFreeStylesheet(xsltStylesheetPtr sheet)
{
    if (sheet == NULL)
        return;

    xsltFreeKeys(sheet);
    xsltFreeExts(sheet);
    xsltFreeTemplateHashes(sheet);
    xsltFreeDecimalFormatList(sheet);
    xsltFreeTemplateList(sheet->templates);
    xsltFreeAttributeSetsHashes(sheet);
    xsltFreeNamespaceAliasHashes(sheet);
    xsltFreeStyleDocuments(sheet);
    xsltFreeStylePreComps(sheet);
    xsltShutdownExts(sheet);
    if (sheet->doc != NULL)
        xmlFreeDoc(sheet->doc);
    if (sheet->variables != NULL)
        xsltFreeStackElemList(sheet->variables);
    if (sheet->cdataSection != NULL)
        xmlHashFree(sheet->cdataSection, NULL);
    if (sheet->stripSpaces != NULL)
        xmlHashFree(sheet->stripSpaces, NULL);
    if (sheet->nsHash != NULL)
        xmlHashFree(sheet->nsHash, NULL);

    if (sheet->exclPrefixTab != NULL)
        xmlFree(sheet->exclPrefixTab);
    if (sheet->method != NULL)
        xmlFree(sheet->method);
    if (sheet->methodURI != NULL)
        xmlFree(sheet->methodURI);
    if (sheet->version != NULL)
        xmlFree(sheet->version);
    if (sheet->encoding != NULL)
        xmlFree(sheet->encoding);
    if (sheet->doctypePublic != NULL)
        xmlFree(sheet->doctypePublic);
    if (sheet->doctypeSystem != NULL)
        xmlFree(sheet->doctypeSystem);
    if (sheet->mediaType != NULL)
        xmlFree(sheet->mediaType);
    if (sheet->attVTs)
        xsltFreeAVTList(sheet->attVTs);

    if (sheet->imports != NULL)
        xsltFreeStylesheetList(sheet->imports);

#ifdef WITH_XSLT_DEBUG
    xsltGenericDebug(xsltGenericDebugContext,
                     "freeing dictionary from stylesheet\n");
#endif
    xmlDictFree(sheet->dict);

    memset(sheet, -1, sizeof(xsltStylesheet));
    xmlFree(sheet);
}

/************************************************************************
 *									*
 *		Parsing of an XSLT Stylesheet				*
 *									*
 ************************************************************************/

/**
 * xsltGetInheritedNsList:
 * @style:  the stylesheet
 * @template: the template
 * @node:  the current node
 *
 * Search all the namespace applying to a given element except the ones 
 * from excluded output prefixes currently in scope. Initialize the
 * template inheritedNs list with it.
 *
 * Returns the number of entries found
 */
static int
xsltGetInheritedNsList(xsltStylesheetPtr style,
	               xsltTemplatePtr template,
	               xmlNodePtr node)
{
    xmlNsPtr cur;
    xmlNsPtr *ret = NULL;
    int nbns = 0;
    int maxns = 10;
    int i;

    if ((style == NULL) || (template == NULL) || (node == NULL) ||
	(template->inheritedNsNr != 0) || (template->inheritedNs != NULL))
	return(0);
    while (node != NULL) {
        if (node->type == XML_ELEMENT_NODE) {
            cur = node->nsDef;
            while (cur != NULL) {
		if (xmlStrEqual(cur->href, XSLT_NAMESPACE))
		    goto skip_ns;
		if ((cur->prefix != NULL) &&
		    (xsltCheckExtPrefix(style, cur->prefix)))
		    goto skip_ns;
		for (i = 0;i < style->exclPrefixNr;i++) {
		    if (xmlStrEqual(cur->href, style->exclPrefixTab[i]))
			goto skip_ns;
		}
                if (ret == NULL) {
                    ret =
                        (xmlNsPtr *) xmlMalloc((maxns + 1) *
                                               sizeof(xmlNsPtr));
                    if (ret == NULL) {
                        xmlGenericError(xmlGenericErrorContext,
                                        "xsltGetInheritedNsList : out of memory!\n");
                        return(0);
                    }
                    ret[nbns] = NULL;
                }
                for (i = 0; i < nbns; i++) {
                    if ((cur->prefix == ret[i]->prefix) ||
                        (xmlStrEqual(cur->prefix, ret[i]->prefix)))
                        break;
                }
                if (i >= nbns) {
                    if (nbns >= maxns) {
                        maxns *= 2;
                        ret = (xmlNsPtr *) xmlRealloc(ret,
                                                      (maxns +
                                                       1) *
                                                      sizeof(xmlNsPtr));
                        if (ret == NULL) {
                            xmlGenericError(xmlGenericErrorContext,
                                            "xsltGetInheritedNsList : realloc failed!\n");
                            return(0);
                        }
                    }
                    ret[nbns++] = cur;
                    ret[nbns] = NULL;
                }
skip_ns:
                cur = cur->next;
            }
        }
        node = node->parent;
    }
    if (nbns != 0) {
#ifdef WITH_XSLT_DEBUG_PARSING
        xsltGenericDebug(xsltGenericDebugContext,
                         "template has %d inherited namespaces\n", nbns);
#endif
	template->inheritedNsNr = nbns;
	template->inheritedNs = ret;
    }
    return (nbns);
}

/**
 * xsltParseStylesheetOutput:
 * @style:  the XSLT stylesheet
 * @cur:  the "output" element
 *
 * parse an XSLT stylesheet output element and record
 * information related to the stylesheet output
 */

void
xsltParseStylesheetOutput(xsltStylesheetPtr style, xmlNodePtr cur)
{
    xmlChar *elements,
     *prop;
    xmlChar *element,
     *end;

    if ((cur == NULL) || (style == NULL))
        return;

    prop = xsltGetNsProp(cur, (const xmlChar *) "version", XSLT_NAMESPACE);
    if (prop != NULL) {
        if (style->version != NULL)
            xmlFree(style->version);
        style->version = prop;
    }

    prop =
        xsltGetNsProp(cur, (const xmlChar *) "encoding", XSLT_NAMESPACE);
    if (prop != NULL) {
        if (style->encoding != NULL)
            xmlFree(style->encoding);
        style->encoding = prop;
    }

    /* relaxed to support xt:document */
    prop = xmlGetProp(cur, (const xmlChar *) "method");
    if (prop != NULL) {
        const xmlChar *URI;

        if (style->method != NULL)
            xmlFree(style->method);
        style->method = NULL;
        if (style->methodURI != NULL)
            xmlFree(style->methodURI);
        style->methodURI = NULL;

	URI = xsltGetQNameURI(cur, &prop);
	if (prop == NULL) {
	    if (style != NULL) style->errors++;
	} else if (URI == NULL) {
            if ((xmlStrEqual(prop, (const xmlChar *) "xml")) ||
                (xmlStrEqual(prop, (const xmlChar *) "html")) ||
                (xmlStrEqual(prop, (const xmlChar *) "text"))) {
                style->method = prop;
            } else {
		xsltTransformError(NULL, style, cur,
                                 "invalid value for method: %s\n", prop);
                if (style != NULL) style->warnings++;
            }
	} else {
	    style->method = prop;
	    style->methodURI = xmlStrdup(URI);
	}
    }

    prop =
        xsltGetNsProp(cur, (const xmlChar *) "doctype-system",
                      XSLT_NAMESPACE);
    if (prop != NULL) {
        if (style->doctypeSystem != NULL)
            xmlFree(style->doctypeSystem);
        style->doctypeSystem = prop;
    }

    prop =
        xsltGetNsProp(cur, (const xmlChar *) "doctype-public",
                      XSLT_NAMESPACE);
    if (prop != NULL) {
        if (style->doctypePublic != NULL)
            xmlFree(style->doctypePublic);
        style->doctypePublic = prop;
    }

    prop = xsltGetNsProp(cur, (const xmlChar *) "standalone",
                         XSLT_NAMESPACE);
    if (prop != NULL) {
        if (xmlStrEqual(prop, (const xmlChar *) "yes")) {
            style->standalone = 1;
        } else if (xmlStrEqual(prop, (const xmlChar *) "no")) {
            style->standalone = 0;
        } else {
	    xsltTransformError(NULL, style, cur,
                             "invalid value for standalone: %s\n", prop);
            if (style != NULL) style->warnings++;
        }
        xmlFree(prop);
    }

    prop = xsltGetNsProp(cur, (const xmlChar *) "indent", XSLT_NAMESPACE);
    if (prop != NULL) {
        if (xmlStrEqual(prop, (const xmlChar *) "yes")) {
            style->indent = 1;
        } else if (xmlStrEqual(prop, (const xmlChar *) "no")) {
            style->indent = 0;
        } else {
	    xsltTransformError(NULL, style, cur,
                             "invalid value for indent: %s\n", prop);
            if (style != NULL) style->warnings++;
        }
        xmlFree(prop);
    }

    prop = xsltGetNsProp(cur, (const xmlChar *) "omit-xml-declaration",
                         XSLT_NAMESPACE);
    if (prop != NULL) {
        if (xmlStrEqual(prop, (const xmlChar *) "yes")) {
            style->omitXmlDeclaration = 1;
        } else if (xmlStrEqual(prop, (const xmlChar *) "no")) {
            style->omitXmlDeclaration = 0;
        } else {
	    xsltTransformError(NULL, style, cur,
                             "invalid value for omit-xml-declaration: %s\n",
                             prop);
            if (style != NULL) style->warnings++;
        }
        xmlFree(prop);
    }

    elements =
        xsltGetNsProp(cur, (const xmlChar *) "cdata-section-elements",
                      XSLT_NAMESPACE);
    if (elements != NULL) {
        if (style->cdataSection == NULL)
            style->cdataSection = xmlHashCreate(10);
        if (style->cdataSection == NULL)
            return;

        element = elements;
        while (*element != 0) {
            while (IS_BLANK(*element))
                element++;
            if (*element == 0)
                break;
            end = element;
            while ((*end != 0) && (!IS_BLANK(*end)))
                end++;
            element = xmlStrndup(element, end - element);
            if (element) {
		const xmlChar *URI;
#ifdef WITH_XSLT_DEBUG_PARSING
                xsltGenericDebug(xsltGenericDebugContext,
                                 "add cdata section output element %s\n",
                                 element);
#endif

		URI = xsltGetQNameURI(cur, &element);
		if (element == NULL) {
		    if (style != NULL) style->errors++;
		} else {
		    xmlNsPtr ns;
		    
		    xmlHashAddEntry2(style->cdataSection, element, URI,
			             (void *) "cdata");
		    /*
		     * if prefix is NULL, we must check whether it's
		     * necessary to also put in the name of the default
		     * namespace.
		     */
		    if (URI == NULL) {
		        ns = xmlSearchNs(style->doc, cur, NULL);
			if (ns != NULL)  
			    xmlHashAddEntry2(style->cdataSection, element,
			        ns->href, (void *) "cdata");
		    }
		    xmlFree(element);
		}
            }
            element = end;
        }
        xmlFree(elements);
    }

    prop = xsltGetNsProp(cur, (const xmlChar *) "media-type", XSLT_NAMESPACE);
    if (prop != NULL) {
	if (style->mediaType)
	    xmlFree(style->mediaType);
	style->mediaType = prop;
    }
}

/**
 * xsltParseStylesheetDecimalFormat:
 * @style:  the XSLT stylesheet
 * @cur:  the "decimal-format" element
 *
 * parse an XSLT stylesheet decimal-format element and
 * and record the formatting characteristics
 */
static void
xsltParseStylesheetDecimalFormat(xsltStylesheetPtr style, xmlNodePtr cur)
{
    xmlChar *prop;
    xsltDecimalFormatPtr format;
    xsltDecimalFormatPtr iter;
    
    if ((cur == NULL) || (style == NULL))
	return;

    format = style->decimalFormat;
    
    prop = xsltGetNsProp(cur, BAD_CAST("name"), XSLT_NAMESPACE);
    if (prop != NULL) {
	format = xsltDecimalFormatGetByName(style, prop);
	if (format != NULL) {
	    xsltTransformError(NULL, style, cur,
	 "xsltParseStylestyleDecimalFormat: %s already exists\n", prop);
	    if (style != NULL) style->warnings++;
	    return;
	}
	format = xsltNewDecimalFormat(prop);
	if (format == NULL) {
	    xsltTransformError(NULL, style, cur,
     "xsltParseStylestyleDecimalFormat: failed creating new decimal-format\n");
	    if (style != NULL) style->errors++;
	    return;
	}
	/* Append new decimal-format structure */
	for (iter = style->decimalFormat; iter->next; iter = iter->next)
	    ;
	if (iter)
	    iter->next = format;
    }

    prop = xsltGetNsProp(cur, (const xmlChar *)"decimal-separator",
	                XSLT_NAMESPACE);
    if (prop != NULL) {
	if (format->decimalPoint != NULL) xmlFree(format->decimalPoint);
	format->decimalPoint  = prop;
    }
    
    prop = xsltGetNsProp(cur, (const xmlChar *)"grouping-separator",
	                XSLT_NAMESPACE);
    if (prop != NULL) {
	if (format->grouping != NULL) xmlFree(format->grouping);
	format->grouping  = prop;
    }

    prop = xsltGetNsProp(cur, (const xmlChar *)"infinity", XSLT_NAMESPACE);
    if (prop != NULL) {
	if (format->infinity != NULL) xmlFree(format->infinity);
	format->infinity  = prop;
    }
    
    prop = xsltGetNsProp(cur, (const xmlChar *)"minus-sign", XSLT_NAMESPACE);
    if (prop != NULL) {
	if (format->minusSign != NULL) xmlFree(format->minusSign);
	format->minusSign  = prop;
    }
    
    prop = xsltGetNsProp(cur, (const xmlChar *)"NaN", XSLT_NAMESPACE);
    if (prop != NULL) {
	if (format->noNumber != NULL) xmlFree(format->noNumber);
	format->noNumber  = prop;
    }
    
    prop = xsltGetNsProp(cur, (const xmlChar *)"percent", XSLT_NAMESPACE);
    if (prop != NULL) {
	if (format->percent != NULL) xmlFree(format->percent);
	format->percent  = prop;
    }
    
    prop = xsltGetNsProp(cur, (const xmlChar *)"per-mille", XSLT_NAMESPACE);
    if (prop != NULL) {
	if (format->permille != NULL) xmlFree(format->permille);
	format->permille  = prop;
    }
    
    prop = xsltGetNsProp(cur, (const xmlChar *)"zero-digit", XSLT_NAMESPACE);
    if (prop != NULL) {
	if (format->zeroDigit != NULL) xmlFree(format->zeroDigit);
	format->zeroDigit  = prop;
    }
    
    prop = xsltGetNsProp(cur, (const xmlChar *)"digit", XSLT_NAMESPACE);
    if (prop != NULL) {
	if (format->digit != NULL) xmlFree(format->digit);
	format->digit  = prop;
    }
    
    prop = xsltGetNsProp(cur, (const xmlChar *)"pattern-separator",
	                XSLT_NAMESPACE);
    if (prop != NULL) {
	if (format->patternSeparator != NULL) xmlFree(format->patternSeparator);
	format->patternSeparator  = prop;
    }
}

/**
 * xsltParseStylesheetPreserveSpace:
 * @style:  the XSLT stylesheet
 * @cur:  the "preserve-space" element
 *
 * parse an XSLT stylesheet preserve-space element and record
 * elements needing preserving
 */

static void
xsltParseStylesheetPreserveSpace(xsltStylesheetPtr style, xmlNodePtr cur) {
    xmlChar *elements;
    xmlChar *element, *end;

    if ((cur == NULL) || (style == NULL))
	return;

    elements = xsltGetNsProp(cur, (const xmlChar *)"elements", XSLT_NAMESPACE);
    if (elements == NULL) {
	xsltTransformError(NULL, style, cur,
	    "xsltParseStylesheetPreserveSpace: missing elements attribute\n");
	if (style != NULL) style->warnings++;
	return;
    }

    if (style->stripSpaces == NULL)
	style->stripSpaces = xmlHashCreate(10);
    if (style->stripSpaces == NULL)
	return;

    element = elements;
    while (*element != 0) {
	while (IS_BLANK(*element)) element++;
	if (*element == 0)
	    break;
        end = element;
	while ((*end != 0) && (!IS_BLANK(*end))) end++;
	element = xmlStrndup(element, end - element);
	if (element) {
#ifdef WITH_XSLT_DEBUG_PARSING
	    xsltGenericDebug(xsltGenericDebugContext,
		"add preserved space element %s\n", element);
#endif
	    if (xmlStrEqual(element, (const xmlChar *)"*")) {
		style->stripAll = -1;
	    } else {
		const xmlChar *URI;

                URI = xsltGetQNameURI(cur, &element);

		xmlHashAddEntry2(style->stripSpaces, element, URI,
				(xmlChar *) "preserve");
	    }
	    xmlFree(element);
	}
	element = end;
    }
    xmlFree(elements);
}

/**
 * xsltParseStylesheetExtPrefix:
 * @style:  the XSLT stylesheet
 * @template:  the "extension-element-prefixes" prefix
 *
 * parse an XSLT stylesheet extension prefix and record
 * prefixes needing stripping
 */

static void
xsltParseStylesheetExtPrefix(xsltStylesheetPtr style, xmlNodePtr cur) {
    xmlChar *prefixes;
    xmlChar *prefix, *end;

    if ((cur == NULL) || (style == NULL))
	return;

    prefixes = xsltGetNsProp(cur, (const xmlChar *)"extension-element-prefixes",
	                    XSLT_NAMESPACE);
    if (prefixes == NULL) {
	return;
    }

    prefix = prefixes;
    while (*prefix != 0) {
	while (IS_BLANK(*prefix)) prefix++;
	if (*prefix == 0)
	    break;
        end = prefix;
	while ((*end != 0) && (!IS_BLANK(*end))) end++;
	prefix = xmlStrndup(prefix, end - prefix);
	if (prefix) {
	    xmlNsPtr ns;

	    if (xmlStrEqual(prefix, (const xmlChar *)"#default"))
		ns = xmlSearchNs(style->doc, cur, NULL);
	    else
		ns = xmlSearchNs(style->doc, cur, prefix);
	    if (ns == NULL) {
		xsltTransformError(NULL, style, cur,
	    "xsl:extension-element-prefix : undefined namespace %s\n",
	                         prefix);
		if (style != NULL) style->warnings++;
	    } else {
#ifdef WITH_XSLT_DEBUG_PARSING
		xsltGenericDebug(xsltGenericDebugContext,
		    "add extension prefix %s\n", prefix);
#endif
		xsltRegisterExtPrefix(style, prefix, ns->href);
	    }
	    xmlFree(prefix);
	}
	prefix = end;
    }
    xmlFree(prefixes);
}

/**
 * xsltParseStylesheetStripSpace:
 * @style:  the XSLT stylesheet
 * @cur:  the "strip-space" element
 *
 * parse an XSLT stylesheet strip-space element and record
 * elements needing stripping
 */

static void
xsltParseStylesheetStripSpace(xsltStylesheetPtr style, xmlNodePtr cur) {
    xmlChar *elements;
    xmlChar *element, *end;

    if ((cur == NULL) || (style == NULL))
	return;

    elements = xsltGetNsProp(cur, (const xmlChar *)"elements", XSLT_NAMESPACE);
    if (elements == NULL) {
	xsltTransformError(NULL, style, cur,
	    "xsltParseStylesheetStripSpace: missing elements attribute\n");
	if (style != NULL) style->warnings++;
	return;
    }

    if (style->stripSpaces == NULL)
	style->stripSpaces = xmlHashCreate(10);
    if (style->stripSpaces == NULL)
	return;

    element = elements;
    while (*element != 0) {
	while (IS_BLANK(*element)) element++;
	if (*element == 0)
	    break;
        end = element;
	while ((*end != 0) && (!IS_BLANK(*end))) end++;
	element = xmlStrndup(element, end - element);
	if (element) {
#ifdef WITH_XSLT_DEBUG_PARSING
	    xsltGenericDebug(xsltGenericDebugContext,
		"add stripped space element %s\n", element);
#endif
	    if (xmlStrEqual(element, (const xmlChar *)"*")) {
		style->stripAll = 1;
	    } else {
		const xmlChar *URI;

                URI = xsltGetQNameURI(cur, &element);

		xmlHashAddEntry2(style->stripSpaces, element, URI,
			        (xmlChar *) "strip");
	    }
	    xmlFree(element);
	}
	element = end;
    }
    xmlFree(elements);
}

/**
 * xsltParseStylesheetExcludePrefix:
 * @style:  the XSLT stylesheet
 * @cur:  the current point in the stylesheet
 *
 * parse an XSLT stylesheet exclude prefix and record
 * namespaces needing stripping
 *
 * Returns the number of Excluded prefixes added at that level
 */

static int
xsltParseStylesheetExcludePrefix(xsltStylesheetPtr style, xmlNodePtr cur) {
    int nb = 0;
    xmlChar *prefixes;
    xmlChar *prefix, *end;

    if ((cur == NULL) || (style == NULL))
	return(0);

    prefixes = xsltGetNsProp(cur, (const xmlChar *)"exclude-result-prefixes",
	                    XSLT_NAMESPACE);
    if (prefixes == NULL) {
	return(0);
    }

    prefix = prefixes;
    while (*prefix != 0) {
	while (IS_BLANK(*prefix)) prefix++;
	if (*prefix == 0)
	    break;
        end = prefix;
	while ((*end != 0) && (!IS_BLANK(*end))) end++;
	prefix = xmlStrndup(prefix, end - prefix);
	if (prefix) {
	    xmlNsPtr ns;

	    if (xmlStrEqual(prefix, (const xmlChar *)"#default"))
		ns = xmlSearchNs(style->doc, cur, NULL);
	    else
		ns = xmlSearchNs(style->doc, cur, prefix);
	    if (ns == NULL) {
		xsltTransformError(NULL, style, cur,
	    "xsl:exclude-result-prefixes : undefined namespace %s\n",
	                         prefix);
		if (style != NULL) style->warnings++;
	    } else {
#ifdef WITH_XSLT_DEBUG_PARSING
		xsltGenericDebug(xsltGenericDebugContext,
		    "exclude result prefix %s\n", prefix);
#endif
		exclPrefixPush(style, (xmlChar *) ns->href);
		nb++;
	    }
	    xmlFree(prefix);
	}
	prefix = end;
    }
    xmlFree(prefixes);
    return(nb);
}

/**
 * xsltPrecomputeStylesheet:
 * @style:  the XSLT stylesheet
 * @cur:  the current child list
 *
 * Clean-up the stylesheet content from unwanted ignorable blank nodes
 * and run the preprocessing of all XSLT constructs.
 *
 * and process xslt:text
 */
static void
xsltPrecomputeStylesheet(xsltStylesheetPtr style, xmlNodePtr cur) {
    xmlNodePtr delete;

    /*
     * This content comes from the stylesheet
     * For stylesheets, the set of whitespace-preserving
     * element names consists of just xsl:text.
     */
    delete = NULL;
    while (cur != NULL) {
	if (delete != NULL) {
#ifdef WITH_XSLT_DEBUG_BLANKS
	    xsltGenericDebug(xsltGenericDebugContext,
	     "xsltPrecomputeStylesheet: removing ignorable blank node\n");
#endif
	    xmlUnlinkNode(delete);
	    xmlFreeNode(delete);
	    delete = NULL;
	}
	if (cur->type == XML_ELEMENT_NODE) {
	    int exclPrefixes;

	    exclPrefixes = xsltParseStylesheetExcludePrefix(style, cur);
	    if (IS_XSLT_ELEM(cur)) {
		xsltStylePreCompute(style, cur);
		if (IS_XSLT_NAME(cur, "text")) {
		    for (;exclPrefixes > 0;exclPrefixes--)
			exclPrefixPop(style);
		    goto skip_children;
		}
	    }

	    /*
	     * Remove excluded prefixes
	     */
	    if ((cur->nsDef != NULL) && (style->exclPrefixNr > 0)) {
		xmlNsPtr ns = cur->nsDef, prev = NULL, next;
		xmlNodePtr root = NULL;
		int i, moved;

		root = xmlDocGetRootElement(cur->doc);
		if ((root != NULL) && (root != cur)) {
		    while (ns != NULL) {
			moved = 0;
			next = ns->next;
			for (i = 0;i < style->exclPrefixNr;i++) {
			    if ((ns->prefix != NULL) && 
			        (xmlStrEqual(ns->href,
					     style->exclPrefixTab[i]))) {
				/*
				 * Move the namespace definition on the root
				 * element to avoid duplicating it without
				 * loosing it.
				 */
				if (prev == NULL) {
				    cur->nsDef = ns->next;
				} else {
				    prev->next = ns->next;
				}
				ns->next = root->nsDef;
				root->nsDef = ns;
				moved = 1;
				break;
			    }
			}
			if (moved == 0)
			    prev = ns;
			ns = next;
		    }
		}
	    }

	    /*
	     * If we have prefixes locally, recurse and pop them up when
	     * going back
	     */
	    if (exclPrefixes > 0) {
		xsltPrecomputeStylesheet(style, cur->children);
		for (;exclPrefixes > 0;exclPrefixes--)
		    exclPrefixPop(style);
		goto skip_children;
	    }

	} else if (cur->type == XML_TEXT_NODE) {
	    if (IS_BLANK_NODE(cur)) {
		if (xmlNodeGetSpacePreserve(cur) != 1) {
		    delete = cur;
		}
	    }
	} else if ((cur->type != XML_ELEMENT_NODE) &&
		   (cur->type != XML_CDATA_SECTION_NODE)) {
	    delete = cur;
	    goto skip_children;
	}

	/*
	 * Skip to next node
	 */
	if (cur->children != NULL) {
	    if ((cur->children->type != XML_ENTITY_DECL) &&
		(cur->children->type != XML_ENTITY_REF_NODE) &&
		(cur->children->type != XML_ENTITY_NODE)) {
		cur = cur->children;
		continue;
	    }
	}
skip_children:
	if (cur->next != NULL) {
	    cur = cur->next;
	    continue;
	}
	
	do {
	    cur = cur->parent;
	    if (cur == NULL)
		break;
	    if (cur == (xmlNodePtr) style->doc) {
		cur = NULL;
		break;
	    }
	    if (cur->next != NULL) {
		cur = cur->next;
		break;
	    }
	} while (cur != NULL);
    }
    if (delete != NULL) {
#ifdef WITH_XSLT_DEBUG_PARSING
	xsltGenericDebug(xsltGenericDebugContext,
	 "xsltPrecomputeStylesheet: removing ignorable blank node\n");
#endif
	xmlUnlinkNode(delete);
	xmlFreeNode(delete);
	delete = NULL;
    }
}

/**
 * xsltGatherNamespaces:
 * @style:  the XSLT stylesheet
 *
 * Browse the stylesheet and build the namspace hash table which
 * will be used for XPath interpretation. If needed do a bit of normalization
 */

static void
xsltGatherNamespaces(xsltStylesheetPtr style) {
    xmlNodePtr cur;
    const xmlChar *URI;

    /* 
     * TODO: basically if the stylesheet uses the same prefix for different
     *       patterns, well they may be in problem, hopefully they will get
     *       a warning first.
     */
    cur = xmlDocGetRootElement(style->doc);
    while (cur != NULL) {
	if (cur->type == XML_ELEMENT_NODE) {
	    xmlNsPtr ns = cur->nsDef;
	    while (ns != NULL) {
		if (ns->prefix != NULL) {
		    if (style->nsHash == NULL) {
			style->nsHash = xmlHashCreate(10);
			if (style->nsHash == NULL) {
			    xsltTransformError(NULL, style, cur,
		 "xsltGatherNamespaces: failed to create hash table\n");
			    if (style != NULL) style->errors++;
			    return;
			}
		    }
		    URI = xmlHashLookup(style->nsHash, ns->prefix);
		    if ((URI != NULL) && (!xmlStrEqual(URI, ns->href))) {
			xsltTransformError(NULL, style, cur,
	     "Namespaces prefix %s used for multiple namespaces\n",ns->prefix);
			if (style != NULL) style->warnings++;
		    } else if (URI == NULL) {
			xmlHashUpdateEntry(style->nsHash, ns->prefix,
			    (void *) ns->href, (xmlHashDeallocator)xmlFree);

#ifdef WITH_XSLT_DEBUG_PARSING
			xsltGenericDebug(xsltGenericDebugContext,
		 "Added namespace: %s mapped to %s\n", ns->prefix, ns->href);
#endif
		    }
		}
		ns = ns->next;
	    }
	}

	/*
	 * Skip to next node
	 */
	if (cur->children != NULL) {
	    if (cur->children->type != XML_ENTITY_DECL) {
		cur = cur->children;
		continue;
	    }
	}
	if (cur->next != NULL) {
	    cur = cur->next;
	    continue;
	}
	
	do {
	    cur = cur->parent;
	    if (cur == NULL)
		break;
	    if (cur == (xmlNodePtr) style->doc) {
		cur = NULL;
		break;
	    }
	    if (cur->next != NULL) {
		cur = cur->next;
		break;
	    }
	} while (cur != NULL);
    }
}

/**
 * xsltParseTemplateContent:
 * @style:  the XSLT stylesheet
 * @templ:  the container node (can be a document for literal results)
 *
 * parse a template content-model
 * Clean-up the template content from unwanted ignorable blank nodes
 * and process xslt:text
 */

void
xsltParseTemplateContent(xsltStylesheetPtr style, xmlNodePtr templ) {
    xmlNodePtr cur, delete;
    /*
     * This content comes from the stylesheet
     * For stylesheets, the set of whitespace-preserving
     * element names consists of just xsl:text.
     */
    cur = templ->children;
    delete = NULL;
    while (cur != NULL) {
	if (delete != NULL) {
#ifdef WITH_XSLT_DEBUG_BLANKS
	    xsltGenericDebug(xsltGenericDebugContext,
	     "xsltParseTemplateContent: removing text\n");
#endif
	    xmlUnlinkNode(delete);
	    xmlFreeNode(delete);
	    delete = NULL;
	}
	if (IS_XSLT_ELEM(cur)) {
	    if (IS_XSLT_NAME(cur, "text")) {
		if (cur->children != NULL) {
		    xmlChar *prop;
		    xmlNodePtr text = cur->children, next;
		    int noesc = 0;
			
		    prop = xsltGetNsProp(cur,
			    (const xmlChar *)"disable-output-escaping",
					XSLT_NAMESPACE);
		    if (prop != NULL) {
#ifdef WITH_XSLT_DEBUG_PARSING
			xsltGenericDebug(xsltGenericDebugContext,
			     "Disable escaping: %s\n", text->content);
#endif
			if (xmlStrEqual(prop, (const xmlChar *)"yes")) {
			    noesc = 1;
			} else if (!xmlStrEqual(prop,
						(const xmlChar *)"no")){
			    xsltTransformError(NULL, style, cur,
	     "xsl:text: disable-output-escaping allows only yes or no\n");
			    if (style != NULL) style->warnings++;

			}
			xmlFree(prop);
		    }

		    while (text != NULL) {
			if (text->type == XML_COMMENT_NODE) {
			    text = text->next;
			    continue;
			}
			if ((text->type != XML_TEXT_NODE) &&
			     (text->type != XML_CDATA_SECTION_NODE)) {
			    xsltTransformError(NULL, style, cur,
		 "xsltParseTemplateContent: xslt:text content problem\n");
			    if (style != NULL) style->errors++;
			    break;
			}
			if ((noesc) && (text->type != XML_CDATA_SECTION_NODE))
			    text->name = xmlStringTextNoenc;
			text = text->next;
		    }

		    /*
		     * replace xsl:text by the list of childs
		     */
		    if (text == NULL) {
			text = cur->children;
			while (text != NULL) {
			    next = text->next;
			    xmlUnlinkNode(text);
			    xmlAddPrevSibling(cur, text);
			    text = next;
			}
		    }
		}
		delete = cur;
		goto skip_children;
	    }
	} else if ((cur->ns != NULL) && (style->nsDefs != NULL) &&
	           (xsltCheckExtPrefix(style, cur->ns->prefix))) {
	    /*
	     * okay this is an extension element compile it too
	     */
	    xsltStylePreCompute(style, cur);
	} else {
	    /*
	     * This is an element which will be output as part of the
	     * template exectution, precompile AVT if found.
	     */
	    if ((cur->ns == NULL) && (style->defaultAlias != NULL) &&
	    		(cur->type == XML_ELEMENT_NODE)) {
		cur->ns = xmlSearchNsByHref(cur->doc, cur,
			style->defaultAlias);
	    }
	    if (cur->properties != NULL) {
	        xmlAttrPtr attr = cur->properties;

		while (attr != NULL) {
		    xsltCompileAttr(style, attr);
		    attr = attr->next;
		}
	    }
	}

	/*
	 * Skip to next node
	 */
	if (cur->children != NULL) {
	    if (cur->children->type != XML_ENTITY_DECL) {
		cur = cur->children;
		continue;
	    }
	}
skip_children:
	if (cur->next != NULL) {
	    cur = cur->next;
	    continue;
	}
	
	do {
	    cur = cur->parent;
	    if (cur == NULL)
		break;
	    if (cur == templ) {
		cur = NULL;
		break;
	    }
	    if (cur->next != NULL) {
		cur = cur->next;
		break;
	    }
	} while (cur != NULL);
    }
    if (delete != NULL) {
#ifdef WITH_XSLT_DEBUG_PARSING
	xsltGenericDebug(xsltGenericDebugContext,
	 "xsltParseTemplateContent: removing text\n");
#endif
	xmlUnlinkNode(delete);
	xmlFreeNode(delete);
	delete = NULL;
    }

    /*
     * Skip the first params
     */
    cur = templ->children;
    while (cur != NULL) {
	if ((IS_XSLT_ELEM(cur)) && (!(IS_XSLT_NAME(cur, "param"))))
	    break;
	cur = cur->next;
    }

    /*
     * Browse the remainder of the template
     */
    while (cur != NULL) {
	if ((IS_XSLT_ELEM(cur)) && (IS_XSLT_NAME(cur, "param"))) {
	    xmlNodePtr param = cur;

	    xsltTransformError(NULL, style, cur,
		"xsltParseTemplateContent: ignoring misplaced param element\n");
	    if (style != NULL) style->warnings++;
            cur = cur->next;
	    xmlUnlinkNode(param);
	    xmlFreeNode(param);
	} else
	    break;
    }
}

/**
 * xsltParseStylesheetKey:
 * @style:  the XSLT stylesheet
 * @key:  the "key" element
 *
 * parse an XSLT stylesheet key definition and register it
 */

static void
xsltParseStylesheetKey(xsltStylesheetPtr style, xmlNodePtr key) {
    xmlChar *prop = NULL;
    xmlChar *use = NULL;
    xmlChar *match = NULL;
    xmlChar *name = NULL;
    xmlChar *nameURI = NULL;

    if (key == NULL)
	return;

    /*
     * Get arguments
     */
    prop = xsltGetNsProp(key, (const xmlChar *)"name", XSLT_NAMESPACE);
    if (prop != NULL) {
        const xmlChar *URI;

	URI = xsltGetQNameURI(key, &prop);
	if (prop == NULL) {
	    if (style != NULL) style->errors++;
	    goto error;
	} else {
	    name = prop;
	    if (URI != NULL)
		nameURI = xmlStrdup(URI);
	}
#ifdef WITH_XSLT_DEBUG_PARSING
	xsltGenericDebug(xsltGenericDebugContext,
	     "xsltParseStylesheetKey: name %s\n", name);
#endif
    } else {
	xsltTransformError(NULL, style, key,
	    "xsl:key : error missing name\n");
	if (style != NULL) style->errors++;
	goto error;
    }

    match = xsltGetNsProp(key, (const xmlChar *)"match", XSLT_NAMESPACE);
    if (match == NULL) {
	xsltTransformError(NULL, style, key,
	    "xsl:key : error missing match\n");
	if (style != NULL) style->errors++;
	goto error;
    }

    use = xsltGetNsProp(key, (const xmlChar *)"use", XSLT_NAMESPACE);
    if (use == NULL) {
	xsltTransformError(NULL, style, key,
	    "xsl:key : error missing use\n");
	if (style != NULL) style->errors++;
	goto error;
    }

    /*
     * register the keys
     */
    xsltAddKey(style, name, nameURI, match, use, key);


error:
    if (use != NULL)
	xmlFree(use);
    if (match != NULL)
	xmlFree(match);
    if (name != NULL)
	xmlFree(name);
    if (nameURI != NULL)
	xmlFree(nameURI);
}

/**
 * xsltParseStylesheetTemplate:
 * @style:  the XSLT stylesheet
 * @template:  the "template" element
 *
 * parse an XSLT stylesheet template building the associated structures
 */

static void
xsltParseStylesheetTemplate(xsltStylesheetPtr style, xmlNodePtr template) {
    xsltTemplatePtr ret;
    xmlChar *prop;
    xmlChar *mode = NULL;
    xmlChar *modeURI = NULL;
    double  priority;
    int exclPrefixes;

    if (template == NULL)
	return;

    /*
     * Create and link the structure
     */
    ret = xsltNewTemplate();
    if (ret == NULL)
	return;
    ret->next = style->templates;
    style->templates = ret;
    ret->style = style;

    /*
     * Check excluded prefixes
     */
    exclPrefixes = xsltParseStylesheetExcludePrefix(style, template);

    /*
     * Get inherited namespaces
     */
    xsltGetInheritedNsList(style, ret, template);

    /*
     * Get arguments
     */
    prop = xsltGetNsProp(template, (const xmlChar *)"mode", XSLT_NAMESPACE);
    if (prop != NULL) {
        const xmlChar *URI;

	URI = xsltGetQNameURI(template, &prop);
	if (prop == NULL) {
	    if (style != NULL) style->errors++;
	    goto error;
	} else {
	    mode = prop;
	    if (URI != NULL)
		modeURI = xmlStrdup(URI);
	}
	ret->mode = mode;
	ret->modeURI = modeURI;
#ifdef WITH_XSLT_DEBUG_PARSING
	xsltGenericDebug(xsltGenericDebugContext,
	     "xsltParseStylesheetTemplate: mode %s\n", mode);
#endif
    } else {
	mode = NULL;
	modeURI = NULL;
    }
    prop = xsltGetNsProp(template, (const xmlChar *)"match", XSLT_NAMESPACE);
    if (prop != NULL) {
	if (ret->match != NULL) xmlFree(ret->match);
	ret->match  = prop;
    }

    prop = xsltGetNsProp(template, (const xmlChar *)"priority", XSLT_NAMESPACE);
    if (prop != NULL) {
	priority = xmlXPathStringEvalNumber(prop);
	ret->priority = (float) priority;
	xmlFree(prop);
    }

    prop = xsltGetNsProp(template, (const xmlChar *)"name", XSLT_NAMESPACE);
    if (prop != NULL) {
        const xmlChar *URI;
	xsltTemplatePtr cur;

	if (ret->name != NULL) {
	    xmlFree(ret->name);
	    ret->name = NULL;
	}
	if (ret->nameURI != NULL) {
	    xmlFree(ret->nameURI);
	    ret->nameURI = NULL;
	}

	URI = xsltGetQNameURI(template, &prop);
	if (prop == NULL) {
	    if (style != NULL) style->errors++;
	    goto error;
	} else {
	    if (xmlValidateNCName(prop,0)) {
	        xsltTransformError(NULL, style, template,
	            "xsl:template : error invalid name '%s'\n", prop);
		if (style != NULL) style->errors++;
		goto error;
	    } 
	    ret->name = prop;
	    if (URI != NULL)
		ret->nameURI = xmlStrdup(URI);
	    else
		ret->nameURI = NULL;
	    cur = ret->next;
	    while (cur != NULL) {
	        if (xmlStrEqual(cur->name, prop)) {
		    xsltTransformError(NULL, style, template,
		        "xsl:template: error duplicate name '%s'\n", prop);
		    style->errors++;
		    goto error;
		}
		cur = cur->next;
	    }
	}
    }

    /*
     * parse the content and register the pattern
     */
    xsltParseTemplateContent(style, template);
    ret->elem = template;
    ret->content = template->children;
    xsltAddTemplate(style, ret, mode, modeURI);

error:
    for (;exclPrefixes > 0;exclPrefixes--)
	exclPrefixPop(style);
}

/**
 * xsltParseStylesheetTop:
 * @style:  the XSLT stylesheet
 * @top:  the top level "stylesheet" element
 *
 * scan the top level elements of an XSL stylesheet
 */

static void
xsltParseStylesheetTop(xsltStylesheetPtr style, xmlNodePtr top) {
    xmlNodePtr cur;
    xmlChar *prop;
#ifdef WITH_XSLT_DEBUG_PARSING
    int templates = 0;
#endif

    if (top == NULL)
	return;

    prop = xsltGetNsProp(top, (const xmlChar *)"version", XSLT_NAMESPACE);
    if (prop == NULL) {
	xsltTransformError(NULL, style, top,
	    "xsl:version is missing: document may not be a stylesheet\n");
	if (style != NULL) style->warnings++;
    } else {
	if ((!xmlStrEqual(prop, (const xmlChar *)"1.0")) &&
            (!xmlStrEqual(prop, (const xmlChar *)"1.1"))) {
	    xsltTransformError(NULL, style, top,
		"xsl:version: only 1.0 features are supported\n");
	     /* TODO set up compatibility when not XSLT 1.0 */
	    if (style != NULL) style->warnings++;
	}
	xmlFree(prop);
    }

    xsltParseStylesheetExtPrefix(style, top);

    cur = top->children;

    /*
     * process xsl:import elements
     */
    while (cur != NULL) {
	    if (IS_BLANK_NODE(cur)) {
		    cur = cur->next;
		    continue;
	    }
	    if (IS_XSLT_ELEM(cur) && IS_XSLT_NAME(cur, "import")) {
		    if (xsltParseStylesheetImport(style, cur) != 0)
			    if (style != NULL) style->errors++;
	    } else
		    break;
	    cur = cur->next;
    }

    /*
     * process other top-level elements
     */
    while (cur != NULL) {
	if (IS_BLANK_NODE(cur)) {
	    cur = cur->next;
	    continue;
	}
	if (cur->type == XML_TEXT_NODE) {
	    if (cur->content != NULL) {
		xsltTransformError(NULL, style, cur,
		    "misplaced text element: '%s'\n", cur->content);
	    }
	    if (style != NULL) style->errors++;
            cur = cur->next;
	    continue;
	}
	if ((cur->type == XML_ELEMENT_NODE) && (cur->ns == NULL)) {
	    xsltGenericError(xsltGenericErrorContext,
		     "Found a top-level element %s with null namespace URI\n",
		     cur->name);
	    if (style != NULL) style->errors++;
	    cur = cur->next;
	    continue;
	}
	if ((cur->type == XML_ELEMENT_NODE) && (!(IS_XSLT_ELEM(cur)))) {
	    xsltTopLevelFunction function;

	    function = xsltExtModuleTopLevelLookup(cur->name,
						   cur->ns->href);
	    if (function != NULL)
		function(style, cur);

#ifdef WITH_XSLT_DEBUG_PARSING
	    xsltGenericDebug(xsltGenericDebugContext,
		    "xsltParseStylesheetTop : found foreign element %s\n",
		    cur->name);
#endif
            cur = cur->next;
	    continue;
	}
	if (IS_XSLT_NAME(cur, "import")) {
	    xsltTransformError(NULL, style, cur,
			"xsltParseStylesheetTop: ignoring misplaced import element\n");
	    if (style != NULL) style->errors++;
    } else if (IS_XSLT_NAME(cur, "include")) {
	    if (xsltParseStylesheetInclude(style, cur) != 0)
		if (style != NULL) style->errors++;
    } else if (IS_XSLT_NAME(cur, "strip-space")) {
	    xsltParseStylesheetStripSpace(style, cur);
    } else if (IS_XSLT_NAME(cur, "preserve-space")) {
	    xsltParseStylesheetPreserveSpace(style, cur);
    } else if (IS_XSLT_NAME(cur, "output")) {
	    xsltParseStylesheetOutput(style, cur);
    } else if (IS_XSLT_NAME(cur, "key")) {
	    xsltParseStylesheetKey(style, cur);
    } else if (IS_XSLT_NAME(cur, "decimal-format")) {
	    xsltParseStylesheetDecimalFormat(style, cur);
    } else if (IS_XSLT_NAME(cur, "attribute-set")) {
	    xsltParseStylesheetAttributeSet(style, cur);
    } else if (IS_XSLT_NAME(cur, "variable")) {
	    xsltParseGlobalVariable(style, cur);
    } else if (IS_XSLT_NAME(cur, "param")) {
	    xsltParseGlobalParam(style, cur);
    } else if (IS_XSLT_NAME(cur, "template")) {
#ifdef WITH_XSLT_DEBUG_PARSING
	    templates++;
#endif
	    xsltParseStylesheetTemplate(style, cur);
    } else if (IS_XSLT_NAME(cur, "namespace-alias")) {
	    xsltNamespaceAlias(style, cur);
	} else {
            if ((style != NULL) && (style->doc->version != NULL) &&
	        (!strncmp((const char *) style->doc->version, "1.0", 3))) {
	        xsltTransformError(NULL, style, cur,
			"xsltParseStylesheetTop: unknown %s element\n",
			cur->name);
	        if (style != NULL) style->errors++;
	    }
	    else {
                /* do Forwards-Compatible Processing */
	        xsltTransformError(NULL, style, cur,
			"xsltParseStylesheetTop: ignoring unknown %s element\n",
			cur->name);
	        if (style != NULL) style->warnings++;
            }
	}
	cur = cur->next;
    }
#ifdef WITH_XSLT_DEBUG_PARSING
    xsltGenericDebug(xsltGenericDebugContext,
		    "parsed %d templates\n", templates);
#endif
}

/**
 * xsltParseStylesheetProcess:
 * @ret:  the XSLT stylesheet
 * @doc:  and xmlDoc parsed XML
 *
 * parse an XSLT stylesheet adding the associated structures
 *
 * Returns the value of the 'ret' parameter if everything
 * went right, NULL if something went amiss.
 */

xsltStylesheetPtr
xsltParseStylesheetProcess(xsltStylesheetPtr ret, xmlDocPtr doc) {
    xmlNodePtr cur;

    if (doc == NULL)
	return(NULL);
    if (ret == NULL)
	return(ret);
    
    /*
     * First steps, remove blank nodes,
     * locate the xsl:stylesheet element and the
     * namespace declaration.
     */
    cur = xmlDocGetRootElement(doc);
    if (cur == NULL) {
	xsltTransformError(NULL, ret, (xmlNodePtr) doc,
		"xsltParseStylesheetProcess : empty stylesheet\n");
	return(NULL);
    }
    xsltParseStylesheetExcludePrefix(ret, cur);
    if (!ret->nopreproc)
	xsltPrecomputeStylesheet(ret, cur);

    if ((IS_XSLT_ELEM(cur)) && 
	((IS_XSLT_NAME(cur, "stylesheet")) ||
	 (IS_XSLT_NAME(cur, "transform")))) {
#ifdef WITH_XSLT_DEBUG_PARSING
	xsltGenericDebug(xsltGenericDebugContext,
		"xsltParseStylesheetProcess : found stylesheet\n");
#endif

	xsltParseStylesheetTop(ret, cur);
    } else {
	xmlChar *prop;
	xsltTemplatePtr template;

	/*
	 * the document itself might be the template, check xsl:version
	 */
	prop = xsltGetNsProp(cur, (const xmlChar *)"version", XSLT_NAMESPACE);
	if (prop == NULL) {
	    xsltTransformError(NULL, ret, cur,
		"xsltParseStylesheetProcess : document is not a stylesheet\n");
	    return(NULL);
	}

#ifdef WITH_XSLT_DEBUG_PARSING
        xsltGenericDebug(xsltGenericDebugContext,
		"xsltParseStylesheetProcess : document is stylesheet\n");
#endif
	
	if (!xmlStrEqual(prop, (const xmlChar *)"1.0")) {
	    xsltTransformError(NULL, ret, cur,
		"xsl:version: only 1.0 features are supported\n");
	     /* TODO set up compatibility when not XSLT 1.0 */
	    ret->warnings++;
	}
	xmlFree(prop);

	/*
	 * Create and link the template
	 */
	template = xsltNewTemplate();
	if (template == NULL) {
	    return(NULL);
	}
	template->next = ret->templates;
	ret->templates = template;
	template->match = xmlStrdup((const xmlChar *)"/");

	/*
	 * parse the content and register the pattern
	 */
	xsltParseTemplateContent(ret, (xmlNodePtr) doc);
	template->elem = (xmlNodePtr) doc;
	template->content = doc->children;
	xsltAddTemplate(ret, template, NULL, NULL);
    }

    return(ret);
}

/**
 * xsltParseStylesheetImportedDoc:
 * @doc:  an xmlDoc parsed XML
 * @style: pointer to parent stylesheet
 *
 * parse an XSLT stylesheet building the associated structures
 * except the processing not needed for imported documents.
 *
 * Returns a new XSLT stylesheet structure.
 */

xsltStylesheetPtr
xsltParseStylesheetImportedDoc(xmlDocPtr doc, xsltStylesheetPtr style) {
    xsltStylesheetPtr ret;

    if (doc == NULL)
	return(NULL);

    ret = xsltNewStylesheet();
    if (ret == NULL)
	return(NULL);
    if (doc->dict != NULL) {
        xmlDictFree(ret->dict);
	ret->dict = doc->dict;
#ifdef WITH_XSLT_DEBUG
        xsltGenericDebug(xsltGenericDebugContext,
                         "reusing dictionary from %s for stylesheet\n",
			 doc->URL);
#endif
	xmlDictReference(ret->dict);
    }
    
    ret->doc = doc;
    ret->parent = style;	/* needed to prevent loops */
    xsltGatherNamespaces(ret);
    if (xsltParseStylesheetProcess(ret, doc) == NULL) {
	ret->doc = NULL;
	xsltFreeStylesheet(ret);
	ret = NULL;
    }
    if (ret != NULL) {
	if (ret->errors != 0) {
	    ret->doc = NULL;
	    xsltFreeStylesheet(ret);
	    ret = NULL;
	}
    }
    
    return(ret);
}

/**
 * xsltParseStylesheetDoc:
 * @doc:  and xmlDoc parsed XML
 *
 * parse an XSLT stylesheet building the associated structures
 *
 * Returns a new XSLT stylesheet structure.
 */

xsltStylesheetPtr
xsltParseStylesheetDoc(xmlDocPtr doc) {
    xsltStylesheetPtr ret;

    ret = xsltParseStylesheetImportedDoc(doc, NULL);
    if (ret == NULL)
	return(NULL);

    xsltResolveStylesheetAttributeSet(ret);

    return(ret);
}

/**
 * xsltParseStylesheetFile:
 * @filename:  the filename/URL to the stylesheet
 *
 * Load and parse an XSLT stylesheet
 *
 * Returns a new XSLT stylesheet structure.
 */

xsltStylesheetPtr
xsltParseStylesheetFile(const xmlChar* filename) {
    xsltSecurityPrefsPtr sec;
    xsltStylesheetPtr ret;
    xmlDocPtr doc;
    

    if (filename == NULL)
	return(NULL);

#ifdef WITH_XSLT_DEBUG_PARSING
    xsltGenericDebug(xsltGenericDebugContext,
	    "xsltParseStylesheetFile : parse %s\n", filename);
#endif

    /*
     * Security framework check
     */
    sec = xsltGetDefaultSecurityPrefs();
    if (sec != NULL) {
	int res;

	res = xsltCheckRead(sec, NULL, filename);
	if (res == 0) {
	    xsltTransformError(NULL, NULL, NULL,
		 "xsltParseStylesheetFile: read rights for %s denied\n",
			     filename);
	    return(NULL);
	}
    }

    doc = xsltDocDefaultLoader(filename, NULL, XSLT_PARSE_OPTIONS,
                               NULL, XSLT_LOAD_START);
    if (doc == NULL) {
	xsltTransformError(NULL, NULL, NULL,
		"xsltParseStylesheetFile : cannot parse %s\n", filename);
	return(NULL);
    }
    ret = xsltParseStylesheetDoc(doc);
    if (ret == NULL) {
	xmlFreeDoc(doc);
	return(NULL);
    }

    return(ret);
}

/************************************************************************
 *									*
 *			Handling of Stylesheet PI			*
 *									*
 ************************************************************************/

#define CUR (*cur)
#define SKIP(val) cur += (val)
#define NXT(val) cur[(val)]
#define SKIP_BLANKS						\
    while (IS_BLANK(CUR)) NEXT
#define NEXT ((*cur) ?  cur++ : cur)

/**
 * xsltParseStylesheetPI:
 * @value: the value of the PI
 *
 * This function checks that the type is text/xml and extracts
 * the URI-Reference for the stylesheet
 *
 * Returns the URI-Reference for the stylesheet or NULL (it need to
 *         be freed by the caller)
 */
static xmlChar *
xsltParseStylesheetPI(const xmlChar *value) {
    const xmlChar *cur;
    const xmlChar *start;
    xmlChar *val;
    xmlChar tmp;
    xmlChar *href = NULL;
    int isXml = 0;

    if (value == NULL)
	return(NULL);

    cur = value;
    while (CUR != 0) {
	SKIP_BLANKS;
	if ((CUR == 't') && (NXT(1) == 'y') && (NXT(2) == 'p') &&
	    (NXT(3) == 'e')) {
	    SKIP(4);
	    SKIP_BLANKS;
	    if (CUR != '=')
		continue;
	    NEXT;
	    if ((CUR != '\'') && (CUR != '"'))
		continue;
	    tmp = CUR;
	    NEXT;
	    start = cur;
	    while ((CUR != 0) && (CUR != tmp))
		NEXT;
	    if (CUR != tmp)
		continue;
	    val = xmlStrndup(start, cur - start);
	    NEXT;
	    if (val == NULL) 
		return(NULL);
	    if ((xmlStrcasecmp(val, BAD_CAST "text/xml")) &&
		(xmlStrcasecmp(val, BAD_CAST "text/xsl"))) {
                xmlFree(val);
		break;
	    }
	    isXml = 1;
	    xmlFree(val);
	} else if ((CUR == 'h') && (NXT(1) == 'r') && (NXT(2) == 'e') &&
	    (NXT(3) == 'f')) {
	    SKIP(4);
	    SKIP_BLANKS;
	    if (CUR != '=')
		continue;
	    NEXT;
	    if ((CUR != '\'') && (CUR != '"'))
		continue;
	    tmp = CUR;
	    NEXT;
	    start = cur;
	    while ((CUR != 0) && (CUR != tmp))
		NEXT;
	    if (CUR != tmp)
		continue;
	    if (href == NULL)
		href = xmlStrndup(start, cur - start);
	    NEXT;
	} else {
	    while ((CUR != 0) && (!IS_BLANK(CUR)))
		NEXT;
	}
            
    }

    if (!isXml) {
	if (href != NULL)
	    xmlFree(href);
	href = NULL;
    }
    return(href);
}

/**
 * xsltLoadStylesheetPI:
 * @doc:  a document to process
 *
 * This function tries to locate the stylesheet PI in the given document
 * If found, and if contained within the document, it will extract 
 * that subtree to build the stylesheet to process @doc (doc itself will
 * be modified). If found but referencing an external document it will
 * attempt to load it and generate a stylesheet from it. In both cases,
 * the resulting stylesheet and the document need to be freed once the
 * transformation is done.
 *
 * Returns a new XSLT stylesheet structure or NULL if not found.
 */
xsltStylesheetPtr
xsltLoadStylesheetPI(xmlDocPtr doc) {
    xmlNodePtr child;
    xsltStylesheetPtr ret = NULL;
    xmlChar *href = NULL;
    xmlURIPtr URI;

    if (doc == NULL)
	return(NULL);

    /*
     * Find the text/xml stylesheet PI id any before the root
     */
    child = doc->children;
    while ((child != NULL) && (child->type != XML_ELEMENT_NODE)) {
	if ((child->type == XML_PI_NODE) &&
	    (xmlStrEqual(child->name, BAD_CAST "xml-stylesheet"))) {
	    href = xsltParseStylesheetPI(child->content);
	    if (href != NULL)
		break;
	}
	child = child->next;
    }

    /*
     * If found check the href to select processing
     */
    if (href != NULL) {
#ifdef WITH_XSLT_DEBUG_PARSING
	xsltGenericDebug(xsltGenericDebugContext,
		"xsltLoadStylesheetPI : found PI href=%s\n", href);
#endif
	URI = xmlParseURI((const char *) href);
	if (URI == NULL) {
	    xsltTransformError(NULL, NULL, child,
		    "xml-stylesheet : href %s is not valid\n", href);
	    xmlFree(href);
	    return(NULL);
	}
	if ((URI->fragment != NULL) && (URI->scheme == NULL) &&
            (URI->opaque == NULL) && (URI->authority == NULL) &&
            (URI->server == NULL) && (URI->user == NULL) &&
            (URI->path == NULL) && (URI->query == NULL)) {
	    xmlAttrPtr ID;

#ifdef WITH_XSLT_DEBUG_PARSING
	    xsltGenericDebug(xsltGenericDebugContext,
		    "xsltLoadStylesheetPI : Reference to ID %s\n", href);
#endif
	    if (URI->fragment[0] == '#')
		ID = xmlGetID(doc, (const xmlChar *) &(URI->fragment[1]));
	    else
		ID = xmlGetID(doc, (const xmlChar *) URI->fragment);
	    if (ID == NULL) {
		xsltTransformError(NULL, NULL, child,
		    "xml-stylesheet : no ID %s found\n", URI->fragment);
	    } else {
		xmlDocPtr fake;
		xmlNodePtr subtree;

		/*
		 * move the subtree in a new document passed to
		 * the stylesheet analyzer
		 */
		subtree = ID->parent;
		fake = xmlNewDoc(NULL);
		if (fake != NULL) {
                    /*
		     * the dictionnary should be shared since nodes are
		     * moved over.
		     */
		    fake->dict = doc->dict;
		    xmlDictReference(doc->dict);
#ifdef WITH_XSLT_DEBUG
		    xsltGenericDebug(xsltGenericDebugContext,
                         "reusing dictionary from %s for stylesheet\n",
			 doc->URL);
#endif

		    xmlUnlinkNode(subtree);
		    xmlAddChild((xmlNodePtr) fake, subtree);
		    ret = xsltParseStylesheetDoc(fake);
		    if (ret == NULL)
			xmlFreeDoc(fake);
		}
	    }
	} else {
	    xmlChar *URL, *base;

	    /*
	     * Reference to an external stylesheet
	     */

	    base = xmlNodeGetBase(doc, (xmlNodePtr) doc);
	    URL = xmlBuildURI(href, base);
	    if (URL != NULL) {
#ifdef WITH_XSLT_DEBUG_PARSING
		xsltGenericDebug(xsltGenericDebugContext,
			"xsltLoadStylesheetPI : fetching %s\n", URL);
#endif
		ret = xsltParseStylesheetFile(URL);
		xmlFree(URL);
	    } else {
#ifdef WITH_XSLT_DEBUG_PARSING
		xsltGenericDebug(xsltGenericDebugContext,
			"xsltLoadStylesheetPI : fetching %s\n", href);
#endif
		ret = xsltParseStylesheetFile(href);
	    }
	    if (base != NULL)
		xmlFree(base);
	}
	xmlFreeURI(URI);
	xmlFree(href);
    }
    return(ret);
}
