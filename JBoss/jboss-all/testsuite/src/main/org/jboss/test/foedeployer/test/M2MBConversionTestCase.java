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

import org.jboss.test.foedeployer.ejb.m2mb.M2MBManager;
import org.jboss.test.foedeployer.ejb.m2mb.M2MBManagerHome;

/**
 * Test of relationships conversion
 *
 * @author <a href="mailto:aloubyansky@hotmail.com">Alex Loubyansky</a>
 * @version $Revision: 1.1.2.2 $
 */
public class M2MBConversionTestCase
   extends JBossTestCase
{
   // Constants -----------------------------------------------------
   public static final String FOE_DEPLOYER = "foe-deployer-3.2.sar";
   public static final String FOE_DEPLOYER_NAME = "jboss:service=FoeDeployer";
   public static final String CONVERTOR_DEPLOYER_QUERY_NAME = "jboss:service=Convertor,*";
   public static final String APPLICATION = "foe-deployer-m2mb-test";
   public static final String MANAGER_SESSION_JNDI_NAME = "M2MBManagerEJB.M2MBManagerHome";

   // Static --------------------------------------------------------
   /**
    * Setup the test suite.
    */
   public static Test suite() throws Exception
   {
      TestSuite suite = new TestSuite();
      suite.addTest( new TestSuite( M2MBConversionTestCase.class ) );

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
   public M2MBConversionTestCase( String name )
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
         log.debug( "+++ testM2MBConversion" );

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
         String[] projects = {"JBoss", "Xdoclet", "WebWork"};
         String[] developers = {"Ivanov", "Petrov", "Sidorov"};

         M2MBManager manager = getM2MBManager();

         log.debug( "cleaning the database" );
         i = 0;
         while( i < projects.length)
            manager.removeProjectIfExists( projects[i++] );
         i = 0;
         while( i < developers.length)
            manager.removeDeveloperIfExists( developers[i++] );

         // create all projects
         i = 0;
         while( i < projects.length )
         {
            log.debug("creating project: " + projects[i] );
            manager.createProject( projects[i++] );
         }

         // create all developers
         i = 0;
         while( i < developers.length )
         {
            log.debug("creating developer: " + developers[i] );
            manager.createDeveloper( developers[i++] );
         }

         // adding projects except the last one to developer 0
         i = 0;
         while( i < projects.length - 1 )
         {
            log.debug("adding project " + projects[i]
               + " to developer " + developers[0]);
            manager.addProjectToDeveloper(developers[0], projects[i++]);
         }

         log.debug("developer " + developers[0] + " have projects: "
            + manager.getProjectsForDeveloper(developers[0]) );

         // adding developer 0 to the last project
         log.debug("adding developer " + developers[0]
               + " to project " + projects[ projects.length - 1 ]);
         manager.addDeveloperToProject(projects[ projects.length-1 ], developers[0]);

         // check whether the developer have all the projects
         Collection prjs = manager.getProjectsForDeveloper(developers[0]);
         log.debug("developer " + developers[0] + " have projects: " + prjs);
         for( i=0; i < projects.length; ++i )
         {
            assertTrue( "Developer '" + developers[0]
               + "' doesn't have project '" + projects[i] + "'",
               prjs.contains(projects[i]) );
         }

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
   private M2MBManager getM2MBManager()
      throws Exception
   {
      log.debug("looking for M2MBManager");
      Object ref = getInitialContext().lookup( MANAGER_SESSION_JNDI_NAME );
      M2MBManagerHome home = (M2MBManagerHome) PortableRemoteObject.narrow(
         ref, M2MBManagerHome.class );
      return home.create();
   }
}
