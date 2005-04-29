
#include "tcl.h"

#ifndef XmlParse_INCLUDED
#define XmlParse_INCLUDED 1

#ifdef XML_UNICODE_WCHAR_T
#  define XML_UNICODE
#endif

typedef void *XML_Parser;

#ifdef XML_UNICODE     /* Information is UTF-16 encoded. */
#  ifdef XML_UNICODE_WCHAR_T
typedef wchar_t XML_Char;
typedef wchar_t XML_LChar;
#  else
typedef unsigned short XML_Char;
typedef char XML_LChar;
#  endif
#else                  /* Information is UTF-8 encoded. */
typedef char XML_Char;
typedef char XML_LChar;
#endif

enum XML_Content_Type {
  XML_CTYPE_EMPTY = 1,
  XML_CTYPE_ANY,
  XML_CTYPE_MIXED,
  XML_CTYPE_NAME,
  XML_CTYPE_CHOICE,
  XML_CTYPE_SEQ
};

enum XML_Content_Quant {
  XML_CQUANT_NONE,
  XML_CQUANT_OPT,
  XML_CQUANT_REP,
  XML_CQUANT_PLUS
};

/* If type == XML_CTYPE_EMPTY or XML_CTYPE_ANY, then quant will be
   XML_CQUANT_NONE, and the other fields will be zero or NULL.
   If type == XML_CTYPE_MIXED, then quant will be NONE or REP and
   numchildren will contain number of elements that may be mixed in
   and children point to an array of XML_Content cells that will be
   all of XML_CTYPE_NAME type with no quantification.

   If type == XML_CTYPE_NAME, then the name points to the name, and
   the numchildren field will be zero and children will be NULL. The
   quant fields indicates any quantifiers placed on the name.

   CHOICE and SEQ will have name NULL, the number of children in
   numchildren and children will point, recursively, to an array
   of XML_Content cells.

   The EMPTY, ANY, and MIXED types will only occur at top level.
*/

typedef struct XML_cp XML_Content;

struct XML_cp {
  enum XML_Content_Type		type;
  enum XML_Content_Quant	quant;
  XML_Char *			name;
  unsigned int			numchildren;
  XML_Content *			children;
};


/* This is called for an element declaration. See above for
   description of the model argument. It's the caller's responsibility
   to free model when finished with it.
*/

typedef void (*XML_ElementDeclHandler) (void *userData,
                                        const XML_Char *name,
                                        XML_Content *model);

/*
  The Attlist declaration handler is called for *each* attribute. So
  a single Attlist declaration with multiple attributes declared will
  generate multiple calls to this handler. The "default" parameter
  may be NULL in the case of the "#IMPLIED" or "#REQUIRED" keyword.
  The "isrequired" parameter will be true and the default value will
  be NULL in the case of "#REQUIRED". If "isrequired" is true and
  default is non-NULL, then this is a "#FIXED" default.
 */

typedef void (*XML_AttlistDeclHandler) (void	       *userData,
                                        const XML_Char *elname,
                                        const XML_Char *attname,
                                        const XML_Char *att_type,
                                        const XML_Char *dflt,
                                        int		isrequired);

  /* The XML declaration handler is called for *both* XML declarations and
     text declarations. The way to distinguish is that the version parameter
     will be null for text declarations. The encoding parameter may be null
     for XML declarations. The standalone parameter will be -1, 0, or 1
     indicating respectively that there was no standalone parameter in
     the declaration, that it was given as no, or that it was given as yes.
  */

typedef void (*XML_XmlDeclHandler) (void		*userData,
                                    const XML_Char	*version,
                                    const XML_Char	*encoding,
                                    int		 	 standalone);

/* atts is array of name/value pairs, terminated by 0;
   names and values are 0 terminated. */

typedef void (*XML_StartElementHandler)(void *userData,
                                        const XML_Char *name,
                                        const XML_Char **atts);

typedef void (*XML_EndElementHandler)(void *userData,
                                      const XML_Char *name);


/* s is not 0 terminated. */
typedef void (*XML_CharacterDataHandler)(void *userData,
                                         const XML_Char *s,
                                         int len);

/* target and data are 0 terminated */
typedef void (*XML_ProcessingInstructionHandler)(void *userData,
                                                 const XML_Char *target,
                                                 const XML_Char *data);

/* data is 0 terminated */
typedef void (*XML_CommentHandler)(void *userData, const XML_Char *data);

typedef void (*XML_StartCdataSectionHandler)(void *userData);
typedef void (*XML_EndCdataSectionHandler)(void *userData);

