// ========================================================================
// Copyright (c) 2001 Mort Bay Consulting (Australia) Pty. Ltd.
// $Id: ServletHttpContext.java,v 1.15.2.7 2003/07/11 00:55:03 jules_gosnell Exp $
// ========================================================================

package org.mortbay.jetty.servlet;

import java.io.IOException;
import javax.servlet.ServletContext;
import org.mortbay.http.HttpContext;
import org.mortbay.http.HttpRequest;
import org.mortbay.http.HttpResponse;
import org.mortbay.http.HttpException;
import org.mortbay.util.Code;

/* ------------------------------------------------------------ */
/** ServletHttpContext.
 * Extends HttpContext with conveniance methods for adding servlets.
 * Enforces a single ServletHandler per context.
 * @version $Id: ServletHttpContext.java,v 1.15.2.7 2003/07/11 00:55:03 jules_gosnell Exp $
 * @author Greg Wilkins (gregw)
 */
public class ServletHttpContext extends HttpContext
{    
    /* ------------------------------------------------------------ */
    /** Constructor. 
     */
    public ServletHttpContext()
    {
        super();
    }
    
    /* ------------------------------------------------------------ */
    /**
     * @return The ServletContext. 
     */
    public ServletContext getServletContext()
    {
        ServletHandler shandler=getServletHandler();
        if (shandler!=null)
            return shandler.getServletContext();
        return null;
    }
    
    /* ------------------------------------------------------------ */
    /** Get the context ServletHandler.
     * Conveniance method. If no ServletHandler exists, a new one is added to
     * the context.
     * @return ServletHandler
     */
    public synchronized ServletHandler getServletHandler()
    {
        ServletHandler shandler=(ServletHandler) getHandler(ServletHandler.class);
        if (shandler==null)
        {
            shandler=new ServletHandler();
            addHandler(shandler);
        }
        return shandler;
    }
    
    /* ------------------------------------------------------------ */
    /** Add a servlet to the context.
     * Conveniance method.
     * If no ServletHandler is found in the context, a new one is added.
     * @param pathSpec The pathspec within the context
     * @param className The classname of the servlet.
     * @return The ServletHolder.
     * @exception ClassNotFoundException 
     * @exception InstantiationException 
     * @exception IllegalAccessException 
     */
    public synchronized ServletHolder addServlet(String pathSpec,
                                                 String className)
        throws ClassNotFoundException,
               InstantiationException,
               IllegalAccessException
    {
        return addServlet(className,pathSpec,className);
    }
    
    /* ------------------------------------------------------------ */
    /** Add a servlet to the context.
     * If no ServletHandler is found in the context, a new one is added.
     * @param name The name of the servlet.
     * @param pathSpec The pathspec within the context
     * @param className The classname of the servlet.
     * @return The ServletHolder.
     * @exception ClassNotFoundException 
     * @exception InstantiationException 
     * @exception IllegalAccessException 
     */
    public synchronized ServletHolder addServlet(String name,
                                                 String pathSpec,
                                                 String className)
        throws ClassNotFoundException,
               InstantiationException,
               IllegalAccessException
    {
        return getServletHandler().addServlet(name,pathSpec,className,null);
    }

    /* ------------------------------------------------------------ */
    /** Setup context for serving dynamic servlets.
     * @deprecated Use org.mortbay.jetty.servlet.Invoker
     */
    public synchronized void setDynamicServletPathSpec(String pathSpecInContext)
    {
        Code.warning("setDynamicServletPathSpec is deprecated.");
    }

    /* ------------------------------------------------------------ */
    protected boolean jSecurityCheck(String pathInContext,
                                     HttpRequest request,
                                     HttpResponse response)
            throws IOException
    {
        if (getAuthenticator() instanceof FormAuthenticator &&
            pathInContext.endsWith(FormAuthenticator.__J_SECURITY_CHECK) &&
            getAuthenticator().authenticated(getRealm(),
                                             pathInContext,
                                             request,
                                             response)==null)
            return false;
        return true;
    }
    
    /* ------------------------------------------------------------ */
    public boolean checkSecurityConstraints(String pathInContext,
                                            HttpRequest request,
                                            HttpResponse response)
            throws HttpException, IOException
    {
        if (!super.checkSecurityConstraints(pathInContext,request,response) ||
            ! jSecurityCheck(pathInContext,request,response))
            return false;
        
        return true;
    }
    
    /* ------------------------------------------------------------ */
    public void stop()
        throws InterruptedException
    {
        super.stop();
    }
    
    /* ------------------------------------------------------------ */
    public String toString()
    {
        return "Servlet"+super.toString(); 
    }
    
}
