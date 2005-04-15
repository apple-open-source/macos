/*
 * extensions.c: Implemetation of the extensions support
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

#include <libxml/xmlmemory.h>
#include <libxml/tree.h>
#include <libxml/hash.h>
#include <libxml/xmlerror.h>
#include <libxml/parserInternals.h>
#include <libxml/xpathInternals.h>
#include "xslt.h"
#include "xsltInternals.h"
#include "xsltutils.h"
#include "imports.h"
#include "extensions.h"

#ifdef WITH_XSLT_DEBUG
#define WITH_XSLT_DEBUG_EXTENSIONS
#endif

/************************************************************************
 * 									*
 * 			Private Types and Globals			*
 * 									*
 ************************************************************************/

typedef struct _xsltExtDef xsltExtDef;
typedef xsltExtDef *xsltExtDefPtr;
struct _xsltExtDef {
    struct _xsltExtDef *next;
    xmlChar *prefix;
    xmlChar *URI;
    void    *data;
};

typedef struct _xsltExtModule xsltExtModule;
typedef xsltExtModule *xsltExtModulePtr;
struct _xsltExtModule {
    xsltExtInitFunction initFunc;
    xsltExtShutdownFunction shutdownFunc;
    xsltStyleExtInitFunction styleInitFunc;
    xsltStyleExtShutdownFunction styleShutdownFunc;
};

typedef struct _xsltExtData xsltExtData;
typedef xsltExtData *xsltExtDataPtr;
struct _xsltExtData {
    xsltExtModulePtr extModule;
    void *extData;
};

typedef struct _xsltExtElement xsltExtElement;
typedef xsltExtElement *xsltExtElementPtr;
struct _xsltExtElement {
    xsltPreComputeFunction precomp;
    xsltTransformFunction  transform;
};

static xmlHashTablePtr xsltExtensionsHash = NULL;
static xmlHashTablePtr xsltFunctionsHash = NULL;
static xmlHashTablePtr xsltElementsHash = NULL;
static xmlHashTablePtr xsltTopLevelsHash = NULL;

/************************************************************************
 * 									*
 * 			Type functions 					*
 * 									*
 ************************************************************************/

/**
 * xsltNewExtDef:
 * @prefix:  the extension prefix
 * @URI:  the namespace URI
 *
 * Create a new XSLT ExtDef
 *
 * Returns the newly allocated xsltExtDefPtr or NULL in case of error
 */
static xsltExtDefPtr
xsltNewExtDef(const xmlChar * prefix, const xmlChar * URI)
{
    xsltExtDefPtr cur;

    cur = (xsltExtDefPtr) xmlMalloc(sizeof(xsltExtDef));
    if (cur == NULL) {
	xsltTransformError(NULL, NULL, NULL,
                         "xsltNewExtDef : malloc failed\n");
        return (NULL);
    }
    memset(cur, 0, sizeof(xsltExtDef));
    if (prefix != NULL)
        cur->prefix = xmlStrdup(prefix);
    if (URI != NULL)
        cur->URI = xmlStrdup(URI);
    return (cur);
}

/**
 * xsltFreeExtDef:
 * @extensiond:  an XSLT extension definition
 *
 * Free up the memory allocated by @extensiond
 */
static void
xsltFreeExtDef(xsltExtDefPtr extensiond) {
    if (extensiond == NULL)
	return;
    if (extensiond->prefix != NULL)
	xmlFree(extensiond->prefix);
    if (extensiond->URI != NULL)
	xmlFree(extensiond->URI);
    xmlFree(extensiond);
}

/**
 * xsltFreeExtDefList:
 * @extensiond:  an XSLT extension definition list
 *
 * Free up the memory allocated by all the elements of @extensiond
 */
static void
xsltFreeExtDefList(xsltExtDefPtr extensiond) {
    xsltExtDefPtr cur;

    while (extensiond != NULL) {
	cur = extensiond;
	extensiond = extensiond->next;
	xsltFreeExtDef(cur);
    }
}

/**
 * xsltNewExtModule:
 * @initFunc:  the module initialization function
 * @shutdownFunc:  the module shutdown function
 * @styleInitFunc:  the stylesheet module data allocator function
 * @styleShutdownFunc:  the stylesheet module data free function
 *
 * Create a new XSLT extension module
 *
 * Returns the newly allocated xsltExtModulePtr or NULL in case of error
 */
static xsltExtModulePtr
xsltNewExtModule(xsltExtInitFunction initFunc,
                 xsltExtShutdownFunction shutdownFunc,
		 xsltStyleExtInitFunction styleInitFunc,
		 xsltStyleExtShutdownFunction styleShutdownFunc)
{
    xsltExtModulePtr cur;

    cur = (xsltExtModulePtr) xmlMalloc(sizeof(xsltExtModule));
    if (cur == NULL) {
	xsltTransformError(NULL, NULL, NULL,
                         "xsltNewExtModule : malloc failed\n");
        return (NULL);
    }
    cur->initFunc = initFunc;
    cur->shutdownFunc = shutdownFunc;
    cur->styleInitFunc = styleInitFunc;
    cur->styleShutdownFunc = styleShutdownFunc;
    return (cur);
}

/**
 * xsltFreeExtModule:
 * @ext:  an XSLT extension module
 *
 * Free up the memory allocated by @ext
 */
static void
xsltFreeExtModule(xsltExtModulePtr ext) {
    if (ext == NULL)
	return;
    xmlFree(ext);
}

/**
 * xsltNewExtData:
 * @extModule:  the module
 * @extData:  the associated data
 *
 * Create a new XSLT extension module data wrapper
 *
 * Returns the newly allocated xsltExtDataPtr or NULL in case of error
 */
static xsltExtDataPtr
xsltNewExtData(xsltExtModulePtr extModule, void *extData)
{
    xsltExtDataPtr cur;

    if (extModule == NULL)
	return(NULL);
    cur = (xsltExtDataPtr) xmlMalloc(sizeof(xsltExtData));
    if (cur == NULL) {
	xsltTransformError(NULL, NULL, NULL,
                         "xsltNewExtData : malloc failed\n");
        return (NULL);
    }
    cur->extModule = extModule;
    cur->extData = extData;
    return (cur);
}

/**
 * xsltFreeExtData:
 * @ext:  an XSLT extension module data wrapper
 *
 * Free up the memory allocated by @ext
 */
static void
xsltFreeExtData(xsltExtDataPtr ext) {
    if (ext == NULL)
	return;
    xmlFree(ext);
}

