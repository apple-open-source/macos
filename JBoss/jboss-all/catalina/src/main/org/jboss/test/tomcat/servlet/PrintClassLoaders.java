package org.jboss.test.tomcat.servlet;           

import java.io.PrintWriter;
import java.io.StringWriter;
import java.net.URL;
import java.net.URLClassLoader;
import javax.servlet.*;
import javax.servlet.http.*;

/** 
 *
 * @author  Scott.Stark@jboss.org
 * @version $Revision: 1.2 $
 */
public class PrintClassLoaders extends HttpServlet
{
    /** Processes requests for both HTTP <code>GET</code> and <code>POST</code> methods.
    * @param request servlet request
    * @param response servlet response
    */
    protected void processRequest(HttpServletRequest request, HttpServletResponse response)
        throws ServletException, java.io.IOException
    {
        response.setContentType("text/html");
        java.io.PrintWriter out = response.getWriter();

        out.println("<html>");
        out.println("<head>");
        out.println("<title>Servlet</title>");  
        out.println("</head>");
        out.println("<body><pre>");
        out.println(getClassLoaders());
        out.println("</body></pre>");
        out.println("</html>");
        out.close();
    }

    protected void doGet(HttpServletRequest request, HttpServletResponse response)
        throws ServletException, java.io.IOException
    {
        processRequest(request, response);
    } 

    protected void doPost(HttpServletRequest request, HttpServletResponse response)
        throws ServletException, java.io.IOException
    {
        processRequest(request, response);
    }

    /** Returns a short description of the servlet.
    */
    public String getServletInfo()
    {
        return "A servlet prints the request ClassLoaders";
    }

    public static String getClassLoaders()
    {
        ClassLoader loader = Thread.currentThread().getContextClassLoader();
        StringBuffer buffer = new StringBuffer();
        do
        {
            buffer.append("+++ ");
            buffer.append(loader);
            buffer.append('\n');
            if( loader instanceof URLClassLoader )
            {
                URLClassLoader uloader = (URLClassLoader) loader;
                URL[] paths = uloader.getURLs();
                for(int p = 0; p < paths.length; p ++)
                {
                    buffer.append(" - ");
                    buffer.append(paths[p]);
                    buffer.append('\n');
                }
            }
            loader = loader.getParent();
        } while( loader != null );
        // Write the call stack
        buffer.append("+++ Call stack:\n");
        Throwable t = new RuntimeException();
        StringWriter sw = new StringWriter();
        PrintWriter pw = new PrintWriter(sw);
        t.printStackTrace(pw);
        buffer.append(sw.toString());
        return buffer.toString();
    }
}
