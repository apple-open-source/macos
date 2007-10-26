
/*
 * $Xorg:xpapputil.c,v 1.1 2002/06/10 02:54:18 gisburn Exp $
 * 
 * xpapputil - Application level utility library for Xprint
 *
 * 
Copyright 2002-2004 Roland Mainz <roland.mainz@nrubsig.org>

All Rights Reserved.

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
OPEN GROUP BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN
AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Except as contained in this notice, the name of The Open Group shall not be
used in advertising or otherwise to promote the sale, use or other dealings
in this Software without prior written authorization from The Open Group.
 *
 * Author:  Roland Mainz <roland.mainz@nrubsig.org>
 */

#include <stdlib.h>
#include <stdio.h>
 
#include <X11/XprintAppUtil/xpapputil.h>

XpauContext *XpauGetContext( const char *printername )
{
  Display     *pdpy;
  XPContext    pcontext;
  XpauContext *context;
  
  if( XpuGetPrinter(printername, &pdpy, &pcontext) != 1 )
    return NULL; 
  
  if( (context = (XpauContext *)malloc(sizeof(XpauContext))) )
  {
    memset(context, 0, sizeof(XpauContext));
    
    context->pdpy     = pdpy;
    context->pcontext = pcontext;

    if( XpQueryExtension(pdpy, &context->xp_event_base, &context->xp_error_base) == False )
    {
      fprintf(stderr, "XpauGetContext: XpQueryExtension() failed.\n");
      XpauReleaseContext(context);
      return NULL;
    }

    /* It may be better to fetch all this info on demand... */
    context->medium_list      = XpuGetMediumSourceSizeList(pdpy, pcontext, &context->medium_num_list_entries);
    context->resolution_list  = XpuGetResolutionList(pdpy,       pcontext, &context->resolution_num_list_entries);
    context->orientation_list = XpuGetOrientationList(pdpy,      pcontext, &context->orientation_num_list_entries);
    context->plex_list        = XpuGetPlexList(pdpy,             pcontext, &context->plex_num_list_entries);
    context->supported_job_attributes  =  XpuGetSupportedJobAttributes(pdpy, pcontext);
    context->supported_doc_attributes  =  XpuGetSupportedDocAttributes(pdpy, pcontext);
    context->supported_page_attributes =  XpuGetSupportedPageAttributes(pdpy, pcontext);
                                                                                                                     
    return context;
  }
  
  XpuClosePrinterDisplay(pdpy, pcontext);
  
  return NULL;
}

void XpauReleaseContext( XpauContext *context )
{
  if( context )
  {
    if( context->medium_list )
      XpuFreeMediumSourceSizeList(context->medium_list);

    if( context->resolution_list )
      XpuFreeResolutionList(context->resolution_list);

    if( context->orientation_list )
      XpuFreeOrientationList(context->orientation_list);

    if( context->plex_list )
      XpuFreePlexList(context->plex_list);
      
    XpuClosePrinterDisplay(context->pdpy, context->pcontext);

    free(context);
  }
}

