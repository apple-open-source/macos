package org.jboss.test.web.servlets;

import java.io.IOException;
import java.io.PrintWriter;
import javax.naming.Context;
import javax.naming.InitialContext;
import javax.naming.NamingException;
import javax.rmi.PortableRemoteObject;
import javax.servlet.ServletConfig;
import javax.servlet.ServletException;
import javax.servlet.http.HttpServlet;
import javax.servlet.http.HttpServletRequest;
import javax.servlet.http.HttpServletResponse;
import javax.sql.DataSource;

import org.jboss.test.web.interfaces.ReferenceTest;
import org.jboss.test.web.interfaces.StatelessSession;
import org.jboss.test.web.interfaces.StatelessSessionHome;
import org.jboss.test.web.interfaces.StatelessSessionLocal;
import org.jboss.test.web.interfaces.StatelessSessionLocalHome;
import org.jboss.test.web.interfaces.ReturnData;
import org.jboss.test.web.util.Util;

/** A servlet that accesses an EJB and tests whether the call argument
 is serialized.

 @author  Scott.Stark@jboss.org
 @version $Revision: 1.3.4.2 $
 */
public class EJBServlet extends HttpServlet
{
   protected void processRequest(HttpServletRequest request, HttpServletResponse response)
         throws ServletException, IOException
   {
      try
      {
         InitialContext ctx = new InitialContext();
         Context enc = (Context) ctx.lookup("java:comp/env");
         StatelessSessionHome home = (StatelessSessionHome) enc.lookup("ejb/OptimizedEJB");
         StatelessSession bean = home.create();
         bean.noop(new ReferenceTest(), true);

         Object homeRef = enc.lookup("ejb/OptimizedEJB");
         home = (StatelessSessionHome) PortableRemoteObject.narrow(homeRef, StatelessSessionHome.class);
         bean = home.create();
         bean.noop(new ReferenceTest(), true);
         ReturnData data = bean.getData();

         StatelessSessionLocalHome localHome = (StatelessSessionLocalHome) enc.lookup("ejb/local/OptimizedEJB");
         StatelessSessionLocal localBean = localHome.create();
         localBean.noop(new ReferenceTest(), true);
      }
      catch (Exception e)
      {
         throw new ServletException("Failed to call OptimizedEJB through remote and local interfaces", e);
      }
      response.setContentType("text/html");
      PrintWriter out = response.getWriter();
      out.println("<html>");
      out.println("<head><title>EJBServlet</title></head>");
      out.println("<body>Tests passed<br>Time:" + Util.getTime() + "</body>");
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
