/* tidylib.c -- internal library definitions

  (c) 1998-2004 (W3C) MIT, ERCIM, Keio University
  See tidy.h for the copyright notice.

  CVS Info :

    $Author: swilkin $ 
    $Date: 2005/01/06 02:01:54 $ 
    $Revision: 1.1.1.3 $ 

  Defines HTML Tidy API implemented by tidy library.
  
  Very rough initial cut for discussion purposes.

  Public interface is const-correct and doesn't explicitly depend
  on any globals.  Thus, thread-safety may be introduced w/out
  changing the interface.

  Looking ahead to a C++ wrapper, C functions always pass 
  this-equivalent as 1st arg.

  Created 2001-05-20 by Charles Reitzel

*/

#include <errno.h>

#include "tidy-int.h"
#include "parser.h"
#include "clean.h"
#include "config.h"
#include "message.h"
#include "pprint.h"
#include "entities.h"
#include "tmbstr.h"
#include "utf8.h"

#ifdef TIDY_WIN32_MLANG_SUPPORT
#include "win32tc.h"
#endif

#ifdef NEVER
TidyDocImpl* tidyDocToImpl( TidyDoc tdoc )
{
  return (TidyDocImpl*) tdoc;
}
TidyDoc      tidyImplToDoc( TidyDocImpl* impl )
{
  return (TidyDoc) impl;
}

Node*        tidyNodeToImpl( TidyNode tnod )
{
  return (Node*) tnod;
}
TidyNode     tidyImplToNode( Node* node )
{
  return (TidyNode) node;
}

AttVal*      tidyAttrToImpl( TidyAttr tattr )
{
  return (AttVal*) tattr;
}
TidyAttr     tidyImplToAttr( AttVal* attval )
{
  return (TidyAttr) attval;
}

const TidyOptionImpl* tidyOptionToImpl( TidyOption topt )
{
  return (const TidyOptionImpl*) topt;
}
TidyOption   tidyImplToOption( const TidyOptionImpl* option )
{
  return (TidyOption) option;
}
#endif

/* Tidy public interface
**
** Most functions return an integer:
**
** 0    -> SUCCESS
** >0   -> WARNING
** <0   -> ERROR
** 
*/

TidyDoc       tidyCreate(void)
{
  TidyDocImpl* impl = tidyDocCreate();
  return tidyImplToDoc( impl );
}

void          tidyRelease( TidyDoc tdoc )
{
  TidyDocImpl* impl = tidyDocToImpl( tdoc );
  tidyDocRelease( impl );
}

TidyDocImpl* tidyDocCreate(void)
{
    TidyDocImpl* doc = (TidyDocImpl*)MemAlloc( sizeof(TidyDocImpl) );
    ClearMemory( doc, sizeof(*doc) );

    InitMap();
    InitTags( doc );
    InitAttrs( doc );
    InitConfig( doc );
    InitPrintBuf( doc );

    /* By default, wire tidy messages to standard error.
    ** Document input will be set by parsing routines.
    ** Document output will be set by pretty print routines.
    ** Config input will be set by config parsing routines.
    ** But we need to start off with a way to report errors.
    */
    doc->errout = StdErrOutput();
    return doc;
}

void          tidyDocRelease( TidyDocImpl* doc )
{
    /* doc in/out opened and closed by parse/print routines */
    if ( doc )
    {
        assert( doc->docIn == NULL );
        assert( doc->docOut == NULL );

        ReleaseStreamOut( doc->errout );
        doc->errout = NULL;

        FreePrintBuf( doc );
        FreeLexer( doc );
        FreeNode(doc, &doc->root);
        ClearMemory(&doc->root, sizeof(Node));

        if (doc->givenDoctype)
            MemFree(doc->givenDoctype);

        FreeConfig( doc );
        FreeAttrTable( doc );
        FreeTags( doc );
        MemFree( doc );
    }
}

/* Let application store a chunk of data w/ each Tidy tdocance.
** Useful for callbacks.
*/
void        tidySetAppData( TidyDoc tdoc, ulong appData )
{
  TidyDocImpl* impl = tidyDocToImpl( tdoc );
  if ( impl )
    impl->appData = appData;
}
ulong       tidyGetAppData( TidyDoc tdoc )
{
  TidyDocImpl* impl = tidyDocToImpl( tdoc );
  if ( impl )
    return impl->appData;
  return 0;
}

ctmbstr     tidyReleaseDate(void)
{
    return ReleaseDate();
}


/* Get/set configuration options
*/
Bool        tidySetOptionCallback( TidyDoc tdoc, TidyOptCallback pOptCallback )
{
  TidyDocImpl* impl = tidyDocToImpl( tdoc );
  if ( impl )
  {
    impl->pOptCallback = pOptCallback;
    return yes;
  }
  return -EINVAL;
}


int     tidyLoadConfig( TidyDoc tdoc, ctmbstr cfgfil )
{
    TidyDocImpl* impl = tidyDocToImpl( tdoc );
    if ( impl )
        return ParseConfigFile( impl, cfgfil );
    return -EINVAL;
}

int     tidyLoadConfigEnc( TidyDoc tdoc, ctmbstr cfgfil, ctmbstr charenc )
{
    TidyDocImpl* impl = tidyDocToImpl( tdoc );
    if ( impl )
        return ParseConfigFileEnc( impl, cfgfil, charenc );
    return -EINVAL;
}

int         tidySetCharEncoding( TidyDoc tdoc, ctmbstr encnam )
{
    TidyDocImpl* impl = tidyDocToImpl( tdoc );
    if ( impl )
    {
        int enc = CharEncodingId( encnam );
        if ( enc >= 0 && AdjustCharEncoding(impl, enc) )
            return 0;

        ReportBadArgument( impl, "char-encoding" );
    }
    return -EINVAL;
}

int           tidySetInCharEncoding( TidyDoc tdoc, ctmbstr encnam )
{
    TidyDocImpl* impl = tidyDocToImpl( tdoc );
    if ( impl )
    {
        int enc = CharEncodingId( encnam );
        if ( enc >= 0 && SetOptionInt( impl, TidyInCharEncoding, enc ) )
            return 0;

        ReportBadArgument( impl, "in-char-encoding" );
    }
    return -EINVAL;
}