/* This is called for any characters in the XML document for
which there is no applicable handler.  This includes both
characters that are part of markup which is of a kind that is
not reported (comments, markup declarations), or characters
that are part of a construct which could be reported but
for which no handler has been supplied. The characters are passed
exactly as they were in the XML document except that
they will be encoded in UTF-8.  Line boundaries are not normalized.
Note that a byte order mark character is not passed to the default handler.
There are no guarantees about how characters are divided between calls
to the default handler: for example, a comment might be split between
multiple calls. */

typedef void (*XML_DefaultHandler)(void *userData,
                                   const XML_Char *s,
                                   int len);

/* This is called for the start of the DOCTYPE declaration, before
   any DTD or internal subset is parsed. */

typedef void (*XML_StartDoctypeDeclHandler)(void *userData,
                                            const XML_Char *doctypeName,
                                            const XML_Char *sysid,
                                            const XML_Char *pubid,
                                            int has_internal_subset);

/* This is called for the start of the DOCTYPE declaration when the
closing > is encountered, but after processing any external subset. */
typedef void (*XML_EndDoctypeDeclHandler)(void *userData);

/* This is called for entity declarations. The is_parameter_entity
   argument will be non-zero if the entity is a parameter entity, zero
   otherwise.

   For internal entities (<!ENTITY foo "bar">), value will
   be non-null and systemId, publicID, and notationName will be null.
   The value string is NOT null terminated; the length is provided in
   the value_length argument. Since it is legal to have zero-length
   values, do not use this argument to test for internal entities.

   For external entities, value will be null and systemId will be non-null.
   The publicId argument will be null unless a public identifier was
   provided. The notationName argument will have a non-null value only
   for unparsed entity declarations.
*/

typedef void (*XML_EntityDeclHandler) (void *userData,
                                       const XML_Char *entityName,
                                       int is_parameter_entity,
                                       const XML_Char *value,
                                       int value_length,
                                       const XML_Char *base,
                                       const XML_Char *systemId,
                                       const XML_Char *publicId,
                                       const XML_Char *notationName);
				
/* This is called for a declaration of notation.
The base argument is whatever was set by XML_SetBase.
The notationName will never be null.  The other arguments can be. */

typedef void (*XML_NotationDeclHandler)(void *userData,
                                        const XML_Char *notationName,
                                        const XML_Char *base,
                                        const XML_Char *systemId,
                                        const XML_Char *publicId);

/* When namespace processing is enabled, these are called once for
each namespace declaration. The call to the start and end element
handlers occur between the calls to the start and end namespace
declaration handlers. For an xmlns attribute, prefix will be null.
For an xmlns="" attribute, uri will be null. */

typedef void (*XML_StartNamespaceDeclHandler)(void *userData,
                                              const XML_Char *prefix,
                                              const XML_Char *uri);

typedef void (*XML_EndNamespaceDeclHandler)(void *userData,
                                            const XML_Char *prefix);

/* This is called if the document is not standalone (it has an
external subset or a reference to a parameter entity, but does not
have standalone="yes"). If this handler returns 0, then processing
will not continue, and the parser will return a
XML_ERROR_NOT_STANDALONE error. */

typedef int (*XML_NotStandaloneHandler)(void *userData);

/* This is called for a reference to an external parsed general entity.
The referenced entity is not automatically parsed.
The application can parse it immediately or later using
XML_ExternalEntityParserCreate.
The parser argument is the parser parsing the entity containing the reference;
it can be passed as the parser argument to XML_ExternalEntityParserCreate.
The systemId argument is the system identifier as specified in the entity
declaration; it will not be null.
The base argument is the system identifier that should be used as the base for
resolving systemId if systemId was relative; this is set by XML_SetBase;
it may be null.
The publicId argument is the public identifier as specified in the entity
declaration, or null if none was specified; the whitespace in the public
identifier will have been normalized as required by the XML spec.
The context argument specifies the parsing context in the format
expected by the context argument to
XML_ExternalEntityParserCreate; context is valid only until the handler
returns, so if the referenced entity is to be parsed later, it must be copied.
The handler should return 0 if processing should not continue because of
a fatal error in the handling of the external entity.
In this case the calling parser will return an
XML_ERROR_EXTERNAL_ENTITY_HANDLING error.
Note that unlike other handlers the first argument is the parser, not
userData. */

typedef int (*XML_ExternalEntityRefHandler)(XML_Parser parser,
                                            const XML_Char *context,
                                            const XML_Char *base,
                                            const XML_Char *systemId,
                                            const XML_Char *publicId);

