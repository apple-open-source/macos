/**
 * perl-libxml-sax.c
 * $Id: perl-libxml-sax.c,v 1.1.1.2 2007/10/10 23:04:14 ahuda Exp $
 */

#ifdef __cplusplus
extern "C" {
#endif
#define PERL_NO_GET_CONTEXT     /* we want efficiency */


#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"
#include "ppport.h"

#include <stdlib.h>
#include <libxml/xmlmemory.h>
#include <libxml/parser.h>
#include <libxml/parserInternals.h>
#include <libxml/tree.h>
#include <libxml/entities.h>
#include <libxml/xmlerror.h>

#ifdef __cplusplus
}
#endif

#define NSDELIM ':'
/* #define NSDEFAULTURI "http://www.w3.org/XML/1998/namespace" */ 
#define NSDEFAULTURI "http://www.w3.org/2000/xmlns/"
typedef struct {
    SV * parser;
    xmlNodePtr ns_stack;
    xmlSAXLocator * locator;
    xmlDocPtr ns_stack_root;
    SV * handler;
    SV * saved_error;
} PmmSAXVector;

typedef PmmSAXVector* PmmSAXVectorPtr;

static U32 PrefixHash; /* pre-computed */
static U32 NsURIHash;
static U32 NameHash;
static U32 LocalNameHash;
static U32 AttributesHash;
static U32 ValueHash;
static U32 DataHash;
static U32 TargetHash;
static U32 VersionHash;
static U32 EncodingHash;

/* helper function C2Sv is ment to work faster than the perl-libxml-mm
   version. this shortcut is usefull, because SAX handles only UTF8
   strings, so there is no conversion logic required.
*/
SV*
_C2Sv( const xmlChar *string, const xmlChar *dummy )
{

    dTHX;
    SV *retval = &PL_sv_undef;
    STRLEN len;

    if ( string != NULL ) {
        len = xmlStrlen( string );
        retval = NEWSV(0, len+1); 
        sv_setpvn(retval, (const char *)string, len );
#ifdef HAVE_UTF8
        SvUTF8_on( retval );
#endif
    }

    return retval;
}

SV*
_C2Sv_len( const xmlChar *string, int len )
{

    dTHX;
    SV *retval = &PL_sv_undef;

    if ( string != NULL ) {
        retval = NEWSV(0, len+1); 
        sv_setpvn(retval, (const char *)string, (STRLEN) len );
#ifdef HAVE_UTF8
        SvUTF8_on( retval );
#endif
    }

    return retval;
}


void
PmmSAXInitialize(pTHX)
{
    PERL_HASH(PrefixHash,     "Prefix",        6);
    PERL_HASH(NsURIHash,      "NamespaceURI", 12);
    PERL_HASH(NameHash,       "Name",          4);
    PERL_HASH(LocalNameHash,  "LocalName",     9);
    PERL_HASH(AttributesHash, "Attributes",   10);
    PERL_HASH(ValueHash,      "Value",         5);
    PERL_HASH(DataHash,       "Data",          4);
    PERL_HASH(TargetHash,     "Target",        6);
    PERL_HASH(VersionHash,    "Version",       7);
    PERL_HASH(EncodingHash,   "Encoding",      8);
}

xmlSAXHandlerPtr PSaxGetHandler();


void
PmmSAXInitContext( xmlParserCtxtPtr ctxt, SV * parser, SV * saved_error )
{
    PmmSAXVectorPtr vec = NULL;
    SV ** th;
    dTHX;

    vec = (PmmSAXVector*) xmlMalloc( sizeof(PmmSAXVector) );

    vec->ns_stack_root = xmlNewDoc(NULL);
    vec->ns_stack      = xmlNewDocNode(vec->ns_stack_root,
                                       NULL,
                                       (const xmlChar*)"stack",
                                       NULL );

    xmlAddChild((xmlNodePtr)vec->ns_stack_root, vec->ns_stack);

    vec->locator = NULL;

    vec->saved_error = saved_error;

    vec->parser  = SvREFCNT_inc( parser );
    th = hv_fetch( (HV*)SvRV(parser), "HANDLER", 7, 0 );
    if ( th != NULL && SvTRUE(*th) ) {
        vec->handler = SvREFCNT_inc(*th)  ;
    }
    else {
        vec->handler = NULL  ;
    }

    if ( ctxt->sax ) {
        xmlFree( ctxt->sax );
    }
    ctxt->sax = PSaxGetHandler();

    ctxt->_private = (void*)vec;
}