int           tidySetOutCharEncoding( TidyDoc tdoc, ctmbstr encnam )
{
    TidyDocImpl* impl = tidyDocToImpl( tdoc );
    if ( impl )
    {
        int enc = CharEncodingId( encnam );
        if ( enc >= 0 && SetOptionInt( impl, TidyOutCharEncoding, enc ) )
            return 0;

        ReportBadArgument( impl, "out-char-encoding" );
    }
    return -EINVAL;
}

TidyOptionId  tidyOptGetIdForName( ctmbstr optnam )
{
    const TidyOptionImpl* option = lookupOption( optnam );
    if ( option )
        return option->id;
    return N_TIDY_OPTIONS;  /* Error */
}

TidyIterator  tidyGetOptionList( TidyDoc tdoc )
{
    TidyDocImpl* impl = tidyDocToImpl( tdoc );
    if ( impl )
        return getOptionList( impl );
    return (TidyIterator) -1;
}

TidyOption    tidyGetNextOption( TidyDoc tdoc, TidyIterator* pos )
{
    TidyDocImpl* impl = tidyDocToImpl( tdoc );
    const TidyOptionImpl* option = NULL;
    if ( impl )
        option = getNextOption( impl, pos );
    else if ( pos )
        *pos = 0;
    return tidyImplToOption( option );
}


TidyOption    tidyGetOption( TidyDoc tdoc, TidyOptionId optId )
{
#pragma unused(tdoc)

    const TidyOptionImpl* option = getOption( optId );
    return tidyImplToOption( option );
}
TidyOption    tidyGetOptionByName( TidyDoc doc, ctmbstr optnam )
{
#pragma unused(doc)

    const TidyOptionImpl* option = lookupOption( optnam );
    return tidyImplToOption( option );
}

TidyOptionId  tidyOptGetId( TidyOption topt )
{
    const TidyOptionImpl* option = tidyOptionToImpl( topt );
    if ( option )
        return option->id;
    return N_TIDY_OPTIONS;
}
ctmbstr       tidyOptGetName( TidyOption topt )
{
    const TidyOptionImpl* option = tidyOptionToImpl( topt );
    if ( option )
        return option->name;
    return NULL;
}
TidyOptionType tidyOptGetType( TidyOption topt )
{
    const TidyOptionImpl* option = tidyOptionToImpl( topt );
    if ( option )
        return option->type;
    return (TidyOptionType) -1;
}
TidyConfigCategory tidyOptGetCategory( TidyOption topt )
{
    const TidyOptionImpl* option = tidyOptionToImpl( topt );
    if ( option )
        return option->category;
    return (TidyConfigCategory) -1;
}
ctmbstr       tidyOptGetDefault( TidyOption topt )
{
    const TidyOptionImpl* option = tidyOptionToImpl( topt );
    if ( option && option->type == TidyString )
        return (ctmbstr) option->dflt;
    return NULL;
}
ulong          tidyOptGetDefaultInt( TidyOption topt )
{
    const TidyOptionImpl* option = tidyOptionToImpl( topt );
    if ( option && option->type != TidyString )
        return option->dflt;
    return ~0U;
}
Bool          tidyOptGetDefaultBool( TidyOption topt )
{
    const TidyOptionImpl* option = tidyOptionToImpl( topt );
    if ( option && option->type != TidyString )
        return ( option->dflt ? yes : no );
    return no;
}
Bool          tidyOptIsReadOnly( TidyOption topt )
{
    const TidyOptionImpl* option = tidyOptionToImpl( topt );
    if ( option  )
        return ( option->parser == NULL );
    return yes;
}


TidyIterator  tidyOptGetPickList( TidyOption topt )
{
    const TidyOptionImpl* option = tidyOptionToImpl( topt );
    if ( option )
      return getOptionPickList( option );
    return (TidyIterator) -1;
}
ctmbstr       tidyOptGetNextPick( TidyOption topt, TidyIterator* pos )
{
    const TidyOptionImpl* option = tidyOptionToImpl( topt );
    if ( option )
        return getNextOptionPick( option, pos );
    return NULL;
}


ctmbstr       tidyOptGetValue( TidyDoc tdoc, TidyOptionId optId )
{
  TidyDocImpl* impl = tidyDocToImpl( tdoc );
  ctmbstr optval = NULL;
  if ( impl )
    optval = cfgStr( impl, optId );
  return optval;
}
Bool        tidyOptSetValue( TidyDoc tdoc, TidyOptionId optId, ctmbstr val )
{
  TidyDocImpl* impl = tidyDocToImpl( tdoc );
  if ( impl )
    return ParseConfigValue( impl, optId, val );
  return no;
}
Bool        tidyOptParseValue( TidyDoc tdoc, ctmbstr optnam, ctmbstr val )
{
  TidyDocImpl* impl = tidyDocToImpl( tdoc );
  if ( impl )
    return ParseConfigOption( impl, optnam, val );
  return no;
}

ulong        tidyOptGetInt( TidyDoc tdoc, TidyOptionId optId )
{
    TidyDocImpl* impl = tidyDocToImpl( tdoc );
    ulong opti = 0;
    if ( impl )
        opti = cfg( impl, optId );
    return opti;
}

Bool        tidyOptSetInt( TidyDoc tdoc, TidyOptionId optId, ulong val )
{
    TidyDocImpl* impl = tidyDocToImpl( tdoc );
    if ( impl )
        return SetOptionInt( impl, optId, val );
    return no;
}

Bool         tidyOptGetBool( TidyDoc tdoc, TidyOptionId optId )
{
    TidyDocImpl* impl = tidyDocToImpl( tdoc );
    Bool optb = no;
    if ( impl )
    {
        const TidyOptionImpl* option = getOption( optId );
        if ( option )
        {
            optb = cfgBool( impl, optId );
        }
    }
    return optb;
}

Bool        tidyOptSetBool( TidyDoc tdoc, TidyOptionId optId, Bool val )
{
    TidyDocImpl* impl = tidyDocToImpl( tdoc );
    if ( impl )
        return SetOptionBool( impl, optId, val );
    return no;
}

