package org.jboss.test.classloader.scoping.override.web;

import java.io.IOException;
import java.io.PrintWriter;
import javax.servlet.http.HttpServlet;
import javax.servlet.http.HttpServletRequest;
import javax.servlet.http.HttpServletResponse;
import javax.servlet.ServletConfig;
import javax.servlet.ServletException;
import javax.naming.InitialContext;
import javax.naming.Context;

/** A servlet that validates the context java:comp/env context when scoping
 * is enabled.
 *
 * @author Scott.Stark@jboss.org
 * @version $Revision: 1.1.2.1 $
 */
public class ENCServlet extends HttpServlet
{
   /**
    *
    * @param servletConfig
    * @throws ServletException
    */
   public void init(ServletConfig servletConfig) throws ServletException
   {
      super.init(servletConfig);
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

   private void processRequest(HttpServletRequest request, HttpServletResponse response)
      throws ServletException, IOException
   {
      response.setContentType("text/html");
      PrintWriter pw = response.getWriter();
      pw.println("<html><head><title>ENCServlet Scoping Test</title></head>");
      pw.println("<body><h1>ENCServlet Scoping Test</h1>");

      try
      {
         InitialContext ctx = new InitialContext();
         Context env = (Context) ctx.lookup("java:comp/env");
         pw.println("env = "+env);
         String value1 = (String) env.lookup("prop1");
         if( value1.equals("value1") == false )
            throw new IllegalStateException("prop1 != value1, "+value1);
      }
      catch (Exception e)
      {
         throw new ServletException("Failed to validate ENC", e);
      }
      pw.println("</body></html>");
   }
}
