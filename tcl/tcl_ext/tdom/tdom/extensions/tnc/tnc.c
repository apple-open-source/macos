/* This code implements a set of tDOM C Handlers that can be
   dynamically loaded into a tclsh with already loaded tDOM
   package. This parser extension does some tests according to the DTD
   against the data within an XML document.

   Copyright (c) 2001-2003 Rolf Ade */

#include <tdom.h>
#include <string.h>
#include <stdlib.h>

/*
 * Beginning with 8.4, Tcl API is CONST'ified
 */
#if (TCL_MAJOR_VERSION == 8) && (TCL_MINOR_VERSION <= 3)
# define CONST84
#endif

#ifndef TCL_THREADS
# define TDomThreaded(x)
#else
# define TDomThreaded(x) x
#endif

/* The inital stack sizes must be at least 1 */
#define TNC_INITCONTENTSTACKSIZE 512

/*----------------------------------------------------------------------------
|   local globals
|
\---------------------------------------------------------------------------*/
/* Counter to generate unique validateCmd names */
static int uniqueCounter = 0;  
TDomThreaded(static Tcl_Mutex counterMutex;) /* Protect the counter */

/* To enable some debugging output at stdout use this.
   But beware: this debugging output isn't systematic
   and only understandable, if you know the internals
   of tnc. */
/*  #define TNC_DEBUG */

/* The elements of TNC_Content carry exactly the same information
   as expats XML_Content. But the element is identified by his
   Tcl_HashEntry entry within the "tagNames" Hashtable (see TNC_Data)
   and not the element name. This should be much more efficient. */
typedef struct TNC_cp TNC_Content;
typedef struct TNC_elemAttInfo TNC_ElemAttInfo;

struct TNC_cp
{
    enum XML_Content_Type   type;
    enum XML_Content_Quant  quant;
    Tcl_HashEntry          *nameId;
    unsigned int            numchildren;
    TNC_Content            *children;
    TNC_ElemAttInfo        *attInfo;
};

typedef struct TNC_contentStack
{
    TNC_Content  *model;
    int           activeChild;
    int           deep;
    int           alreadymatched;
} TNC_ContentStack;


typedef struct TNC_data
{
    char             *doctypeName;            /* From DOCTYPE declaration */
    int               ignoreWhiteCDATAs;      /* Flag: white space allowed in 
                                                 current content model? */
    int               ignorePCDATA;           /* Flag: currently mixed content
                                                 model? */
    Tcl_HashTable    *tagNames;               /* Hash table of all ELEMENT
                                                 declarations of the DTD.
                                                 Element name is the key.
                                                 While parsing, entry points
                                                 to the XML_Content of that
                                                 Element, after finishing of
                                                 DTD parsing, entry holds a
                                                 pointer to the TNC_Content
                                                 of that element. */
    TNC_ElemAttInfo  *elemAttInfo;            /* TncElementStartCommand stores
                                                 the elemAttInfo pointer of
                                                 the current element here for
                                                 DOM validation, to avoid two
                                                 element name lookups. */
    int               elemContentsRewriten;   /* Signals, if the tagNames
                                                 entries point to
                                                 TNC_Contents */
    int               status;                 /* While used with expat obj:
                                                 1 after successful parsed
                                                 DTD, 0 otherwise.
                                                 For validateCmd used for
                                                 error report during
                                                 validation: 0 OK, 1 validation
                                                 error. */
    int               idCheck;                /* Flag: check IDREF resolution*/
    Tcl_HashTable    *attDefsTables;          /* Used to store ATTLIST 
                                                 declarations while parsing.
                                                 Keys are the element names. */
    Tcl_HashTable    *entityDecls;            /* Used to store ENTITY
                                                 declarations. */
    Tcl_HashTable    *notationDecls;          /* Used to store NOTATION
                                                 declarations. */
    Tcl_HashTable    *ids;                    /* Used to track IDs */
    Tcl_Interp       *interp;                 
    Tcl_Obj          *expatObj;               /* If != NULL, points to the
                                                 parserCmd structure. NULL
                                                 for ValidateCmds. Used, to
                                                 distinguish between SAX
                                                 and DOM validation. */
    int               contentStackSize;       /* Current size of the content
                                                 stack */
    int               contentStackPtr;        /* Points to the currently active
                                                 content model on the stack */
    TNC_ContentStack *contentStack;           /* Stack for the currently
                                                 nested open content models. */
} TNC_Data;

typedef enum TNC_attType {
    TNC_ATTTYPE_CDATA,
    TNC_ATTTYPE_ID,
    TNC_ATTTYPE_IDREF,
    TNC_ATTTYPE_IDREFS,
    TNC_ATTTYPE_ENTITY,
    TNC_ATTTYPE_ENTITIES,
    TNC_ATTTYPE_NMTOKEN,
    TNC_ATTTYPE_NMTOKENS,
    TNC_ATTTYPE_NOTATION,
    TNC_ATTTYPE_ENUMERATION,
} TNC_AttType;

struct TNC_elemAttInfo
{
    Tcl_HashTable *attributes;
    int            nrOfreq;
    int            nrOfIdAtts;
};

typedef struct TNC_attDecl
{
    TNC_AttType    att_type;
    char          *dflt;
    int            isrequired;
    Tcl_HashTable *lookupTable;   /* either NotationTypes or enum values */
} TNC_AttDecl;

typedef struct TNC_entityInfo
{
    int    is_notation;
    char  *notationName;
} TNC_EntityInfo;

typedef Tcl_HashEntry TNC_NameId;

static char tnc_usage[] =
               "Usage tnc <expat parser obj> <subCommand>, where subCommand can be: \n"
               "        enable    \n"
               "        remove    \n"
               "        getValidateCmd ?cmdName?\n"
               ;

static char validateCmd_usage[] =
               "Usage validateCmd <method> <args>, where method can be: \n"
               "        validateDocument <domDocument>                  \n"
               "        validateTree <domNode>                          \n"
               "        validateAttributes <domNode>                    \n"
               "        delete                                          \n" 
               ;

enum TNC_Error {
    TNC_ERROR_NONE,
    TNC_ERROR_DUPLICATE_ELEMENT_DECL,
    TNC_ERROR_DUPLICATE_MIXED_ELEMENT,
    TNC_ERROR_UNKNOWN_ELEMENT,
    TNC_ERROR_EMPTY_ELEMENT,
    TNC_ERROR_DISALLOWED_PCDATA,
    TNC_ERROR_DISALLOWED_CDATA,
    TNC_ERROR_NO_DOCTYPE_DECL,
    TNC_ERROR_WRONG_ROOT_ELEMENT,
    TNC_ERROR_NO_ATTRIBUTES,
    TNC_ERROR_UNKOWN_ATTRIBUTE,
    TNC_ERROR_WRONG_FIXED_ATTVALUE,
    TNC_ERROR_MISSING_REQUIRED_ATTRIBUTE,
    TNC_ERROR_MORE_THAN_ONE_ID_ATT,
    TNC_ERROR_ID_ATT_DEFAULT,
    TNC_ERROR_DUPLICATE_ID_VALUE,
    TNC_ERROR_UNKOWN_ID_REFERRED,
    TNC_ERROR_ENTITY_ATTRIBUTE,
    TNC_ERROR_ENTITIES_ATTRIBUTE,
    TNC_ERROR_ATT_ENTITY_DEFAULT_MUST_BE_DECLARED,
    TNC_ERROR_NOTATION_REQUIRED,
    TNC_ERROR_NOTATION_MUST_BE_DECLARED,
    TNC_ERROR_IMPOSSIBLE_DEFAULT,
    TNC_ERROR_ENUM_ATT_WRONG_VALUE,
    TNC_ERROR_NMTOKEN_REQUIRED,
    TNC_ERROR_NAME_REQUIRED,
    TNC_ERROR_NAMES_REQUIRED,
    TNC_ERROR_ELEMENT_NOT_ALLOWED_HERE,
    TNC_ERROR_ELEMENT_CAN_NOT_END_HERE,
    TNC_ERROR_ONLY_THREE_BYTE_UTF8,
    TNC_ERROR_UNKNOWN_NODE_TYPE
};

const char *
TNC_ErrorString (int code)
{
    static const char *message[] = {
        "No error.",
        "Element declared more than once.",
        "The same name must not appear more than once in \n\tone mixed-content declaration.",
        "No declaration for this element.",
        "Element is declared to be empty, but isn't.",
        "PCDATA not allowed here.",
        "CDATA section not allowed here.",
        "No DOCTYPE declaration.",
        "Root element doesn't match DOCTYPE name.",
        "No attributes defined for this element.",
        "Unknown attribute for this element.",
        "Attribute value must match the FIXED default.",
        "Required attribute missing.",
        "Only one attribute with type ID allowed.",
        "No default value allowed for attribute type ID.",
        "ID attribute values must be unique within the document.",
        "Unknown ID referred.",
        "Attribute value has to be a unparsed entity.",
        "Attribute value has to be a sequence of unparsed entities.",
        "The defaults of attributes with type ENTITY or ENTITIES\nhas to be unparsed entities.",
        "Attribute value has to be one of the allowed notations.",
        "Every used NOTATION must be declared.",
        "Attribute default is not one of the allowed values",
        "Attribute hasn't one of the allowed values.",
        "Attribute value has to be a NMTOKEN.",
        "Attribute value has to be a Name.",
        "Attribute value has to match production Names.",
        "Element is not allowed here.",
        "Element can not end here (required element(s) missing).",
        "Can only handle UTF8 chars up to 3 bytes length."
        "Unknown or unexpected dom node type."
    };
/*      if (code > 0 && code < sizeof(message)/sizeof(message[0])) */
        return message[code];
    return 0;
}

#define CHECK_UTF_CHARLEN(d) if (!(d)) { \
                                signalNotValid (userData, TNC_ERROR_ONLY_THREE_BYTE_UTF8);\
                                return;\
                             }

#define CHECK_UTF_CHARLENR(d) if (!(d)) { \
                                signalNotValid (userData, TNC_ERROR_ONLY_THREE_BYTE_UTF8);\
                                return 0;\
                             }

#define CHECK_UTF_CHARLEN_COPY(d) if (!(d)) { \
                                signalNotValid (userData, TNC_ERROR_ONLY_THREE_BYTE_UTF8);\
                                FREE (copy);\
                                return;\
                                }

#define SetResult(str) Tcl_ResetResult(interp); \
                     Tcl_SetStringObj(Tcl_GetObjResult(interp), (str), -1)

#define SetBooleanResult(i) Tcl_ResetResult(interp); \
                     Tcl_SetBooleanObj(Tcl_GetObjResult(interp), (i))

extern char *Tdom_InitStubs (Tcl_Interp *interp, char *version, int exact);

static void
signalNotValid (userData, code)
    void        *userData;
    int          code;
{
    TNC_Data *tncdata = (TNC_Data *) userData;
    TclGenExpatInfo *expat;
    char s[1000];

    if (tncdata->expatObj) {
        expat = GetExpatInfo (tncdata->interp, tncdata->expatObj);
        sprintf (s, "Validation error at line %ld, character %ld: %s",
                 XML_GetCurrentLineNumber (expat->parser),
                 XML_GetCurrentColumnNumber (expat->parser),
                 TNC_ErrorString (code));
        expat->status = TCL_ERROR;
        expat->result = Tcl_NewStringObj (s, -1);
        Tcl_IncrRefCount (expat->result);
    } else {
        tncdata->status = 1;
        Tcl_SetResult (tncdata->interp, (char *)TNC_ErrorString (code),
                       TCL_VOLATILE);
    }
}

/*
 *----------------------------------------------------------------------------
 *
 * FindUniqueCmdName --
 *
 *	Generate new command name. Used for getValidateCmd.
 *
 * Results:
 *	Returns newly allocated Tcl object containing name.
 *
 * Side effects:
 *	Allocates Tcl object.
 *
 *----------------------------------------------------------------------------
 */

static void
FindUniqueCmdName(
    Tcl_Interp *interp,
    char       *s
    )
{
    Tcl_CmdInfo info;
    
    TDomThreaded(Tcl_MutexLock(&counterMutex);)
        do {
            sprintf(s, "DTDvalidator%d", uniqueCounter++);
            
        } while (Tcl_GetCommandInfo(interp, s, &info));
    TDomThreaded(Tcl_MutexUnlock(&counterMutex);)
}

/*
 *----------------------------------------------------------------------------
 *
 * TncStartDoctypeDeclHandler --
 *
 *	This procedure is called for the start of the DOCTYPE
 *      declaration.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Stores the doctype Name in the TNC_data.
 *
 *----------------------------------------------------------------------------
 */

