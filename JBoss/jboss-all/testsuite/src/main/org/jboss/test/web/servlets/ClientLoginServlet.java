package org.jboss.test.web.servlets;           

import java.io.IOException;
import java.io.PrintWriter;
import java.security.Principal;
import javax.naming.InitialContext;
import javax.naming.NamingException;
import javax.servlet.ServletConfig;
import javax.servlet.ServletException;
import javax.servlet.http.HttpServlet;
import javax.servlet.http.HttpServletRequest;
import javax.servlet.http.HttpServletResponse;
import javax.security.auth.login.LoginContext;
import javax.security.auth.login.LoginException;

import org.jboss.security.auth.callback.UsernamePasswordHandler;
import org.jboss.test.web.interfaces.StatelessSession;
import org.jboss.test.web.interfaces.StatelessSessionHome;

/** A servlet that performs a JAAS login to access a secure EJB.

@author Scott.Stark@jboss.org
@version $Revision: 1.2 $
*/
public class ClientLoginServlet extends HttpServlet
{
    protected void processRequest(HttpServletRequest request, HttpServletResponse response)
        throws ServletException, IOException
    {
        LoginContext lc = null;
        String echoMsg = null;
        try
        {
            lc = doLogin("jduke", "theduke");
            InitialContext ctx = new InitialContext();
            StatelessSessionHome home = (StatelessSessionHome) ctx.lookup("java:comp/env/ejb/SecuredEJB");
            StatelessSession bean = home.create();
            echoMsg = bean.echo("ClientLoginServlet called SecuredEJB.echo");
        }
        catch(LoginException e)
        {
            throw new ServletException("Failed to login to client-login domain as jduke", e);
        }
        catch(Exception e)
        {
            throw new ServletException("Failed to access SecuredEJB", e);
        }
        finally
        {
            if( lc != null )
            {
                try
                {
                    lc.logout();
                }
                catch(LoginException e)
                {
                }
            }
        }

        response.setContentType("text/html");
        PrintWriter out = response.getWriter();
        out.println("<html>");
        out.println("<head><title>ClientLoginServlet</title></head>");
        out.println("<h1>ClientLoginServlet Accessed</h1>");
        out.println("<body>Login as user=jduke succeeded.<br>SecuredEJB.echo returned:"+echoMsg+"</body>");
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

    private LoginContext doLogin(String username, String password) throws LoginException
    {
        UsernamePasswordHandler handler = new UsernamePasswordHandler(username, password.toCharArray());
        LoginContext lc = new LoginContext("client-login", handler);
        lc.login();
        return lc;
    }
}