void 
PmmSAXCloseContext( xmlParserCtxtPtr ctxt )
{
    PmmSAXVector * vec = (PmmSAXVectorPtr) ctxt->_private;
    dTHX;

    if ( vec->handler != NULL ) {
        SvREFCNT_dec( vec->handler );
        vec->handler = NULL;
    }

    xmlFree( ctxt->sax );
    ctxt->sax = NULL;

    SvREFCNT_dec( vec->parser );
    vec->parser = NULL;

    xmlFreeDoc( vec->ns_stack_root );
    vec->ns_stack_root = NULL;
    xmlFree( vec );
    ctxt->_private = NULL;
}


xmlNsPtr
PmmGetNsMapping( xmlNodePtr ns_stack, const xmlChar * prefix )
{
    if ( ns_stack != NULL ) {
        return xmlSearchNs( ns_stack->doc, ns_stack, prefix );
    }
    
    return NULL;
}


void
PSaxStartPrefix( PmmSAXVectorPtr sax, const xmlChar * prefix,
                 const xmlChar * uri, SV * handler )
{
    dTHX;
    HV * param;
    SV * rv;

    dSP;

    ENTER;
    SAVETMPS;

    param = newHV();

    hv_store(param, "NamespaceURI", 12,
             _C2Sv(uri, NULL), NsURIHash);

    if ( prefix != NULL ) {
        hv_store(param, "Prefix", 6,
                 _C2Sv(prefix, NULL), PrefixHash);
    }
    else {
        hv_store(param, "Prefix", 6,
                 _C2Sv((const xmlChar*)"", NULL), PrefixHash);
    }

    PUSHMARK(SP) ;
    XPUSHs(handler);

    rv = newRV_noinc((SV*)param);

    XPUSHs(rv);
    PUTBACK;

    call_method( "start_prefix_mapping", G_SCALAR | G_EVAL | G_DISCARD );
    sv_2mortal(rv);
    if (SvTRUE(ERRSV)) {
        STRLEN n_a;
        croak(SvPV(ERRSV, n_a));
    }
    FREETMPS ;
    LEAVE ;
}

void
PSaxEndPrefix( PmmSAXVectorPtr sax, const xmlChar * prefix,
               const xmlChar * uri, SV * handler )
{
    dTHX;
    HV * param;
    SV * rv;

    dSP;

    ENTER;
    SAVETMPS;
    param = newHV();
    hv_store(param, "NamespaceURI", 12,
             _C2Sv(uri, NULL), NsURIHash);

    if ( prefix != NULL ) {
        hv_store(param, "Prefix", 6,
                 _C2Sv(prefix, NULL), PrefixHash);
    }
    else {
        hv_store(param, "Prefix", 6,
                 _C2Sv((const xmlChar *)"", NULL), PrefixHash);
    }

    PUSHMARK(SP) ;
    XPUSHs(handler);


    rv = newRV_noinc((SV*)param);

    XPUSHs(rv);
    PUTBACK;

    call_method( "end_prefix_mapping", G_SCALAR | G_EVAL | G_DISCARD );
    sv_2mortal(rv);
    if (SvTRUE(ERRSV)) {
        STRLEN n_a;
        croak(SvPV(ERRSV, n_a));
    }
    
    FREETMPS ;
    LEAVE ;
}

void 
PmmExtendNsStack( PmmSAXVectorPtr sax , const xmlChar * name) {
    xmlNodePtr newNS = NULL;
    xmlChar * localname = NULL;
    xmlChar * prefix = NULL;
    
    localname = xmlSplitQName( NULL, name, &prefix );
    if ( prefix != NULL ) {
        /* check if we can find a namespace with that prefix... */
        xmlNsPtr ns = xmlSearchNs( sax->ns_stack->doc, sax->ns_stack, prefix );

        if ( ns != NULL ) {
            newNS = xmlNewDocNode( sax->ns_stack_root, ns, localname, NULL );
        }
        else {
            newNS = xmlNewDocNode( sax->ns_stack_root, NULL, name, NULL );
        }
    }
    else {
        newNS = xmlNewDocNode( sax->ns_stack_root, NULL, name, NULL );
    }

    if ( newNS != NULL ) {
        xmlAddChild(sax->ns_stack, newNS);
        sax->ns_stack = newNS;
    }

    if ( localname != NULL ) {
        xmlFree( localname ) ;
    }
    if ( prefix != NULL ) {
        xmlFree( prefix );
    }
}

