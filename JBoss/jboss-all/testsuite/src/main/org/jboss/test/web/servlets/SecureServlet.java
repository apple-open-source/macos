package org.jboss.test.web.servlets;

import java.io.IOException;
import java.io.PrintWriter;
import java.security.Principal;
import javax.servlet.ServletConfig;
import javax.servlet.ServletException;
import javax.servlet.http.HttpServlet;
import javax.servlet.http.HttpServletRequest;
import javax.servlet.http.HttpServletResponse;
import javax.servlet.http.HttpSession;

/** A servlet that is secured by the web.xml descriptor. When accessed
 * it simply prints the getUserPrincipal that accessed the url.
 *
 * @author Scott.Stark@jboss.org
 * @version $Revision: 1.4 $
 */
public class SecureServlet extends HttpServlet
{
   protected void processRequest(HttpServletRequest request, HttpServletResponse response)
      throws ServletException, IOException
   {
      Principal user = request.getUserPrincipal();
      HttpSession session = request.getSession(false);
      response.setContentType("text/html");
      PrintWriter out = response.getWriter();
      out.println("<html>");
      out.println("<head><title>SecureServlet</title></head>");
      out.println("<h1>SecureServlet Accessed</h1>");
      out.println("<body>");
      out.println("You have accessed this servlet as user:"+user);
      if( session != null )
         out.println("<br>The session id is: "+session.getId());
      else
         out.println("<br>There is no session");
      out.println("</body></html>");
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
