/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.test.security.test;

import java.rmi.RemoteException;
import javax.naming.InitialContext;
import javax.rmi.PortableRemoteObject;
import javax.security.auth.login.Configuration;
import javax.security.auth.login.LoginContext;

import org.jboss.security.auth.login.XMLLoginConfigImpl;
import org.jboss.test.JBossTestCase;
import org.jboss.test.JBossTestSetup;
import org.jboss.test.security.interfaces.IOSession;
import org.jboss.test.security.interfaces.IOSessionHome;
import org.jboss.test.security.interfaces.ReadAccessException;
import org.jboss.test.util.AppCallbackHandler;

import junit.extensions.TestSetup;
import junit.framework.Test;
import junit.framework.TestSuite;


/** Tests of the EJB security proxy.
 
 @author Scott.Stark@jboss.org
 @version $Revision: 1.4.2.2 $
 */
public class SecurityProxyUnitTestCase
   extends JBossTestCase
{
   LoginContext lc;
   boolean loggedIn;

   public SecurityProxyUnitTestCase(String name)
   {
      super(name);
   }

   /** Test that the echo method is accessible by an Echo
    role. Since the noop() method of the StatelessSession
    bean was not assigned any permissions it should not be
    accessible by any user.
    */
   public void testMethodAccess() throws Exception
   {
      log.debug("+++ testMethodAccess");
      login();
      Object obj = getInitialContext().lookup("security-proxy/ProxiedStatelessBean");
      obj = PortableRemoteObject.narrow(obj, IOSessionHome.class);
      IOSessionHome home = (IOSessionHome) obj;
      log.debug("Found IOSessionHome");
      IOSession bean = home.create();
      log.debug("Created IOSession");
      
      try
      {
         // This should not be allowed
         bean.read("/restricted/pgp.keys");
         fail("Was able to call read(/restricted/pgp.keys)");
      }
      catch(RemoteException e)
      {
         log.debug("IOSession.read failed as expected");
      }
      bean.read("/public/pgp.keys");

      try
      {
         // This should not be allowed
         bean.retryableRead("/restricted/pgp.keys");
         fail("Was able to call read(/restricted/pgp.keys)");
      }
      catch(ReadAccessException e)
      {
         log.debug("IOSession.read failed as expected with ReadAccessException");
         bean.read("/public/pgp.keys");
      }

      try
      {
         // This should not be allowed
         bean.write("/restricted/pgp.keys");
         fail("Was able to call write(/restricted/pgp.keys)");
      }
      catch(RemoteException e)
      {
         log.debug("IOSession.write failed as expected");
      }
      bean.write("/public/pgp.keys");

      bean.remove();
   }

   /** Login as user scott using the conf.name login config or
    'spec-test' if conf.name is not defined.
    */
   private void login() throws Exception
   {
      login("jduke", "theduke".toCharArray());
   }
   private void login(String username, char[] password) throws Exception
   {
      if( loggedIn )
         return;

      lc = null;
      String confName = System.getProperty("conf.name", "spec-test");
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
      suite.addTest(new TestSuite(SecurityProxyUnitTestCase.class));

      // Create an initializer for the test suite
      TestSetup wrapper = new JBossTestSetup(suite)
      {
         protected void setUp() throws Exception
         {
            super.setUp();
            Configuration.setConfiguration(new XMLLoginConfigImpl());
            redeploy("security-proxy.jar");
            flushAuthCache();
         }
         protected void tearDown() throws Exception
         {
            undeploy("security-proxy.jar");
            super.tearDown();
         
         }
      };
      return wrapper;
   }

}
