/*
 * dynamic.c: Implementation of the EXSLT -- Dynamic module
 *
 * References:
 *   http://www.exslt.org/dyn/dyn.html
 *
 * See Copyright for the status of this software.
 *
 * Authors:
 *   Mark Vakoc <mark_vakoc@jdedwards.com>
 *   Thomas Broyer <tbroyer@ltgt.net>
 *
 * TODO:
 * elements:
 * functions:
 *    min
 *    max
 *    sum
 *    map
 *    closure
 */

#define IN_LIBEXSLT
#include "libexslt/libexslt.h"

#if defined(WIN32) && !defined (__CYGWIN__) && (!__MINGW32__)
#include <win32config.h>
#else
#include "config.h"
#endif

#include <libxml/tree.h>
#include <libxml/xpath.h>
#include <libxml/xpathInternals.h>

#include <libxslt/xsltconfig.h>
#include <libxslt/xsltutils.h>
#include <libxslt/xsltInternals.h>
#include <libxslt/extensions.h>

#include "exslt.h"

/**
 * exsltDynEvaluateFunction:
 * @ctxt:  an XPath parser context
 * @nargs:  the number of arguments
 *
 * Evaluates the string as an XPath expression and returns the result
 * value, which may be a boolean, number, string, node set, result tree
 * fragment or external object.
 */

static void
exsltDynEvaluateFunction(xmlXPathParserContextPtr ctxt, int nargs) {
	xmlChar *str = NULL;
	xmlXPathObjectPtr ret = NULL;

	if (ctxt == NULL)
		return;
	if (nargs != 1) {
		xsltPrintErrorContext(xsltXPathGetTransformContext(ctxt), NULL, NULL);
        xsltGenericError(xsltGenericErrorContext,
			"dyn:evalute() : invalid number of args %d\n", nargs);
		ctxt->error = XPATH_INVALID_ARITY;
		return;
	}
	str = xmlXPathPopString(ctxt);
	/* return an empty node-set if an empty string is passed in */
	if (!str||!xmlStrlen(str)) {
		if (str) xmlFree(str);
		valuePush(ctxt,xmlXPathNewNodeSet(NULL));
		return;
	}
	ret = xmlXPathEval(str,ctxt->context);
	if (ret)
		valuePush(ctxt,ret);
 	else {
		xsltGenericError(xsltGenericErrorContext,
			"dyn:evaluate() : unable to evaluate expression '%s'\n",str);
		valuePush(ctxt,xmlXPathNewNodeSet(NULL));
	}	
	xmlFree(str);
	return;
}

/**
 * exsltDynRegister:
 *
 * Registers the EXSLT - Dynamic module
 */

void
exsltDynRegister (void) {
    xsltRegisterExtModuleFunction ((const xmlChar *) "evaluate",
				   EXSLT_DYNAMIC_NAMESPACE,
				   exsltDynEvaluateFunction);
}
