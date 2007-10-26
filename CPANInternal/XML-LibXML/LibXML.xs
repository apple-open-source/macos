/* $Id: LibXML.xs,v 1.1.1.1 2004/05/20 17:55:25 jpetri Exp $ */

#ifdef __cplusplus
extern "C" {
#endif

/* perl stuff */
#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"
#include "ppport.h"

#include <fcntl.h>

#ifndef WIN32
#  include <unistd.h>
#endif

/* libxml2 configuration properties */
#include <libxml/xmlversion.h>

#define DEBUG_C14N

/* libxml2 stuff */
#include <libxml/xmlversion.h>
#include <libxml/globals.h>
#include <libxml/xmlmemory.h>
#include <libxml/parser.h>
#include <libxml/parserInternals.h>
#include <libxml/HTMLparser.h>
#include <libxml/HTMLtree.h>
#include <libxml/DOCBparser.h>
#include <libxml/c14n.h>
#include <libxml/tree.h>
#include <libxml/xpath.h>
#include <libxml/xmlIO.h>
/* #include <libxml/debugXML.h> */
#include <libxml/xmlerror.h>
#include <libxml/xinclude.h>
#include <libxml/valid.h>

#if LIBXML_VERSION >= 20510
#define HAVE_SCHEMAS
#include <libxml/relaxng.h>
#include <libxml/xmlschemas.h>
#endif

#ifdef LIBXML_CATALOG_ENABLED
#include <libxml/catalog.h>
#endif

/* GDOME support
 * libgdome installs only the core functions to the system.
 * this is not enough for XML::LibXML <-> XML::GDOME conversion.
 * therefore there is the need to ship as well the GDOME core headers.
 */
#ifdef XML_LIBXML_GDOME_SUPPORT

#include <libgdome/gdome.h>
#include <libgdome/gdome-libxml-util.h>

#endif

/* XML::LibXML stuff */
#include "perl-libxml-mm.h"
#include "perl-libxml-sax.h"

#include "dom.h"
#include "xpath.h"

#ifdef __cplusplus
}
#endif

#define TEST_PERL_FLAG(flag) \
    SvTRUE(perl_get_sv(flag, FALSE)) ? 1 : 0

static SV * LibXML_match_cb = NULL;
static SV * LibXML_read_cb  = NULL;
static SV * LibXML_open_cb  = NULL;
static SV * LibXML_close_cb = NULL;

/* this should keep the default */
static xmlExternalEntityLoader LibXML_old_ext_ent_loader = NULL;

/* ****************************************************************
 * Error handler
 * **************************************************************** */

static SV * LibXML_error    = NULL;

/* stores libxml errors into $@ */
void
LibXML_error_handler(void * ctxt, const char * msg, ...)
{
    va_list args;
    SV * sv;
    /* xmlParserCtxtPtr context = (xmlParserCtxtPtr) ctxt; */
    sv = NEWSV(0,512);

    va_start(args, msg);
    sv_vsetpvfn(sv, msg, strlen(msg), &args, NULL, 0, NULL);
    va_end(args);

    if (LibXML_error != NULL) {
        sv_catsv(LibXML_error, sv); /* remember the last error */
    }
    else {
       croak("%s", SvPV_nolen(sv));
    }

    SvREFCNT_dec(sv);
}

void
LibXML_validity_error(void * ctxt, const char * msg, ...)
{
    va_list args;
    SV * sv;

    sv = NEWSV(0,512);

    va_start(args, msg);
    sv_vsetpvfn(sv, msg, strlen(msg), &args, NULL, 0, NULL);
    va_end(args);

    if (LibXML_error != NULL) {
        sv_catsv(LibXML_error, sv); /* remember the last error */
    }
    else {
        croak("%s", SvPV_nolen(sv));
    }
    SvREFCNT_dec(sv);
}

void
LibXML_validity_warning(void * ctxt, const char * msg, ...)
{
    va_list args;
    STRLEN len;
    SV * sv;
    char * string = NULL;

    sv = NEWSV(0,512);

    va_start(args, msg);
    sv_vsetpvfn(sv, msg, strlen(msg), &args, NULL, 0, NULL);
    va_end(args);

    string = SvPV(sv, len);
    if ( string != NULL ) {
        if ( len > 0 ) {
            warn("validation error: '%s'" , string);
        }
        Safefree(string);
    }

    SvREFCNT_dec(sv);
}

static void
LibXML_init_error(SV ** saved_errorp)
{
    *saved_errorp = LibXML_error;
    LibXML_error = NEWSV(0, 512);
    sv_setpvn(LibXML_error, "", 0);
    xmlSetGenericErrorFunc(NULL, (xmlGenericErrorFunc) LibXML_error_handler);
}

static void
LibXML_report_error(SV * saved_error, int recover)
{
    SV *my_error = sv_2mortal(LibXML_error);
    LibXML_error = saved_error;
    if ( SvCUR( my_error ) > 0 ) {
        if ( recover ) {
            warn("%s", SvPV_nolen(my_error));
        } else {
            croak("%s", SvPV_nolen(my_error));
        }
    }
}

static int
LibXML_get_recover(HV * real_obj)
{
    SV** item = hv_fetch( real_obj, "XML_LIBXML_RECOVER", 18, 0 );
    return ( item != NULL && SvTRUE(*item) ) ? 1 : 0;
}

static SV *
LibXML_NodeToSv(HV * real_obj, xmlNodePtr real_doc)
{
    SV** item = hv_fetch( real_obj, "XML_LIBXML_GDOME", 16, 0 );

    if ( item != NULL && SvTRUE(*item) ) {
        return PmmNodeToGdomeSv(real_doc);
    }
    else {
        return PmmNodeToSv(real_doc, NULL);
    }
}

/* ****************************************************************
 * IO callbacks
 * **************************************************************** */

int
LibXML_read_perl (SV * ioref, char * buffer, int len)
{
    dTHX;
    dSP;

    int cnt;
    SV * read_results;
    STRLEN read_length;
    char * chars;
    SV * tbuff = NEWSV(0,len);
    SV * tsize = newSViv(len);

    ENTER;
    SAVETMPS;

    PUSHMARK(SP);
    EXTEND(SP, 3);
    PUSHs(ioref);
    PUSHs(sv_2mortal(tbuff));
    PUSHs(sv_2mortal(tsize));
    PUTBACK;

    if (sv_isobject(ioref)) {
        cnt = perl_call_method("read", G_SCALAR | G_EVAL);
    }
    else {
        cnt = perl_call_pv("XML::LibXML::__read", G_SCALAR | G_EVAL);
    }

    SPAGAIN;

    if (cnt != 1) {
        croak("read method call failed");
    }

    if (SvTRUE(ERRSV)) {
       croak("read on filehandle failed: %s", SvPV_nolen(ERRSV));
       POPs ;
    }

    read_results = POPs;

    if (!SvOK(read_results)) {
        croak("read error");
    }

    read_length = SvIV(read_results);

    chars = SvPV(tbuff, read_length);
    strncpy(buffer, chars, read_length);

    FREETMPS;
    LEAVE;

    return read_length;
}

int
LibXML_input_match(char const * filename)
{
    int results = 0;
    SV * global_cb;
    SV * callback = NULL;

    if (LibXML_match_cb && SvTRUE(LibXML_match_cb)) {
        callback = LibXML_match_cb;
    }
    else if ((global_cb = perl_get_sv("XML::LibXML::match_cb", FALSE))
             && SvTRUE(global_cb)) {
        callback = global_cb;
    }

    if (callback) {
        int count;
        SV * res;

        dTHX;
        dSP;

        ENTER;
        SAVETMPS;

        PUSHMARK(SP);
        EXTEND(SP, 1);
        PUSHs(sv_2mortal(newSVpv((char*)filename, 0)));
        PUTBACK;

        count = perl_call_sv(callback, G_SCALAR | G_EVAL);

        SPAGAIN;

        if (count != 1) {
            croak("match callback must return a single value");
        }

        if (SvTRUE(ERRSV)) {
            croak("input match callback died: %s", SvPV_nolen(ERRSV));
            POPs ;
        }

        res = POPs;

        if (SvTRUE(res)) {
            results = 1;
        }

        PUTBACK;
        FREETMPS;
        LEAVE;
    }

    return results;
}

void *
LibXML_input_open(char const * filename)
{
    SV * results;
    SV * global_cb;
    SV * callback = NULL;

    if (LibXML_open_cb && SvTRUE(LibXML_open_cb)) {
        callback = LibXML_open_cb;
    }
    else if ((global_cb = perl_get_sv("XML::LibXML::open_cb", FALSE))
            && SvTRUE(global_cb)) {
        callback = global_cb;
    }

    if (callback) {
        int count;

        dTHX;
        dSP;

        ENTER;
        SAVETMPS;

        PUSHMARK(SP);
        EXTEND(SP, 1);
        PUSHs(sv_2mortal(newSVpv((char*)filename, 0)));
        PUTBACK;

        count = perl_call_sv(callback, G_SCALAR | G_EVAL);

        SPAGAIN;

        if (count != 1) {
            croak("open callback must return a single value");
        }

        if (SvTRUE(ERRSV)) {
            croak("input callback died: %s", SvPV_nolen(ERRSV));
            POPs ;
        }

        results = POPs;

        SvREFCNT_inc(results);

        PUTBACK;
        FREETMPS;
        LEAVE;
    }

    return (void *)results;
}

int
LibXML_input_read(void * context, char * buffer, int len)
{
    STRLEN res_len = 0;
    const char * output;
    SV * global_cb;
    SV * callback = NULL;
    SV * ctxt = (SV *)context;

    if (LibXML_read_cb && SvTRUE(LibXML_read_cb)) {
        callback = LibXML_read_cb;
    }
    else if ((global_cb = perl_get_sv("XML::LibXML::read_cb", FALSE))
            && SvTRUE(global_cb)) {
        callback = global_cb;
    }

    if (callback) {
        int count;

        dTHX;
        dSP;

        ENTER;
        SAVETMPS;

        PUSHMARK(SP);
        EXTEND(SP, 2);
        PUSHs(ctxt);
        PUSHs(sv_2mortal(newSViv(len)));
        PUTBACK;

        count = perl_call_sv(callback, G_SCALAR | G_EVAL);

        SPAGAIN;

        if (count != 1) {
            croak("read callback must return a single value");
        }

        if (SvTRUE(ERRSV)) {
            croak("read callback died: %s", SvPV_nolen(ERRSV));
            POPs ;
        }

        output = POPp;
        if (output != NULL) {
            res_len = strlen(output);
            if (res_len) {
                strncpy(buffer, output, res_len);
            }
            else {
                buffer[0] = 0;
            }
        }

        FREETMPS;
        LEAVE;
    }

    /* warn("read, asked for: %d, returning: [%d] %s\n", len, res_len, buffer); */
    return res_len;
}

void
LibXML_input_close(void * context)
{
    SV * global_cb;
    SV * callback = NULL;
    SV * ctxt = (SV *)context;

    if (LibXML_close_cb && SvTRUE(LibXML_close_cb)) {
        callback = LibXML_close_cb;
    }
    else if ((global_cb = perl_get_sv("XML::LibXML::close_cb", FALSE))
            && SvTRUE(global_cb)) {
        callback = global_cb;
    }

    if (callback) {
        int count;

        dTHX;
        dSP;

        ENTER;
        SAVETMPS;

        PUSHMARK(SP);
        EXTEND(SP, 1);
        PUSHs(ctxt);
        PUTBACK;

        count = perl_call_sv(callback, G_SCALAR | G_EVAL);

        SPAGAIN;

        SvREFCNT_dec(ctxt);

        if (!count) {
            croak("close callback failed");
        }

        if (SvTRUE(ERRSV)) {
            croak("close callback died: %s", SvPV_nolen(ERRSV));
            POPs ;
        }

        PUTBACK;
        FREETMPS;
        LEAVE;
    }
}

int
LibXML_output_write_handler(void * ioref, char * buffer, int len)
{
    if ( buffer != NULL && len > 0) {
        dTHX;
        dSP;

        int cnt;
        SV * tbuff = newSVpv(buffer,len);
        SV * tsize = newSViv(len);


        ENTER;
        SAVETMPS;

        PUSHMARK(SP);
        EXTEND(SP, 3);
        PUSHs((SV*)ioref);
        PUSHs(sv_2mortal(tbuff));
        PUSHs(sv_2mortal(tsize));
        PUTBACK;

        cnt = perl_call_pv("XML::LibXML::__write", G_SCALAR | G_EVAL);

        SPAGAIN;

        if (cnt != 1) {
            croak("write method call failed");
        }

        if (SvTRUE(ERRSV)) {
            croak("write method call died: %s", SvPV_nolen(ERRSV));
            POPs ;
        }

        FREETMPS;
        LEAVE;
    }
    return len;
}

int
LibXML_output_close_handler( void * handler )
{
    return 1;
}

xmlParserInputPtr
LibXML_load_external_entity(
        const char * URL,
        const char * ID,
        xmlParserCtxtPtr ctxt)
{
    SV * self;
    HV * real_obj;
    SV ** func;
    int count;
    SV * results;
    STRLEN results_len;
    const char * results_pv;
    xmlParserInputBufferPtr input_buf;

    if (ctxt->_private == NULL) {
        return xmlNewInputFromFile(ctxt, URL);
    }

    if (URL == NULL) {
        URL = "";
    }
    if (ID == NULL) {
        ID = "";
    }

    self = (SV *)ctxt->_private;
    real_obj = (HV *)SvRV(self);
    func = hv_fetch(real_obj, "ext_ent_handler", 15, 0);

    if (func) {
        dTHX;
        dSP;

        ENTER;
        SAVETMPS;

        PUSHMARK(SP) ;
        XPUSHs(sv_2mortal(newSVpv((char*)URL, 0)));
        XPUSHs(sv_2mortal(newSVpv((char*)ID, 0)));
        PUTBACK;

        count = perl_call_sv(*func, G_SCALAR | G_EVAL);

        SPAGAIN;

        if (!count) {
            croak("external entity handler did not return a value");
        }

        if (SvTRUE(ERRSV)) {
            croak("external entity callback died: %s", SvPV_nolen(ERRSV));
            POPs ;
        }

        results = POPs;

        results_pv = SvPV(results, results_len);
        input_buf = xmlParserInputBufferCreateMem(
                        results_pv,
                        results_len,
                        XML_CHAR_ENCODING_NONE
                        );

        FREETMPS;
        LEAVE;

        return xmlNewIOInputStream(ctxt, input_buf, XML_CHAR_ENCODING_NONE);
    }
    else {
        if (URL == NULL) {
            return NULL;
        }
        return xmlNewInputFromFile(ctxt, URL);
    }
}

/* ****************************************************************
 * Helper functions
 * **************************************************************** */

HV*
LibXML_init_parser( SV * self ) {
    /* we fetch all switches and callbacks from the hash */
    HV* real_obj = NULL;
    SV** item    = NULL;
    SV*  item2   = NULL;
    /* xmlInitParser(); */ /* useless call */
    xmlGetWarningsDefaultValue = 0;

    if ( self != NULL ) {
        /* first fetch the values from the hash */
        real_obj = (HV *)SvRV(self);

        item = hv_fetch( real_obj, "XML_LIBXML_VALIDATION", 21, 0 );
        if ( item != NULL && SvTRUE(*item) ) {
            xmlDoValidityCheckingDefaultValue = 1;
            xmlLoadExtDtdDefaultValue |= XML_DETECT_IDS;
        }
        else {
            xmlDoValidityCheckingDefaultValue = 0;
        }

        item = hv_fetch( real_obj, "XML_LIBXML_EXPAND_ENTITIES", 26, 0 );
        if ( item != NULL ) {
            if ( SvTRUE(*item) ) {
                xmlSubstituteEntitiesDefaultValue = 1;
                xmlLoadExtDtdDefaultValue |= XML_DETECT_IDS;
            }
            else {
                xmlSubstituteEntitiesDefaultValue = 0;
            }
        }
        else {
            xmlSubstituteEntitiesDefaultValue = 1;
            xmlLoadExtDtdDefaultValue |= XML_DETECT_IDS;
        }

        item = hv_fetch( real_obj, "XML_LIBXML_KEEP_BLANKS", 22, 0 );
        if ( item != NULL ) {
            if ( SvTRUE(*item) )
                xmlKeepBlanksDefault(1);
            else {
                xmlKeepBlanksDefault(0);
            }
        }
        else {
            /* keep blanks on default */
            xmlKeepBlanksDefault(1);
        }

        item = hv_fetch( real_obj, "XML_LIBXML_PEDANTIC", 19, 0 );
        if ( item != NULL && SvTRUE(*item) ) {
#ifdef LIBXML_THREAD_ENABLED
#if LIBXML_VERSION != 20507
            xmlThrDefPedanticParserDefaultValue( 1 );
#endif
#endif
            xmlPedanticParserDefaultValue = 1;
        }
        else {
#ifdef LIBXML_THREAD_ENABLED
#if LIBXML_VERSION != 20507
            xmlThrDefPedanticParserDefaultValue( 0 );
#endif
#endif
            xmlPedanticParserDefaultValue = 0;
        }

        item =  hv_fetch( real_obj, "XML_LIBXML_LINENUMBERS", 22, 0 );
        if ( item != NULL && SvTRUE(*item) ) {
            xmlLineNumbersDefault( 1 );
        }
        else {
            xmlLineNumbersDefault( 0 );
        }

        item = hv_fetch( real_obj, "XML_LIBXML_EXT_DTD", 18, 0 );
        if ( item != NULL && SvTRUE(*item) )
            xmlLoadExtDtdDefaultValue |= 1;
        else
            xmlLoadExtDtdDefaultValue ^= 1;

        item = hv_fetch( real_obj, "XML_LIBXML_COMPLETE_ATTR", 24, 0 );
        if (item != NULL && SvTRUE(*item)) {
            xmlLoadExtDtdDefaultValue |= XML_COMPLETE_ATTRS;
        }
        else {
            xmlLoadExtDtdDefaultValue ^= XML_COMPLETE_ATTRS;
        }
        /* now fetch the callbacks */

        item = hv_fetch( real_obj, "XML_LIBXML_READ_CB", 18, 0 );
        if ( item != NULL && SvTRUE(*item))
            LibXML_read_cb= *item;

        item = hv_fetch( real_obj, "XML_LIBXML_MATCH_CB", 19, 0 );
        if ( item != NULL  && SvTRUE(*item))
            LibXML_match_cb= *item;

        item = hv_fetch( real_obj, "XML_LIBXML_OPEN_CB", 18, 0 );
        if ( item != NULL  && SvTRUE(*item))
            LibXML_open_cb = *item;

        item = hv_fetch( real_obj, "XML_LIBXML_CLOSE_CB", 19, 0 );
        if ( item != NULL  && SvTRUE(*item))
            LibXML_close_cb = *item;

        item = hv_fetch(real_obj, "ext_ent_handler", 15, 0);
        if ( item != NULL  && SvTRUE(*item)) {
            LibXML_old_ext_ent_loader =  xmlGetExternalEntityLoader();
            xmlSetExternalEntityLoader( (xmlExternalEntityLoader)LibXML_load_external_entity );
        }
        else {
            /* LibXML_old_ext_ent_loader =  NULL; */
        }
    }

    /*
     * If the parser callbacks are not set, we have to check CLASS wide
     * callbacks.
     */

    if ( LibXML_match_cb == NULL ) {
        item2 = perl_get_sv("XML::LibXML::MatchCB", 0);
        if ( item != NULL  && SvTRUE(item2))
            LibXML_match_cb= item2;
    }
    if ( LibXML_read_cb == NULL ) {
        item2 = perl_get_sv("XML::LibXML::ReadCB", 0);
        if ( item2 != NULL  && SvTRUE(item2))
            LibXML_read_cb= item2;
    }
    if ( LibXML_open_cb == NULL ) {
        item2 = perl_get_sv("XML::LibXML::OpenCB", 0);
        if ( item2 != NULL  && SvTRUE(item2))
            LibXML_open_cb= item2;
    }
    if ( LibXML_close_cb == NULL ) {
        item2 = perl_get_sv("XML::LibXML::CloseCB", 0);
        if ( item2 != NULL  && SvTRUE(item2))
            LibXML_close_cb= item2;
    }

     /* LibXML_old_ext_ent_loader =  xmlGetExternalEntityLoader();  */
     /* xmlSetExternalEntityLoader( (xmlExternalEntityLoader)LibXML_load_external_entity ); */

    xmlRegisterInputCallbacks((xmlInputMatchCallback) LibXML_input_match,
                              (xmlInputOpenCallback) LibXML_input_open,
                              (xmlInputReadCallback) LibXML_input_read,
                              (xmlInputCloseCallback) LibXML_input_close);

    return real_obj;
}

void
LibXML_cleanup_parser() {
    xmlSubstituteEntitiesDefaultValue = 1;
    xmlKeepBlanksDefaultValue = 1;
    xmlGetWarningsDefaultValue = 0;
    xmlLoadExtDtdDefaultValue = 5;
    xmlPedanticParserDefaultValue = 0;
    xmlLineNumbersDefault( 0 );
    xmlDoValidityCheckingDefaultValue = 0;

    if (LibXML_old_ext_ent_loader != NULL ) {
        xmlSetExternalEntityLoader( (xmlExternalEntityLoader)LibXML_old_ext_ent_loader );
    }
}

void
LibXML_cleanup_callbacks() {

/*    xs_warn("      cleanup parser callbacks!\n"); */

    xmlCleanupInputCallbacks();
    xmlRegisterDefaultInputCallbacks();

/*    if ( LibXML_old_ext_ent_loader != NULL ) { */
/*        xmlSetExternalEntityLoader( NULL ); */
/*        xmlSetExternalEntityLoader( LibXML_old_ext_ent_loader ); */
/*        LibXML_old_ext_ent_loader = NULL; */
/*    } */

}


int
LibXML_test_node_name( xmlChar * name )
{
    xmlChar * cur = name;
    int tc  = 0;
    int len = 0;

    if ( cur == NULL || *cur == 0 ) {
        /* warn("name is empty" ); */
        return(0);
    }

    tc = domParseChar( cur, &len );

    if ( !( IS_LETTER( tc ) || (tc == '_') || (tc == ':')) ) {
        /* warn( "is not a letter\n" ); */
        return(0);
    }

    tc  =  0;
    cur += len;

    while (*cur != 0 ) {
        tc = domParseChar( cur, &len );

        if (!(IS_LETTER(tc) || IS_DIGIT(tc) || (tc == '_') ||
             (tc == '-') || (tc == ':') || (tc == '.') ||
             IS_COMBINING(tc) || IS_EXTENDER(tc)) ) {
            /* warn( "is not a letter\n" ); */
            return(0);
        }
        tc = 0;
        cur += len;
    }

    /* warn("name is ok"); */
    return(1);
}

