package org.jboss.test.web.servlets;

import java.io.IOException;
import java.io.PrintWriter;
import java.security.Principal;
import javax.naming.InitialContext;
import javax.naming.Context;
import javax.servlet.ServletException;
import javax.servlet.http.HttpServlet;
import javax.servlet.http.HttpServletRequest;
import javax.servlet.http.HttpServletResponse;

import org.jboss.test.web.interfaces.StatelessSessionLocalHome;
import org.jboss.test.web.interfaces.StatelessSessionLocal;
import org.jboss.logging.Logger;

/** A servlet deployed under an unrestricted path that invokes the method
 * specified as a parameter on a secured EJB.
 *
 * @author  Scott.Stark@jboss.org
 * @version $Revision: 1.2.4.1 $
 */
public class UnsecureEJBServlet extends HttpServlet
{
   Logger log = Logger.getLogger(UnsecureEJBServlet.class);

   protected void processRequest(HttpServletRequest request, HttpServletResponse response)
      throws ServletException, IOException
   {
      boolean includeHead = true;
      String param = request.getParameter("includeHead");
      if( param != null )
         includeHead = Boolean.valueOf(param).booleanValue();
      String method = request.getParameter("method");
      if( method == null )
         method = "echo";

      try
      {
         InitialContext ctx = new InitialContext();
         StatelessSessionLocalHome home = null;
         Context enc = (Context) ctx.lookup("java:comp/env");
         home = (StatelessSessionLocalHome) enc.lookup("ejb/local/SecuredEJB");
         StatelessSessionLocal bean = home.create();
         if( method.equals("echo") )
            bean.echo("UnsecureEJBServlet called SecuredEJB.echo");
         else if( method.equals("unchecked") )
            bean.unchecked();
         else if( method.equals("checkRunAs") )
            bean.checkRunAs();
         else
            throw new IllegalArgumentException("method must be one of: echo, unchecked, checkRunAs");
      }
      catch(Exception e)
      {
         log.error("Access to failed to method: "+method, e);
         throw new ServletException("Access to failed to method: "+method, e);
      }

      Principal user = request.getUserPrincipal();
      PrintWriter out = response.getWriter();
      if( includeHead == true )
      {
         response.setContentType("text/html");
         out.println("<html>");
         out.println("<head><title>UnsecureEJBServlet</title></head><body>");
      }
      out.println("<h1>UnsecureEJBServlet Accessed</h1>");
      out.println("<pre>You have accessed this servlet as user: "+user+"<br>");
      out.println("You have accessed SecuredEJB as user: "+user);
      out.println("You have invoked SecuredEJB."+method);
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
