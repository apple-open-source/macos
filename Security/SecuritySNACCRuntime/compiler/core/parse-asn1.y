/*
 * compiler/core/parse-asn1.y
 *
 *	yacc source for ASN.1 '88 Parser
 *	As interpreted from Appendix II of CCITT recomendation X.208
 *
 *       Parses ASN.1 into a monster data structure
 *
 *       Some old versions of yacc will croak due the length
 *       of some of the symbols  (use -Nc10000 with other versions)
 *
 *	Mike Sample
 *	90/05/03
 *       91/09/02  Rewritten with "ASN.1" generated data struct
 *
 * Copyright (C) 1990, 1991, 1992  Michael Sample
 *               and the University of British Columbia
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * $Header: /cvs/Darwin/src/live/Security/SecuritySNACCRuntime/compiler/core/parse-asn1.y,v 1.1 2001/06/20 21:27:58 dmitch Exp $
 * $Log: parse-asn1.y,v $
 * Revision 1.1  2001/06/20 21:27:58  dmitch
 * Adding missing snacc compiler files.
 *
 * Revision 1.1.1.1  1999/03/16 18:06:51  aram
 * Originals from SMIME Free Library.
 *
 * Revision 1.11  1997/08/28 09:46:41  wan
 * Reworked number range checking, only gives warning now.
 *
 * Revision 1.10  1997/06/19 09:17:17  wan
 * Added isPdu flag to tables. Added value range checks during parsing.
 *
 * Revision 1.9  1997/03/13 14:48:28  wan
 * Parsed SEQUENCE SIZE(..) OF as SET, corrected.
 *
 * Revision 1.8  1997/03/03 11:58:34  wan
 * Final pre-delivery stuff (I hope).
 *
 * Revision 1.7  1997/02/28 13:39:55  wan
 * Modifications collected for new version 1.3: Bug fixes, tk4.2.
 *
 * Revision 1.6  1995/07/25 19:17:55  rj
 * use memzero that is defined in .../snacc.h to use either memset or bzero.
 *
 * changed `_' to `-' in file names.
 *
 * Revision 1.5  1995/02/18  12:52:21  rj
 * portablity fix (string(s).h)
 *
 * Revision 1.4  1995/02/17  20:13:21  rj
 * portablity fix (string(s).h)
 *
 * Revision 1.3  1994/10/08  03:42:46  rj
 * renamed the FLEX cpp define to FLEX_SCANNER since that's what flex defines.
 *
 * Revision 1.2  1994/09/01  00:42:03  rj
 * snacc_config.h removed.
 *
 * Revision 1.1  1994/08/28  09:49:29  rj
 * first check-in. for a list of changes to the snacc-1.1 distribution please refer to the ChangeLog.
 *
 */

%{

#include "snacc.h"

#if STDC_HEADERS || HAVE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif
#include <stdio.h>

#include "asn-incl.h"
#include "mem.h"
#include "asn1module.h"
#include "lib-types.h"
#include "snacc-util.h"
#include "exports.h"
#include "parser.h"
#include "lex-stuff.h"

/*
 * smallErrG
 *    used for small errors that should prevent code generation but not
 *    prevent the later error checking passes
 */
int smallErrG = FALSE;

/*
 * firstTimeThroughG
 *    used incase the asn1.lex was compiled with flex in which
 *    case the lexical analyzer must be reset for every ASN.1 file
 *    parsed, except the first
 */
static int firstTimeThroughG = TRUE;

/*
 *  modulePtrG
 *    used to hold the parsed value.  The root of the parse tree.
 */
Module *modulePtrG;


/*
 * oidElmtValDefsG
 *    used to hold integer values that are defined as arc numbers
 *    the modules object identifiers.
 * eg. FOO-MODULE { joint-iso-ccitt dod (2) foo (2) 3 2 } DEFINITIONS ::=
 *     would put dod/2 and foo/2 in the oidElmtValDefsG list
 * Note: only some oid's (modules name/import list module names)
 *       are parsed by the yacc code.  The rest are parsed later
 *       due to ambiguities that arise without type info.
 */
ValueDefList *oidElmtValDefsG = NULL;


/*
 * ApplTag
 *   used to hold APPLICATION tags that have been defined in
 *   a module.  This permits checking for the the error of
 *   using the same APPLICATION tag in 1 module.  The
 *   ApplTags list (appTagsG) is emptied for each module.
 */
typedef struct ApplTag
{
    unsigned long int lineNo;
    unsigned long int tagCode;
    struct ApplTag *next;
} ApplTag;

ApplTag *applTagsG  = NULL;

/*
 * Protos for ApplTag related stuff. These are defined at the
 * end  of this file
 */
void PushApplTag PROTO ((unsigned long int tagCode, unsigned long int lineNo));
void FreeApplTags();



/*
 * the following are globals to simplify disparity between
 * productions and produced data structure
 */

/*
 * these are used in the ValueRange subtype production
 */
static int      valueRangeUpperEndInclusiveG;
static int      valueRangeLowerEndInclusiveG;

/*
 * used to set exports flag in Type/value defs
 * exportListG holds the explicitly exported elements.
 * see SetExports routine in export.c
 */
ExportElmt *exportListG = NULL;
int         exportsParsedG;


/*
 * globals for the APPLICATION-CONTEXT macro productions
 */
static ValueList *rosAcSymmetricAsesG;
static ValueList *rosAcResponderConsumerOfG;
static ValueList *rosAcInitiatorConsumerOfG;

/*
 * used with MTSAS Extension macro
 * set to NULL for the initial parse.
 */
static AsnBool *mtsasCriticalForSubmissionG = NULL;
static AsnBool *mtsasCriticalForTransferG = NULL;
static AsnBool *mtsasCriticalForDeliveryG = NULL;

/*
 * Asn PORT macro globals
 */
static TypeOrValueList *asnConsumerG;
static TypeOrValueList *asnSupplierG;


/*
 * parseErrCountG
 *   used to prevent too many cascade errors
 */
int parseErrCountG = 0;
#define MAX_ERR 50
#define PARSE_ERROR()\
    parseErrCountG++;\
    modulePtrG->status = MOD_ERROR;\
    if (parseErrCountG > MAX_ERR)\
    {\
        fprintf (stderr, "Ackkkkk! too many errors - bye!\n");\
        exit (1);\
    }


%}


/*
 *	Union structure.  A terminal or non-terminal can have
 *	one of these type values.
 */

%union
{
    int              intVal;
    unsigned int     uintVal;
    char            *charPtr;
    Type            *typePtr;
    NamedType       *namedTypePtr;
    NamedTypeList   *namedTypeListPtr;
    Value           *valuePtr;
    NamedValue      *namedValuePtr;
    SubtypeValue    *subtypeValuePtr;
    Subtype         *subtypePtr;
    ModuleId        *moduleId;
    OID             *oidPtr;
    OidList         *oidListPtr;
    TypeDef         *typeDefPtr;
    TypeDefList     *typeDefListPtr;
    ValueDef        *valueDefPtr;
    ValueDefList    *valueDefListPtr;
    ExportElmt      *exportList;
    ImportModule    *importModulePtr;
    ImportModuleList *importModuleListPtr;
    ImportElmt      *importElmtPtr;
    ImportElmtList  *importElmtListPtr;
    Tag             *tagPtr;
    TagList         *tagListPtr;
    Constraint      *constraintPtr;
    ConstraintList  *constraintListPtr;
    InnerSubtype    *innerSubtypePtr;
    ValueList       *valueListPtr;
    TypeOrValueList *typeOrValueListPtr;
    TypeOrValue     *typeOrValuePtr;
    AsnPort         *asnPortPtr;
    AsnPortList     *asnPortListPtr;
    AttributeList   *attrList;
}

/*
 *	Terminals. Definitions can be found in input.lex.
 */

/*
 * these tokens (literals) have attributes (set in asn1.lex)
 */
%token <charPtr> BSTRING_SYM HSTRING_SYM CSTRING_SYM
                 UCASEFIRST_IDENT_SYM LCASEFIRST_IDENT_SYM
                 NAMEDMACRO_SYM MACRODEFBODY_SYM
                 BRACEBAL_SYM NUMBER_ERANGE

%token <uintVal> NUMBER_SYM

%token <charPtr> SNACC_ATTRIBUTES

/*
 * these tokens have no attributes
 */
%token        DOT_SYM COMMA_SYM LEFTBRACE_SYM RIGHTBRACE_SYM LEFTPAREN_SYM
              RIGHTPAREN_SYM LEFTBRACKET_SYM RIGHTBRACKET_SYM LESSTHAN_SYM
              MINUS_SYM GETS_SYM BAR_SYM TAGS_SYM BOOLEAN_SYM INTEGER_SYM
              BIT_SYM STRING_SYM OCTET_SYM NULL_SYM SEQUENCE_SYM OF_SYM
              SET_SYM IMPLICIT_SYM CHOICE_SYM ANY_SYM
              OBJECT_IDENTIFIER_SYM OPTIONAL_SYM DEFAULT_SYM COMPONENTS_SYM
              UNIVERSAL_SYM APPLICATION_SYM PRIVATE_SYM TRUE_SYM FALSE_SYM
              BEGIN_SYM END_SYM DEFINITIONS_SYM EXPLICIT_SYM ENUMERATED_SYM
              EXPORTS_SYM IMPORTS_SYM REAL_SYM INCLUDES_SYM MIN_SYM MAX_SYM
              SIZE_SYM FROM_SYM WITH_SYM COMPONENT_SYM PRESENT_SYM ABSENT_SYM
              DEFINED_SYM BY_SYM PLUS_INFINITY_SYM MINUS_INFINITY_SYM
              SEMI_COLON_SYM IA5STRING_SYM PRINTABLESTRING_SYM
              NUMERICSTRING_SYM TELETEXSTRING_SYM T61STRING_SYM
              VIDEOTEXSTRING_SYM VISIBLESTRING_SYM ISO646STRING_SYM
              GRAPHICSTRING_SYM GENERALSTRING_SYM
              GENERALIZEDTIME_SYM UTCTIME_SYM EXTERNAL_SYM
              OBJECTDESCRIPTOR_SYM
              /* the following are used in macros */
              OPERATION_SYM ARGUMENT_SYM RESULT_SYM ERRORS_SYM LINKED_SYM
              ERROR_SYM PARAMETER_SYM
              BIND_SYM BINDERROR_SYM UNBIND_SYM UNBINDERROR_SYM
              ASE_SYM OPERATIONS_SYM CONSUMERINVOKES_SYM
              SUPPLIERINVOKES_SYM
              AC_SYM ASES_SYM REMOTE_SYM INITIATOR_SYM RESPONDER_SYM
              ABSTRACTSYNTAXES_SYM CONSUMER_SYM
              EXTENSIONS_SYM CHOSEN_SYM
              EXTENSION_SYM CRITICAL_SYM FOR_SYM DELIVERY_SYM SUBMISSION_SYM
              TRANSFER_SYM  EXTENSIONATTRIBUTE_SYM
              TOKEN_SYM TOKENDATA_SYM
              SECURITYCATEGORY_SYM
              OBJECT_SYM PORTS_SYM BOXC_SYM BOXS_SYM
              PORT_SYM ABSTRACTOPS_SYM
              REFINE_SYM AS_SYM RECURRING_SYM VISIBLE_SYM PAIRED_SYM
              ABSTRACTBIND_SYM ABSTRACTUNBIND_SYM TO_SYM
              ABSTRACTERROR_SYM ABSTRACTOPERATION_SYM
              ALGORITHM_SYM ENCRYPTED_SYM SIGNED_SYM
              SIGNATURE_SYM PROTECTED_SYM
              OBJECTTYPE_SYM SYNTAX_SYM ACCESS_SYM STATUS_SYM
              DESCRIPTION_SYM REFERENCE_SYM INDEX_SYM
              DEFVAL_SYM