void
TncStartDoctypeDeclHandler (userData, doctypeName, sysid, pubid, has_internal_subset)
    void       *userData;
    const char *doctypeName;
    const char *sysid;
    const char *pubid;
    int         has_internal_subset;
{
    TNC_Data *tncdata = (TNC_Data *) userData;

#ifdef TNC_DEBUG
    printf ("TncStartDoctypeDeclHandler start\n");
#endif
    tncdata->doctypeName = tdomstrdup (doctypeName);
}


/*
 *----------------------------------------------------------------------------
 *
 * TncFreeTncModel --
 *
 *	This helper procedure frees recursively TNC_Contents.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Frees memory.
 *
 *----------------------------------------------------------------------------
 */

static void
TncFreeTncModel (tmodel)
    TNC_Content *tmodel;
{
    unsigned int i;

    if (tmodel->children) {
        for (i = 0; i < tmodel->numchildren; i++) {
            TncFreeTncModel (&tmodel->children[i]);
        }
        FREE ((char *) tmodel->children);
    }
}


/*
 *----------------------------------------------------------------------------
 *
 * TncRewriteModel --
 *
 *	This helper procedure creates recursively a TNC_Content from
 *      a XML_Content.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Allocates memory for the TNC_Content models.
 *
 *----------------------------------------------------------------------------
 */

static void
TncRewriteModel (emodel, tmodel, tagNames)
    XML_Content   *emodel;
    TNC_Content   *tmodel;
    Tcl_HashTable *tagNames;
{
    Tcl_HashEntry *entryPtr;
    unsigned int i;

    tmodel->type = emodel->type;
    tmodel->quant = emodel->quant;
    tmodel->numchildren = emodel->numchildren;
    tmodel->children = NULL;
    tmodel->nameId = NULL;
    switch (emodel->type) {
    case XML_CTYPE_MIXED:
        if (emodel->quant == XML_CQUANT_REP) {
            tmodel->children = (TNC_Content *)
                MALLOC (sizeof (TNC_Content) * emodel->numchildren);
            for (i = 0; i < emodel->numchildren; i++) {
                TncRewriteModel (&emodel->children[i], &tmodel->children[i],
                                 tagNames);
            }
        }
        break;
    case XML_CTYPE_ANY:
    case XML_CTYPE_EMPTY:
        /* do nothing */
        break;
    case XML_CTYPE_SEQ:
    case XML_CTYPE_CHOICE:
        tmodel->children = (TNC_Content *)
            MALLOC (sizeof (TNC_Content) * emodel->numchildren);
        for (i = 0; i < emodel->numchildren; i++) {
            TncRewriteModel (&emodel->children[i], &tmodel->children[i],
                             tagNames);
        }
        break;
    case XML_CTYPE_NAME:
        entryPtr = Tcl_FindHashEntry (tagNames, emodel->name);
        /* Notice, that it is possible for entryPtr to be NULL.
           This means, a content model uses a not declared element.
           This is legal even in valid documents. (Of course, if the
           undeclared element actually shows up in the document
           that would make the document invalid.) See rec 3.2

           QUESTION: Should there be a flag to enable a warning,
           when a declaration contains an element type for which
           no declaration is provided, as rec 3.2 metioned?
           This would be the appropriated place to omit the
           warning. */
        tmodel->nameId = entryPtr;
    }
}


/*
 *----------------------------------------------------------------------------
 *
 * TncEndDoctypeDeclHandler --
 *
 *	This procedure is called at the end of the DOCTYPE
 *      declaration, after processing any external subset.
 *      It rewrites the XML_Content models to TNC_Content
 *      models and frees the XML_Content models.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Rewrites the XML_Content models to TNC_Content
 *      models.
 *
 *----------------------------------------------------------------------------
 */

void
TncEndDoctypeDeclHandler (userData)
    void *userData;
{
    TNC_Data *tncdata = (TNC_Data *) userData;
    Tcl_HashEntry *entryPtr, *ePtr1;
    Tcl_HashSearch search;
    XML_Content   *emodel;
    TNC_Content   *tmodel = NULL;
    char *elementName;

    entryPtr = Tcl_FirstHashEntry (tncdata->tagNames, &search);
    while (entryPtr != NULL) {
#ifdef TNC_DEBUG
        printf ("name: %-20s   nameId: %p\n",
                Tcl_GetHashKey (tncdata->tagNames, entryPtr),
                entryPtr);
#endif
        emodel = (XML_Content*) Tcl_GetHashValue (entryPtr);
        tmodel = (TNC_Content*) MALLOC (sizeof (TNC_Content));
        TncRewriteModel (emodel, tmodel, tncdata->tagNames);
        elementName = Tcl_GetHashKey (tncdata->tagNames, entryPtr);
        ePtr1 = Tcl_FindHashEntry (tncdata->attDefsTables, elementName);
        if (ePtr1) {
            tmodel->attInfo = (TNC_ElemAttInfo *) Tcl_GetHashValue (ePtr1);
        } else {
            tmodel->attInfo = NULL;
        }
        Tcl_SetHashValue (entryPtr, tmodel);
        entryPtr = Tcl_NextHashEntry (&search);
    }
    tncdata->elemContentsRewriten = 1;
    /* Checks, if every used notation name is in deed declared */
    entryPtr = Tcl_FirstHashEntry (tncdata->notationDecls, &search);
    while (entryPtr != NULL) {
#ifdef TNC_DEBUG
        printf ("check notation name %s\n",
                Tcl_GetHashKey (tncdata->notationDecls, entryPtr));
        printf ("value %p\n", Tcl_GetHashValue (entryPtr));
#endif
        if (!Tcl_GetHashValue (entryPtr)) {
            signalNotValid (userData, TNC_ERROR_NOTATION_MUST_BE_DECLARED);
            return;
        }
        entryPtr = Tcl_NextHashEntry (&search);
    }
    /* Checks, if every used entity name is indeed declared */
    entryPtr = Tcl_FirstHashEntry (tncdata->entityDecls, &search);
    while (entryPtr != NULL) {
        if (!Tcl_GetHashValue (entryPtr)) {
            signalNotValid (userData,
                            TNC_ERROR_ATT_ENTITY_DEFAULT_MUST_BE_DECLARED);
            return;
        }
        entryPtr = Tcl_NextHashEntry (&search);
    }
    tncdata->status = 1;
}


/*
 *----------------------------------------------------------------------------
 *
 * TncEntityDeclHandler --
 *
 *	This procedure is called for every entity declaration.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Stores either the name of the entity and
 *      type information in a lookup table.
 *
 *----------------------------------------------------------------------------
 */

void
TncEntityDeclHandler (userData, entityName, is_parameter_entity, value,
                       value_length, base, systemId, publicId, notationName)
    void *userData;
    const char *entityName;
    int is_parameter_entity;
    const char *value;
    int value_length;
    const char *base;
    const char *systemId;
    const char *publicId;
    const char *notationName;
{
    TNC_Data *tncdata = (TNC_Data *) userData;
    Tcl_HashEntry *entryPtr, *entryPtr1;
    int newPtr;
    TNC_EntityInfo *entityInfo;


    /* expat collects entity definitions internaly by himself. So this is
       maybe superfluous, if it possible to access the expat internal
       represention. To study this is left to the reader. */

    if (is_parameter_entity) return;
    entryPtr = Tcl_CreateHashEntry (tncdata->entityDecls, entityName, &newPtr);
    /* multiple declaration of the same entity are allowed; first
       definition wins (rec. 4.2) */
    if (!newPtr) {
        /* Eventually, an attribute declaration with type ENTITY or ENTITIES
           has used this (up to the attribute declaration undeclared) ENTITY
           within his default value. In this case, the hash value have to
           be NULL and the entity must be a unparsed entity. */
        if (!Tcl_GetHashValue (entryPtr)) {
            if (notationName == NULL) {
                signalNotValid (userData,
                                TNC_ERROR_ATT_ENTITY_DEFAULT_MUST_BE_DECLARED);
                return;
            }
            newPtr = 1;
        }
    }
    if (newPtr) {
        entityInfo = (TNC_EntityInfo *) MALLOC (sizeof (TNC_EntityInfo));
        if (notationName != NULL) {
            entityInfo->is_notation = 1;
            entryPtr1 = Tcl_CreateHashEntry (tncdata->notationDecls,
                                             notationName, &newPtr);
            entityInfo->notationName = tdomstrdup (notationName);
        }
        else {
            entityInfo->is_notation = 0;
        }
        Tcl_SetHashValue (entryPtr, entityInfo);
    }
}

/*
 *----------------------------------------------------------------------------
 *
 * TncNotationDeclHandler --
 *
 *	This procedure is called for every notation declaration.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Stores the notationName in the notationDecls table with value
 *      one.
 *
 *----------------------------------------------------------------------------
 */

void
TncNotationDeclHandler (userData, notationName, base, systemId, publicId)
    void       *userData;
    const char *notationName;
    const char *base;
    const char *systemId;
    const char *publicId;
{
    TNC_Data *tncdata = (TNC_Data *) userData;
    Tcl_HashEntry *entryPtr;
    int newPtr;

    entryPtr = Tcl_CreateHashEntry (tncdata->notationDecls,
                                    notationName,
                                    &newPtr);
#ifdef TNC_DEBUG
    printf ("Notation %s declared\n", notationName);
#endif
    Tcl_SetHashValue (entryPtr, (char *) 1);
}



/*
 *----------------------------------------------------------------------------
 *
 * TncElementDeclCommand --
 *
 *	This procedure is called for every element declaration.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Stores the tag name of the element in a lookup table.
 *
 *----------------------------------------------------------------------------
 */

void
TncElementDeclCommand (userData, name, model)
    void *userData;
    const char *name;
    XML_Content *model;
{
    TNC_Data *tncdata = (TNC_Data *) userData;
    Tcl_HashEntry *entryPtr;
    int newPtr;
    unsigned int i, j;

    entryPtr = Tcl_CreateHashEntry (tncdata->tagNames, name, &newPtr);
    /* "No element type may be declared more than once." (rec. 3.2) */
    if (!newPtr) {
        signalNotValid (userData, TNC_ERROR_DUPLICATE_ELEMENT_DECL);
        return;
    }
    /* "The same name must not appear more than once in a
        single mixed-content declaration." (rec. 3.2.2)
        NOTE: OK, OK, doing it this way may not be optimal or even fast
        in some cases. Please step in with a more fancy solution, if you
        feel the need. */
    if (model->type == XML_CTYPE_MIXED && model->quant == XML_CQUANT_REP) {
        for (i = 0; i < model->numchildren; i++) {
            for (j = i + 1; j < model->numchildren; j++) {
                if (strcmp ((&model->children[i])->name,
                            (&model->children[j])->name) == 0) {
                    signalNotValid (userData,
                                    TNC_ERROR_DUPLICATE_MIXED_ELEMENT);
                    return;
                }
            }
        }
    }
    Tcl_SetHashValue (entryPtr, model);
    return;
}


/*
 *----------------------------------------------------------------------------
 *
 * TncAttDeclCommand --
 *
 *	This procedure is called for *each* attribute in an XML
 *      ATTLIST declaration. It stores the attribute definition in
 *      an element specific hash table.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Stores the tag name of the element in a lookup table.
 *
 *----------------------------------------------------------------------------
 */