ctmbstr       tidyOptGetEncName( TidyDoc tdoc, TidyOptionId optId )
{
  uint enc = tidyOptGetInt( tdoc, optId );
  return CharEncodingName( enc );
}

ctmbstr       tidyOptGetCurrPick( TidyDoc tdoc, TidyOptionId optId )
{
    const TidyOptionImpl* option = getOption( optId );
    if ( option && option->pickList )
    {
        uint ix, pick = tidyOptGetInt( tdoc, optId );
        const ctmbstr* pL = option->pickList;
        for ( ix=0; *pL && ix < pick; ++ix )
            ++pL;
        if ( *pL )
            return *pL;
    }
    return NULL;
}


TidyIterator  tidyOptGetDeclTagList( TidyDoc tdoc )
{
    TidyDocImpl* impl = tidyDocToImpl( tdoc );
    TidyIterator declIter = 0;
    if ( impl )
        declIter = GetDeclaredTagList( impl );
    return declIter;
}

ctmbstr       tidyOptGetNextDeclTag( TidyDoc tdoc, TidyOptionId optId,
                                     TidyIterator* iter )
{
    TidyDocImpl* impl = tidyDocToImpl( tdoc );
    ctmbstr tagnam = NULL;
    if ( impl )
    {
        int tagtyp = 0;
        if ( optId == TidyInlineTags )
            tagtyp = tagtype_inline;
        else if ( optId == TidyBlockTags )
            tagtyp = tagtype_block;
        else if ( optId == TidyEmptyTags )
            tagtyp = tagtype_empty;
        else if ( optId == TidyPreTags )
            tagtyp = tagtype_pre;
        if ( tagtyp )
            tagnam = GetNextDeclaredTag( impl, tagtyp, iter );
    }
    return tagnam;
}

int tidyOptSaveFile( TidyDoc tdoc, ctmbstr cfgfil )
{
    TidyDocImpl* impl = tidyDocToImpl( tdoc );
    if ( impl )
        return SaveConfigFile( impl, cfgfil );
    return -EINVAL;
}

int tidyOptSaveSink( TidyDoc tdoc, TidyOutputSink* sink )
{
    TidyDocImpl* impl = tidyDocToImpl( tdoc );
    if ( impl )
        return SaveConfigSink( impl, sink );
    return -EINVAL;
}

Bool tidyOptSnapshot( TidyDoc tdoc )
{
    TidyDocImpl* impl = tidyDocToImpl( tdoc );
    if ( impl )
    {
        TakeConfigSnapshot( impl );
        return yes;
    }
    return no;
}
Bool tidyOptResetToSnapshot( TidyDoc tdoc )
{
    TidyDocImpl* impl = tidyDocToImpl( tdoc );
    if ( impl )
    {
        ResetConfigToSnapshot( impl );
        return yes;
    }
    return no;
}
Bool tidyOptResetAllToDefault( TidyDoc tdoc )
{
    TidyDocImpl* impl = tidyDocToImpl( tdoc );
    if ( impl )
    {
        ResetConfigToDefault( impl );
        return yes;
    }
    return no;
}

Bool tidyOptResetToDefault( TidyDoc tdoc, TidyOptionId optId )
{
    TidyDocImpl* impl = tidyDocToImpl( tdoc );
    if ( impl )
        return ResetOptionToDefault( impl, optId );
    return no;
}

Bool tidyOptDiffThanDefault( TidyDoc tdoc )
{
    TidyDocImpl* impl = tidyDocToImpl( tdoc );
    if ( impl )
        return ConfigDiffThanDefault( impl );
    return no;
}
Bool          tidyOptDiffThanSnapshot( TidyDoc tdoc )
{
    TidyDocImpl* impl = tidyDocToImpl( tdoc );
    if ( impl )
        return ConfigDiffThanSnapshot( impl );
    return no;
}

Bool tidyOptCopyConfig( TidyDoc to, TidyDoc from )
{
    TidyDocImpl* docTo = tidyDocToImpl( to );
    TidyDocImpl* docFrom = tidyDocToImpl( from );
    if ( docTo && docFrom )
    {
        CopyConfig( docTo, docFrom );
        return yes;
    }
    return no;
}


/* I/O and Message handling interface
**
** By default, Tidy will define, create and use 
** tdocances of input and output handlers for 
** standard C buffered I/O (i.e. FILE* stdin,
** FILE* stdout and FILE* stderr for content
** input, content output and diagnostic output,
** respectively.  A FILE* cfgFile input handler
** will be used for config files.  Command line
** options will just be set directly.
*/

/* Use TidyReportFilter to filter messages by diagnostic level:
** info, warning, etc.  Just set diagnostic output 
** handler to redirect all diagnostics output.  Return true
** to proceed with output, false to cancel.
*/
Bool        tidySetReportFilter( TidyDoc tdoc, TidyReportFilter filt )
{
  TidyDocImpl* impl = tidyDocToImpl( tdoc );
  if ( impl )
  {
    impl->mssgFilt = filt;
    return yes;
  }
  return -EINVAL;
}

#if 0   /* Not yet */
int         tidySetContentOutputSink( TidyDoc tdoc, TidyOutputSink* outp )
{
  TidyDocImpl* impl = tidyDocToImpl( tdoc );
  if ( impl )
  {
    impl->docOut = outp;
    return 0;
  }
  return -EINVAL;
}
int         tidySetDiagnosticOutputSink( TidyDoc tdoc, TidyOutputSink* outp )
{
  TidyDocImpl* impl = tidyDocToImpl( tdoc );
  if ( impl )
  {
    impl->msgOut = outp;
    return 0;
  }
  return -EINVAL;
}


/* Library helpers
*/
cmbstr       tidyLookupMessage( TidyDoc tdoc, int errorNo )
{
  TidyDocImpl* impl = tidyDocToImpl( tdoc );
  cmbstr mssg = NULL;
  if ( impl )
    mssg = tidyMessage_Lookup( impl->messages, errorNo );
  return mssg;
}
#endif