void
PmmNarrowNsStack( PmmSAXVectorPtr sax, SV *handler )
{
    xmlNodePtr parent = sax->ns_stack->parent;
    xmlNsPtr list = sax->ns_stack->nsDef;

    while ( list ) {
        if ( !xmlStrEqual(list->prefix, (const xmlChar*)"xml") ) {
            PSaxEndPrefix( sax, list->prefix, list->href, handler );
        }
        list = list->next;        
    }
    xmlUnlinkNode(sax->ns_stack);
    xmlFreeNode(sax->ns_stack);
    sax->ns_stack = parent;
}

void
PmmAddNamespace( PmmSAXVectorPtr sax, const xmlChar * name,
                 const xmlChar * href, SV *handler)
{
    xmlNsPtr ns         = NULL;
    xmlChar * prefix    = NULL;
    xmlChar * localname = NULL;


    if ( sax->ns_stack == NULL ) {
        return;
    }

    ns = xmlNewNs( sax->ns_stack, href, name );         

    if ( sax->ns_stack->ns == NULL ) {
        localname = xmlSplitQName( NULL, sax->ns_stack->name, &prefix );

        if ( name != NULL ) {
            if ( xmlStrEqual( prefix , name ) ) {
                xmlChar * oname = (xmlChar*)(sax->ns_stack->name);
                sax->ns_stack->ns = ns;
                xmlFree( oname );
                sax->ns_stack->name = (const xmlChar*) xmlStrdup( localname );
            }
        }
        else if ( prefix == NULL ) {
            sax->ns_stack->ns = ns;
        }
    }

    if ( prefix ) {
        xmlFree( prefix );
    }
    if ( localname ) {
        xmlFree( localname );
    }

    PSaxStartPrefix( sax, name, href, handler );
}

HV *
PmmGenElementSV( pTHX_ PmmSAXVectorPtr sax, const xmlChar * name )
{
    HV * retval = newHV();
    xmlChar * localname = NULL;
    xmlChar * prefix    = NULL;

    xmlNsPtr ns = NULL;

    if ( name != NULL && xmlStrlen( name )  ) {
        hv_store(retval, "Name", 4,
                 _C2Sv(name, NULL), NameHash);

        localname = xmlSplitQName(NULL, name, &prefix);
        if (localname != NULL) xmlFree(localname);
        ns = PmmGetNsMapping( sax->ns_stack, prefix );
        if (prefix != NULL) xmlFree(prefix);

        if ( ns != NULL ) {
            hv_store(retval, "NamespaceURI", 12,
                     _C2Sv(ns->href, NULL), NsURIHash);
            if ( ns->prefix ) {
                hv_store(retval, "Prefix", 6,
                         _C2Sv(ns->prefix, NULL), PrefixHash);
            }
            else {
                hv_store(retval, "Prefix", 6,
                         _C2Sv((const xmlChar *)"",NULL), PrefixHash);
            }

            hv_store(retval, "LocalName", 9,
                     _C2Sv(sax->ns_stack->name, NULL), LocalNameHash);
        }
        else {
            hv_store(retval, "NamespaceURI", 12,
                     _C2Sv((const xmlChar *)"",NULL), NsURIHash);
            hv_store(retval, "Prefix", 6,
                     _C2Sv((const xmlChar *)"",NULL), PrefixHash);
            hv_store(retval, "LocalName", 9,
                     _C2Sv(name, NULL), LocalNameHash);
        }
    }

    return retval;
}

xmlChar *
PmmGenNsName( const xmlChar * name, const xmlChar * nsURI )
{
    int namelen = 0;
    int urilen = 0;
    xmlChar * retval = NULL;

    if ( name == NULL ) {
        return NULL;
    }
    namelen = xmlStrlen( name );

    retval =xmlStrncat( retval, (const xmlChar *)"{", 1 );
    if ( nsURI != NULL ) {
        urilen = xmlStrlen( nsURI );
        retval =xmlStrncat( retval, nsURI, urilen );
    } 
    retval = xmlStrncat( retval, (const xmlChar *)"}", 1 );
    retval = xmlStrncat( retval, name, namelen );
    return retval;
}