/*
 *  Type definitions of non-terminal symbols
 */

%type <intVal>  LineNo SetOpening SequenceOpening

%type <intVal>  TagDefault

%type <importElmtListPtr> SymbolList

%type <importModuleListPtr> SymbolsFromModuleList

%type <importModulePtr>  SymbolsFromModule

%type <typeDefPtr>    TypeAssignment

%type <valueDefPtr>    ValueAssignment

%type <charPtr> BinaryString HexString CharString

%type <intVal>   number Class

%type <intVal>   SignedNumber

%type <charPtr> modulereference typereference identifier Symbol


%type <valuePtr> ExternalValueReference
%type <valuePtr> Value DefinedValue BuiltinValue BooleanValue
                 NullValue SpecialRealValue

%type <namedValuePtr>  NamedValue

%type <moduleId>   ModuleIdentifier

%type <oidPtr>     ObjectIdentifierValue AssignedIdentifier
                   ObjIdComponent NumberForm NameAndNumberForm
                   ObjIdComponentList
%type <charPtr>    NameForm
%type <typePtr> BuiltinType DefinedType Subtype BooleanType IntegerType
                BitStringType NullType SequenceType
                SequenceOfType SetType SetOfType ChoiceType SelectionType
                TaggedType AnyType ObjectIdentifierType
                EnumeratedType RealType Type ExternalTypeReference



%type <namedTypePtr> NamedType ElementType

%type <namedTypeListPtr> AlternativeTypes AlternativeTypeList
                         ElementTypes ElementTypeList

%type <subtypeValuePtr> SubtypeValueSet SingleValue ContainedSubtype ValueRange
                        PermittedAlphabet SizeConstraint InnerTypeConstraints

%type <subtypePtr> SubtypeSpec SubtypeValueSetList

%type <constraintPtr> NamedConstraint Constraint

%type <constraintListPtr> TypeConstraints

%type <innerSubtypePtr>  FullSpecification PartialSpecification
                         SingleTypeConstraint MultipleTypeConstraints

%type <valuePtr> LowerEndPoint UpperEndPoint LowerEndValue UpperEndValue

%type <intVal>       PresenceConstraint

%type <subtypePtr>    ValueConstraint

%type <exportList>    ExportSymbolList

%type <valueDefPtr>   NamedNumber

%type <valueDefListPtr> NamedNumberList NamedBitList

%type <tagPtr>       Tag ClassNumber

%type <attrList>  SnaccAttributes SnaccAttributeCommentList

%type <charPtr>  DefinedMacroName MacroReference

%type <typePtr>  DefinedMacroType

%type <valueListPtr> PossiblyEmptyValueList  ValueList

%type <typeOrValueListPtr> PossiblyEmptyTypeOrValueList TypeOrValueList

%type <typeOrValuePtr> TypeOrValue

%type <typePtr>  RosOperationMacroType RosOperationMacroBody RosErrorMacroType
                 RosBindMacroType RosUnbindMacroType
                 RosAseMacroType RosAcMacroType

%type <namedTypePtr> RosOpArgument RosOpResult RosOpResultType

%type <typeOrValueListPtr>  RosOpErrors RosOpLinkedOps

%type <namedTypePtr> RosErrParameter

%type <namedTypePtr>  RosBindArgument RosBindResult RosBindError RosUnbindError

%type <valueListPtr>  RosAseSymmetricAse RosAseConsumerInvokes
                     RosAseSupplierInvokes RosAseOperationList

%type <valueListPtr>  RosAcNonRoElements

%type <valuePtr>  RosAcRoElements

%type <oidListPtr>  OidList RosAcAbstractSyntaxes

%type <typePtr> MtsasExtensionsMacroType MtsasExtensionMacroType
                MtsasExtensionAttributeMacroType MtsasTokenMacroType
                MtsasTokenDataMacroType MtsasSecurityCategoryMacroType

%type <valuePtr> MtsasExtDefaultVal

%type <typePtr>    AsnObjectMacroType AsnPortMacroType AsnRefineMacroType
                   AsnAbstractBindMacroType AsnAbstractUnbindMacroType
                   AsnAbstractOperationMacroType AsnAbstractErrorMacroType

%type <asnPortListPtr> AsnPorts AsnPortList

%type <asnPortPtr> AsnPort

%type <intVal>     AsnPortType

%type <intVal>     AsnObject AsnObjectList AsnPortSpec AsnPortSpecList
                   AsnObjectSpec AsnComponent AsnComponentList

%type <typeOrValueListPtr> AsnOperations AsnConsumer AsnSupplier

%type <asnPortListPtr> AsnAbstractBindPorts AsnAbstractUnbindPorts

%type <typePtr>    AfAlgorithmMacroType AfEncryptedMacroType
                   AfSignedMacroType AfSignatureMacroType
                   AfProtectedMacroType

%type <typePtr>  SnmpObjectTypeMacroType
%type <intVal>   SnmpStatus SnmpAccess
%type <valuePtr> SnmpDescrPart SnmpReferPart SnmpDefValPart
%type <typeOrValueListPtr>  SnmpIndexPart

%start ModuleDefinition
%%



/*-----------------------------------------------------------------------*/
/* Module def/import/export productions */
/*-----------------------------------------------------------------------*/

LineNo: { $$ = myLineNoG; }

ModuleDefinition:
     ModuleIdentifier
     DEFINITIONS_SYM
     TagDefault { modulePtrG->tagDefault = $3; }
     GETS_SYM
     BEGIN_SYM
     ModuleBody
     END_SYM
     {
         modulePtrG->modId      = $1;

         /*
          * Set exported flags in type/value defs as appropriate
          */
         SetExports (modulePtrG, exportListG, exportsParsedG);

         /* clean up */

         /* Free Application tag list */
         FreeApplTags();

         /*
          * Add values defined in any parsed object identifiers.
          * Only the Module name and some macro oids have been parsed,
          * the rest are just "{...}" strings at this point
          * (they will be parsed in later)
          */
         modulePtrG->valueDefs =
             AsnListConcat (modulePtrG->valueDefs, oidElmtValDefsG);

         /*
          * free list head only
          */
         Free (oidElmtValDefsG);
     }
;

TagDefault:
    EXPLICIT_SYM TAGS_SYM { $$ = EXPLICIT_TAGS; }
  | IMPLICIT_SYM TAGS_SYM { $$ = IMPLICIT_TAGS; }
  | empty
    {
        /* default is EXPLICIT TAGS */
        $$ = EXPLICIT_TAGS;
    }
;

ModuleIdentifier:
    modulereference AssignedIdentifier
    {
        $$ = MT (ModuleId);
        $$->name = $1;
        $$->oid = $2;
    }
;

AssignedIdentifier:
    ObjectIdentifierValue
  | empty { $$ = NULL; }
;

ModuleBody:
    Exports Imports AssignmentList
  | empty
;

Exports:
    EXPORTS_SYM SymbolsExported SEMI_COLON_SYM
    {
        /*
         *  allows differentiation between "EXPORTS;"
         *         (in which no exports allowed)
         *  and when the EXPORTS symbol does not appear
         *         (then all are exported)
         */
        exportsParsedG = TRUE;
    }
  | EXPORTS_SYM error SEMI_COLON_SYM
    {
        PARSE_ERROR();
        exportsParsedG = FALSE;
        exportListG = NULL;
        yyerrok;
    }
  | empty { exportsParsedG = FALSE; }
;

SymbolsExported:
    ExportSymbolList { exportListG = $1; }
  | empty { exportListG = NULL; }
;

ExportSymbolList:
    Symbol
    {
        $$ = MT (ExportElmt);
        $$->name = $1;
        $$->lineNo = myLineNoG;
        $$->next = NULL;
    }
  | ExportSymbolList COMMA_SYM LineNo Symbol
    {
        $$ = MT (ExportElmt);
        $$->name = $4;
        $$->next = $1;
        $$->lineNo = $3;
    }
;

Imports:
    IMPORTS_SYM SymbolsImported SEMI_COLON_SYM
  | IMPORTS_SYM error SEMI_COLON_SYM
    {
       PARSE_ERROR();
       yyerrok;
    }
  | empty
;

SymbolsImported:
    SymbolsFromModuleList { modulePtrG->imports = $1; }
  | empty
;

SymbolsFromModuleList:
    SymbolsFromModuleList SymbolsFromModule
    {
        APPEND ($2,$1);
    }
  | SymbolsFromModule
    {
        $$ = NEWLIST();
        APPEND ($1, $$);
    }
;

SymbolsFromModule:
    SymbolList FROM_SYM LineNo ModuleIdentifier
    {
        $$ = MT (ImportModule);
        $$->modId   = $4;
        $$->lineNo = $3;
        $$->importElmts = $1;
    }
;


SymbolList:
    SymbolList COMMA_SYM Symbol
    {
        ImportElmt *ie;

        ie = MT (ImportElmt);
        ie->name = $3;
        ie->lineNo = myLineNoG;
        APPEND (ie, $1);
        $$ = $1;
    }
  | Symbol
    {
        ImportElmt *ie;

        /* called for the first element only, so create list head */
        $$ = NEWLIST();
        ie = MT (ImportElmt);
        ie->name = $1;
        ie->lineNo = myLineNoG;
        APPEND (ie, $$);
    }
;

Symbol:
    typereference
  | identifier
  | DefinedMacroName  /* This solves macro "keyword" problem */
    {
        /*
         * hack to make DefinedMacroNames "freeable"
         * like idents and typeref
         */
        $$ = Malloc (strlen ($1)+1);
        strcpy ($$, $1);
    }
;



AssignmentList:
    AssignmentList AssignmentOrError
  | AssignmentOrError
;

AssignmentOrError:
    Assignment
  | Assignment SEMI_COLON_SYM
  | error SEMI_COLON_SYM
    {
        PARSE_ERROR();
        yyerrok;
    }
;

