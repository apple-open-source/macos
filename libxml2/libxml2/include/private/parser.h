#ifndef XML_PARSER_H_PRIVATE__
#define XML_PARSER_H_PRIVATE__

#include <libxml/parser.h>
#include <libxml/xmlversion.h>

#define XML_MAX_URI_LENGTH 2000

XML_HIDDEN void
xmlHaltParser(xmlParserCtxtPtr ctxt);

#endif /* XML_PARSER_H_PRIVATE__ */