void
TncAttDeclCommand (userData, elname, attname, att_type, dflt, isrequired)
    void       *userData;
    const char *elname;
    const char *attname;
    const char *att_type;
    const char *dflt;
    int         isrequired;
{
    TNC_Data *tncdata = (TNC_Data *) userData;
    Tcl_HashEntry *entryPtr, *entryPtr1;
    Tcl_HashTable *elemAtts;
    TNC_ElemAttInfo *elemAttInfo;
    TNC_AttDecl *attDecl;
    TNC_EntityInfo *entityInfo;
    int newPtr, start, i, clen;
    char *copy;

    entryPtr = Tcl_CreateHashEntry (tncdata->attDefsTables, elname, &newPtr);
    if (newPtr) {
        elemAttInfo = (TNC_ElemAttInfo *) MALLOC (sizeof (TNC_ElemAttInfo));
        elemAtts = (Tcl_HashTable *) MALLOC (sizeof (Tcl_HashTable));
        Tcl_InitHashTable (elemAtts, TCL_STRING_KEYS);
        elemAttInfo->attributes = elemAtts;
        elemAttInfo->nrOfreq = 0;
        elemAttInfo->nrOfIdAtts = 0;
        Tcl_SetHashValue (entryPtr, elemAttInfo);
    } else {
        elemAttInfo = (TNC_ElemAttInfo *) Tcl_GetHashValue (entryPtr);
        elemAtts = elemAttInfo->attributes;
    }
    entryPtr = Tcl_CreateHashEntry (elemAtts, attname, &newPtr);
    /* Multiple Attribute declarations are allowed, but later declarations
       are ignored. See rec 3.3. */
    if (newPtr) {
        attDecl = (TNC_AttDecl *) MALLOC (sizeof (TNC_AttDecl));
        if (strcmp (att_type, "CDATA") == 0) {
            attDecl->att_type = TNC_ATTTYPE_CDATA;
        }
        else if (strcmp (att_type, "ID") == 0) {
            if (elemAttInfo->nrOfIdAtts) {
                signalNotValid (userData, TNC_ERROR_MORE_THAN_ONE_ID_ATT);
                return;
            }
            elemAttInfo->nrOfIdAtts++;
            if (dflt != NULL) {
                signalNotValid (userData, TNC_ERROR_ID_ATT_DEFAULT);
                return;
            }
            attDecl->att_type = TNC_ATTTYPE_ID;
        }
        else if (strcmp (att_type, "IDREF") == 0) {
            attDecl->att_type = TNC_ATTTYPE_IDREF;
        }
        else if (strcmp (att_type, "IDREFS") == 0) {
            attDecl->att_type = TNC_ATTTYPE_IDREFS;
        }
        else if (strcmp (att_type, "ENTITY") == 0) {
            attDecl->att_type = TNC_ATTTYPE_ENTITY;
        }
        else if (strcmp (att_type, "ENTITIES") == 0) {
            attDecl->att_type = TNC_ATTTYPE_ENTITIES;
        }
        else if (strcmp (att_type, "NMTOKEN") == 0) {
            attDecl->att_type = TNC_ATTTYPE_NMTOKEN;
        }
        else if (strcmp (att_type, "NMTOKENS") == 0) {
            attDecl->att_type = TNC_ATTTYPE_NMTOKENS;
        }
        else if (strncmp (att_type, "NOTATION(", 9) == 0) {
            /* This is a bit puzzling. expat returns something like
               <!NOTATION gif PUBLIC "gif">
               <!ATTLIST c type NOTATION (gif) #IMPLIED>
               as att_type "NOTATION(gif)". */
            attDecl->att_type = TNC_ATTTYPE_NOTATION;
            attDecl->lookupTable =
                (Tcl_HashTable *) MALLOC (sizeof (Tcl_HashTable));
            Tcl_InitHashTable (attDecl->lookupTable, TCL_STRING_KEYS);
            copy = tdomstrdup (att_type);
            start = i = 9;
            while (i) {
                if (copy[i] == ')') {
                    copy[i] = '\0';
#ifdef TNC_DEBUG
                    printf ("att type NOTATION: notation %s allowed\n",
                            &copy[start]);
#endif
                    Tcl_CreateHashEntry (attDecl->lookupTable,
                                                    &copy[start], &newPtr);
                    entryPtr1 = Tcl_CreateHashEntry (tncdata->notationDecls,
                                                    &copy[start], &newPtr);
#ifdef TNC_DEBUG
                    if (newPtr) {
                        printf ("up to now unknown NOTATION\n");
                    } else {
                        printf ("NOTATION already known\n");
                    }
#endif
                    FREE (copy);
                    break;
                }
                if (copy[i] == '|') {
                    copy[i] = '\0';
#ifdef TNC_DEBUG
                    printf ("att type NOTATION: notation %s allowed\n",
                            &copy[start]);
#endif
                    Tcl_CreateHashEntry (attDecl->lookupTable,
                                                    &copy[start], &newPtr);
                    entryPtr1 = Tcl_CreateHashEntry (tncdata->notationDecls,
                                                    &copy[start], &newPtr);
#ifdef TNC_DEBUG
                    if (newPtr) {
                        printf ("up to now unknown NOTATION\n");
                    } else {
                        printf ("NOTATION already known\n");
                    }
#endif
                    start = ++i;
                    continue;
                }
                clen = UTF8_CHAR_LEN (copy[i]);
                CHECK_UTF_CHARLEN_COPY (clen);
                if (!UTF8_GET_NAMING_NMTOKEN (&copy[i], clen)) {
                    signalNotValid (userData, TNC_ERROR_NMTOKEN_REQUIRED);
                    FREE (copy);
                    return;
                }
                i += clen;
            }
        }
        else {
            /* expat returns something like
               <!ATTLIST a type (  numbered
                   |bullets ) #IMPLIED>
               as att_type "(numbered|bullets)", e.g. in some
               "non-official" normalized way.
               Makes things easier for us. */
            attDecl->att_type = TNC_ATTTYPE_ENUMERATION;
            attDecl->lookupTable =
                (Tcl_HashTable *) MALLOC (sizeof (Tcl_HashTable));
            Tcl_InitHashTable (attDecl->lookupTable, TCL_STRING_KEYS);
            copy = tdomstrdup (att_type);
            start = i = 1;
            while (1) {
                if (copy[i] == ')') {
                    copy[i] = '\0';
                    Tcl_CreateHashEntry (attDecl->lookupTable,
                                                    &copy[start], &newPtr);
                    FREE (copy);
                    break;
                }
                if (copy[i] == '|') {
                    copy[i] = '\0';
                    Tcl_CreateHashEntry (attDecl->lookupTable,
                                                    &copy[start], &newPtr);
                    start = ++i;
                    continue;
                }
                clen = UTF8_CHAR_LEN (copy[i]);
                CHECK_UTF_CHARLEN_COPY (clen);
                if (!UTF8_GET_NAMING_NMTOKEN (&copy[i], clen)) {
                    signalNotValid (userData, TNC_ERROR_NMTOKEN_REQUIRED);
                    FREE (copy);
                    return;
                }
                i += clen;
            }
        }
        if (dflt != NULL) {
            switch (attDecl->att_type) {
            case TNC_ATTTYPE_ENTITY:
            case TNC_ATTTYPE_IDREF:
                clen = UTF8_CHAR_LEN (*dflt);
                CHECK_UTF_CHARLEN (clen);
                if (!UTF8_GET_NAME_START (dflt, clen)) {
                    signalNotValid (userData, TNC_ERROR_NAME_REQUIRED);
                    return;
                }
                i = clen;
                while (1) {
                    if (dflt[i] == '\0') {
                        break;
                    }
                    clen = UTF8_CHAR_LEN (dflt[i]);
                    CHECK_UTF_CHARLEN (clen);
                    if (!UTF8_GET_NAMING_NMTOKEN (&dflt[i], clen)) {
                        signalNotValid (userData, TNC_ERROR_NAME_REQUIRED);
                        return;
                    }
                    i += clen;
                }
                if (attDecl->att_type == TNC_ATTTYPE_ENTITY) {
                    entryPtr1 = Tcl_CreateHashEntry (tncdata->entityDecls,
                                                     dflt, &newPtr);
                    if (!newPtr) {
                        entityInfo =
                            (TNC_EntityInfo *) Tcl_GetHashValue (entryPtr1);
                        if (!entityInfo->is_notation) {
                            signalNotValid (userData,TNC_ERROR_ATT_ENTITY_DEFAULT_MUST_BE_DECLARED);
                        }
                    }
                }
                break;
            case TNC_ATTTYPE_IDREFS:
                start = i = 0;
                while (1) {
                    if (dflt[i] == '\0') {
                        break;
                    }
                    if (dflt[i] == ' ') {
                        start = ++i;
                    }
                    if (start == i) {
                        clen = UTF8_CHAR_LEN (dflt[i]);
                        CHECK_UTF_CHARLEN (clen);
                        if (!UTF8_GET_NAME_START (&dflt[i], clen)) {
                            signalNotValid (userData, TNC_ERROR_NAME_REQUIRED);
                            return;
                        }
                        i += clen;
                    }
                    else {
                        clen = UTF8_CHAR_LEN (dflt[i]);
                        CHECK_UTF_CHARLEN (clen);
                        if (!UTF8_GET_NAMING_NMTOKEN (&dflt[i], clen)) {
                            signalNotValid (userData, TNC_ERROR_NAME_REQUIRED);
                            return;
                        }
                        i += clen;
                    }
                }
                break;
            case TNC_ATTTYPE_ENTITIES:
                copy = tdomstrdup (dflt);
                start = i = 0;
                while (1) {
                    if (copy[i] == '\0') {
                        FREE (copy);
                        break;
                    }
                    if (copy[i] == ' ') {
                        copy[i] = '\0';
                        entryPtr1 = Tcl_CreateHashEntry (tncdata->entityDecls,
                                                         &copy[start],
                                                         &newPtr);
                        if (!newPtr) {
                            entityInfo =
                                (TNC_EntityInfo *) Tcl_GetHashValue (entryPtr1);
                            if (!entityInfo->is_notation) {
                                signalNotValid (userData,TNC_ERROR_ATT_ENTITY_DEFAULT_MUST_BE_DECLARED);
                            }
                        }
                        start = ++i;
                    }
                    if (start == i) {
                        clen = UTF8_CHAR_LEN (copy[i]);
                        CHECK_UTF_CHARLEN_COPY (clen);
                        if (!UTF8_GET_NAME_START (&copy[i], clen)) {
                            signalNotValid (userData, TNC_ERROR_NAME_REQUIRED);
                            FREE (copy);
                            return;
                        }
                        i += clen;
                    }
                    else {
                        clen = UTF8_CHAR_LEN (copy[i]);
                        CHECK_UTF_CHARLEN_COPY (clen);
                        if (!UTF8_GET_NAMING_NMTOKEN (&copy[i], clen)) {
                            signalNotValid (userData, TNC_ERROR_NAME_REQUIRED);
                            FREE (copy);
                            return;
                        }
                        i += clen;
                    }
                }
                break;
            case TNC_ATTTYPE_NMTOKEN:
                i = 0;
                while (1) {
                    if (dflt[i] == '\0') {
                        break;
                    }
                    clen = UTF8_CHAR_LEN (dflt[i]);
                    CHECK_UTF_CHARLEN (clen);
                    if (!UTF8_GET_NAMING_NMTOKEN (&dflt[i], clen)) {
                        signalNotValid (userData, TNC_ERROR_NMTOKEN_REQUIRED);
                        return;
                    }
                    i += clen;
                }
                if (!i) signalNotValid (userData, TNC_ERROR_NMTOKEN_REQUIRED);
                break;
            case TNC_ATTTYPE_NMTOKENS:
                i = 0;
                while (1) {
                    if (dflt[i] == '\0') {
                        break;
                    }
                    if (dflt[i] == ' ') {
                        i++;
                    }
                    clen = UTF8_CHAR_LEN (dflt[i]);
                    CHECK_UTF_CHARLEN (clen);
                    if (!UTF8_GET_NAMING_NMTOKEN (&dflt[i], clen)) {
                        signalNotValid (userData, TNC_ERROR_NMTOKEN_REQUIRED);
                        return;
                    }
                    i += clen;
                }
                if (!i) signalNotValid (userData, TNC_ERROR_NMTOKEN_REQUIRED);
                break;
            case TNC_ATTTYPE_NOTATION:
                if (!Tcl_FindHashEntry (attDecl->lookupTable, dflt)) {
                    signalNotValid (userData, TNC_ERROR_IMPOSSIBLE_DEFAULT);
                    return;
                }
            case TNC_ATTTYPE_ENUMERATION:
                if (!Tcl_FindHashEntry (attDecl->lookupTable, dflt)) {
                    signalNotValid (userData, TNC_ERROR_IMPOSSIBLE_DEFAULT);
                    return;
                }
            case TNC_ATTTYPE_CDATA:
            case TNC_ATTTYPE_ID:
                /* This both cases are only there, to pacify -Wall.
                   CDATA may have any allowed characters (and
                   everything else is detected by extpat).  ID's not
                   allowed to have defaults (handled above). */
                ;
            }
            attDecl->dflt = tdomstrdup (dflt);
        }
        else {
            attDecl->dflt = NULL;
        }
        if (isrequired) {
            elemAttInfo->nrOfreq++;
        }
        attDecl->isrequired = isrequired;
        Tcl_SetHashValue (entryPtr, attDecl);
    }
}

#ifdef TNC_DEBUG

void
printNameIDs (TNC_Data *tncdata)
{
    Tcl_HashEntry *entryPtr;
    Tcl_HashSearch search;

    for (entryPtr = Tcl_FirstHashEntry (tncdata->tagNames, &search);
         entryPtr != NULL;
         entryPtr = Tcl_NextHashEntry (&search)) {
        printf ("name: %-20s   nameId: %p\n",
                Tcl_GetHashKey (tncdata->tagNames, entryPtr),
                entryPtr);
    }
}

