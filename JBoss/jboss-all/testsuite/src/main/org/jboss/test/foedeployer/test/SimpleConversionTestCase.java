/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.test.foedeployer.test;

import java.io.IOException;
import java.net.InetAddress;
import java.rmi.RemoteException;
import java.util.Set;
import javax.ejb.CreateException;
import javax.ejb.Handle;
import javax.management.ObjectName;
import javax.naming.InitialContext;
import javax.naming.NamingException;
import javax.rmi.PortableRemoteObject;

import junit.extensions.TestSetup;
import junit.framework.Test;
import junit.framework.TestCase;
import junit.framework.TestSuite;

import org.jboss.test.JBossTestCase;
import org.jboss.test.JBossTestSetup;

import org.jboss.test.foedeployer.ejb.simple.SecretManager;
import org.jboss.test.foedeployer.ejb.simple.SecretManagerHome;

/**
 * Test of a simple WebLogic Application Conversion
 *
 * @author <a href="mailto:andreas@jboss.org">Andreas Schaefer</a>.
 * @author <a href="mailto:loubyansky@hotmail.com">Alex Loubyansky</a>
 * @version $Revision: 1.4.2.2 $
 */
public class SimpleConversionTestCase
   extends JBossTestCase
{
   // Constants -----------------------------------------------------
   public static final String FOE_DEPLOYER = "foe-deployer-3.2.sar";
   public static final String FOE_DEPLOYER_NAME = "jboss:service=FoeDeployer";
   public static final String CONVERTOR_DEPLOYER_QUERY_NAME = "jboss:service=Convertor,*";
   public static final String SIMPLE_APPLICATION = "foe-deployer-simple-test";
   public static final String SECRET_SESSION_JNDI_NAME = "ejb/SecretManager";

   // Static --------------------------------------------------------
   /**
    * Setup the test suite.
    */
   public static Test suite() throws Exception
   {
      TestSuite lSuite = new TestSuite();
      lSuite.addTest( new TestSuite( SimpleConversionTestCase.class ) );

      // Create an initializer for the test suite
      TestSetup lWrapper = new JBossTestSetup( lSuite )
      {
         protected void setUp() throws Exception
         {
            super.setUp();
         }
         protected void tearDown() throws Exception
         {
            super.tearDown();
         }
      };
      return lWrapper;
   }

   // Constructors --------------------------------------------------
   public SimpleConversionTestCase( String pName )
   {
      super( pName );
   }

   // Public --------------------------------------------------------
   /**
    * Test a simple conversion
    **/
   public void testSimpleConversion()
      throws Exception
   {
      try
      {
         log.debug("+++ testSimpleConversion");

         // First check if foe-deployer is deployed
         boolean lIsInitiallyDeployed = getServer().isRegistered( new ObjectName( FOE_DEPLOYER_NAME ) );
         if(!lIsInitiallyDeployed)
            deploy(FOE_DEPLOYER);

         boolean lIsDeployed = getServer().isRegistered(new ObjectName(FOE_DEPLOYER_NAME));
         assertTrue("Foe-Deployer is not deployed", lIsDeployed);

         // Count number of convertors (must be a list one)
         int lCount = getServer().queryNames(new ObjectName(CONVERTOR_DEPLOYER_QUERY_NAME), null).size();
         assertTrue("No Convertor found on web server", lCount > 0);

         // Deploy the simple application
         deploy(SIMPLE_APPLICATION + ".wlar");

         // Because the Foe-Deployer copies the converted JAR back to the original place
         // it has to be deployed from here again
         deploy(SIMPLE_APPLICATION + ".jar");

         // Access the Session Bean and invoke some methods on it
         SecretManager session = getSecretManagerEJB();
         String key = "xxx";
         String secret = "the sun is shining brightly";

         session.createSecret( key, secret );
         assertTrue( "the secret read is not equal to secret set", secret.equals(session.getSecret(key)) );
         session.removeSecret(key);

         // Undeploy converted application to clean up
         undeploy(SIMPLE_APPLICATION + ".jar");
         // undeploy wlar (though it should work without it)
         undeploy(SIMPLE_APPLICATION + ".wlar");

         // Only undeploy if deployed here
         if(!lIsInitiallyDeployed)
            undeploy(FOE_DEPLOYER);
      }
      catch(Exception e)
      {
         e.printStackTrace();
         throw e;
      }
   }

   // Private -------------------------------------------------------
   private SecretManager getSecretManagerEJB()
      throws Exception
   {
      log.debug("+++ getSecretManagerEJB()");
      Object lObject = getInitialContext().lookup( SECRET_SESSION_JNDI_NAME );
      SecretManagerHome lHome = (SecretManagerHome) PortableRemoteObject.narrow(
         lObject,
         SecretManagerHome.class
      );
      log.debug( "Found SecretManagerBean" );
      return lHome.create();
   }
}
