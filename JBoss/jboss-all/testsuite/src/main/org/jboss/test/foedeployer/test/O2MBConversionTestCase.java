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
import java.util.Collection;
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
import org.jboss.test.foedeployer.ejb.o2mb.O2MBManager;
import org.jboss.test.foedeployer.ejb.o2mb.O2MBManagerHome;

/**
 * Test of relationships conversion
 *
 * @author <a href="mailto:loubyansky@hotmail.com">Alex Loubyansky</a>
 * @version $Revision: 1.2.2.2 $
 */
public class O2MBConversionTestCase
   extends JBossTestCase
{
   // Constants -----------------------------------------------------
   public static final String FOE_DEPLOYER = "foe-deployer-3.2.sar";
   public static final String FOE_DEPLOYER_NAME = "jboss:service=FoeDeployer";
   public static final String CONVERTOR_DEPLOYER_QUERY_NAME = "jboss:service=Convertor,*";
   public static final String APPLICATION = "foe-deployer-o2mb-test";
   public static final String MANAGER_SESSION_JNDI_NAME = "O2MBManagerEJB.O2MBManagerHome";

   // Static --------------------------------------------------------
   /**
    * Setup the test suite.
    */
   public static Test suite() throws Exception
   {
      TestSuite suite = new TestSuite();
      suite.addTest( new TestSuite( O2MBConversionTestCase.class ) );

      // Create an initializer for the test suite
      TestSetup wrapper = new JBossTestSetup( suite )
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
      return wrapper;
   }

   // Constructors --------------------------------------------------
   public O2MBConversionTestCase( String name )
   {
      super( name );
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
         log.debug( "+++ testO2MBConversion" );

         // First check if foe-deployer is deployed
         boolean isInitiallyDeployed = getServer().isRegistered( new ObjectName( FOE_DEPLOYER_NAME ) );
         if( !isInitiallyDeployed ) deploy(FOE_DEPLOYER);

         boolean isDeployed = getServer().isRegistered(new ObjectName(FOE_DEPLOYER_NAME));
         assertTrue("Foe-Deployer is not deployed", isDeployed);

         // Count number of convertors (must be a list one)
         int count = getServer().queryNames(new ObjectName(CONVERTOR_DEPLOYER_QUERY_NAME), null).size();
         assertTrue("No Convertor found on web server", count > 0);

         // Deploy the simple application
         deploy(APPLICATION + ".wlar");

         // Because the Foe-Deployer copies the converted JAR back to the original place
         // it has to be deployed from here again
         deploy(APPLICATION + ".jar");

         // Access the Session Bean and invoke some methods on it
         int i;
         String companyName = "Romashka";
         String[] employees = {"Ivanov", "Petrov", "Sidorov"};

         O2MBManager manager = getO2MBManager();

         log.debug( "cleaning the database" );
         manager.removeCompanyIfExists( companyName );

         log.debug( "creating company: " + companyName );
         manager.createCompany( companyName );

         // create all employees except the last one
         for( i = 0; i < employees.length - 1; ++i )
         {
            log.debug( "creating employee '" + employees[i]
               + "' for company '" + companyName + "'" );
            manager.createEmployeeForCompany( employees[i], companyName );
         }

         // fetch created employees
         Collection emps = manager.getEmployeesForCompany( companyName );
         log.debug("employees for company '" + companyName + "': " + emps );

         log.debug( "checking whether employees employed by the company" );
         for( i = 0; i < employees.length - 1; ++i )
         {
            assertTrue( "Employee '" + employees[i] + "' must have been employed",
               emps.contains( employees[ i ] ) );
         }

         log.debug( "creating the last employee: " + employees[employees.length-1] );
         manager.createEmployee( employees[ employees.length - 1 ] );

         log.debug( "employ '" + employees[ employees.length-1 ] + "'" );
         manager.employ( employees[employees.length-1], companyName );

         // verifying the last employee is employed
         assertTrue( "Employee '" + employees[ employees.length-1 ]
            + "' must have been employed",
            companyName.equals(
               manager.getCompanyForEmployee( employees[ employees.length-1 ] ) )
         );

         log.debug( "checking whether all employees are employed by the company" );
         emps = manager.getEmployeesForCompany( companyName );
         for( i = 0; i < employees.length; ++i )
         {
            assertTrue( "Employee '" + employees[i] + "' must have been employed",
               emps.contains( employees[ i ] ) );
         }

         log.debug("removing company: " + companyName );
         manager.removeCompany( companyName );

         // Undeploy converted application to clean up
         undeploy(APPLICATION + ".jar");
         // undeploy wlar (though it should work without it)
         undeploy(APPLICATION + ".wlar");

         // Only undeploy if deployed here
         if(!isInitiallyDeployed) undeploy( FOE_DEPLOYER );
      }
      catch(Exception e)
      {
         e.printStackTrace();
         throw e;
      }
   }

   // Private -------------------------------------------------------
   private O2MBManager getO2MBManager()
      throws Exception
   {
      log.debug("looking for O2MBManager");
      Object ref = getInitialContext().lookup( MANAGER_SESSION_JNDI_NAME );
      O2MBManagerHome home = (O2MBManagerHome) PortableRemoteObject.narrow(
         ref, O2MBManagerHome.class );
      return home.create();
   }
}