void
printStackElm (TNC_ContentStack *stackelm)
{
    if (stackelm->model->type == XML_CTYPE_NAME) {
        printf ("\tmodel %p\tNAME: %p\n\tactiveChild %d\n\tdeep %d\n\talreadymatched %d\n",
                stackelm->model, stackelm->model->nameId,
                stackelm->activeChild, stackelm->deep, stackelm->alreadymatched);
    }
    else {
        printf ("\tmodel %p\n\tactiveChild %d\n\tdeep %d\n\talreadymatched %d\n",
                stackelm->model, stackelm->activeChild, stackelm->deep,
                stackelm->alreadymatched);
    }
}

void
printTNC_Content (TNC_Content *model)
    {
        printf ("TNC_Content..\n\ttype %d\n\tquant %d\n\tnameId %p\n\tnumchildren %d\n\tchildren %p\n", model->type, model->quant, model->nameId,
            model->numchildren, model->children);
}

void
printContentStack (TNC_Data *tncdata)
{
    TNC_ContentStack stackelm;
    int i;

    printf ("Current contentStack state (used stack slots %d):\n",
            tncdata->contentStackPtr);
    for (i = 0; i < tncdata->contentStackPtr; i++) {
        stackelm = tncdata->contentStack[i];
        printf ("%3d:\n", i);
        printStackElm (&stackelm);
    }
}
#endif /* TNC_DEBUG */


/*
 *----------------------------------------------------------------------------
 *
 * TncProbeElement --
 *
 *	This function checks, if the element match the
 *      topmost content model on the content stack.
 *
 * Results:
 *	1 if the element match,
 *      0 if not.
 *     -1 if not, but this isn't a validation error
 *
 * Side effects:
 *	Eventually pushes data to the contentStack (even in
 *      recurive calls).
 *
 *----------------------------------------------------------------------------
 */

static int
TncProbeElement (nameId, tncdata)
    TNC_NameId *nameId;
    TNC_Data   *tncdata;
{
    TNC_ContentStack *stackelm;
    TNC_Content *activeModel;
    int myStackPtr, zeroMatchPossible, result;
    unsigned int i, seqstartindex;

#ifdef TNC_DEBUG
    printf ("TncProbeElement start\n");
    printContentStack (tncdata);
#endif
    myStackPtr = tncdata->contentStackPtr - 1;
    stackelm = &(tncdata->contentStack)[myStackPtr];
    switch (stackelm->model->type) {
    case XML_CTYPE_MIXED:
#ifdef TNC_DEBUG
        printf ("TncProbeElement XML_CTYPE_MIXED\n");
#endif
        for (i = 0; i < stackelm->model->numchildren; i++) {
            if ((&stackelm->model->children[i])->nameId == nameId) {
                return 1;
            }
        }
        return 0;
    case XML_CTYPE_ANY:
#ifdef TNC_DEBUG
        printf ("TncProbeElement XML_CTYPE_ANY\n");
#endif
        return 1;
    case XML_CTYPE_EMPTY:
#ifdef TNC_DEBUG
        printf ("TncProbeElement XML_CTYPE_EMPTY\n");
#endif
        return 0;
    case XML_CTYPE_CHOICE:
#ifdef TNC_DEBUG
        printf ("TncProbeElement XML_CTYPE_CHOICE\n");
#endif
        if (stackelm->alreadymatched) {
            activeModel = &stackelm->model->children[stackelm->activeChild];
            if (activeModel->type == XML_CTYPE_NAME) {
                /* so this stackelement must be the topmost */
                if (activeModel->quant == XML_CQUANT_REP
                    || activeModel->quant == XML_CQUANT_PLUS) {
                    /* the last matched element is multiple, maybe it
                       matches again */
                    if (nameId == activeModel->nameId) {
#ifdef TNC_DEBUG
                        printf ("-->matched! child Nr. %d\n",
                                stackelm->activeChild);
#endif
                        /* stack and activeChild nr. are already OK, just
                           report success. */
                        return 1;
                    }
                }
            }
            /* The active child is a SEQ or CHOICE. */
            if (stackelm->model->quant == XML_CQUANT_NONE ||
                stackelm->model->quant == XML_CQUANT_OPT) {
                /*The child cp's type SEQ or CHOICE keep track by
                  themselve about if they are repeated. Because we are
                  here, they don't.  Since the current cp has already
                  matched and isn't multiple, the current cp as a whole
                  is done.  But no contradiction detected, so return
                  "search futher" */
                return -1;
            }
        }

        /* If one of the alternatives within the CHOICE cp is quant
           REP or OPT, it isn't a contradition to the document structure,
           if the cp doesn't match, even if it is quant
           NONE or PLUS, because of the "zero time" match of this one
           alternative. We use zeroMatchPossible, to know about this.*/
        zeroMatchPossible = 0;
        for (i = 0; i < stackelm->model->numchildren; i++) {
            if ((&stackelm->model->children[i])->type == XML_CTYPE_NAME) {
#ifdef TNC_DEBUG
                printf ("child is type NAME\n");
#endif
                if ((&stackelm->model->children[i])->nameId == nameId) {
#ifdef TNC_DEBUG
                    printf ("-->matched! child Nr. %d\n",i);
#endif
                    (&tncdata->contentStack[myStackPtr])->activeChild = i;
                    (&tncdata->contentStack[myStackPtr])->alreadymatched = 1;
                    return 1;
                }
                else {
                    /* If the name child is optional, we have a
                       candidat for "zero match". */
                    if ((&stackelm->model->children[i])->quant
                        == XML_CQUANT_OPT ||
                        (&stackelm->model->children[i])->quant
                        == XML_CQUANT_REP) {
#ifdef TNC_DEBUG
                        printf ("zero match possible\n");
#endif
                        zeroMatchPossible = 1;
                    }
                }
            }
            else {
#ifdef TNC_DEBUG
                printf ("complex child type\n");
#endif
                if (tncdata->contentStackPtr == tncdata->contentStackSize) {
                    tncdata->contentStack = (TNC_ContentStack *)
                        Tcl_Realloc ((char *)tncdata->contentStack,
                                     sizeof (TNC_Content *) * 2 *
                                     tncdata->contentStackSize);
                    tncdata->contentStackSize *= 2;
                }
                (&tncdata->contentStack[tncdata->contentStackPtr])->model
                    = &stackelm->model->children[i];
                tncdata->contentStack[tncdata->contentStackPtr].activeChild
                    = 0;
                tncdata->contentStack[tncdata->contentStackPtr].deep
                    = stackelm->deep + 1;
                tncdata->contentStack[tncdata->contentStackPtr].alreadymatched
                    = 0;
                tncdata->contentStackPtr++;
                result = TncProbeElement (nameId, tncdata);
                if (result == 1) {
#ifdef TNC_DEBUG
                    printf ("-->matched! child nr. %d\n",i);
#endif
                    (&tncdata->contentStack[myStackPtr])->activeChild = i;
                    (&tncdata->contentStack[myStackPtr])->alreadymatched = 1;
                    return 1;
                }
                /* The child cp says, it doesn't has matched, but says
                   also, it's perfectly OK, if it doesn't at all. So we
                   have a candidat for "zero match". */
                if (result == -1) {
                    zeroMatchPossible = 1;
                }
                tncdata->contentStackPtr--;
            }
        }
        /* OK, nobody has claimed a match. Question is: try futher or is
           this a document structure error. */
        if (zeroMatchPossible ||
            stackelm->alreadymatched ||
            stackelm->model->quant == XML_CQUANT_REP ||
            stackelm->model->quant == XML_CQUANT_OPT) {
            return -1;
        }
#ifdef TNC_DEBUG
        printf ("validation error\n");
#endif
        return 0;
    case XML_CTYPE_SEQ:
#ifdef TNC_DEBUG
        printf ("TncProbeElement XML_CTYPE_SEQ\n");
#endif
        if (stackelm->alreadymatched) {
            activeModel = &stackelm->model->children[stackelm->activeChild];
            if (activeModel->type == XML_CTYPE_NAME) {
                /* so this stackelement must be the topmost */
                if (activeModel->quant == XML_CQUANT_REP
                    || activeModel->quant == XML_CQUANT_PLUS) {
                    /* the last matched element is multiple, maybe it
                       matches again */
                    if (nameId == activeModel->nameId) {
#ifdef TNC_DEBUG
                        printf ("-->matched! child Nr. %d\n",
                                stackelm->activeChild);
#endif
                        /* stack and activeChild nr. are already OK, just
                           report success. */
                        return 1;
                    }
                }
            }
        }

        if (stackelm->alreadymatched) {
            seqstartindex = stackelm->activeChild + 1;
        }
        else {
            seqstartindex = 0;
        }
        /* This time zeroMatchPossible flags, if every of the remaining
           childs - that may every child, if !alreadymatched - doesn't
           must occur.  We assume, the (outstanding childs of, in case
           of alreadymatched) current stackelement model has only
           optional childs, and set to wrong, if we find any
           non-optional child */
        zeroMatchPossible = 1;
        for (i = seqstartindex; i < stackelm->model->numchildren; i++) {
            if ((&stackelm->model->children[i])->type == XML_CTYPE_NAME) {
                if ((&stackelm->model->children[i])->nameId == nameId) {
#ifdef TNC_DEBUG
                    printf ("-->matched! child Nr. %d\n",i);
#endif
                    (&tncdata->contentStack[myStackPtr])->activeChild = i;
                    (&tncdata->contentStack[myStackPtr])->alreadymatched = 1;
                    return 1;
                } else if ((&stackelm->model->children[i])->quant
                           == XML_CQUANT_NONE
                           || (&stackelm->model->children[i])->quant
                               == XML_CQUANT_PLUS) {
                    zeroMatchPossible = 0;
                    break;
                }
            } else {
                if (tncdata->contentStackPtr == tncdata->contentStackSize) {
                    tncdata->contentStack = (TNC_ContentStack *)
                        Tcl_Realloc ((char *)tncdata->contentStack,
                                     sizeof (TNC_Content *) * 2 *
                                     tncdata->contentStackSize);
                    tncdata->contentStackSize *= 2;
                }
                (&tncdata->contentStack[tncdata->contentStackPtr])->model =
                    &stackelm->model->children[i];
                tncdata->contentStack[tncdata->contentStackPtr].activeChild
                    = 0;
                tncdata->contentStack[tncdata->contentStackPtr].deep
                    = stackelm->deep + 1;
                tncdata->contentStack[tncdata->contentStackPtr].alreadymatched
                    = 0;
                tncdata->contentStackPtr++;
                result = TncProbeElement (nameId, tncdata);
                if (result == 1) {
                    (&tncdata->contentStack[myStackPtr])->activeChild = i;
                    (&tncdata->contentStack[myStackPtr])->alreadymatched = 1;
                    return 1;
                }
                tncdata->contentStackPtr--;
                if (result == 0) {
                    zeroMatchPossible = 0;
                    break;
                }
            }
        }
        if (!stackelm->alreadymatched) {
            if (zeroMatchPossible) {
                /* The stackelm hasn't matched, but don't have to
                   after all.  Return try futher */
                return -1;
            } else {
                /* No previous match, but at least one child is
                   necessary. Return depends of the quant of the
                   entire seq */
                if (stackelm->model->quant == XML_CQUANT_NONE ||
                    stackelm->model->quant == XML_CQUANT_PLUS) {
                    /* DTD claims, the seq as to be there, but isn't */
                    return 0;
                } else {
                    /* The seq is optional */
                    return -1;
                }
            }
        }
        if (stackelm->alreadymatched) {
            if (!zeroMatchPossible) {
                /* Some child at the start of the seq has matched in
                   the past, but since zeroMatchPossible has changed
                   to zero, there must be a non-matching non-optional
                   child later. Error in document structure. */
                return 0;
            } else {
                /* OK, SEQ has matched befor. But after the last match, there
                   where no required (quant NONE or PLUS) childs. */
                if (stackelm->model->quant == XML_CQUANT_NONE ||
                    stackelm->model->quant == XML_CQUANT_OPT) {
                    /* The entire seq isn't multiple. Just look futher. */
                    return -1;
                }
            }
        }
        /* The last untreated case is alreadymatched true,
           zeroMatchPossible (of the rest of the seq childs after the
           last match) true and the entire seq may be
           multiple. Therefore start again with activeChild = 0, to
           see, if the current nameId starts a repeated match of the
           seq.  By the way: zeroMatchPossible still has inital value
           1, therefor no second initaliation is needed */
        for (i = 0; i < seqstartindex; i++) {
            if ((&stackelm->model->children[i])->type == XML_CTYPE_NAME) {
                if ((&stackelm->model->children[i])->nameId == nameId) {
#ifdef TNC_DEBUG
                    printf ("-->matched! child Nr. %d\n",i);
#endif
                    (&tncdata->contentStack[myStackPtr])->activeChild = i;
                    (&tncdata->contentStack[myStackPtr])->alreadymatched = 1;
                    return 1;
                } else if ((&stackelm->model->children[i])->quant
                           == XML_CQUANT_NONE
                           || (&stackelm->model->children[i])->quant
                               == XML_CQUANT_PLUS) {
                    zeroMatchPossible = 0;
                    break;
                }
            } else {
                if (tncdata->contentStackPtr == tncdata->contentStackSize) {
                    tncdata->contentStack = (TNC_ContentStack *)
                        Tcl_Realloc ((char *)tncdata->contentStack,
                                     sizeof (TNC_Content *) * 2 *
                                     tncdata->contentStackSize);
                    tncdata->contentStackSize *= 2;
                }
                (&tncdata->contentStack[tncdata->contentStackPtr])->model =
                    &stackelm->model->children[i];
                tncdata->contentStack[tncdata->contentStackPtr].activeChild
                    = 0;
                tncdata->contentStack[tncdata->contentStackPtr].deep
                    = stackelm->deep + 1;
                tncdata->contentStack[tncdata->contentStackPtr].alreadymatched
                    = 0;
                tncdata->contentStackPtr++;
                result = TncProbeElement (nameId, tncdata);
                if (result) {
                    (&tncdata->contentStack[myStackPtr])->activeChild = i;
                    /* alreadymatched is already 1 */
                    return 1;
                }
                tncdata->contentStackPtr--;
                if (result == 0) {
                    /* OK, the seq doesn't match again. But since it have
                       already matched, this isn't return 0 but.. */
                    return -1;
                }
            }
        }
        /* seq doesn't match again and every seq child from the very first
           up to (not including) the last match aren't required. This last
           fact may be nice to know, but after all since the entire seq have
           matched already ... */
        return -1;
    case XML_CTYPE_NAME:
        /* NAME type dosen't occur at top level of a content model and is
           handled in some "shotcut" way directly in the CHOICE and SEQ cases.
           It's only here to pacify gcc -Wall. */
        printf ("error!!! - in TncProbeElement: XML_CTYPE_NAME shouldn't reached in any case.\n");
    default:
        printf ("error!!! - in TncProbeElement: unknown content type: %d\n",
                stackelm->model->type);
    }
    /* not reached */
    printf ("error!!! - in TncProbeElement: end of function reached.\n");
    return 0;
}