Assignment:
    TypeAssignment
    {
        /*
         * a macro may produce a null type
         */
        if ($1 != NULL)
        {
            /*
             * add to head of  type def list
             */
            APPEND ($1, modulePtrG->typeDefs);
        }

    }
  | ValueAssignment
    {
        /*
         * a macro may produce a null value
         */
        if ($1 != NULL)
        {
            /*
             * add to head of value def list
             */
            APPEND ($1, modulePtrG->valueDefs);
        }
    }
  | NAMEDMACRO_SYM GETS_SYM BEGIN_SYM LineNo { LexBeginMacroDefContext(); }
    MACRODEFBODY_SYM
    {
        TypeDef *tmpTypeDef;

        /*
         *  LEXICAL TIE IN!!
         * create macro type to eliminate import resolution
         * errors msgs from other modules importing the macro.
         * (hopefully) Only the import list will link with
         * these type defs.
         * keeps macro def around incase of future processing needs
         *
         * NOTE: MACRODEFBODY_SYM returns the macro def body with
         * with "BEGIN" at the begininning and "END" at the end
         */

        /*
         * put lexical analyzer back in normal state
         */
        /*  BEGIN (INITIAL);  */
        LexBeginInitialContext();

        tmpTypeDef = MT (TypeDef);
        SetupType (&tmpTypeDef->type, BASICTYPE_MACRODEF, $4);
        tmpTypeDef->definedName = $1;

        /*
         * keeps the macro def body
         * (all text between & including the BEGIN and END)
         * as a simple string - incase you want to fart around with
         * it.
         */
        tmpTypeDef->type->basicType->a.macroDef = $6;

        /*
         * put in type list
         */
        APPEND (tmpTypeDef, modulePtrG->typeDefs);

    }
  | NAMEDMACRO_SYM GETS_SYM MacroReference
    {
        TypeDef *tmpTypeDef;

        tmpTypeDef = MT (TypeDef);
        SetupType (&tmpTypeDef->type, BASICTYPE_MACRODEF, myLineNoG);
        tmpTypeDef->definedName = $1;

        tmpTypeDef->type->basicType->a.macroDef = $3;

        /*
         * put in type list
         */
        APPEND (tmpTypeDef, modulePtrG->typeDefs);

    }
  | NAMEDMACRO_SYM GETS_SYM modulereference DOT_SYM MacroReference
    {
        TypeDef *tmpTypeDef;

        tmpTypeDef = MT (TypeDef);
        SetupType (&tmpTypeDef->type, BASICTYPE_MACRODEF, myLineNoG);
        tmpTypeDef->definedName = $1;

        tmpTypeDef->type->basicType->a.macroDef =
               (MyString) Malloc (strlen ($3) + strlen ($5) + 2);

        strcpy (tmpTypeDef->type->basicType->a.macroDef, $3);
        strcat (tmpTypeDef->type->basicType->a.macroDef, ".");
        strcat (tmpTypeDef->type->basicType->a.macroDef, $5);

       /*
         * put in type list
         */
        APPEND (tmpTypeDef, modulePtrG->typeDefs);

        Free ($3);
        Free ($5);
    }
;

MacroReference:
    typereference
  | DefinedMacroName
;

/*-----------------------------------------------------------------------*/
/* Type Notation Productions */
/*-----------------------------------------------------------------------*/

TypeAssignment:
    typereference GETS_SYM SnaccAttributes LineNo Type SnaccAttributes
    {
        /*
         * a macro type may produce a null type
         */
        if ($5 != NULL)
        {
            $$ = MT (TypeDef);
            $$->type =  $5;
            $$->type->lineNo = $4;
            $$->type->attrList = $6;
            $$->definedName = $1;
            $$->attrList = $3;
        }
        else
            $$ = NULL;
    }
;


ExternalTypeReference:
    modulereference DOT_SYM LineNo typereference
    {
        /* allocate a Type with basic type of ImportTypeRef */
        SetupType (&$$, BASICTYPE_IMPORTTYPEREF, $3);
        $$->basicType->a.importTypeRef = MT (TypeRef);
        $$->basicType->a.importTypeRef->typeName = $4;
        $$->basicType->a.importTypeRef->moduleName = $1;

        /* add entry to this module's import list */
        AddPrivateImportElmt (modulePtrG, $4, $1, $3);
    }
;


DefinedType:      /* could by CharacterString or Useful types too */
    ExternalTypeReference { $$ = $1; }
  | typereference
    {
        SetupType (&$$, BASICTYPE_LOCALTYPEREF, myLineNoG);
        $$->basicType->a.localTypeRef = MT (TypeRef);
        $$->basicType->a.localTypeRef->typeName = $1;
    }
;



Type:
    DefinedMacroType
  | BuiltinType
  | DefinedType
  | Subtype
;

BuiltinType:
    BooleanType
  | IntegerType
  | BitStringType
  | NullType
  | SequenceType
  | SequenceOfType
  | SetType
  | SetOfType
  | ChoiceType
  | SelectionType
  | TaggedType
  | AnyType
  | ObjectIdentifierType
  | EnumeratedType
  | RealType
  | OCTET_SYM STRING_SYM
    {
        SetupType (&$$, BASICTYPE_OCTETSTRING, myLineNoG);
    }
;

NamedType:
    identifier Type
    {
        $$ = MT (NamedType);
        $$->type = $2;
        $$->fieldName = $1;
    }
  | Type       /* this handles selectionType as well */
    {
        $$ = MT (NamedType);
        $$->type = $1;
    }
;

BooleanType:
    BOOLEAN_SYM
    {
        SetupType (&$$, BASICTYPE_BOOLEAN, myLineNoG);
    }
;

IntegerType:
    INTEGER_SYM
    {
        SetupType (&$$, BASICTYPE_INTEGER, myLineNoG);
        $$->basicType->a.integer = NEWLIST();  /* empty list */
    }
  | INTEGER_SYM LEFTBRACE_SYM NamedNumberList RIGHTBRACE_SYM
    {
        SetupType (&$$, BASICTYPE_INTEGER, myLineNoG);
        $$->basicType->a.integer = $3;
    }
;


NamedNumberList:
    NamedNumber
    {
        $$ = NEWLIST();
        APPEND ($1, $$);
    }
  | NamedNumberList COMMA_SYM NamedNumber
    {
        APPEND ($3,$1);
        $$ = $1;
    }
;

NamedNumber:
    identifier LEFTPAREN_SYM SignedNumber RIGHTPAREN_SYM
    {
        $$ = MT (ValueDef);
        $$->definedName = $1;
        SetupValue (&$$->value, BASICVALUE_INTEGER, myLineNoG);
        $$->value->basicValue->a.integer = $3;
    }
  | identifier LEFTPAREN_SYM DefinedValue RIGHTPAREN_SYM
    {
        $$ = MT (ValueDef);
        $$->definedName = $1;
        $$->value = $3;
    }
;

SignedNumber:
    NUMBER_SYM
    {
	if ($1>0x7FFFFFFF) {
	    yyerror("Warning: positive signed number out of range");
	    $$ = 0x7FFFFFFF;
	}
    }
    | NUMBER_ERANGE
    {
	yyerror ("Warning: positive signed number out of range");
	$$ = 0x7FFFFFFF;
	/* modulePtrG->status = MOD_ERROR; */
    }
    | MINUS_SYM NUMBER_SYM
    {
	if ($2>0x80000000) {
	    yyerror("Warning: negative signed number out of range");
	    $$ = -0x80000000;
	} else if ($2==0x80000000) {
	    $$ = -0x80000000;
	} else {
	    $$ = -$2;
	}
    }
    | MINUS_SYM NUMBER_ERANGE
    {
	yyerror ("Warning: negative signed number out of range");
	$$ = -0x80000000;
	/* modulePtrG->status = MOD_ERROR; */
    }
;

EnumeratedType:
    ENUMERATED_SYM LEFTBRACE_SYM NamedNumberList RIGHTBRACE_SYM
    {
        SetupType (&$$, BASICTYPE_ENUMERATED, myLineNoG);
        $$->basicType->a.enumerated = $3;
    }
;


RealType:
    REAL_SYM
    {
        SetupType (&$$, BASICTYPE_REAL, myLineNoG);
    }
;

BitStringType:
    BIT_SYM STRING_SYM
    {
        SetupType (&$$, BASICTYPE_BITSTRING, myLineNoG);
        $$->basicType->a.bitString = NEWLIST(); /* empty list */
    }
  | BIT_SYM STRING_SYM LEFTBRACE_SYM NamedBitList RIGHTBRACE_SYM
    {
        SetupType (&$$, BASICTYPE_BITSTRING, myLineNoG);
        $$->basicType->a.bitString = $4;
    }
;

NamedBitList:
    NamedNumberList
;



NullType:
    NULL_SYM
    {
        SetupType (&$$, BASICTYPE_NULL, myLineNoG);
    }
;

SequenceOpening:
    SEQUENCE_SYM LineNo LEFTBRACE_SYM
    { $$ = $2; }
;

SequenceType:
    SequenceOpening ElementTypes RIGHTBRACE_SYM
    {
        NamedType *n;

        SetupType (&$$, BASICTYPE_SEQUENCE, $1);

        if (AsnListCount ((AsnList*)$2) != 0)
        {
            n = (NamedType*) FIRST_LIST_ELMT ((AsnList*)$2);
            n->type->lineNo = $1;
        }

        $$->basicType->a.sequence = $2;

    }
  | SequenceOpening RIGHTBRACE_SYM
    {
        SetupType (&$$, BASICTYPE_SEQUENCE, $1);

        /* set up empty list for SEQ with no elmts */
        $$->basicType->a.sequence = AsnListNew (sizeof (void*));
    }
/*  | SEQUENCE_SYM LEFTBRACE_SYM error RIGHTBRACE_SYM
    {
        PARSE_ERROR();
        yyerrok;
    } */
;

ElementTypes:
    ElementTypeList SnaccAttributes
    {
        NamedType *lastElmt;

        if ($2 != NULL)
        {
            lastElmt = (NamedType*)LAST_LIST_ELMT ($1);
            lastElmt->type->attrList = $2;
        }
        $$ = $1;
    }
;

ElementTypeList:
    ElementType
    {
        $$ = NEWLIST();
        APPEND ($1,$$);
    }
  | ElementTypeList COMMA_SYM SnaccAttributes LineNo ElementType
    {
        NamedType *lastElmt;

        if ($3 != NULL)
        {
            lastElmt = (NamedType*)LAST_LIST_ELMT ($1);
            lastElmt->type->attrList = $3;
        }

       APPEND ($5, $1);
       lastElmt = (NamedType*)LAST_LIST_ELMT ($1);
       lastElmt->type->lineNo = $4;
       $$ = $1;
    }
;

ElementType:
    NamedType
  | NamedType OPTIONAL_SYM
    {
        $$ = $1;
        $$->type->optional = TRUE;
    }
  | NamedType DEFAULT_SYM NamedValue
    {
        /*
         * this rules uses NamedValue instead of Value
         * for the stupid choice value syntax (fieldname value)
         * it should be like a set/seq value (ie with
         * enclosing { }
         */
        $$ = $1;
        $$->type->defaultVal = $3;
        /*
         * could link value to the elmt type here (done in link_types.c)
         */
    }
  | COMPONENTS_SYM OF_SYM Type
    {
        $$ = MT (NamedType);
        SetupType (&$$->type, BASICTYPE_COMPONENTSOF, myLineNoG);
        $$->type->basicType->a.componentsOf = $3;
    }
  | identifier COMPONENTS_SYM OF_SYM Type
    {
        $$ = MT (NamedType);
        SetupType (&$$->type, BASICTYPE_COMPONENTSOF, myLineNoG);
        $$->fieldName = $1;
        $$->type->basicType->a.componentsOf = $4;
    }
;



SequenceOfType:
    SEQUENCE_SYM OF_SYM Type
    {
        NamedType *n;

        /* does not use SEQUENCE == SEQ OF ANY abrev*/
        SetupType (&$$, BASICTYPE_SEQUENCEOF, myLineNoG);

        /* grab line number from first elmt */
        if ($3 != NULL)
            $$->lineNo = $3->lineNo - 1;

        $$->basicType->a.sequenceOf = $3;
    }
;

SetOpening:
    SET_SYM LineNo LEFTBRACE_SYM { $$ = $2; }
;

SetType:
    SetOpening ElementTypes RIGHTBRACE_SYM
    {
        NamedType *n;

        SetupType (&$$, BASICTYPE_SET, $1);

        /* reset first elmt's line number */
        if (AsnListCount ((AsnList*)$2) != 0)
        {
            n = (NamedType*)FIRST_LIST_ELMT ((AsnList*)$2);
            n->type->lineNo = $1;
        }
        $$->basicType->a.set = $2;
    }
  | SetOpening RIGHTBRACE_SYM
    {
        SetupType (&$$, BASICTYPE_SET, $1);

        /* set up empty elmt list for SET */
        $$->basicType->a.set = AsnListNew (sizeof (void*));
    }