/* This structure is filled in by the XML_UnknownEncodingHandler
to provide information to the parser about encodings that are unknown
to the parser.
The map[b] member gives information about byte sequences
whose first byte is b.
If map[b] is c where c is >= 0, then b by itself encodes the Unicode scalar
value c.
If map[b] is -1, then the byte sequence is malformed.
If map[b] is -n, where n >= 2, then b is the first byte of an n-byte
sequence that encodes a single Unicode scalar value.
The data member will be passed as the first argument to the convert function.
The convert function is used to convert multibyte sequences;
s will point to a n-byte sequence where map[(unsigned char)*s] == -n.
The convert function must return the Unicode scalar value
represented by this byte sequence or -1 if the byte sequence is malformed.
The convert function may be null if the encoding is a single-byte encoding,
that is if map[b] >= -1 for all bytes b.
When the parser is finished with the encoding, then if release is not null,
it will call release passing it the data member;
once release has been called, the convert function will not be called again.

Expat places certain restrictions on the encodings that are supported
using this mechanism.

1. Every ASCII character that can appear in a well-formed XML document,
other than the characters

  $@\^`{}~

must be represented by a single byte, and that byte must be the
same byte that represents that character in ASCII.

2. No character may require more than 4 bytes to encode.

3. All characters encoded must have Unicode scalar values <= 0xFFFF, (i.e.,
characters that would be encoded by surrogates in UTF-16 are  not
allowed).  Note that this restriction doesn't apply to the built-in
support for UTF-8 and UTF-16.

4. No Unicode character may be encoded by more than one distinct sequence
of bytes. */

typedef struct {
  int map[256];
  void *data;
  int (*convert)(void *data, const char *s);
  void (*release)(void *data);
} XML_Encoding;

/* This is called for an encoding that is unknown to the parser.
The encodingHandlerData argument is that which was passed as the
second argument to XML_SetUnknownEncodingHandler.
The name argument gives the name of the encoding as specified in
the encoding declaration.
If the callback can provide information about the encoding,
it must fill in the XML_Encoding structure, and return 1.
Otherwise it must return 0.
If info does not describe a suitable encoding,
then the parser will return an XML_UNKNOWN_ENCODING error. */

typedef int (*XML_UnknownEncodingHandler)(void *encodingHandlerData,
                                          const XML_Char *name,
                                          XML_Encoding *info);

/* Returns the last value set by XML_SetUserData or null. */
#define XML_GetUserData(parser) (*(void **)(parser))


typedef struct {
  int major;
  int minor;
  int micro;
} XML_Expat_Version;


/* Expat follows the GNU/Linux convention of odd number minor version for
   beta/development releases and even number minor version for stable
   releases. Micro is bumped with each release, and set to 0 with each
   change to major or minor version. */

#define XML_MAJOR_VERSION 1
#define XML_MINOR_VERSION 95
#define XML_MICRO_VERSION 3

#endif



/*  typedef struct TclGenExpatInfoStruct TclGenExpatInfo ; */
struct TclGenExpatInfo;

typedef void (*CHandlerSet_userDataReset)(Tcl_Interp *interp, void *userData);
typedef void (*CHandlerSet_userDataFree)(Tcl_Interp *interp, void *userData);
typedef void (*CHandlerSet_parserReset)(XML_Parser parser, void *userData);
typedef void (*CHandlerSet_initParse)(const struct TclGenExpatInfo *expat,
                                      void *userData);