MODULE = XML::LibXML         PACKAGE = XML::LibXML

PROTOTYPES: DISABLE

BOOT:
    LIBXML_TEST_VERSION
    xmlInitParser();
    PmmSAXInitialize(aTHX);

    /* make the callback mechnism available to perl coders */
    xmlRegisterInputCallbacks((xmlInputMatchCallback) LibXML_input_match,
                              (xmlInputOpenCallback) LibXML_input_open,
                              (xmlInputReadCallback) LibXML_input_read,
                              (xmlInputCloseCallback) LibXML_input_close);

    xmlSetGenericErrorFunc( NULL ,
                           (xmlGenericErrorFunc)LibXML_error_handler);
    xmlDoValidityCheckingDefaultValue = 0;
    xmlSubstituteEntitiesDefaultValue = 1;
    xmlGetWarningsDefaultValue = 0;
    xmlKeepBlanksDefaultValue = 1;
    xmlLoadExtDtdDefaultValue = 5;
    xmlPedanticParserDefaultValue = 0;
    xmlLineNumbersDefault(0);
    xmlSetGenericErrorFunc(NULL, NULL);
#ifdef LIBXML_CATALOG_ENABLED
    /* xmlCatalogSetDebug(10); */
    xmlInitializeCatalog(); /* use catalog data */
#endif

char *
LIBXML_DOTTED_VERSION()
    CODE:
        RETVAL = LIBXML_DOTTED_VERSION;
    OUTPUT:
        RETVAL


int
LIBXML_VERSION()
    CODE:
        RETVAL = LIBXML_VERSION;
    OUTPUT:
        RETVAL


void
END()
    CODE:
        xmlCleanupParser();

char *
get_last_error(CLASS)
        SV * CLASS
    CODE:
        RETVAL = NULL;
        if (LibXML_error != NULL) {
            RETVAL = SvPV_nolen(LibXML_error);
        }
    OUTPUT:
        RETVAL

SV*
_parse_string(self, string, dir = &PL_sv_undef)
        SV * self
        SV * string
        SV * dir
    PREINIT:
        char * directory = NULL;
        STRLEN len;
        char * ptr;
        SV * saved_error;
        HV * real_obj;
        int well_formed;
        int valid;
        xmlDocPtr real_doc;
        int recover = 0;
    INIT:
        if (SvPOK(dir)) {
            directory = SvPV(dir, len);
            if (len <= 0) {
                directory = NULL;
            }
        }
        ptr = SvPV(string, len);
        if (len <= 0) {
            croak("Empty string");
            XSRETURN_UNDEF;
        }
    CODE:
        RETVAL = &PL_sv_undef;
        LibXML_init_error(&saved_error);
        real_obj = LibXML_init_parser(self);

        {
            xmlParserCtxtPtr ctxt = xmlCreateMemoryParserCtxt((const char*)ptr, len);
            if (ctxt == NULL) {
                LibXML_report_error(saved_error, 1);
                croak("Couldn't create memory parser context: %s", strerror(errno));
            }
            xs_warn( "context created\n");

            if ( directory != NULL ) {
                ctxt->directory = directory;
            }
            ctxt->_private = (void*)self;

            /* make libxml2-2.6 display line number on error */
            if ( ctxt->input != NULL ) {
                ctxt->input->filename = xmlStrdup((const xmlChar *) "");
            }

            xs_warn( "context initialized\n" );

            {
#if LIBXML_VERSION > 20600
                SV** item =  hv_fetch( real_obj, "XML_LIBXML_NSCLEAN", 18, 0 );
                if ( item != NULL && SvTRUE(*item) ) {
                    ctxt->options |= XML_PARSE_NSCLEAN;
                }
#endif
                xmlParseDocument(ctxt);
                xs_warn( "document parsed \n");
            }

            ctxt->directory = NULL;
            well_formed = ctxt->wellFormed;
            valid = ctxt->valid;
            real_doc = ctxt->myDoc;
            ctxt->myDoc = NULL;
            xmlFreeParserCtxt(ctxt);
        }
        if ( real_doc != NULL ) {
            recover = LibXML_get_recover(real_obj);

            if ( directory == NULL ) {
                SV * newURI = sv_2mortal(newSVpvf("unknown-%12.12d", (void*)real_doc));
                real_doc->URL = xmlStrdup((const xmlChar*)SvPV_nolen(newURI));
            } else {
                real_doc->URL = xmlStrdup((const xmlChar*)directory);
            }
            if ( recover || ( well_formed &&
                              ( !xmlDoValidityCheckingDefaultValue
                                || ( valid || ( real_doc->intSubset == NULL
                                                && real_doc->extSubset == NULL ))))) {
                RETVAL = LibXML_NodeToSv( real_obj, (xmlNodePtr) real_doc );
            } else {
                xmlFreeDoc(real_doc);
            }
        }

        LibXML_cleanup_callbacks();
        LibXML_cleanup_parser();
        LibXML_report_error(saved_error, recover);
    OUTPUT:
        RETVAL

int
_parse_sax_string(self, string)
        SV * self
        SV * string
    PREINIT:
        STRLEN len;
        char * ptr;
        SV * saved_error;
        HV * real_obj;
        int recover = 0;
    INIT:
        ptr = SvPV(string, len);
        if (len <= 0) {
            croak("Empty string");
            XSRETURN_UNDEF;
        }
    CODE:
        RETVAL = 0;
        LibXML_init_error(&saved_error);
        real_obj = LibXML_init_parser(self);
        recover = LibXML_get_recover(real_obj);

        {
            xmlParserCtxtPtr ctxt = xmlCreateMemoryParserCtxt((const char*)ptr, len);
            if (ctxt == NULL) {
                LibXML_report_error(saved_error, 1);
                croak("Couldn't create memory parser context: %s", strerror(errno));
            }
            xs_warn( "context created\n");

            PmmSAXInitContext( ctxt, self );
            xs_warn( "context initialized \n");

            {
                RETVAL = xmlParseDocument(ctxt);
                xs_warn( "document parsed \n");
            }

            PmmSAXCloseContext(ctxt);
            xmlFreeParserCtxt(ctxt);
        }

        LibXML_cleanup_callbacks();
        LibXML_cleanup_parser();
        LibXML_report_error(saved_error, recover);
    OUTPUT:
        RETVAL

SV*
_parse_fh(self, fh, dir = &PL_sv_undef)
        SV * self
        SV * fh
        SV * dir
    PREINIT:
        STRLEN len;
        char * directory = NULL;
        SV * saved_error;
        HV * real_obj;
        int well_formed;
        int valid;
        xmlDocPtr real_doc;
        int recover = 0;
    INIT:
        if (SvPOK(dir)) {
            directory = SvPV(dir, len);
            if (len <= 0) {
                directory = NULL;
            }
        }
    CODE:
        RETVAL = &PL_sv_undef;
        LibXML_init_error(&saved_error);
        real_obj = LibXML_init_parser(self);

        {
            int read_length;
            char buffer[1024];
            xmlParserCtxtPtr ctxt;

            read_length = LibXML_read_perl(fh, buffer, 4);
            if (read_length <= 0) {
                croak( "Empty Stream" );
            }

            ctxt = xmlCreatePushParserCtxt(NULL, NULL, buffer, read_length, NULL);
            if (ctxt == NULL) {
                LibXML_report_error(saved_error, 1);
                croak("Could not create xml push parser context: %s",
                      strerror(errno));
            }
            xs_warn( "context created\n");

            if ( directory != NULL ) {
                ctxt->directory = directory;
            }
            ctxt->_private = (void*)self;
            xs_warn( "context initialized \n");
            {
                int ret;
#if LIBXML_VERSION > 20600
                SV** item =  hv_fetch( real_obj, "XML_LIBXML_NSCLEAN", 18, 0 );
                if ( item != NULL && SvTRUE(*item) ) {
                    ctxt->options |= XML_PARSE_NSCLEAN;
                }
#endif

                while ((read_length = LibXML_read_perl(fh, buffer, 1024))) {
                    ret = xmlParseChunk(ctxt, buffer, read_length, 0);
                    if ( ret != 0 ) {
                        break;
                    }
                }
                ret = xmlParseChunk(ctxt, buffer, 0, 1);
                xs_warn( "document parsed \n");
            }

            ctxt->directory = NULL;
            well_formed = ctxt->wellFormed;
            valid = ctxt->valid;
            real_doc = ctxt->myDoc;
            ctxt->myDoc = NULL;
            xmlFreeParserCtxt(ctxt);
        }

        if ( real_doc != NULL ) {
            recover = LibXML_get_recover(real_obj);

            if ( directory == NULL ) {
                SV * newURI = sv_2mortal(newSVpvf("unknown-%12.12d", (void*)real_doc));
                real_doc->URL = xmlStrdup((const xmlChar*)SvPV_nolen(newURI));
            } else {
                real_doc->URL = xmlStrdup((const xmlChar*)directory);
            }

            if ( recover || ( well_formed &&
                              ( !xmlDoValidityCheckingDefaultValue
                                || ( valid || ( real_doc->intSubset == NULL
                                                && real_doc->extSubset == NULL ))))) {
                RETVAL = LibXML_NodeToSv( real_obj, (xmlNodePtr) real_doc );
            } else {
                xmlFreeDoc(real_doc);
            }
        }

        LibXML_cleanup_callbacks();
        LibXML_cleanup_parser();
        LibXML_report_error(saved_error, recover);
    OUTPUT:
        RETVAL

void
_parse_sax_fh(self, fh, dir = &PL_sv_undef)
        SV * self
        SV * fh
        SV * dir
    PREINIT:
        STRLEN len;
        char * directory = NULL;
        SV * saved_error;
        HV * real_obj;
        int recover = 0;
    INIT:
        if (SvPOK(dir)) {
            directory = SvPV(dir, len);
            if (len <= 0) {
                directory = NULL;
            }
        }
    CODE:
        LibXML_init_error(&saved_error);
        real_obj = LibXML_init_parser(self);
        recover = LibXML_get_recover(real_obj);

        {
            int read_length;
            char buffer[1024];
            xmlSAXHandlerPtr sax;
            xmlParserCtxtPtr ctxt;

            read_length = LibXML_read_perl(fh, buffer, 4);
            if (read_length <= 0) {
                croak( "Empty Stream" );
            }

            sax = PSaxGetHandler();
            ctxt = xmlCreatePushParserCtxt(sax, NULL, buffer, read_length, NULL);
            if (ctxt == NULL) {
                LibXML_report_error(saved_error, 1);
                croak("Could not create xml push parser context: %s",
                      strerror(errno));
            }
            xs_warn( "context created\n");

            if ( directory != NULL ) {
                ctxt->directory = directory;
            }
            PmmSAXInitContext( ctxt, self );
            xs_warn( "context initialized \n");

            {
                int ret;
                while ((read_length = LibXML_read_perl(fh, buffer, 1024))) {
                    ret = xmlParseChunk(ctxt, buffer, read_length, 0);
                    if ( ret != 0 ) {
                        break;
                    }
                }
                ret = xmlParseChunk(ctxt, buffer, 0, 1);
                xs_warn( "document parsed \n");
            }

            ctxt->directory = NULL;
            xmlFree(ctxt->sax);
            ctxt->sax = NULL;
            xmlFree(sax);
            PmmSAXCloseContext(ctxt);
            xmlFreeParserCtxt(ctxt);
        }

        LibXML_cleanup_callbacks();
        LibXML_cleanup_parser();
        LibXML_report_error(saved_error, recover);

SV*
_parse_file(self, filename_sv)
        SV * self
        SV * filename_sv
    PREINIT:
        STRLEN len;
        char * filename;
        SV * saved_error;
        HV * real_obj;
        int well_formed;
        int valid;
        xmlDocPtr real_doc;
        int recover = 0;
    INIT:
        filename = SvPV(filename_sv, len);
        if (len <= 0) {
            croak("Empty filename");
            XSRETURN_UNDEF;
        }
    CODE:
        RETVAL = &PL_sv_undef;
        LibXML_init_error(&saved_error);
        real_obj = LibXML_init_parser(self);

        {
            xmlParserCtxtPtr ctxt = xmlCreateFileParserCtxt(filename);
            if (ctxt == NULL) {
                LibXML_report_error(saved_error, 1);
                croak("Couldn't create file parser context for file \"%s\": %s",
                      filename, strerror(errno));
            }
            xs_warn( "context created\n");

            ctxt->_private = (void*)self;
            xs_warn( "context initialized \n");

            {
#if LIBXML_VERSION > 20600
                SV** item =  hv_fetch( real_obj, "XML_LIBXML_NSCLEAN", 18, 0 );
                if ( item != NULL && SvTRUE(*item) ) {
                    ctxt->options |= XML_PARSE_NSCLEAN;
                }
#endif
                xmlParseDocument(ctxt);
                xs_warn( "document parsed \n");
            }

            well_formed = ctxt->wellFormed;
            valid = ctxt->valid;
            real_doc = ctxt->myDoc;
            ctxt->myDoc = NULL;
            xmlFreeParserCtxt(ctxt);
        }

        if ( real_doc != NULL ) {
            recover = LibXML_get_recover(real_obj);

            if ( recover || ( well_formed &&
                              ( !xmlDoValidityCheckingDefaultValue
                                || ( valid || ( real_doc->intSubset == NULL
                                                && real_doc->extSubset == NULL ))))) {
                RETVAL = LibXML_NodeToSv( real_obj, (xmlNodePtr) real_doc );
            } else {
                xmlFreeDoc(real_doc);
            }
        }

        LibXML_cleanup_callbacks();
        LibXML_cleanup_parser();
        LibXML_report_error(saved_error, recover);
    OUTPUT:
        RETVAL

void
_parse_sax_file(self, filename_sv)
        SV * self
        SV * filename_sv
    PREINIT:
        STRLEN len;
        char * filename;
        SV * saved_error;
        HV * real_obj;
        int recover = 0;
    INIT:
        filename = SvPV(filename_sv, len);
        if (len <= 0) {
            croak("Empty filename");
            XSRETURN_UNDEF;
        }
    CODE:
        LibXML_init_error(&saved_error);
        real_obj = LibXML_init_parser(self);
        recover = LibXML_get_recover(real_obj);

        {
            xmlParserCtxtPtr ctxt = xmlCreateFileParserCtxt(filename);
            if (ctxt == NULL) {
                LibXML_report_error(saved_error, 1);
                croak("Couldn't create file parser context for file \"%s\": %s",
                      filename, strerror(errno));
            }
            xs_warn( "context created\n");

            ctxt->sax = PSaxGetHandler();
            PmmSAXInitContext( ctxt, self );
            xs_warn( "context initialized \n");

            {
                xmlParseDocument(ctxt);
                xs_warn( "document parsed \n");
            }

            PmmSAXCloseContext(ctxt);
            xmlFreeParserCtxt(ctxt);
        }

        LibXML_cleanup_callbacks();
        LibXML_cleanup_parser();
        LibXML_report_error(saved_error, recover);

SV*
parse_html_string(self, string)
        SV * self
        SV * string
    PREINIT:
        STRLEN len;
        char * ptr;
        SV * saved_error;
        HV * real_obj;
        htmlDocPtr real_doc;
        int recover = 0;
    INIT:
        ptr = SvPV(string, len);
        if (len <= 0) {
            croak("Empty string");
            XSRETURN_UNDEF;
        }
    CODE:
        RETVAL = &PL_sv_undef;
        LibXML_init_error(&saved_error);
        real_obj = LibXML_init_parser(self);

        real_doc = htmlParseDoc((xmlChar*)ptr, NULL);

        if ( real_doc != NULL ) {
            recover = LibXML_get_recover(real_obj);

            {
                SV * newURI = sv_2mortal(newSVpvf("unknown-%12.12d", (void*)real_doc));
                real_doc->URL = xmlStrdup((const xmlChar*)SvPV_nolen(newURI));
            }

            /* This HTML memory parser doesn't use a ctxt; there is no "well-formed"
             * distinction, and if it manages to parse the HTML, it returns non-null. */
            RETVAL = LibXML_NodeToSv( real_obj, (xmlNodePtr) real_doc );
        }

        LibXML_cleanup_callbacks();
        LibXML_cleanup_parser();
        LibXML_report_error(saved_error, recover);
    OUTPUT:
        RETVAL

SV*
parse_html_fh(self, fh)
        SV * self
        SV * fh
    PREINIT:
        SV * saved_error;
        HV * real_obj;
        htmlDocPtr real_doc;
        int well_formed;
        int recover = 0;
    CODE:
        RETVAL = &PL_sv_undef;
        LibXML_init_error(&saved_error);
        real_obj = LibXML_init_parser(self);

        {
            int read_length;
            char buffer[1024];
            htmlParserCtxtPtr ctxt;

            read_length = LibXML_read_perl(fh, buffer, 4);
            if (read_length <= 0) {
                croak( "Empty Stream" );
            }

            ctxt = htmlCreatePushParserCtxt(NULL, NULL, buffer, read_length,
                                            NULL, XML_CHAR_ENCODING_NONE);
            if (ctxt == NULL) {
                LibXML_report_error(saved_error, 1);
                croak("Could not create html push parser context: %s",
                      strerror(errno));
            }
            xs_warn( "context created\n");

            ctxt->_private = (void*)self;
            xs_warn( "context initialized \n");

            {
                int ret;
                while ((read_length = LibXML_read_perl(fh, buffer, 1024))) {
                    ret = htmlParseChunk(ctxt, buffer, read_length, 0);
                    if ( ret != 0 ) {
                        break;
                    }
                }
                ret = htmlParseChunk(ctxt, buffer, 0, 1);
                xs_warn( "document parsed \n");
            }

            well_formed = ctxt->wellFormed;
            real_doc = ctxt->myDoc;
            ctxt->myDoc = NULL;
            htmlFreeParserCtxt(ctxt);
        }

        if ( real_doc != NULL ) {
            recover = LibXML_get_recover(real_obj);

            {
                SV * newURI = sv_2mortal(newSVpvf("unknown-%12.12d", (void*)real_doc));
                real_doc->URL = xmlStrdup((const xmlChar*)SvPV_nolen(newURI));
            }

            if ( recover || well_formed ) {
                RETVAL = LibXML_NodeToSv( real_obj, (xmlNodePtr) real_doc );
            } else {
                xmlFreeDoc(real_doc);
            }
        }

        LibXML_cleanup_callbacks();
        LibXML_cleanup_parser();
        LibXML_report_error(saved_error, recover);
    OUTPUT:
        RETVAL

SV*
parse_html_file(self, filename_sv)
        SV * self
        SV * filename_sv
    PREINIT:
        STRLEN len;
        char * filename;
        SV * saved_error;
        HV * real_obj;
        htmlDocPtr real_doc;
        int recover = 0;
    INIT:
        filename = SvPV(filename_sv, len);
        if (len <= 0) {
            croak("Empty filename");
            XSRETURN_UNDEF;
        }
    CODE:
        RETVAL = &PL_sv_undef;
        LibXML_init_error(&saved_error);
        real_obj = LibXML_init_parser(self);

        real_doc = htmlParseFile((const char *)filename, NULL);

        if ( real_doc != NULL ) {
            recover = LibXML_get_recover(real_obj);

            /* This HTML file parser doesn't use a ctxt; there is no "well-formed"
             * distinction, and if it manages to parse the HTML, it returns non-null. */
            RETVAL = LibXML_NodeToSv( real_obj, (xmlNodePtr) real_doc );
        }

        LibXML_cleanup_callbacks();
        LibXML_cleanup_parser();
        LibXML_report_error(saved_error, recover);
    OUTPUT:
        RETVAL

SV*
parse_sgml_string(self, string, enc = &PL_sv_undef)
        SV * self
        SV * string
        SV * enc
    PREINIT:
        STRLEN len;
        char * ptr;
        char * encoding = NULL;
        SV * saved_error;
        HV * real_obj;
        docbDocPtr real_doc;
        int recover = 0;
    INIT:
        ptr = SvPV(string, len);
        if (len <= 0) {
            croak("Empty string");
            XSRETURN_UNDEF;
        }
        if (SvPOK(enc)) {
            encoding = SvPV(enc, len);
            if (len <= 0) {
                encoding = NULL;
            }
        }
    CODE:
        RETVAL = &PL_sv_undef;
        LibXML_init_error(&saved_error);
        real_obj = LibXML_init_parser(self);

        real_doc = docbParseDoc((xmlChar*)ptr, (const char *) encoding);

        if ( real_doc != NULL ) {
            recover = LibXML_get_recover(real_obj);

            {
                SV * newURI = sv_2mortal(newSVpvf("unknown-%12.12d", (void*)real_doc));
                real_doc->URL = xmlStrdup((const xmlChar*)SvPV_nolen(newURI));
            }

            /* This SGML memory parser doesn't use a ctxt; there is no "well-formed"
             * distinction, and if it manages to parse the SGML, it returns non-null. */
            RETVAL = LibXML_NodeToSv( real_obj, (xmlNodePtr) real_doc );
        }

        LibXML_cleanup_callbacks();
        LibXML_cleanup_parser();
        LibXML_report_error(saved_error, recover);
    OUTPUT:
        RETVAL