/*  | SET_SYM LEFTBRACE_SYM error RIGHTBRACE_SYM
    {
        PARSE_ERROR();
        yyerrok;
    } */
;


SetOfType:
    SET_SYM OF_SYM Type
    {
       /* does not allow SET == SET OF ANY Abrev */
        SetupType (&$$, BASICTYPE_SETOF, myLineNoG);

        if ($3 != NULL)
            $$->lineNo = $3->lineNo;

        $$->basicType->a.setOf = $3;
    }
;


ChoiceType:
    CHOICE_SYM LineNo LEFTBRACE_SYM AlternativeTypes RIGHTBRACE_SYM
    {
        NamedType *n;

        SetupType (&$$, BASICTYPE_CHOICE, $2);

        $$->basicType->a.choice = $4;

        if (AsnListCount ($4) != 0)
        {
            n = (NamedType*)FIRST_LIST_ELMT ($4);
            n->type->lineNo = $2;
        }
    }
;

AlternativeTypes:
    AlternativeTypeList SnaccAttributes
    {
        NamedType *lastElmt;
        if ($2 != NULL)
        {
            lastElmt = (NamedType*)LAST_LIST_ELMT ($1);
            lastElmt->type->attrList = $2;
        }
        $$ = $1;
    }
;

AlternativeTypeList:
    NamedType
    {
        $$ = NEWLIST();
        APPEND ($1, $$);
    }
  | AlternativeTypeList COMMA_SYM SnaccAttributes NamedType
    {
        NamedType *lastElmt;

        if ($3 != NULL)
        {
            lastElmt = (NamedType*)LAST_LIST_ELMT ($1);
            lastElmt->type->attrList = $3;
        }
        APPEND ($4,$1);
        $$ = $1;
    }
;


SelectionType:
    identifier LESSTHAN_SYM Type
    {
        /*
         * the selection type should be replaced after
         * link with actual type
         */
        SetupType (&$$, BASICTYPE_SELECTION, myLineNoG);

        $$->basicType->a.selection = MT (SelectionType);
        $$->basicType->a.selection->typeRef = $3;
        $$->basicType->a.selection->fieldName = $1;
    }
;

TaggedType:
    Tag Type
    {
        Tag *tag;

        /* remove next tag if any  && IMPLICIT_TAGS */
 	if ((modulePtrG->tagDefault == IMPLICIT_TAGS) &&
            ($2->tags != NULL) && !LIST_EMPTY ($2->tags))
        {
            tag = (Tag*)FIRST_LIST_ELMT ($2->tags); /* set curr to first */
	    AsnListFirst ($2->tags); /* set curr to first elmt */
            AsnListRemove ($2->tags);      /* remove first elmt */

            /*
             * set implicit if implicitly tagged built in type (ie not ref)
             * (this simplifies the module ASN.1 printer (print.c))
             */
            if (tag->tclass == UNIV)
                 $2->implicit = TRUE;

            Free (tag);
        }

        PREPEND ($1, $2->tags);
        $$ = $2;
    }
  | Tag IMPLICIT_SYM Type
    {
        Tag *tag;

        /* remove next tag if any */
 	if (($3->tags != NULL) && !LIST_EMPTY ($3->tags))
        {
            tag = (Tag*)FIRST_LIST_ELMT ($3->tags); /* set curr to first */
	    AsnListFirst ($3->tags); /* set curr to first elmt */
            AsnListRemove ($3->tags);      /* remove first elmt */

            if (tag->tclass == UNIV)
                 $3->implicit = TRUE;

            Free (tag);
        }

        /*
         * must check after linking that implicitly tagged
         * local/import type refs are not untagged choice/any etc
         */
        else if (($3->basicType->choiceId == BASICTYPE_IMPORTTYPEREF) ||
                 ($3->basicType->choiceId == BASICTYPE_LOCALTYPEREF) ||
                 ($3->basicType->choiceId == BASICTYPE_SELECTION))
            $3->implicit = TRUE;

        /*
         *  all other implicitly tagable types should have tags
         *  to remove - if this else clause fires then it is
         *  probably a CHOICE or ANY type
         */
        else
        {
            PrintErrLoc (modulePtrG->asn1SrcFileName, $3->lineNo);
            fprintf (stderr, "ERROR - attempt to implicitly reference untagged type\n");
            smallErrG = 1;
        }

        PREPEND ($1, $3->tags);
        $$ = $3;
    }
  | Tag EXPLICIT_SYM Type
    {
        /* insert tag at head of list */
        $1->explicit = TRUE;
        PREPEND ($1, $3->tags);
        $$ = $3;
    }
;

Tag:
    LEFTBRACKET_SYM Class ClassNumber RIGHTBRACKET_SYM
    {
        $$ = $3;
        $$->tclass = $2;
        $$->explicit = FALSE; /* default to false */

        /*
         *  keep track of APPLICATION Tags per module
         *  should only be used once
         */
        if ($2 == APPL)
        {
            PushApplTag ($$->code, myLineNoG);
        }
    }
;

ClassNumber:
    number
    {
        $$ = MT (Tag);
        $$->code = $1;
    }
  | DefinedValue
    {
        $$ = MT (Tag);
        $$->code = NO_TAG_CODE;
        $$->valueRef = $1;
    }
;

Class:
    UNIVERSAL_SYM    { $$ = UNIV; }
  | APPLICATION_SYM  { $$ = APPL; }
  | PRIVATE_SYM      { $$ = PRIV; }
  | empty            { $$ = CNTX; }
;


AnyType:
    ANY_SYM
    {
        SetupType (&$$, BASICTYPE_ANY, myLineNoG);
    }
  | ANY_SYM DEFINED_SYM BY_SYM identifier
    {
        SetupType (&$$, BASICTYPE_ANYDEFINEDBY, myLineNoG);
        $$->basicType->a.anyDefinedBy = MT (AnyDefinedByType);
        $$->basicType->a.anyDefinedBy->fieldName = $4;
    }
;


ObjectIdentifierType:
    OBJECT_IDENTIFIER_SYM
    {
        SetupType (&$$, BASICTYPE_OID, myLineNoG);
    }
;


Subtype:
    Type SubtypeSpec
    {
        /*
         * append new subtype list to existing one (s) if any
         * with AND relation
         */
        AppendSubtype (&$1->subtypes, $2, SUBTYPE_AND);
        $$ = $1;
    }
  | SET_SYM SizeConstraint OF_SYM Type
    {
        Subtype *s;

        SetupType (&$$, BASICTYPE_SETOF, myLineNoG);
        $$->basicType->a.setOf = $4;

        /* add size constraint */
        s = MT (Subtype);
        s->choiceId = SUBTYPE_SINGLE;
        s->a.single = $2;
        AppendSubtype (&$$->subtypes, s, SUBTYPE_AND);
    }
  | SEQUENCE_SYM SizeConstraint OF_SYM Type
    {
        Subtype *s;

        SetupType (&$$, BASICTYPE_SEQUENCEOF, myLineNoG);
        $$->basicType->a.sequenceOf = $4;

        /* add size constraint */
        s = MT (Subtype);
        s->choiceId = SUBTYPE_SINGLE;
        s->a.single = $2;
        AppendSubtype (&$$->subtypes, s, SUBTYPE_AND);
    }
;


SubtypeSpec:
    LEFTPAREN_SYM SubtypeValueSetList RIGHTPAREN_SYM
    {
        $$ = $2;
    }
;

SubtypeValueSetList:
    SubtypeValueSet
    {
        Subtype *s;

        /* OR relation between all elmts of in  ValueSetList */

        $$ = MT (Subtype);
        $$->choiceId = SUBTYPE_OR;
        $$->a.or = NEWLIST();

        s = MT (Subtype);
        s->choiceId = SUBTYPE_SINGLE;
        s->a.single = $1;
        APPEND (s, $$->a.or);
    }
  | SubtypeValueSetList BAR_SYM SubtypeValueSet
    {
        Subtype *s;
        s = MT (Subtype);
        s->choiceId = SUBTYPE_SINGLE;
        s->a.single = $3;
        APPEND (s, $1->a.or);
        $$ = $1;
    }
;


SubtypeValueSet:
    SingleValue
  | ContainedSubtype
  | ValueRange
  | PermittedAlphabet
  | SizeConstraint
  | InnerTypeConstraints
;

SingleValue:
    Value
    {
        $$ = MT (SubtypeValue);
        $$->choiceId = SUBTYPEVALUE_SINGLEVALUE;
        $$->a.singleValue = $1;
    }

ContainedSubtype:
    INCLUDES_SYM Type
    {
        $$ = MT (SubtypeValue);
        $$->choiceId = SUBTYPEVALUE_CONTAINED;
        $$->a.contained = $2;
    }
;

ValueRange:
    LowerEndPoint DOT_SYM DOT_SYM UpperEndPoint
    {
        $$ = MT (SubtypeValue);
        $$->choiceId = SUBTYPEVALUE_VALUERANGE;
        $$->a.valueRange = MT (ValueRangeSubtype);
        $$->a.valueRange->lowerEndInclusive =
            valueRangeLowerEndInclusiveG;
        $$->a.valueRange->upperEndInclusive =
            valueRangeUpperEndInclusiveG;
        $$->a.valueRange->lowerEndValue  = $1;
        $$->a.valueRange->upperEndValue = $4;
    }
;

LowerEndPoint:
    LowerEndValue
    {
       $$ = $1;
       valueRangeLowerEndInclusiveG = TRUE;
    }
  | LowerEndValue LESSTHAN_SYM
    {
       $$ = $1;
       valueRangeLowerEndInclusiveG = FALSE;
    }
;

UpperEndPoint:
    UpperEndValue
    {
       $$ = $1;
       valueRangeUpperEndInclusiveG = TRUE;
    }
  | LESSTHAN_SYM UpperEndValue
    {
       $$ = $2;
       valueRangeUpperEndInclusiveG = FALSE;
    }
;

LowerEndValue:
    Value { $$ = $1; }
  | MIN_SYM
    {
        SetupValue (&$$, BASICVALUE_SPECIALINTEGER, myLineNoG);
        $$->basicValue->a.specialInteger =  MIN_INT;
    }
;

UpperEndValue:
    Value { $$ = $1; }
  | MAX_SYM
    {
        SetupValue (&$$, BASICVALUE_SPECIALINTEGER, myLineNoG);
        $$->basicValue->a.specialInteger =  MAX_INT;
    }
;

SizeConstraint:
    SIZE_SYM SubtypeSpec
    {
        $$ = MT (SubtypeValue);
        $$->choiceId = SUBTYPEVALUE_SIZECONSTRAINT;
        $$->a.sizeConstraint = $2;
    }
;


PermittedAlphabet:
    FROM_SYM SubtypeSpec
    {
        $$ = MT (SubtypeValue);
        $$->choiceId = SUBTYPEVALUE_PERMITTEDALPHABET;
        $$->a.permittedAlphabet = $2;
    }
;