typedef struct CHandlerSet {
    struct CHandlerSet *nextHandlerSet;
    char *name;                     /* refname of the handler set */
    int ignoreWhiteCDATAs;          /* ignore 'white' CDATA sections */

    void *userData;                 /* Handler set specific Data Structure;
                                       the C handler set extention has to
                                       malloc the needed structure in his
                                       init func and has to provide a
                                       cleanup func (to free it). */

    CHandlerSet_userDataReset        resetProc;
    CHandlerSet_userDataFree         freeProc;
    CHandlerSet_parserReset          parserResetProc;
    CHandlerSet_initParse            initParseProc;

    /* C func for element start */
    XML_StartElementHandler          elementstartcommand;
    /* C func for element end */
    XML_EndElementHandler            elementendcommand;
    /* C func for character data */
    XML_CharacterDataHandler         datacommand;
    /* C func for namespace decl start */
    XML_StartNamespaceDeclHandler    startnsdeclcommand;
    /* C func for namespace decl end */
    XML_EndNamespaceDeclHandler      endnsdeclcommand;
    /* C func for processing instruction */
    XML_ProcessingInstructionHandler picommand;
    /* C func for default data */
    XML_DefaultHandler               defaultcommand;
    /* C func for unparsed entity declaration */
    XML_NotationDeclHandler          notationcommand;
    /* C func for external entity */
    XML_ExternalEntityRefHandler     externalentitycommand;
    /* C func for unknown encoding */
    XML_UnknownEncodingHandler       unknownencodingcommand;
    /* C func for comments */
    XML_CommentHandler               commentCommand;
    /* C func for "not standalone" docs */
    XML_NotStandaloneHandler         notStandaloneCommand;
    /* C func for CDATA section start */
    XML_StartCdataSectionHandler     startCdataSectionCommand;
    /* C func for CDATA section end */
    XML_EndCdataSectionHandler       endCdataSectionCommand;
    /* C func for <!ELEMENT decl's */
    XML_ElementDeclHandler           elementDeclCommand;
    /* C func for <!ATTLIST decl's */
    XML_AttlistDeclHandler           attlistDeclCommand;
    /* C func for <!DOCTYPE decl's */
    XML_StartDoctypeDeclHandler      startDoctypeDeclCommand;
    /* C func for <!DOCTYPE decl ends */
    XML_EndDoctypeDeclHandler        endDoctypeDeclCommand;
    /* C func for <?XML decl's */
    XML_XmlDeclHandler               xmlDeclCommand;
    /* C func for <!ENTITY decls's */
    XML_EntityDeclHandler            entityDeclCommand;
} CHandlerSet;

/*----------------------------------------------------------------------------
|   The structure below is used to refer to an event handler set
|   of tcl scripts.
\---------------------------------------------------------------------------*/

typedef struct TclHandlerSet {
    struct TclHandlerSet *nextHandlerSet;
    char *name;                     /* refname of the handler set */
    int status;                     /* handler set status */
    int continueCount;		    /* reference count for continue */
    int ignoreWhiteCDATAs;          /* ignore 'white' CDATA sections */

    Tcl_Obj *elementstartcommand;      /* Script for element start */
    Tcl_ObjCmdProc *elementstartObjProc;
    ClientData      elementstartclientData;
    Tcl_Obj *elementendcommand;        /* Script for element end */
    Tcl_ObjCmdProc *elementendObjProc;
    ClientData      elementendclientData;
    Tcl_Obj *datacommand;	       /* Script for character data */
    Tcl_ObjCmdProc *datacommandObjProc;
    ClientData      datacommandclientData;
    Tcl_Obj *startnsdeclcommand;       /* Script for namespace decl start */
    Tcl_Obj *endnsdeclcommand;         /* Script for namespace decl end */
    Tcl_Obj *picommand;		       /* Script for processing instruction */
    Tcl_Obj *defaultcommand;	       /* Script for default data */
    Tcl_Obj *notationcommand;	       /* Script for notation declaration */
    Tcl_Obj *externalentitycommand;    /* Script for external entity */
    Tcl_Obj *unknownencodingcommand;   /* Script for unknown encoding */
    Tcl_Obj *commentCommand;           /* Script for comments */
    Tcl_Obj *notStandaloneCommand;     /* Script for "not standalone" docs */
    Tcl_Obj *startCdataSectionCommand; /* Script for CDATA section start */
    Tcl_Obj *endCdataSectionCommand;   /* Script for CDATA section end */
    Tcl_Obj *elementDeclCommand;       /* Script for <!ELEMENT decl's */
    Tcl_Obj *attlistDeclCommand;       /* Script for <!ATTLIST decl's */
    Tcl_Obj *startDoctypeDeclCommand;  /* Script for <!DOCTYPE decl's */
    Tcl_Obj *endDoctypeDeclCommand;    /* Script for <!DOCTYPE decl ends */
    Tcl_Obj *xmlDeclCommand;           /* Script for <?XML decl's */
    Tcl_Obj *entityDeclCommand;        /* Script for <!ENTITY decl's */
} TclHandlerSet;

typedef struct TclGenExpatInfo {
    XML_Parser  parser;		/* The expat parser structure */
    Tcl_Interp *interp;		/* Interpreter for this instance */
    Tcl_Obj    *name;		/* name of this instance */
    int final;			/* input data complete? */
    int needWSCheck;            /* Any handler set has ignoreWhiteCDATAs==1? */
    int status;			/* application status */
    Tcl_Obj *result;		/* application return result */
    const char *context;        /* reference to the context pointer */
    Tcl_Obj *cdata;             /* Accumulates character data */
    int      ns_mode;           /* namespace mode */
    XML_Char nsSeparator;

    TclHandlerSet *firstTclHandlerSet;
    CHandlerSet *firstCHandlerSet;
} TclGenExpatInfo;

#include "dom.h"
#include "tdomDecls.h"