/**
 * xsltNewExtElement:
 * @precomp:  the pre-computation function
 * @transform:  the transformation function
 *
 * Create a new XSLT extension element
 *
 * Returns the newly allocated xsltExtElementPtr or NULL in case of
 * error
 */
static xsltExtElementPtr
xsltNewExtElement (xsltPreComputeFunction precomp,
		   xsltTransformFunction transform) {
    xsltExtElementPtr cur;

    if (transform == NULL)
	return(NULL);

    cur = (xsltExtElementPtr) xmlMalloc(sizeof(xsltExtElement));
    if (cur == NULL) {
	xsltTransformError(NULL, NULL, NULL,
                         "xsltNewExtElement : malloc failed\n");
        return (NULL);
    }
    cur->precomp = precomp;
    cur->transform = transform;
    return(cur);
}

/**
 * xsltFreeExtElement:
 * @ext: an XSLT extension element
 *
 * Frees up the memory allocated by @ext
 */
static void
xsltFreeExtElement (xsltExtElementPtr ext) {
    if (ext == NULL)
	return;
    xmlFree(ext);
}


/************************************************************************
 * 									*
 * 		The stylesheet extension prefixes handling		*
 * 									*
 ************************************************************************/


/**
 * xsltFreeExts:
 * @style: an XSLT stylesheet
 *
 * Free up the memory used by XSLT extensions in a stylesheet
 */
void
xsltFreeExts(xsltStylesheetPtr style) {
    if (style->nsDefs != NULL)
	xsltFreeExtDefList((xsltExtDefPtr) style->nsDefs);
}

/**
 * xsltRegisterExtPrefix:
 * @style: an XSLT stylesheet
 * @prefix: the prefix used
 * @URI: the URI associated to the extension
 *
 * Registers an extension namespace
 *
 * Returns 0 in case of success, -1 in case of failure
 */
int
xsltRegisterExtPrefix(xsltStylesheetPtr style,
		      const xmlChar *prefix, const xmlChar *URI) {
    xsltExtDefPtr def, ret;

    if ((style == NULL) || (prefix == NULL) | (URI == NULL))
	return(-1);

#ifdef WITH_XSLT_DEBUG_EXTENSIONS
    xsltGenericDebug(xsltGenericDebugContext,
	 "Registering extension prefix %s : %s\n", prefix, URI);
#endif
    def = (xsltExtDefPtr) style->nsDefs;
    while (def != NULL) {
	if (xmlStrEqual(prefix, def->prefix))
	    return(-1);
	def = def->next;
    }
    ret = xsltNewExtDef(prefix, URI);
    if (ret == NULL)
	return(-1);
    ret->next = (xsltExtDefPtr) style->nsDefs;
    style->nsDefs = ret;

    /*
     * check wether there is an extension module with a stylesheet
     * initialization function.
     */
    if (xsltExtensionsHash != NULL) {
	xsltExtModulePtr module;

	module = xmlHashLookup(xsltExtensionsHash, URI);
	if (module != NULL) {
	    xsltStyleGetExtData(style, URI);
	}
    }
    return(0);
}

/************************************************************************
 * 									*
 * 		The extensions modules interfaces			*
 * 									*
 ************************************************************************/

/**
 * xsltRegisterExtFunction:
 * @ctxt: an XSLT transformation context
 * @name: the name of the element
 * @URI: the URI associated to the element
 * @function: the actual implementation which should be called 
 *
 * Registers an extension function
 *
 * Returns 0 in case of success, -1 in case of failure
 */
int
xsltRegisterExtFunction(xsltTransformContextPtr ctxt, const xmlChar *name,
	                const xmlChar *URI, xmlXPathFunction function) {
    if ((ctxt == NULL) || (name == NULL) ||
	(URI == NULL) || (function == NULL))
	return(-1);
    if (ctxt->xpathCtxt != NULL) {
	xmlXPathRegisterFuncNS(ctxt->xpathCtxt, name, URI, function);
    }
    if (ctxt->extFunctions == NULL)
	ctxt->extFunctions = xmlHashCreate(10);
    if (ctxt->extFunctions == NULL)
	return(-1);
    return(xmlHashAddEntry2(ctxt->extFunctions, name, URI, (void *) function));
}

/**
 * xsltRegisterExtElement:
 * @ctxt: an XSLT transformation context
 * @name: the name of the element
 * @URI: the URI associated to the element
 * @function: the actual implementation which should be called 
 *
 * Registers an extension element
 *
 * Returns 0 in case of success, -1 in case of failure
 */
int	
xsltRegisterExtElement(xsltTransformContextPtr ctxt, const xmlChar *name,
		       const xmlChar *URI, xsltTransformFunction function) {
    if ((ctxt == NULL) || (name == NULL) ||
	(URI == NULL) || (function == NULL))
	return(-1);
    if (ctxt->extElements == NULL)
	ctxt->extElements = xmlHashCreate(10);
    if (ctxt->extElements == NULL)
	return(-1);
    return(xmlHashAddEntry2(ctxt->extElements, name, URI, (void *) function));
}

/**
 * xsltFreeCtxtExts:
 * @ctxt: an XSLT transformation context
 *
 * Free the XSLT extension data
 */
void
xsltFreeCtxtExts(xsltTransformContextPtr ctxt) {
    if (ctxt->extElements != NULL)
	xmlHashFree(ctxt->extElements, NULL);
    if (ctxt->extFunctions != NULL)
	xmlHashFree(ctxt->extFunctions, NULL);
}

/**
 * xsltStyleGetExtData:
 * @style: an XSLT stylesheet
 * @URI:  the URI associated to the exension module
 *
 * Retrieve the data associated to the extension module in this given
 * stylesheet.
 *
 * Returns the pointer or NULL if not present
 */
