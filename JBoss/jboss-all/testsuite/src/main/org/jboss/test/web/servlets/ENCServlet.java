package org.jboss.test.web.servlets;

import java.io.IOException;
import java.io.PrintWriter;
import java.net.URL;
import java.util.Hashtable;
import java.util.Iterator;
import javax.jms.QueueConnectionFactory;
import javax.mail.Session;
import javax.naming.Context;
import javax.naming.InitialContext;
import javax.naming.NamingException;
import javax.servlet.ServletException;
import javax.servlet.http.HttpServlet;
import javax.servlet.http.HttpServletRequest;
import javax.servlet.http.HttpServletResponse;
import javax.sql.DataSource;

import org.jboss.test.cts.interfaces.CtsBmpHome;
import org.jboss.test.web.interfaces.StatelessSessionHome;
import org.jboss.test.web.interfaces.StatelessSessionLocalHome;

import org.jboss.test.web.util.Util;

/** Tests of the server ENC naming context
 *
 * @author Scott.Stark@jboss.org
 * @version $Revision: 1.8.4.6 $
 */
public class ENCServlet extends HttpServlet
{
   org.apache.log4j.Category log = org.apache.log4j.Category.getInstance(getClass());

   protected void processRequest(HttpServletRequest request, HttpServletResponse response)
         throws ServletException, IOException
   {
      testENC();
      response.setContentType("text/html");
      PrintWriter out = response.getWriter();
      out.println("<html>");
      out.println("<head><title>ENCServlet</title></head>");
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

   private void testENC() throws ServletException
   {
      try
      {
         // Obtain the enterprise bean’s environment naming context.
         Context initCtx = new InitialContext();
         Hashtable env = initCtx.getEnvironment();
         Iterator keys = env.keySet().iterator();
         log.info("InitialContext.env:");
         while( keys.hasNext() )
         {
            Object key = keys.next();
            log.info("Key: "+key+", value: "+env.get(key));
         }
         Context myEnv = (Context) initCtx.lookup("java:comp/env");
         testEjbRefs(initCtx, myEnv);
         testJdbcDataSource(initCtx, myEnv);
         testMail(initCtx, myEnv);
         testJMS(initCtx, myEnv);
         testURL(initCtx, myEnv);
         testEnvEntries(initCtx, myEnv);
      }
      catch (NamingException e)
      {
         log.debug("Lookup failed", e);
         throw new ServletException("Lookup failed, ENC tests failed", e);
      }
      catch (RuntimeException e)
      {
         log.debug("Runtime error", e);
         throw new ServletException("Runtime error, ENC tests failed", e);
      }
   }

   private void testEnvEntries(Context initCtx, Context myEnv) throws NamingException
   {
      // Basic env values
      Integer i = (Integer) myEnv.lookup("Ints/i0");
      log.debug("Ints/i0 = " + i);
      i = (Integer) initCtx.lookup("java:comp/env/Ints/i1");
      log.debug("Ints/i1 = " + i);
      Float f = (Float) myEnv.lookup("Floats/f0");
      log.debug("Floats/f0 = " + f);
      f = (Float) initCtx.lookup("java:comp/env/Floats/f1");
      log.debug("Floats/f1 = " + f);
      String s = (String) myEnv.lookup("Strings/s0");
      log.debug("Strings/s0 = " + s);
      s = (String) initCtx.lookup("java:comp/env/Strings/s1");
      log.debug("Strings/s1 = " + s);
      s = (String) initCtx.lookup("java:comp/env/ejb/catalog/CatalogDAOClass");
      log.debug("ejb/catalog/CatalogDAOClass = " + s);
   }

   private void testEjbRefs(Context initCtx, Context myEnv) throws NamingException
   {
      // EJB References
      Object ejb = myEnv.lookup("ejb/bean0");
      if ((ejb instanceof StatelessSessionHome) == false)
         throw new NamingException("ejb/bean0 is not a StatelessSessionHome");
      log.debug("ejb/bean0 = " + ejb);
      ejb = initCtx.lookup("java:comp/env/ejb/bean1");
      if ((ejb instanceof StatelessSessionHome) == false)
         throw new NamingException("ejb/bean1 is not a StatelessSessionHome");
      log.debug("ejb/bean1 = " + ejb);
      ejb = initCtx.lookup("java:comp/env/ejb/bean2");
      if ((ejb instanceof StatelessSessionHome) == false)
         throw new NamingException("ejb/bean2 is not a StatelessSessionHome");
      log.debug("ejb/bean2 = " + ejb);
      //do lookup on bean specified without ejb-link
      ejb = initCtx.lookup("java:comp/env/ejb/bean3");
      if ((ejb instanceof StatelessSessionHome) == false)
         throw new NamingException("ejb/bean3 is not a StatelessSessionHome");
      log.debug("ejb/bean3 = " + ejb);


      ejb = initCtx.lookup("java:comp/env/ejb/UnsecuredEJB");
      if ((ejb instanceof StatelessSessionHome) == false)
         throw new NamingException("ejb/UnsecuredEJB is not a StatelessSessionHome");
      log.debug("ejb/UnsecuredEJB = " + ejb);
      ejb = initCtx.lookup("java:comp/env/ejb/SecuredEJB");
      if ((ejb instanceof StatelessSessionHome) == false)
         throw new NamingException("ejb/SecuredEJB is not a StatelessSessionHome");
      log.debug("ejb/SecuredEJB = " + ejb);
      ejb = initCtx.lookup("java:comp/env/ejb/CtsBmp");
      if ((ejb instanceof CtsBmpHome) == false)
         throw new NamingException("ejb/CtsBmp is not a CtsBmpHome");
      log.debug("ejb/CtsBmp = " + ejb);
      ejb = initCtx.lookup("java:comp/env/ejb/RelativeBean");
      if ((ejb instanceof StatelessSessionHome) == false )
         throw new NamingException("ejb/RelativeBean is not a StatelessSessionHome");
      log.debug("ejb/RelativeBean = " + ejb);

      // EJB Local References
      ejb = initCtx.lookup("java:comp/env/ejb/local/bean0");
      if ((ejb instanceof StatelessSessionLocalHome) == false)
         throw new NamingException("ejb/local/bean0 is not a StatelessSessionLocalHome");
      log.debug("ejb/local/bean0 = " + ejb);
      ejb = initCtx.lookup("java:comp/env/ejb/local/bean1");
      if ((ejb instanceof StatelessSessionLocalHome) == false)
         throw new NamingException("ejb/local/bean1 is not a StatelessSessionLocalHome");
      log.debug("ejb/local/bean1 = " + ejb);

      //lookup of local-ejb-ref bean specified without ejb-link
      ejb = initCtx.lookup("java:comp/env/ejb/local/bean3");
      if ((ejb instanceof StatelessSessionLocalHome) == false)
         throw new NamingException("ejb/local/bean3 is not a StatelessSessionLocalHome");
      log.debug("ejb/local/bean3 = " + ejb);

      ejb = initCtx.lookup("java:comp/env/ejb/local/OptimizedEJB");
      if ((ejb instanceof StatelessSessionLocalHome) == false)
         throw new NamingException("ejb/local/OptimizedEJB is not a StatelessSessionLocalHome");
      log.debug("ejb/local/OptimizedEJB = " + ejb);
      ejb = initCtx.lookup("java:comp/env/ejb/local/RelativeBean");
      if ((ejb instanceof StatelessSessionLocalHome) == false )
         throw new NamingException("ejb/local/RelativeBean is not a StatelessSessionLocalHome");
      log.debug("ejb/local/RelativeBean = " + ejb);
   }

   private void testJdbcDataSource(Context initCtx, Context myEnv) throws NamingException
   {
      // JDBC DataSource
      DataSource ds = (DataSource) myEnv.lookup("jdbc/DefaultDS");
      log.debug("jdbc/DefaultDS = " + ds);
   }

   private void testMail(Context initCtx, Context myEnv) throws NamingException
   {
      // JavaMail Session
      Session session = (Session) myEnv.lookup("mail/DefaultMail");
      log.debug("mail/DefaultMail = " + session);
   }

   private void testJMS(Context initCtx, Context myEnv) throws NamingException
   {
      // JavaMail Session
      QueueConnectionFactory qf = (QueueConnectionFactory) myEnv.lookup("jms/QueFactory");
      log.debug("jms/QueFactory = " + qf);
   }

   private void testURL(Context initCtx, Context myEnv) throws NamingException
   {
      // URLs
      URL home1 = (URL) myEnv.lookup("url/JBossHome");
      log.debug("url/JBossHome = " + home1);
      URL home2 = (URL) initCtx.lookup("java:comp/env/url/JBossHome");
      log.debug("url/JBossHome = " + home2);
      if( home1.equals(home2) == false )
         throw new NamingException("url/JBossHome != java:comp/env/url/JBossHome");
   }
}
/*
vim:ts=3:sw=3:et
*/
