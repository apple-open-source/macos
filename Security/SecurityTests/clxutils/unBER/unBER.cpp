/*
 * Decodes and prints ASN.1 BER.
 *
 * Written by Henning Schulzrinne, Columbia University, (c) 1997.
 * Updated by Doug Mitchell 9/6/00 to use oidParser
 */

#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <security_cdsa_utils/cuFileIo.h>
#include <security_cdsa_utils/cuOidParser.h>

static void usage(char **argv)
{
	printf("Usage: %s berfile [o (parse octet strings)] "
		"[b (parse bit strings) ]\n", argv[0]);
	exit(1);
}

#define PARSE_BIT_STR		0x01
#define PARSE_OCTET_STR		0x02

/* bogus infinite length loop index target */
#define INDEFINITE 1000000

typedef unsigned char uchar;

#define CLASS_MASK		0xc0
#define CLASS_UNIV		0x00
#define CLASS_APPL		0x40
#define CLASS_CONTEXT	0x80
#define CLASS_PRIVATE	0xc0
#define CONSTRUCT_BIT	0x20

static int sequence(int t, const char *tag, uchar *b, int length, 
	OidParser &parser, unsigned parseFlags);
static int value(int t, uchar *b, int length, OidParser &parser,
	unsigned parseFlags);

static void indent(int t)
{
  int i;
  for (i = 0; i < t; i++) putchar(' ');
} /* indent */

static int oid(int t, uchar *b, int length, OidParser &parser)
{
  indent(t);
  char oidStr[OID_PARSER_STRING_SIZE];
  parser.oidParse(b, length, oidStr);
  printf("OID <%d>: %s\n", length, oidStr);
  return length;
} /* oid */


static int integer(int t, const char *type, uchar *b, int length)
{
  int i;

  indent(t);
  printf("%s <%d>:", type, length);
  for (i = 0; i < length; i++) {
    printf("%02x ", b[i]);
  }
  printf("\n");
  return length;
} /* integer */


static int bitstring(int t, uchar *b, int length)
{
  int i;

  indent(t);
  printf("BIT STRING <%d, %d>:", length, b[0]);
  for (i = 1; i < length; i++) {
    if ((i % 16) == 0) {
      printf("\n");
      indent(t+3);
    }
    printf("%02x ", b[i]);
  }
  printf("\n");
  return length;  
} /* bitstring */


static int octetstring(int t, uchar *b, int length)
{
  int i;

  indent(t);
  printf("OCTET STRING <%d>:", length);
  for (i = 0; i < length; i++) {
    if ((i % 16) == 0) {
      printf("\n");
      indent(t+3);
    }
    printf("%02x ", b[i]);
  }
  printf("\n");
  return length;  
} /* bitstring */

static int bmpstring(int t, uchar *b, int length)
{
	/* enventually convert via unicode to printable */
	int i;
	
	indent(t);
	printf("BMP STRING <%d>:", length);
	for (i = 0; i < length; i++) {
		if ((i % 16) == 0) {
			printf("\n");
			indent(t+3);
		}
		printf("%02x ", b[i]);
	}
	printf("\n");
	return length;  
} /* bmpstring */



static int string(int t, const char *tag, uchar *b, int length)
{
  indent(t);
  printf("%s <%d>: %*.*s\n", tag, length, length, length, b);
  return length;
} /* string */

static int unparsed(int t, uchar *b, int length)
{
  int i;

  indent(t);
  printf("UNPARSED DATA <%d>:", length);
  for (i = 0; i < length; i++) {
    if ((i % 16) == 0) {
      printf("\n");
      indent(t+3);
    }
    printf("%02x ", b[i]);
  }
  printf("\n");
  return length;  
} /* unparsed */