void *
xsltStyleGetExtData(xsltStylesheetPtr style, const xmlChar * URI) {
    xsltExtDataPtr data = NULL;
    xsltStylesheetPtr tmp;


    if ((style == NULL) || (URI == NULL))
        return (NULL);

    tmp = style;
    while (tmp != NULL) {
	if (tmp->extInfos != NULL) {
	    data = (xsltExtDataPtr) xmlHashLookup(tmp->extInfos, URI);
	    if (data != NULL)
		break;
	}
        tmp = xsltNextImport(tmp);
    }
    if (data == NULL) {
	if (style->extInfos == NULL) {
	    style->extInfos = xmlHashCreate(10);
	    if (style->extInfos == NULL)
		return(NULL);
	}
    }
    if (data == NULL) {
	void *extData;
	xsltExtModulePtr module;

	module = xmlHashLookup(xsltExtensionsHash, URI);
	if (module == NULL) {
#ifdef WITH_XSLT_DEBUG_EXTENSIONS
	    xsltGenericDebug(xsltGenericDebugContext,
			     "Not registered extension module: %s\n", URI);
#endif
	    return(NULL);
	} else {
	    if (module->styleInitFunc == NULL) {
#ifdef WITH_XSLT_DEBUG_EXTENSIONS
		xsltGenericDebug(xsltGenericDebugContext,
			     "Registering style module: %s\n", URI);
#endif
		extData = NULL;
	    } else {
#ifdef WITH_XSLT_DEBUG_EXTENSIONS
		xsltGenericDebug(xsltGenericDebugContext,
				 "Initializing module: %s\n", URI);
#endif
		extData = module->styleInitFunc(style, URI);
	    }

	    data = xsltNewExtData(module, extData);
	    if (data == NULL)
		return (NULL);
	    if (xmlHashAddEntry(style->extInfos, URI,
				(void *) data) < 0) {
		xsltGenericError(xsltGenericErrorContext,
				 "Failed to register module data: %s\n", URI);
		if (module->styleShutdownFunc)
		    module->styleShutdownFunc(style, URI, extData);
		xsltFreeExtData(data);
		return(NULL);
	    }
	}
    }
    return (data->extData);
}

/**
 * xsltGetExtData:
 * @ctxt: an XSLT transformation context
 * @URI:  the URI associated to the exension module
 *
 * Retrieve the data associated to the extension module in this given
 * transformation.
 *
 * Returns the pointer or NULL if not present
 */
void *
xsltGetExtData(xsltTransformContextPtr ctxt, const xmlChar * URI) {
    xsltExtDataPtr data;

    if ((ctxt == NULL) || (URI == NULL))
        return (NULL);
    if (ctxt->extInfos == NULL) {
	ctxt->extInfos = xmlHashCreate(10);
	if (ctxt->extInfos == NULL)
	    return(NULL);
	data = NULL;
    } else {
	data = (xsltExtDataPtr) xmlHashLookup(ctxt->extInfos, URI);
    }
    if (data == NULL) {
	void *extData;
	xsltExtModulePtr module;

	module = xmlHashLookup(xsltExtensionsHash, URI);
	if (module == NULL) {
#ifdef WITH_XSLT_DEBUG_EXTENSIONS
	    xsltGenericDebug(xsltGenericDebugContext,
			     "Not registered extension module: %s\n", URI);
#endif
	    return(NULL);
	} else {
	    if (module->initFunc == NULL)
		return(NULL);

#ifdef WITH_XSLT_DEBUG_EXTENSIONS
	    xsltGenericDebug(xsltGenericDebugContext,
			     "Initializing module: %s\n", URI);
#endif

	    extData = module->initFunc(ctxt, URI);
	    if (extData == NULL)
		return(NULL);

	    data = xsltNewExtData(module, extData);
	    if (data == NULL)
		return (NULL);
	    if (xmlHashAddEntry(ctxt->extInfos, URI,
				(void *) data) < 0) {
		xsltTransformError(ctxt, NULL, NULL,
				 "Failed to register module data: %s\n", URI);
		if (module->shutdownFunc)
		    module->shutdownFunc(ctxt, URI, extData);
		xsltFreeExtData(data);
		return(NULL);
	    }
	}
    }
    return (data->extData);
}

typedef struct _xsltInitExtCtxt xsltInitExtCtxt;
struct _xsltInitExtCtxt {
    xsltTransformContextPtr ctxt;
    int ret;
};

/**
 * xsltInitCtxtExt:
 * @styleData:  the registered stylesheet data for the module
 * @ctxt:  the XSLT transformation context + the return value
 * @URI:  the extension URI
 *
 * Initializes an extension module
 */
static void
xsltInitCtxtExt (xsltExtDataPtr styleData, xsltInitExtCtxt *ctxt,
		 const xmlChar *URI) {
    xsltExtModulePtr module;
    xsltExtDataPtr ctxtData;
    void *extData;

    if ((styleData == NULL) || (ctxt == NULL) || (URI == NULL) ||
	(ctxt->ret == -1)) {
#ifdef WITH_XSLT_DEBUG_EXTENSIONS
    xsltGenericDebug(xsltGenericDebugContext,
		     "xsltInitCtxtExt: NULL param or error\n");
#endif
        return;
    }
    module = styleData->extModule;
    if ((module == NULL) || (module->initFunc == NULL)) {
#ifdef WITH_XSLT_DEBUG_EXTENSIONS
	xsltGenericDebug(xsltGenericDebugContext,
			 "xsltInitCtxtExt: no module or no initFunc\n");
#endif
        return;
    }

    ctxtData = (xsltExtDataPtr) xmlHashLookup(ctxt->ctxt->extInfos, URI);
    if (ctxtData != NULL) {
#ifdef WITH_XSLT_DEBUG_EXTENSIONS
	xsltGenericDebug(xsltGenericDebugContext,
			 "xsltInitCtxtExt: already initialized\n");
#endif
        return;
    }

    extData = module->initFunc(ctxt->ctxt, URI);
    if (extData == NULL) {
#ifdef WITH_XSLT_DEBUG_EXTENSIONS
	xsltGenericDebug(xsltGenericDebugContext,
			 "xsltInitCtxtExt: no extData\n");
#endif
    }
    ctxtData = xsltNewExtData(module, extData);
    if (ctxtData == NULL) {
	ctxt->ret = -1;
	return;
    }

    if (ctxt->ctxt->extInfos == NULL)
	ctxt->ctxt->extInfos = xmlHashCreate(10);
    if (ctxt->ctxt->extInfos == NULL) {
	ctxt->ret = -1;
	return;
    }

    if (xmlHashAddEntry(ctxt->ctxt->extInfos, URI, ctxtData) < 0) {
	xsltGenericError(xsltGenericErrorContext,
			 "Failed to register module data: %s\n", URI);
	if (module->shutdownFunc)
	    module->shutdownFunc(ctxt->ctxt, URI, extData);
	xsltFreeExtData(ctxtData);
	ctxt->ret = -1;
	return;
    }
#ifdef WITH_XSLT_DEBUG_EXTENSIONS
    xsltGenericDebug(xsltGenericDebugContext, "Registered module %s\n",
		     URI);
#endif
    ctxt->ret++;
}

