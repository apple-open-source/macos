// ========================================================================
// Copyright (c) 1999,2002 Mort Bay Consulting (Australia) Pty. Ltd.
// $Id: HttpHandler.java,v 1.15.2.3 2003/06/04 04:47:41 starksm Exp $
// ========================================================================

package org.mortbay.http;
import java.io.IOException;
import java.io.Serializable;
import org.mortbay.util.LifeCycle;


/* ------------------------------------------------------------ */
/** HTTP handler.
 * The HTTP Handler interface is implemented by classes that wish to
 * receive and handle requests from the HttpServer.  The handle method
 * is called for each request and the handler may ignore, modify or
 * handle the request.
 * Examples of HttpHandler instances include:<UL>
 * <LI>org.mortbay.http.handler.ResourceHandler</LI>
 * <LI>org.mortbay.jetty.servlet.ServletHandler</LI>
 * </UL>
 * @see org.mortbay.http.HttpServer
 * @see org.mortbay.http.HttpContext
 * @version $Id: HttpHandler.java,v 1.15.2.3 2003/06/04 04:47:41 starksm Exp $
 * @author Greg Wilkins (gregw)
 */
public interface HttpHandler extends LifeCycle, Serializable
{
    /* ------------------------------------------------------------ */
    /** Get the name of the handler.
     * @return The name of the handler used for logging and reporting.
     */
    public String getName();
    
    /* ------------------------------------------------------------ */
    public HttpContext getHttpContext();

    /* ------------------------------------------------------------ */
    public void initialize(HttpContext context);
    
    /* ------------------------------------------------------------ */
    /** Handle a request.
     *
     * Note that Handlers are tried in order until one has handled the
     * request. i.e. until request.isHandled() returns true.
     *
     * In broad terms this means, either a response has been commited
     * or request.setHandled(true) has been called.
     *
     * @param pathInContext The context path
     * @param pathParams Path parameters such as encoded Session ID
     * @param request The HttpRequest request
     * @param response The HttpResponse response
     */
    public void handle(String pathInContext,
                       String pathParams,
                       HttpRequest request,
                       HttpResponse response)
        throws HttpException, IOException;
}