InnerTypeConstraints:
    WITH_SYM COMPONENT_SYM SingleTypeConstraint
    {
        $$ = MT (SubtypeValue);
        $$->choiceId = SUBTYPEVALUE_INNERSUBTYPE;
        $$->a.innerSubtype = $3;
    }
  | WITH_SYM COMPONENTS_SYM MultipleTypeConstraints
    {
        $$ = MT (SubtypeValue);
        $$->choiceId = SUBTYPEVALUE_INNERSUBTYPE;
        $$->a.innerSubtype = $3;
    }
;

SingleTypeConstraint:
    SubtypeSpec
    {
        Constraint *constraint;

        /* this constrains the elmt of setof or seq of */
        $$ = MT (InnerSubtype);
        $$->constraintType = SINGLE_CT;
        $$->constraints = NEWLIST();
        constraint = MT (Constraint);
        APPEND (constraint, $$->constraints);
        constraint->valueConstraints = $1;
    }
;

MultipleTypeConstraints:
    FullSpecification
  | PartialSpecification
;

FullSpecification:
    LEFTBRACE_SYM TypeConstraints RIGHTBRACE_SYM
    {
        $$ = MT (InnerSubtype);
        $$->constraintType = FULL_CT;
        $$->constraints = $2;
    }
;

PartialSpecification:
    LEFTBRACE_SYM DOT_SYM DOT_SYM DOT_SYM COMMA_SYM TypeConstraints RIGHTBRACE_SYM
    {
        $$ = MT (InnerSubtype);
        $$->constraintType = PARTIAL_CT;
        $$->constraints = $6;
    }
;


TypeConstraints:
    NamedConstraint
    {
        $$ = NEWLIST();
        APPEND ($1, $$);
    }
  | TypeConstraints COMMA_SYM NamedConstraint
    {
        APPEND ($3, $1);
        $$ = $1;
    }
;

NamedConstraint:
    identifier Constraint
    {
        $$ = $2;
        $$->fieldRef = $1;
    }
  | Constraint

;

Constraint:
    ValueConstraint PresenceConstraint
    {
        $$ = MT (Constraint);
        $$->presenceConstraint = $2;
        $$->valueConstraints = $1;
    }
;

ValueConstraint:
    SubtypeSpec { $$ = $1; }
  | empty       { $$ = NULL; }
;

PresenceConstraint:
    PRESENT_SYM   { $$ = PRESENT_CT; }
  | ABSENT_SYM    { $$ = ABSENT_CT; }
  | empty         { $$ = EMPTY_CT; }
  | OPTIONAL_SYM  { $$ = OPTIONAL_CT; }
;






/*-----------------------------------------------------------------------*/
/* Value Notation Productions */
/*-----------------------------------------------------------------------*/

ValueAssignment:
    identifier Type GETS_SYM LineNo Value
    {
        $$ = MT (ValueDef);
        $$->definedName = $1;
        $$->value = $5;
        $$->value->lineNo = $4;
        $$->value->type = $2;
    }
;


Value:
    BuiltinValue
  | DefinedValue
;

DefinedValue:
    ExternalValueReference { $$ =  $1; }
  | identifier  /* a defined value or a named elmt ref  */
    {
        /*
         * for parse, may be set to BASICVALUE_IMPORTEDTYPEREF
         * by linker
         */
        SetupValue (&$$, BASICVALUE_LOCALVALUEREF, myLineNoG);
        $$->basicValue->a.localValueRef = MT (ValueRef);
        $$->basicValue->a.localValueRef->valueName = $1;
        $$->valueType = BASICTYPE_UNKNOWN;
   }
;

ExternalValueReference:
    modulereference  DOT_SYM LineNo identifier
    {
        /* Alloc value with basicValue of importValueRef */
        SetupValue (&$$, BASICVALUE_IMPORTVALUEREF, $3);
        $$->valueType = BASICTYPE_UNKNOWN;
        $$->basicValue->a.importValueRef = MT (ValueRef);
        $$->basicValue->a.importValueRef->valueName = $4;
        $$->basicValue->a.importValueRef->moduleName = $1;

        /* add entry to this module's import list */
        AddPrivateImportElmt (modulePtrG, $4, $1, $3);
    }
;

BuiltinValue:
    BooleanValue
  | NullValue
  | SpecialRealValue
  | SignedNumber         /* IntegerValue  or "0" real val*/
    {
        SetupValue (&$$, BASICVALUE_INTEGER, myLineNoG);
        $$->valueType = BASICTYPE_UNKNOWN;
        $$->basicValue->a.integer = $1;
    }
  | HexString    /* OctetStringValue or BinaryStringValue */
    {
        SetupValue (&$$, BASICVALUE_ASCIIHEX, myLineNoG);
        $$->valueType = BASICTYPE_UNKNOWN;
        $$->basicValue->a.asciiHex = MT (AsnOcts);
        $$->basicValue->a.asciiHex->octs = $1;
        $$->basicValue->a.asciiHex->octetLen = strlen ($1);
    }
  | BinaryString    /*  BinaryStringValue */
    {
        SetupValue (&$$, BASICVALUE_ASCIIBITSTRING, myLineNoG);
        $$->valueType = BASICTYPE_UNKNOWN;
        $$->basicValue->a.asciiBitString = MT (AsnOcts);
        $$->basicValue->a.asciiBitString->octs = $1;
        $$->basicValue->a.asciiBitString->octetLen = strlen ($1);
    }
  | CharString
    {
        SetupValue (&$$, BASICVALUE_ASCIITEXT, myLineNoG);
        $$->valueType = BASICTYPE_UNKNOWN;
        $$->basicValue->a.asciiText = MT (AsnOcts);
        $$->basicValue->a.asciiText->octs = $1;
        $$->basicValue->a.asciiText->octetLen = strlen ($1);
    }
  | LEFTBRACE_SYM { LexBeginBraceBalContext(); } BRACEBAL_SYM
    {
        /*
         *  LEXICAL TIE IN!!
         * string returned by BRACEBAL_SYM has
         * the $1 '{' prepended and includes everything
         * upto and including '}' that balances $1
         */
        LexBeginInitialContext();
        SetupValue (&$$, BASICVALUE_VALUENOTATION, myLineNoG);
        $$->basicValue->a.valueNotation = MT (AsnOcts);
        $$->basicValue->a.valueNotation->octs = $3;
        $$->basicValue->a.valueNotation->octetLen = strlen ($3);
        $$->valueType = BASICTYPE_UNKNOWN;
    }
;

BooleanValue:
    TRUE_SYM
    {
        SetupValue (&$$, BASICVALUE_BOOLEAN, myLineNoG);
        $$->valueType = BASICTYPE_UNKNOWN;
        $$->basicValue->a.boolean =  TRUE;
    }
  | FALSE_SYM
    {
        SetupValue (&$$, BASICVALUE_BOOLEAN, myLineNoG);
        $$->valueType = BASICTYPE_UNKNOWN;
        $$->basicValue->a.boolean = FALSE;
    }
;


SpecialRealValue:
    PLUS_INFINITY_SYM
    {
        SetupValue (&$$, BASICVALUE_SPECIALREAL, myLineNoG);
        $$->valueType = BASICTYPE_UNKNOWN;
        $$->basicValue->a.specialReal =  PLUS_INFINITY_REAL;
    }
  | MINUS_INFINITY_SYM
    {
        SetupValue (&$$, BASICVALUE_SPECIALREAL, myLineNoG);
        $$->valueType = BASICTYPE_UNKNOWN;
        $$->basicValue->a.specialReal = MINUS_INFINITY_REAL;
    }
;



NullValue:
    NULL_SYM
    {
        /* create a NULL value  */
        SetupValue (&$$, BASICVALUE_NULL, myLineNoG);
        $$->valueType = BASICTYPE_UNKNOWN;
    }
;


NamedValue:
    Value
    {
        $$ = MT (NamedValue);
        $$->value = $1;
    }
  | identifier Value
    {
        $$ = MT (NamedValue);
        $$->value = $2;
        $$->fieldName = $1;
    }
;


ObjectIdentifierValue:
    LEFTBRACE_SYM ObjIdComponentList RIGHTBRACE_SYM
    {
        /*
         * example OID setup
         *
         * for { ccitt foo (1) bar bell (bunt) 2 }
         *
         * ccitt
         *   - arcnum is set to number from oid table (oid.c)
         * foo (1)
         *   - sets up a new value def foo defined as 1
         *   - makes oid valueref a value ref to foo (doesn't link it tho)
         * bar
         *   - makes oid valueref a value ref to bar (doesn't link it tho)
         * bell (bunt)
         *   - sets up a new value def bell defined as a val ref to bunt
         *   - makes oid valueref a value ref to bell (doesn't link it tho)
         * 2
         *  - arcnum is set to 2
         */

        $$ = $2;
    }
;


ObjIdComponentList:
    ObjIdComponentList ObjIdComponent
    {
        OID *o;
        /* append component */
        for (o = $1; o->next != NULL; o = o->next)
	    ;
        o->next = $2;
        $$ = $1;
    }
  | ObjIdComponent

;


ObjIdComponent:
    NumberForm
  | NameForm
    {
        Value *newVal;
        /*
         * if the arcName is a defined arc name like
         * ccitt or iso etc, fill in the arc number.
         * otherwise make a value ref to that named value
         */
        $$ = MT (OID);

        $$->arcNum = OidArcNameToNum ($1);
        if ($$->arcNum == NULL_OID_ARCNUM)
        {
            /* set up value ref to named value */
            SetupValue (&newVal, BASICVALUE_LOCALVALUEREF, myLineNoG);
            newVal->basicValue->a.localValueRef = MT (ValueRef);
            newVal->valueType = BASICTYPE_INTEGER;
            newVal->basicValue->a.localValueRef->valueName = $1;
            $$->valueRef = newVal;
        }
    }
  | NameAndNumberForm
;


NumberForm:
    number
    {
        $$ = MT (OID);
        $$->arcNum = $1;
    }
;

NameForm:
    identifier
;


NameAndNumberForm:
    identifier LEFTPAREN_SYM NumberForm RIGHTPAREN_SYM
    {
        Value *newVal;

        $$ = $3;

        /* shared refs to named numbers name */
        SetupValue (&newVal, BASICVALUE_INTEGER, myLineNoG);
        newVal->basicValue->a.integer = $$->arcNum;
        newVal->valueType = BASICTYPE_INTEGER;
        AddNewValueDef (oidElmtValDefsG, $1, newVal);

        SetupValue (&newVal, BASICVALUE_LOCALVALUEREF, myLineNoG);
        newVal->basicValue->a.localValueRef = MT (ValueRef);
        newVal->basicValue->a.localValueRef->valueName = $1;

        $$->valueRef = newVal;
    }
  | identifier LEFTPAREN_SYM DefinedValue RIGHTPAREN_SYM
    {
        Value *newVal;

        /* shared refs to named numbers name */
        $$ = MT (OID);
        $$->arcNum = NULL_OID_ARCNUM;

        AddNewValueDef (oidElmtValDefsG, $1, $3);

        SetupValue (&newVal, BASICVALUE_LOCALVALUEREF, myLineNoG);
        newVal->basicValue->a.localValueRef = MT (ValueRef);
        newVal->basicValue->a.localValueRef->valueName = $1;

        $$->valueRef = newVal;
    }

;



BinaryString:
    BSTRING_SYM
;

HexString:
    HSTRING_SYM
;

CharString:
    CSTRING_SYM
;

number:
    NUMBER_SYM
	{
	if ($1>0x7FFFFFFF) {
	    yyerror("Warning: number out of range");
	    $$ = 0x7FFFFFFF;
	}
	}
    | NUMBER_ERANGE
	{
	yyerror ("Warning: number out of range");
	$$ = 0x7FFFFFFF;
	/* modulePtrG->status = MOD_ERROR; */
	}
