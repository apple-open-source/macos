/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.test.client.test;

import java.util.Properties;
import java.net.URL;
import javax.naming.InitialContext;
import javax.naming.Context;
import javax.naming.NamingException;
import javax.jms.Queue;

import org.jboss.test.cts.interfaces.StatelessSession;
import org.jboss.test.cts.interfaces.StatelessSessionHome;
import org.jboss.test.JBossTestCase;

import junit.framework.Test;

/** Tests of accessing a j2ee application client deployment
 * 
 *  @author Scott.Stark@jboss.org
 *  @version $Revision: 1.1.2.2 $
 */
public class AppClientUnitTestCase extends JBossTestCase
{
   public AppClientUnitTestCase(String name)
   {
      super(name);
   }

   /** Test that the client java:comp/env context contains what is expected
    * @throws Exception
    */ 
   public void testENC() throws Exception
   {
      Context enc = getENC();
      getLog().debug("ENC: "+enc);

      String str0 = (String) enc.lookup("String0");
      assertTrue("String0 == String0Value", str0.equals("String0Value"));

      Float flt0 = (Float) enc.lookup("Float0");
      assertTrue("Float0 == 3.14", flt0.equals(new Float("3.14")));

      Long long0 = (Long) enc.lookup("Long0");
      assertTrue("Long0 == 123456789", long0.equals(new Long(123456789)));

      StatelessSessionHome home = (StatelessSessionHome) enc.lookup("ejb/StatelessSessionBean");
      assertTrue("ejb/StatelessSessionBean isa StatelessSessionHome", home != null);

      URL jbossHome = (URL) enc.lookup("url/JBossHome");
      assertTrue("url/JBossHome == http://www.jboss.org",
         jbossHome.toString().equals("http://www.jboss.org"));

      Queue testQueue = (Queue) enc.lookup("jms/aQueue");
      assertTrue("jms/aQueue isa Queue", testQueue != null);
   }

   /** Test access to EJBs located through the java:comp/env context
    * @throws Exception
    */ 
   public void testEjbs() throws Exception
   {
      Context enc = getENC();      
      StatelessSessionHome home = (StatelessSessionHome) enc.lookup("ejb/StatelessSessionBean");
      StatelessSession session = home.create();
      session.method1("testEjbs");
      session.remove();
   }

   /** Build the InitialContext factory 
    * @return
    * @throws NamingException
    */ 
   private Context getENC() throws NamingException
   {
      Properties env = new Properties();
      env.setProperty(Context.INITIAL_CONTEXT_FACTORY,
         "org.jnp.interfaces.NamingContextFactory");
      env.setProperty(Context.URL_PKG_PREFIXES, "org.jboss.naming.client");
      env.setProperty(Context.PROVIDER_URL, "jnp://localhost:1099");
      env.setProperty("j2ee.clientName", "test-client");
      InitialContext ctx = new InitialContext(env);
      Context enc = (Context) ctx.lookup("java:comp/env");
      return enc;
   }

   public static Test suite() throws Exception
   {
      return getDeploySetup(AppClientUnitTestCase.class, "app-client.ear");
   }
}