/*
 *----------------------------------------------------------------------------
 *
 * TncProbeAttribute --
 *
 *	This function checks, if the given attribute 
 *      and it's value are allowed for this element.
 *
 * Results:
 *	1 if the attribute name/value is OK,
 *      0 if not.
 *
 * Side effects:
 *	Eventually increments the required attributes counter.
 *
 *----------------------------------------------------------------------------
 */

static int
TncProbeAttribute (userData, elemAtts, attrName, attrValue, nrOfreq)
    void *userData;
    Tcl_HashTable *elemAtts;
    char *attrName;
    char *attrValue;
    int *nrOfreq;
{
    TNC_Data *tncdata = (TNC_Data *) userData;
    Tcl_HashEntry *entryPtr;
    TNC_AttDecl *attDecl;
    char *pc, *copy, save;
    int clen, i, start, hnew;
    TNC_EntityInfo *entityInfo;

    entryPtr = Tcl_FindHashEntry (elemAtts, attrName);
    if (!entryPtr) {
        signalNotValid (userData, TNC_ERROR_UNKOWN_ATTRIBUTE);
        return 0;
    }
    /* NOTE: attribute uniqueness per element is a wellformed
               constrain and therefor done by expat. */
    attDecl = (TNC_AttDecl *) Tcl_GetHashValue (entryPtr);
    switch (attDecl->att_type) {
    case TNC_ATTTYPE_CDATA:
        if (attDecl->isrequired && attDecl->dflt) {
            if (strcmp (attDecl->dflt, attrValue) != 0) {
                signalNotValid (userData,
                                TNC_ERROR_WRONG_FIXED_ATTVALUE);
                return 0;
            }
        }
        break;

    case TNC_ATTTYPE_ID:
        pc = (char*)attrValue;
        clen = UTF8_CHAR_LEN (*pc);
        CHECK_UTF_CHARLENR (clen);
        if (!UTF8_GET_NAME_START (pc, clen)) {
            signalNotValid (userData, TNC_ERROR_NAME_REQUIRED);
        }
        pc += clen;
        while (1) {
            if (*pc == '\0') {
                break;
            }
            clen = UTF8_CHAR_LEN (*pc);
            CHECK_UTF_CHARLENR (clen);
            if (!UTF8_GET_NAMING_NMTOKEN (pc, clen)) {
                signalNotValid (userData, TNC_ERROR_NAME_REQUIRED);
                return 0;
            }
            pc += clen;
        }
        entryPtr = Tcl_CreateHashEntry (tncdata->ids, attrValue, &hnew);
        if (!hnew) {
            if (Tcl_GetHashValue (entryPtr)) {
                signalNotValid (userData,
                                TNC_ERROR_DUPLICATE_ID_VALUE);
                return 0;
            }
        }
        Tcl_SetHashValue (entryPtr, (char *) 1);
        break;

    case TNC_ATTTYPE_IDREF:
        /* Name type constraint "implicit" checked. If the
           referenced ID exists, the type must be OK, because the
           type of the ID's within the document are checked.
           If there isn't such an ID, it's an error anyway. */
        if (attrValue[0] == '\0') {
            signalNotValid (userData, TNC_ERROR_NAME_REQUIRED);
            return 0;
        }
        entryPtr = Tcl_CreateHashEntry (tncdata->ids, attrValue, &hnew);
        break;

    case TNC_ATTTYPE_IDREFS:
        if (attrValue[0] == '\0') {
            signalNotValid (userData, TNC_ERROR_NAMES_REQUIRED);
            return 0;
        }
        /* Due to attribute value normalization (xml rec 3.3.3) this
           is a simple list "ref ref ref ..." without leading or
           trailing spaces and exact one space between the refs. */
        start = i = 0;
        while (attrValue[i]) {
            if (attrValue[i] == ' ') {
                save = attrValue[i];
                attrValue[i] = '\0';
                entryPtr = Tcl_CreateHashEntry (tncdata->ids,
                                                &attrValue[start], &hnew);
                attrValue[i] = save;
                start = ++i;
                continue;
            }
            i++;
        }
        entryPtr = Tcl_CreateHashEntry (tncdata->ids, &attrValue[start], 
                                        &hnew);
        break;

    case TNC_ATTTYPE_ENTITY:
        /* There is a validity constraint requesting entity attributes
           values to be type Name. But if there would be an entity
           declaration that doesn't fit this constraint, expat would
           have already complained about the definition. So we go the
           easy way and just look up the att value. If it's declared,
           type must be OK, if not, it's an error anyway. */
        entryPtr = Tcl_FindHashEntry (tncdata->entityDecls, attrValue);
        if (!entryPtr) {
            signalNotValid (userData, TNC_ERROR_ENTITY_ATTRIBUTE);
            return 0;
        }
        entityInfo = (TNC_EntityInfo *) Tcl_GetHashValue (entryPtr);
        if (!entityInfo->is_notation) {
            signalNotValid (userData, TNC_ERROR_ENTITY_ATTRIBUTE);
            return 0;
        }
        break;

    case TNC_ATTTYPE_ENTITIES:
        /* Normalized by exapt; for type see comment to
           TNC_ATTTYPE_ENTITY */
        copy = tdomstrdup (attrValue);
        start = i = 0;
        while (1) {
            if (copy[i] == '\0') {
                entryPtr = Tcl_FindHashEntry (tncdata->entityDecls,
                                              &copy[start]);
                if (!entryPtr) {
                    signalNotValid (userData, TNC_ERROR_ENTITIES_ATTRIBUTE);
                    FREE (copy);
                    return 0;
                }
                entityInfo = (TNC_EntityInfo *) Tcl_GetHashValue (entryPtr);
                if (!entityInfo->is_notation) {
                    signalNotValid (userData, TNC_ERROR_ENTITIES_ATTRIBUTE);
                    FREE (copy);
                    return 0;
                }
                FREE (copy);
                break;
            }
            if (copy[i] == ' ') {
                copy[i] = '\0';
                entryPtr = Tcl_FindHashEntry (tncdata->entityDecls,
                                              &copy[start]);
                if (!entryPtr) {
                    signalNotValid (userData, TNC_ERROR_ENTITIES_ATTRIBUTE);
                    FREE (copy);
                    return 0;
                }
                entityInfo = (TNC_EntityInfo *) Tcl_GetHashValue (entryPtr);
                if (!entityInfo->is_notation) {
                    signalNotValid (userData, TNC_ERROR_ENTITIES_ATTRIBUTE);
                    FREE (copy);
                    return 0;
                }
                start = ++i;
                continue;
            }
            i++;
        }
        break;

    case TNC_ATTTYPE_NMTOKEN:
        /* We assume, that the UTF-8 representation of the value is
           valid (no partial chars, minimum encoding). This makes
           things a little more easy and faster. I guess (but
           haven't deeply checked - QUESTION -), expat would have
           already complained otherwise. */
        pc = (char*)attrValue;
        clen = 0;
        while (1) {
            if (*pc == '\0') {
                break;
            }
            clen = UTF8_CHAR_LEN (*pc);
            CHECK_UTF_CHARLENR (clen);
            if (!UTF8_GET_NAMING_NMTOKEN (pc, clen)) {
                signalNotValid (userData, TNC_ERROR_NMTOKEN_REQUIRED);
                return 0;
            }
            pc += clen;
        }
        if (!clen) 
            signalNotValid (userData, TNC_ERROR_NMTOKEN_REQUIRED);
        break;

    case TNC_ATTTYPE_NMTOKENS:
        pc = (char*)attrValue;
        clen = 0;
        while (1) {
            if (*pc == '\0') {
                break;
            }
            /* NMTOKENS are normalized by expat, so this should
               be secure. */
            if (*pc == ' ') {
                pc++;
            }
            clen = UTF8_CHAR_LEN (*pc);
            CHECK_UTF_CHARLENR (clen);
            if (!UTF8_GET_NAMING_NMTOKEN (pc, clen)) {
                signalNotValid (userData, TNC_ERROR_NMTOKEN_REQUIRED);
                return 0;
            }
            pc += clen;
        }
        if (!clen)
            signalNotValid (userData, TNC_ERROR_NMTOKEN_REQUIRED);
        break;

    case TNC_ATTTYPE_NOTATION:
        entryPtr = Tcl_FindHashEntry (attDecl->lookupTable, attrValue);
        if (!entryPtr) {
            signalNotValid (userData, TNC_ERROR_NOTATION_REQUIRED);
            return 0;
        }
        break;

    case TNC_ATTTYPE_ENUMERATION:
        if (!Tcl_FindHashEntry (attDecl->lookupTable, attrValue)) {
            signalNotValid (userData, TNC_ERROR_ENUM_ATT_WRONG_VALUE);
            return 0;
        }
        break;
    }

    if (attDecl->isrequired) {
        (*nrOfreq)++;
    }

    return 1;
}

/*
 *----------------------------------------------------------------------------
 *
 * TncElementStartCommand --
 *
 *	This procedure is called for every element start event
 *      while parsing XML Data with a "tnc" enabled tclexpat
 *      parser. Checks, if the element can occur here and if it
 *      has an acceptable set of attributes.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Eventually signals application error.
 *
 *----------------------------------------------------------------------------
 */