;

identifier:
    LCASEFIRST_IDENT_SYM
;

modulereference:
    UCASEFIRST_IDENT_SYM
;

typereference:
    UCASEFIRST_IDENT_SYM
;

empty:
;


/* Snacc attributes/extra type info
 *  - encapsulated in special comments
 */
SnaccAttributes:
    SnaccAttributeCommentList
  | empty {$$ = NULL;}
;

SnaccAttributeCommentList:
    SNACC_ATTRIBUTES
    {
        $$ = NEWLIST();
        APPEND ($1,$$);
    }
  | SnaccAttributeCommentList  SNACC_ATTRIBUTES
    {
        APPEND ($2,$1);
        $$ = $1;
    }
;

/*
 *  Macro Syntax definitions
 **************************/

DefinedMacroType:
    RosOperationMacroType
  | RosErrorMacroType
  | RosBindMacroType
  | RosUnbindMacroType
  | RosAseMacroType
  | RosAcMacroType
  | MtsasExtensionMacroType
  | MtsasExtensionsMacroType
  | MtsasExtensionAttributeMacroType
  | MtsasTokenMacroType
  | MtsasTokenDataMacroType
  | MtsasSecurityCategoryMacroType
  | AsnObjectMacroType
  | AsnPortMacroType
  | AsnRefineMacroType
  | AsnAbstractBindMacroType
  | AsnAbstractUnbindMacroType
  | AsnAbstractOperationMacroType
  | AsnAbstractErrorMacroType
  | AfAlgorithmMacroType
  | AfEncryptedMacroType
  | AfProtectedMacroType
  | AfSignatureMacroType
  | AfSignedMacroType
  | SnmpObjectTypeMacroType
;

DefinedMacroName:
   OPERATION_SYM          { $$ = "OPERATION"; }
 | ERROR_SYM              { $$ = "ERROR"; }
 | BIND_SYM               { $$ = "BIND"; }
 | UNBIND_SYM             { $$ = "UNBIND"; }
 | ASE_SYM                { $$ = "APPLICATION-SERVICE-ELEMENT"; }
 | AC_SYM                 { $$ = "APPLICATION-CONTEXT"; }
 | EXTENSION_SYM          { $$ = "EXTENSION"; }
 | EXTENSIONS_SYM         { $$ = "EXTENSIONS"; }
 | EXTENSIONATTRIBUTE_SYM { $$ = "EXTENSION-ATTRIBUTE"; }
 | TOKEN_SYM              { $$ = "TOKEN"; }
 | TOKENDATA_SYM          { $$ = "TOKEN-DATA"; }
 | SECURITYCATEGORY_SYM   { $$ = "SECURITY-CATEGORY"; }
 | OBJECT_SYM             { $$ = "OBJECT"; }
 | PORT_SYM               { $$ = "PORT"; }
 | REFINE_SYM             { $$ = "REFINE"; }
 | ABSTRACTBIND_SYM       { $$ = "ABSTRACT-BIND"; }
 | ABSTRACTUNBIND_SYM     { $$ = "ABSTRACT-UNBIND"; }
 | ABSTRACTOPERATION_SYM  { $$ = "ABSTRACT-OPERATION"; }
 | ABSTRACTERROR_SYM      { $$ = "ABSTRACT-ERROR"; }
 | ALGORITHM_SYM          { $$ = "ALGORITHM"; }
 | ENCRYPTED_SYM          { $$ = "ENCRYPTED"; }
 | SIGNED_SYM             { $$ = "SIGNED"; }
 | SIGNATURE_SYM          { $$ = "SIGNATURE"; }
 | PROTECTED_SYM          { $$ = "PROTECTED"; }
 | OBJECTTYPE_SYM         { $$ = "OBJECT-TYPE"; }
;


/*
 * Operation Macro (ROS)  added by MS 91/08/27
 */

RosOperationMacroType:
    OPERATION_SYM RosOperationMacroBody { $$ = $2; }
;

RosOperationMacroBody:
    RosOpArgument RosOpResult RosOpErrors RosOpLinkedOps
    {
        RosOperationMacroType *r;

        SetupMacroType (&$$, MACROTYPE_ROSOPERATION, myLineNoG);
        r = $$->basicType->a.macroType->a.rosOperation  =
            MT (RosOperationMacroType);
        r->arguments = $1;
        r->result    = $2;
        r->errors    = $3;
        r->linkedOps = $4;
    }
;


RosOpArgument:
    ARGUMENT_SYM NamedType { $$ = $2; }
  | empty { $$ = NULL; }
;

RosOpResult:
    RESULT_SYM RosOpResultType { $$ = $2; }
  | empty { $$ = NULL; }
;


RosOpResultType:
    NamedType
  | empty { $$ = NULL; }
;


RosOpErrors:
    ERRORS_SYM LEFTBRACE_SYM PossiblyEmptyTypeOrValueList RIGHTBRACE_SYM
    {
        $$ = $3;
    }
  | empty { $$ = NULL; }
;



RosOpLinkedOps:
    LINKED_SYM LEFTBRACE_SYM PossiblyEmptyTypeOrValueList RIGHTBRACE_SYM
    {
        $$ = $3;
    }
  | empty { $$ = NULL; }
;




/*
 * ROS ERROR macro - ms 91/08/27
 */


RosErrorMacroType:
    ERROR_SYM RosErrParameter
    {
        RosErrorMacroType *r;
        /*
         * defines error macro type
         */
        SetupMacroType (&$$, MACROTYPE_ROSERROR, myLineNoG);
        r = $$->basicType->a.macroType->a.rosError = MT (RosErrorMacroType);
        r->parameter = $2;
    }
;


RosErrParameter:
    PARAMETER_SYM NamedType { $$ = $2; }
  | empty { $$ = NULL; }
;


/*
 * ROS BIND macro - ms 91/09/13
 */

RosBindMacroType:
    BIND_SYM RosBindArgument RosBindResult RosBindError
    {
        RosBindMacroType *r;

        SetupMacroType (&$$, MACROTYPE_ROSBIND, myLineNoG);

        r = $$->basicType->a.macroType->a.rosBind = MT (RosBindMacroType);
        r->argument  = $2;
        r->result = $3;
        r->error  = $4;
    }
;

RosBindArgument:
    ARGUMENT_SYM NamedType { $$ = $2; }
  | empty { $$ = NULL; }
;


RosBindResult:
    RESULT_SYM NamedType { $$ = $2; }
  | empty { $$ = NULL; }
;


RosBindError:
    BINDERROR_SYM NamedType { $$ = $2; }
  | empty { $$ = NULL; }
;


/*
 * ROS UNBIND ms 91/09/13
 */

RosUnbindMacroType:
     UNBIND_SYM RosBindArgument RosBindResult RosUnbindError
    {
        RosBindMacroType *r;

        SetupMacroType (&$$, MACROTYPE_ROSUNBIND, myLineNoG);

        r = $$->basicType->a.macroType->a.rosUnbind = MT (RosBindMacroType);
        r->argument = $2;
        r->result = $3;
        r->error  = $4;
    }
;


RosUnbindError:
    UNBINDERROR_SYM NamedType { $$ = $2; }
  | empty { $$ = NULL; }
;


/*
 * ROS APPLICATION-SERVICE-ELEMENT macro ms 91/09/13
 */

RosAseMacroType:
    ASE_SYM RosAseSymmetricAse
    {
        RosAseMacroType *r;

        SetupMacroType (&$$, MACROTYPE_ROSASE, myLineNoG);
        r = $$->basicType->a.macroType->a.rosAse  = MT (RosAseMacroType);
        r->operations = $2;
    }
  | ASE_SYM RosAseConsumerInvokes RosAseSupplierInvokes
    {
        RosAseMacroType *r;

        SetupMacroType (&$$, MACROTYPE_ROSASE, myLineNoG);
        r = $$->basicType->a.macroType->a.rosAse  = MT (RosAseMacroType);
        r->consumerInvokes = $2;
        r->supplierInvokes = $3;
    }
;


RosAseSymmetricAse:
    OPERATIONS_SYM LEFTBRACE_SYM RosAseOperationList RIGHTBRACE_SYM
    {
        $$ = $3;
    }
;


RosAseConsumerInvokes:
    CONSUMERINVOKES_SYM LEFTBRACE_SYM RosAseOperationList RIGHTBRACE_SYM
    {
        $$ = $3;
    }
  | empty { $$ = NULL; }
;


RosAseSupplierInvokes:
    SUPPLIERINVOKES_SYM  LEFTBRACE_SYM RosAseOperationList RIGHTBRACE_SYM
    {
        $$ = $3;
    }
  | empty { $$ = NULL; }
;


RosAseOperationList:
    ValueList
;


/*
 * ROS APPLICATION-CONTEXT macro ms 91/09/13
 */

RosAcMacroType:
    AC_SYM
    RosAcNonRoElements
    BIND_SYM Type
    UNBIND_SYM Type
    RosAcRoElements
    RosAcAbstractSyntaxes
    {
        RosAcMacroType *r;

        SetupMacroType (&$$, MACROTYPE_ROSAC, myLineNoG);
        r = $$->basicType->a.macroType->a.rosAc = MT (RosAcMacroType);
        r->nonRoElements = $2;
        r->bindMacroType = $4;
        r->unbindMacroType = $6;
        r->remoteOperations = $7;
        r->operationsOf = rosAcSymmetricAsesG;
        r->initiatorConsumerOf = rosAcInitiatorConsumerOfG;
        r->responderConsumerOf = rosAcResponderConsumerOfG;
        r->abstractSyntaxes = $8;
    }
;


RosAcNonRoElements:
    ASES_SYM LEFTBRACE_SYM ValueList RIGHTBRACE_SYM
    {
        $$ = $3;
    }
;


RosAcRoElements:
    REMOTE_SYM OPERATIONS_SYM LEFTBRACE_SYM Value RIGHTBRACE_SYM
    RosAcSymmetricAses RosAcAsymmetricAses
    {
        $$ = $4;
    }
  | empty
    {
        $$ = NULL;
        rosAcSymmetricAsesG = NULL;
        rosAcInitiatorConsumerOfG = NULL;
        rosAcResponderConsumerOfG = NULL;
    }
;

RosAcSymmetricAses:
    OPERATIONS_SYM OF_SYM  LEFTBRACE_SYM  ValueList RIGHTBRACE_SYM
    {
        rosAcSymmetricAsesG = $4;
    }
  | empty { rosAcSymmetricAsesG = NULL; }
;

RosAcAsymmetricAses:
    RosAcInitiatorConsumerOf RosAcResponderConsumerOf
;

RosAcInitiatorConsumerOf:
    INITIATOR_SYM CONSUMER_SYM OF_SYM LEFTBRACE_SYM  ValueList RIGHTBRACE_SYM
    {
        rosAcInitiatorConsumerOfG = $5;
    }
  | empty { rosAcInitiatorConsumerOfG = NULL; }
;

RosAcResponderConsumerOf:
    RESPONDER_SYM CONSUMER_SYM OF_SYM LEFTBRACE_SYM  ValueList RIGHTBRACE_SYM
    {
        rosAcResponderConsumerOfG = $5;
    }
  | empty { rosAcResponderConsumerOfG = NULL; }
;