SV*
parse_sgml_fh(self, fh, enc = &PL_sv_undef)
        SV * self
        SV * fh
        SV * enc
    PREINIT:
        STRLEN len;
        char * encoding = NULL;
        SV * saved_error;
        HV * real_obj;
        docbDocPtr real_doc;
        int well_formed;
        int recover = 0;
    INIT:
        if (SvPOK(enc)) {
            encoding = SvPV(enc, len);
            if (len <= 0) {
                encoding = NULL;
            }
        }
    CODE:
        RETVAL = &PL_sv_undef;
        LibXML_init_error(&saved_error);
        real_obj = LibXML_init_parser(self);

        {
            int read_length;
            char buffer[1024];
            docbParserCtxtPtr ctxt;

            read_length = LibXML_read_perl(fh, buffer, 4);
            if (read_length <= 0) {
                croak( "Empty Stream" );
            }

            ctxt = docbCreatePushParserCtxt(NULL, NULL, buffer, read_length, NULL,
                                            xmlParseCharEncoding((const char*)encoding));
            if (ctxt == NULL) {
                LibXML_report_error(saved_error, 1);
                croak("Could not create docbook SGML push parser context: %s",
                      strerror(errno));
            }
            xs_warn( "context created\n");

            ctxt->_private = (void*)self;
            xs_warn( "context initialized \n");

            {
                int ret;
                while ((read_length = LibXML_read_perl(fh, buffer, 1024))) {
                    ret = docbParseChunk(ctxt, buffer, read_length, 0);
                    if ( ret != 0 ) {
                        break;
                    }
                }
                ret = docbParseChunk(ctxt, buffer, 0, 1);
                xs_warn( "document parsed \n");
            }

            well_formed = ctxt->wellFormed;
            real_doc = ctxt->myDoc;
            ctxt->myDoc = NULL;
            docbFreeParserCtxt(ctxt);
        }

        if ( real_doc != NULL ) {
            recover = LibXML_get_recover(real_obj);

            {
                SV * newURI = sv_2mortal(newSVpvf("unknown-%12.12d", (void*)real_doc));
                real_doc->URL = xmlStrdup((const xmlChar*)SvPV_nolen(newURI));
            }

            if ( recover || well_formed ) {
                RETVAL = LibXML_NodeToSv( real_obj, (xmlNodePtr) real_doc );
            } else {
                xmlFreeDoc(real_doc);
            }
        }

        LibXML_cleanup_callbacks();
        LibXML_cleanup_parser();
        LibXML_report_error(saved_error, recover);
    OUTPUT:
        RETVAL

SV*
parse_sgml_file(self, filename_sv, enc = &PL_sv_undef)
        SV * self
        SV * filename_sv
        SV * enc
    PREINIT:
        STRLEN len;
        char * filename;
        char * encoding = NULL;
        SV * saved_error;
        HV * real_obj;
        docbDocPtr real_doc;
        int recover = 0;
    INIT:
        filename = SvPV(filename_sv, len);
        if (len <= 0) {
            croak("Empty filename");
            XSRETURN_UNDEF;
        }
        if (SvPOK(enc)) {
            encoding = SvPV(enc, len);
            if (len <= 0) {
                encoding = NULL;
            }
        }
    CODE:
        RETVAL = &PL_sv_undef;
        LibXML_init_error(&saved_error);
        real_obj = LibXML_init_parser(self);

        real_doc = docbParseFile((const char *)filename, (const char *) encoding);

        if ( real_doc != NULL ) {
            recover = LibXML_get_recover(real_obj);

            /* This SGML file parser doesn't use a ctxt; there is no "well-formed"
             * distinction, and if it manages to parse the SGML, it returns non-null. */
            RETVAL = LibXML_NodeToSv( real_obj, (xmlNodePtr) real_doc );
        }

        LibXML_cleanup_callbacks();
        LibXML_cleanup_parser();
        LibXML_report_error(saved_error, recover);
    OUTPUT:
        RETVAL

void
_parse_sax_sgml_file(self, filename_sv, enc = &PL_sv_undef)
        SV * self
        SV * filename_sv
        SV * enc
    PREINIT:
        STRLEN len;
        char * filename;
        char * encoding = NULL;
        SV * saved_error;
        HV * real_obj;
        int recover = 0;
    INIT:
        filename = SvPV(filename_sv, len);
        if (len <= 0) {
            croak("Empty filename");
            XSRETURN_UNDEF;
        }
        if (SvPOK(enc)) {
            encoding = SvPV(enc, len);
            if (len <= 0) {
                encoding = NULL;
            }
        }
    CODE:
        LibXML_init_error(&saved_error);
        real_obj = LibXML_init_parser(self);
        recover = LibXML_get_recover(real_obj);

        {
            docbParserCtxtPtr ctxt = docbCreateFileParserCtxt(filename, encoding);
            if (ctxt == NULL) {
                LibXML_report_error(saved_error, 1);
                croak("Couldn't create file parser context for file \"%s\": %s",
                      filename, strerror(errno));
            }
            xs_warn( "context created\n");

            ctxt->sax = PSaxGetHandler();
            PmmSAXInitContext( ctxt, self );
            xs_warn( "context initialized \n");

            {
                docbParseDocument(ctxt);
                xs_warn( "document parsed \n");
            }

            PmmSAXCloseContext(ctxt);
            docbFreeParserCtxt(ctxt);
        }

        LibXML_cleanup_callbacks();
        LibXML_cleanup_parser();
        LibXML_report_error(saved_error, recover);

SV*
_parse_xml_chunk(self, svchunk, enc = &PL_sv_undef)
        SV * self
        SV * svchunk
        SV * enc
    PREINIT:
        STRLEN len;
        char * encoding = "UTF-8";
        SV * saved_error;
        HV * real_obj;
        int recover = 0;
        xmlChar * chunk;
        xmlNodePtr rv = NULL;
    INIT:
        if (SvPOK(enc)) {
            encoding = SvPV(enc, len);
            if (len <= 0) {
                encoding = "UTF-8";
            }
        }
    CODE:
        RETVAL = &PL_sv_undef;
        LibXML_init_error(&saved_error);
        real_obj = LibXML_init_parser(self);

        chunk = Sv2C(svchunk, (const xmlChar*)encoding);

        if ( chunk != NULL ) {
            recover = LibXML_get_recover(real_obj);

            rv = domReadWellBalancedString( NULL, chunk, recover );

            if ( rv != NULL ) {
                xmlNodePtr fragment= NULL;
                xmlNodePtr rv_end = NULL;

                /* now we append the nodelist to a document
                   fragment which is unbound to a Document!!!! */

                /* step 1: create the fragment */
                fragment = xmlNewDocFragment( NULL );
                RETVAL = LibXML_NodeToSv(real_obj, fragment);

                /* step 2: set the node list to the fragment */
                fragment->children = rv;
                rv_end = rv;
                while ( rv_end->next != NULL ) {
                    rv_end->parent = fragment;
                    rv_end = rv_end->next;
                }
                /* the following line is important, otherwise we'll have 
                   occasional segmentation faults
                 */
                rv_end->parent = fragment;
                fragment->last = rv_end;
            }

            /* free the chunk we created */
            xmlFree( chunk );
        }

        LibXML_cleanup_callbacks();
        LibXML_cleanup_parser();
        LibXML_report_error(saved_error, recover);

	if (rv == NULL) {
            croak("_parse_xml_chunk: chunk parsing failed");
        }
    OUTPUT:
        RETVAL

void
_parse_sax_xml_chunk(self, svchunk, enc = &PL_sv_undef)
        SV * self
        SV * svchunk
        SV * enc
    PREINIT:
        STRLEN len;
        char * ptr;
        char * encoding = "UTF-8";
        SV * saved_error;
        HV * real_obj;
        int recover = 0;
        xmlChar * chunk;
        int retCode              = -1;
        xmlNodePtr nodes         = NULL;
        xmlSAXHandlerPtr handler = NULL;
    INIT:
        if (SvPOK(enc)) {
            encoding = SvPV(enc, len);
            if (len <= 0) {
                encoding = "UTF-8";
            }
        }
        ptr = SvPV(svchunk, len);
        if (len <= 0) {
            croak("Empty string");
        }
    CODE:
        LibXML_init_error(&saved_error);
        real_obj = LibXML_init_parser(self);
        recover = LibXML_get_recover(real_obj);

        chunk = Sv2C(svchunk, (const xmlChar*)encoding);

        if ( chunk != NULL ) {
            xmlParserCtxtPtr ctxt = xmlCreateMemoryParserCtxt((const char*)ptr, len);
            if (ctxt == NULL) {
                LibXML_report_error(saved_error, 1);
                croak("Couldn't create memory parser context: %s", strerror(errno));
            }
            xs_warn( "context created\n");

            PmmSAXInitContext( ctxt, self );
            handler = PSaxGetHandler();

            retCode = xmlParseBalancedChunkMemory( NULL,
                                                   handler,
                                                   ctxt,
                                                   0,
                                                   chunk,
                                                   &nodes );

            xmlFree( handler );
            PmmSAXCloseContext(ctxt);
            xmlFreeParserCtxt(ctxt);

            /* free the chunk we created */
            xmlFree( chunk );
        }

        LibXML_cleanup_callbacks();
        LibXML_cleanup_parser();
        LibXML_report_error(saved_error, recover);

	if (retCode == -1) {
            croak("_parse_sax_xml_chunk: chunk parsing failed");
        }

int
_processXIncludes(self, doc)
        SV * self
        SV * doc
    PREINIT:
        xmlDocPtr real_doc;
        SV * saved_error;
        HV * real_obj;
        int recover = 0;
    INIT:
        real_doc = (xmlDocPtr) PmmSvNode(doc);
        if (real_doc == NULL) {
            croak("No document to process!");
            XSRETURN_UNDEF;
        }
    CODE:
        RETVAL = 0;
        LibXML_init_error(&saved_error);
        real_obj = LibXML_init_parser(self);
        recover = LibXML_get_recover(real_obj);

        RETVAL = xmlXIncludeProcess(real_doc);

        LibXML_cleanup_callbacks();
        LibXML_cleanup_parser();
        LibXML_report_error(saved_error, recover);

        if ( RETVAL < 0 ) {
            croak( "unknown error during XInclude processing" );
            XSRETURN_UNDEF;
        } else if ( RETVAL == 0 ) {
            RETVAL = 1;
        }
    OUTPUT:
        RETVAL

SV*
_start_push(self, with_sax=0)
        SV * self
        int with_sax
    PREINIT:
        SV * saved_error;
        HV * real_obj;
        int recover = 0;
        xmlParserCtxtPtr ctxt = NULL;
    CODE:
        RETVAL = &PL_sv_undef;
        LibXML_init_error(&saved_error);
        real_obj = LibXML_init_parser(self);
        recover = LibXML_get_recover(real_obj);

        /* create empty context */
        ctxt = xmlCreatePushParserCtxt( NULL, NULL, NULL, 0, NULL );
#if LIBXML_VERSION > 20600
        {
                SV** item =  hv_fetch( real_obj, "XML_LIBXML_NSCLEAN", 18, 0 );
                if ( item != NULL && SvTRUE(*item) ) {
                    ctxt->options |= XML_PARSE_NSCLEAN;
                }
        }
#endif
        if ( with_sax == 1 ) {
            PmmSAXInitContext( ctxt, self );
        }

        RETVAL = PmmContextSv( ctxt );

        LibXML_cleanup_callbacks();
        LibXML_cleanup_parser();
        LibXML_report_error(saved_error, recover);
    OUTPUT:
        RETVAL

int
_push(self, pctxt, data)
        SV * self
        SV * pctxt
        SV * data
    PREINIT:
        SV * saved_error;
        HV * real_obj;
        int recover = 0;
        xmlParserCtxtPtr ctxt = NULL;
        STRLEN len = 0;
        char * chunk = NULL;
    INIT:
        ctxt = PmmSvContext( pctxt );
        if ( ctxt == NULL ) {
            croak( "parser context already freed" );
            XSRETURN_UNDEF;
        }
        if ( data == &PL_sv_undef ) {
            XSRETURN_UNDEF;
        }
        chunk = SvPV( data, len );
        if ( len <= 0 ) {
            xs_warn( "empty string" );
            XSRETURN_UNDEF;
        }
    CODE:
        RETVAL = 0;
        LibXML_init_error(&saved_error);
        real_obj = LibXML_init_parser(self);
        recover = LibXML_get_recover(real_obj);

        xmlParseChunk(ctxt, (const char *)chunk, len, 0);

        LibXML_cleanup_callbacks();
        LibXML_cleanup_parser();
        LibXML_report_error(saved_error, recover);

        if ( ctxt->wellFormed == 0 ) {
            croak( "XML not well-formed in xmlParseChunk" );
            XSRETURN_UNDEF;
        }
        RETVAL = 1;
    OUTPUT:
        RETVAL

SV*
_end_push(self, pctxt, restore)
        SV * self
        SV * pctxt
        int restore
    PREINIT:
        SV * saved_error;
        HV * real_obj;
        int well_formed;
        xmlParserCtxtPtr ctxt = NULL;
        xmlDocPtr real_doc = NULL;
    INIT:
        ctxt = PmmSvContext( pctxt );
        if ( ctxt == NULL ) {
            croak( "parser context already freed" );
            XSRETURN_UNDEF;
        }
    CODE:
        RETVAL = &PL_sv_undef;
        LibXML_init_error(&saved_error);
        real_obj = LibXML_init_parser(self);

        xmlParseChunk(ctxt, "", 0, 1); /* finish the parse */
        xs_warn( "Finished with push parser\n" );

        well_formed = ctxt->wellFormed;
        real_doc = ctxt->myDoc;
        ctxt->myDoc = NULL;
        xmlFreeParserCtxt(ctxt);
        PmmNODE( SvPROXYNODE( pctxt ) ) = NULL;

        if ( real_doc != NULL ) {
            if ( restore || well_formed ) {
                RETVAL = LibXML_NodeToSv( real_obj, (xmlNodePtr) real_doc );
            } else {
                xmlFreeDoc(real_doc);
                real_doc = NULL;
            }
        }

        LibXML_cleanup_callbacks();
        LibXML_cleanup_parser();
        LibXML_report_error(saved_error, restore);

        if ( real_doc == NULL ){
            croak( "no document found!" );
            XSRETURN_UNDEF;
        }
    OUTPUT:
        RETVAL

void
_end_sax_push(self, pctxt)
        SV * self
        SV * pctxt
    PREINIT:
        SV * saved_error;
        HV * real_obj;
        xmlParserCtxtPtr ctxt = NULL;
    INIT:
        ctxt = PmmSvContext( pctxt );
        if ( ctxt == NULL ) {
            croak( "parser context already freed" );
        }
    CODE:
        LibXML_init_error(&saved_error);
        real_obj = LibXML_init_parser(self);

        xmlParseChunk(ctxt, "", 0, 1); /* finish the parse */
        xs_warn( "Finished with SAX push parser\n" );

        xmlFree(ctxt->sax);
        ctxt->sax = NULL;
        PmmSAXCloseContext(ctxt);
        xmlFreeParserCtxt(ctxt);
        PmmNODE( SvPROXYNODE( pctxt ) ) = NULL;

        LibXML_cleanup_callbacks();
        LibXML_cleanup_parser();
        LibXML_report_error(saved_error, 0);

SV*
import_GDOME( dummy, sv_gdome, deep=1 )
        SV * dummy
        SV * sv_gdome
        int deep
    PREINIT:
        xmlNodePtr node  = NULL;
    INIT:
        RETVAL = &PL_sv_undef;
#ifndef XML_LIBXML_GDOME_SUPPORT
        croak( "GDOME Support not compiled" );
#endif
        if ( sv_gdome == NULL || sv_gdome == &PL_sv_undef ) {
            croak( "no XML::GDOME data found" );
        }
#ifdef XML_LIBXML_GDOME_SUPPORT
        else {
            GdomeNode* gnode = NULL;
            gnode = (GdomeNode*)SvIV((SV*)SvRV( sv_gdome ));
            if ( gnode == NULL ) {
                croak( "no XML::GDOME data found (datastructure empty)" );
            }

            node = gdome_xml_n_get_xmlNode( gnode );
            if ( node == NULL ) {
                croak( "no XML::LibXML node found in GDOME object" );
            }
        }
#endif
    CODE:
        if ( node->type == XML_NAMESPACE_DECL ) {
            const char * CLASS = "XML::LibXML::Namespace";
            RETVAL = sv_newmortal();
            RETVAL = sv_setref_pv( RETVAL,
                                   CLASS,
                                   (void*)xmlCopyNamespace((xmlNsPtr)node) );
        }
        else {
            RETVAL = PmmNodeToSv( PmmCloneNode( node, deep ), NULL );
        }
    OUTPUT:
        RETVAL


SV*
export_GDOME( dummy, sv_libxml, deep=1 )
        SV * dummy
        SV * sv_libxml
        int deep
    PREINIT:
        xmlNodePtr node  = NULL, retnode = NULL;
    INIT:
        RETVAL = &PL_sv_undef;
#ifndef XML_LIBXML_GDOME_SUPPORT
        croak( "GDOME Support not configured!" );
#endif
        if ( sv_libxml == NULL || sv_libxml == &PL_sv_undef ) {
            croak( "no XML::LibXML data found" );
        }
        node = PmmSvNode( sv_libxml );
        if ( node == NULL ) {
            croak( "no XML::LibXML data found (empty structure)" );
        }
    CODE:
        retnode = PmmCloneNode( node, deep );
        if ( retnode == NULL ) {
            croak( "Copy node failed" );
        }

        RETVAL =  PmmNodeToGdomeSv( retnode );
    OUTPUT:
        RETVAL


int
load_catalog( self, filename )
        SV * self
        SV * filename
    PREINIT:
        const char * fn = (const char *) Sv2C(filename, NULL);
    INIT:
        if ( fn == NULL || xmlStrlen( (xmlChar *)fn ) == 0 ) {
            croak( "cannot load catalog" );
        }
    CODE:
#ifdef LIBXML_CATALOG_ENABLED
        RETVAL = xmlLoadCatalog( fn );
#else
        XSRETURN_UNDEF;
#endif
    OUTPUT:
        RETVAL



int
_default_catalog( self, catalog )
        SV * self
        SV * catalog
    PREINIT:
#ifdef LIBXML_CATALOG_ENABLED
        xmlCatalogPtr catal = (xmlCatalogPtr)SvIV(SvRV(catalog));
#endif
    INIT:
        if ( catal == NULL ) {
            croak( "empty catalog" );
        }
    CODE:
        warn( "this feature is not implemented" );
        RETVAL = 0;
    OUTPUT:
        RETVAL

MODULE = XML::LibXML         PACKAGE = XML::LibXML::ParserContext

void
DESTROY( self )
        SV * self
    CODE:
        xs_warn( "DROP PARSER CONTEXT!" );
        PmmContextREFCNT_dec( SvPROXYNODE( self ) );


MODULE = XML::LibXML         PACKAGE = XML::LibXML::Document

SV *
_toString(self, format=0)
        xmlDocPtr self
        int format
    PREINIT:
        /* SV * saved_error; */
        xmlChar *result=NULL;
        int len=0;
        SV* internalFlag = NULL;
        int oldTagFlag = xmlSaveNoEmptyTags;
        xmlDtdPtr intSubset = NULL;
    CODE:
        RETVAL = &PL_sv_undef;
        internalFlag = perl_get_sv("XML::LibXML::setTagCompression", 0);
        if( internalFlag ) {
            xmlSaveNoEmptyTags = SvTRUE(internalFlag);
        }

        internalFlag = perl_get_sv("XML::LibXML::skipDTD", 0);
        if ( internalFlag && SvTRUE(internalFlag) ) {
            intSubset = xmlGetIntSubset( self );
            if ( intSubset )
                xmlUnlinkNode( (xmlNodePtr)intSubset );
        }

        /* LibXML_init_error(&saved_error); */

        if ( format <= 0 ) {
            xs_warn( "use no formated toString!" );
            xmlDocDumpMemory(self, &result, &len);
        }
        else {
            int t_indent_var = xmlIndentTreeOutput;
            xs_warn( "use formated toString!" );
            xmlIndentTreeOutput = 1;
            xmlDocDumpFormatMemory( self, &result, &len, format );
            xmlIndentTreeOutput = t_indent_var;
        }

        if ( intSubset != NULL ) {
            if (self->children == NULL) {
                xmlAddChild((xmlNodePtr) self, (xmlNodePtr) intSubset);
            }
            else {
                xmlAddPrevSibling(self->children, (xmlNodePtr) intSubset);
            }
        }

        xmlSaveNoEmptyTags = oldTagFlag;

        /* LibXML_report_error(saved_error, 0); */

        if (result == NULL) {
            xs_warn("Failed to convert doc to string");
            XSRETURN_UNDEF;
        } else {
            /* warn("%s, %d\n",result, len); */
            RETVAL = C2Sv( result, self->encoding );
            xmlFree(result);
        }
    OUTPUT:
        RETVAL

int
toFH( self, filehandler, format=0 )
        xmlDocPtr self
        SV * filehandler
        int format
    PREINIT:
        SV * saved_error;
        xmlOutputBufferPtr buffer;
        const xmlChar * encoding = NULL;
        xmlCharEncodingHandlerPtr handler = NULL;
        SV* internalFlag = NULL;
        int oldTagFlag = xmlSaveNoEmptyTags;
        xmlDtdPtr intSubset = NULL;
        int t_indent_var = xmlIndentTreeOutput;
    CODE:
        internalFlag = perl_get_sv("XML::LibXML::setTagCompression", 0);
        if( internalFlag ) {
            xmlSaveNoEmptyTags = SvTRUE(internalFlag);
        }

        internalFlag = perl_get_sv("XML::LibXML::skipDTD", 0);
        if ( internalFlag && SvTRUE(internalFlag) ) {
            intSubset = xmlGetIntSubset( self );
            if ( intSubset )
                xmlUnlinkNode( (xmlNodePtr)intSubset );
        }

        xmlRegisterDefaultOutputCallbacks();
        encoding = (self)->encoding;
        if ( encoding != NULL ) {
            if ( xmlParseCharEncoding((const char*)encoding) != XML_CHAR_ENCODING_UTF8) {
                handler = xmlFindCharEncodingHandler((const char*)encoding);
            }

        }
        else {
            xs_warn("no encoding?");
        }

        buffer = xmlOutputBufferCreateIO( (xmlOutputWriteCallback) &LibXML_output_write_handler,
                                          (xmlOutputCloseCallback)&LibXML_output_close_handler,
                                          filehandler,
                                          handler );

        if ( format <= 0 ) {
            format = 0;
            xmlIndentTreeOutput = 0;
        }
        else {
            xmlIndentTreeOutput = 1;
        }

        LibXML_init_error(&saved_error);

        RETVAL = xmlSaveFormatFileTo( buffer,
                                      self,
                                      (const char *) encoding,
                                      format);

        if ( intSubset != NULL ) {
            if (self->children == NULL) {
                xmlAddChild((xmlNodePtr) self, (xmlNodePtr) intSubset);
            }
            else {
                xmlAddPrevSibling(self->children, (xmlNodePtr) intSubset);
            }
        }

        xmlIndentTreeOutput = t_indent_var;
        xmlSaveNoEmptyTags = oldTagFlag;

        LibXML_report_error(saved_error, 0);
    OUTPUT:
        RETVAL

