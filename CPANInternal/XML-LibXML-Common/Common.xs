#ifdef __cplusplus
extern "C" {
#endif

/* perl stuff */
#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"
#include "ppport.h"

#include <libxml/parser.h>
/* #include <libxml/tree.h> */

#ifdef __cplusplus
}
#endif

static SV * LibXML_COMMON_error    = NULL;

/* stores libxml errors into $@ */
void
LIBXML_COMMON_error_handler(void * ctxt, const char * msg, ...)
{
    va_list args;
    SV * sv;

    sv = NEWSV(0,512);

    va_start(args, msg);
    sv_vsetpvfn(sv, msg, strlen(msg), &args, NULL, 0, NULL);
    va_end(args);

    if (LibXML_COMMON_error != NULL) {
        sv_catsv(LibXML_COMMON_error, sv); /* remember the last error */
    }
    else {
        croak(SvPV(sv, PL_na));
    }
    SvREFCNT_dec(sv);
}

MODULE = XML::LibXML::Common         PACKAGE = XML::LibXML::Common

PROTOTYPES: DISABLE

SV*
encodeToUTF8( encoding, string )
        const char * encoding
        SV * string
    PREINIT:
        xmlChar * realstring = NULL;
        xmlChar * tstr = NULL;
        xmlCharEncoding enc = 0;
        STRLEN len = 0;
        xmlBufferPtr in = NULL, out = NULL;
        xmlCharEncodingHandlerPtr coder = NULL;
    CODE:
        realstring = SvPV(string, len);
        if ( realstring != NULL ) {
            /* warn("encode %s", realstring ); */
#ifdef HAVE_UTF8
            if ( !DO_UTF8(string) && encoding != NULL ) {
#else 
            if ( encoding != NULL ) {
#endif
                enc = xmlParseCharEncoding( encoding );
    
                if ( enc == 0 ) {
                    /* this happens if the encoding is "" or NULL */
                    enc = XML_CHAR_ENCODING_UTF8;
                }

                if ( enc == XML_CHAR_ENCODING_UTF8 ) {
                    /* copy the string */
                    /* warn( "simply copy the string" ); */
                    tstr = xmlStrdup( realstring );
                }
                else {
                    LibXML_COMMON_error = NEWSV(0, 512);
                    xmlSetGenericErrorFunc(PerlIO_stderr(),
                               (xmlGenericErrorFunc)LIBXML_COMMON_error_handler);

    
                    if ( enc > 1 ) {
                        coder= xmlGetCharEncodingHandler( enc );
                    }
                    else if ( enc == XML_CHAR_ENCODING_ERROR ){
                        coder =xmlFindCharEncodingHandler( encoding );
                    }
                    else {
                        croak("no encoder found\n");
                    }

                    if ( coder == NULL ) {  
                        croak( "cannot encode string" );
                    }
                
                    in    = xmlBufferCreate();
                    out   = xmlBufferCreate();
                    xmlBufferCCat( in, realstring );
                    if ( xmlCharEncInFunc( coder, out, in ) >= 0 ) {
                        tstr = xmlStrdup( out->content );
                    }
        
                    xmlBufferFree( in );
                    xmlBufferFree( out );
                    xmlCharEncCloseFunc( coder );

                    sv_2mortal(LibXML_COMMON_error);

                    if ( SvCUR( LibXML_COMMON_error ) > 0 ) {
                        croak(SvPV(LibXML_COMMON_error, len));
                    }
                }
            }
            else {
                tstr = xmlStrdup( realstring );
            }

            if ( !tstr ) {
                croak( "return value missing!" );
            }

            len = xmlStrlen( tstr ); 
            RETVAL = newSVpvn( (const char *)tstr, len );
#ifdef HAVE_UTF8
            SvUTF8_on(RETVAL);
#endif  
            xmlFree(tstr);
        }
        else {
            XSRETURN_UNDEF;
        }
    OUTPUT:
        RETVAL

SV*
decodeFromUTF8( encoding, string ) 
        const char * encoding
        SV* string
    PREINIT:
        xmlChar * tstr = NULL;
        xmlChar * realstring = NULL;
        xmlCharEncoding enc = 0;
        STRLEN len = 0;
        xmlBufferPtr in = NULL, out = NULL;
        xmlCharEncodingHandlerPtr coder = NULL;
    CODE: 
#ifdef HAVE_UTF8
        if ( !SvUTF8(string) ) {
            croak("string is not utf8!!");
        }
        else {
#endif
            realstring = SvPV(string, len);
            if ( realstring != NULL ) {
                /* warn("decode %s", realstring ); */
                enc = xmlParseCharEncoding( encoding );
                if ( enc == 0 ) {
                    /* this happens if the encoding is "" or NULL */
                    enc = XML_CHAR_ENCODING_UTF8;
                }

                if ( enc == XML_CHAR_ENCODING_UTF8 ) {
                    /* copy the string */
                    /* warn( "simply copy the string" ); */
                    tstr = xmlStrdup( realstring );
                    len = xmlStrlen( tstr );
                }
                else {
                    LibXML_COMMON_error = NEWSV(0, 512);
                    xmlSetGenericErrorFunc(PerlIO_stderr(),
                           (xmlGenericErrorFunc)LIBXML_COMMON_error_handler);

                    sv_2mortal(LibXML_COMMON_error);

                    if ( enc > 1 ) {
                        coder= xmlGetCharEncodingHandler( enc );
                    }
                    else if ( enc == XML_CHAR_ENCODING_ERROR ){
                        coder = xmlFindCharEncodingHandler( encoding );
                    }
                    else {
                        croak("no encoder found\n");
                    }

                    if ( coder == NULL ) {  
                        croak( "cannot encode string" );
                    }

                    in    = xmlBufferCreate();
                    out   = xmlBufferCreate();
                    xmlBufferCCat( in, realstring );
                    if ( xmlCharEncOutFunc( coder, out, in ) >= 0 ) {
                        len  = xmlBufferLength( out );
                        tstr = xmlCharStrndup( xmlBufferContent( out ), len );
                    }
        
                    xmlBufferFree( in );
                    xmlBufferFree( out );
                    xmlCharEncCloseFunc( coder );

                    if ( SvCUR( LibXML_COMMON_error ) > 0 ) {
                        croak(SvPV(LibXML_COMMON_error, len));
                    }
            
                    if ( !tstr ) {
                        croak( "return value missing!" );
                    }
                }

                RETVAL = newSVpvn( (const char *)tstr, len );
                xmlFree( tstr );
#ifdef HAVE_UTF8
                if ( enc == XML_CHAR_ENCODING_UTF8 ) {
                    SvUTF8_on(RETVAL);
                }
#endif  
            }
            else {
                XSRETURN_UNDEF;
            }
#ifdef HAVE_UTF8
        }
#endif
    OUTPUT:
        RETVAL

