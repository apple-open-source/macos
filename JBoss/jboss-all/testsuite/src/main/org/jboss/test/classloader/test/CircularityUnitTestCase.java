package org.jboss.test.classloader.test;

import javax.management.ObjectName;

import org.jboss.jmx.adaptor.rmi.RMIAdaptor;
import org.jboss.test.JBossTestCase;
import org.jboss.test.JBossTestSetup;

import junit.framework.Test;
import junit.framework.TestSuite;
import junit.extensions.TestSetup;

/** Unit tests for the org.jboss.mx.loading.UnifiedLoaderRepository
 * @author Scott.Stark@jboss.org
 * @version $Revision: 1.1.4.5 $
 */
public class CircularityUnitTestCase extends JBossTestCase
{
   private static String JMX_NAME = "jboss.test:name=CircularityError";
   private ObjectName testObjectName;
   private Object[] args = {};
   private String[] sig = {};
   RMIAdaptor server;

   public CircularityUnitTestCase(String name) throws Exception
   {
      super(name);
      testObjectName = new ObjectName(JMX_NAME);
      server = getServer();
   }

   /** Test the UnifiedLoaderRepository for ClassCircularityError
    */
   public void testDuplicateClass() throws Exception
   {
      server.invoke(testObjectName, "testDuplicateClass", args, sig);
   }
   public void testUCLOwner() throws Exception
   {
      server.invoke(testObjectName, "testUCLOwner", args, sig);
   }
   public void testMissingSuperClass() throws Exception
   {
      server.invoke(testObjectName, "testMissingSuperClass", args, sig);
   }
   public void testLoading() throws Exception
   {
      server.invoke(testObjectName, "testLoading", args, sig);
   }
   public void testPackageProtected() throws Exception
   {
      server.invoke(testObjectName, "testPackageProtected", args, sig);
   }
   public void testDeadlockCase1() throws Exception
   {
      server.invoke(testObjectName, "testDeadlockCase1", args, sig);
   }
   public void testRecursiveLoadMT() throws Exception
   {
      server.invoke(testObjectName, "testRecursiveLoadMT", args, sig);
   }

   /**
    * Setup the test suite.
    */
   public static Test suite() throws Exception
   {
      TestSuite suite = new TestSuite();
      suite.addTest(new TestSuite(CircularityUnitTestCase.class));

      // Create an initializer for the test suite
      TestSetup wrapper = new JBossTestSetup(suite)
      {
         protected void setUp() throws Exception
         {
            super.setUp();
            deploy("circularity.sar");
         }
         protected void tearDown() throws Exception
         {
            undeploy("circularity.sar");
            super.tearDown();
         }
      };
      return wrapper;
   }

}