FILE*   tidySetErrorFile( TidyDoc tdoc, ctmbstr errfilnam )
{
    TidyDocImpl* impl = tidyDocToImpl( tdoc );
    if ( impl )
    {
        FILE* errout = fopen( errfilnam, "wb" );
        if ( errout )
        {
            uint outenc = cfg( impl, TidyOutCharEncoding );
            uint nl = cfg( impl, TidyNewline );
            ReleaseStreamOut( impl->errout );
            impl->errout = FileOutput( errout, outenc, nl );
            return errout;
        }
        else /* Emit message to current error sink */
            FileError( impl, errfilnam, TidyError );
    }
    return NULL;
}

int     tidySetErrorBuffer( TidyDoc tdoc, TidyBuffer* errbuf )
{
    TidyDocImpl* impl = tidyDocToImpl( tdoc );
    if ( impl )
    {
        uint outenc = cfg( impl, TidyOutCharEncoding );
        uint nl = cfg( impl, TidyNewline );
        ReleaseStreamOut( impl->errout );
        impl->errout = BufferOutput( errbuf, outenc, nl );
        return ( impl->errout ? 0 : -ENOMEM );
    }
    return -EINVAL;
}

int     tidySetErrorSink( TidyDoc tdoc, TidyOutputSink* sink )
{
    TidyDocImpl* impl = tidyDocToImpl( tdoc );
    if ( impl )
    {
        uint outenc = cfg( impl, TidyOutCharEncoding );
        uint nl = cfg( impl, TidyNewline );
        ReleaseStreamOut( impl->errout );
        impl->errout = UserOutput( sink, outenc, nl );
        return ( impl->errout ? 0 : -ENOMEM );
    }
    return -EINVAL;
}


/* Document info */
int         tidyStatus( TidyDoc tdoc )
{
    TidyDocImpl* impl = tidyDocToImpl( tdoc );
    int tidyStat = -EINVAL;
    if ( impl )
        tidyStat = tidyDocStatus( impl );
    return tidyStat;
}
int         tidyDetectedHtmlVersion( TidyDoc tdoc )
{
#pragma unused(tdoc)

/*    TidyDocImpl* impl = tidyDocToImpl( tdoc ); */
    return 0;
}
Bool        tidyDetectedXhtml( TidyDoc tdoc )
{
#pragma unused(tdoc)

/*    TidyDocImpl* impl = tidyDocToImpl( tdoc ); */
    return no;
}
Bool        tidyDetectedGenericXml( TidyDoc tdoc )
{
#pragma unused(tdoc)

/*    TidyDocImpl* impl = tidyDocToImpl( tdoc ); */
    return no;
}

uint        tidyErrorCount( TidyDoc tdoc )
{
    TidyDocImpl* impl = tidyDocToImpl( tdoc );
    uint count = 0xFFFFFFFF;
    if ( impl )
        count = impl->errors;
    return count;
}
uint        tidyWarningCount( TidyDoc tdoc )
{
    TidyDocImpl* impl = tidyDocToImpl( tdoc );
    uint count = 0xFFFFFFFF;
    if ( impl )
        count = impl->warnings;
    return count;
}
uint        tidyAccessWarningCount( TidyDoc tdoc )
{
    TidyDocImpl* impl = tidyDocToImpl( tdoc );
    uint count = 0xFFFFFFFF;
    if ( impl )
        count = impl->accessErrors;
    return count;
}
uint        tidyConfigErrorCount( TidyDoc tdoc )
{
    TidyDocImpl* impl = tidyDocToImpl( tdoc );
    uint count = 0xFFFFFFFF;
    if ( impl )
        count = impl->optionErrors;
    return count;
}


/* Error reporting functions 
*/
void         tidyErrorSummary( TidyDoc tdoc )
{
    TidyDocImpl* impl = tidyDocToImpl( tdoc );
    if ( impl )
        ErrorSummary( impl );
}
void         tidyGeneralInfo( TidyDoc tdoc )
{
    TidyDocImpl* impl = tidyDocToImpl( tdoc );
    if ( impl )
        GeneralInfo( impl );
}


/* I/O Functions
**
** Initial version supports only whole-file operations.
** Do not expose Tidy StreamIn or Out data structures - yet.
*/

/* Parse/load Functions
**
** HTML/XHTML version determined from input.
*/
int   tidyParseFile( TidyDoc tdoc, ctmbstr filnam )
{
    TidyDocImpl* doc = tidyDocToImpl( tdoc );
    return tidyDocParseFile( doc, filnam );
}
int   tidyParseStdin( TidyDoc tdoc )
{
    TidyDocImpl* doc = tidyDocToImpl( tdoc );
    return tidyDocParseStdin( doc );
}
int   tidyParseString( TidyDoc tdoc, ctmbstr content )
{
    TidyDocImpl* doc = tidyDocToImpl( tdoc );
    return tidyDocParseString( doc, content );
}
int   tidyParseBuffer( TidyDoc tdoc, TidyBuffer* inbuf )
{
    TidyDocImpl* doc = tidyDocToImpl( tdoc );
    return tidyDocParseBuffer( doc, inbuf );
}
int   tidyParseSource( TidyDoc tdoc, TidyInputSource* source )
{
    TidyDocImpl* doc = tidyDocToImpl( tdoc );
    return tidyDocParseSource( doc, source );
}


int   tidyDocParseFile( TidyDocImpl* doc, ctmbstr filnam )
{
    int status = -ENOENT;
    FILE* fin = fopen( filnam, "rb" );

#if PRESERVE_FILE_TIMES
    struct stat sbuf = {0};
    /* get last modified time */
    ClearMemory( &doc->filetimes, sizeof(doc->filetimes) );
    if ( fin && cfgBool(doc,TidyKeepFileTimes) &&
         fstat(fileno(fin), &sbuf) != -1 )
    {
          doc->filetimes.actime  = sbuf.st_atime;
          doc->filetimes.modtime = sbuf.st_mtime;
    }
#endif

    if ( fin )
    {
        StreamIn* in = FileInput( doc, fin, cfg( doc, TidyInCharEncoding ));
        status = tidyDocParseStream( doc, in );
        freeFileSource(&in->source, yes);
        freeStreamIn(in);
    }
    else /* Error message! */
        FileError( doc, filnam, TidyError );
    return status;
}