/**
 * xsltInitCtxtExts:
 * @ctxt: an XSLT transformation context
 *
 * Initialize the set of modules with registered stylesheet data
 *
 * Returns the number of modules initialized or -1 in case of error
 */
int
xsltInitCtxtExts(xsltTransformContextPtr ctxt)
{
    xsltStylesheetPtr style;
    xsltInitExtCtxt ctx;

    if (ctxt == NULL)
        return (-1);

    style = ctxt->style;
    if (style == NULL)
        return (-1);

    ctx.ctxt = ctxt;
    ctx.ret = 0;

    while (style != NULL) {
	if (style->extInfos != NULL) {
	    xmlHashScan(style->extInfos,
			(xmlHashScanner) xsltInitCtxtExt, &ctx);
	    if (ctx.ret == -1)
		return(-1);
	}
        style = xsltNextImport(style);
    }
#ifdef WITH_XSLT_DEBUG_EXTENSIONS
    xsltGenericDebug(xsltGenericDebugContext, "Registered %d modules\n",
		     ctx.ret);
#endif
    return (ctx.ret);
}

/**
 * xsltShutdownCtxtExt:
 * @data:  the registered data for the module
 * @ctxt:  the XSLT transformation context
 * @URI:  the extension URI
 *
 * Shutdown an extension module loaded
 */
static void
xsltShutdownCtxtExt(xsltExtDataPtr data, xsltTransformContextPtr ctxt,
                    const xmlChar * URI)
{
    xsltExtModulePtr module;

    if ((data == NULL) || (ctxt == NULL) || (URI == NULL))
        return;
    module = data->extModule;
    if ((module == NULL) || (module->shutdownFunc == NULL))
        return;

#ifdef WITH_XSLT_DEBUG_EXTENSIONS
    xsltGenericDebug(xsltGenericDebugContext,
                     "Shutting down module : %s\n", URI);
#endif
    module->shutdownFunc(ctxt, URI, data->extData);
}

/**
 * xsltShutdownCtxtExts:
 * @ctxt: an XSLT transformation context
 *
 * Shutdown the set of modules loaded
 */
void
xsltShutdownCtxtExts(xsltTransformContextPtr ctxt)
{
    if (ctxt == NULL)
	return;
    if (ctxt->extInfos == NULL)
	return;
    xmlHashScan(ctxt->extInfos, (xmlHashScanner) xsltShutdownCtxtExt, ctxt);
    xmlHashFree(ctxt->extInfos, (xmlHashDeallocator) xsltFreeExtData);
    ctxt->extInfos = NULL;
}

/**
 * xsltShutdownExt:
 * @data:  the registered data for the module
 * @ctxt:  the XSLT stylesheet
 * @URI:  the extension URI
 *
 * Shutdown an extension module loaded
 */
static void
xsltShutdownExt(xsltExtDataPtr data, xsltStylesheetPtr style,
		const xmlChar * URI)
{
    xsltExtModulePtr module;

    if ((data == NULL) || (style == NULL) || (URI == NULL))
        return;
    module = data->extModule;
    if ((module == NULL) || (module->styleShutdownFunc == NULL))
        return;

#ifdef WITH_XSLT_DEBUG_EXTENSIONS
    xsltGenericDebug(xsltGenericDebugContext,
                     "Shutting down module : %s\n", URI);
#endif
    module->styleShutdownFunc(style, URI, data->extData);
    xmlHashRemoveEntry(style->extInfos, URI,
                       (xmlHashDeallocator) xsltFreeExtData);
}

/**
 * xsltShutdownExts:
 * @style: an XSLT stylesheet
 *
 * Shutdown the set of modules loaded
 */
void
xsltShutdownExts(xsltStylesheetPtr style)
{
    if (style == NULL)
	return;
    if (style->extInfos == NULL)
	return;
    xmlHashScan(style->extInfos, (xmlHashScanner) xsltShutdownExt, style);
    xmlHashFree(style->extInfos, (xmlHashDeallocator) xsltFreeExtData);
    style->extInfos = NULL;
}

/**
 * xsltCheckExtPrefix:
 * @style: the stylesheet
 * @prefix: the namespace prefix (possibly NULL)
 *
 * Check if the given prefix is one of the declared extensions
 *
 * Returns 1 if this is an extension, 0 otherwise
 */
int
xsltCheckExtPrefix(xsltStylesheetPtr style, const xmlChar *prefix) {
    xsltExtDefPtr cur;

    if ((style == NULL) || (style->nsDefs == NULL))
	return(0);

    if (prefix == NULL)
	prefix = BAD_CAST "#default";

    cur = (xsltExtDefPtr) style->nsDefs;
    while (cur != NULL) {
	if (xmlStrEqual(prefix, cur->prefix))
	    return(1);
	cur = cur->next;
    }
    return(0);
}

/**
 * xsltRegisterExtModuleFull:
 * @URI:  URI associated to this module
 * @initFunc:  the module initialization function
 * @shutdownFunc:  the module shutdown function
 * @styleInitFunc:  the module initialization function
 * @styleShutdownFunc:  the module shutdown function
 *
 * Register an XSLT extension module to the library.
 *
 * Returns 0 if sucessful, -1 in case of error
 */
int
xsltRegisterExtModuleFull(const xmlChar * URI,
			  xsltExtInitFunction initFunc,
			  xsltExtShutdownFunction shutdownFunc,
			  xsltStyleExtInitFunction styleInitFunc,
			  xsltStyleExtShutdownFunction styleShutdownFunc)
{
    int ret;
    xsltExtModulePtr module;

    if ((URI == NULL) || (initFunc == NULL))
        return (-1);
    if (xsltExtensionsHash == NULL)
        xsltExtensionsHash = xmlHashCreate(10);

    if (xsltExtensionsHash == NULL)
        return (-1);

    module = xmlHashLookup(xsltExtensionsHash, URI);
    if (module != NULL) {
        if ((module->initFunc == initFunc) &&
            (module->shutdownFunc == shutdownFunc))
            return (0);
        return (-1);
    }
    module = xsltNewExtModule(initFunc, shutdownFunc,
			      styleInitFunc, styleShutdownFunc);
    if (module == NULL)
        return (-1);
    ret = xmlHashAddEntry(xsltExtensionsHash, URI, (void *) module);
    return (ret);
}

/**
 * xsltRegisterExtModule:
 * @URI:  URI associated to this module
 * @initFunc:  the module initialization function
 * @shutdownFunc:  the module shutdown function
 *
 * Register an XSLT extension module to the library.
 *
 * Returns 0 if sucessful, -1 in case of error
 */
