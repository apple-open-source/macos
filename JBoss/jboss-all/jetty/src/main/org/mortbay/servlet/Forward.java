// ===========================================================================
// Copyright (c) 1996 Mort Bay Consulting Pty. Ltd. All rights reserved.
// $Id: Forward.java,v 1.15.2.4 2003/06/04 04:47:56 starksm Exp $
// ---------------------------------------------------------------------------

package org.mortbay.servlet;
import java.io.IOException;
import java.util.Enumeration;
import java.util.HashMap;
import java.util.Map;
import javax.servlet.RequestDispatcher;
import javax.servlet.ServletConfig;
import javax.servlet.ServletContext;
import javax.servlet.ServletException;
import javax.servlet.http.HttpServlet;
import javax.servlet.http.HttpServletRequest;
import javax.servlet.http.HttpServletResponse;
import org.mortbay.util.Code;

/* ------------------------------------------------------------ */
/** Forward Servlet Request.
 * This servlet can be configured with init parameters to use
 * a RequestDispatcher to forward requests.
 *
 * The servlet path of a request is used to look for a initparameter
 * of that name. If a parameter is found, it's value is used to get a
 * RequestDispatcher.
 *
 * @version $Id: Forward.java,v 1.15.2.4 2003/06/04 04:47:56 starksm Exp $
 * @author Greg Wilkins (gregw)
 */
public class Forward extends HttpServlet
{
    /* ------------------------------------------------------------ */
    Map _forwardMap= new HashMap();

    /* ------------------------------------------------------------ */
    public void init(ServletConfig config)
         throws ServletException
    {
        super.init(config);

        Enumeration enum = config.getInitParameterNames();
        while (enum.hasMoreElements())
        {
            String path=(String)enum.nextElement();
            String forward=config.getInitParameter(path);
            _forwardMap.put(path,forward);
        }

    }
    
    /* ------------------------------------------------------------ */
    public void doPost(HttpServletRequest sreq, HttpServletResponse sres) 
        throws ServletException, IOException
    {
        doGet(sreq,sres);
    }
    
    /* ------------------------------------------------------------ */
    public void doGet(HttpServletRequest sreq, HttpServletResponse sres) 
        throws ServletException, IOException
    {
        String path = (String)
            sreq.getAttribute("javax.servlet.include.servlet_path");
        if (path==null)
            path=sreq.getServletPath();
        if (path.length()==0)
        {
            path=(String)sreq.getAttribute("javax.servlet.include.path_info");
            if (path==null)
                path=sreq.getPathInfo();
        }

        String forward=(String)_forwardMap.get(path);
        Code.debug("Forward ",path," to ",forward);
        if (forward!=null)
        {            
            ServletContext context =
                getServletContext().getContext(forward);
            String contextPath=sreq.getContextPath();
            if (contextPath.length()>1)
                forward=forward.substring(contextPath.length());
            
            RequestDispatcher dispatch =
                context.getRequestDispatcher(forward);
            if (dispatch!=null)
            {
                dispatch.forward(sreq,sres);
                return;
            }
        }

        sres.sendError(404);
    }

    /* ------------------------------------------------------------ */
    public String getServletInfo()
    {
        return "Forward Servlet";
    }

    /* ------------------------------------------------------------ */
    public synchronized void destroy()
    {
        Code.debug("Destroyed");
    }
    
}