static int metaClass(int t, 	// indent
	uchar		tag,			// tag
	uchar 		*b, 			// start of contents
	int 		length, 		// content length
	const char 	*className,
	OidParser 	&parser,
	unsigned 	parseFlags)
{
	uchar underlyingTag = tag & ~(CLASS_MASK | CONSTRUCT_BIT);
	indent(t);
	if(length == -1) {
		printf("%s (tag %d) <indefinite> {\n", className, underlyingTag);
	}
	else {
		printf("%s (tag %d) <%d> {\n", className, underlyingTag, length);
	}
	/* just print uninterpreted bytes if !constructed, !universal */
	if( ( ( tag & CLASS_MASK) != CLASS_UNIV) &&
		!(tag & CONSTRUCT_BIT) ) {
		/* fixme - can't do this for indefinite length */
		unparsed(t+3, b, length);
	}
	else {
		length = value(t + 3, b, length, parser, parseFlags);
	}
	indent(t);
	printf("}\n");
	return length;
} /* metaClass */


static int value(
	int t, 					// indent depth
	uchar *b, 				// start of item (at tag)
	int length, 			// length if item, -1 ==> indefinite
	OidParser &parser,		
	unsigned parseFlags)	// 	PARSE_BIT_STR, etc.
{
	int i, j, k;
	int tag_length, length_length, len;
	uchar classId;
	uchar constructBit;
	const char *parseStr = NULL;	// name of recursively parsed bit/octet string
	uchar *parseVal = NULL;	// contents to parse
	unsigned parseLen = 0;
	
	if (length == -1) length = INDEFINITE;
	
	for (i = 0; i < length; ) {
		/* tag length */
		tag_length = 1;
		
		/* get length: short form (single byte) or 0x8b or 0x80 (indefinite) */
		if (b[i+1] == 0x80) {
			len = -1;
			length_length = 1;
		}
		else if (b[i+1] & 0x80) {
			/* long length of n bytes */
			length_length = (b[i+1] & 0x7f) + 1;
			len = 0;
			for (j = 1; j < length_length; j++) {
			len = len * 256 + b[i+1+j];
			}
		}
		else {
			/* short length form */
			len = b[i+1];
			length_length = 1;
		}
		
		/* 
		 * i is index of current tag
		 * len is content length of current item
		 * set k as index to start of content 
		 */
		k = i + tag_length + length_length;
		
		if(length != INDEFINITE) {
			if((k + len) > length) {
				printf("***content overflow\n");
			}
		}
		/* handle special case classes */
		classId = b[i] & CLASS_MASK;
		constructBit = b[i] & CONSTRUCT_BIT;
		
		switch(classId) {
			case CLASS_UNIV:		// normal case handled below
				goto parseTag;
			case CLASS_CONTEXT:
				i += metaClass(t, b[i], &b[k], len, "CONTEXT SPECIFIC", 
					parser, parseFlags);
				break;
			case CLASS_APPL:
				i += metaClass(t, b[i], &b[k], len, "APPLICATION SPECIFIC", 
					parser, parseFlags);
				break;
			case CLASS_PRIVATE:
				i += metaClass(t, b[i], &b[k], len, "PRIVATE", 
					parser, parseFlags);
				break;
			default:
				/* not reached */
				break;
		}
		goto done;			// this item already parsed per class
	parseTag:
		parseStr = NULL;
		parseVal = b + k;	// default recursively parsed value
		parseLen = len;
		
		switch(b[i]) {
			case 0x0:    /* end of indefinite length */
				i += tag_length + length_length;
				return i;
			
			case 0x01:	 /* BOOLEAN, a one-byte integer */
				i += integer(t, "BOOLEAN", &b[k], len);
				break;

			case 0x02:   /* INTEGER */
				i += integer(t, "INTEGER", &b[k], len);
				break;
			
			case 0x03:   /* BIT STRING */
				i += bitstring(t, &b[k], len);
				if(parseFlags & PARSE_OCTET_STR) {
					parseStr = "BIT STRING";
					parseVal++;	// skip reminder byte
					parseLen--;
				}
				break;
			
			case 0x04:   /* OCTET STRING */
				i += octetstring(t, &b[k], len);
				if(parseFlags & PARSE_OCTET_STR) {
					parseStr = "OCTET STRING";
				}
				break;
			
			case 0x5:    /* NULL */
				indent(t);
				printf("NULL\n");
				break;
			
			case 0x06:   /* OBJECT IDENTIFIER */
				i += oid(t, &b[k], len, parser);
				break;
			
			case 0x0A:  /* enumerated */
				i += integer(t, "ENUM", &b[k], len);
				break;
				
			case 0xc:   /* UTF8 string */
				i += string(t, "UTF8String", &b[k], len);
				break;
			
			case 0x13:   /* PrintableString */
				i += string(t, "PrintableString", &b[k], len);
				break;
			
			case 0x14:   /* T61String */
				i += string(t, "T61String", &b[k], len);
				break;
			
			case 0x16:   /* IA5String */
				i += string(t, "IA5String", &b[k], len);
				break;
			
			case 0x17:   /* UTCTime */
				i += string(t, "UTCTime", &b[k], len);
				break;
			
			case 0x18:   /* generalized Time */
				i += string(t, "GenTime", &b[k], len);
				break;
			
			case 0x19:	/* SEC_ASN1_GRAPHIC_STRING */
				i += string(t, "Graphic", &b[k], len);
				break;
				
			case 0x1a:	/* SEC_ASN1_VISIBLE_STRING */
				i += string(t, "Visible", &b[k], len);
				break;
				
			case 0x1b:	/* SEC_ASN1_GENERAL_STRING */
				i += string(t, "General", &b[k], len);
				break;
				
			case 0x1e:	/* BMP string, unicode */
				i += bmpstring(t, &b[k], len);
				break;
				
			case 0xA0:   /* SIGNED? */
				i += sequence(t, "EXPLICIT", &b[k], len, parser, 
					parseFlags);
				break;
			
			case 0x30:   /* SEQUENCE OF */
				i += sequence(t, "SEQUENCE OF", &b[k], len, parser,
					parseFlags);
				break;
			
			case 0x31:   /* SET OF */
				i += sequence(t, "SET OF", &b[k], len, parser,
					parseFlags);
				break;
			
			case 0x39:   /* SET OF */
				i += sequence(t, "structured", &b[k], len, parser,
					parseFlags);
				break;
			
			case 0x24:	/* CONSTRUCTED octet string */
				i += sequence(t, "CONSTR OCTET STRING", &b[k], len, 
					parser, parseFlags);
				if(parseFlags & PARSE_OCTET_STR) {
					parseStr = "OCTET STRING";
				}
				break;

			default:
				printf("ACK! Unknown tag (0x%x); aborting\n", b[i]);
				exit(1);
		}	/* switch tag */
	done:
		if(parseStr) {
			indent(t);
			fpurge(stdin);
			printf("Parse contents (y/anything)? ");
			char resp = getchar();
			if(resp == 'y') {
				indent(t+3);
				printf("Parsed %s contents {\n", parseStr);
				value(t+6, parseVal, parseLen, parser, parseFlags);
				indent(t+3);
				printf("} end of Parsed %s\n", parseStr);
			}
		}
		i += tag_length + length_length;
	}	/* main loop for i to length */
	return i;
} /* value */


static int sequence(int t, const char *tag, uchar *b, int length, 
	OidParser &parser, unsigned parseFlags)
{
  int len;

  indent(t);
  if (length < 0) {
    printf("%s <indefinite> {\n", tag);
  }
  else {
    printf("%s <%d> {\n", tag, length);
  }
  len = value(t + 3, b, length, parser, parseFlags);
  indent(t);
  printf("}\n");
  return len;
} /* sequence */


int main(int argc, char *argv[])
{
	uchar* bfr;
	int i = 0;
	if(argc < 2) {
		usage(argv);
	}
	if(readFile(argv[1], &bfr, (unsigned int *)&i)) {
		printf("Error reading %s\n", argv[1]);
		exit(1);
	}
	
	unsigned parseFlags = 0;
	
	for(int dex=2; dex<argc; dex++) {
		switch(argv[dex][0]) {
			case 'b':
				parseFlags |= PARSE_BIT_STR;
				break;
			case 'o':
				parseFlags |= PARSE_OCTET_STR;
				break;
			default:
				usage(argv);
		}
	}
	
	OidParser parser;
	value(0, bfr, i, parser, parseFlags);
	free(bfr);
	return 0;
} /* main */