int
toFile( self, filename, format=0 )
        xmlDocPtr self
        char * filename
        int format
    PREINIT:
        SV * saved_error;
        SV* internalFlag = NULL;
        int oldTagFlag = xmlSaveNoEmptyTags;
    CODE:
        internalFlag = perl_get_sv("XML::LibXML::setTagCompression", 0);
        if( internalFlag ) {
            xmlSaveNoEmptyTags = SvTRUE(internalFlag);
        }

        LibXML_init_error(&saved_error);

        if ( format <= 0 ) {
            xs_warn( "use no formated toFile!" );
            RETVAL = xmlSaveFile( filename, self );
        }
        else {
            int t_indent_var = xmlIndentTreeOutput;
            xmlIndentTreeOutput = 1;
            RETVAL =xmlSaveFormatFile( filename,
                                       self,
                                       format);
            xmlIndentTreeOutput = t_indent_var;
        }

        xmlSaveNoEmptyTags = oldTagFlag;

        LibXML_report_error(saved_error, 0);

        if ( RETVAL > 0 )
            RETVAL = 1;
        else
            XSRETURN_UNDEF;
    OUTPUT:
        RETVAL

SV *
toStringHTML(self)
        xmlDocPtr self
    ALIAS:
       XML::LibXML::Document::serialize_html = 1
    PREINIT:
        SV * saved_error;
        xmlChar *result=NULL;
        STRLEN len = 0;
    CODE:
        xs_warn( "use no formated toString!" );
        LibXML_init_error(&saved_error);
        htmlDocDumpMemory(self, &result, (int*)&len);

        LibXML_report_error(saved_error, 0);

        if (result == NULL) {
            XSRETURN_UNDEF;
        } else {
            /* warn("%s, %d\n",result, len); */
            RETVAL = newSVpvn((char *)result, (STRLEN)len);
            xmlFree(result);
        }
    OUTPUT:
        RETVAL


const char *
URI( self )
        xmlDocPtr self
    CODE:
        RETVAL = (const char*)xmlStrdup(self->URL );
    OUTPUT:
        RETVAL

void
setBaseURI( self, new_URI )
        xmlDocPtr self
        char * new_URI
    CODE:
        if (new_URI) {
            xmlFree((xmlChar*)self->URL );
            self->URL = xmlStrdup((const xmlChar*)new_URI);
        }


SV*
createDocument( CLASS, version="1.0", encoding=NULL )
        char * CLASS
        char * version
        char * encoding
    ALIAS:
        XML::LibXML::Document::new = 1
    PREINIT:
        xmlDocPtr doc=NULL;
    CODE:
        doc = xmlNewDoc((const xmlChar*)version);
        if (encoding && *encoding != 0) {
            doc->encoding = (const xmlChar*)xmlStrdup((const xmlChar*)encoding);
        }
        RETVAL = PmmNodeToSv((xmlNodePtr)doc,NULL);
    OUTPUT:
        RETVAL

SV*
createInternalSubset( self, Pname, extID, sysID )
        xmlDocPtr self
        SV * Pname
        SV * extID
        SV * sysID
    PREINIT:
        xmlDtdPtr dtd = NULL;
        xmlChar * name = NULL;
        xmlChar * externalID = NULL;
        xmlChar * systemID = NULL;
    CODE:
        name = Sv2C( Pname, NULL );
        if ( name == NULL ) {
            XSRETURN_UNDEF;
        }

        externalID = Sv2C(extID, NULL);
        systemID   = Sv2C(sysID, NULL);

        dtd = xmlCreateIntSubset( self, name, externalID, systemID );
        xmlFree(externalID);
        xmlFree(systemID);
        xmlFree(name);
        if ( dtd ) {
            RETVAL = PmmNodeToSv( (xmlNodePtr)dtd, PmmPROXYNODE(self) );
        }
        else {
            XSRETURN_UNDEF;
        }
    OUTPUT:
        RETVAL

SV*
createExternalSubset( self, Pname, extID, sysID )
        xmlDocPtr self
        SV * Pname
        SV * extID
        SV * sysID
    PREINIT:
        xmlDtdPtr dtd = NULL;
        xmlChar * name = NULL;
        xmlChar * externalID = NULL;
        xmlChar * systemID = NULL;
    CODE:
        name = Sv2C( Pname, NULL );
        if ( name == NULL ) {
            XSRETURN_UNDEF;
        }

        externalID = Sv2C(extID, NULL);
        systemID   = Sv2C(sysID, NULL);

        dtd = xmlNewDtd( self, name, externalID, systemID );

        xmlFree(externalID);
        xmlFree(systemID);
        xmlFree(name);
        if ( dtd ) {
            RETVAL = PmmNodeToSv( (xmlNodePtr)dtd, PmmPROXYNODE(self) );
        }
        else {
            XSRETURN_UNDEF;
        }
    OUTPUT:
        RETVAL

SV*
createDTD( self, Pname, extID, sysID )
        xmlDocPtr self
        SV * Pname
        SV * extID
        SV * sysID
    PREINIT:
        xmlDtdPtr dtd = NULL;
        xmlChar * name = NULL;
        xmlChar * externalID = NULL;
        xmlChar * systemID = NULL;
    CODE:
        name = Sv2C( Pname, NULL );
        if ( name == NULL ) {
            XSRETURN_UNDEF;
        }

        externalID = Sv2C(extID, NULL);
        systemID   = Sv2C(sysID, NULL);

        dtd = xmlNewDtd( NULL, name, externalID, systemID );
        dtd->doc = self;

        xmlFree(externalID);
        xmlFree(systemID);
        xmlFree(name);
        if ( dtd ) {
            RETVAL = PmmNodeToSv( (xmlNodePtr)dtd, PmmPROXYNODE(self) );
        }
        else {
            XSRETURN_UNDEF;
        }
    OUTPUT:
        RETVAL

SV*
createDocumentFragment( self )
        xmlDocPtr self
    CODE:
        RETVAL = PmmNodeToSv(xmlNewDocFragment(self), PmmPROXYNODE(self));
    OUTPUT:
        RETVAL

SV*
createElement( self, name )
        xmlDocPtr self
        SV* name
    PREINIT:
        xmlNodePtr newNode;
        xmlChar * elname = NULL;
        ProxyNodePtr docfrag = NULL;
    CODE:
        elname = nodeSv2C( name , (xmlNodePtr) self);
        if ( !LibXML_test_node_name( elname ) ) {
            xmlFree( elname );
            croak( "bad name" );
        }

        newNode = xmlNewNode(NULL , elname);
        xmlFree(elname);
        if ( newNode != NULL ) {
            docfrag = PmmNewFragment( self );
            newNode->doc = self;
            xmlAddChild(PmmNODE(docfrag), newNode);
            RETVAL = PmmNodeToSv(newNode,docfrag);
        }
        else {
            xs_warn( "no node created!" );
            XSRETURN_UNDEF;
        }
    OUTPUT:
        RETVAL

SV*
createRawElement( self, name )
        xmlDocPtr self
        SV* name
    PREINIT:
        xmlNodePtr newNode;
        xmlChar * elname = NULL;
        ProxyNodePtr docfrag = NULL;
    CODE:
        elname = nodeSv2C( name , (xmlNodePtr) self);
        if ( !elname || xmlStrlen(elname) <= 0 ) {
            xmlFree( elname );
            croak( "bad name" );
        }

        newNode = xmlNewDocNode(self,NULL , elname, NULL);
        xmlFree(elname);
        if ( newNode != NULL ) {
            docfrag = PmmNewFragment( self );
            xmlAddChild(PmmNODE(docfrag), newNode);
            RETVAL = PmmNodeToSv(newNode,docfrag);
        }
        else {
            xs_warn( "no node created!" );
            XSRETURN_UNDEF;
        }
    OUTPUT:
        RETVAL


SV*
createElementNS( self, nsURI, name )
        xmlDocPtr self
        SV * nsURI
        SV * name
    PREINIT:
        xmlChar * ename        = NULL;
        xmlChar * prefix       = NULL;
        xmlChar * localname    = NULL;
        xmlChar * eURI         = NULL;
        xmlNsPtr ns            = NULL;
        ProxyNodePtr docfrag   = NULL;
        xmlNodePtr newNode     = NULL;
    CODE:
        ename = nodeSv2C( name , (xmlNodePtr) self );
        if ( !LibXML_test_node_name( ename ) ) {
            xmlFree( ename );
            croak( "bad name" );
        }

        eURI  = Sv2C( nsURI , NULL );

        if ( eURI != NULL && xmlStrlen(eURI)!=0 ){
            localname = xmlSplitQName2(ename, &prefix);
            if ( localname == NULL ) {
                localname = xmlStrdup( ename );
            }

            newNode = xmlNewNode( NULL , localname );
            newNode->doc = self;

            ns = xmlSearchNsByHref( self, newNode, eURI );
            if ( ns == NULL ) {
                /* create a new NS if the NS does not already exists */
                ns = xmlNewNs(newNode, eURI , prefix );
            }

            if ( ns == NULL ) {
                xmlFreeNode( newNode );
                xmlFree(eURI);
                xmlFree(localname);
                if ( prefix != NULL ) {
                    xmlFree(prefix);
                }
                xmlFree(ename);
                XSRETURN_UNDEF;
            }

            xmlFree(localname);
        }
        else {
            xs_warn( " ordinary element " );
            /* ordinary element */
            localname = ename;

            newNode = xmlNewNode( NULL , localname );
            newNode->doc = self;
        }

        xmlSetNs(newNode, ns);
        docfrag = PmmNewFragment( self );
        xmlAddChild(PmmNODE(docfrag), newNode);
        RETVAL = PmmNodeToSv(newNode, docfrag);

        if ( prefix != NULL ) {
            xmlFree(prefix);
        }
        if ( eURI != NULL ) {
            xmlFree(eURI);
        }
        xmlFree(ename);
    OUTPUT:
        RETVAL


SV*
createRawElementNS( self, nsURI, name )
        xmlDocPtr self
        SV * nsURI
        SV * name
    PREINIT:
        xmlChar * ename        = NULL;
        xmlChar * prefix       = NULL;
        xmlChar * localname    = NULL;
        xmlChar * eURI         = NULL;
        xmlNsPtr ns            = NULL;
        ProxyNodePtr docfrag   = NULL;
        xmlNodePtr newNode     = NULL;
    CODE:
        ename = nodeSv2C( name , (xmlNodePtr) self );
        if ( !LibXML_test_node_name( ename ) ) {
            xmlFree( ename );
            croak( "bad name" );
        }

        eURI  = Sv2C( nsURI , NULL );

        if ( eURI != NULL && xmlStrlen(eURI)!=0 ){
            localname = xmlSplitQName2(ename, &prefix);
            if ( localname == NULL ) {
                localname = xmlStrdup( ename );
            }

            newNode = xmlNewDocNode( self,NULL , localname, NULL );

            ns = xmlSearchNsByHref( self, newNode, eURI );
            if ( ns == NULL ) {
                /* create a new NS if the NS does not already exists */
                ns = xmlNewNs(newNode, eURI , prefix );
            }

            if ( ns == NULL ) {
                xmlFreeNode( newNode );
                xmlFree(eURI);
                xmlFree(localname);
                if ( prefix != NULL ) {
                    xmlFree(prefix);
                }
                xmlFree(ename);
                XSRETURN_UNDEF;
            }

            xmlFree(localname);
        }
        else {
            xs_warn( " ordinary element " );
            /* ordinary element */
            localname = ename;

            newNode = xmlNewDocNode( self, NULL , localname, NULL );
        }

        xmlSetNs(newNode, ns);
        docfrag = PmmNewFragment( self );
        xmlAddChild(PmmNODE(docfrag), newNode);
        RETVAL = PmmNodeToSv(newNode, docfrag);

        if ( prefix != NULL ) {
            xmlFree(prefix);
        }
        if ( eURI != NULL ) {
            xmlFree(eURI);
        }
        xmlFree(ename);
    OUTPUT:
        RETVAL

SV *
createTextNode( self, content )
        xmlDocPtr self
        SV * content
    PREINIT:
        xmlNodePtr newNode;
        xmlChar * elname = NULL;
        ProxyNodePtr docfrag = NULL;
    CODE:
        elname = nodeSv2C( content , (xmlNodePtr) self );
        if ( elname != NULL || xmlStrlen(elname) > 0 ) {
            newNode = xmlNewDocText( self, elname );
            xmlFree(elname);
            if ( newNode != NULL ) {
                docfrag = PmmNewFragment( self );
                newNode->doc = self;
                xmlAddChild(PmmNODE(docfrag), newNode);
                RETVAL = PmmNodeToSv(newNode,docfrag);
            }
            else {
                xs_warn( "no node created!" );
                XSRETURN_UNDEF;
            }
        }
        else {
            XSRETURN_UNDEF;
        }
    OUTPUT:
        RETVAL

SV *
createComment( self , content )
        xmlDocPtr self
        SV * content
    PREINIT:
        xmlNodePtr newNode;
        xmlChar * elname = NULL;
        ProxyNodePtr docfrag = NULL;
    CODE:
        elname = nodeSv2C( content , (xmlNodePtr) self );
        if ( elname != NULL || xmlStrlen(elname) > 0 ) {
            newNode = xmlNewDocComment( self, elname );
            xmlFree(elname);
            if ( newNode != NULL ) {
                docfrag = PmmNewFragment( self );
                newNode->doc = self;
                xmlAddChild(PmmNODE(docfrag), newNode);
                xs_warn( newNode->name );
                RETVAL = PmmNodeToSv(newNode,docfrag);
            }
            else {
                xs_warn( "no node created!" );
                XSRETURN_UNDEF;
            }
        }
        else {
            XSRETURN_UNDEF;
        }
    OUTPUT:
        RETVAL

SV *
createCDATASection( self, content )
        xmlDocPtr self
        SV * content
    PREINIT:
        xmlNodePtr newNode;
        xmlChar * elname = NULL;
        ProxyNodePtr docfrag = NULL;
    CODE:
        elname = nodeSv2C( content , (xmlNodePtr)self );
        if ( elname != NULL || xmlStrlen(elname) > 0 ) {
            newNode = xmlNewCDataBlock( self, elname, xmlStrlen(elname) );
            xmlFree(elname);
            if ( newNode != NULL ) {
                docfrag = PmmNewFragment( self );
                newNode->doc = self;
                xmlAddChild(PmmNODE(docfrag), newNode);
                xs_warn( "[CDATA section]" );
                RETVAL = PmmNodeToSv(newNode,docfrag);
            }
            else {
                xs_warn( "no node created!" );
                XSRETURN_UNDEF;
            }
        }
        else {
            XSRETURN_UNDEF;
        }
    OUTPUT:
        RETVAL

SV*
createEntityReference( self , pname )
        xmlDocPtr self
        SV * pname
    PREINIT:
        xmlNodePtr newNode;
        xmlChar * name = Sv2C( pname, NULL );
        ProxyNodePtr docfrag = NULL;
    CODE:
        if ( name == NULL ) {
            XSRETURN_UNDEF;
        }
        newNode = xmlNewReference( self, name );
        xmlFree(name);
        if ( newNode == NULL ) {
            XSRETURN_UNDEF;
        }
        docfrag = PmmNewFragment( self );
        xmlAddChild(PmmNODE(docfrag), newNode);
        RETVAL = PmmNodeToSv( newNode, docfrag );
    OUTPUT:
        RETVAL

SV*
createAttribute( self, pname, pvalue=&PL_sv_undef )
        xmlDocPtr self
        SV * pname
        SV * pvalue
    PREINIT:
        xmlChar * name = NULL;
        xmlChar * value = NULL;
        xmlAttrPtr newAttr = NULL;
    CODE:
        name = nodeSv2C( pname , (xmlNodePtr) self );
        if ( !LibXML_test_node_name( name ) ) {
            xmlFree(name);
            XSRETURN_UNDEF;
        }

        value = nodeSv2C( pvalue , (xmlNodePtr) self );
        newAttr = xmlNewDocProp( self, name, value );
        RETVAL = PmmNodeToSv((xmlNodePtr)newAttr, NULL);

        xmlFree(name);
        if ( value ) {
            xmlFree(value);
        }
    OUTPUT:
        RETVAL

SV*
createAttributeNS( self, URI, pname, pvalue=&PL_sv_undef )
        xmlDocPtr self
        SV * URI
        SV * pname
        SV * pvalue
    PREINIT:
        xmlChar * name = NULL;
        xmlChar * value = NULL;
        xmlChar * prefix = NULL;
        const xmlChar * pchar = NULL;
        xmlChar * localname = NULL;
        xmlChar * nsURI = NULL;
        xmlAttrPtr newAttr = NULL;
        xmlNsPtr ns = NULL;
    CODE:
        name = nodeSv2C( pname , (xmlNodePtr) self );
        if ( !LibXML_test_node_name( name ) ) {
            xmlFree(name);
            XSRETURN_UNDEF;
        }

        nsURI = Sv2C( URI , NULL );
        value = nodeSv2C( pvalue, (xmlNodePtr) self  );

        if ( nsURI != NULL && xmlStrlen(nsURI) > 0 ) {
            xmlNodePtr root = xmlDocGetRootElement(self );
            if ( root ) {
                pchar = xmlStrchr(name, ':');
                if ( pchar != NULL ) {
                    localname = xmlSplitQName2(name, &prefix);
                }
                else {
                    localname = xmlStrdup( name );
                }
                ns = xmlSearchNsByHref( self, root, nsURI );
                if ( ns == NULL ) {
                    /* create a new NS if the NS does not already exists */
                    ns = xmlNewNs(root, nsURI , prefix );
                }

                if ( ns == NULL ) {
                    xmlFree(nsURI);
                    xmlFree(localname);
                    if ( prefix ) {
                        xmlFree(prefix);
                    }
                    xmlFree(name);
                    if ( value ) {
                        xmlFree(value);
                    }
                    XSRETURN_UNDEF;
                }

                newAttr = xmlNewDocProp( self, localname, value );
                newAttr->ns = ns;

                RETVAL = PmmNodeToSv((xmlNodePtr)newAttr, NULL );

                xmlFree(nsURI);
                xmlFree(name);
                if ( prefix ) {
                    xmlFree(prefix);
                }
                xmlFree(localname);
                if ( value ) {
                    xmlFree(value);
                }
            }
            else {
                croak( "can't create a new namespace on an attribute!" );
                xmlFree(name);
                if ( value ) {
                    xmlFree(value);
                }
                XSRETURN_UNDEF;
            }
        }
        else {
            newAttr = xmlNewDocProp( self, name, value );
            RETVAL = PmmNodeToSv((xmlNodePtr)newAttr,NULL);
            xmlFree(name);
            if ( value ) {
                xmlFree(value);
            }
        }
    OUTPUT:
        RETVAL

SV*
createProcessingInstruction(self, name, value=&PL_sv_undef)
        xmlDocPtr self
        SV * name
        SV * value
    ALIAS:
        createPI = 1
    PREINIT:
        xmlChar * n = NULL;
        xmlChar * v = NULL;
        xmlNodePtr pinode = NULL;
    CODE:
        n = nodeSv2C(name, (xmlNodePtr)self);
        if ( !n ) {
            XSRETURN_UNDEF;
        }
        v = nodeSv2C(value, (xmlNodePtr)self);
        pinode = xmlNewPI(n,v);
        pinode->doc = self;

        RETVAL = PmmNodeToSv(pinode,NULL);

        xmlFree(v);
        xmlFree(n);
    OUTPUT:
        RETVAL



void
_setDocumentElement( self , proxy )
        xmlDocPtr self
        SV * proxy
    PREINIT:
        xmlNodePtr elem, oelem;
    INIT:
        elem = PmmSvNode(proxy);
        if ( elem == NULL ) {
            XSRETURN_UNDEF;
        }
    CODE:
        /* please correct me if i am wrong: the document element HAS to be
         * an ELEMENT NODE
         */
        if ( elem->type == XML_ELEMENT_NODE ) {
            if ( self != elem->doc ) {
                domImportNode( self, elem, 1 );
            }

            oelem = xmlDocGetRootElement( self );
            if ( oelem == NULL || oelem->_private == NULL ) {
                xmlDocSetRootElement( self, elem );
            }
            else {
                ProxyNodePtr docfrag = PmmNewFragment( self );
                xmlReplaceNode( oelem, elem );
                xmlAddChild( PmmNODE(docfrag), oelem );
                PmmFixOwner( ((ProxyNodePtr)oelem->_private), docfrag);
            }

            if ( elem->_private != NULL ) {
                PmmFixOwner( SvPROXYNODE(proxy), PmmPROXYNODE(self));
            }
        }

SV *
documentElement( self )
        xmlDocPtr self
    ALIAS:
        XML::LibXML::Document::getDocumentElement = 1
    PREINIT:
        xmlNodePtr elem;
    CODE:
        elem = xmlDocGetRootElement( self );
        if ( elem ) {
            RETVAL = PmmNodeToSv(elem, PmmPROXYNODE(self));
        }
        else {
            XSRETURN_UNDEF;
        }
    OUTPUT:
        RETVAL

SV *
externalSubset( self )
        xmlDocPtr self
    PREINIT:
        xmlDtdPtr dtd;
    CODE:
        if ( self->extSubset == NULL ) {
            XSRETURN_UNDEF;
        }

        dtd = self->extSubset;
        RETVAL = PmmNodeToSv((xmlNodePtr)dtd, PmmPROXYNODE(self));
    OUTPUT:
        RETVAL

SV *
internalSubset( self )
        xmlDocPtr self
    PREINIT:
        xmlDtdPtr dtd;
    CODE:
        if ( self->intSubset == NULL ) {
            XSRETURN_UNDEF;
        }

        dtd = self->intSubset;
        RETVAL = PmmNodeToSv((xmlNodePtr)dtd, PmmPROXYNODE(self));
    OUTPUT:
        RETVAL

void
setExternalSubset( self, extdtd )
        xmlDocPtr self
        SV * extdtd
    PREINIT:
        xmlDtdPtr dtd = NULL;
        xmlDtdPtr olddtd = NULL;
    INIT:
        dtd = (xmlDtdPtr)PmmSvNode(extdtd);
        if ( dtd == NULL ) {
            croak( "lost DTD node" );
        }
    CODE:
        if ( dtd && dtd != self->extSubset ) {
            if ( dtd->doc == NULL ) {
                xmlSetTreeDoc( (xmlNodePtr) dtd, self );
            } else if ( dtd->doc != self ) {
                domImportNode( self, (xmlNodePtr) dtd,1); 
            }

            if ( dtd == self->intSubset ) {
                xmlUnlinkNode( (xmlNodePtr)dtd );
                self->intSubset = NULL;
            }

            olddtd = self->extSubset;
            if ( olddtd && olddtd->_private == NULL ) {
                xmlFreeDtd( olddtd );
            }
            self->extSubset = dtd;
        }

