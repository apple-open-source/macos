package org.jboss.test.web.servlets;           

import java.io.IOException;
import java.io.PrintWriter;
import java.net.URL;
import java.net.URLClassLoader;
import java.security.Principal;
import javax.naming.Context;
import javax.naming.InitialContext;
import javax.servlet.ServletConfig;
import javax.servlet.ServletException;
import javax.servlet.http.HttpServlet;
import javax.servlet.http.HttpServletRequest;
import javax.servlet.http.HttpServletResponse;

import org.jboss.test.web.util.Util;

/** A servlet that dumps out debugging information about its environment.
 * @author  Scott.Stark@jboss.org
 * @version $Revision: 1.6 $
 */
public class DebugServlet extends HttpServlet
{
    protected void processRequest(HttpServletRequest request, HttpServletResponse response)
        throws ServletException, IOException
    {
        response.setContentType("text/html");
        PrintWriter out = response.getWriter();
        out.println("<html>");
        out.println("<head><title>ENCServlet</title></head>");
        out.println("<h1>Debug Accessed</h1>");
        out.println("<body>");
        out.println("<h2>Call Stack</h2>");
        out.println("<pre>");
        Throwable t = new Throwable("Trace");
        t.printStackTrace(out);
        out.println("</pre>");
        out.println("<h2>ClassLoaders</h2>");
        ClassLoader cl = Thread.currentThread().getContextClassLoader();
        out.println("<pre>");
        Util.dumpClassLoader(cl, out);
        out.println("</pre>");
        out.println("<h2>JNDI</h2>");
        out.println("<pre>");
        try
        {
            InitialContext iniCtx = new InitialContext();
            super.log("InitialContext.env: "+iniCtx.getEnvironment());
            out.println("InitialContext.env: "+iniCtx.getEnvironment());
            out.println("</pre><h3>java:comp</h3><pre>");
            Util.showTree(" ", (Context) iniCtx.lookup("java:comp"), out);
        }
        catch(Exception e)
        {
            super.log("Failed to create InitialContext", e);
            e.printStackTrace(out);
        }
        out.println("</pre></body></html>");
        out.close();
    }

    protected void doGet(HttpServletRequest request, HttpServletResponse response)
        throws ServletException, IOException
    {
        processRequest(request, response);
    }

    protected void doPost(HttpServletRequest request, HttpServletResponse response)
        throws ServletException, IOException
    {
        processRequest(request, response);
    }
}
