package org.jboss.test.web.servlets;           

import java.io.IOException;
import java.io.PrintWriter;
import java.security.Principal;
import javax.naming.Context;
import javax.naming.InitialContext;
import javax.naming.NamingException;
import javax.servlet.ServletConfig;
import javax.servlet.ServletException;
import javax.servlet.http.HttpServlet;
import javax.servlet.http.HttpServletRequest;
import javax.servlet.http.HttpServletResponse;

import org.jboss.test.web.interfaces.StatelessSession;
import org.jboss.test.web.interfaces.StatelessSessionHome;

/** 
 *
 * @author  Scott.Stark@jboss.org
 * @version $Revision: 1.4 $
 */
public class SecureEJBServlet extends HttpServlet
{
    protected void processRequest(HttpServletRequest request, HttpServletResponse response)
        throws ServletException, IOException
    {
        String echoMsg = null;
        boolean testPropagation = false;
        boolean includeHead = true;
        String param = request.getParameter("testPropagation");
        if( param != null )
            testPropagation = Boolean.valueOf(param).booleanValue();
        param = request.getParameter("includeHead");
        if( param != null )
            includeHead = Boolean.valueOf(param).booleanValue();

        try
        {
            InitialContext ctx = new InitialContext();
            StatelessSessionHome home = null;
            if( testPropagation == true )
            {
                home = (StatelessSessionHome) ctx.lookup("java:comp/env/ejb/UnsecuredEJB");
                StatelessSession bean = home.create();
                echoMsg = bean.forward("SecureEJBServlet called UnsecuredEJB.forward");
            }
            else
            {
                home = (StatelessSessionHome) ctx.lookup("java:comp/env/ejb/SecuredEJB");
                StatelessSession bean = home.create();
                echoMsg = bean.echo("SecureEJBServlet called SecuredEJB.echo");
            }
        }
        catch(Exception e)
        {
            throw new ServletException("Failed to call SecuredEJB.echo", e);
        }
        Principal user = request.getUserPrincipal();
        PrintWriter out = response.getWriter();
        if( includeHead == true )
        {
           response.setContentType("text/html");
           out.println("<html>");
           out.println("<head><title>ENCServlet</title></head><body>");
        }
        out.println("<h1>SecureServlet Accessed</h1>");
        out.println("<pre>You have accessed this servlet as user: "+user);
        out.println("You have accessed SecuredEJB as user: "+echoMsg);
        out.println("</pre>");
        if( includeHead == true )
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
