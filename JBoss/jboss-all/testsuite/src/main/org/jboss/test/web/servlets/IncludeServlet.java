package org.jboss.test.web.servlets;

import java.io.IOException;
import java.io.PrintWriter;
import java.security.Principal;
import javax.servlet.RequestDispatcher;
import javax.servlet.ServletConfig;
import javax.servlet.ServletException;
import javax.servlet.http.HttpServlet;
import javax.servlet.http.HttpServletRequest;
import javax.servlet.http.HttpServletResponse;

/**
 *
 * @author  Scott.Stark@jboss.org
 * @version $Revision: 1.2 $
 */
public class IncludeServlet extends HttpServlet
{
   protected void processRequest(HttpServletRequest request, HttpServletResponse response)
      throws ServletException, IOException
   {
      Principal user = request.getUserPrincipal();
      boolean isSecure = request.getRemoteUser() != null;
      response.setBufferSize(2048);
      PrintWriter out = response.getWriter();
      response.setContentType("text/html");
      out.println("<html>");
      out.println("<head><title>IncludeServlet</title></head>");
      out.println("<h1>IncludeServlet Accessed</h1>");
      out.println("<body>You have accessed this servlet as user:"+user);
      try
      {
         out.println("Accessing /restricted/SecureEJBAccess?includeHead=false<br>");
         RequestDispatcher rd = request.getRequestDispatcher("/restricted/SecureEJBAccess?includeHead=false");
         rd.include(request, response);
      }
      catch(ServletException e)
      {
         if( isSecure == true )
            throw e;
         out.println("Access to /restricted/SecureEJBAccess failed as expected<br>");
      }

      out.println("Accessing /UnsecureEJBAccess?includeHead=false<br>");
      RequestDispatcher rd = request.getRequestDispatcher("/UnsecureEJBAccess?includeHead=false");
      rd.include(request, response);
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