int   tidyDocParseStdin( TidyDocImpl* doc )
{
    StreamIn* in = FileInput( doc, stdin, cfg( doc, TidyInCharEncoding ));
    int status = tidyDocParseStream( doc, in );
    freeStreamIn(in);
    return status;
}

int   tidyDocParseBuffer( TidyDocImpl* doc, TidyBuffer* inbuf )
{
    int status = -EINVAL;
    if ( inbuf )
    {
        StreamIn* in = BufferInput( doc, inbuf, cfg( doc, TidyInCharEncoding ));
        status = tidyDocParseStream( doc, in );
        freeStreamIn(in);
    }
    return status;
}

int   tidyDocParseString( TidyDocImpl* doc, ctmbstr content )
{
    int status = -EINVAL;
    TidyBuffer inbuf = {0};
    StreamIn* in = NULL;

    if ( content )
    {
        tidyBufAttach( &inbuf, (void*)content, tmbstrlen(content)+1 );
        in = BufferInput( doc, &inbuf, cfg( doc, TidyInCharEncoding ));
        status = tidyDocParseStream( doc, in );
        tidyBufDetach( &inbuf );
        freeStreamIn(in);
    }
    return status;
}

int   tidyDocParseSource( TidyDocImpl* doc, TidyInputSource* source )
{
    StreamIn* in = UserInput( doc, source, cfg( doc, TidyInCharEncoding ));
    int status = tidyDocParseStream( doc, in );
    freeStreamIn(in);
    return status;
}


/* Print/save Functions
**
*/
int         tidySaveFile( TidyDoc tdoc, ctmbstr filnam )
{
    TidyDocImpl* doc = tidyDocToImpl( tdoc );
    return tidyDocSaveFile( doc, filnam );
}
int         tidySaveStdout( TidyDoc tdoc )
{
    TidyDocImpl* doc = tidyDocToImpl( tdoc );
    return tidyDocSaveStdout( doc );
}
int         tidySaveString( TidyDoc tdoc, tmbstr buffer, uint* buflen )
{
    TidyDocImpl* doc = tidyDocToImpl( tdoc );
    return tidyDocSaveString( doc, buffer, buflen );
}
int         tidySaveBuffer( TidyDoc tdoc, TidyBuffer* outbuf )
{
    TidyDocImpl* doc = tidyDocToImpl( tdoc );
    return tidyDocSaveBuffer( doc, outbuf );
}
int         tidySaveSink( TidyDoc tdoc, TidyOutputSink* sink )
{
    TidyDocImpl* doc = tidyDocToImpl( tdoc );
    return tidyDocSaveSink( doc, sink );
}

int         tidyDocSaveFile( TidyDocImpl* doc, ctmbstr filnam )
{
    int status = -ENOENT;
    FILE* fout = NULL;

    /* Don't zap input file if no output */
    if ( doc->errors > 0 &&
         cfgBool(doc, TidyWriteBack) && !cfgBool(doc, TidyForceOutput) )
        status = tidyDocStatus( doc );
    else 
        fout = fopen( filnam, "wb" );

    if ( fout )
    {
        uint outenc = cfg( doc, TidyOutCharEncoding );
        uint nl = cfg( doc, TidyNewline );
        StreamOut* out = FileOutput( fout, outenc, nl );

        status = tidyDocSaveStream( doc, out );

        fclose( fout );
        MemFree( out );

#if PRESERVE_FILE_TIMES
        if ( doc->filetimes.actime )
        {
            /* set file last accessed/modified times to original values */
            utime( filnam, &doc->filetimes );
            ClearMemory( &doc->filetimes, sizeof(doc->filetimes) );
        }
#endif /* PRESERVFILETIMES */
    }
    if ( status < 0 ) /* Error message! */
        FileError( doc, filnam, TidyError );
    return status;
}



/* Note, _setmode() does NOT work on Win2K Pro w/ VC++ 6.0 SP3.
** The code has been left in in case it works w/ other compilers
** or operating systems.  If stdout is in Text mode, be aware that
** it will garble UTF16 documents.  In text mode, when it encounters
** a single byte of value 10 (0xA), it will insert a single byte 
** value 13 (0xD) just before it.  This has the effect of garbling
** the entire document.
*/

#if !defined(NO_SETMODE_SUPPORT)

#if defined(_WIN32) || defined(OS2_OS)
#include <fcntl.h>
#include <io.h>
#endif

#endif

int         tidyDocSaveStdout( TidyDocImpl* doc )
{
#if !defined(NO_SETMODE_SUPPORT)

#if defined(_WIN32) || defined(OS2_OS)
    int oldstdoutmode = -1, oldstderrmode = -1;
#endif

#endif
    int status = 0;
    uint outenc = cfg( doc, TidyOutCharEncoding );
    uint nl = cfg( doc, TidyNewline );
    StreamOut* out = FileOutput( stdout, outenc, nl );

#if !defined(NO_SETMODE_SUPPORT)

#if defined(_WIN32) || defined(OS2_OS)
    oldstdoutmode = setmode( fileno(stdout), _O_BINARY );
    oldstderrmode = setmode( fileno(stderr), _O_BINARY );
#endif

#endif

    if ( 0 == status )
      status = tidyDocSaveStream( doc, out );

    fflush(stdout);
    fflush(stderr);

#if !defined(NO_SETMODE_SUPPORT)

#if defined(_WIN32) || defined(OS2_OS)
    if ( oldstdoutmode != -1 )
        oldstdoutmode = setmode( fileno(stdout), oldstdoutmode );
    if ( oldstderrmode != -1 )
        oldstderrmode = setmode( fileno(stderr), oldstderrmode );
#endif

#endif

    MemFree( out );
    return status;
}

int         tidyDocSaveString( TidyDocImpl* doc, tmbstr buffer, uint* buflen )
{
    uint outenc = cfg( doc, TidyOutCharEncoding );
    uint nl = cfg( doc, TidyNewline );
    TidyBuffer outbuf = {0};

    StreamOut* out = BufferOutput( &outbuf, outenc, nl );
    int status = tidyDocSaveStream( doc, out );

    if ( outbuf.size > *buflen )
        status = -ENOMEM;
    else
        memcpy( buffer, outbuf.bp, outbuf.size );

    *buflen = outbuf.size;
    tidyBufFree( &outbuf );
    MemFree( out );
    return status;
}

