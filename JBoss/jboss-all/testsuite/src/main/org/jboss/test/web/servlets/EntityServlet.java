package org.jboss.test.web.servlets;           

import java.io.IOException;
import java.io.PrintWriter;
import javax.naming.Context;
import javax.naming.InitialContext;
import javax.naming.NamingException;
import javax.servlet.ServletConfig;
import javax.servlet.ServletException;
import javax.servlet.http.HttpServlet;
import javax.servlet.http.HttpServletRequest;
import javax.servlet.http.HttpServletResponse;

import org.jboss.test.web.interfaces.Entity;
import org.jboss.test.web.interfaces.EntityHome;
import org.jboss.test.web.util.Util;

/** A servlet that accesses an entity EJB 

@author  Scott.Stark@jboss.org
@version $Revision: 1.2 $
*/
public class EntityServlet extends HttpServlet
{
    protected void processRequest(HttpServletRequest request, HttpServletResponse response)
        throws ServletException, IOException
    {
        try
        {
            InitialContext ctx = new InitialContext();
            Context enc = (Context) ctx.lookup("java:comp/env");
            EntityHome home = (EntityHome) enc.lookup("ejb/Entity");
            Entity bean = home.create(12345, 6789);
            bean.write(7890);
            bean.read();
            bean.remove();
        }
        catch(Exception e)
        {
            throw new ServletException("Failed to call Entity through remote interfaces", e);
        }
        response.setContentType("text/html");
        PrintWriter out = response.getWriter();
        out.println("<html>");
        out.println("<head><title>EntityServlet</title></head>");
        out.println("<body>Tests passed<br>Time:"+Util.getTime()+"</body>");
        out.println("</html>");
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