RosAcAbstractSyntaxes:
    ABSTRACTSYNTAXES_SYM LEFTBRACE_SYM  OidList RIGHTBRACE_SYM
    {
        $$ = $3;
    }
  | empty { $$ = NULL; }
;


OidList:
    ObjectIdentifierValue
    {
        $$ = NEWLIST();
        APPEND ($1,$$);
    }
  | OidList COMMA_SYM ObjectIdentifierValue
    {
        APPEND ($3, $1);
        $$ = $1;
    }
;


/*
 * MTSAbstractSvc EXTENSIONS macro
 */

MtsasExtensionsMacroType:
      EXTENSIONS_SYM CHOSEN_SYM FROM_SYM
      LEFTBRACE_SYM PossiblyEmptyValueList RIGHTBRACE_SYM
      {
          MtsasExtensionsMacroType *m;

          SetupMacroType (&$$, MACROTYPE_MTSASEXTENSIONS, myLineNoG);
          m = $$->basicType->a.macroType->a.mtsasExtensions =
              MT (MtsasExtensionsMacroType);
          m->extensions = $5;
      }
;


PossiblyEmptyValueList:
    ValueList
  | empty { $$ = NULL; }
;

ValueList:
     Value
     {
         $$ = NEWLIST();
         APPEND ($1, $$);
     }
  | ValueList COMMA_SYM Value
     {
         APPEND ($3,$1);
         $$ = $1;
     }
;

PossiblyEmptyTypeOrValueList:
    TypeOrValueList
  | empty { $$ = NULL; }
;

TypeOrValueList:
    TypeOrValue
     {
         $$ = NEWLIST();
         APPEND ($1, $$);
     }
  | TypeOrValueList COMMA_SYM TypeOrValue
     {
         APPEND ($3,$1);
         $$ = $1;
     }
;

TypeOrValue:
    Type
    {
         $$ = MT (TypeOrValue);
         $$->choiceId = TYPEORVALUE_TYPE;
         $$->a.type = $1;
     }
  | Value
    {
         $$ = MT (TypeOrValue);
         $$->choiceId = TYPEORVALUE_VALUE;
         $$->a.value = $1;
     }
;

/*
 *  MTSAbstractSvc EXTENSION macro
 */

MtsasExtensionMacroType:
    EXTENSION_SYM NamedType MtsasExtDefaultVal MtsasExtCritical
    {
        MtsasExtensionMacroType *m;

        SetupMacroType (&$$, MACROTYPE_MTSASEXTENSION, myLineNoG);
        m = $$->basicType->a.macroType->a.mtsasExtension =
            MT (MtsasExtensionMacroType);
        m->elmtType = $2;
        m->defaultValue = $3;
        m->criticalForSubmission = mtsasCriticalForSubmissionG;
        m->criticalForTransfer = mtsasCriticalForTransferG;
        m->criticalForDelivery = mtsasCriticalForDeliveryG;

        mtsasCriticalForSubmissionG = NULL;  /* set up for next parse */
        mtsasCriticalForTransferG = NULL;
        mtsasCriticalForDeliveryG = NULL;
    }
  | EXTENSION_SYM
    {
        SetupMacroType (&$$, MACROTYPE_MTSASEXTENSION, myLineNoG);
        $$->basicType->a.macroType->a.mtsasExtension =
            MT (MtsasExtensionMacroType);
        /*
         * all fields are NULL in the MtsasExtensionsMacroType
         * for this production
         */
    }
;

MtsasExtDefaultVal:
    DEFAULT_SYM Value { $$ = $2; }
  | empty { $$ = NULL; }
;

MtsasExtCritical:
     CRITICAL_SYM FOR_SYM MtsasExtCriticalityList
   | empty
;


MtsasExtCriticalityList:
    MtsasExtCriticality
  | MtsasExtCriticalityList COMMA_SYM MtsasExtCriticality
;

MtsasExtCriticality:
    SUBMISSION_SYM
    {
        mtsasCriticalForSubmissionG = MT (AsnBool);
        *mtsasCriticalForSubmissionG = TRUE;
    }
  | TRANSFER_SYM
    {
        mtsasCriticalForTransferG = MT (AsnBool);
        *mtsasCriticalForTransferG = TRUE;
    }
  | DELIVERY_SYM
    {
        mtsasCriticalForDeliveryG = MT (AsnBool);
        *mtsasCriticalForDeliveryG = TRUE;
    }
;



/*
 * MTSAbstractSvc X.411  EXTENSION-ATTRIBUTE macro
 */

MtsasExtensionAttributeMacroType:
    EXTENSIONATTRIBUTE_SYM
    {
        MtsasExtensionAttributeMacroType *m;

        SetupMacroType (&$$, MACROTYPE_MTSASEXTENSIONATTRIBUTE, myLineNoG);
        m = $$->basicType->a.macroType->a.mtsasExtensionAttribute =
            MT (MtsasExtensionAttributeMacroType);
        m->type = NULL;
    }
  | EXTENSIONATTRIBUTE_SYM Type
    {
        MtsasExtensionAttributeMacroType *m;

        SetupMacroType (&$$, MACROTYPE_MTSASEXTENSIONATTRIBUTE, myLineNoG);
        m = $$->basicType->a.macroType->a.mtsasExtensionAttribute =
            MT (MtsasExtensionAttributeMacroType);
        m->type = $2;
    }
;


/*
 * X.411 MTSAbstractSvc TOKEN macro
 */
MtsasTokenMacroType:
    TOKEN_SYM
    {
        MtsasTokenMacroType *m;

        SetupMacroType (&$$, MACROTYPE_MTSASTOKEN, myLineNoG);
        m = $$->basicType->a.macroType->a.mtsasToken = MT (MtsasTokenMacroType);
        m->type = NULL;
    }
  | TOKEN_SYM Type
    {
        MtsasTokenMacroType *m;

        SetupMacroType (&$$, MACROTYPE_MTSASTOKEN, myLineNoG);
        m = $$->basicType->a.macroType->a.mtsasToken = MT (MtsasTokenMacroType);
        m->type = $2;
    }
;


/*
 * X.411 MTSAS TOKEN-DATA macro type
 */
MtsasTokenDataMacroType:
    TOKENDATA_SYM
    {
        MtsasTokenDataMacroType *m;

        SetupMacroType (&$$, MACROTYPE_MTSASTOKENDATA, myLineNoG);
        m = $$->basicType->a.macroType->a.mtsasTokenData =
            MT (MtsasTokenDataMacroType);
        m->type = NULL;
    }
  | TOKENDATA_SYM Type
    {
        MtsasTokenDataMacroType *m;

        SetupMacroType (&$$, MACROTYPE_MTSASTOKENDATA, myLineNoG);
        m = $$->basicType->a.macroType->a.mtsasTokenData =
            MT (MtsasTokenDataMacroType);
        m->type = $2;
    }
;


/*
 * X.411 MTSAS SECURITY-CATEGORY
 */
MtsasSecurityCategoryMacroType:
    SECURITYCATEGORY_SYM
    {
        MtsasSecurityCategoryMacroType *m;

        SetupMacroType (&$$, MACROTYPE_MTSASSECURITYCATEGORY, myLineNoG);
        m = $$->basicType->a.macroType->a.mtsasSecurityCategory =
            MT (MtsasSecurityCategoryMacroType);
        m->type = NULL;
    }
  | SECURITYCATEGORY_SYM Type
    {
        MtsasSecurityCategoryMacroType *m;

        SetupMacroType (&$$, MACROTYPE_MTSASSECURITYCATEGORY, myLineNoG);
        m = $$->basicType->a.macroType->a.mtsasSecurityCategory =
            MT (MtsasSecurityCategoryMacroType);
        m->type = $2;
    }
;


/*
 * X.407 Abstract Service Notation Macro Type productions
 * MS 91/09/14
 */


/*
 * OBJECT Macro X.407
 */
AsnObjectMacroType:
    OBJECT_SYM AsnPorts
    {
        AsnObjectMacroType *a;
        SetupMacroType (&$$, MACROTYPE_ASNOBJECT, myLineNoG);
        a = $$->basicType->a.macroType->a.asnObject = MT (AsnObjectMacroType);
        a->ports = $2;
    }
;

AsnPorts:
    PORTS_SYM LEFTBRACE_SYM AsnPortList RIGHTBRACE_SYM
    {
        $$ = $3;
    }
  | empty { $$ = NULL; }
;

AsnPortList:
    AsnPort
    {
        $$ = NEWLIST();
        APPEND ($1, $$);
    }
  | AsnPortList COMMA_SYM AsnPort
    {
        APPEND ($3, $1);
        $$ = $1;
    }
;

AsnPort:
    Value AsnPortType
    {
        $$ = MT (AsnPort);
        $$->portValue = $1;
        $$->portType = $2;
    }
;

AsnPortType:
    BOXC_SYM
    {
        /* [C] consumer */
        $$ = CONSUMER_PORT;
    }
  | BOXS_SYM
    {
        /* [S] supplier */
        $$ = SUPPLIER_PORT;
    }
  | empty
    {
       /* symmetric */
        $$ = SYMMETRIC_PORT;
    }
;



/*
 * PORT Macro X.407
 */
AsnPortMacroType:
    PORT_SYM AsnOperations
    {
        AsnPortMacroType *a;

        SetupMacroType (&$$, MACROTYPE_ASNPORT, myLineNoG);
        a = $$->basicType->a.macroType->a.asnPort = MT (AsnPortMacroType);
        a->abstractOps = $2;
        a->consumerInvokes = asnConsumerG;
        a->supplierInvokes = asnSupplierG;
    }
  | PORT_SYM
    {
        SetupMacroType (&$$, MACROTYPE_ASNPORT, myLineNoG);
        $$->basicType->a.macroType->a.asnPort = MT (AsnPortMacroType);
    }
;


AsnOperations:
    ABSTRACTOPS_SYM LEFTBRACE_SYM TypeOrValueList RIGHTBRACE_SYM
    {
        $$ = $3;
    }
  | AsnConsumer
    {
        $$ = NULL;
        asnConsumerG = $1;
        asnSupplierG = NULL;
    }
  | AsnSupplier
    {
        $$ = NULL;
        asnConsumerG = $1;
        asnSupplierG = NULL;
    }
  | AsnConsumer AsnSupplier
    {
        $$ = NULL;
        asnConsumerG = $1;
        asnSupplierG = NULL;
    }
  | AsnSupplier AsnConsumer
    {
        $$ = NULL;
        asnConsumerG = $1;
        asnSupplierG = NULL;
    }
;

AsnConsumer:
    CONSUMERINVOKES_SYM LEFTBRACE_SYM TypeOrValueList RIGHTBRACE_SYM
    {
        $$ = $3;
    }
;

AsnSupplier:
    SUPPLIERINVOKES_SYM LEFTBRACE_SYM TypeOrValueList RIGHTBRACE_SYM
    {
        $$ = $3;
    }

;




/*
 * REFINE Macro X.407
 *
 * just parse it - don't keep any info at the moment
 */
AsnRefineMacroType:
    REFINE_SYM AsnObject AS_SYM AsnComponentList
    {
        SetupType (&$$, BASICTYPE_UNKNOWN, myLineNoG);
    }
;

AsnComponentList:
    AsnComponent
  | AsnComponentList COMMA_SYM AsnComponent
;

AsnComponent:
    AsnObjectSpec AsnPortSpecList
;

AsnObjectSpec:
    AsnObject
  | AsnObject RECURRING_SYM
;

AsnPortSpecList:
    AsnPortSpec
  | AsnPortSpecList COMMA_SYM AsnPortSpec