int         tidyDocSaveBuffer( TidyDocImpl* doc, TidyBuffer* outbuf )
{
    int status = -EINVAL;
    if ( outbuf )
    {
        uint outenc = cfg( doc, TidyOutCharEncoding );
        uint nl = cfg( doc, TidyNewline );
        StreamOut* out = BufferOutput( outbuf, outenc, nl );
    
        status = tidyDocSaveStream( doc, out );
        MemFree( out );
    }
    return status;
}

int         tidyDocSaveSink( TidyDocImpl* doc, TidyOutputSink* sink )
{
    uint outenc = cfg( doc, TidyOutCharEncoding );
    uint nl = cfg( doc, TidyNewline );
    StreamOut* out = UserOutput( sink, outenc, nl );
    int status = tidyDocSaveStream( doc, out );
    MemFree( out );
    return status;
}

int         tidyDocStatus( TidyDocImpl* doc )
{
    if ( doc->errors > 0 )
        return 2;
    if ( doc->warnings > 0 || doc->accessErrors > 0 )
        return 1;
    return 0;
}



int         tidyCleanAndRepair( TidyDoc tdoc )
{
    TidyDocImpl* impl = tidyDocToImpl( tdoc );
    if ( impl )
      return tidyDocCleanAndRepair( impl );
    return -EINVAL;
}

int         tidyRunDiagnostics( TidyDoc tdoc )
{
    TidyDocImpl* impl = tidyDocToImpl( tdoc );
    if ( impl )
      return tidyDocRunDiagnostics( impl );
    return -EINVAL;
}


/* Workhorse functions.
**
** Parse requires input source, all input config items 
** and diagnostic sink to have all been set before calling.
**
** Emit likewise requires that document sink and all
** pretty printing options have been set.
*/
static ctmbstr integrity = "\nPanic - tree has lost its integrity\n";

int         tidyDocParseStream( TidyDocImpl* doc, StreamIn* in )
{
    Bool xmlIn = cfgBool( doc, TidyXmlTags );
    int bomEnc;

    assert( doc != NULL && in != NULL );
    assert( doc->docIn == NULL );
    doc->docIn = in;

    TakeConfigSnapshot( doc );    /* Save config state */
    FreeLexer( doc );
    FreeAnchors( doc );

    FreeNode(doc, &doc->root);
    ClearMemory(&doc->root, sizeof(Node));

    if (doc->givenDoctype)
        MemFree(doc->givenDoctype);

    doc->givenDoctype = NULL;

    doc->lexer = NewLexer( doc );
    /* doc->lexer->root = &doc->root; */
    doc->root.line = doc->lexer->lines;
    doc->root.column = doc->lexer->columns;
    doc->inputHadBOM = no;

    bomEnc = ReadBOMEncoding(in);

    if (bomEnc != -1)
    {
        in->encoding = bomEnc;
        SetOptionInt(doc, TidyInCharEncoding, bomEnc);
    }

#ifdef TIDY_WIN32_MLANG_SUPPORT
    if (in->encoding > WIN32MLANG)
        Win32MLangInitInputTranscoder(in, in->encoding);
#endif /* TIDY_WIN32_MLANG_SUPPORT */

    /* Tidy doesn't alter the doctype for generic XML docs */
    if ( xmlIn )
    {
        ParseXMLDocument( doc );
        if ( !CheckNodeIntegrity( &doc->root ) )
            FatalError( integrity );
    }
    else
    {
        doc->warnings = 0;
        ParseDocument( doc );
        if ( !CheckNodeIntegrity( &doc->root ) )
            FatalError( integrity );
    }

#ifdef TIDY_WIN32_MLANG_SUPPORT
    Win32MLangUninitInputTranscoder(in);
#endif /* TIDY_WIN32_MLANG_SUPPORT */

    doc->docIn = NULL;
    return tidyDocStatus( doc );
}

int         tidyDocRunDiagnostics( TidyDocImpl* doc )
{
    uint acclvl = cfg( doc, TidyAccessibilityCheckLevel );
    Bool quiet = cfgBool( doc, TidyQuiet );
    Bool force = cfgBool( doc, TidyForceOutput );

    if ( !quiet )
    {

        ReportMarkupVersion( doc );
        ReportNumWarnings( doc );
    }
    
    if ( doc->errors > 0 && !force )
        NeedsAuthorIntervention( doc );

#if SUPPORT_ACCESSIBILITY_CHECKS
     if ( acclvl > 0 )
         AccessibilityChecks( doc );
#endif

     return tidyDocStatus( doc );
}