void
TncElementStartCommand (userData, name, atts)
    void *userData;
    const char *name;
    const char **atts;
{
    TNC_Data *tncdata = (TNC_Data *) userData;
    Tcl_HashEntry *entryPtr;
    Tcl_HashTable *elemAtts;
    const char **atPtr;
    TNC_ElemAttInfo *elemAttInfo;
    TNC_Content *model;
    int result, nrOfreq, acceptNoDoctype = 0;

#ifdef TNC_DEBUG
    printf ("TncElementStartCommand name: %s\n", name);
#endif

    /* If the document doesn't have a doctype declaration, but the
       user have used the -useForeignDTD 1 feature, the collected
       data out of the provided DTD isn't postprocessed by 
       TncElementStartCommand. We do this now.
       NOTE: Since there wasn't a doctype declaration, there is no
       information avaliable which element is expected to be the
       document element. Eventually it would be desirable, to set
       this somehow. For now, this means, that every valid subtree
       of the given DTD information is accepted.  */
    if (!tncdata->contentStackPtr && !tncdata->elemContentsRewriten) {
        TncEndDoctypeDeclHandler (userData);
        acceptNoDoctype = 1;
    }

    entryPtr = Tcl_FindHashEntry (tncdata->tagNames, name);
    if (!entryPtr) {
        signalNotValid (userData, TNC_ERROR_UNKNOWN_ELEMENT);
        return;
    }
    model = (TNC_Content *) Tcl_GetHashValue (entryPtr);

    switch (model->type) {
    case XML_CTYPE_MIXED:
    case XML_CTYPE_ANY:
        tncdata->ignoreWhiteCDATAs = 1;
        tncdata->ignorePCDATA = 1;
        break;
    case XML_CTYPE_EMPTY:
        tncdata->ignoreWhiteCDATAs = 0;
        break;
    case XML_CTYPE_CHOICE:
    case XML_CTYPE_SEQ:
        tncdata->ignoreWhiteCDATAs = 1;
        tncdata->ignorePCDATA = 0;
        break;
    case XML_CTYPE_NAME:
        break;
    }

    if (tncdata->contentStackPtr) {
        /* This is the normal case, within some content,
           at least the root element content. */
        while (1) {
            result = TncProbeElement (entryPtr, tncdata);
            if (result == -1) {
                if (tncdata->contentStack[tncdata->contentStackPtr - 1].deep
                    == 0) {
                    signalNotValid (userData,
                                    TNC_ERROR_ELEMENT_NOT_ALLOWED_HERE);
                    return;
                }
                tncdata->contentStackPtr--;
                continue;
            }
            if (result) {
                break;
            }
            if (!result) {
                signalNotValid (userData, TNC_ERROR_ELEMENT_NOT_ALLOWED_HERE);
                return;
            }
        }
        if (tncdata->contentStackPtr == tncdata->contentStackSize) {
            tncdata->contentStackSize *= 2;
            tncdata->contentStack = (TNC_ContentStack *)
                Tcl_Realloc ((char *)tncdata->contentStack,
                             sizeof (TNC_Content *)*tncdata->contentStackSize);
        }
        (&tncdata->contentStack[tncdata->contentStackPtr])->model = model;
        (&tncdata->contentStack[tncdata->contentStackPtr])->activeChild = 0;
        (&tncdata->contentStack[tncdata->contentStackPtr])->deep = 0;
        (&tncdata->contentStack[tncdata->contentStackPtr])->alreadymatched = 0;
        tncdata->contentStackPtr++;
    } else {
        /* This is only in case of the root element */
        if (atts) {
            if (!tncdata->doctypeName) {
                if (!acceptNoDoctype) {
                    signalNotValid (userData, TNC_ERROR_NO_DOCTYPE_DECL);
                    return;
                }
            } else {
                if (strcmp (tncdata->doctypeName, name) != 0) {
                    signalNotValid (userData, TNC_ERROR_WRONG_ROOT_ELEMENT);
                    return;
                }
            }
        }
        (&(tncdata->contentStack)[0])->model = model;
        (&(tncdata->contentStack)[0])->activeChild = 0;
        (&(tncdata->contentStack)[0])->deep = 0;
        (&(tncdata->contentStack)[0])->alreadymatched = 0;
        tncdata->contentStackPtr++;
    }
    
    if (atts) {
        elemAttInfo = model->attInfo;
        if (!elemAttInfo) {
            if (atts[0] != NULL) {
                signalNotValid (userData, TNC_ERROR_NO_ATTRIBUTES);
                return;
            }
        } else {
            elemAtts = elemAttInfo->attributes;
            nrOfreq = 0;
            for (atPtr = atts; atPtr[0]; atPtr += 2) {
                if (!TncProbeAttribute (userData, elemAtts, (char *) atPtr[0],
                                        (char *) atPtr[1], &nrOfreq))    
                    return;
            }
            if (nrOfreq != elemAttInfo->nrOfreq) {
                signalNotValid (userData, 
                                TNC_ERROR_MISSING_REQUIRED_ATTRIBUTE);
            return;
            }
        }
    } else {
        tncdata->elemAttInfo = model->attInfo;
    }

#ifdef TNC_DEBUG
    printf ("TncElementStartCommand end\n");
#endif
}


/*
 *----------------------------------------------------------------------------
 *
 * TncProbeElementEnd --
 *
 *	This procedure checks, if the current content allows the
 *      the element to end here.
 *
 * Results:
 *	1 if element end is OK,
 *      0 if not.
 *
 * Side effects:
 *	Let the contentStackPtr point to the last current content
 *      model before the element had started.
 *
 *----------------------------------------------------------------------------
 */

static int
TncProbeElementEnd (tncdata)
    TNC_Data *tncdata;
{
    TNC_ContentStack stackelm;
    unsigned int i;
    int zeroMatchPossible, seqstartindex;

    stackelm = tncdata->contentStack[tncdata->contentStackPtr - 1];
    switch (stackelm.model->type) {
    case XML_CTYPE_MIXED:
    case XML_CTYPE_ANY:
    case XML_CTYPE_EMPTY:
        return 1;
    case XML_CTYPE_CHOICE:
        if (stackelm.alreadymatched) {
            return 1;
        }

        if (stackelm.model->quant == XML_CQUANT_REP ||
            stackelm.model->quant == XML_CQUANT_OPT) {
            return 1;
        }
        zeroMatchPossible = 0;
        for (i = 0; i < stackelm.model->numchildren; i++) {
            if ((&stackelm.model->children[i])->type == XML_CTYPE_NAME) {
                if ((&stackelm.model->children[i])->quant == XML_CQUANT_OPT ||
                    (&stackelm.model->children[i])->quant == XML_CQUANT_REP) {
                    zeroMatchPossible = 1;
                    break;
                }
            }
            else {
                if (tncdata->contentStackPtr == tncdata->contentStackSize) {
                    tncdata->contentStack = (TNC_ContentStack *)
                        Tcl_Realloc ((char *)tncdata->contentStack,
                                     sizeof (TNC_Content *) * 2 *
                                     tncdata->contentStackSize);
                    tncdata->contentStackSize *= 2;
                }
                (&tncdata->contentStack[tncdata->contentStackPtr])->model
                    = &stackelm.model->children[i];
                tncdata->contentStack[tncdata->contentStackPtr].activeChild
                    = 0;
                tncdata->contentStack[tncdata->contentStackPtr].deep
                    = stackelm.deep + 1;
                tncdata->contentStack[tncdata->contentStackPtr].alreadymatched
                    = 0;
                tncdata->contentStackPtr++;
                if (TncProbeElementEnd (tncdata)) {
                    zeroMatchPossible = 1;
                    tncdata->contentStackPtr--;
                    break;
                }
                tncdata->contentStackPtr--;
            }
        }
        if (zeroMatchPossible) {
            return 1;
        } else {
            return 0;
        }
    case XML_CTYPE_SEQ:
        if (!stackelm.alreadymatched) {
            if (stackelm.model->quant == XML_CQUANT_REP ||
                stackelm.model->quant == XML_CQUANT_OPT) {
                return 1;
            }
        }
        if (!stackelm.alreadymatched) {
            seqstartindex = 0;
        }
        else {
            seqstartindex = stackelm.activeChild + 1;
        }
        for (i = seqstartindex; i < stackelm.model->numchildren; i++) {
            if ((&stackelm.model->children[i])->type == XML_CTYPE_NAME) {
                if ((&stackelm.model->children[i])->quant == XML_CQUANT_OPT ||
                    (&stackelm.model->children[i])->quant == XML_CQUANT_REP) {
                    continue;
                } else {
                    return 0;
                }
            } else {
                if (tncdata->contentStackPtr == tncdata->contentStackSize) {
                    tncdata->contentStack = (TNC_ContentStack *)
                        Tcl_Realloc ((char *)tncdata->contentStack,
                                     sizeof (TNC_Content *) * 2 *
                                     tncdata->contentStackSize);
                    tncdata->contentStackSize *= 2;
                }
                (&tncdata->contentStack[tncdata->contentStackPtr])->model
                    = &stackelm.model->children[i];
                tncdata->contentStack[tncdata->contentStackPtr].activeChild
                    = 0;
                tncdata->contentStack[tncdata->contentStackPtr].deep
                    = stackelm.deep + 1;
                tncdata->contentStack[tncdata->contentStackPtr].alreadymatched
                    = 0;
                tncdata->contentStackPtr++;
                if (TncProbeElementEnd (tncdata)) {
                    tncdata->contentStackPtr--;
                    continue;
                }
                else {
                    tncdata->contentStackPtr--;
                    return 0;
                }
            }
        }
        return 1;
    case XML_CTYPE_NAME:
        /* NAME type dosen't occur at top level of a content model and is
           handled in some "shotcut" way directly in the CHOICE and SEQ cases.
           It's only here to pacify gcc -Wall. */
        fprintf (stderr, "error!!! - in TncProbeElementEnd: XML_CTYPE_NAME "
                 "shouldn't be reached in any case.\n");
    default:
        fprintf (stderr, "error!!! - in TncProbeElementEnd: unknown content "
                 "type: %d\n", stackelm.model->type);
        return 1;
    }
}


/*
 *----------------------------------------------------------------------------
 *
 * TncElementEndCommand --
 *
 *	This procedure is called for every element end event
 *      while parsing XML Data with a "tnc" enabled tclexpat
 *      parser. Checks, if the content model allows the element
 *      to end at this point.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Eventually signals application error.
 *
 *----------------------------------------------------------------------------
 */

void
TncElementEndCommand (userData, name)
    void       *userData;
    const char *name;
{
    TNC_Data *tncdata = (TNC_Data *) userData;
    Tcl_HashEntry *entryPtr;
    Tcl_HashSearch search;

#ifdef TNC_DEBUG
    printf ("TncElementEndCommand start\n");
    printContentStack (tncdata);
#endif
    while (1) {
        if (!TncProbeElementEnd (tncdata, 0)) {
            signalNotValid (userData, TNC_ERROR_ELEMENT_CAN_NOT_END_HERE);
            return;
        }
        if (tncdata->contentStack[tncdata->contentStackPtr - 1].deep == 0) {
            break;
        }
        tncdata->contentStackPtr--;
    }
    /* Remove the content model of the closed element from the stack */
    tncdata->contentStackPtr--;
#ifdef TNC_DEBUG
    printf ("after removing ended element from the stack\n");
    printContentStack (tncdata);
#endif
    if (tncdata->contentStackPtr) {
        switch ((&tncdata->contentStack[tncdata->contentStackPtr - 1])->model->type) {
        case XML_CTYPE_MIXED:
        case XML_CTYPE_ANY:
            tncdata->ignoreWhiteCDATAs = 1;
            tncdata->ignorePCDATA = 1;
            break;
        case XML_CTYPE_EMPTY:
            tncdata->ignoreWhiteCDATAs = 0;
            break;
        case XML_CTYPE_CHOICE:
        case XML_CTYPE_SEQ:
        case XML_CTYPE_NAME:
            tncdata->ignoreWhiteCDATAs = 1;
            tncdata->ignorePCDATA = 0;
            break;
        }
    } else {
        /* This means, the root element is closed,
           therefor the place to check, if every IDREF points
           to a ID. */
        if (tncdata->idCheck) {
            for (entryPtr = Tcl_FirstHashEntry (tncdata->ids, &search);
                 entryPtr != NULL;
                 entryPtr = Tcl_NextHashEntry (&search)) {
#ifdef TNC_DEBUG
                printf ("check id value %s\n",
                        Tcl_GetHashKey (tncdata->ids, entryPtr));
                printf ("value %p\n", Tcl_GetHashValue (entryPtr));
#endif
                if (!Tcl_GetHashValue (entryPtr)) {
                    signalNotValid (userData, TNC_ERROR_UNKOWN_ID_REFERRED);
                return;
                }
            }
        }
    }
}

/*
 *----------------------------------------------------------------------------
 *
 * TncCharacterdataCommand --
 *
 *	This procedure is called with a piece of CDATA found in
 *      document.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Eventually signals application error.
 *
 *----------------------------------------------------------------------------
 */