void
setInternalSubset( self, extdtd )
        xmlDocPtr self
        SV * extdtd
    PREINIT:
        xmlDtdPtr dtd = NULL;
        xmlDtdPtr olddtd = NULL;
    INIT:
        dtd = (xmlDtdPtr)PmmSvNode(extdtd);
        if ( dtd == NULL ) {
            croak( "lost DTD node" );
        }
    CODE:
        if ( dtd && dtd != self->intSubset ) {
            if ( dtd->doc != self ) {
                croak( "can't import DTDs" );
                domImportNode( self, (xmlNodePtr) dtd,1);
            }

            if ( dtd == self->extSubset ) {
                self->extSubset = NULL;
            }

            olddtd = xmlGetIntSubset( self );
            if( olddtd ) {
                xmlReplaceNode( (xmlNodePtr)olddtd, (xmlNodePtr) dtd );
                if ( olddtd->_private == NULL ) {
                    xmlFreeDtd( olddtd );
                }
            }
            else {
                if (self->children == NULL)
                    xmlAddChild((xmlNodePtr) self, (xmlNodePtr) dtd);
                else
                    xmlAddPrevSibling(self->children, (xmlNodePtr) dtd);
            }
            self->intSubset = dtd;
        }

SV *
removeInternalSubset( self )
        xmlDocPtr self
    PREINIT:
        xmlDtdPtr dtd = NULL;
    CODE:
        dtd = xmlGetIntSubset(self);
        if ( !dtd ) {
            XSRETURN_UNDEF;
        }
        xmlUnlinkNode( (xmlNodePtr)dtd );
        self->intSubset = NULL;
        RETVAL = PmmNodeToSv( (xmlNodePtr)dtd, PmmPROXYNODE(self) );
    OUTPUT:
        RETVAL

SV *
removeExternalSubset( self )
        xmlDocPtr self
    PREINIT:
        xmlDtdPtr dtd = NULL;
    CODE:
        dtd = self->extSubset;
        if ( !dtd ) {
            XSRETURN_UNDEF;
        }
        self->extSubset = NULL;
        RETVAL = PmmNodeToSv( (xmlNodePtr)dtd, PmmPROXYNODE(self) );
    OUTPUT:
        RETVAL

SV *
importNode( self, node, dummy=0 )
        xmlDocPtr self
        xmlNodePtr node
        int dummy
    PREINIT:
        xmlNodePtr ret = NULL;
        ProxyNodePtr docfrag = NULL;
    CODE:
        if ( node->type == XML_DOCUMENT_NODE
             || node->type == XML_HTML_DOCUMENT_NODE ) {
            croak( "Can't import Documents!" );
            XSRETURN_UNDEF;
        }

        ret = domImportNode( self, node, 0 );
        if ( ret ) {
            docfrag = PmmNewFragment( self );
            xmlAddChild( PmmNODE(docfrag), ret );
            RETVAL = PmmNodeToSv( ret, docfrag);
        }
        else {
            XSRETURN_UNDEF;
        }
    OUTPUT:
        RETVAL

SV *
adoptNode( self, node )
        xmlDocPtr self
        xmlNodePtr node
    PREINIT:
        xmlNodePtr ret = NULL;
        ProxyNodePtr docfrag = NULL;
    CODE:
        if ( node->type == XML_DOCUMENT_NODE
             || node->type == XML_HTML_DOCUMENT_NODE ) {
            croak( "Can't adopt Documents!" );
            XSRETURN_UNDEF;
        }

        ret = domImportNode( self, node, 1 );

        if ( ret ) {
            docfrag = PmmNewFragment( self );
            RETVAL = PmmNodeToSv(node, docfrag);
            xmlAddChild( PmmNODE(docfrag), ret );
            PmmFixOwner(SvPROXYNODE(RETVAL), docfrag);
        }
        else {
            XSRETURN_UNDEF;
        }
    OUTPUT:
        RETVAL

char*
encoding( self )
        xmlDocPtr self
    ALIAS:
        XML::LibXML::Document::getEncoding    = 1
        XML::LibXML::Document::actualEncoding = 2
    CODE:
        RETVAL = (char*)xmlStrdup(self->encoding );
    OUTPUT:
        RETVAL

void
setEncoding( self, encoding )
        xmlDocPtr self
        char *encoding
    PREINIT:
        int charset = XML_CHAR_ENCODING_ERROR;
    CODE:
        if ( self->encoding != NULL ) {
            xmlFree( (xmlChar*) self->encoding );
        }
        self->encoding = xmlStrdup( (const xmlChar *)encoding );
        charset = (int)xmlParseCharEncoding( (const char*)self->encoding );
        if ( charset > 0 ) {
            ((ProxyNodePtr)self->_private)->encoding = charset;
        }
        else {
            ((ProxyNodePtr)self->_private)->encoding = XML_CHAR_ENCODING_ERROR;
        }


int
standalone( self )
        xmlDocPtr self
    CODE:
        RETVAL = self->standalone;
    OUTPUT:
        RETVAL

void
setStandalone( self, value = 0 )
        xmlDocPtr self
        int value
    CODE:
        if ( value > 0 ) {
            self->standalone = 1;
        }
        else if ( value < 0 ) {
            self->standalone = -1;
        }
        else {
            self->standalone = 0;
        }

char*
version( self )
         xmlDocPtr self
    ALIAS:
        XML::LibXML::Document::getVersion = 1
    CODE:
        RETVAL = (char*)xmlStrdup(self->version );
    OUTPUT:
        RETVAL

void
setVersion( self, version )
        xmlDocPtr self
        char *version
    CODE:
        if ( self->version != NULL ) {
            xmlFree( (xmlChar*) self->version );
        }
        self->version = xmlStrdup( (const xmlChar*)version );

int
compression( self )
        xmlDocPtr self
    CODE:
        RETVAL = xmlGetDocCompressMode(self);
    OUTPUT:
        RETVAL

void
setCompression( self, zLevel )
        xmlDocPtr self
        int zLevel
    CODE:
        xmlSetDocCompressMode(self, zLevel);


int
is_valid(self, ...)
        xmlDocPtr self
    PREINIT:
        SV * saved_error;
        xmlValidCtxt cvp;
        xmlDtdPtr dtd;
        SV * dtd_sv;
    CODE:
        LibXML_init_error(&saved_error);

        cvp.userData = (void*)PerlIO_stderr();
        cvp.error = (xmlValidityErrorFunc)LibXML_validity_error;
        cvp.warning = (xmlValidityWarningFunc)LibXML_validity_warning;

        /* we need to initialize the node stack, because perl might
         * already have messed it up.
         */
        cvp.nodeNr = 0;
        cvp.nodeTab = NULL;
        cvp.vstateNr = 0;
        cvp.vstateTab = NULL;

        if (items > 1) {
            dtd_sv = ST(1);
            if ( sv_isobject(dtd_sv) && (SvTYPE(SvRV(dtd_sv)) == SVt_PVMG) ) {
                dtd = (xmlDtdPtr)PmmSvNode(dtd_sv);
            }
            RETVAL = xmlValidateDtd(&cvp, self, dtd);
        }
        else {
            RETVAL = xmlValidateDocument(&cvp, self);
        }

        /* LibXML_report_error(saved_error, 1); */
    OUTPUT:
        RETVAL

int
validate(self, ...)
        xmlDocPtr self
    PREINIT:
        SV * saved_error;
        xmlValidCtxt cvp;
        xmlDtdPtr dtd;
        SV * dtd_sv;
    CODE:
        LibXML_init_error(&saved_error);

        cvp.userData = (void*)PerlIO_stderr();
        cvp.error = (xmlValidityErrorFunc)LibXML_validity_error;
        cvp.warning = (xmlValidityWarningFunc)LibXML_validity_warning;
        /* we need to initialize the node stack, because perl might
         * already have messed it up.
         */
        cvp.nodeNr = 0;
        cvp.nodeTab = NULL;
        cvp.vstateNr = 0;
        cvp.vstateTab = NULL;

        if (items > 1) {
            dtd_sv = ST(1);
            if ( sv_isobject(dtd_sv) && (SvTYPE(SvRV(dtd_sv)) == SVt_PVMG) ) {
                dtd = (xmlDtdPtr)PmmSvNode(dtd_sv);
            }
            else {
                croak("is_valid: argument must be a DTD object");
            }
            RETVAL = xmlValidateDtd(&cvp, self , dtd);
        }
        else {
            RETVAL = xmlValidateDocument(&cvp, self);
        }

        LibXML_report_error(saved_error, RETVAL);
    OUTPUT:
        RETVAL

SV*
cloneNode( self, deep=0 ) 
        xmlDocPtr self
        int deep
    PREINIT:
        xmlDocPtr ret = NULL;
    CODE:
        ret = xmlCopyDoc( self, deep );
        if ( ret == NULL ) {
            XSRETURN_UNDEF;
        }
        RETVAL = PmmNodeToSv((xmlNodePtr)ret, NULL); 
    OUTPUT:
        RETVAL

int
indexElements ( self )
        xmlDocPtr self
     CODE:
#if LIBXML_VERSION >= 20508
        RETVAL = xmlXPathOrderDocElems( self );
#else
        RETVAL = -2;
#endif
     OUTPUT:
        RETVAL

MODULE = XML::LibXML         PACKAGE = XML::LibXML::Node

void
DESTROY( node )
        SV * node
    CODE:
        xs_warn("DESTROY PERL NODE\n");
        PmmREFCNT_dec(SvPROXYNODE(node));

SV*
nodeName( self )
        xmlNodePtr self
    ALIAS:
        XML::LibXML::Node::getName = 1
        XML::LibXML::Element::tagName = 2
    PREINIT:
        xmlChar * name = NULL;
    INIT:
        if( self->name == NULL ) {
            croak( "lost the name!?" );
        }
    CODE:
        name =  (xmlChar*)domName( self );
        if ( name != NULL ) {
            RETVAL = C2Sv(name,NULL);
            xmlFree( name );
        }
        else {
            XSRETURN_UNDEF;
        }
    OUTPUT:
        RETVAL

SV*
localname( self )
        xmlNodePtr self
    ALIAS:
        XML::LibXML::Node::getLocalName = 1
        XML::LibXML::Attr::name         = 2
        XML::LibXML::Node::localName    = 3
    CODE:
        if (    self->type == XML_ELEMENT_NODE
             || self->type == XML_ATTRIBUTE_NODE
             || self->type == XML_ELEMENT_DECL
             || self->type == XML_ATTRIBUTE_DECL ) {
            RETVAL = C2Sv(self->name,NULL);
        }
        else {
            XSRETURN_UNDEF;
        }
    OUTPUT:
        RETVAL

SV*
prefix( self )
        xmlNodePtr self
    ALIAS:
        XML::LibXML::Node::getPrefix = 1
    CODE:
        if( self->ns != NULL
            && self->ns->prefix != NULL ) {
            RETVAL = C2Sv(self->ns->prefix, NULL);
        }
        else {
            XSRETURN_UNDEF;
        }
    OUTPUT:
        RETVAL

SV*
namespaceURI( self )
        xmlNodePtr self
    ALIAS:
        getNamespaceURI = 1
    PREINIT:
        xmlChar * nsURI;
    CODE:
        if ( self->ns != NULL
             && self->ns->href != NULL ) {
            nsURI =  xmlStrdup(self->ns->href);
            RETVAL = C2Sv( nsURI, NULL );
            xmlFree( nsURI );
        }
        else {
            XSRETURN_UNDEF;
        }
    OUTPUT:
        RETVAL


SV*
lookupNamespaceURI( self, svprefix=&PL_sv_undef )
        xmlNodePtr self
        SV * svprefix
    PREINIT:
        xmlChar * nsURI;
        xmlChar * prefix = NULL;
    CODE:
        prefix = nodeSv2C( svprefix , self );
        if ( prefix != NULL && xmlStrlen(prefix) > 0) {
            xmlNsPtr ns = xmlSearchNs( self->doc, self, prefix );
            xmlFree( prefix );
            if ( ns != NULL ) {
                nsURI = xmlStrdup(ns->href);
                RETVAL = C2Sv( nsURI, NULL );
                xmlFree( nsURI );
            }
            else {
                XSRETURN_UNDEF;
            }
        }
        else {
            XSRETURN_UNDEF;
        }
    OUTPUT:
        RETVAL

SV*
lookupNamespacePrefix( self, svuri )
        xmlNodePtr self
        SV * svuri
    PREINIT:
        xmlChar * nsprefix;
        xmlChar * href = NULL;
    CODE:
        href = nodeSv2C( svuri , self );
        if ( href != NULL && xmlStrlen(href) > 0) {
            xmlNsPtr ns = xmlSearchNsByHref( self->doc, self, href );
            xmlFree( href );
            if ( ns != NULL ) {
                nsprefix = xmlStrdup( ns->prefix );
                RETVAL = C2Sv( nsprefix, NULL );
                xmlFree(nsprefix);
            }
            else {
                XSRETURN_UNDEF;
            }
        }
        else {
            XSRETURN_UNDEF;
        }
    OUTPUT:
        RETVAL

void
setNodeName( self , value )
        xmlNodePtr self
        SV* value
    ALIAS:
        setName = 1
    PREINIT:
        xmlChar* string;
        xmlChar* localname;
        xmlChar* prefix;
    CODE:
        string = nodeSv2C( value , self );
        if ( !LibXML_test_node_name( string ) ) {
            xmlFree(string);
            croak( "bad name" );
        }
        if( self->ns ){
            localname = xmlSplitQName2(string, &prefix);
            xmlNodeSetName(self, localname );
            xmlFree(localname);
            xmlFree(prefix);
        }
        else {
            xs_warn("node name normal\n");
            xmlNodeSetName(self, string );
        }
        xmlFree(string);

void
setRawName( self, value )
        xmlNodePtr self
        SV * value
    PREINIT:
        xmlChar* string;
        xmlChar* localname;
        xmlChar* prefix;
    CODE:
        string = nodeSv2C( value , self );
        if ( !string || xmlStrlen( string) <= 0 ) {
            xmlFree(string);
            XSRETURN_UNDEF;
        }
        if( self->ns ){
            localname = xmlSplitQName2(string, &prefix);
            xmlNodeSetName(self, localname );
            xmlFree(localname);
            xmlFree(prefix);
        }
        else {
            xmlNodeSetName(self, string );
        }
        xmlFree(string);


SV*
nodeValue( self, useDomEncoding = &PL_sv_undef )
        xmlNodePtr self
        SV * useDomEncoding
    ALIAS:
        XML::LibXML::Attr::value     = 1
        XML::LibXML::Attr::getValue  = 2
        XML::LibXML::Text::data      = 3
        XML::LibXML::Node::getValue  = 4
        XML::LibXML::Node::getData   = 5
    PREINIT:
        xmlChar * content = NULL;
    CODE:
        content = domGetNodeValue( self );

        if ( content != NULL ) {
            if ( SvTRUE(useDomEncoding) ) {
                RETVAL = nodeC2Sv(content, self);
            }
            else {
                RETVAL = C2Sv(content, NULL);
            }
            xmlFree(content);
        }
        else {
            XSRETURN_UNDEF;
        }
    OUTPUT:
        RETVAL

int
nodeType( self )
        xmlNodePtr self
    ALIAS:
        XML::LibXML::Node::getType = 1
    CODE:
        RETVAL = self->type;
    OUTPUT:
        RETVAL

SV*
parentNode( self )
        xmlNodePtr self
    ALIAS:
        XML::LibXML::Attr::ownerElement    = 1
        XML::LibXML::Node::getParentNode   = 2
        XML::LibXML::Attr::getOwnerElement = 3
    CODE:
        RETVAL = PmmNodeToSv( self->parent,
                              PmmOWNERPO( PmmPROXYNODE(self) ) );
    OUTPUT:
        RETVAL

SV*
nextSibling( self )
        xmlNodePtr self
    ALIAS:
        getNextSibling = 1
    CODE:
        RETVAL = PmmNodeToSv( self->next,
                              PmmOWNERPO(PmmPROXYNODE(self)) );
    OUTPUT:
        RETVAL

SV*
previousSibling( self )
        xmlNodePtr self
    ALIAS:
        getPreviousSibling = 1
    CODE:
        RETVAL = PmmNodeToSv( self->prev,
                              PmmOWNERPO( PmmPROXYNODE(self) ) );
    OUTPUT:
        RETVAL

void
_childNodes( self )
        xmlNodePtr self
    ALIAS:
        XML::LibXML::Node::getChildnodes = 1
    PREINIT:
        xmlNodePtr cld;
        SV * element;
        int len = 0;
        int wantarray = GIMME_V;
    PPCODE:
        if ( self->type != XML_ATTRIBUTE_NODE ) {
            cld = self->children;
            xs_warn("childnodes start");
            while ( cld ) {
                if( wantarray != G_SCALAR ) {
                    element = PmmNodeToSv(cld, PmmOWNERPO(PmmPROXYNODE(self)) );
                    XPUSHs(sv_2mortal(element));
                }
                cld = cld->next;
                len++;
            }
        }
        if ( wantarray == G_SCALAR ) {
            XPUSHs(sv_2mortal(newSViv(len)) );
        }

SV*
firstChild( self )
        xmlNodePtr self
    ALIAS:
        getFirstChild = 1
    CODE:
        RETVAL = PmmNodeToSv( self->children,
                              PmmOWNERPO( PmmPROXYNODE(self) ) );
    OUTPUT:
        RETVAL

SV*
lastChild( self )
        xmlNodePtr self
    ALIAS:
        getLastChild = 1
    CODE:
        RETVAL = PmmNodeToSv( self->last,
                              PmmOWNERPO( PmmPROXYNODE(self) ) );
    OUTPUT:
        RETVAL

void
_attributes( self )
        xmlNodePtr self
    ALIAS:
        XML::LibXML::Node::getAttributes = 1
    PREINIT:
        xmlAttrPtr attr = NULL;
        xmlNsPtr ns = NULL;
        SV * element;
        int len=0;
        int wantarray = GIMME_V;
    PPCODE:
        if ( self->type != XML_ATTRIBUTE_NODE ) {
            attr      = self->properties;
            while ( attr != NULL ) {
                if ( wantarray != G_SCALAR ) {
                    element = PmmNodeToSv((xmlNodePtr)attr,
                                           PmmOWNERPO(PmmPROXYNODE(self)) );
                    XPUSHs(sv_2mortal(element));
                }
                attr = attr->next;
                len++;
            }

            ns = self->nsDef;
            while ( ns != NULL ) {
                const char * CLASS = "XML::LibXML::Namespace";
                if ( wantarray != G_SCALAR ) {
                    /* namespace handling is kinda odd:
                     * as soon we have a namespace isolated from its
                     * owner, we loose the context. therefore it is
                     * forbidden to access the NS information directly.
                     * instead the use will recieve a copy of the real
                     * namespace, that can be destroied and is not
                     * bound to a document.
                     *
                     * this avoids segfaults in the end.
                     */
                    xmlNsPtr tns = xmlCopyNamespace(ns);
                    if ( tns != NULL ) {
                        element = sv_newmortal();
                        XPUSHs(sv_setref_pv( element,
                                             (char *)CLASS,
                                             (void*)tns));
                    }
                }
                ns = ns->next;
                len++;
            }
        }
        if( wantarray == G_SCALAR ) {
            XPUSHs( sv_2mortal(newSViv(len)) );
        }

int
hasChildNodes( self )
        xmlNodePtr self
    CODE:
        if ( self->type == XML_ATTRIBUTE_NODE ) {
            RETVAL = 0;
        }
        else {
            RETVAL =  self->children ? 1 : 0 ;
        }
    OUTPUT:
        RETVAL

int
hasAttributes( self )
        xmlNodePtr self
    CODE:
        if ( self->type == XML_ATTRIBUTE_NODE ) {
            RETVAL = 0;
        }
        else {
            RETVAL =  self->properties ? 1 : 0 ;
        }
    OUTPUT:
        RETVAL

SV*
ownerDocument( self )
        xmlNodePtr self
    ALIAS:
        XML::LibXML::Node::getOwnerDocument = 1
    CODE:
        xs_warn( "GET OWNERDOC\n" );
        if( self != NULL
            && self->doc != NULL ){
            RETVAL = PmmNodeToSv((xmlNodePtr)(self->doc), NULL);
        }
        else {
            XSRETURN_UNDEF;
        }
    OUTPUT:
        RETVAL

SV*
ownerNode( self )
        xmlNodePtr self
    ALIAS:
        XML::LibXML::Node::getOwner = 1
        XML::LibXML::Node::getOwnerElement = 2
    CODE:
        RETVAL = PmmNodeToSv(PmmNODE(PmmOWNERPO(PmmPROXYNODE(self))), NULL);
    OUTPUT:
        RETVAL


int
normalize( self )
        xmlNodePtr self
    CODE:
        RETVAL = domNodeNormalize( self );
    OUTPUT:
        RETVAL


SV*
insertBefore( self, nNode, ref )
        xmlNodePtr self
        xmlNodePtr nNode
        SV * ref
    PREINIT:
        xmlNodePtr oNode=NULL, rNode;
    INIT:
        oNode = PmmSvNode(ref);
    CODE:
        if ( self->type    == XML_DOCUMENT_NODE
             && nNode->type == XML_ELEMENT_NODE ) {
            xs_warn( "NOT_SUPPORTED_ERR\n" );
            XSRETURN_UNDEF;
        }
        else {
            rNode = domInsertBefore( self, nNode, oNode );
            if ( rNode != NULL ) {
                RETVAL = PmmNodeToSv( rNode,
                                      PmmOWNERPO(PmmPROXYNODE(self)) );
                PmmFixOwner(PmmOWNERPO(SvPROXYNODE(RETVAL)),
                            PmmOWNERPO(PmmPROXYNODE(self)) );
            }
            else {
                 XSRETURN_UNDEF;
            }
        }
    OUTPUT:
        RETVAL

