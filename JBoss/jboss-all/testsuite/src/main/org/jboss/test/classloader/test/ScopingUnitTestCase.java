/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.test.classloader.test;

import java.net.URL;
import java.io.InputStream;
import java.io.IOException;
import javax.management.ObjectName;
import javax.naming.InitialContext;

import org.jboss.test.JBossTestCase;
import org.jboss.test.classloader.scoping.override.ejb.log4j113.StatelessSession;
import org.jboss.test.classloader.scoping.override.ejb.log4j113.StatelessSessionHome;
import org.jboss.system.ServiceMBean;

/** Unit tests for class and resource scoping
 *
 * @author Scott.Stark@jboss.org
 * @version $Revision: 1.1.2.3 $
 */
public class ScopingUnitTestCase extends JBossTestCase
{
   public ScopingUnitTestCase(String name)
   {
      super(name);
   }

   /** Test the scoping of singleton classes in two independent service
    * deployments
    */
   public void testSingletons() throws Exception
   {
      getLog().debug("+++ testSingletons");
      try
      {
         deploy("singleton1.sar");
         ObjectName testObjectName = new ObjectName("jboss.test:service=TestService,version=V1");
         boolean isRegistered = getServer().isRegistered(testObjectName);
         assertTrue("jboss.test:loader=singleton.sar,version=V1 isRegistered", isRegistered);
         Integer state = (Integer) getServer().getAttribute(testObjectName, "State");
         assertTrue("state.intValue() == ServiceMBean.STARTED",
               state.intValue() == ServiceMBean.STARTED);
         Object[] args = {"V1"};
         String[] sig = {"java.lang.String"};
         Boolean matches = (Boolean) getServer().invoke(testObjectName, "checkVersion", args, sig);
         assertTrue("checkVersion(V1) is true", matches.booleanValue());
      }
      catch(Exception e)
      {
         getLog().info("Failed to validate singleton1.sar", e);
         undeploy("singleton1.sar");
         throw e;
      }

      try
      {
         deploy("singleton2.sar");
         ObjectName testObjectName = new ObjectName("jboss.test:service=TestService,version=V2");
         boolean isRegistered = getServer().isRegistered(testObjectName);
         assertTrue("jboss.test:loader=singleton.sar,version=V2 isRegistered", isRegistered);
         Integer state = (Integer) getServer().getAttribute(testObjectName, "State");
         assertTrue("state.intValue() == ServiceMBean.STARTED",
               state.intValue() == ServiceMBean.STARTED);
         Object[] args = {"V2"};
         String[] sig = {"java.lang.String"};
         Boolean matches = (Boolean) getServer().invoke(testObjectName, "checkVersion", args, sig);
         assertTrue("checkVersion(V2) is true", matches.booleanValue());
      }
      catch(Exception e)
      {
         getLog().info("Failed to validate singleton2.sar", e);
         undeploy("singleton2.sar");
         throw e;
      }

      undeploy("singleton1.sar");
      undeploy("singleton2.sar");
   }

   /** Test the ability to override the server classes with war local versions
    */
   public void testWarOverrides() throws Exception
   {
      getLog().debug("+++ testWarOverrides");
      try
      {
         deploy("log4j113.war");
         URL log4jServletURL = new URL("http://localhost:8080/log4j113/Log4jServlet/");
         InputStream reply = (InputStream) log4jServletURL.getContent();
         getLog().debug("Accessed http://localhost:8080/log4j113/Log4jServlet/");
         logReply(reply);

         URL encServletURL = new URL("http://localhost:8080/log4j113/ENCServlet/");
         reply = (InputStream) encServletURL.getContent();
         getLog().debug("Accessed http://localhost:8080/log4j113/ENCServlet/");
         logReply(reply);
      }
      catch(Exception e)
      {
         getLog().info("Failed to access Log4jServlet in log4j113.war", e);
         throw e;
      }
      finally
      {
         undeploy("log4j113.war");
      }
   }

   /** Test the ability to override the server classes with ejb local versions
    */
   public void testEjbOverrides() throws Exception
   {
      getLog().debug("+++ testEjbOverrides");
      try
      {
         deploy("log4j113-ejb.jar");
         InitialContext ctx = new InitialContext();
         StatelessSessionHome home = (StatelessSessionHome) ctx.lookup("Log4j113StatelessBean");
         StatelessSession bean = home.create();
         Throwable error = bean.checkVersion();
         getLog().debug("StatelessSession.checkVersion returned:", error);
         assertTrue("checkVersion returned null", error == null);
      }
      catch(Exception e)
      {
         getLog().info("Failed to access Log4j113StatelessBean in log4j113-ejb.jar", e);
         throw e;
      }
      finally
      {
         undeploy("log4j113-ejb.jar");
      }
   }

   private void logReply(InputStream reply) throws IOException
   {
      getLog().debug("Begin reply");
      byte[] tmp = new byte[256];
      while( reply.read(tmp) > 0 )
         getLog().debug(new String(tmp));
      reply.close();
      getLog().debug("End reply");
   }
}
