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
 * @version $Revision: 1.2 $
 */
public class UnsecureEJBServlet extends HttpServlet
{
   protected void processRequest(HttpServletRequest request, HttpServletResponse response)
      throws ServletException, IOException
   {
      String echoMsg = null;
      boolean includeHead = true;
      String param = request.getParameter("includeHead");
      if( param != null )
         includeHead = Boolean.valueOf(param).booleanValue();

      Exception ex = null;
      try
      {
         InitialContext ctx = new InitialContext();
         StatelessSessionHome home = null;
         home = (StatelessSessionHome) ctx.lookup("java:comp/env/ejb/SecuredEJB");
         StatelessSession bean = home.create();
         echoMsg = bean.echo("UnsecureEJBServlet called SecuredEJB.echo");
      }
      catch(Exception e)
      {
         ex = e;
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
      if( ex != null )
         out.println("Access to SecuredEJB failed with ex="+ex);
      else
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
