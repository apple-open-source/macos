/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.test.security.test;

import java.lang.reflect.UndeclaredThrowableException;
import java.rmi.RemoteException;
import javax.rmi.PortableRemoteObject;
import javax.security.auth.login.LoginContext;

import junit.extensions.TestSetup;
import junit.framework.Test;
import junit.framework.TestSuite;

import org.jboss.test.util.AppCallbackHandler;
import org.jboss.test.JBossTestCase;
import org.jboss.test.JBossTestSetup;
import org.jboss.test.security.interfaces.StatelessSession;
import org.jboss.test.security.interfaces.StatelessSessionHome;

/** Test of the secure remote password(SRP) session key to perform crypto
operations.
 
 
 @author Scott.Stark@jboss.org
 @version $Revision: 1.1.4.5 $
 */
public class SRPUnitTestCase extends JBossTestCase
{
   static final String JAR = "security-srp.jar";
   static String username = "scott";
   static char[] password = "echoman".toCharArray();

   LoginContext lc;
   boolean loggedIn;

   public SRPUnitTestCase(String name)
   {
      super(name);
   }

   /** Test that the echo method is secured by the SRPCacheLogin module
    */
   public void testEchoArgs() throws Exception
   {
      log.debug("+++ testEchoArgs");
      login("srp-test", username, password);
      Object obj = getInitialContext().lookup("srp.StatelessSession");
      obj = PortableRemoteObject.narrow(obj, StatelessSessionHome.class);
      StatelessSessionHome home = (StatelessSessionHome) obj;
      log.debug("Found StatelessSessionHome");
      StatelessSession bean = home.create();
      log.debug("Created spec.StatelessSession");
      try
      {
         log.debug("Bean.echo('Hello') -> "+bean.echo("Hello"));
      }
      catch(Exception e)
      {
         Throwable t = e;
         if( e instanceof UndeclaredThrowableException )
         {
            UndeclaredThrowableException ex = (UndeclaredThrowableException) e;
            t = ex.getUndeclaredThrowable();
         }
         else if( e instanceof RemoteException )
         {
            RemoteException ex = (RemoteException) e;
            t = ex.detail;
         }

         log.error("echo failed", t);
         boolean failure = true;
         if( t instanceof SecurityException )
         {
            String msg = t.getMessage();
            if( msg.startsWith("Unsupported keysize") )
            {
               /* The size of the srp session key is bigger than the JCE version
               in use supports. Most likely the unlimited strength policy is
               not installed so don't fail the test.
               */
               failure = false;
               log.info("Not failing test due to key size issue");
            }
         }

         if( failure )
            fail("Call to echo failed: "+t.getMessage());
      }

      logout();
   }

   /** Login using the given confName login configuration with the provided
    username and password credential.
    */
   private void login(String confName, String username, char[] password)
      throws Exception
   {
      if( loggedIn )
         return;

      lc = null;
      AppCallbackHandler handler = new AppCallbackHandler(username, password);
      log.debug("Creating LoginContext("+confName+")");
      lc = new LoginContext(confName, handler);
      lc.login();
      log.debug("Created LoginContext, subject="+lc.getSubject());
      loggedIn = true;
   }
   private void logout() throws Exception
   {
      if( loggedIn )
      {
         loggedIn = false;
         lc.logout();
      }
   }

   /**
    * Setup the test suite.
    */
   public static Test suite() throws Exception
   {
      TestSuite suite = new TestSuite();
      suite.addTest(new TestSuite(SRPUnitTestCase.class));

      // Create an initializer for the test suite
      TestSetup wrapper = new JBossTestSetup(suite)
      {
         protected void setUp() throws Exception
         {
            super.setUp();
            deploy(JAR);
            // Establish the JAAS login config
            String authConfPath = super.getResourceURL("security-srp/auth.conf");
            System.setProperty("java.security.auth.login.config", authConfPath);
         }
         protected void tearDown() throws Exception
         {
            undeploy(JAR);
            super.tearDown();
         }
      };
      return wrapper;
   }

}