static
XpAuErrorValue XpauParseArg( const char *arg_name, const char *arg_value, XpauContext *context, 
                             XpauFlags *jobvaluemask, XpauJobValues *jobvalues,
                             XpauFlags *docvaluemask, XpauDocValues *docvalues,
                             char **jobfilename )
{
  XPAU_DEBUG(printf("XpauParseArg: name='%s', value='%s'\n", arg_name, arg_value));

  if( !strcasecmp(arg_name, "file") ||
      !strcasecmp(arg_name, "tofile") )
  {
    if( *jobfilename )
      free((char *)*jobfilename);
      
    *jobfilename = strdup(arg_value);
    if( !*jobfilename )
      return XpAuError_error_no_memory;
      
    return XpAuError_success;
  }
  else if( !strcasecmp(arg_name, "medium") ||
           !strcasecmp(arg_name, "papersize"))
  {
    if( !(context->medium_list && (context->supported_doc_attributes & XPUATTRIBUTESUPPORTED_DEFAULT_MEDIUM)) )
      return XpAuError_unsupported_medium;

    /* XXX: What about the tray name ? */
    docvalues->medium = XpuFindMediumSourceSizeByName(context->medium_list,
                                                      context->medium_num_list_entries, 
                                                      NULL, arg_value);

    if( !docvalues->medium )
      return XpAuError_unsupported_medium;

    *docvaluemask |= XpauVMedium;
    return XpAuError_success;
  }  
  else if( !strcasecmp(arg_name, "resolution") )
  {   
    if( !(context->resolution_list && (context->supported_doc_attributes & XPUATTRIBUTESUPPORTED_DEFAULT_PRINTER_RESOLUTION)) )
      return XpAuError_unsupported_resolution;

    docvalues->resolution = XpuFindResolutionByName(context->resolution_list,
                                              context->resolution_num_list_entries, 
                                              arg_value);

    if( !docvalues->resolution )
      return XpAuError_unsupported_resolution;

    *docvaluemask |= XpauVResolution;
    return XpAuError_success;
  }
  else if( !strcasecmp(arg_name, "orientation") )
  {
    if( !(context->orientation_list && (context->supported_doc_attributes & XPUATTRIBUTESUPPORTED_CONTENT_ORIENTATION)) )
      return XpAuError_unsupported_orientation;

    docvalues->orientation = XpuFindOrientationByName(context->orientation_list,
                                                      context->orientation_num_list_entries, 
                                                      arg_value);

    if( !docvalues->orientation )
      return XpAuError_unsupported_orientation;

    *docvaluemask |= XpauVOrientation;
    return XpAuError_success;
  }
  else if( !strcasecmp(arg_name, "plex") )
  {
    if( !(context->plex_list && (context->supported_doc_attributes & XPUATTRIBUTESUPPORTED_PLEX)) )
      return XpAuError_unsupported_plex;
      
    docvalues->plex = XpuFindPlexByName(context->plex_list,
                                        context->plex_num_list_entries, 
                                        arg_value);

    if( !docvalues->plex )
      return XpAuError_unsupported_plex;

    *docvaluemask |= XpauVPlex;
    return XpAuError_success;  
  }
  else if( !strcasecmp(arg_name, "copies") )
  {
    if( !(context->supported_job_attributes & XPUATTRIBUTESUPPORTED_COPY_COUNT) )
      return XpAuError_unsupported_copy_count;
      
    jobvalues->copies = atoi(arg_value);
    if( jobvalues->copies <= 1 )
      return XpAuError_unsupported_copy_count;

    *jobvaluemask |= XpauVCopies;
    return XpAuError_success;  
  }
  else if( !strcasecmp(arg_name, "title") )
  {
    if( !(context->supported_job_attributes & XPUATTRIBUTESUPPORTED_JOB_NAME) )
      return XpAuError_unsupported_job_name;

    if( jobvalues->title )
      free((char *)jobvalues->title);
      
    jobvalues->title = strdup(arg_value);
    if( !jobvalues->title )
      return XpAuError_error_no_memory;
      
    *jobvaluemask |= XpauVTitle;
    return XpAuError_success;
  }

  return XpAuError_unknown_argument;
}

XpAuErrorValue XpauParseArgs( XpauContext *context,
                              XpauFlags *jobvaluemask, XpauJobValues *jobvalues,
                              XpauFlags *docvaluemask, XpauDocValues *docvalues,
                              char **jobfilename,
                              const char *argument_string )
{
  char *args_string,
       *arg_name,
       *arg_value,
       *start,
       *s;
  int   result;
  
  if( !argument_string )
    return XpAuError_success;

  args_string = strdup(argument_string); /* Create copy of read-only string that we can write into it... */
  if( !args_string )
    return XpAuError_error_no_memory;

  s = args_string;
  
  while( s != NULL )
  {
    /* Fetch argument name */
    start = s; 
    while( *s != '\0' && *s != '=' )
      s++;   
    if( *s == '\0' )
    {
      free(args_string);
      return XpAuError_unexpected_eos;
    }
    *s++ = '\0';
    arg_name = start;
  
    /* Fetch argument value */
    start = s; 
    while( *s != '\0' && *s != ',' )
      s++;
    if( *s == '\0' )
      s = NULL;
    else
      *s++ = '\0';
    arg_value = start;
    
    result = XpauParseArg(arg_name, arg_value, context, jobvaluemask, jobvalues, docvaluemask, docvalues, jobfilename);
    if( result != XpAuError_success )
    {
      free(args_string);
      return result;
    }
  }

  free(args_string);
  return XpAuError_success;
}