HV *
PmmGenAttributeHashSV( pTHX_ PmmSAXVectorPtr sax,
                       const xmlChar **attr, SV * handler )
{
    HV * retval     = NULL;
    HV * atV        = NULL;
    xmlNsPtr ns     = NULL;

    U32 atnameHash = 0;
    int len = 0;

    const xmlChar * nsURI = NULL;
    const xmlChar **ta    = attr;
    const xmlChar * name  = NULL;
    const xmlChar * value = NULL;

    xmlChar * keyname     = NULL;
    xmlChar * localname   = NULL;
    xmlChar * prefix      = NULL;

    retval = newHV();

    if ( ta != NULL ) {
        while ( *ta != NULL ) {
            atV = newHV();
            name = *ta;  ta++;
            value = *ta; ta++;

            if ( name != NULL && xmlStrlen( name ) ) {
                localname = xmlSplitQName(NULL, name, &prefix);

                hv_store(atV, "Name", 4,
                         _C2Sv(name, NULL), NameHash);
                if ( value != NULL ) {
                    hv_store(atV, "Value", 5,
                             _C2Sv(value, NULL), ValueHash);
                }

                if ( xmlStrEqual( (const xmlChar *)"xmlns", name ) ) {
                    /* a default namespace */
                    PmmAddNamespace( sax, NULL, value, handler);  
                    /* nsURI = (const xmlChar*)NSDEFAULTURI; */
                    nsURI = NULL;
                    hv_store(atV, "Name", 4,
                             _C2Sv(name, NULL), NameHash);

                    hv_store(atV, "Prefix", 6,
                             _C2Sv((const xmlChar *)"", NULL), PrefixHash);
                    hv_store(atV, "LocalName", 9,
                             _C2Sv(name,NULL), LocalNameHash);
                    hv_store(atV, "NamespaceURI", 12,
                             _C2Sv((const xmlChar *)"", NULL), NsURIHash);
                    
                }
                else if (xmlStrncmp((const xmlChar *)"xmlns:", name, 6 ) == 0 ) {
                    PmmAddNamespace( sax,
                                     localname,
                                     value,
                                     handler);

                    nsURI = (const xmlChar*)NSDEFAULTURI;
         
                    hv_store(atV, "Prefix", 6,
                             _C2Sv(prefix, NULL), PrefixHash);
                    hv_store(atV, "LocalName", 9,
                             _C2Sv(localname, NULL), LocalNameHash);
                    hv_store(atV, "NamespaceURI", 12,
                             _C2Sv((const xmlChar *)NSDEFAULTURI,NULL),
                             NsURIHash);
                }
                else if ( prefix != NULL
                          && (ns = PmmGetNsMapping( sax->ns_stack, prefix ) ) ) {
                    nsURI = ns->href;

                    hv_store(atV, "NamespaceURI", 12,
                             _C2Sv(ns->href, NULL), NsURIHash);
                    hv_store(atV, "Prefix", 6,
                             _C2Sv(ns->prefix, NULL), PrefixHash);
                    hv_store(atV, "LocalName", 9,
                             _C2Sv(localname, NULL), LocalNameHash);
                }
                else {
                    nsURI = NULL;
                    hv_store(atV, "NamespaceURI", 12,
                             _C2Sv((const xmlChar *)"", NULL), NsURIHash);
                    hv_store(atV, "Prefix", 6,
                             _C2Sv((const xmlChar *)"", NULL), PrefixHash);
                    hv_store(atV, "LocalName", 9,
                             _C2Sv(name, NULL), LocalNameHash);
                }

                keyname = PmmGenNsName( localname != NULL ? localname : name,
                                        nsURI );

                len = xmlStrlen( keyname );
                PERL_HASH( atnameHash, (const char *)keyname, len );
                hv_store(retval,
                         (const char *)keyname,
                         len,
                         newRV_noinc((SV*)atV),
                         atnameHash );

                if ( keyname != NULL ) {
                    xmlFree( keyname );
                }
                if ( localname != NULL ) {
                    xmlFree(localname);
                }
                localname = NULL;
                if ( prefix != NULL ) {
                    xmlFree( prefix );
                }
                prefix    = NULL;

            }            
        }
    }

    return retval;
}