void
TncCharacterdataCommand (userData, data, len)
    void       *userData;
    const char *data;
    int         len;
{
    TNC_Data *tncdata = (TNC_Data *) userData;
    int i;
    char *pc;

    if (!tncdata->ignoreWhiteCDATAs && len > 0) {
        signalNotValid (userData, TNC_ERROR_EMPTY_ELEMENT);
        return;
    }
    if (!tncdata->ignorePCDATA) {
        for (i = 0, pc = (char*)data; i < len; i++, pc++) {
            if ( (*pc == ' ')  ||
                 (*pc == '\n') ||
                 (*pc == '\r') ||
                 (*pc == '\t') ) {
                continue;
            }
            signalNotValid (userData, TNC_ERROR_DISALLOWED_PCDATA);
            return;
        }
    }
}

/*
 *----------------------------------------------------------------------------
 *
 * TncStartCdataSectionHandler --
 *
 *	This procedure is called at the start of a CDATA section
 *      within the document.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Eventually signals application error.
 *
 *----------------------------------------------------------------------------
 */

void
TncStartCdataSectionHandler (userData)
    void *userData;
{
    TNC_Data *tncdata = (TNC_Data *) userData;

    if (!tncdata->ignorePCDATA) {
        signalNotValid (userData, TNC_ERROR_DISALLOWED_CDATA);
    }
}

/*
 *----------------------------------------------------------------------------
 *
 * validateNodeAttributes --
 *
 *	Validates the attributes of the given domNode. The domNode must be
 *      an ELEMENT_NODE.
 *
 * Results:
 *	1 if OK, 0 for validation error.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------------
 */

static int 
validateNodeAttributes (
    TNC_Data        *tncdata,
    TNC_ElemAttInfo *elemAttInfo,
    domNode         *node
    )
{
    int          nrOfreq;
    domAttrNode *attr;
    
    if (!elemAttInfo) {
        if (node->firstAttr) {
            signalNotValid (tncdata, TNC_ERROR_NO_ATTRIBUTES);
            return 0;
        }
    } else {
        attr = node->firstAttr;
        nrOfreq = 0;
        while (attr) {
            if (!TncProbeAttribute (tncdata, 
                                    elemAttInfo->attributes,
                                    attr->nodeName,
                                    attr->nodeValue,
                                    &nrOfreq))
                return 0;
            attr = attr->nextSibling;
        }
        if (nrOfreq != elemAttInfo->nrOfreq) {
            signalNotValid (tncdata, 
                            TNC_ERROR_MISSING_REQUIRED_ATTRIBUTE);
            return 0;
        }
    }
    return 1;
}

/*
 *----------------------------------------------------------------------------
 *
 * validateTree --
 *
 *	Validates a complete DOM (sub-)tree against a the DTD informations in
 *      the given tncdata structure. The node argument acts as root of the
 *      (sub-)tree.
 *
 * Results:
 *	1 if OK, 0 for validation error.
 *
 * Side effects:
 *	May alter the context state part of the tnc clientData (and even
 *      mallocs additional memory for them).
 *
 *----------------------------------------------------------------------------
 */
static int validateTree (
    TNC_Data *tncdata,
    domNode  *node
    )
{
    domNode       *child;

    switch (node->nodeType) {
    case ELEMENT_NODE:
        TncElementStartCommand (tncdata, node->nodeName, NULL);
        if (tncdata->status) return 0;
        if (!validateNodeAttributes (tncdata, tncdata->elemAttInfo, node)) 
            return 0;
        if (node->firstChild) {
            child = node->firstChild;
            while (child) {
                if (!validateTree (tncdata, child)) return 0;
                child = child->nextSibling;
            }
        }
        TncElementEndCommand (tncdata, node->nodeName);
        if (tncdata->status) return 0;
        break;
    case TEXT_NODE:
    case CDATA_SECTION_NODE:
        TncCharacterdataCommand (tncdata, ((domTextNode*)node)->nodeValue, 
                                 ((domTextNode*)node)->valueLength);
        if (tncdata->status) return 0;
        break;
    case COMMENT_NODE:
    case PROCESSING_INSTRUCTION_NODE:
        break;
    default:
        signalNotValid (tncdata, TNC_ERROR_UNKNOWN_NODE_TYPE);
        return 0;
    }
    return 1;
}

/*
 *----------------------------------------------------------------------------
 *
 * tnc_ValidateObjCmd
 *
 *	Implements the validateObjCmds. See the user documentation
 *      for details.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	May alter some parts of the tnc_ValidateObjCmd clientData
 *      structure.
 *
 *----------------------------------------------------------------------------
 */

static int
tnc_ValidateObjCmd (
    ClientData  clientData,
    Tcl_Interp *interp,
    int         objc,
    Tcl_Obj    *CONST objv[]
    )
{
    TNC_Data        *tncdata = (TNC_Data*) clientData;
    int              methodIndex, result = 1;
    domNode         *node;
    char            *errMsg = NULL;
    Tcl_HashEntry   *entryPtr;
    TNC_Content     *model;
    
    static CONST84 char *validateMethods[] = {
        "validateTree",   "validateDocument", "validateAttributes",
        "delete",
        NULL
    };
    enum validateMethod {
        m_validateTree, m_validateDocument, m_validateAttributes,
        m_delete
    };

    if (objc < 2 || objc > 4) {
        SetResult (validateCmd_usage);
        return TCL_ERROR;
    }
    
    if (Tcl_GetIndexFromObj (interp, objv[1], validateMethods, "method", 0,
                             &methodIndex) != TCL_OK) {
        return TCL_ERROR;
    }
    

    switch ((enum validateMethod) methodIndex) {

    case m_validateTree:
        if (objc < 3 || objc > 4) {
            SetResult (validateCmd_usage);
            return TCL_ERROR;
        }
        node = tcldom_getNodeFromName (
            interp, Tcl_GetStringFromObj(objv[2], NULL), &errMsg
            );
        if (!node || (node->nodeType != ELEMENT_NODE)) {
            SetResult ("The validateTree method needs a domNode as argument.");
            return TCL_ERROR;
        }
        tncdata->status = 0;
        tncdata->idCheck = 0;
        if (tncdata->ids->numEntries) {
            Tcl_DeleteHashTable (tncdata->ids);
            Tcl_InitHashTable (tncdata->ids, TCL_STRING_KEYS);
        }
        tncdata->contentStackPtr = 0;
        Tcl_ResetResult (interp);
        result = validateTree (tncdata, node);
        if (objc == 4) {
            if (Tcl_ObjSetVar2(interp, objv[3], NULL,
                               Tcl_GetObjResult(interp), 0) == NULL) {
                Tcl_ResetResult(interp);
                Tcl_AppendToObj(Tcl_GetObjResult(interp),  
                                "couldn't save msg in variable", -1);
                return TCL_ERROR;
            }
        }
        if (result) {
            SetBooleanResult (1);
        } else {
            SetBooleanResult (0);
        }
        break;
        
    case m_validateDocument:
        if (objc < 3 || objc > 4) {
            SetResult (validateCmd_usage);
            return TCL_ERROR;
        }
        node = (domNode *) tcldom_getDocumentFromName (
            interp, Tcl_GetStringFromObj (objv[2], NULL), &errMsg
            );
        if (!node) {
            SetResult ("The validateDocument method needs a domDocument as argument.");
            return TCL_ERROR;
        }
        node = ((domDocument *) node)->documentElement;
        if (!tncdata->doctypeName) {
            signalNotValid (tncdata, TNC_ERROR_NO_DOCTYPE_DECL);
            if (objc == 4) {
                if (Tcl_ObjSetVar2(interp, objv[3], NULL,
                                   Tcl_GetObjResult(interp), 0) == NULL) {
                    Tcl_ResetResult(interp);
                    Tcl_AppendToObj(Tcl_GetObjResult(interp),  
                                    "couldn't save msg in variable", -1);
                    return TCL_ERROR;
                }
            }
            SetBooleanResult (0);
            return TCL_OK;
        }
        if (strcmp (tncdata->doctypeName, node->nodeName) != 0) {
            signalNotValid (tncdata, TNC_ERROR_WRONG_ROOT_ELEMENT);
            if (objc == 4) {
                if (Tcl_ObjSetVar2(interp, objv[3], NULL,
                                   Tcl_GetObjResult(interp), 0) == NULL) {
                    Tcl_ResetResult(interp);
                    Tcl_AppendToObj(Tcl_GetObjResult(interp),  
                                    "couldn't save msg in variable", -1);
                    return TCL_ERROR;
                }
            }
            SetBooleanResult (0);
            return TCL_OK;
        }
        tncdata->status = 0;
        tncdata->idCheck = 1;
        if (tncdata->ids->numEntries) {
            Tcl_DeleteHashTable (tncdata->ids);
            Tcl_InitHashTable (tncdata->ids, TCL_STRING_KEYS);
        }
        tncdata->contentStackPtr = 0;
        Tcl_ResetResult (interp);
        result = validateTree (tncdata, node);
        if (objc == 4) {
            if (Tcl_ObjSetVar2(interp, objv[3], NULL,
                               Tcl_GetObjResult(interp), 0) == NULL) {
                Tcl_ResetResult(interp);
                Tcl_AppendToObj(Tcl_GetObjResult(interp),  
                                "couldn't save msg in variable", -1);
                return TCL_ERROR;
            }
        }
        if (result) {
            SetBooleanResult (1);
        } else {
            SetBooleanResult (0);
        }
        break;
        
    case m_validateAttributes:
        if (objc < 3 || objc > 4) {
            SetResult (validateCmd_usage);
            return TCL_ERROR;
        }
        node = tcldom_getNodeFromName (
            interp, Tcl_GetStringFromObj(objv[2], NULL), &errMsg
            );
        if (!node || (node->nodeType != ELEMENT_NODE)) {
            SetResult ("The validateAttributes method needs a domNode as argument.");
            return TCL_ERROR;
        }
        entryPtr = Tcl_FindHashEntry (tncdata->tagNames, node->nodeName);
        if (!entryPtr) {
            signalNotValid (tncdata, TNC_ERROR_UNKNOWN_ELEMENT);
            if (objc == 4) {
                if (Tcl_ObjSetVar2(interp, objv[3], NULL,
                                   Tcl_GetObjResult(interp), 0) == NULL) {
                    Tcl_ResetResult(interp);
                    Tcl_AppendToObj(Tcl_GetObjResult(interp),  
                                    "couldn't save msg in variable", -1);
                    return TCL_ERROR;
                }
            }
            SetBooleanResult (0);
            return TCL_OK;
        }
        model = (TNC_Content *) Tcl_GetHashValue (entryPtr);
        tncdata->status = 0;
        tncdata->idCheck = 0;
        if (tncdata->ids->numEntries) {
            Tcl_DeleteHashTable (tncdata->ids);
            Tcl_InitHashTable (tncdata->ids, TCL_STRING_KEYS);
        }
        Tcl_ResetResult (interp);
        result = validateNodeAttributes (tncdata, model->attInfo, node);
        if (objc == 4) {
            if (Tcl_ObjSetVar2(interp, objv[3], NULL,
                               Tcl_GetObjResult(interp), 0) == NULL) {
                Tcl_ResetResult(interp);
                Tcl_AppendToObj(Tcl_GetObjResult(interp),  
                                "couldn't save msg in variable", -1);
                return TCL_ERROR;
            }
        }
        if (result) {
            SetBooleanResult (1);
        } else {
            SetBooleanResult (0);
        }
        break;
        
    case m_delete:
        if (objc != 2) {
            SetResult (validateCmd_usage);
            return TCL_ERROR;
        }
        Tcl_DeleteCommand (interp, Tcl_GetStringFromObj (objv[0], NULL));
        SetResult ("");
        break;
    }

    return TCL_OK;
}

/*
 *----------------------------------------------------------------------------
 *
 * FreeTncData
 *
 *	Helper proc, used from TncResetProc and TncFreeProc. Frees all
 *	collected DTD data and the id table.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Frees memory.
 *
 *---------------------------------------------------------------------------- */
