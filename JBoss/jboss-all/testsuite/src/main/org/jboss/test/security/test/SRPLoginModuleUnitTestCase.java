/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.test.security.test;

import java.security.Principal;
import javax.security.auth.login.LoginContext;
import javax.security.auth.login.LoginException;
import javax.management.ObjectName;

import junit.extensions.TestSetup;
import junit.framework.Test;
import junit.framework.TestSuite;

import org.apache.log4j.Category;

import org.jboss.test.util.AppCallbackHandler;
import org.jboss.test.JBossTestCase;
import org.jboss.test.JBossTestSetup;
import org.jboss.jmx.adaptor.rmi.RMIAdaptor;
import org.jboss.security.SecurityAssociation;
import org.jboss.security.Util;

/** Test of the secure remote password(SRP) service and its usage via JAAS
login modules.
 
 @author Scott.Stark@jboss.org
 @version $Revision: 1.1.4.8 $
 */
public class SRPLoginModuleUnitTestCase extends JBossTestCase
{
   static final String JAR = "security-srp.sar";
   static String username = "scott";
   static char[] password = "echoman".toCharArray();

   LoginContext lc;
   boolean loggedIn;

   public SRPLoginModuleUnitTestCase(String name)
   {
      super(name);
   }

   /** Test a login against the SRP service using the SRPLoginModule
    */
   public void testSRPLogin() throws Exception
   {
      log.debug("+++ testSRPLogin");
      login("srp-test", username, password, null);
      logout();
   }

   /** Test a login against the SRP service using the SRPLoginModule
    */
   public void testSRPLoginHTTP() throws Exception
   {
      log.debug("+++ testSRPLoginHTTP");
      login("srp-test-http", username, password, null);
      logout();
   }

   /** Test a login against the SRP service using the SRPLoginModule
    */
   public void testSRPLoginHTTPHA() throws Exception
   {
      log.debug("+++ testSRPLoginHTTPHA");
      login("srp-test-http-ha", username, password, null);
      logout();
   }

   /** Test a login against the SRP service using the SRPLoginModule and
    specify the random number used in the client A public key.
    */
   public void testSRPLoginWithExternalA() throws Exception
   {
      log.debug("+++ testSRPLoginWithExternalA");
      byte[] abytes = "abcdefgh".getBytes();
      login("srp-test-ex", username, password, abytes);
      logout();
   }

   /** Test a login against the SRP service using the SRPLoginModule and
    provide an auxillarly challenge to be validated by the server.
    */
   public void testSRPLoginWithAuxChallenge() throws Exception
   {
      log.debug("+++ testSRPLoginWithAuxChallenge");
      // Check for javax/crypto/SealedObject
      try
      {
         Class.forName("javax.crypto.SealedObject");
         log.debug("Found javax/crypto/SealedObject");
         login("srp-test-aux", username, password, null, "token-123");
      }
      catch(ClassNotFoundException e)
      {
         log.debug("Failed to find javax/crypto/SealedObject, skipping test");
         return;
      }
      catch(NoClassDefFoundError e)
      {
         log.debug("Failed to find javax/crypto/SealedObject, skipping test");
         return;
      }
      catch(LoginException e)
      {
         boolean hasUnlimitedCrypto = Util.hasUnlimitedCrypto();
         log.warn("login failure, hasUnlimitedCrypto="+hasUnlimitedCrypto, e);
         // See if 
         if( hasUnlimitedCrypto == true )
            fail("Unable to complete login: "+e.getMessage());
         log.info("Skipping test due to missing UnlimitedCrypto");
         return;
      }
      catch(Exception e)
      {
         log.error("Non CNFE exception during testSRPLoginWithAuxChallenge", e);
         fail("Non CNFE exception during testSRPLoginWithAuxChallenge");
      }

      logout();
   }

   /** Test a login against the SRP service using the SRPLoginModule with
    multiple sessions for the same user. This creates two threads 
    */
   public void testSRPLoginWithMultipleSessions() throws Exception
   {
      log.debug("+++ testSRPLoginWithMultipleSessions");
      AppCallbackHandler handler = new AppCallbackHandler(username, password, null);
      RMIAdaptor server = super.getServer();

      // Session #1
      SessionThread t1 = new SessionThread(log, handler, server);
      t1.start();

      // Session #2
      SessionThread t2 = new SessionThread(log, handler, server);
      t2.start();

      t1.join();
      t2.join();
      assertTrue("Session1.error == null", t1.getError() == null);
      assertTrue("Session2.error == null", t2.getError() == null);
   }
   static class SessionThread extends Thread
   {
      private Throwable error;
      private Category log;
      private AppCallbackHandler handler;
      private RMIAdaptor server;

      SessionThread(Category log, AppCallbackHandler handler, RMIAdaptor server)
      {
         super("SRPSession");
         this.log = log;
         this.handler = handler;
         this.server = server;
      }

      public Throwable getError()
      {
         return error;
      }
      public void run()
      {
         try
         {
            log.debug("Creating LoginContext(srp-test-multi): "+getName());
            LoginContext lc = new LoginContext("srp-test-multi", handler);
            lc.login();
            log.debug("Created LoginContext, subject="+lc.getSubject());
            // Invoke the 
            ObjectName service = new ObjectName("jboss.security.tests:service=SRPCacheTest");
            Principal user = SecurityAssociation.getPrincipal();
            byte[] key = (byte[]) SecurityAssociation.getCredential();
            Object[] args = {user, key};
            String[] sig = {Principal.class.getName(), key.getClass().getName()};
            for(int n = 0; n < 5; n ++)
               server.invoke(service, "testSession", args, sig);
            lc.logout();
         }
         catch(Throwable t)
         {
            error = t;
            log.error("Session failed", t);
         }
      }
   }

   /** Login using the given confName login configuration with the provided
    username and password credential.
    */
   private void login(String confName, String username, char[] password,
      byte[] data) throws Exception
   {
      this.login(confName, username, password, data, null);
   }
   private void login(String confName, String username, char[] password,
      byte[] data, String text) throws Exception
   {
      if( loggedIn )
         return;

      lc = null;
      AppCallbackHandler handler = new AppCallbackHandler(username, password, data, text);
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
      suite.addTest(new TestSuite(SRPLoginModuleUnitTestCase.class));

      // Create an initializer for the test suite
      TestSetup wrapper = new JBossTestSetup(suite)
      {
         protected void setUp() throws Exception
         {
            super.setUp();
            super.redeploy(JAR);
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
