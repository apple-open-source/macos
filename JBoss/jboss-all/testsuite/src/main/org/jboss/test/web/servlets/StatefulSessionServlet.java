package org.jboss.test.web.servlets;

import java.io.IOException;
import java.io.PrintWriter;
import javax.ejb.Handle;
import javax.naming.Context;
import javax.naming.InitialContext;
import javax.servlet.ServletException;
import javax.servlet.http.HttpServlet;
import javax.servlet.http.HttpServletRequest;
import javax.servlet.http.HttpServletResponse;
import javax.servlet.http.HttpSession;
import javax.servlet.http.HttpSessionActivationListener;
import javax.servlet.http.HttpSessionEvent;

import org.apache.log4j.Logger;
import org.jboss.test.web.interfaces.StatelessSession;
import org.jboss.test.web.interfaces.StatelessSessionHome;

/** A servlet that accesses a stateful session EJB and stores a handle in the session context
 * to test retrieval of the session from the handle.

 @author  Scott.Stark@jboss.org
 @version $Revision: 1.1.4.1 $
 */
public class StatefulSessionServlet extends HttpServlet
{
   static private Logger log = Logger.getLogger(StatefulSessionServlet.class);

   static class SessionHandle implements HttpSessionActivationListener
   {
      Handle h;
      SessionHandle(Handle h)
      {
         this.h = h;
      }
      public void sessionWillPassivate(HttpSessionEvent event)
      {
         log.info("sessionWillPassivate, event="+event);
      }

      public void sessionDidActivate(HttpSessionEvent event)
      {
         log.info("sessionDidActivate, event="+event);
      }
   }

   protected void processRequest(HttpServletRequest request, HttpServletResponse response)
         throws ServletException, IOException
   {
      HttpSession session = request.getSession();
      try
      {
         StatelessSession localBean = null;
         // See if there is an existing session
         if( session.isNew() )
         {
            log.info("Creating a new stateful session");
            InitialContext ctx = new InitialContext();
            Context enc = (Context) ctx.lookup("java:comp/env");
            StatelessSessionHome localHome = (StatelessSessionHome) enc.lookup("ejb/StatefulEJB");
            localBean = localHome.create();
            Handle h = localBean.getHandle();
            SessionHandle wrapper = new SessionHandle(h);
            session.setAttribute("StatefulEJB", wrapper);
         }
         else
         {
            log.info("Getting existing stateful session");
            SessionHandle wrapper = (SessionHandle) session.getAttribute("StatefulEJB");
            localBean = (StatelessSession) wrapper.h.getEJBObject();
         }
         localBean.echo("Hello");
      }
      catch (Exception e)
      {
         throw new ServletException("Failed to call StatefulEJB", e);
      }
      response.setContentType("text/html");
      PrintWriter out = response.getWriter();
      out.println("<html>");
      out.println("<head><title>StatefulSessionServlet</title></head>");
      out.println("<body>");
      out.println("<h1>Session Information</h1>");
      out.println("SessionID: "+session.getId());
      out.println("IsNew: "+session.isNew());
      out.println("CreationTime: "+session.getCreationTime());
      out.println("LastAccessedTime: "+session.getLastAccessedTime());
      out.println("Now: "+System.currentTimeMillis());
      out.println("MaxInactiveInterval: "+session.getMaxInactiveInterval());
      out.println("</body>");
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