static void
FreeTncData (tncdata)
    TNC_Data *tncdata;
{
    Tcl_HashEntry *entryPtr, *attentryPtr;
    Tcl_HashSearch search, attsearch;
    TNC_Content *model;
    TNC_ElemAttInfo *elemAttInfo;
    TNC_EntityInfo *entityInfo;
    TNC_AttDecl *attDecl;

    if (tncdata->elemContentsRewriten) {
        entryPtr = Tcl_FirstHashEntry (tncdata->tagNames, &search);
        while (entryPtr) {
            model = Tcl_GetHashValue (entryPtr);
            if (model) {
                TncFreeTncModel (model);
                FREE ((char *) model);
            }
            entryPtr = Tcl_NextHashEntry (&search);
        }
    }
    Tcl_DeleteHashTable (tncdata->tagNames);
    entryPtr = Tcl_FirstHashEntry (tncdata->attDefsTables, &search);
    while (entryPtr) {
        elemAttInfo = Tcl_GetHashValue (entryPtr);
        if (!elemAttInfo) {
            entryPtr = Tcl_NextHashEntry (&search);
            continue;
        }
        attentryPtr = Tcl_FirstHashEntry (elemAttInfo->attributes, &attsearch);
        while (attentryPtr) {
            attDecl = Tcl_GetHashValue (attentryPtr);
            if (attDecl) {
                if (attDecl->att_type == TNC_ATTTYPE_NOTATION ||
                    attDecl->att_type == TNC_ATTTYPE_ENUMERATION) {
                    Tcl_DeleteHashTable (attDecl->lookupTable);
                    FREE ((char *) attDecl->lookupTable);
                }
                if (attDecl->dflt) {
                    FREE (attDecl->dflt);
                }
                FREE ((char *) attDecl);
            }
            attentryPtr = Tcl_NextHashEntry (&attsearch);
        }
        Tcl_DeleteHashTable (elemAttInfo->attributes);
        FREE ((char *) elemAttInfo->attributes);
        FREE ((char *) elemAttInfo);
        entryPtr = Tcl_NextHashEntry (&search);
    }
    Tcl_DeleteHashTable (tncdata->attDefsTables);
    entryPtr = Tcl_FirstHashEntry (tncdata->entityDecls, &search);
    while (entryPtr) {
        entityInfo = Tcl_GetHashValue (entryPtr);
        if (entityInfo) {
            if (entityInfo->is_notation) {
                FREE (entityInfo->notationName);
            }
            FREE ((char *) entityInfo);
        }
        entryPtr = Tcl_NextHashEntry (&search);
    }
    Tcl_DeleteHashTable (tncdata->entityDecls);
    Tcl_DeleteHashTable (tncdata->notationDecls);
    Tcl_DeleteHashTable (tncdata->ids);
    if (tncdata->doctypeName) {
        FREE (tncdata->doctypeName);
    }
}

/*
 *----------------------------------------------------------------------------
 *
 * TncResetProc
 *
 *	Called for C handler set specific reset actions in case of
 *      parser reset.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Resets the "userData" of the C handler set parser extension.
 *
 *----------------------------------------------------------------------------
 */

void
TncResetProc (interp, userData)
    Tcl_Interp *interp;
    void *userData;
{
    TNC_Data *tncdata = (TNC_Data *) userData;

    FreeTncData (tncdata);
    Tcl_InitHashTable (tncdata->tagNames, TCL_STRING_KEYS);
    tncdata->elemContentsRewriten = 0;
    tncdata->status = 0;
    tncdata->idCheck = 1;
    Tcl_InitHashTable (tncdata->attDefsTables, TCL_STRING_KEYS);
    Tcl_InitHashTable (tncdata->entityDecls, TCL_STRING_KEYS);
    Tcl_InitHashTable (tncdata->notationDecls, TCL_STRING_KEYS);
    Tcl_InitHashTable (tncdata->ids, TCL_STRING_KEYS);
    tncdata->doctypeName = NULL;
    tncdata->ignoreWhiteCDATAs = 1;
    tncdata->ignorePCDATA = 0;
    tncdata->contentStackPtr = 0;
}

/*
 *----------------------------------------------------------------------------
 *
 * createTncData --
 *
 *	Helper proc. Allocates a TNC_Data structure and initializes it.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Memory allocation and initialization.
 *
 *----------------------------------------------------------------------------
 */

static TNC_Data *
createTncData (
    Tcl_Interp *interp,
    Tcl_Obj    *expatObj
    )
{
    TNC_Data *tncdata;
    
    tncdata = (TNC_Data *) MALLOC (sizeof (TNC_Data));
    tncdata->tagNames = (Tcl_HashTable *) MALLOC (sizeof (Tcl_HashTable));
    Tcl_InitHashTable (tncdata->tagNames, TCL_STRING_KEYS);
    tncdata->elemContentsRewriten = 0;
    tncdata->status = 0;
    tncdata->idCheck = 1;
    tncdata->attDefsTables = 
        (Tcl_HashTable *) MALLOC (sizeof (Tcl_HashTable));
    Tcl_InitHashTable (tncdata->attDefsTables, TCL_STRING_KEYS);
    tncdata->entityDecls = 
        (Tcl_HashTable *) MALLOC (sizeof (Tcl_HashTable));
    Tcl_InitHashTable (tncdata->entityDecls, TCL_STRING_KEYS);
    tncdata->notationDecls =
        (Tcl_HashTable *) MALLOC (sizeof (Tcl_HashTable));
    Tcl_InitHashTable (tncdata->notationDecls, TCL_STRING_KEYS);
    tncdata->ids = (Tcl_HashTable *) MALLOC (sizeof (Tcl_HashTable));
    Tcl_InitHashTable (tncdata->ids, TCL_STRING_KEYS);
    tncdata->doctypeName = NULL;
    tncdata->interp = interp;
    tncdata->expatObj = expatObj;
    tncdata->ignoreWhiteCDATAs = 1;
    tncdata->ignorePCDATA = 0;
    tncdata->contentStack = (TNC_ContentStack *)
        MALLOC (sizeof (TNC_ContentStack) * TNC_INITCONTENTSTACKSIZE);
    tncdata->contentStackSize = TNC_INITCONTENTSTACKSIZE;
    tncdata->contentStackPtr = 0;
    
    return tncdata;
}

/*
 *----------------------------------------------------------------------------
 *
 * TncFreeProc
 *
 *	Called for C handler set specific cleanup in case of parser
 *      delete.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	C handler set specific userData gets free'd.
 *
 *----------------------------------------------------------------------------
 */

void
TncFreeProc (interp, userData)
    Tcl_Interp *interp;
    void *userData;
{
    TNC_Data *tncdata = (TNC_Data *) userData;

    FreeTncData (tncdata);
    FREE ((char *) tncdata->tagNames);
    FREE ((char *) tncdata->attDefsTables);
    FREE ((char *) tncdata->entityDecls);
    FREE ((char *) tncdata->notationDecls);
    FREE ((char *) tncdata->ids);
    FREE ((char *) tncdata->contentStack);
    FREE ((char *) tncdata);
}

/*
 *----------------------------------------------------------------------------
 *
 * tnc_ValidateObjDeleteCmd
 *
 *	Called when a validateObjCmd is deleted. It's infact nothing
 *      but a wrapper for TncFreeProc.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The clientData structure will be freed, during cleanup routine calls.
 *
 *----------------------------------------------------------------------------
 */

static void
tnc_ValidateObjDeleteCmd (
    ClientData clientData
    )
{
    TNC_Data *tncdata = (TNC_Data*) clientData;
    
    TncFreeProc (tncdata->interp, tncdata);
    
}

/*
 *----------------------------------------------------------------------------
 *
 * TclTncObjCmd --
 *
 *	This procedure is invoked to process the "tnc" command.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	The expat parser object provided as argument is enhanced by
 *      by the "tnc" handler set.
 *
 *----------------------------------------------------------------------------
 */

int
TclTncObjCmd(dummy, interp, objc, objv)
     ClientData dummy;
     Tcl_Interp *interp;
     int objc;
     Tcl_Obj *CONST objv[];
{
    char          *method, *cmdName, s[20];
    CHandlerSet   *handlerSet;
    int            methodIndex, result;
    TNC_Data      *tncdata;

    static CONST84 char *tncMethods[] = {
        "enable",  "remove", "getValidateCmd",
        NULL
    };
    enum tncMethod {
        m_enable, m_remove, m_getValidateCmd
    };

    if (!CheckExpatParserObj (interp, objv[1])) {
        SetResult ("First argument has to be a expat parser object");
        return TCL_ERROR;
    }

    method = Tcl_GetStringFromObj (objv[2], NULL);
    if (Tcl_GetIndexFromObj (interp, objv[2], tncMethods, "method", 0,
                             &methodIndex) != TCL_OK)
    {
        return TCL_ERROR;
    }

    switch ((enum tncMethod) methodIndex) {

    case m_enable:
        if (objc != 3) {
            Tcl_WrongNumArgs (interp, 1, objv, tnc_usage);
            return TCL_ERROR;
        }   
        handlerSet = CHandlerSetCreate ("tnc");
        handlerSet->userData = createTncData (interp, objv[1]);
        handlerSet->ignoreWhiteCDATAs = 0;
        handlerSet->resetProc = TncResetProc;
        handlerSet->freeProc = TncFreeProc;
        handlerSet->elementDeclCommand = TncElementDeclCommand;
        handlerSet->attlistDeclCommand = TncAttDeclCommand;
        handlerSet->entityDeclCommand = TncEntityDeclHandler;
        handlerSet->notationcommand = TncNotationDeclHandler;
        handlerSet->elementstartcommand = TncElementStartCommand;
        handlerSet->elementendcommand = TncElementEndCommand;
        handlerSet->datacommand = TncCharacterdataCommand;
        handlerSet->startCdataSectionCommand = TncStartCdataSectionHandler;
        handlerSet->startDoctypeDeclCommand = TncStartDoctypeDeclHandler;
        handlerSet->endDoctypeDeclCommand = TncEndDoctypeDeclHandler;

        result = CHandlerSetInstall (interp, objv[1], handlerSet);
        if (result != 0) {
            SetResult ("already have tnc C handler set");
            TncFreeProc (interp, handlerSet->userData);
            FREE (handlerSet->name);
            FREE ((char *) handlerSet);
            return TCL_ERROR;
        }
        return TCL_OK;

    case m_remove:
        if (objc != 3) {
            Tcl_WrongNumArgs (interp, 1, objv, tnc_usage);
            return TCL_ERROR;
        }   
        result = CHandlerSetRemove (interp, objv[1], "tnc");
        if (result == 1) {
            /* This should not happen if CheckExpatParserObj() is used. */
            SetResult ("argument has to be a expat parser object");
            return TCL_ERROR;
        }
        if (result == 2) {
            SetResult("expat parser obj hasn't a C handler set named \"tnc\"");
            return TCL_ERROR;
        }
        return TCL_OK;

    case m_getValidateCmd:
        if (objc != 3 && objc != 4) {
            Tcl_WrongNumArgs (interp, 1, objv, tnc_usage);
            return TCL_ERROR;
        }
        handlerSet = CHandlerSetGet (interp, objv[1], "tnc");
        if (!handlerSet) {
            SetResult("expat parser obj hasn't a C handler set named \"tnc\"");
            return TCL_ERROR;
        }
        tncdata = (TNC_Data *) handlerSet->userData;
        if (!tncdata->status) {
            SetResult ("No complete and error free DTD data available.");
            return TCL_ERROR;
        }
        /* After we finished, the validator structure is its own command,
           there isn't a parser cmd anymore. */
        tncdata->expatObj = NULL;
        tncdata->status = 0;
        handlerSet->userData = createTncData (interp, objv[1]);
        if (objc == 4) {
            cmdName = Tcl_GetStringFromObj (objv[3], NULL);
        } else {
            FindUniqueCmdName (interp, s);
            cmdName = s;
        }
        Tcl_CreateObjCommand (interp, cmdName, tnc_ValidateObjCmd, tncdata,
                              tnc_ValidateObjDeleteCmd);
        Tcl_SetResult (interp, cmdName, TCL_VOLATILE);
        return TCL_OK;

    default:
        Tcl_SetResult (interp, "unknown method", NULL);
        return TCL_ERROR;
    }

}

#ifdef BUILD_tnc
# undef TCL_STORAGE_CLASS
# define TCL_STORAGE_CLASS DLLEXPORT
#endif


/*
 *----------------------------------------------------------------------------
 *
 * Tnc_Init --
 *
 *	Initialization routine for loadable module
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Defines "tnc" enhancement command for expat parser obj
 *
 *----------------------------------------------------------------------------
 */

#if defined(_MSC_VER)
#  undef TCL_STORAGE_CLASS
#  define TCL_STORAGE_CLASS DLLEXPORT
#endif

EXTERN int
Tnc_Init (interp)
    Tcl_Interp *interp;
{
#ifdef USE_TCL_STUBS
    if (Tcl_InitStubs(interp, "8", 0) == NULL) {
        return TCL_ERROR;
    }
#endif
#ifdef USE_TDOM_STUBS
    if (Tdom_InitStubs(interp, "0.8", 0) == NULL) {
        return TCL_ERROR;
    }
#endif
    Tcl_PkgRequire (interp, "tdom", "0.8.0", 0);
    Tcl_CreateObjCommand (interp, "tnc", TclTncObjCmd, NULL, NULL );
    Tcl_PkgProvide (interp, "tnc", PACKAGE_VERSION);
    return TCL_OK;
}