;

AsnPortSpec:
    Value AsnPortType AsnPortStatus
    {
       $$ = 0; /* just to quiet yacc warning */
    }
;

AsnPortStatus:
    VISIBLE_SYM
  | PAIRED_SYM WITH_SYM AsnObjectList
;


AsnObjectList:
    AsnObject
  | AsnObjectList COMMA_SYM AsnObject
;

AsnObject:
    Value
    {
        $$ = 0; /* just to quiet yacc warning */
    }
;




/*
 * ABSTRACT-BIND Macro X.407
 */
AsnAbstractBindMacroType:
    ABSTRACTBIND_SYM AsnAbstractBindPorts
    {
        AsnAbstractBindMacroType *a;

        SetupMacroType (&$$, MACROTYPE_ASNABSTRACTBIND, myLineNoG);
        a = $$->basicType->a.macroType->a.asnAbstractBind =
            MT (AsnAbstractBindMacroType);
        a->ports = $2;
    }
  | ABSTRACTBIND_SYM AsnAbstractBindPorts Type
    {
        AsnAbstractBindMacroType *a;

        SetupMacroType (&$$, MACROTYPE_ASNABSTRACTBIND, myLineNoG);
        a = $$->basicType->a.macroType->a.asnAbstractBind =
            MT (AsnAbstractBindMacroType);
        a->ports = $2;
        a->type = $3;
    }
;

AsnAbstractBindPorts:
    TO_SYM LEFTBRACE_SYM AsnPortList RIGHTBRACE_SYM
    {
        $$ = $3;
    }
  | empty { $$ = NULL; }
;




/*
 * ABSTRACT-UNBIND Macro X.407
 */
AsnAbstractUnbindMacroType:
    ABSTRACTUNBIND_SYM AsnAbstractUnbindPorts
    {
        AsnAbstractBindMacroType *a;

        SetupMacroType (&$$, MACROTYPE_ASNABSTRACTUNBIND, myLineNoG);
        a = $$->basicType->a.macroType->a.asnAbstractUnbind =
            MT (AsnAbstractBindMacroType);

        a->ports = $2;
    }
  | ABSTRACTUNBIND_SYM AsnAbstractUnbindPorts Type
    {
        AsnAbstractBindMacroType *a;

        SetupMacroType (&$$, MACROTYPE_ASNABSTRACTUNBIND, myLineNoG);
        a = $$->basicType->a.macroType->a.asnAbstractUnbind =
            MT (AsnAbstractBindMacroType);

        a->ports = $2;
        a->type = $3;
    }
;

AsnAbstractUnbindPorts:
    FROM_SYM LEFTBRACE_SYM AsnPortList RIGHTBRACE_SYM
    {
        $$ = $3;
    }
  | empty { $$ = NULL; }
;



/*
 * ABSTRACT-OPERATION Macro X.407 (same as ROS Operation)
 */
AsnAbstractOperationMacroType:
    ABSTRACTOPERATION_SYM RosOperationMacroBody
    {
       $$ = $2;
       $2->basicType->a.macroType->choiceId = MACROTYPE_ASNABSTRACTOPERATION;
    }
;


/*
 * ABSTRACT-ERROR Macro X.407 (same as ROS Error)
 */
AsnAbstractErrorMacroType:
    ABSTRACTERROR_SYM RosErrParameter
    {
        SetupMacroType (&$$, MACROTYPE_ASNABSTRACTERROR, myLineNoG);
        $$->basicType->a.macroType->a.asnAbstractError = MT (RosErrorMacroType);
        $$->basicType->a.macroType->a.asnAbstractError->parameter = $2;
    }
;


/*
 * X.509 Authentication Framework  ALGORITHM macro type
 */
AfAlgorithmMacroType:
    ALGORITHM_SYM PARAMETER_SYM Type
    {
        SetupMacroType (&$$, MACROTYPE_AFALGORITHM, myLineNoG);
        $$->basicType->a.macroType->a.afAlgorithm = $3;
    }
;

/*
 * X.509 Authentication Framework  ENCRYPTED macro type
 */
AfEncryptedMacroType:
    ENCRYPTED_SYM Type
    {
        SetupMacroType (&$$, MACROTYPE_AFENCRYPTED, myLineNoG);
        $$->basicType->a.macroType->a.afEncrypted = $2;
    }
;


/*
 * X.509 Authentication Framework SIGNED macro type
 */
AfSignedMacroType:
    SIGNED_SYM Type
    {
        SetupMacroType (&$$, MACROTYPE_AFSIGNED, myLineNoG);
        $$->basicType->a.macroType->a.afSigned = $2;
    }
;

/*
 * X.509 Authentication Framework SIGNATURE macro type
 */
AfSignatureMacroType:
    SIGNATURE_SYM Type
    {
        SetupMacroType (&$$, MACROTYPE_AFSIGNATURE, myLineNoG);
        $$->basicType->a.macroType->a.afSignature = $2;
    }
;



/*
 * X.509 Authentication Framework PROTECTED macro type
 *  (same as SIGNATURE except for key word)
 */
AfProtectedMacroType:
    PROTECTED_SYM Type
    {
        SetupMacroType (&$$, MACROTYPE_AFPROTECTED, myLineNoG);
        $$->basicType->a.macroType->a.afProtected = $2;
    }
;



SnmpObjectTypeMacroType:
    OBJECTTYPE_SYM
    SYNTAX_SYM Type
    ACCESS_SYM SnmpAccess
    STATUS_SYM SnmpStatus
    SnmpDescrPart
    SnmpReferPart
    SnmpIndexPart
    SnmpDefValPart
    {
        SnmpObjectTypeMacroType *s;

        SetupMacroType (&$$, MACROTYPE_SNMPOBJECTTYPE, myLineNoG);
        s = $$->basicType->a.macroType->a.snmpObjectType =
            MT (SnmpObjectTypeMacroType);

        s->syntax = $3;
        s->access = $5;
        s->status = $7;
        s->description = $8;
        s->reference = $9;
        s->index = $10;
        s->defVal = $11;
    }
;

SnmpAccess:
   identifier
   {
        if (strcmp ($1, "read-only") == 0)
            $$ = SNMP_READ_ONLY;
        else if (strcmp ($1, "read-write") == 0)
            $$ = SNMP_READ_WRITE;
        else if (strcmp ($1, "write-only") == 0)
            $$ = SNMP_WRITE_ONLY;
        else if (strcmp ($1, "not-accessible") == 0)
            $$ = SNMP_NOT_ACCESSIBLE;
        else
        {
            yyerror ("ACCESS field of SNMP OBJECT-TYPE MACRO can only be one of \"read-write\", \"write-only\" or \"not-accessible\"");
            $$ = -1;
            modulePtrG->status = MOD_ERROR;
        }
        Free ($1);
   }
;


SnmpStatus:
   identifier
   {
        if (strcmp ($1, "mandatory") == 0)
            $$ = SNMP_MANDATORY;
        else if (strcmp ($1, "optional") == 0)
            $$ = SNMP_OPTIONAL;
        else if (strcmp ($1, "obsolete") == 0)
            $$ = SNMP_OBSOLETE;
        else if (strcmp ($1, "deprecated") == 0)
            $$ = SNMP_DEPRECATED;
        else
        {
            yyerror ("STATUS field of SNMP OBJECT-TYPE MACRO can only be one of \"optional\", \"obsolete\" or \"deprecated\"");
            $$ = -1;
            modulePtrG->status = MOD_ERROR;
        }
        Free ($1);
   }
;

SnmpDescrPart:
   DESCRIPTION_SYM Value { $$ = $2; }
  | { $$ = NULL; }
;

SnmpReferPart:
   REFERENCE_SYM Value { $$ = $2; }
 | { $$ = NULL; }
;

SnmpIndexPart:
   INDEX_SYM LEFTBRACE_SYM TypeOrValueList RIGHTBRACE_SYM
   {
       $$  = $3;
   }
  | { $$ = NULL; }
;

SnmpDefValPart:
   DEFVAL_SYM LEFTBRACE_SYM Value RIGHTBRACE_SYM
   {
       $$  = $3;
   }
  | { $$ = NULL; }
;

%%

yyerror (s)
char*s;
{
	fprintf (stderr,"file \"%s\", line %d: %s at symbol \"%s\"\n\n", modulePtrG->asn1SrcFileName, myLineNoG, s, yytext);
}


/*
 * given a Module*, the file name associated witht the open
 * FILE *fPtr, InitAsn1Parser sets up the yacc/lex parser
 * to parse an ASN.1 module read from fPtr and write the
 * parse results into the given Module *mod.
 */
int
InitAsn1Parser PARAMS ((mod, fileName, fPtr),
    Module	*mod _AND_
    char	*fileName _AND_
    FILE	*fPtr)
{
    yyin = fPtr;

    /*
     * reset lexical analyzer input file ptr
     * (only do this on succesive calls ow yyrestart seg faults
     */
#ifdef FLEX_IN_USE
    if (!firstTimeThroughG)
        yyrestart (fPtr);

    firstTimeThroughG = FALSE;
#endif


    /*
     * init modulePtr
     */
    memzero (mod, sizeof (Module));
    modulePtrG = mod;
    mod->asn1SrcFileName = fileName;
    mod->status = MOD_NOT_LINKED;
    mod->hasAnys = FALSE;

    /* init lists to empty */
    mod->typeDefs = AsnListNew (sizeof (void*));
    mod->valueDefs = AsnListNew (sizeof (void*));

    /*
     * init export list stuff
     */
    exportListG = NULL;
    exportsParsedG = FALSE;

    /*
     * reset line number to 1
     */
    myLineNoG = 1;

    /*
     * reset error count
     */
    parseErrCountG = 0;

   /*
    * set up list to hold values defined in parsed oids
    */
    oidElmtValDefsG = AsnListNew (sizeof (void *));

    smallErrG = 0;

    return 0;

}  /* InitAsn1Parser */


/*
 * puts the applicatin tag code, tagCode, and line number it was
 * parsed at into the applTagsG list.  If the APPLICATION tag code
 * is already in the applTagsG list then an error is printed.
 * and the smallErrG flag set to prevent code production.
 */
void
PushApplTag PARAMS ((tagCode, lineNo),
    unsigned long int tagCode _AND_
    unsigned long int lineNo)
{
    ApplTag *l;
    ApplTag *new;
    int wasDefined = 0;

    /* make sure not already in list */
    for (l = applTagsG; l != NULL; l = l->next)
    {
        if (l->tagCode == tagCode)
        {
            PrintErrLoc (modulePtrG->asn1SrcFileName, lineNo);
            fprintf (stderr,"ERROR - APPLICATION tags can be used only once per ASN.1 module.  The tag \"[APPLICATION %d]\" was previously used on line %d.\n", tagCode, l->lineNo);
            wasDefined = 1;
            smallErrG = 1;
        }
    }
    if (!wasDefined)
    {
        new = MT (ApplTag);
        new->lineNo = lineNo;
        new->tagCode = tagCode;
        new->next = applTagsG;
        applTagsG = new;
    }
}  /* PushApplTag */


/*
 * Empties the applTagsG list.  Usually done between modules.
 */
void
FreeApplTags()
{
    ApplTag *l;
    ApplTag *lTmp;

    for (l = applTagsG; l != NULL; )
    {
        lTmp = l->next;
        Free (l);
        l = lTmp;
    }
    applTagsG = NULL;
}  /* FreeApplTags */