int
xsltRegisterExtModule(const xmlChar * URI,
		      xsltExtInitFunction initFunc,
		      xsltExtShutdownFunction shutdownFunc) {
    return xsltRegisterExtModuleFull(URI, initFunc, shutdownFunc,
				     NULL, NULL);
}

/**
 * xsltUnregisterExtModule:
 * @URI:  URI associated to this module
 *
 * Unregister an XSLT extension module from the library.
 *
 * Returns 0 if sucessful, -1 in case of error
 */
int
xsltUnregisterExtModule(const xmlChar * URI)
{
    int ret;

    if (URI == NULL)
        return (-1);
    if (xsltExtensionsHash == NULL)
        return (-1);

    ret =
        xmlHashRemoveEntry(xsltExtensionsHash, URI,
                           (xmlHashDeallocator) xsltFreeExtModule);
    return (ret);
}

/**
 * xsltUnregisterAllExtModules:
 *
 * Unregister all the XSLT extension module from the library.
 */
static void
xsltUnregisterAllExtModules(void)
{
    if (xsltExtensionsHash == NULL)
	return;

    xmlHashFree(xsltExtensionsHash, (xmlHashDeallocator) xsltFreeExtModule);
    xsltExtensionsHash = NULL;
}

/**
 * xsltXPathGetTransformContext:
 * @ctxt:  an XPath transformation context
 *
 * Provides the XSLT transformation context from the XPath transformation
 * context. This is useful when an XPath function in the extension module
 * is called by the XPath interpreter and that the XSLT context is needed
 * for example to retrieve the associated data pertaining to this XSLT
 * transformation.
 *
 * Returns the XSLT transformation context or NULL in case of error.
 */
xsltTransformContextPtr
xsltXPathGetTransformContext(xmlXPathParserContextPtr ctxt)
{
    if ((ctxt == NULL) || (ctxt->context == NULL))
	return(NULL);
    return(ctxt->context->extra);
}

/**
 * xsltRegisterExtModuleFunction:
 * @name:  the function name
 * @URI:  the function namespace URI
 * @function:  the function callback
 *
 * Registers an extension module function.
 *
 * Returns 0 if successful, -1 in case of error.
 */
int
xsltRegisterExtModuleFunction (const xmlChar *name, const xmlChar *URI,
			       xmlXPathFunction function) {
    if ((name == NULL) || (URI == NULL) || (function == NULL))
	return(-1);

    if (xsltFunctionsHash == NULL)
	xsltFunctionsHash = xmlHashCreate(10);
    if (xsltFunctionsHash == NULL)
	return(-1);

    xmlHashUpdateEntry2(xsltFunctionsHash, name, URI,
			(void *) function, NULL);

    return(0);
}

/**
 * xsltExtModuleFunctionLookup:
 * @name:  the function name
 * @URI:  the function namespace URI
 *
 * Looks up an extension module function
 *
 * Returns the function if found, NULL otherwise.
 */
xmlXPathFunction
xsltExtModuleFunctionLookup (const xmlChar *name, const xmlChar *URI) {
    if ((xsltFunctionsHash == NULL) || (name == NULL) || (URI == NULL))
	return(NULL);

    return (xmlXPathFunction) xmlHashLookup2(xsltFunctionsHash, name, URI);
}

/**
 * xsltUnregisterExtModuleFunction:
 * @name:  the function name
 * @URI:  the function namespace URI
 *
 * Unregisters an extension module function
 *
 * Returns 0 if successful, -1 in case of error.
 */
int
xsltUnregisterExtModuleFunction (const xmlChar *name,
				 const xmlChar *URI) {
    if ((xsltFunctionsHash == NULL) || (name == NULL) || (URI == NULL))
	return(-1);

    return xmlHashRemoveEntry2 (xsltFunctionsHash, name, URI, NULL);
}

/**
 * xsltUnregisterAllExtModuleFunction:
 *
 * Unregisters all extension module function
 */
static void
xsltUnregisterAllExtModuleFunction (void) {
    xmlHashFree(xsltFunctionsHash, NULL);
    xsltFunctionsHash = NULL;
}


/**
 * xsltNewElemPreComp:
 * @style:  the XSLT stylesheet
 * @inst:  the element node
 * @function: the transform function
 *
 * Creates and initializes an #xsltElemPreComp
 *
 * Returns the new and initialized #xsltElemPreComp
 */
xsltElemPreCompPtr
xsltNewElemPreComp (xsltStylesheetPtr style, xmlNodePtr inst,
		    xsltTransformFunction function) {
    xsltElemPreCompPtr cur;

    cur = (xsltElemPreCompPtr) xmlMalloc (sizeof(xsltElemPreComp));
    if (cur == NULL) {
	xsltTransformError(NULL, style, NULL,
                         "xsltNewExtElement : malloc failed\n");
        return (NULL);
    }
    memset(cur, 0, sizeof(xsltElemPreComp));

    xsltInitElemPreComp (cur, style, inst, function,
			 (xsltElemPreCompDeallocator) xmlFree);

    return (cur);
}

/**
 * xsltInitElemPreComp:
 * @comp:  an #xsltElemPreComp (or generally a derived structure)
 * @style:  the XSLT stylesheet
 * @inst:  the element node
 * @function:  the transform function
 * @freeFunc:  the @comp deallocator
 *
 * Initializes an existing #xsltElemPreComp structure. This is usefull
 * when extending an #xsltElemPreComp to store precomputed data.
 * This function MUST be called on any extension element precomputed
 * data struct.
 */
void
xsltInitElemPreComp (xsltElemPreCompPtr comp, xsltStylesheetPtr style,
		     xmlNodePtr inst, xsltTransformFunction function,
		     xsltElemPreCompDeallocator freeFunc) {
    comp->type = XSLT_FUNC_EXTENSION;
    comp->func = function;
    comp->inst = inst;
    comp->free = freeFunc;

    comp->next = style->preComps;
    style->preComps = comp;
}

/**
 * xsltPreComputeExtModuleElement:
 * @style:  the stylesheet
 * @inst:  the element node
 *
 * Precomputes an extension module element
 *
 * Returns the precomputed data
 */
xsltElemPreCompPtr
xsltPreComputeExtModuleElement (xsltStylesheetPtr style,
				xmlNodePtr inst) {
    xsltExtElementPtr ext;
    xsltElemPreCompPtr comp = NULL;

    if ((style == NULL) || (inst == NULL) ||
	(inst->type != XML_ELEMENT_NODE) || (inst->ns == NULL))
	return (NULL);

    ext = (xsltExtElementPtr)
	xmlHashLookup2 (xsltElementsHash, inst->name,
			inst->ns->href);
    if (ext == NULL)
	return (NULL);

    if (ext->precomp != NULL)
	comp = ext->precomp(style, inst, ext->transform);
    if (comp == NULL)
	comp = xsltNewElemPreComp (style, inst, ext->transform);

    return (comp);
}