XpAuErrorValue
XpauSetJobValues( XpauContext *context, XpauFlags valuemask, XpauJobValues *jobvalues )
{
  /* "Dry-run" tests */

  /* We can't change job attributes while we are at the "job level"
   * (e.g. after |XpStartJob()| and before |XpEndJob()| */
  XPAU_RETURN_IF_FAIL(context->inJob == False, XpAuError_inside_job);
    
  if( valuemask & XpauVTitle ) 
    XPAU_RETURN_IF_FAIL(jobvalues->title && (context->supported_job_attributes & XPUATTRIBUTESUPPORTED_JOB_NAME), XpAuError_unsupported_job_name);
    
  if( valuemask & XpauVCopies )
    XPAU_RETURN_IF_FAIL((jobvalues->copies > 0) && (context->supported_job_attributes & XPUATTRIBUTESUPPORTED_COPY_COUNT), XpAuError_unsupported_copy_count);

  /* Set values */

  if( valuemask & XpauVTitle )
    XpuSetJobTitle(context->pdpy, context->pcontext, jobvalues->title);

#ifdef NOTNOW
  if( valuemask & XpauVResolution )
    XpuJobDocumentCopies(context->pdpy, context->pcontext, jobvalues->copies);   
#endif /* NOTNOW */

  return XpAuError_success;
}

XpAuErrorValue
XpauSetDocValues( XpauContext *context, XpauFlags valuemask, XpauDocValues *docvalues )
{
  /* "Dry-run" tests */

  /* We can't change document attributes while we are at the "document level"
   * (e.g. after |XpStartDoc()| and before |XpEndDoc()| */
  XPAU_RETURN_IF_FAIL(context->inDocument == False, XpAuError_inside_document);
    
  if( valuemask & XpauVMedium )
    XPAU_RETURN_IF_FAIL(docvalues->medium && (context->supported_doc_attributes & XPUATTRIBUTESUPPORTED_DEFAULT_MEDIUM), XpAuError_unsupported_medium);
  
  if( valuemask & XpauVOrientation )
    XPAU_RETURN_IF_FAIL(docvalues->orientation && (context->supported_doc_attributes & XPUATTRIBUTESUPPORTED_CONTENT_ORIENTATION), XpAuError_unsupported_orientation);

  if( valuemask & XpauVPlex )
    XPAU_RETURN_IF_FAIL(docvalues->plex && (context->supported_doc_attributes & XPUATTRIBUTESUPPORTED_PLEX), XpAuError_unsupported_plex);

  if( valuemask & XpauVResolution )
    XPAU_RETURN_IF_FAIL(docvalues->resolution && (context->supported_doc_attributes & XPUATTRIBUTESUPPORTED_DEFAULT_PRINTER_RESOLUTION), XpAuError_unsupported_resolution);

  if( valuemask & XpauVCopies )
    XPAU_RETURN_IF_FAIL((docvalues->copies > 0) && (context->supported_doc_attributes & XPUATTRIBUTESUPPORTED_COPY_COUNT), XpAuError_unsupported_copy_count);
        
  /* Set values */

  if( valuemask & XpauVMedium )
    XpuSetDocMediumSourceSize(context->pdpy, context->pcontext, docvalues->medium);

  if( valuemask & XpauVOrientation )
    XpuSetDocOrientation(context->pdpy, context->pcontext, docvalues->orientation);

  if( valuemask & XpauVPlex )
    XpuSetDocPlex(context->pdpy, context->pcontext, docvalues->plex);

  if( valuemask & XpauVResolution )
    XpuSetDocResolution(context->pdpy, context->pcontext, docvalues->resolution);
    
  if( valuemask & XpauVCopies )
    XpuSetDocumentCopies(context->pdpy, context->pcontext, docvalues->copies);   

  return XpAuError_success;
}