int         tidyDocCleanAndRepair( TidyDocImpl* doc )
{
    Bool word2K   = cfgBool( doc, TidyWord2000 );
    Bool logical  = cfgBool( doc, TidyLogicalEmphasis );
    Bool clean    = cfgBool( doc, TidyMakeClean );
    Bool dropFont = cfgBool( doc, TidyDropFontTags );
    Bool htmlOut  = cfgBool( doc, TidyHtmlOut );
    Bool xmlOut   = cfgBool( doc, TidyXmlOut );
    Bool xhtmlOut = cfgBool( doc, TidyXhtmlOut );
    Bool xmlDecl  = cfgBool( doc, TidyXmlDecl );
    Bool tidyMark = cfgBool( doc, TidyMark );
    Node* node;

    /* simplifies <b><b> ... </b> ...</b> etc. */
    NestedEmphasis( doc, &doc->root );

    /* cleans up <dir>indented text</dir> etc. */
    List2BQ( doc, &doc->root );
    BQ2Div( doc, &doc->root );

    /* replaces i by em and b by strong */
    if ( logical )
        EmFromI( doc, &doc->root );

    if ( word2K && IsWord2000(doc) )
    {
        /* prune Word2000's <![if ...]> ... <![endif]> */
        DropSections( doc, &doc->root );

        /* drop style & class attributes and empty p, span elements */
        CleanWord2000( doc, &doc->root );
    }

    /* replaces presentational markup by style rules */
    if ( clean || dropFont )
        CleanDocument( doc );

    /*  Move terminating <br /> tags from out of paragraphs  */
    /*!  Do we want to do this for all block-level elements?  */

    /* This is disabled due to http://tidy.sf.net/bug/681116 */
#if 0
    FixBrakes( doc, FindBody( doc ));
#endif

    /*  Reconcile http-equiv meta element with output encoding  */
    if (cfg( doc, TidyOutCharEncoding) != RAW
#ifndef NO_NATIVE_ISO2022_SUPPORT
        && cfg( doc, TidyOutCharEncoding) != ISO2022
#endif
        )
        VerifyHTTPEquiv( doc, FindHEAD( doc ));

    if ( !CheckNodeIntegrity( &doc->root ) )
        FatalError( integrity );

    /* remember given doctype for reporting */
    node = FindDocType(doc);
    if (node)
    {
        AttVal* fpi = GetAttrByName(node, "PUBLIC");
        if (AttrHasValue(fpi))
            doc->givenDoctype = tmbstrdup(fpi->value);
    }

    if ( doc->root.content )
    {
        /* If we had XHTML input but want HTML output */
        if ( htmlOut && doc->lexer->isvoyager )
        {
            Node* node = FindDocType(doc);
            /* Remove reference, but do not free */
            if (node)
              RemoveNode(node);
        }

        if (xhtmlOut && !htmlOut)
        {
            SetXHTMLDocType(doc);
            FixAnchors(doc, &doc->root, yes, yes, yes);
            FixXhtmlNamespace(doc, yes);
            FixLanguageInformation(doc, &doc->root, yes, yes);
        }
        else
        {
            FixDocType(doc);
            FixAnchors(doc, &doc->root, yes, yes, no);
            FixXhtmlNamespace(doc, no);
            FixLanguageInformation(doc, &doc->root, no, yes);
        }

        if (tidyMark )
            AddGenerator(doc);
    }

    /* ensure presence of initial <?xml version="1.0"?> */
    if ( xmlOut && xmlDecl )
        FixXmlDecl( doc );

    return tidyDocStatus( doc );
}

int         tidyDocSaveStream( TidyDocImpl* doc, StreamOut* out )
{
    Bool showMarkup  = cfgBool( doc, TidyShowMarkup );
    Bool forceOutput = cfgBool( doc, TidyForceOutput );
#if SUPPORT_UTF16_ENCODINGS
    Bool outputBOM   = ( cfg(doc, TidyOutputBOM) == yes );
    Bool smartBOM    = ( cfg(doc, TidyOutputBOM) == TidyAutoState );
#endif
    Bool xmlOut      = cfgBool( doc, TidyXmlOut );
    Bool xhtmlOut    = cfgBool( doc, TidyXhtmlOut );
    Bool bodyOnly    = cfgBool( doc, TidyBodyOnly );

    Bool dropComments = cfgBool(doc, TidyHideComments);
    Bool makeClean    = cfgBool(doc, TidyMakeClean);
    Bool asciiChars   = cfgBool(doc, TidyAsciiChars);
    Bool makeBare     = cfgBool(doc, TidyMakeBare);
    Bool escapeCDATA  = cfgBool(doc, TidyEscapeCdata);

    if (escapeCDATA)
        ConvertCDATANodes(doc, &doc->root);

    if (dropComments)
        DropComments(doc, &doc->root);

    if (makeClean)
    {
        /* noop */
        DropFontElements(doc, &doc->root, NULL);
        WbrToSpace(doc, &doc->root);
    }

    if ((makeClean && asciiChars) || makeBare)
        DowngradeTypography(doc, &doc->root);

    if (makeBare)
        /* Note: no longer replaces &nbsp; in */
        /* attribute values / non-text tokens */
        NormalizeSpaces(doc->lexer, &doc->root);
    else
        ReplacePreformattedSpaces(doc, &doc->root);

    if ( showMarkup && (doc->errors == 0 || forceOutput) )
    {
#if SUPPORT_UTF16_ENCODINGS
        /* Output a Byte Order Mark if required */
        if ( outputBOM || (doc->inputHadBOM && smartBOM) )
            outBOM( out );
#endif

        /* No longer necessary. No DOCTYPE == HTML 3.2,
        ** which gives you only the basic character entities,
        ** which are safe in any browser.
        ** if ( !FindDocType(doc) )
        **    SetOptionBool( doc, TidyNumEntities, yes );
        */

        doc->docOut = out;
        if ( xmlOut && !xhtmlOut )
            PPrintXMLTree( doc, 0, 0, &doc->root );
        else if ( bodyOnly )
            PrintBody( doc );
        else
            PPrintTree( doc, 0, 0, &doc->root );

        PFlushLine( doc, 0 );
        doc->docOut = NULL;
    }

    ResetConfigToSnapshot( doc );
    return tidyDocStatus( doc );
}

/* Tree traversal functions
**
** The big issue here is the degree to which we should mimic
** a DOM and/or SAX nodes.
** 
** Is it 100% possible (and, if so, how difficult is it) to 
** emit SAX events from this API?  If SAX events are possible,
** is that 100% of data needed to build a DOM?
*/

TidyNode    tidyGetRoot( TidyDoc tdoc )
{
    TidyDocImpl* impl = tidyDocToImpl( tdoc );
    return tidyImplToNode( &impl->root );
}

TidyNode    tidyGetHtml( TidyDoc tdoc )
{
  TidyDocImpl* impl = tidyDocToImpl( tdoc );
  Node* node = NULL;
  if ( impl )
      node = FindHTML( impl );
  return tidyImplToNode( node );
}

TidyNode    tidyGetHead( TidyDoc tdoc )
{
  TidyDocImpl* impl = tidyDocToImpl( tdoc );
  Node* node = NULL;
  if ( impl )
      node = FindHEAD( impl );
  return tidyImplToNode( node );
}

TidyNode    tidyGetBody( TidyDoc tdoc )
{
  TidyDocImpl* impl = tidyDocToImpl( tdoc );
  Node* node = NULL;
  if ( impl )
      node = FindBody( impl );
  return tidyImplToNode( node );
}

/* parent / child */
TidyNode    tidyGetParent( TidyNode tnod )
{
  Node* nimp = tidyNodeToImpl( tnod );
  return tidyImplToNode( nimp->parent );
}
TidyNode    tidyGetChild( TidyNode tnod )
{
  Node* nimp = tidyNodeToImpl( tnod );
  return tidyImplToNode( nimp->content );
}

