// ===========================================================================
// Copyright (c) 1996 Mort Bay Consulting Pty. Ltd. All rights reserved.
// $Id: SendRedirect.java,v 1.1.2.5 2003/06/04 04:47:56 starksm Exp $
// ---------------------------------------------------------------------------

package org.mortbay.servlet;
import java.io.IOException;
import java.io.PrintWriter;
import javax.servlet.ServletException;
import javax.servlet.http.HttpServlet;
import javax.servlet.http.HttpServletRequest;
import javax.servlet.http.HttpServletResponse;
import org.mortbay.html.Heading;
import org.mortbay.html.Page;
import org.mortbay.html.TableForm;
import org.mortbay.util.Code;
import org.mortbay.util.URI;

/* ------------------------------------------------------------ */
/** Dump Servlet Request.
 * 
 */
public class SendRedirect extends HttpServlet
{
    /* ------------------------------------------------------------ */
    public void doGet(HttpServletRequest request, HttpServletResponse response) 
        throws ServletException, IOException
    {
        response.setContentType("text/html");
        response.setHeader("Pragma", "no-cache");
        response.setHeader("Cache-Control", "no-cache,no-store");

        String url=request.getParameter("URL");
        if (url!=null && url.length()>0)
        {
            response.sendRedirect(url);
        }
        else
        {
            PrintWriter pout = response.getWriter();
            Page page=null;
            
            try{
                page = new Page();
                page.title("SendRedirect Servlet");     
                
                page.add(new Heading(1,"SendRedirect Servlet"));
                
                page.add(new Heading(1,"Form to generate Dump content"));
                TableForm tf = new TableForm
                    (response.encodeURL(URI.addPaths(request.getContextPath(),
                                                     request.getServletPath())+
                                        "/action"));
                tf.method("GET");
                tf.addTextField("URL","URL",40,request.getContextPath()+"/dump");
                tf.addButton("Redirect","Redirect");
                page.add(tf);
                page.write(pout);
                pout.close();
            }
            catch (Exception e)
            {
                Code.warning(e);
            }
        }
    }

}