XpAuErrorValue
XpauSetPageValues( XpauContext *context, XpauFlags valuemask, XpauPageValues *pagevalues )
{
  /* "Dry-run" tests */
  
  /* We can't change page attributes while we are at the "page level"
   * (e.g. after |XpStartPage()| and before |XpEndPage()| */
  XPAU_RETURN_IF_FAIL(context->inPage == False, XpAuError_inside_page);

  if( valuemask & XpauVMedium )
     XPAU_RETURN_IF_FAIL(pagevalues->medium && (context->supported_page_attributes & XPUATTRIBUTESUPPORTED_DEFAULT_MEDIUM), XpAuError_unsupported_medium);
  
  if( valuemask & XpauVOrientation )
     XPAU_RETURN_IF_FAIL(pagevalues->orientation && (context->supported_page_attributes & XPUATTRIBUTESUPPORTED_CONTENT_ORIENTATION), XpAuError_unsupported_orientation);

  if( valuemask & XpauVPlex )
     XPAU_RETURN_IF_FAIL(pagevalues->plex && (context->supported_page_attributes & XPUATTRIBUTESUPPORTED_PLEX), XpAuError_unsupported_plex);

  if( valuemask & XpauVResolution )
     XPAU_RETURN_IF_FAIL(pagevalues->resolution && (context->supported_page_attributes & XPUATTRIBUTESUPPORTED_DEFAULT_PRINTER_RESOLUTION), XpAuError_unsupported_resolution);
       
  /* Set values */

  if( valuemask & XpauVMedium )
    XpuSetPageMediumSourceSize(context->pdpy, context->pcontext, pagevalues->medium);

  if( valuemask & XpauVOrientation )
    XpuSetPageOrientation(context->pdpy, context->pcontext, pagevalues->orientation);

  if( valuemask & XpauVPlex )
    XpuSetPagePlex(context->pdpy, context->pcontext, pagevalues->plex);

  if( valuemask & XpauVResolution )
    XpuSetPageResolution(context->pdpy, context->pcontext, pagevalues->resolution);
    
  return XpAuError_success;
}

XpAuErrorValue XpauStartJob( XpauContext *context, const char *printerfile )
{
  void *handle;
  XpAuErrorValue  result = XpAuError_unspecified_error;
  
  XPAU_RETURN_IF_FAIL(context->inJob == False, XpAuError_inside_job);

  /* Set print context
   * Note that this modifies the available fonts, including build-in printer prints.
   * All XListFonts()/XLoadFont() stuff should be done _after_ setting the print 
   * context to obtain the proper fonts.
   */ 
  XpSetContext(context->pdpy, context->pcontext);
      
  if( printerfile )
  {
    context->print_to_filehandle = XpuStartJobToFile(context->pdpy, context->pcontext, printerfile);
    if( context->print_to_filehandle )
    {
      result = XpAuError_success;
    }
    else
    {
      result = XpAuError_errno;
    }
  }
  else
  {
    XpuStartJobToSpooler(context->pdpy);
    result = XpAuError_success;
  }

  /* Get default printer resolution */   
  if( XpuGetResolution(context->pdpy, context->pcontext, &context->document_dpi_x, &context->document_dpi_y) != 1 )
  {
    result = XpAuError_no_dpi_set;
  }
    
  if( result == XpAuError_success )
  {
    context->inJob         = True;
    context->pscreen       = XpGetScreenOfContext(context->pdpy, context->pcontext);
    context->pscreennumber = XScreenNumberOfScreen(context->pscreen);
  }
  
  return result;
}

XpAuErrorValue XpauEndJob( XpauContext *context )
{
  XPAU_RETURN_IF_FAIL(context->inJob, XpAuError_not_inside_job);
  
  /* End the print job - the final results are sent by the X print
   * server to the spooler sub system.
   */
  XpEndJob(context->pdpy);
  
  /* Be sure to process the X traffic (remember that we registered a
   * "consumer" hook via Xlib internal magic when we print to a
   * file)
   * FixMe: |XpuWaitForPrintFileChild()| should call XFlush() instead!
   */
  XFlush(context->pdpy);

  context->inJob           = False;
  context->pscreen         = NULL;
  context->pscreennumber   = -1;
  context->document_dpi_x  = 0L;
  context->document_dpi_y  = 0L;
     
  if( context->print_to_filehandle )
  {
    if( XpuWaitForPrintFileChild(context->print_to_filehandle) != XPGetDocFinished )
    {
      return XpAuError_errno;
    }
  } 
  
  return XpAuError_success;
}