/* siblings */
TidyNode    tidyGetNext( TidyNode tnod )
{
  Node* nimp = tidyNodeToImpl( tnod );
  return tidyImplToNode( nimp->next );
}
TidyNode    tidyGetPrev( TidyNode tnod )
{
  Node* nimp = tidyNodeToImpl( tnod );
  return tidyImplToNode( nimp->prev );
}

/* Node info */
TidyNodeType tidyNodeGetType( TidyNode tnod )
{
  Node* nimp = tidyNodeToImpl( tnod );
  TidyNodeType ntyp = TidyNode_Root;
  if ( nimp )
    ntyp = (TidyNodeType) nimp->type;
  return ntyp;
}

uint tidyNodeLine( TidyNode tnod )
{
  Node* nimp = tidyNodeToImpl( tnod );
  uint line = 0;
  if ( nimp )
    line = nimp->line;
  return line;
}
uint tidyNodeColumn( TidyNode tnod )
{
  Node* nimp = tidyNodeToImpl( tnod );
  uint col = 0;
  if ( nimp )
    col = nimp->column;
  return col;
}

ctmbstr        tidyNodeGetName( TidyNode tnod )
{
  Node* nimp = tidyNodeToImpl( tnod );
  ctmbstr nnam = NULL;
  if ( nimp )
    nnam = nimp->element;
  return nnam;
}


Bool  tidyNodeHasText( TidyDoc tdoc, TidyNode tnod )
{
  TidyDocImpl* doc = tidyDocToImpl( tdoc );
  if ( doc )
      return nodeHasText( doc, tidyNodeToImpl(tnod) );
  return no;
}


Bool  tidyNodeGetText( TidyDoc tdoc, TidyNode tnod, TidyBuffer* outbuf )
{
  TidyDocImpl* doc = tidyDocToImpl( tdoc );
  Node* nimp = tidyNodeToImpl( tnod );
  if ( doc && nimp && outbuf )
  {
      uint outenc     = cfg( doc, TidyOutCharEncoding );
      uint nl         = cfg( doc, TidyNewline );
      StreamOut* out  = BufferOutput( outbuf, outenc, nl );
      Bool xmlOut     = cfgBool( doc, TidyXmlOut );
      Bool xhtmlOut   = cfgBool( doc, TidyXhtmlOut );

      doc->docOut = out;
      if ( xmlOut && !xhtmlOut )
          PPrintXMLTree( doc, 0, 0, nimp );
      else
          PPrintTree( doc, 0, 0, nimp );

      PFlushLine( doc, 0 );
      doc->docOut = NULL;
  
      MemFree( out );
      return yes;
  }
  return no;
}


Bool tidyNodeIsProp( TidyDoc tdoc, TidyNode tnod )
{
#pragma unused(tdoc)

  Node* nimp = tidyNodeToImpl( tnod );
  Bool isProprietary = yes;
  if ( nimp )
  {
    switch ( nimp->type )
    {
    case RootNode:
    case DocTypeTag:
    case CommentTag:
    case XmlDecl:
    case ProcInsTag:
    case TextNode:
    case CDATATag:
        isProprietary = no;
        break;

    case SectionTag:
    case AspTag:
    case JsteTag:
    case PhpTag:
        isProprietary = yes;
        break;

    case StartTag:
    case EndTag:
    case StartEndTag:
        isProprietary = ( nimp->tag
                          ? (nimp->tag->versions&VERS_PROPRIETARY)!=0
                          : yes );
        break;
    }
  }
  return isProprietary;
}

TidyTagId tidyNodeGetId(TidyNode tnod)
{
    Node* nimp = tidyNodeToImpl(tnod);

    TidyTagId tagId = TidyTag_UNKNOWN;
    if (nimp && nimp->tag)
        tagId = nimp->tag->id;

    return tagId;
}


/* Null for non-element nodes and all pure HTML
cmbstr       tidyNodeNsLocal( TidyNode tnod )
{
}
cmbstr       tidyNodeNsPrefix( TidyNode tnod )
{
}
cmbstr       tidyNodeNsUri( TidyNode tnod )
{
}
*/

/* Iterate over attribute values */
TidyAttr    tidyAttrFirst( TidyNode tnod )
{
  Node* nimp = tidyNodeToImpl( tnod );
  AttVal* attval = NULL;
  if ( nimp )
    attval = nimp->attributes;
  return tidyImplToAttr( attval );
}
TidyAttr    tidyAttrNext( TidyAttr tattr )
{
  AttVal* attval = tidyAttrToImpl( tattr );
  AttVal* nxtval = NULL;
  if ( attval )
    nxtval = attval->next;
  return tidyImplToAttr( nxtval );
}

ctmbstr       tidyAttrName( TidyAttr tattr )
{
  AttVal* attval = tidyAttrToImpl( tattr );
  ctmbstr anam = NULL;
  if ( attval )
    anam = attval->attribute;
  return anam;
}
ctmbstr       tidyAttrValue( TidyAttr tattr )
{
  AttVal* attval = tidyAttrToImpl( tattr );
  ctmbstr aval = NULL;
  if ( attval )
    aval = attval->value;
  return aval;
}

/* Null for pure HTML
ctmbstr       tidyAttrNsLocal( TidyAttr tattr )
{
}
ctmbstr       tidyAttrNsPrefix( TidyAttr tattr )
{
}
ctmbstr       tidyAttrNsUri( TidyAttr tattr )
{
}
*/

TidyAttrId tidyAttrGetId( TidyAttr tattr )
{
  AttVal* attval = tidyAttrToImpl( tattr );
  TidyAttrId attrId = TidyAttr_UNKNOWN;
  if ( attval && attval->dict )
    attrId = attval->dict->id;
  return attrId;
}
Bool tidyAttrIsProp( TidyAttr tattr )
{
  AttVal* attval = tidyAttrToImpl( tattr );
  Bool isProprietary = yes;
  if ( attval )
    isProprietary = ( attval->dict 
                      ? (attval->dict->versions & VERS_PROPRIETARY) != 0
                      : yes );
  return isProprietary;
}