HV * 
PmmGenCharDataSV( pTHX_ PmmSAXVectorPtr sax, const xmlChar * data, int len )
{
    HV * retval = newHV();

    if ( data != NULL && xmlStrlen( data ) ) {
        hv_store(retval, "Data", 4,
                 _C2Sv_len(data, len), DataHash);
    }

    return retval;
}

HV * 
PmmGenPISV( pTHX_ PmmSAXVectorPtr sax,
            const xmlChar * target,
            const xmlChar * data )
{
    HV * retval = newHV();

    if ( target != NULL && xmlStrlen( target ) ) {
        hv_store(retval, "Target", 6,
                 _C2Sv(target, NULL), TargetHash);

        if ( data != NULL && xmlStrlen( data ) ) {
            hv_store(retval, "Data", 4,
                     _C2Sv(data, NULL), DataHash);
        }
        else {
            hv_store(retval, "Data", 4,
                     _C2Sv((const xmlChar *)"", NULL), DataHash);
        }
    }

    return retval;
}

int
PSaxStartDocument(void * ctx)
{
    xmlParserCtxtPtr ctxt = (xmlParserCtxtPtr)ctx;
    PmmSAXVectorPtr sax   = (PmmSAXVectorPtr)ctxt->_private;
    dTHX;
    HV* empty;
    SV * handler         = sax->handler;

    SV * rv;
    if ( handler != NULL ) {
       
        dSP;
        
        ENTER;
        SAVETMPS;
        
        empty = newHV();
        PUSHMARK(SP) ;
        XPUSHs(handler);
        XPUSHs(sv_2mortal(newRV_noinc((SV*)empty)));
        PUTBACK;
        
        call_method( "start_document", G_SCALAR | G_EVAL | G_DISCARD );
        if (SvTRUE(ERRSV)) {
            STRLEN n_a;
            croak(SvPV(ERRSV, n_a));
        }
        
        SPAGAIN;

        PUSHMARK(SP) ;

    
        XPUSHs(handler);

        empty = newHV();
        if ( ctxt->version != NULL ) {
            hv_store(empty, "Version", 7,
                     _C2Sv(ctxt->version, NULL), VersionHash);
        }
        else {
            hv_store(empty, "Version", 7,
                     _C2Sv((const xmlChar *)"1.0", NULL), VersionHash);
        }
        
        if ( ctxt->input->encoding != NULL ) {
            hv_store(empty, "Encoding", 8,
                     _C2Sv(ctxt->input->encoding, NULL), EncodingHash);
        }

        rv = newRV_noinc((SV*)empty);
        XPUSHs( rv);

        PUTBACK;
        
        call_method( "xml_decl", G_SCALAR | G_EVAL | G_DISCARD );
        sv_2mortal(rv);
        if (SvTRUE(ERRSV)) {
            STRLEN n_a;
            croak(SvPV(ERRSV, n_a));
        }
        
        FREETMPS ;
        LEAVE ;
    }

    return 1;
}

int
PSaxEndDocument(void * ctx)
{
    xmlParserCtxtPtr ctxt = (xmlParserCtxtPtr)ctx;
    PmmSAXVectorPtr  sax  = (PmmSAXVectorPtr)ctxt->_private;

    dTHX;
    dSP;

    ENTER;
    SAVETMPS;

    PUSHMARK(SP) ;
    XPUSHs(sax->parser);
    PUTBACK;

    call_pv( "XML::LibXML::_SAXParser::end_document", G_SCALAR | G_EVAL | G_DISCARD );
    if (SvTRUE(ERRSV)) {
        STRLEN n_a;
        croak(SvPV(ERRSV, n_a));
    }
    
    FREETMPS ;
    LEAVE ;

    return 1;
}

int
PSaxStartElement(void *ctx, const xmlChar * name, const xmlChar** attr)
{
    xmlParserCtxtPtr ctxt = (xmlParserCtxtPtr)ctx;
    PmmSAXVectorPtr  sax  = (PmmSAXVectorPtr)ctxt->_private;
    dTHX;
    HV * attrhash         = NULL;
    HV * element          = NULL;
    SV * handler          = sax->handler;
    SV * rv;
    SV * arv;

    dSP;
    
    ENTER;
    SAVETMPS;

    PmmExtendNsStack(sax, name);

    attrhash = PmmGenAttributeHashSV(aTHX_ sax, attr, handler );
    element  = PmmGenElementSV(aTHX_ sax, name);

    arv = newRV_noinc((SV*)attrhash);
    hv_store( element,
              "Attributes",
              10,
              arv,
              AttributesHash );
    
    PUSHMARK(SP) ;

    XPUSHs(handler);
    rv = newRV_noinc((SV*)element);
    XPUSHs(rv);
    PUTBACK;

    call_method( "start_element", G_SCALAR | G_EVAL | G_DISCARD );
    sv_2mortal(rv) ;

    if (SvTRUE(ERRSV)) {
        STRLEN n_a;
        croak(SvPV(ERRSV, n_a));
    }
    
    FREETMPS ;
    LEAVE ;


    return 1;
}