/**
 * xsltRegisterExtModuleElement:
 * @name:  the element name
 * @URI:  the element namespace URI
 * @precomp:  the pre-computation callback
 * @transform:  the transformation callback
 *
 * Registers an extension module element.
 *
 * Returns 0 if successful, -1 in case of error.
 */
int
xsltRegisterExtModuleElement (const xmlChar *name, const xmlChar *URI,
			      xsltPreComputeFunction precomp,
			      xsltTransformFunction transform) {
    xsltExtElementPtr ext;

    if ((name == NULL) || (URI == NULL) || (transform == NULL))
	return(-1);

    if (xsltElementsHash == NULL)
	xsltElementsHash = xmlHashCreate(10);
    if (xsltElementsHash == NULL)
	return(-1);

    ext = xsltNewExtElement(precomp, transform);
    if (ext == NULL)
	return(-1);

    xmlHashUpdateEntry2(xsltElementsHash, name, URI, (void *) ext,
			(xmlHashDeallocator) xsltFreeExtElement);

    return(0);
}

/**
 * xsltExtElementLookup:
 * @ctxt:  an XSLT process context
 * @name:  the element name
 * @URI:  the element namespace URI
 *
 * Looks up an extension element. @ctxt can be NULL to search only in
 * module elements.
 *
 * Returns the element callback or NULL if not found
 */
xsltTransformFunction
xsltExtElementLookup (xsltTransformContextPtr ctxt,
		      const xmlChar *name, const xmlChar *URI) {
    xsltTransformFunction ret;

    if ((name == NULL) || (URI == NULL))
	return(NULL);

    if ((ctxt != NULL) && (ctxt->extElements != NULL)) {
	ret = (xsltTransformFunction)
	    xmlHashLookup2(ctxt->extElements, name, URI);
	if (ret != NULL)
	    return(ret);
    }
    return xsltExtModuleElementLookup(name, URI);
}

/**
 * xsltExtModuleElementLookup:
 * @name:  the element name
 * @URI:  the element namespace URI
 *
 * Looks up an extension module element
 *
 * Returns the callback function if found, NULL otherwise.
 */
xsltTransformFunction
xsltExtModuleElementLookup (const xmlChar *name, const xmlChar *URI) {
    xsltExtElementPtr ext;

    if ((xsltElementsHash == NULL) || (name == NULL) || (URI == NULL))
	return(NULL);

    ext = (xsltExtElementPtr) xmlHashLookup2(xsltElementsHash, name, URI);

    if (ext == NULL)
	return(NULL);
    return(ext->transform);
}

/**
 * xsltExtModuleElementPreComputeLookup:
 * @name:  the element name
 * @URI:  the element namespace URI
 *
 * Looks up an extension module element pre-computation function
 *
 * Returns the callback function if found, NULL otherwise.
 */
xsltPreComputeFunction
xsltExtModuleElementPreComputeLookup (const xmlChar *name,
				      const xmlChar *URI) {
    xsltExtElementPtr ext;

    if ((xsltElementsHash == NULL) || (name == NULL) || (URI == NULL))
	return(NULL);

    ext = (xsltExtElementPtr) xmlHashLookup2(xsltElementsHash, name, URI);

    if (ext == NULL)
	return(NULL);
    return(ext->precomp);
}

/**
 * xsltUnregisterExtModuleElement:
 * @name:  the element name
 * @URI:  the element namespace URI
 *
 * Unregisters an extension module element
 *
 * Returns 0 if successful, -1 in case of error.
 */
int
xsltUnregisterExtModuleElement (const xmlChar *name,
				const xmlChar *URI) {
    if ((xsltElementsHash == NULL) || (name == NULL) || (URI == NULL))
	return(-1);

    return xmlHashRemoveEntry2 (xsltElementsHash, name, URI,
				(xmlHashDeallocator) xsltFreeExtElement);
}

/**
 * xsltUnregisterAllExtModuleElement:
 *
 * Unregisters all extension module element
 */
static void
xsltUnregisterAllExtModuleElement (void) {
    xmlHashFree(xsltElementsHash, (xmlHashDeallocator) xsltFreeExtElement);
    xsltElementsHash = NULL;
}

/**
 * xsltRegisterExtModuleTopLevel:
 * @name:  the top-level element name
 * @URI:  the top-level element namespace URI
 * @function:  the top-level element callback
 *
 * Registers an extension module top-level element.
 *
 * Returns 0 if successful, -1 in case of error.
 */
int
xsltRegisterExtModuleTopLevel (const xmlChar *name, const xmlChar *URI,
			       xsltTopLevelFunction function) {
    if ((name == NULL) || (URI == NULL) || (function == NULL))
	return(-1);

    if (xsltTopLevelsHash == NULL)
	xsltTopLevelsHash = xmlHashCreate(10);
    if (xsltTopLevelsHash == NULL)
	return(-1);

    xmlHashUpdateEntry2(xsltTopLevelsHash, name, URI,
			(void *) function, NULL);

    return(0);
}

/**
 * xsltExtModuleTopLevelLookup:
 * @name:  the top-level element name
 * @URI:  the top-level element namespace URI
 *
 * Looks up an extension module top-level element
 *
 * Returns the callback function if found, NULL otherwise.
 */
xsltTopLevelFunction
xsltExtModuleTopLevelLookup (const xmlChar *name, const xmlChar *URI) {
    if ((xsltTopLevelsHash == NULL) || (name == NULL) || (URI == NULL))
	return(NULL);

    return((xsltTopLevelFunction)
	    xmlHashLookup2(xsltTopLevelsHash, name, URI));
}

/**
 * xsltUnregisterExtModuleTopLevel:
 * @name:  the top-level element name
 * @URI:  the top-level element namespace URI
 *
 * Unregisters an extension module top-level element
 *
 * Returns 0 if successful, -1 in case of error.
 */
int
xsltUnregisterExtModuleTopLevel (const xmlChar *name,
				 const xmlChar *URI) {
    if ((xsltTopLevelsHash == NULL) || (name == NULL) || (URI == NULL))
	return(-1);

    return xmlHashRemoveEntry2 (xsltTopLevelsHash, name, URI, NULL);
}

