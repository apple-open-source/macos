package org.jboss.test.tomcat.servlet;

import java.io.*;
import java.text.*;
import java.util.*;
import javax.servlet.*;
import javax.servlet.http.*;

import javax.naming.Context;
import javax.naming.InitialContext;
import javax.naming.NamingException;

import org.jboss.test.tomcat.ejb.interfaces.StatelessSessionHome;
import org.jboss.test.tomcat.ejb.interfaces.StatelessSession;

/** A servlet that accesses the

@author Scott.Stark@jboss.org
@version $Revision: 1.2 $
*/
public class HelloEJB extends HttpServlet
{
   
   public void doGet(HttpServletRequest request, HttpServletResponse response)
      throws IOException, ServletException
   {
      response.setContentType("text/html");
      PrintWriter out = response.getWriter();

      out.println("<html>");
      out.println("<head>");      
      out.println("<title>HelloEJB</title>");
      out.println("</head>");
      out.println("<body>");
      out.println("<h1>Servlet calling EJB</h1>");
      try
      {
         boolean speedTest = "speed".equals(request.getQueryString());
         boolean trace = "trace".equals(request.getQueryString());
         testBean("NonOptimized", speedTest, trace, out);
         testBean("Optimized", speedTest, trace, out);
         if( trace == true )
         {
            out.println("<pre><h2>Servlet Trace Info:</h2>\n");
            out.println(PrintClassLoaders.getClassLoaders());
            out.println("<h2>JNDI ENC:</h2>\n");
            Util.dumpENC(out);
            out.println("</pre>");
         }
         out.println("</body>");
         out.println("</html>");
      }
      catch (Exception e)
      {
         out.println("Context not found: exception message:<pre>");
         e.printStackTrace(out);
         out.println("</pre>");
         out.println("</body>");
         out.println("</html>");
      }
   }
   
   static void testBean(String jndiName, boolean speedTest, boolean trace, PrintWriter out)
      throws NamingException
   {
      Context ctx = new InitialContext();
      try
      {
         jndiName = "java:comp/env/ejb/" + jndiName;
         StatelessSessionHome home = (StatelessSessionHome)ctx.lookup(jndiName);
         StatelessSession bean = home.create();

         out.print("<h2>Accessing EJB: " + jndiName + ", method=<tt>getMessage()</tt></h2>");
         if( trace == false )
            out.print(bean.getMessage());
         else
         {
            out.println("<pre>\n");
            out.print(bean.getMessageAndTrace());
            out.println("</pre>\n");
         }
         
         int iter = 1000;
         if (speedTest)
         {
            
            out.println("<h3>Speed Test ("+ iter + " iterations)</h3>");
            
            long start = System.currentTimeMillis();
            long start2 = start;
            
            for (int i = 0 ; i < iter; i++)
            {
               bean.getMessage();
               
               if (i % 100 == 0 && i != 0)
               {
                  long end = System.currentTimeMillis();
                  out.println("Time/call(ms):"+((end-start2)/100.0));
                  start2 = end;
               }
            }
            
            long end = System.currentTimeMillis();
            out.println("<br>Avg. time/call(ms):"+((end-start)/(float)iter)+"</p>");
         }
      }
      catch (Exception e)
      {
         
         out.println("<br>Call failed... Exception:");
         out.println("<pre>");
         e.printStackTrace(out);
         out.println("</pre>");   
      }
   }
}