XpAuErrorValue XpauStartDocument( XpauContext *context, XPDocumentType type )
{ 
  XPAU_RETURN_IF_FAIL(context->inJob      == True,  XpAuError_not_inside_job);
  XPAU_RETURN_IF_FAIL(context->inDocument == False, XpAuError_inside_document);
  XPAU_RETURN_IF_FAIL(context->inPage     == False, XpAuError_inside_page);
  
  XpStartDoc(context->pdpy, type);
  context->inDocument = True;

  return XpAuError_success;
}

XpAuErrorValue XpauEndDocument( XpauContext *context )
{
  XPAU_RETURN_IF_FAIL(context->inDocument, XpAuError_not_inside_document);
     
  XpEndDoc(context->pdpy);
  context->inDocument = False;
  
  return XpAuError_success;
}

XpAuErrorValue XpauStartPage( XpauContext *context, Window pwin )
{ 
  XPAU_RETURN_IF_FAIL(context->inPage == False, XpAuError_inside_page);
  XPAU_RETURN_IF_FAIL(context->inJob  == True,  XpAuError_not_inside_job);
  
  XpStartPage(context->pdpy, pwin);

  /* |XpStartPage()| will generate a "synthetic" |XpStartDoc()|
   * if it was not called yet */
  if( context->inDocument == False )
  {
    context->inDocument = True;
  }
  
  context->inPage = True; 

  return XpAuError_success;
}

XpAuErrorValue XpauEndPage( XpauContext *context )
{
  XPAU_RETURN_IF_FAIL(context->inPage, XpAuError_not_inside_page);

  XpEndPage(context->pdpy);
  context->inPage = False;

  return XpAuError_success;
}

void XpauWaitForPrintNotify(XpauContext *context, int type)
{
  XpuWaitForPrintNotify(context->pdpy, context->xp_event_base, type);
}


const char *XpAuErrorValueToString(XpAuErrorValue value)
{
  char *msg;

  switch(value)
  {
    case XpAuError_success:                  msg = "success" ;                                           break;
    case XpAuError_errno:                    msg = strerror(errno) ;                                     break;
    case XpAuError_error_no_memory:          msg = "out of memory" ;                                     break;
    case XpAuError_unexpected_eos:           msg = "unexpected end of string" ;                          break;
    case XpAuError_unknown_argument:         msg = "unknown argument" ;                                  break;
    case XpAuError_unsupported_medium:       msg = "unsupported print medium" ;                          break;
    case XpAuError_unsupported_resolution:   msg = "unsupported print resolution" ;                      break;
    case XpAuError_unsupported_orientation:  msg = "unsupported orientation" ;                           break;
    case XpAuError_unsupported_plex:         msg = "unsupported plex" ;                                  break;
    case XpAuError_unsupported_copy_count:   msg = "unsupported copy count" ;                            break;
    case XpAuError_unsupported_job_name:     msg = "unsupported job name" ;                              break;
    case XpAuError_no_dpi_set:               msg = "no DPI set (or no default DPI provided by server)" ; break;
    case XpAuError_not_inside_job:           msg = "state error: not inside job" ;                       break;
    case XpAuError_not_inside_document:      msg = "state error: not inside document" ;                  break;
    case XpAuError_not_inside_page:          msg = "stage error: not inside page" ;                      break;
    case XpAuError_inside_job:               msg = "state error: (already) inside job" ;                 break;
    case XpAuError_inside_document:          msg = "state error: (already) inside document" ;            break;
    case XpAuError_inside_page:              msg = "stage error: (already) inside page" ;                break;
    case XpAuError_unspecified_error:        msg = "unspecified error" ;                                 break;
    default:
        msg = "unknown error" ;
        break;
  }
  
  return msg;
}