int
PSaxEndElement(void *ctx, const xmlChar * name) {
    xmlParserCtxtPtr ctxt = (xmlParserCtxtPtr)ctx;
    PmmSAXVectorPtr  sax  = (PmmSAXVectorPtr)ctxt->_private;
    dTHX;
    SV * handler         = sax->handler;
    SV * rv;
    HV * element;

    dSP;

    ENTER;
    SAVETMPS;

    PUSHMARK(SP) ;
    XPUSHs(handler);

    element = PmmGenElementSV(aTHX_ sax, name);
    rv = newRV_noinc((SV*)element);

    XPUSHs(rv);
    PUTBACK;

    call_method( "end_element", G_SCALAR | G_EVAL | G_DISCARD );
    sv_2mortal(rv);
    
    if (SvTRUE(ERRSV)) {
        STRLEN n_a;
        croak(SvPV(ERRSV, n_a));
    }
    
    FREETMPS ;
    LEAVE ;

    PmmNarrowNsStack(sax, handler);

    return 1;
}

int
PSaxCharacters(void *ctx, const xmlChar * ch, int len) {
    xmlParserCtxtPtr ctxt = (xmlParserCtxtPtr)ctx;
    PmmSAXVectorPtr sax = (PmmSAXVectorPtr)ctxt->_private;
    dTHX;
    HV* element;
    SV * handler;
    SV * rv = NULL;

    if ( sax == NULL ) {
/*         warn( "lost my sax context!? ( %s, %d )\n", ch, len ); */
        return 0;
    }

    handler = sax->handler;

    if ( ch != NULL && handler != NULL ) {

        dSP;

        ENTER;
        SAVETMPS;

        PUSHMARK(SP) ;

        XPUSHs(handler);
        element = PmmGenCharDataSV(aTHX_ sax, ch, len );

        rv = newRV_noinc((SV*)element);
        XPUSHs(rv);
        sv_2mortal(rv);

        PUTBACK;

        call_method( "characters", G_SCALAR | G_EVAL | G_DISCARD );

        if (SvTRUE(ERRSV)) {
	  STRLEN n_a;
	  croak(SvPV(ERRSV, n_a));
	}
        
        FREETMPS ;
        LEAVE ;

    }

    return 1;
}

int
PSaxComment(void *ctx, const xmlChar * ch) {
    xmlParserCtxtPtr ctxt = (xmlParserCtxtPtr)ctx;
    PmmSAXVectorPtr sax = (PmmSAXVectorPtr)ctxt->_private;
    dTHX;
    HV* element;
    SV * handler = sax->handler;
    
    SV * rv = NULL;

    if ( ch != NULL && handler != NULL ) {
        int len = xmlStrlen( ch );

        dSP;

        ENTER;
        SAVETMPS;

        PUSHMARK(SP) ;
        XPUSHs(handler);
        element = PmmGenCharDataSV(aTHX_ sax, ch, len);

        rv = newRV_noinc((SV*)element);
        XPUSHs(rv);
        PUTBACK;

        call_method( "comment", G_SCALAR | G_EVAL | G_DISCARD );
        sv_2mortal(rv);

        if (SvTRUE(ERRSV)) {
            STRLEN n_a;
            croak(SvPV(ERRSV, n_a));
        }
        
        FREETMPS ;
        LEAVE ;
    }

    return 1;
}

