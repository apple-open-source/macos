package org.jboss.test.web.servlets;

import java.io.IOException;
import java.io.PrintWriter;
import javax.servlet.ServletConfig;
import javax.servlet.ServletException;
import javax.servlet.http.HttpServlet;
import javax.servlet.http.HttpServletRequest;
import javax.servlet.http.HttpServletResponse;

import org.jboss.test.web.util.Util;

/** A servlet that tests use of various servlet API calls that can be affected
 by the web container integration layer.
 
 @author  Scott.Stark@jboss.org
 @version $Revision: 1.2 $
 */
public class APIServlet extends HttpServlet
{
   protected void processRequest(HttpServletRequest request, HttpServletResponse response)
      throws ServletException, IOException
   {
      response.setContentType("text/html");
      PrintWriter out = response.getWriter();
      out.println("<html>");
      out.println("<head><title>APIServlet</title></head><body><pre>");
      String realPath = testGetRealPath();
      out.println("testGetRealPath ok, realPath="+realPath+"\n");
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

   private String testGetRealPath()
      throws ServletException
   {
      String realPath = getServletContext().getRealPath("/");
      if( realPath == null )
         throw new ServletException("getServletContext().getRealPath(/) returned null");
      return realPath;
   }
}