SV*
insertAfter( self, nNode, ref )
        xmlNodePtr self
        xmlNodePtr nNode
        SV* ref
    PREINIT:
        xmlNodePtr oNode = NULL, rNode;
    INIT:
        oNode = PmmSvNode(ref);
    CODE:
        if ( self->type    == XML_DOCUMENT_NODE
             && nNode->type == XML_ELEMENT_NODE ) {
            xs_warn( "NOT_SUPPORTED_ERR\n" );
            XSRETURN_UNDEF;
        }
        else {
            rNode = domInsertAfter( self, nNode, oNode );
            if ( rNode != NULL ) {
                RETVAL = PmmNodeToSv( rNode,
                                      PmmOWNERPO(PmmPROXYNODE(self)) );
                PmmFixOwner(PmmOWNERPO(SvPROXYNODE(RETVAL)),
                            PmmOWNERPO(PmmPROXYNODE(self)) );
            }
            else {
                XSRETURN_UNDEF;
            }
        }
    OUTPUT:
        RETVAL

SV*
replaceChild( self, nNode, oNode )
        xmlNodePtr self
        xmlNodePtr nNode
        xmlNodePtr oNode
    PREINIT:
        xmlNodePtr ret = NULL;
        ProxyNodePtr docfrag = NULL;
    CODE:
       if ( self->type == XML_DOCUMENT_NODE ) {
                switch ( nNode->type ) {
                case XML_ELEMENT_NODE:
                case XML_DOCUMENT_FRAG_NODE:
                case XML_TEXT_NODE:
                case XML_CDATA_SECTION_NODE:
                    XSRETURN_UNDEF;
                    break;
                default:
                    break;
                }
        }
        ret = domReplaceChild( self, nNode, oNode );
        if (ret == NULL) {
            XSRETURN_UNDEF;
        }
        else {
                docfrag = PmmNewFragment( self->doc );
                /* create document fragment */
                xmlAddChild( PmmNODE(docfrag), ret );
                RETVAL = PmmNodeToSv(ret, docfrag);

                if ( nNode->_private != NULL ) {
                    PmmFixOwner( PmmPROXYNODE(nNode),
                                 PmmOWNERPO(PmmPROXYNODE(self)) );
                }
                PmmFixOwner( SvPROXYNODE(RETVAL), docfrag );
        }
    OUTPUT:
        RETVAL

SV*
replaceNode( self,nNode )
        xmlNodePtr self
        xmlNodePtr nNode
    PREINIT:
        xmlNodePtr ret = NULL;
        ProxyNodePtr docfrag = NULL;
    CODE:
        if ( domIsParent( self, nNode ) == 1 ) {
            XSRETURN_UNDEF;
        }
        if ( self->doc != nNode->doc ) {
            domImportNode( self->doc, nNode, 1 );
        }

        if ( self->type != XML_ATTRIBUTE_NODE ) {
              ret = domReplaceChild( self->parent, nNode, self);
        }
        else {
             ret = xmlReplaceNode( self, nNode );
        }
        if ( ret ) {
            if ( ret->type == XML_ATTRIBUTE_NODE ) {
                docfrag = NULL;
            }
            else {
                /* create document fragment */
                docfrag = PmmNewFragment( self->doc );
                xmlAddChild( PmmNODE(docfrag), ret );
            }

            RETVAL = PmmNodeToSv(ret, docfrag);
            if ( nNode->_private != NULL ) {
                PmmFixOwner( PmmPROXYNODE(nNode),
                             PmmOWNERPO(PmmPROXYNODE(self)));
            }
            PmmFixOwner( SvPROXYNODE(RETVAL), docfrag );
        }
        else {
            croak( "replacement failed" );
            XSRETURN_UNDEF;
        }
    OUTPUT:
        RETVAL

SV*
removeChild( self, node )
        xmlNodePtr self
        xmlNodePtr node
    PREINIT:
        xmlNodePtr ret;
    CODE:
        ret = domRemoveChild( self, node );
        if (ret == NULL) {
            XSRETURN_UNDEF;
        }
        else {
                ProxyNodePtr docfrag = PmmNewFragment( ret->doc );
                xmlAddChild( PmmNODE(docfrag), ret );
                RETVAL = PmmNodeToSv(ret,NULL);
                PmmFixOwner( SvPROXYNODE(RETVAL), docfrag );
        }
    OUTPUT:
        RETVAL

void
removeChildNodes( self )
        xmlNodePtr self
    PREINIT:
        xmlNodePtr elem, fragment;
        ProxyNodePtr docfrag;
    CODE:
        docfrag  = PmmNewFragment( self->doc );
        fragment = PmmNODE( docfrag );
        elem = self->children;
        while ( elem ) {
            xmlUnlinkNode( elem );
            /* this following piece is the function of domAppendChild()
             * but in this special case we can avoid most of the logic of
             * that function.
             */
            if ( fragment->children != NULL ) {
                xs_warn("unlink node!\n");
                domAddNodeToList( elem, fragment->last, NULL );
            }
            else {
                fragment->children = elem;
                fragment->last     = elem;
                elem->parent= fragment;
            }
            PmmFixOwnerNode( elem, docfrag );
            elem = elem->next;
        }

        self->children = self->last = NULL;
        if ( PmmREFCNT(docfrag) <= 0 ) {
            xs_warn( "have not references left" );
            PmmREFCNT_inc( docfrag );
            PmmREFCNT_dec( docfrag );
        }

void
unbindNode( self )
        xmlNodePtr self
    ALIAS:
        XML::LibXML::Node::unlink = 1
        XML::LibXML::Node::unlinkNode = 2
    PREINIT:
        ProxyNodePtr docfrag     = NULL;
    CODE:
        if ( self->type != XML_DOCUMENT_NODE
             || self->type != XML_DOCUMENT_FRAG_NODE ) {
            xmlUnlinkNode( self );
            if ( self->type != XML_ATTRIBUTE_NODE ) {
                docfrag = PmmNewFragment( self->doc );
                xmlAddChild( PmmNODE(docfrag), self );
            }
            PmmFixOwner( PmmPROXYNODE(self), docfrag );
        }

SV*
appendChild( self, nNode )
        xmlNodePtr self
        xmlNodePtr nNode
    PREINIT:
        xmlNodePtr rNode;
    CODE:
        if (self->type == XML_DOCUMENT_NODE ) {
            /* NOT_SUPPORTED_ERR
             */
            switch ( nNode->type ) {
            case XML_ELEMENT_NODE:
            case XML_DOCUMENT_FRAG_NODE:
            case XML_TEXT_NODE:
            case XML_CDATA_SECTION_NODE:
                XSRETURN_UNDEF;
                break;
            default:
                break;
            }
        }

        rNode = domAppendChild( self, nNode );

        if ( rNode == NULL ) {
            XSRETURN_UNDEF;
        }

        RETVAL = PmmNodeToSv( nNode,
                              PmmOWNERPO(PmmPROXYNODE(self)) );
        PmmFixOwner( SvPROXYNODE(RETVAL), PmmPROXYNODE(self) );
    OUTPUT:
        RETVAL

SV*
addChild( self, nNode )
        xmlNodePtr self
        xmlNodePtr nNode
    PREINIT:
        xmlNodePtr retval = NULL;
        ProxyNodePtr proxy;
    CODE:
        xmlUnlinkNode(nNode);
        proxy = PmmPROXYNODE(nNode);
        retval = xmlAddChild( self, nNode );

        if ( retval == NULL ) {
            croak( "ERROR!\n" );
        }

        if ( retval != nNode ) {
            xs_warn( "node was lost during operation\n" );
            PmmNODE(proxy) = NULL;
        }

        RETVAL = PmmNodeToSv( retval,
                              PmmOWNERPO(PmmPROXYNODE(self)) );
        if ( retval != self ) {
            PmmFixOwner( SvPROXYNODE(RETVAL), PmmPROXYNODE(self) );
        }
    OUTPUT:
        RETVAL


SV*
addSibling( self, nNode )
        xmlNodePtr self
        xmlNodePtr nNode
    PREINIT:
        xmlNodePtr ret = NULL;
    CODE:
        if ( nNode->type == XML_DOCUMENT_FRAG_NODE ) {
            XSRETURN_UNDEF;
        }

        ret = xmlAddSibling( self, nNode );

        if ( ret ) {
            RETVAL = PmmNodeToSv(ret,NULL);
            PmmFixOwner( SvPROXYNODE(RETVAL), PmmOWNERPO(PmmPROXYNODE(self)) );
        }
        else {
            XSRETURN_UNDEF;
        }
    OUTPUT:
        RETVAL

SV*
cloneNode( self, deep=0 )
        xmlNodePtr self
        int deep
    PREINIT:
        xmlNodePtr ret;
        xmlDocPtr doc = NULL;
        ProxyNodePtr docfrag = NULL;
    CODE:
        ret = PmmCloneNode( self, deep );
        if ( ret == NULL ) {
            XSRETURN_UNDEF;
        }

        if ( ret->type  == XML_DTD_NODE ) {
            RETVAL = PmmNodeToSv(ret, NULL);
        }
        else {
            doc = self->doc;

            if ( doc != NULL ) {
                xmlSetTreeDoc(ret, doc);
            }

            docfrag = PmmNewFragment( doc );
            xmlAddChild( PmmNODE(docfrag), ret );
            RETVAL = PmmNodeToSv(ret, docfrag);
        }
    OUTPUT:
        RETVAL

int
isSameNode( self, oNode )
        xmlNodePtr self
        xmlNodePtr oNode
    ALIAS:
        XML::LibXML::Node::isEqual = 1
    CODE:
        RETVAL = ( self == oNode ) ? 1 : 0;
    OUTPUT:
        RETVAL

SV *
baseURI( self )
        xmlNodePtr self
    PREINIT:
        xmlChar * uri;
    CODE:
        uri = xmlNodeGetBase( self->doc, self );
        RETVAL = C2Sv( uri, NULL );
        xmlFree( uri );
    OUTPUT:
        RETVAL

void
setBaseURI( self, URI )
        xmlNodePtr self
        SV * URI
    PREINIT:
        xmlChar * uri;
    CODE:
        uri = nodeSv2C( URI, self );
        if ( uri != NULL ) {
            xmlNodeSetBase( self, uri );
        }

SV*
toString( self, format=0, useDomEncoding = &PL_sv_undef )
        xmlNodePtr self
        SV * useDomEncoding
        int format
    ALIAS:
        XML::LibXML::Node::serialize = 1
    PREINIT:
        xmlBufferPtr buffer;
        const xmlChar *ret = NULL;
        SV* internalFlag = NULL;
        int oldTagFlag = xmlSaveNoEmptyTags;
    CODE:
        internalFlag = perl_get_sv("XML::LibXML::setTagCompression", 0);

        if ( internalFlag ) {
            xmlSaveNoEmptyTags = SvTRUE(internalFlag);
        }
        buffer = xmlBufferCreate();

        if ( format <= 0 ) {
            xmlNodeDump( buffer,
                         self->doc,
                         self, 0, format);
        }
        else {
            int t_indent_var = xmlIndentTreeOutput;
            xmlIndentTreeOutput = 1;
            xmlNodeDump( buffer,
                         self->doc,
                         self, 0, format);
            xmlIndentTreeOutput = t_indent_var;
        }

        if ( xmlBufferLength(buffer) > 0 ) {
            ret = xmlBufferContent( buffer );
        }

        xmlSaveNoEmptyTags = oldTagFlag;

        if ( ret != NULL ) {
            if ( useDomEncoding != &PL_sv_undef && SvTRUE(useDomEncoding) ) {
                RETVAL = nodeC2Sv((xmlChar*)ret, PmmNODE(PmmPROXYNODE(self))) ;
            }
            else {
                RETVAL = C2Sv((xmlChar*)ret, NULL) ;
            }
            xmlBufferFree( buffer );
        }
        else {
            xmlBufferFree( buffer );
            xs_warn("Failed to convert doc to string");
            XSRETURN_UNDEF;
        }
    OUTPUT:
        RETVAL


SV *
toStringC14N(self, comments=0, xpath = &PL_sv_undef)
        xmlNodePtr self
        int comments
        SV * xpath
    PREINIT:
        /* SV * saved_error; */
        xmlChar *result               = NULL;
        xmlChar *nodepath             = NULL;
        xmlXPathContextPtr child_ctxt = NULL;
        xmlXPathObjectPtr child_xpath = NULL;
        xmlNodeSetPtr nodelist        = NULL;
        xmlNodePtr refNode            = NULL;
    INIT:
        /* due to how c14n is implemented, the nodeset it receives must
          include child nodes; ie, child nodes aren't assumed to be rendered.
          so we use an xpath expression to find all of the child nodes. */

        if ( self->doc == NULL ) {
            croak("Node passed to toStringC14N must be part of a document");
        }

        refNode = self;
    CODE:
        if ( xpath != NULL && xpath != &PL_sv_undef ) {
            nodepath = Sv2C( xpath, NULL );
        }

        if ( nodepath != NULL && xmlStrlen( nodepath ) == 0 ) {
            xmlFree( nodepath );
            nodepath = NULL;
        }

        if ( nodepath == NULL
             && self->type != XML_DOCUMENT_NODE
             && self->type != XML_HTML_DOCUMENT_NODE
             && self->type != XML_DOCB_DOCUMENT_NODE
           ) {
            if (comments)
                nodepath = xmlStrdup( "(.//node() | .//@* | .//namespace::*)" );
            else
                nodepath = xmlStrdup( "(.//node() | .//@* | .//namespace::*)[not(self::comment())]" );
        }

        if ( nodepath != NULL ) {
            if ( self->type == XML_DOCUMENT_NODE
                 || self->type == XML_HTML_DOCUMENT_NODE
                 || self->type == XML_DOCB_DOCUMENT_NODE ) {
                refNode = xmlDocGetRootElement( self->doc );
            }

            child_ctxt = xmlXPathNewContext(self->doc);
            if (!child_ctxt) {
                if ( nodepath != NULL ) {
                    xmlFree( nodepath );
                }
                croak("Failed to create xpath context");
            }

            child_ctxt->node = self;
            /* get the namespace information */
            if (self->type == XML_DOCUMENT_NODE) {
                child_ctxt->namespaces = xmlGetNsList( self->doc,
                                                       xmlDocGetRootElement( self->doc ) );
            }
            else {
                child_ctxt->namespaces = xmlGetNsList(self->doc, self);
            }
            child_ctxt->nsNr = 0;
            if (child_ctxt->namespaces != NULL) {
                while (child_ctxt->namespaces[child_ctxt->nsNr] != NULL)
                child_ctxt->nsNr++;
            }

            child_xpath = xmlXPathEval(nodepath, child_ctxt);
            if (child_xpath == NULL) {
                if (child_ctxt->namespaces != NULL) {
                    xmlFree( child_ctxt->namespaces );
                }
                xmlXPathFreeContext(child_ctxt);
                if ( nodepath != NULL ) {
                    xmlFree( nodepath );
                }
                croak("2 Failed to compile xpath expression");
            }

            nodelist = child_xpath->nodesetval;
            if ( nodelist == NULL ) {
                xmlFree( nodepath );
                xmlXPathFreeObject(child_xpath);
                if (child_ctxt->namespaces != NULL) {
                    xmlFree( child_ctxt->namespaces );
                }
                xmlXPathFreeContext(child_ctxt);
                croak( "cannot canonize empty nodeset!" );
            }
        }

        /* LibXML_init_error(&saved_error); */

        xmlC14NDocDumpMemory( self->doc,
                              nodelist,
                              0, NULL,
                              comments,
                              &result );

        if ( child_xpath ) {
            xmlXPathFreeObject(child_xpath);
        }
        if ( child_ctxt ) {
            if (child_ctxt->namespaces != NULL) {
                xmlFree( child_ctxt->namespaces );
            }
            xmlXPathFreeContext(child_ctxt);
        }
        if ( nodepath != NULL ) {
            xmlFree( nodepath );
        }

        /* LibXML_report_error(saved_error, 0); */

        if (result == NULL) {
             croak("Failed to convert doc to string in doc->toStringC14N");
        } else {
            RETVAL = C2Sv( result, NULL );
            xmlFree(result);
        }
    OUTPUT:
        RETVAL

SV*
string_value ( self, useDomEncoding = &PL_sv_undef )
        xmlNodePtr self
        SV * useDomEncoding
    ALIAS:
        to_literal = 1
        textContent = 2
    PREINIT:
         xmlChar * string = NULL;
    CODE:
        /* we can't just return a string, because of UTF8! */
        string = xmlXPathCastNodeToString(self);
        if ( SvTRUE(useDomEncoding) ) {
            RETVAL = nodeC2Sv(string,
                              self);
        }
        else {
            RETVAL = C2Sv(string,
                          NULL);
        }
        xmlFree(string);
    OUTPUT:
        RETVAL

double
to_number ( self )
        xmlNodePtr self
    CODE:
        RETVAL = xmlXPathCastNodeToNumber(self);
    OUTPUT:
        RETVAL


void
_find( pnode, pxpath )
        SV* pnode
        SV * pxpath
    PREINIT:
        SV * saved_error;
        xmlNodePtr node = PmmSvNode(pnode);
        ProxyNodePtr owner = NULL;
        xmlXPathObjectPtr found = NULL;
        xmlNodeSetPtr nodelist = NULL;
        STRLEN len = 0 ;
        xmlChar * xpath = nodeSv2C(pxpath, node);
    INIT:
        if ( node == NULL ) {
            croak( "lost node" );
        }
        if ( !(xpath && xmlStrlen(xpath)) ) {
            xs_warn( "bad xpath\n" );
            if ( xpath )
                xmlFree(xpath);
            croak( "empty XPath found" );
            XSRETURN_UNDEF;
        }
    PPCODE:
        if ( node->doc ) {
            domNodeNormalize( xmlDocGetRootElement( node->doc ) );
        }
        else {
            domNodeNormalize( PmmOWNER(SvPROXYNODE(pnode)) );
        }

        LibXML_init_error(&saved_error);

        found = domXPathFind( node, xpath );
        xmlFree( xpath );

        if (found) {
            switch (found->type) {
                case XPATH_NODESET:
                    /* return as a NodeList */
                    /* access ->nodesetval */
                    XPUSHs(sv_2mortal(newSVpv("XML::LibXML::NodeList", 0)));
                    nodelist = found->nodesetval;
                    if ( nodelist ) {
                        if ( nodelist->nodeNr > 0 ) {
                            int i;
                            const char * cls = "XML::LibXML::Node";
                            xmlNodePtr tnode;
                            SV * element;

                            owner = PmmOWNERPO(SvPROXYNODE(pnode));
                            len = nodelist->nodeNr;
                            for( i=0 ; i < len; i++){
                                /* we have to create a new instance of an
                                 * objectptr. and then
                                 * place the current node into the new
                                 * object. afterwards we can
                                 * push the object to the array!
                                 */
                                tnode = nodelist->nodeTab[i];

                                /* let's be paranoid */
                                if (tnode->type == XML_NAMESPACE_DECL) {
                                     xmlNsPtr newns = xmlCopyNamespace((xmlNsPtr)tnode);
                                    if ( newns != NULL ) {
                                        element = NEWSV(0,0);
                                        cls = PmmNodeTypeName( tnode );
                                        element = sv_setref_pv( element,
                                                                (const char *)cls,
                                                                (void*)newns
                                                          );
                                    }
                                    else {
                                        continue;
                                    }
                                }
                                else {
                                    element = PmmNodeToSv(tnode, owner);
                                }

                                XPUSHs( sv_2mortal(element) );
                            }
                        }
                        xmlXPathFreeNodeSet( found->nodesetval );
                        found->nodesetval = NULL;
                    }
                    break;
                case XPATH_BOOLEAN:
                    /* return as a Boolean */
                    /* access ->boolval */
                    XPUSHs(sv_2mortal(newSVpv("XML::LibXML::Boolean", 0)));
                    XPUSHs(sv_2mortal(newSViv(found->boolval)));
                    break;
                case XPATH_NUMBER:
                    /* return as a Number */
                    /* access ->floatval */
                    XPUSHs(sv_2mortal(newSVpv("XML::LibXML::Number", 0)));
                    XPUSHs(sv_2mortal(newSVnv(found->floatval)));
                    break;
                case XPATH_STRING:
                    /* access ->stringval */
                    /* return as a Literal */
                    XPUSHs(sv_2mortal(newSVpv("XML::LibXML::Literal", 0)));
                    XPUSHs(sv_2mortal(C2Sv(found->stringval, NULL)));
                    break;
                default:
                    croak("Unknown XPath return type");
            }
            xmlXPathFreeObject(found);
        }

        LibXML_report_error(saved_error, 0);

void
_findnodes( pnode, perl_xpath )
        SV* pnode
        SV * perl_xpath
    PREINIT:
        SV * saved_error;
        xmlNodePtr node = PmmSvNode(pnode);
        ProxyNodePtr owner = NULL;
        xmlNodeSetPtr nodelist = NULL;
        SV * element = NULL ;
        STRLEN len = 0 ;
        xmlChar * xpath = nodeSv2C(perl_xpath, node);
    INIT:
        if ( node == NULL ) {
            croak( "lost node" );
        }
        if ( !(xpath && xmlStrlen(xpath)) ) {
            xs_warn( "bad xpath\n" );
            if ( xpath )
                xmlFree(xpath);
            croak( "empty XPath found" );
            XSRETURN_UNDEF;
        }
    PPCODE:
        if ( node->doc ) {
            domNodeNormalize( xmlDocGetRootElement(node->doc ) );
        }
        else {
            domNodeNormalize( PmmOWNER(SvPROXYNODE(pnode)) );
        }

        LibXML_init_error(&saved_error);

        nodelist = domXPathSelect( node, xpath );
        xmlFree(xpath);

        if ( nodelist ) {
            if ( nodelist->nodeNr > 0 ) {
                int i;
                const char * cls = "XML::LibXML::Node";
                xmlNodePtr tnode;
                owner = PmmOWNERPO(SvPROXYNODE(pnode));
                len = nodelist->nodeNr;
                for(i=0 ; i < len; i++){
                    /* we have to create a new instance of an objectptr.
                     * and then place the current node into the new object.
                     * afterwards we can push the object to the array!
                     */
                    element = NULL;
                    tnode = nodelist->nodeTab[i];
                    if (tnode->type == XML_NAMESPACE_DECL) {
                        xmlNsPtr newns = xmlCopyNamespace((xmlNsPtr)tnode);
                        if ( newns != NULL ) {
                            element = NEWSV(0,0);
                            cls = PmmNodeTypeName( tnode );
                            element = sv_setref_pv( element,
                                                    (const char *)cls,
                                                    newns
                                                  );
                        }
                        else {
                            continue;
                        }
                    }
                    else {
                        element = PmmNodeToSv(tnode, owner);
                    }

                    XPUSHs( sv_2mortal(element) );
                }
            }
            xmlXPathFreeNodeSet( nodelist );
        }

        LibXML_report_error(saved_error, 0);