int
PSaxCDATABlock(void *ctx, const xmlChar * ch, int len) {
    xmlParserCtxtPtr ctxt = (xmlParserCtxtPtr)ctx;
    PmmSAXVectorPtr sax = (PmmSAXVectorPtr)ctxt->_private;
    dTHX;

    HV* element;
    SV * handler = sax->handler;
    
    SV * rv = NULL;

    if ( ch != NULL && handler != NULL ) {

        dSP;

        ENTER;
        SAVETMPS;

        PUSHMARK(SP) ;
        XPUSHs(handler);
        PUTBACK;
        call_method( "start_cdata", G_SCALAR | G_EVAL | G_DISCARD );
        if (SvTRUE(ERRSV)) {
            STRLEN n_a;
            croak(SvPV(ERRSV, n_a));
        }
        
        SPAGAIN;        
        PUSHMARK(SP) ;
    
        XPUSHs(handler);
        element = PmmGenCharDataSV(aTHX_ sax, ch, len);

        rv = newRV_noinc((SV*)element);
        XPUSHs(rv);
        PUTBACK;

        call_method( "characters", G_SCALAR | G_EVAL | G_DISCARD);
        if (SvTRUE(ERRSV)) {
            STRLEN n_a;
            croak(SvPV(ERRSV, n_a));
        }
        
        SPAGAIN;        
        PUSHMARK(SP) ;
    
        XPUSHs(handler);
        PUTBACK;

        call_method( "end_cdata", G_SCALAR | G_EVAL | G_DISCARD );
        sv_2mortal(rv);
        
        if (SvTRUE(ERRSV)) {
            STRLEN n_a;
            croak(SvPV(ERRSV, n_a));
        }
        
        FREETMPS ;
        LEAVE ;

    }

    return 1;

}

int
PSaxProcessingInstruction( void * ctx, const xmlChar * target, const xmlChar * data )
{
    xmlParserCtxtPtr ctxt = (xmlParserCtxtPtr)ctx;
    PmmSAXVectorPtr sax   = (PmmSAXVectorPtr)ctxt->_private;
    dTHX;
    SV * handler          = sax->handler;

    SV * element;
    SV * rv = NULL;

    if ( handler != NULL ) {
        dSP;
    
        ENTER;
        SAVETMPS;

        PUSHMARK(SP) ;
        XPUSHs(handler);
        element = (SV*)PmmGenPISV(aTHX_ sax, (const xmlChar *)target, data);
        rv = newRV_noinc((SV*)element);
        XPUSHs(rv);

        PUTBACK;

        call_method( "processing_instruction", G_SCALAR | G_EVAL | G_DISCARD );

        sv_2mortal(rv);

        if (SvTRUE(ERRSV)) {
            STRLEN n_a;
            croak(SvPV(ERRSV, n_a));
        }
        
        FREETMPS ;
        LEAVE ;
    }
    return 1;
}

int
PmmSaxWarning(void * ctx, const char * msg, ...)
{
    xmlParserCtxtPtr ctxt = (xmlParserCtxtPtr)ctx;
    PmmSAXVectorPtr sax = (PmmSAXVectorPtr)ctxt->_private;

    va_list args;
    SV * svMessage;

    dTHX;
    dSP;
    svMessage = NEWSV(0,512);

    va_start(args, msg);
    sv_vsetpvfn(svMessage,
                msg,
                xmlStrlen((const xmlChar *)msg),
                &args,
                NULL,
                0,
                NULL);
    va_end(args);

    ENTER;
    SAVETMPS;

    PUSHMARK(SP) ;
    XPUSHs(sax->parser);

    XPUSHs(sv_2mortal(svMessage));
    XPUSHs(sv_2mortal(newSViv(ctxt->input->line)));
    XPUSHs(sv_2mortal(newSViv(ctxt->input->col)));

    PUTBACK;

    call_pv( "XML::LibXML::_SAXParser::warning", G_SCALAR | G_EVAL | G_DISCARD );

    if (SvTRUE(ERRSV)) {
        STRLEN n_a;
        croak(SvPV(ERRSV, n_a));
    }
    
    FREETMPS ;
    LEAVE ;

    return 1;
}