/**
 * xsltUnregisterAllExtModuleTopLevel:
 *
 * Unregisters all extension module function
 */
static void
xsltUnregisterAllExtModuleTopLevel (void) {
    xmlHashFree(xsltTopLevelsHash, NULL);
    xsltTopLevelsHash = NULL;
}

/**
 * xsltGetExtInfo:
 * @style:  pointer to a stylesheet
 * @URI:    the namespace URI desired
 *
 * looks up URI in extInfos of the stylesheet
 *
 * returns a pointer to the hash table if found, else NULL
 */
xmlHashTablePtr
xsltGetExtInfo (xsltStylesheetPtr style, const xmlChar *URI) {
    xsltExtDataPtr data;
    
    if (style != NULL && style->extInfos != NULL) {
	data = xmlHashLookup(style->extInfos, URI);
	if (data != NULL && data->extData != NULL)
	    return data->extData;
    }
    return NULL;
}

/************************************************************************
 * 									*
 * 		Test module http://xmlsoft.org/XSLT/			*
 * 									*
 ************************************************************************/

/************************************************************************
 * 									*
 * 		Test of the extension module API			*
 * 									*
 ************************************************************************/

static xmlChar *testData = NULL;
static xmlChar *testStyleData = NULL;

/**
 * xsltExtFunctionTest:
 * @ctxt:  the XPath Parser context
 * @nargs:  the number of arguments
 *
 * function libxslt:test() for testing the extensions support.
 */
static void
xsltExtFunctionTest(xmlXPathParserContextPtr ctxt, int nargs ATTRIBUTE_UNUSED)
{
    xsltTransformContextPtr tctxt;
    void *data = NULL;

    tctxt = xsltXPathGetTransformContext(ctxt);

    if (testData == NULL) {
        xsltGenericDebug(xsltGenericDebugContext,
                         "xsltExtFunctionTest: not initialized,"
			 " calling xsltGetExtData\n");
	data = xsltGetExtData(tctxt, (const xmlChar *) XSLT_DEFAULT_URL);
	if (data == NULL) {
	    xsltTransformError(tctxt, NULL, NULL,
			     "xsltExtElementTest: not initialized\n");
	    return;
	}
    }
    if (tctxt == NULL) {
	xsltTransformError(xsltXPathGetTransformContext(ctxt), NULL, NULL,
                         "xsltExtFunctionTest: failed to get the transformation context\n");
        return;
    }
    if (data == NULL)
	data = xsltGetExtData(tctxt, (const xmlChar *) XSLT_DEFAULT_URL);
    if (data == NULL) {
	xsltTransformError(xsltXPathGetTransformContext(ctxt), NULL, NULL,
                         "xsltExtFunctionTest: failed to get module data\n");
        return;
    }
    if (data != testData) {
	xsltTransformError(xsltXPathGetTransformContext(ctxt), NULL, NULL,
                         "xsltExtFunctionTest: got wrong module data\n");
        return;
    }
#ifdef WITH_XSLT_DEBUG_FUNCTION
    xsltGenericDebug(xsltGenericDebugContext,
                     "libxslt:test() called with %d args\n", nargs);
#endif
}

/**
 * xsltExtElementPreCompTest:
 * @style:  the stylesheet
 * @inst:  the instruction in the stylesheet
 *
 * Process a libxslt:test node
 */
static xsltElemPreCompPtr
xsltExtElementPreCompTest(xsltStylesheetPtr style, xmlNodePtr inst,
			  xsltTransformFunction function) {
    xsltElemPreCompPtr ret;

    if (style == NULL) {
	xsltTransformError(NULL, NULL, inst,
		 "xsltExtElementTest: no transformation context\n");
        return (NULL);
    }
    if (testStyleData == NULL) {
        xsltGenericDebug(xsltGenericDebugContext,
		 "xsltExtElementPreCompTest: not initialized,"
		 " calling xsltStyleGetExtData\n");
	xsltStyleGetExtData(style, (const xmlChar *) XSLT_DEFAULT_URL);
	if (testStyleData == NULL) {
	    xsltTransformError(NULL, style, inst,
		 "xsltExtElementPreCompTest: not initialized\n");
	    if (style != NULL) style->errors++;
	    return (NULL);
	}
    }
    if (inst == NULL) {
	xsltTransformError(NULL, style, inst,
		 "xsltExtElementPreCompTest: no instruction\n");
	if (style != NULL) style->errors++;
        return (NULL);
    }
    ret = xsltNewElemPreComp (style, inst, function);
    return (ret);
}

/**
 * xsltExtElementTest:
 * @ctxt:  an XSLT processing context
 * @node:  The current node
 * @inst:  the instruction in the stylesheet
 * @comp:  precomputed informations
 *
 * Process a libxslt:test node
 */
static void
xsltExtElementTest(xsltTransformContextPtr ctxt, xmlNodePtr node,
                   xmlNodePtr inst,
                   xsltElemPreCompPtr comp ATTRIBUTE_UNUSED)
{
    xmlNodePtr commentNode;

    if (testData == NULL) {
        xsltGenericDebug(xsltGenericDebugContext,
                         "xsltExtElementTest: not initialized,"
			 " calling xsltGetExtData\n");
	xsltGetExtData(ctxt, (const xmlChar *) XSLT_DEFAULT_URL);
	if (testData == NULL) {
	    xsltTransformError(ctxt, NULL, inst,
			     "xsltExtElementTest: not initialized\n");
	    return;
	}
    }
    if (ctxt == NULL) {
	xsltTransformError(ctxt, NULL, inst,
                         "xsltExtElementTest: no transformation context\n");
        return;
    }
    if (node == NULL) {
	xsltTransformError(ctxt, NULL, inst,
                         "xsltExtElementTest: no current node\n");
        return;
    }
    if (inst == NULL) {
	xsltTransformError(ctxt, NULL, inst,
                         "xsltExtElementTest: no instruction\n");
        return;
    }
    if (ctxt->insert == NULL) {
	xsltTransformError(ctxt, NULL, inst,
                         "xsltExtElementTest: no insertion point\n");
        return;
    }
    commentNode =
        xmlNewComment((const xmlChar *)
                      "libxslt:test element test worked");
    xmlAddChild(ctxt->insert, commentNode);
}

/**
 * xsltExtInitTest:
 * @ctxt:  an XSLT transformation context
 * @URI:  the namespace URI for the extension
 *
 * A function called at initialization time of an XSLT extension module
 *
 * Returns a pointer to the module specific data for this transformation
 */