void
getNamespaces( pnode )
        SV * pnode
    ALIAS:
        namespaces = 1
    PREINIT:
        xmlNodePtr node;
        xmlNsPtr ns = NULL;
        xmlNsPtr newns = NULL;
        SV* element;
        const char * class = "XML::LibXML::Namespace";
    INIT:
        node = PmmSvNode(pnode);
        if ( node == NULL ) {
            croak( "lost node" );
        }
    PPCODE:
        ns = node->nsDef;
        while ( ns != NULL ) {
            newns = xmlCopyNamespace(ns);
            if ( newns != NULL ) {
                element = NEWSV(0,0);
                element = sv_setref_pv( element,
                                        (const char *)class,
                                        (void*)newns
                                      );
                XPUSHs( sv_2mortal(element) );
            }
            ns = ns->next;
        }

SV *
getNamespace( node )
        xmlNodePtr node
    ALIAS:
        localNamespace = 1
        localNS        = 2
    PREINIT:
        xmlNsPtr ns = NULL;
        xmlNsPtr newns = NULL;
        const char * class = "XML::LibXML::Namespace";
    CODE:
        ns = node->ns;
        if ( ns != NULL ) {
            newns = xmlCopyNamespace(ns);
            if ( newns != NULL ) {
                RETVAL = NEWSV(0,0);
                RETVAL = sv_setref_pv( RETVAL,
                                       (const char *)class,
                                       (void*)newns
                                      );
            }
        }
        else {
            XSRETURN_UNDEF;
        }
    OUTPUT:
        RETVAL


SV *
nodePath( self )
        xmlNodePtr self
    PREINIT:
        xmlChar * path = NULL;
    CODE:
        path = xmlGetNodePath( self );
        if ( path == NULL ) {
            croak( "cannot calculate path for the given node" );
        }
        RETVAL = nodeC2Sv( path, self );
    OUTPUT:
        RETVAL

int
line_number( self )
        xmlNodePtr self
    CODE:
        RETVAL = xmlGetLineNo( self );
    OUTPUT:
        RETVAL

MODULE = XML::LibXML         PACKAGE = XML::LibXML::Element

SV*
new(CLASS, name )
        char * CLASS
        char * name
    PREINIT:
        xmlNodePtr newNode;
        ProxyNodePtr docfrag;
    CODE:
        docfrag = PmmNewFragment(NULL);
        newNode = xmlNewNode( NULL, (const xmlChar*)name );
        newNode->doc = NULL;
        xmlAddChild(PmmNODE(docfrag), newNode);
        RETVAL = PmmNodeToSv(newNode, docfrag );
    OUTPUT:
        RETVAL

int
_setNamespace(self, namespaceURI, namespacePrefix = &PL_sv_undef, flag = 1 )
        SV * self
        SV * namespaceURI
        SV * namespacePrefix
        int flag
    PREINIT:
        xmlNodePtr node = PmmSvNode(self);
        xmlChar * nsURI = nodeSv2C(namespaceURI,node);
        xmlChar * nsPrefix = NULL;
        xmlNsPtr ns = NULL;
    INIT:
        if ( node == NULL ) {
            croak( "lost node" );
        }
    CODE:
        if ( !nsURI ){
            XSRETURN_UNDEF;
        }

        nsPrefix = nodeSv2C(namespacePrefix, node);
        if ( xmlStrlen( nsPrefix ) == 0 ) {
            xmlFree(nsPrefix);
            nsPrefix = NULL;
        }
        if ( (ns = xmlSearchNsByHref(node->doc, node, nsURI)) ) {
            if ( ns->prefix == nsPrefix               /* both are NULL then */
                 || xmlStrEqual( ns->prefix, nsPrefix ) ) {
                RETVAL = 1;
            }
            else if ( (ns = xmlNewNs( node, nsURI, nsPrefix )) ) {
                RETVAL = 1;
            }
            else {
                RETVAL = 0;
            }
        }
        else if ( (ns = xmlNewNs( node, nsURI, nsPrefix )) )
            RETVAL = 1;
        else
            RETVAL = 0;

        if ( flag && ns ) {
            node->ns = ns;
        }

        xmlFree(nsPrefix);
        xmlFree(nsURI);
    OUTPUT:
        RETVAL

int
hasAttribute( self, attr_name )
        xmlNodePtr self
        SV * attr_name
    PREINIT:
        xmlChar * name;
    CODE:
        name  = nodeSv2C(attr_name, self );
        if ( ! name ) {
            XSRETURN_UNDEF;
        }
        if ( xmlHasProp( self, name ) ) {
            RETVAL = 1;
        }
        else {
            RETVAL = 0;
        }
        xmlFree(name);
    OUTPUT:
        RETVAL

int
hasAttributeNS( self, namespaceURI, attr_name )
        xmlNodePtr self
        SV * namespaceURI
        SV * attr_name
    PREINIT:
        xmlChar * name;
        xmlChar * nsURI;
    CODE:
        name = nodeSv2C(attr_name, self );
        nsURI = nodeSv2C(namespaceURI, self );

        if ( !name ) {
            xmlFree(nsURI);
            XSRETURN_UNDEF;
        }
        if ( !nsURI ){
            xmlFree(name);
            XSRETURN_UNDEF;
        }
        if ( xmlHasNsProp( self, name, nsURI ) ) {
            RETVAL = 1;
        }
        else {
            RETVAL = 0;
        }

        xmlFree(name);
        xmlFree(nsURI);
    OUTPUT:
        RETVAL

SV*
getAttribute( self, attr_name, doc_enc = 0 )
        xmlNodePtr self
        SV * attr_name
        int doc_enc
    PREINIT:
        xmlChar * name;
        xmlChar * ret = NULL;
    CODE:
        name = nodeSv2C(attr_name, self );
        if( !name ) {
            XSRETURN_UNDEF;
        }

        ret = xmlGetProp(self, name);
        xmlFree(name);

        if ( ret ) {
            if ( doc_enc == 1 ) {
                RETVAL = nodeC2Sv(ret, self);
            }
            else {
                RETVAL = C2Sv(ret, NULL);
            }
            xmlFree( ret );
        }
        else {
            XSRETURN_UNDEF;
        }
    OUTPUT:
        RETVAL

void
_setAttribute( self, attr_name, attr_value )
        xmlNodePtr self
        SV * attr_name
        SV * attr_value
    PREINIT:
        xmlChar * name  = NULL;
        xmlChar * value = NULL;
    CODE:
        name  = nodeSv2C(attr_name, self );

        if ( !LibXML_test_node_name(name) ) {
            xmlFree(name);
            croak( "bad name" );
        }
        value = nodeSv2C(attr_value, self );

        xmlSetProp( self, name, value );
        xmlFree(name);
        xmlFree(value);


void
removeAttribute( self, attr_name )
        xmlNodePtr self
        SV * attr_name
    PREINIT:
        xmlChar * name;
        xmlAttrPtr xattr = NULL;
    CODE:
        name  = nodeSv2C(attr_name, self );
        if ( name ) {
            xattr = xmlHasProp( self, name );

            if ( xattr ) {
                xmlUnlinkNode((xmlNodePtr)xattr);
                if ( xattr->_private ) {
                    PmmFixOwner((ProxyNodePtr)xattr->_private, NULL);
                }
                else {
                    xmlFreeProp(xattr);
                }
            }
            xmlFree(name);
        }

SV*
getAttributeNode( self, attr_name )
        xmlNodePtr self
        SV * attr_name
    PREINIT:
        xmlChar * name;
        xmlAttrPtr ret = NULL;
    CODE:
        name = nodeSv2C(attr_name, self );
        if ( !name ) {
            XSRETURN_UNDEF;
        }

        ret = xmlHasProp( self, name );
        xmlFree(name);

        if ( ret ) {
            RETVAL = PmmNodeToSv( (xmlNodePtr)ret,
                                   PmmOWNERPO(PmmPROXYNODE(self)) );
        }
        else {
            XSRETURN_UNDEF;
        }
    OUTPUT:
        RETVAL

SV *
setAttributeNode( self, attr_node )
        xmlNodePtr self
        SV * attr_node
    PREINIT:
        xmlAttrPtr attr = (xmlAttrPtr)PmmSvNode( attr_node );
        xmlAttrPtr ret = NULL;
    INIT:
        if ( attr == NULL ) {
            croak( "lost attribute" );
        }
    CODE:
        if ( attr != NULL && attr->type != XML_ATTRIBUTE_NODE ) {
            XSRETURN_UNDEF;
        }
        if ( attr->doc != self->doc ) {
            domImportNode( self->doc, (xmlNodePtr)attr, 1);
        }
        ret = xmlHasProp( self, attr->name );
        if ( ret != NULL ) {
            if ( ret != attr ) {
                xmlReplaceNode( (xmlNodePtr)ret, (xmlNodePtr)attr );
            }
            else {
                XSRETURN_UNDEF;
            }
        }
        else {
            xmlAddChild( self, (xmlNodePtr)attr );
        }

        if ( attr->_private != NULL ) {
            PmmFixOwner( SvPROXYNODE(attr_node), PmmPROXYNODE(self) );
        }

        if ( ret == NULL ) {
            XSRETURN_UNDEF;
        }

        RETVAL = PmmNodeToSv( (xmlNodePtr)ret, NULL );
        PmmFixOwner( SvPROXYNODE(RETVAL), NULL );
    OUTPUT:
        RETVAL

SV *
getAttributeNS( self, namespaceURI, attr_name )
        xmlNodePtr self
        SV * namespaceURI
        SV * attr_name
    PREINIT:
        xmlChar * name;
        xmlChar * nsURI;
        xmlChar * ret = NULL;
    CODE:
        name = nodeSv2C( attr_name, self );
        nsURI = nodeSv2C( namespaceURI, self );
        if ( !name ) {
            xmlFree(nsURI);
            XSRETURN_UNDEF;
        }
        if ( nsURI && xmlStrlen(nsURI) ) {
            ret = xmlGetNsProp( self, name, nsURI );
        }
        else {
            ret = xmlGetProp( self, name );
        }

        xmlFree( name );
        if ( nsURI ) {
            xmlFree( nsURI );
        }
        if ( ret ) {
            RETVAL = nodeC2Sv( ret, self );
            xmlFree( ret );
        }
        else {
            XSRETURN_UNDEF;
        }
    OUTPUT:
        RETVAL

void
setAttributeNS( self, namespaceURI, attr_name, attr_value )
        xmlNodePtr self
        SV * namespaceURI
        SV * attr_name
        SV * attr_value
    PREINIT:
        xmlChar * nsURI;
        xmlChar * name  = NULL;
        xmlChar * value = NULL;
        xmlNsPtr ns         = NULL;
        xmlChar * localname = NULL;
        xmlChar * prefix    = NULL;
    INIT:
        name  = nodeSv2C( attr_name, self );

        if ( !LibXML_test_node_name(name) ) {
            xmlFree(name);
            croak( "bad name" );
        }

        nsURI = nodeSv2C( namespaceURI, self );
        localname = xmlSplitQName2(name, &prefix);
        if ( localname ) {
            xmlFree( name );
            name = localname;
        }
    CODE:
        value = nodeSv2C( attr_value, self );

        if ( nsURI && xmlStrlen(nsURI) ) {
            xs_warn( "found uri" );

            ns = xmlSearchNsByHref( self->doc, self, nsURI );
            if ( !ns ) {
                /* create new ns */
                 if ( prefix && xmlStrlen( prefix ) ) {
                    ns = xmlNewNs(self, nsURI , prefix );
                 }
                 else {
                    ns = NULL;
                 }
            }
            else if ( !ns->prefix ) {
                if ( ns->next && ns->next->prefix ) {
                    ns = ns->next;
                }
                else if ( prefix && xmlStrlen( prefix ) ) {
                    ns = xmlNewNs(self, nsURI , prefix );
                }
                else {
                    ns = NULL;
                }
            }
        }

        if ( nsURI && xmlStrlen(nsURI) && !ns ) {
            xs_warn( "bad ns attribute!" );
        }
        else {
            /* warn( "set attribute %s->%s", name, value ); */
            xmlSetNsProp( self, ns, name, value );
        }

        if ( prefix ) {
            xmlFree( prefix );
        }
        if ( nsURI ) {
            xmlFree( nsURI );
        }
        xmlFree( name );
        xmlFree( value );

void
removeAttributeNS( self, namespaceURI, attr_name )
        xmlNodePtr self
        SV * namespaceURI
        SV * attr_name
    PREINIT:
        xmlChar * nsURI;
        xmlChar * name  = NULL;
        xmlAttrPtr xattr = NULL;
    CODE:
        nsURI = nodeSv2C( namespaceURI, self );
        name  = nodeSv2C( attr_name, self );
        if ( ! name ) {
            xmlFree(nsURI);
            XSRETURN_UNDEF;
        }

        if ( nsURI && xmlStrlen(nsURI) ) {
            xattr = xmlHasNsProp( self, name, nsURI );
        }
        else {
            xattr = xmlHasNsProp( self, name, NULL );
        }
        if ( xattr ) {
            xmlUnlinkNode((xmlNodePtr)xattr);
            if ( xattr->_private ) {
                PmmFixOwner((ProxyNodePtr)xattr->_private, NULL);
            }
            else {
                xmlFreeProp(xattr);
            }
        }
        xmlFree(nsURI);
        xmlFree( name );


SV*
getAttributeNodeNS( self,namespaceURI, attr_name )
        xmlNodePtr self
        SV * namespaceURI
        SV * attr_name
    PREINIT:
        xmlChar * nsURI;
        xmlChar * name;
        xmlAttrPtr ret = NULL;
    CODE:
        nsURI = nodeSv2C(namespaceURI, self );
        name = nodeSv2C(attr_name, self );
        if ( !name ) {
            xmlFree(nsURI);
            XSRETURN_UNDEF;
        }
        if ( !nsURI ){
            xmlFree(name);
            XSRETURN_UNDEF;
        }

        ret = xmlHasNsProp( self, name, nsURI );
        xmlFree(name);
        xmlFree(nsURI);

        if ( ret ) {
            RETVAL = PmmNodeToSv( (xmlNodePtr)ret,
                                   PmmOWNERPO(PmmPROXYNODE(self)) );
        }
        else {
            /* warn("no prop\n"); */
            XSRETURN_UNDEF;
        }
    OUTPUT:
        RETVAL

SV *
setAttributeNodeNS( self, attr_node )
        xmlNodePtr self
        SV * attr_node
    PREINIT:
        xmlAttrPtr attr = (xmlAttrPtr)PmmSvNode( attr_node );
        xmlNsPtr ns = NULL;
        xmlAttrPtr ret = NULL;
    INIT:
        if ( attr == NULL ) {
            croak( "lost attribute node" );
        }
    CODE:
        if ( attr->type != XML_ATTRIBUTE_NODE ) {
            XSRETURN_UNDEF;
        }

        if ( attr->doc != self->doc ) {
            domImportNode( self->doc, (xmlNodePtr)attr, 1);
        }


        ns = attr->ns;
        if ( ns != NULL ) {
            ret = xmlHasNsProp( self, ns->href, attr->name );
        }
        else {
            ret = xmlHasProp( self, attr->name );
        }

        if ( ret != NULL ) {
            if ( ret != attr ) {
                xmlReplaceNode( (xmlNodePtr)ret, (xmlNodePtr)attr );
            }
            else {
                XSRETURN_UNDEF;
            }
        }
        else {
            xmlAddChild( self, (xmlNodePtr)attr );
            xmlReconciliateNs(self->doc, self);
        }
        if ( attr->_private != NULL ) {
            PmmFixOwner( SvPROXYNODE(attr_node), PmmPROXYNODE(self) );
        }
        if ( ret == NULL ) {
            XSRETURN_UNDEF;
        }
        RETVAL = PmmNodeToSv( (xmlNodePtr)ret, NULL );
        PmmFixOwner( SvPROXYNODE(RETVAL), NULL );
    OUTPUT:
        RETVAL

SV *
removeAttributeNode( self, attr_node )
        xmlNodePtr self
        SV * attr_node
    PREINIT:
        xmlAttrPtr attr = (xmlAttrPtr)PmmSvNode( attr_node );
        xmlAttrPtr ret;
    INIT:
        if ( attr == NULL ) {
            croak( "lost attribute node" );
        }
    CODE:
        if ( attr->type != XML_ATTRIBUTE_NODE ) {
            XSRETURN_UNDEF;
        }
        if ( attr->parent != self ) {
            XSRETURN_UNDEF;
        }
        ret = attr;
        xmlUnlinkNode( (xmlNodePtr)attr );
        RETVAL = PmmNodeToSv( (xmlNodePtr)ret, NULL );
        PmmFixOwner( SvPROXYNODE(RETVAL), NULL );
    OUTPUT:
        RETVAL

void
appendText( self, string )
        xmlNodePtr self
        SV * string
    ALIAS:
        appendTextNode = 1
        XML::LibXML::DocumentFragment::appendText = 2
        XML::LibXML::DocumentFragment::appendTextNode = 3
    PREINIT:
        xmlChar * content = NULL;
    INIT:
        content = nodeSv2C( string, self );
        if ( content == NULL ) {
            XSRETURN_UNDEF;
        }
        if ( xmlStrlen(content) == 0 ) {
            xmlFree( content );
            XSRETURN_UNDEF;
        }
    CODE:
        xmlNodeAddContent( self, content );
        xmlFree(content);


void
appendTextChild( self, strname, strcontent=&PL_sv_undef, nsURI=&PL_sv_undef )
        xmlNodePtr self
        SV * strname
        SV * strcontent
        SV * nsURI
    PREINIT:
        xmlChar * name;
        xmlChar * content = NULL;
        xmlChar * encstr  = NULL;
    INIT:
        name    = nodeSv2C( strname, self );
        if ( xmlStrlen(name) == 0 ) {
            xmlFree(name);
            XSRETURN_UNDEF;
        }
    CODE:
        content = nodeSv2C(strcontent, self);
        if ( content &&  xmlStrlen( content ) == 0 ) {
            xmlFree(content);
            content=NULL;
        }
        else if ( content ) {
            encstr = xmlEncodeEntitiesReentrant( self->doc, content );
            xmlFree(content);
        }

        xmlNewChild( self, NULL, name, encstr );

        if ( encstr )
            xmlFree(encstr);
        xmlFree(name);

SV *
addNewChild( self, namespaceURI, nodename )
        xmlNodePtr self
        SV * namespaceURI
        SV * nodename
    ALIAS:
        XML::LibXML::DocumentFragment::addNewChild = 1
    PREINIT:
        xmlChar * nsURI = NULL;
        xmlChar * name  = NULL;
        xmlChar * localname  = NULL;
        xmlChar * prefix     = NULL;
        xmlNodePtr newNode = NULL;
        xmlNodePtr prev = NULL;
        xmlNsPtr ns = NULL;
    CODE:
        name = nodeSv2C(nodename, self);
        if ( name &&  xmlStrlen( name ) == 0 ) {
            xmlFree(name);
            XSRETURN_UNDEF;
        }

        nsURI = nodeSv2C(namespaceURI, self);
        if ( nsURI &&  xmlStrlen( nsURI ) == 0 ) {
            xmlFree(nsURI);
            nsURI=NULL;
        }

        if ( nsURI != NULL ) {
            localname = xmlSplitQName2(name, &prefix);
            ns = xmlSearchNsByHref(self->doc, self, nsURI);

            newNode = xmlNewDocNode(self->doc,
                                ns,
                                localname?localname:name,
                                NULL);
            if ( ns == NULL )  {
                newNode->ns = xmlNewNs(newNode, nsURI, prefix);
            }

            xmlFree(localname);
            xmlFree(prefix);
            xmlFree(nsURI);
        }
        else {
            newNode = xmlNewDocNode(self->doc,
                                    NULL,
                                    name,
                                    NULL);
        }
        xmlFree(name);
        /* add the node to the parent node */
        newNode->type = XML_ELEMENT_NODE;
        newNode->parent = self;
        newNode->doc = self->doc;

        if (self->children == NULL) {
            self->children = newNode;
            self->last = newNode;
        } else {
            prev = self->last;
            prev->next = newNode;
            newNode->prev = prev;
            self->last = newNode;
        }
        RETVAL = PmmNodeToSv(newNode, PmmOWNERPO(PmmPROXYNODE(self)) );
    OUTPUT:
        RETVAL

MODULE = XML::LibXML         PACKAGE = XML::LibXML::Text

SV *
new( CLASS, content )
        const char * CLASS
        SV * content
    PREINIT:
        xmlChar * data;
        xmlNodePtr newNode;
        ProxyNodePtr docfrag = NULL;
    CODE:
        data = Sv2C(content, NULL);
        newNode = xmlNewText( data );
        xmlFree(data);
        if( newNode != NULL ) {
            docfrag = PmmNewFragment( NULL );
            xmlAddChild(PmmNODE(docfrag), newNode);
            RETVAL = PmmNodeToSv(newNode,docfrag);
        }
        else {
            XSRETURN_UNDEF;
        }
    OUTPUT:
        RETVAL

SV *
substringData( self, offset, length )
        xmlNodePtr self
        int offset
        int length
    PREINIT:
        xmlChar * data = NULL;
        xmlChar * substr = NULL;
        int len = 0;
        int dl = 0;
    CODE:
        if ( offset >= 0 && length > 0 ) {
            dl = offset + length - 1 ;
            data = domGetNodeValue( self );
            len = xmlStrlen( data );
            if ( data != NULL && len > 0 && len > offset ) {
                if ( dl > len )
                    dl = offset + len;

                substr = xmlStrsub( data, offset, dl );
                RETVAL = C2Sv( (const xmlChar*)substr, NULL );
                xmlFree( substr );
            }
            else {
                XSRETURN_UNDEF;
            }
        }
        else {
            XSRETURN_UNDEF;
        }
    OUTPUT:
        RETVAL

void
setData( self, value )
        xmlNodePtr self
        SV * value
    ALIAS:
        XML::LibXML::Attr::setValue = 1
        XML::LibXML::PI::_setData = 2
    PREINIT:
        xmlChar * encstr = NULL;
    CODE:
        encstr = nodeSv2C(value,self);
        domSetNodeValue( self, encstr );
        xmlFree(encstr);