int
PmmSaxError(void * ctx, const char * msg, ...)
{
    xmlParserCtxtPtr ctxt = (xmlParserCtxtPtr)ctx;
    PmmSAXVectorPtr sax = (PmmSAXVectorPtr)ctxt->_private;

    va_list args;
    SV * svMessage;
#if LIBXML_VERSION > 20600
    xmlErrorPtr last_err = xmlCtxtGetLastError( ctxt );
#endif    
    dTHX;
    dSP;

    ENTER;
    SAVETMPS;

    PUSHMARK(SP) ;

    XPUSHs(sax->parser);

    svMessage = NEWSV(0,512);

    va_start(args, msg);
    sv_vsetpvfn(svMessage, msg, xmlStrlen((const xmlChar *)msg), &args, NULL, 0, NULL);
    va_end(args);

    sv_catsv( sax->saved_error, svMessage );

    XPUSHs(sv_2mortal(svMessage));
    XPUSHs(sv_2mortal(newSViv(ctxt->input->line)));
    XPUSHs(sv_2mortal(newSViv(ctxt->input->col)));

    PUTBACK;
#if LIBXML_VERSION > 20600
    /* 
       this is a workaround: at least some versions of libxml2 didn't not call 
       the fatalError callback at all
    */
    if (last_err && last_err->level == XML_ERR_FATAL) {
      call_pv( "XML::LibXML::_SAXParser::fatal_error", G_SCALAR | G_EVAL | G_DISCARD );
    } else {
      call_pv( "XML::LibXML::_SAXParser::error", G_SCALAR | G_EVAL | G_DISCARD );
    }
#else
    /* actually, we do not know if it is a fatal error or not */
    call_pv( "XML::LibXML::_SAXParser::fatal_error", G_SCALAR | G_EVAL | G_DISCARD );
#endif
    if (SvTRUE(ERRSV)) {
        STRLEN n_a;
        croak(SvPV(ERRSV, n_a));
    }
    
    FREETMPS ;
    LEAVE ;
    return 1;
}


int
PmmSaxFatalError(void * ctx, const char * msg, ...)
{
    xmlParserCtxtPtr ctxt = (xmlParserCtxtPtr)ctx;
    PmmSAXVectorPtr sax = (PmmSAXVectorPtr)ctxt->_private;

    va_list args;
    SV * svMessage;
 
    dTHX;
    dSP;

    svMessage = NEWSV(0,512);

    va_start(args, msg);
    sv_vsetpvfn(svMessage, msg, xmlStrlen((const xmlChar *)msg), &args, NULL, 0, NULL);
    va_end(args);

    ENTER;
    SAVETMPS;

    PUSHMARK(SP) ;
    XPUSHs(sax->parser);

    sv_catsv( sax->saved_error, svMessage );

    XPUSHs(sv_2mortal(svMessage));
    XPUSHs(sv_2mortal(newSViv(ctxt->input->line)));
    XPUSHs(sv_2mortal(newSViv(ctxt->input->col)));

    PUTBACK;
    call_pv( "XML::LibXML::_SAXParser::fatal_error", G_SCALAR | G_EVAL | G_DISCARD );
    if (SvTRUE(ERRSV)) {
        STRLEN n_a;
        croak(SvPV(ERRSV, n_a));
    }
    
    FREETMPS ;
    LEAVE ;
    return 1;
}

/* NOTE:
 * end document is not handled by the parser itself! use 
 * XML::LibXML::SAX instead!
 */
xmlSAXHandlerPtr
PSaxGetHandler()
{
    xmlSAXHandlerPtr retval = (xmlSAXHandlerPtr)xmlMalloc(sizeof(xmlSAXHandler));
    memset(retval, 0, sizeof(xmlSAXHandler));

    retval->startDocument = (startDocumentSAXFunc)&PSaxStartDocument;

    /* libxml2 will not handle perls returnvalue correctly, so we have 
     * to end the document ourselfes
     */
    retval->endDocument   = NULL; /* (endDocumentSAXFunc)&PSaxEndDocument; */

    retval->startElement  = (startElementSAXFunc)&PSaxStartElement;
    retval->endElement    = (endElementSAXFunc)&PSaxEndElement;

    retval->characters    = (charactersSAXFunc)&PSaxCharacters;
    retval->ignorableWhitespace = (ignorableWhitespaceSAXFunc)&PSaxCharacters;

    retval->comment       = (commentSAXFunc)&PSaxComment;
    retval->cdataBlock    = (cdataBlockSAXFunc)&PSaxCDATABlock;

    retval->processingInstruction = (processingInstructionSAXFunc)&PSaxProcessingInstruction;

    /* warning functions should be internal */
    retval->warning    = (warningSAXFunc)&PmmSaxWarning;
    retval->error      = (errorSAXFunc)&PmmSaxError;
    retval->fatalError = (fatalErrorSAXFunc)&PmmSaxFatalError;

    return retval;
}