static void *
xsltExtInitTest(xsltTransformContextPtr ctxt, const xmlChar * URI) {
    if (testStyleData == NULL) {
        xsltGenericDebug(xsltGenericErrorContext,
                         "xsltExtInitTest: not initialized,"
			 " calling xsltStyleGetExtData\n");
	xsltStyleGetExtData(ctxt->style, URI);
	if (testStyleData == NULL) {
	    xsltTransformError(ctxt, NULL, NULL,
			     "xsltExtInitTest: not initialized\n");
	    return (NULL);
	}
    }	
    if (testData != NULL) {
	xsltTransformError(ctxt, NULL, NULL,
                         "xsltExtInitTest: already initialized\n");
        return (NULL);
    }
    testData = (void *) "test data";
    xsltGenericDebug(xsltGenericDebugContext,
                     "Registered test module : %s\n", URI);
    return (testData);
}


/**
 * xsltExtShutdownTest:
 * @ctxt:  an XSLT transformation context
 * @URI:  the namespace URI for the extension
 * @data:  the data associated to this module
 *
 * A function called at shutdown time of an XSLT extension module
 */
static void
xsltExtShutdownTest(xsltTransformContextPtr ctxt,
                    const xmlChar * URI, void *data) {
    if (testData == NULL) {
	xsltTransformError(ctxt, NULL, NULL,
                         "xsltExtShutdownTest: not initialized\n");
        return;
    }
    if (data != testData) {
	xsltTransformError(ctxt, NULL, NULL,
                         "xsltExtShutdownTest: wrong data\n");
    }
    testData = NULL;
    xsltGenericDebug(xsltGenericDebugContext,
                     "Unregistered test module : %s\n", URI);
}
/**
 * xsltExtStyleInitTest:
 * @style:  an XSLT stylesheet
 * @URI:  the namespace URI for the extension
 *
 * A function called at initialization time of an XSLT extension module
 *
 * Returns a pointer to the module specific data for this transformation
 */
static void *
xsltExtStyleInitTest(xsltStylesheetPtr style ATTRIBUTE_UNUSED,
	             const xmlChar * URI)
{
    if (testStyleData != NULL) {
	xsltTransformError(NULL, NULL, NULL,
                         "xsltExtInitTest: already initialized\n");
        return (NULL);
    }
    testStyleData = (void *) "test data";
    xsltGenericDebug(xsltGenericDebugContext,
                     "Registered test module : %s\n", URI);
    return (testStyleData);
}


/**
 * xsltExtStyleShutdownTest:
 * @style:  an XSLT stylesheet
 * @URI:  the namespace URI for the extension
 * @data:  the data associated to this module
 *
 * A function called at shutdown time of an XSLT extension module
 */
static void
xsltExtStyleShutdownTest(xsltStylesheetPtr style ATTRIBUTE_UNUSED,
			 const xmlChar * URI, void *data) {
    if (testStyleData == NULL) {
        xsltGenericError(xsltGenericErrorContext,
                         "xsltExtShutdownTest: not initialized\n");
        return;
    }
    if (data != testStyleData) {
	xsltTransformError(NULL, NULL, NULL,
                         "xsltExtShutdownTest: wrong data\n");
    }
    testStyleData = NULL;
    xsltGenericDebug(xsltGenericDebugContext,
                     "Unregistered test module : %s\n", URI);
}

/**
 * xsltRegisterTestModule:
 *
 * Registers the test module
 */
void
xsltRegisterTestModule (void) {
    xsltRegisterExtModuleFull((const xmlChar *) XSLT_DEFAULT_URL,
			      xsltExtInitTest, xsltExtShutdownTest,
			      xsltExtStyleInitTest,
			      xsltExtStyleShutdownTest);
    xsltRegisterExtModuleFunction((const xmlChar *) "test",
                            (const xmlChar *) XSLT_DEFAULT_URL,
                            xsltExtFunctionTest);
    xsltRegisterExtModuleElement((const xmlChar *) "test",
				 (const xmlChar *) XSLT_DEFAULT_URL,
				 xsltExtElementPreCompTest ,
				 xsltExtElementTest);
}

/**
 * xsltCleanupGlobals:
 *
 * Unregister all global variables set up by the XSLT library
 */
void
xsltCleanupGlobals(void)
{
    xsltUnregisterAllExtModules();
    xsltUnregisterAllExtModuleFunction();
    xsltUnregisterAllExtModuleElement();
    xsltUnregisterAllExtModuleTopLevel();
}

static void
xsltDebugDumpExtensionsCallback(void* function ATTRIBUTE_UNUSED,
	                        FILE *output, const xmlChar* name,
				const xmlChar* URI,
				const xmlChar* not_used ATTRIBUTE_UNUSED) {
	if (!name||!URI)
		return;
	fprintf(output,"{%s}%s\n",URI,name);
}

static void
xsltDebugDumpExtModulesCallback(void* function ATTRIBUTE_UNUSED,
	                        FILE *output, const xmlChar* URI,
				const xmlChar* not_used ATTRIBUTE_UNUSED,
				const xmlChar* not_used2 ATTRIBUTE_UNUSED) {
	if (!URI)
		return;
	fprintf(output,"%s\n",URI);
}

/**
 * xsltDebugDumpExtensions:
 * @output:  the FILE * for the output, if NULL stdout is used
 *
 * Dumps a list of the registered XSLT extension functions and elements
 */
void
xsltDebugDumpExtensions(FILE * output)
{
	if (output == NULL)
		output = stdout;
    fprintf(output,"Registered XSLT Extensions\n--------------------------\n");
	if (!xsltFunctionsHash)
		fprintf(output,"No registered extension functions\n");
	else {
		fprintf(output,"Registered Extension Functions:\n");
		xmlHashScanFull(xsltFunctionsHash,(xmlHashScannerFull)xsltDebugDumpExtensionsCallback,output);
 	}
	if (!xsltElementsHash)
		fprintf(output,"\nNo registered extension elements\n");
	else {
		fprintf(output,"\nRegistered Extension Elements:\n");
		xmlHashScanFull(xsltElementsHash,(xmlHashScannerFull)xsltDebugDumpExtensionsCallback,output);
 	}
	if (!xsltExtensionsHash)
		fprintf(output,"\nNo registered extension modules\n");
	else {
		fprintf(output,"\nRegistered Extension Modules:\n");
		xmlHashScanFull(xsltExtensionsHash,(xmlHashScannerFull)xsltDebugDumpExtModulesCallback,output);
 	}

}