void
appendData( self, value )
        xmlNodePtr self
        SV * value
    PREINIT:
        xmlChar * encstring = NULL;
        int strlen = 0;
    CODE:
        encstring = Sv2C( value,
                          self->doc!=NULL ? self->doc->encoding : NULL );

       if ( encstring != NULL ) {
            strlen = xmlStrlen( encstring );
            xmlTextConcat( self, encstring, strlen );
            xmlFree( encstring );
        }

void
insertData( self, offset, value )
        xmlNodePtr self
        int offset
        SV * value
    PREINIT:
        xmlChar * after= NULL;
        xmlChar * data = NULL;
        xmlChar * new  = NULL;
        xmlChar * encstring = NULL;
        int dl = 0;
    CODE:
        if ( offset >= 0 ) {
            encstring = Sv2C( value,
                              self->doc!=NULL ? self->doc->encoding : NULL );
            if ( encstring != NULL && xmlStrlen( encstring ) > 0 ) {
                data = domGetNodeValue(self);
                if ( data != NULL && xmlStrlen( data ) > 0 ) {
                    if ( xmlStrlen( data ) < offset ) {
                        data = xmlStrcat( data, encstring );
                        domSetNodeValue( self, data );
                    }
                    else {
                        dl = xmlStrlen( data ) - offset;

                        if ( offset > 0 )
                            new   = xmlStrsub(data, 0, offset );

                        after = xmlStrsub(data, offset, dl );

                        if ( new != NULL ) {
                            new = xmlStrcat(new, encstring );
                        }
                        else {
                            new = xmlStrdup( encstring );
                        }

                        if ( after != NULL )
                            new = xmlStrcat(new, after );

                        domSetNodeValue( self, new );

                        xmlFree( new );
                        xmlFree( after );
                    }
                    xmlFree( data );
                }
                else {
                    domSetNodeValue( self, encstring );
                }
                xmlFree(encstring);
            }
        }

void
deleteData( self, offset, length )
        xmlNodePtr self
        int offset
        int length
    PREINIT:
        xmlChar * data  = NULL;
        xmlChar * after = NULL;
        xmlChar * new   = NULL;
        int len = 0;
        int dl1 = 0;
        int dl2 = 0;
    CODE:
        if ( length > 0 && offset >= 0 ) {
            data = domGetNodeValue(self);
            len = xmlStrlen( data );
            if ( data != NULL
                 && len > 0
                 && len > offset ) {
                dl1 = offset + length;
                if ( offset > 0 )
                    new = xmlStrsub( data, 0, offset );

                if ( len > dl1 ) {
                    dl2 = len - dl1;
                    after = xmlStrsub( data, dl1, dl2 );
                    if ( new != NULL ) {
                        new = xmlStrcat( new, after );
                        xmlFree(after);
                    }
                    else {
                        new = after;
                    }
                }

                domSetNodeValue( self, new );
                xmlFree(new);
            }
        }

void
replaceData( self, offset,length, value )
        xmlNodePtr self
        int offset
        int length
        SV * value
    PREINIT:
        xmlChar * after= NULL;
        xmlChar * data = NULL;
        xmlChar * new  = NULL;
        xmlChar * encstring = NULL;
        int len = 0;
        int dl1 = 0;
        int dl2 = 0;
    CODE:
        if ( offset >= 0 ) {
            encstring = Sv2C( value,
                              self->doc!=NULL ? self->doc->encoding : NULL );

            if ( encstring != NULL && xmlStrlen( encstring ) > 0 ) {
                data = domGetNodeValue(self);
                len = xmlStrlen( data );

                if ( data != NULL
                     && len > 0
                     && len > offset  ) {

                    dl1 = offset + length;
                    if ( dl1 < len ) {
                        dl2 = xmlStrlen( data ) - dl1;
                        if ( offset > 0 ) {
                            new = xmlStrsub(data, 0, offset );
                            new = xmlStrcat(new, encstring );
                        }
                        else {
                            new   = xmlStrdup( encstring );
                        }

                        after = xmlStrsub(data, dl1, dl2 );
                        new = xmlStrcat(new, after );

                        domSetNodeValue( self, new );

                        xmlFree( new );
                        xmlFree( after );
                    }
                    else {
                        /* replace until end! */
                        if ( offset > 0 ) {
                            new = xmlStrsub(data, 0, offset );
                            new = xmlStrcat(new, encstring );
                        }
                        else {
                            new   = xmlStrdup( encstring );
                        }
                        domSetNodeValue( self, new );
                        xmlFree( new );
                    }
                    xmlFree( data );
                }

                xmlFree(encstring);
            }
        }

MODULE = XML::LibXML         PACKAGE = XML::LibXML::Comment

SV *
new( CLASS, content )
        const char * CLASS
        SV * content
    PREINIT:
        xmlChar * encstring;
        xmlNodePtr newNode;
        ProxyNodePtr docfrag = NULL;
    CODE:
        encstring = Sv2C(content, NULL);
        newNode = xmlNewComment( encstring );
        xmlFree(encstring);
        if( newNode != NULL ) {
            docfrag = PmmNewFragment( NULL );
            xmlAddChild(PmmNODE(docfrag), newNode);
            RETVAL = PmmNodeToSv(newNode,docfrag);
        }
        else {
            XSRETURN_UNDEF;
        }
    OUTPUT:
        RETVAL

MODULE = XML::LibXML         PACKAGE = XML::LibXML::CDATASection

SV *
new( CLASS , content )
        const char * CLASS
        SV * content
    PREINIT:
        xmlChar * encstring;
        xmlNodePtr newNode;
        ProxyNodePtr docfrag = NULL;
    CODE:
        encstring = Sv2C(content, NULL);
        newNode = xmlNewCDataBlock( NULL , encstring, xmlStrlen( encstring ) );
        xmlFree(encstring);
        if ( newNode != NULL ){
            docfrag = PmmNewFragment( NULL );
            xmlAddChild(PmmNODE(docfrag), newNode);
            RETVAL = PmmNodeToSv(newNode,docfrag);
        }
        else {
            XSRETURN_UNDEF;
        }
    OUTPUT:
        RETVAL

MODULE = XML::LibXML         PACKAGE = XML::LibXML::DocumentFragment

SV*
new( CLASS )
        char * CLASS
    PREINIT:
        xmlNodePtr real_doc=NULL;
    CODE:
        real_doc = xmlNewDocFragment( NULL );
        RETVAL = PmmNodeToSv( real_doc, NULL );
    OUTPUT:
        RETVAL

MODULE = XML::LibXML         PACKAGE = XML::LibXML::Attr

SV*
new( CLASS, pname, pvalue )
        char * CLASS
        SV * pname
        SV * pvalue
    PREINIT:
        xmlNodePtr attr = NULL;
        xmlChar * name;
        xmlChar * value;
    CODE:
        name  = Sv2C(pname,NULL);
        value = Sv2C(pvalue,NULL);
        if ( name == NULL ) {
            XSRETURN_UNDEF;
        }
        attr =  (xmlNodePtr)xmlNewProp( NULL, name, value );
        attr->doc = NULL;
        RETVAL = PmmNodeToSv(attr,NULL);
    OUTPUT:
        RETVAL


SV*
parentElement( attrnode )
        SV * attrnode
    ALIAS:
        XML::LibXML::Attr::getParentNode = 1
        XML::LibXML::Attr::getNextSibling = 2
        XML::LibXML::Attr::getPreviousSibling = 3
        XML::LibXML::Attr::nextSibling = 4
        XML::LibXML::Attr::previousSibling = 5
    CODE:
        /* override the original parentElement(), since this an attribute is
         * not part of the main tree
         */

        XSRETURN_UNDEF;
    OUTPUT:
        RETVAL

int
_setNamespace(self, namespaceURI, namespacePrefix = &PL_sv_undef )
        SV * self
        SV * namespaceURI
        SV * namespacePrefix
    PREINIT:
        xmlAttrPtr node = (xmlAttrPtr)PmmSvNode(self);
        xmlChar * nsURI = nodeSv2C(namespaceURI,(xmlNodePtr)node);
        xmlChar * nsPrefix = NULL;
        xmlNsPtr ns = NULL;
    INIT:
        if ( node == NULL ) {
            croak( "lost node" );
        }
    CODE:
        if ( !nsURI ){
            XSRETURN_UNDEF;
        }
        if ( !node->parent ) {
            XSRETURN_UNDEF;
        }
        nsPrefix = nodeSv2C(namespacePrefix, (xmlNodePtr)node);
        if ( (ns = xmlSearchNsByHref(node->doc, node->parent, nsURI)) )
            RETVAL = 1;
        else
            RETVAL = 0;

        if ( ns )
            node->ns = ns;

        xmlFree(nsPrefix);
        xmlFree(nsURI);
    OUTPUT:
        RETVAL

MODULE = XML::LibXML         PACKAGE = XML::LibXML::Namespace

SV*
new(CLASS, namespaceURI, namespacePrefix=&PL_sv_undef)
        const char * CLASS
        SV * namespaceURI
        SV * namespacePrefix
    PREINIT:
        xmlNsPtr ns = NULL;
        xmlChar* nsURI;
        xmlChar* nsPrefix;
    CODE:
        nsURI = Sv2C(namespaceURI,NULL);
        if ( !nsURI ) {
            XSRETURN_UNDEF;
        }
        nsPrefix = Sv2C(namespacePrefix, NULL);
        ns = xmlNewNs(NULL, nsURI, nsPrefix);
        if ( ns ) {
            RETVAL = sv_newmortal();
            RETVAL = sv_setref_pv( RETVAL,
                                   CLASS,
                                   (void*)ns);
        }
        xmlFree(nsURI);
        if ( nsPrefix )
            xmlFree(nsPrefix);
    OUTPUT:
        RETVAL

void
DESTROY(self)
        SV * self
    PREINIT:
        xmlNsPtr ns = (xmlNsPtr)SvIV(SvRV(self));
    CODE:
        xs_warn( "DESTROY NS" );
        if (ns) {
            xmlFreeNs(ns);
        }

int
nodeType(self)
        SV * self
    ALIAS:
        getType = 1
    PREINIT:
        xmlNsPtr ns = (xmlNsPtr)SvIV(SvRV(self));
    CODE:
        RETVAL = ns->type;
    OUTPUT:
        RETVAL

SV*
href(self)
        SV * self
    ALIAS:
        value = 1
        nodeValue = 2
        getData = 3
        getNamespaceURI = 4
    PREINIT:
        xmlNsPtr ns = (xmlNsPtr)SvIV(SvRV(self));
        xmlChar * href;
    CODE:
        href = xmlStrdup(ns->href);
        RETVAL = C2Sv(href, NULL);
        xmlFree(href);
    OUTPUT:
        RETVAL

SV*
localname(self)
        SV * self
    ALIAS:
        name = 1
        getLocalName = 2
        getName = 3
        getPrefix = 4
    PREINIT:
        xmlNsPtr ns = (xmlNsPtr)SvIV(SvRV(self));
        xmlChar * prefix;
    CODE:
        prefix = xmlStrdup(ns->prefix);
        RETVAL = C2Sv(prefix, NULL);
        xmlFree(prefix);
    OUTPUT:
        RETVAL

int
_isEqual(self, ref)
       SV * self
       SV * ref
    PREINIT:
       xmlNsPtr ns  = (xmlNsPtr)SvIV(SvRV(self));
       xmlNsPtr ons = (xmlNsPtr)SvIV(SvRV(ref));
    CODE:
       RETVAL = 0;
       if ( ns == ons ) {
           RETVAL = 1;
       }
       else if ( xmlStrEqual(ns->href, ons->href)
            && xmlStrEqual(ns->prefix, ons->prefix) ) {
           RETVAL = 1;
       }
    OUTPUT:
       RETVAL


MODULE = XML::LibXML         PACKAGE = XML::LibXML::Dtd

SV *
new(CLASS, external, system)
        char * CLASS
        char * external
        char * system
    ALIAS:
        parse_uri = 1
    PREINIT:
        SV * saved_error;
        xmlDtdPtr dtd = NULL;
    CODE:
        LibXML_init_error(&saved_error);
        dtd = xmlParseDTD((const xmlChar*)external, (const xmlChar*)system);
        if ( dtd == NULL ) {
            XSRETURN_UNDEF;
        } else {
            xmlSetTreeDoc((xmlNodePtr)dtd, NULL);
            RETVAL = PmmNodeToSv( (xmlNodePtr) dtd, NULL );
        }
        LibXML_report_error(saved_error, 0);
    OUTPUT:
        RETVAL

void
DESTROY( node )
        SV * node
    CODE:
        xs_warn("DESTROY DTD NODE\n");
        PmmREFCNT_dec(SvPROXYNODE(node));

SV *
parse_string(CLASS, str, ...)
        char * CLASS
        char * str
    PREINIT:
        SV * saved_error;
        xmlDtdPtr res;
        SV * encoding_sv;
        xmlParserInputBufferPtr buffer;
        xmlCharEncoding enc = XML_CHAR_ENCODING_NONE;
        xmlChar * new_string;
    CODE:
        LibXML_init_error(&saved_error);
        if (items > 2) {
            encoding_sv = ST(2);
            if (items > 3) {
                croak("parse_string: too many parameters");
            }
            /* warn("getting encoding...\n"); */
            enc = xmlParseCharEncoding(SvPV_nolen(encoding_sv));
            if (enc == XML_CHAR_ENCODING_ERROR) {
                LibXML_report_error(saved_error, 1);
                croak("Parse of encoding %s failed", SvPV_nolen(encoding_sv));
            }
        }
        buffer = xmlAllocParserInputBuffer(enc);
        /* buffer = xmlParserInputBufferCreateMem(str, xmlStrlen(str), enc); */
        if ( !buffer)
            croak("cant create buffer!\n" );

        new_string = xmlStrdup((const xmlChar*)str);
        xmlParserInputBufferPush(buffer, xmlStrlen(new_string), (const char*)new_string);

        res = xmlIOParseDTD(NULL, buffer, enc);

        /* NOTE: xmlIOParseDTD is documented to free its InputBuffer */
        xmlFree(new_string);

        LibXML_report_error(saved_error, 0);
        if (res == NULL) {
            croak("no DTD parsed!");
        }
        RETVAL = PmmNodeToSv((xmlNodePtr)res, NULL);
    OUTPUT:
        RETVAL


#ifdef HAVE_SCHEMAS

MODULE = XML::LibXML         PACKAGE = XML::LibXML::RelaxNG

void 
DESTROY( self ) 
        xmlRelaxNGPtr self
    CODE:
        xmlRelaxNGFree( self );


xmlRelaxNGPtr
parse_location( self, url )
        SV * self
        char * url
    PREINIT:
        SV * saved_error;
        const char * CLASS = "XML::LibXML::RelaxNG";
        xmlRelaxNGParserCtxtPtr rngctxt = NULL;
    CODE:
        LibXML_init_error(&saved_error);

        rngctxt = xmlRelaxNGNewParserCtxt( url );
        if ( rngctxt == NULL ) {
            croak( "failed to initialize RelaxNG parser" );
        }

        /* Register Error callbacks */
        xmlRelaxNGSetParserErrors( rngctxt,
                                  (xmlRelaxNGValidityErrorFunc)LibXML_error_handler,
                                  (xmlRelaxNGValidityWarningFunc)LibXML_error_handler,
                                  rngctxt );

        RETVAL = xmlRelaxNGParse( rngctxt );
        xmlRelaxNGFreeParserCtxt( rngctxt );

        LibXML_report_error(saved_error, (RETVAL != NULL));
    OUTPUT:
        RETVAL


xmlRelaxNGPtr
parse_buffer( self, perlstring )
        SV * self
        SV * perlstring
    PREINIT:
        SV * saved_error;
        const char * CLASS = "XML::LibXML::RelaxNG";
        xmlRelaxNGParserCtxtPtr rngctxt = NULL;
        char * string = NULL;
        STRLEN len    = 0;
    INIT:
        string = SvPV( perlstring, len );
        if ( string == NULL ) {
            croak( "cannot parse empty string" );
        }
    CODE:
        LibXML_init_error(&saved_error);

        rngctxt = xmlRelaxNGNewMemParserCtxt( string,len );
        if ( rngctxt == NULL ) {
            croak( "failed to initialize RelaxNG parser" );
        }

        /* Register Error callbacks */
        xmlRelaxNGSetParserErrors( rngctxt,
                                  (xmlRelaxNGValidityErrorFunc)LibXML_error_handler,
                                  (xmlRelaxNGValidityWarningFunc)LibXML_error_handler,
                                  rngctxt );

        RETVAL = xmlRelaxNGParse( rngctxt );
        xmlRelaxNGFreeParserCtxt( rngctxt );

        LibXML_report_error(saved_error, (RETVAL != NULL));
    OUTPUT:
        RETVAL


xmlRelaxNGPtr
parse_document( self, doc )
        SV * self
        xmlDocPtr doc
    PREINIT:
        SV * saved_error;
        const char * CLASS = "XML::LibXML::RelaxNG";
        xmlRelaxNGParserCtxtPtr rngctxt = NULL;
    CODE:
        LibXML_init_error(&saved_error);

        rngctxt = xmlRelaxNGNewDocParserCtxt( doc );
        if ( rngctxt == NULL ) {
            croak( "failed to initialize RelaxNG parser" );
        }

        /* Register Error callbacks */
        xmlRelaxNGSetParserErrors( rngctxt,
                                  (xmlRelaxNGValidityErrorFunc)  LibXML_error_handler,
                                  (xmlRelaxNGValidityWarningFunc)LibXML_error_handler,
                                  rngctxt );

        RETVAL = xmlRelaxNGParse( rngctxt );
        xmlRelaxNGFreeParserCtxt( rngctxt );

        LibXML_report_error(saved_error, (RETVAL != NULL));
    OUTPUT:
        RETVAL

int
validate( self, doc )
        xmlRelaxNGPtr self
        xmlDocPtr doc
    PREINIT:
        SV * saved_error;
        xmlRelaxNGValidCtxtPtr vctxt = NULL;
    CODE:
        LibXML_init_error(&saved_error);

        vctxt  = xmlRelaxNGNewValidCtxt( self );
        if ( vctxt == NULL ) {
            croak( "cannot initialize the validation context" );
        }

        /* Register Error callbacks */
        xmlRelaxNGSetValidErrors( vctxt,
                                  (xmlRelaxNGValidityErrorFunc)LibXML_error_handler,
                                  (xmlRelaxNGValidityWarningFunc)LibXML_error_handler,
                                  vctxt );

        RETVAL = xmlRelaxNGValidateDoc( vctxt, doc );
        xmlRelaxNGFreeValidCtxt( vctxt );

        LibXML_report_error(saved_error, 0);
        if ( RETVAL == 1 ) {
            XSRETURN_UNDEF;
        }
        if ( RETVAL == -1 ) {
            croak( "API Error" );
            XSRETURN_UNDEF;
        }
    OUTPUT:
        RETVAL


MODULE = XML::LibXML         PACKAGE = XML::LibXML::Schema

void 
DESTROY( self ) 
        xmlSchemaPtr self
    CODE:
        xmlSchemaFree( self );


xmlSchemaPtr
parse_location( self, url )
        SV * self
        char * url
    PREINIT:
        SV * saved_error;
        const char * CLASS = "XML::LibXML::Schema";
        xmlSchemaParserCtxtPtr rngctxt = NULL;
    CODE:
        LibXML_init_error(&saved_error);

        rngctxt = xmlSchemaNewParserCtxt( url );
        if ( rngctxt == NULL ) {
            croak( "failed to initialize Schema parser" );
        }

        /* Register Error callbacks */
        xmlSchemaSetParserErrors( rngctxt,
                                  (xmlSchemaValidityErrorFunc)LibXML_error_handler,
                                  (xmlSchemaValidityWarningFunc)LibXML_error_handler,
                                  rngctxt );

        RETVAL = xmlSchemaParse( rngctxt );
        xmlSchemaFreeParserCtxt( rngctxt );

        LibXML_report_error(saved_error, (RETVAL != NULL));
    OUTPUT:
        RETVAL


xmlSchemaPtr
parse_buffer( self, perlstring )
        SV * self
        SV * perlstring
    PREINIT:
        SV * saved_error;
        const char * CLASS = "XML::LibXML::Schema";
        xmlSchemaParserCtxtPtr rngctxt = NULL;
        char * string = NULL;
        STRLEN len    = 0;
    INIT:
        string = SvPV( perlstring, len );
        if ( string == NULL ) {
            croak( "cannot parse empty string" );
        }
    CODE:
        LibXML_init_error(&saved_error);

        rngctxt = xmlSchemaNewMemParserCtxt( string,len );
        if ( rngctxt == NULL ) {
            croak( "failed to initialize Schema parser" );
        }

        /* Register Error callbacks */
        xmlSchemaSetParserErrors( rngctxt,
                                  (xmlSchemaValidityErrorFunc)LibXML_error_handler,
                                  (xmlSchemaValidityWarningFunc)LibXML_error_handler,
                                  NULL );

        RETVAL = xmlSchemaParse( rngctxt );
        xmlSchemaFreeParserCtxt( rngctxt );

        LibXML_report_error(saved_error, (RETVAL != NULL));
    OUTPUT:
        RETVAL


int
validate( self, doc )
        xmlSchemaPtr self
        xmlDocPtr doc
    PREINIT:
        SV * saved_error;
        xmlSchemaValidCtxtPtr vctxt = NULL;
    CODE:
        LibXML_init_error(&saved_error);

        vctxt  = xmlSchemaNewValidCtxt( self );
        if ( vctxt == NULL ) {
            croak( "cannot initialize the validation context" );
        }

        /* Register Error callbacks */
        xmlSchemaSetValidErrors( vctxt,
                                  (xmlSchemaValidityErrorFunc)LibXML_error_handler,
                                  (xmlSchemaValidityWarningFunc)LibXML_error_handler,
                                  NULL );

        RETVAL = xmlSchemaValidateDoc( vctxt, doc );
        xmlSchemaFreeValidCtxt( vctxt );

        LibXML_report_error(saved_error, 0);
        if ( RETVAL > 0 ) {
            XSRETURN_UNDEF;
        }
        if ( RETVAL == -1 ) {
            croak( "API Error" );
            XSRETURN_UNDEF;
        }
    OUTPUT:
        RETVAL

#endif /* HAVE_SCHEMAS */
