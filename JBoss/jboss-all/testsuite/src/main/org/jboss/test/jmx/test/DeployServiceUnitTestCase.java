/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.test.jmx.test;

import java.io.File;
import java.net.URL;
import java.util.Collection;
import javax.management.ObjectInstance;
import javax.management.ObjectName;
import javax.management.ReflectionException;

import org.jboss.deployment.IncompleteDeploymentException;
import org.jboss.test.JBossTestCase;

/**
 * @author <a href="mailto:d_jencks@users.sourceforge.net">David Jencks</a>
 * @author Scott.Stark@jboss.org
 * @version $Revision: 1.19.2.3 $
 */
public class DeployServiceUnitTestCase
       extends JBossTestCase
{
   // Constants -----------------------------------------------------
   protected final static int INSTALLED = 0;
   protected final static int CONFIGURED = 1;
   protected final static int CREATED = 2;
   protected final static int RUNNING = 3;
   protected final static int FAILED = 4;
   protected final static int STOPPED = 5;
   protected final static int DESTROYED = 6;
   protected final static int NOTYETINSTALLED = 7;
   // Attributes ----------------------------------------------------

   ObjectName serviceControllerName;
   // Static --------------------------------------------------------
   // Constructors --------------------------------------------------
   /**
    * Constructor for the DeployServiceUnitTestCase object
    *
    * @param name  Test case name
    */
   public DeployServiceUnitTestCase(String name)
   {
      super(name);
      try 
      {
         serviceControllerName = new ObjectName("jboss.system:service=ServiceController");
      } 
      catch (Exception e) 
      {
      } // end of try-catch

   }

   // Public --------------------------------------------------------

   /**
    * Test deployment and undeployment of *-service.xml files. Make sure mbeans
    * are removed on undeployment
    *
    * @exception Exception  Description of Exception
    */
   public void testDeployXML() throws Exception
   {
      // The class loader used to locate the configuration file
      ClassLoader loader = Thread.currentThread().getContextClassLoader();
      assertTrue("ContextClassloader missing", loader != null);
      //Get URL for deployable *service.xml file in resources
      URL url = loader.getResource("jmx/test-service.xml");
      if (url == null)
      {
         //if we're running from the jmxtest.jar, it should be here instead
         url = loader.getResource("test-service.xml");
      }
      assertTrue("resource test-service.xml not found", url != null);
      String testUrl = url.toString();

      //the mbeans we are trying to deploy/undeploy
      ObjectName testObjectName = new ObjectName("jboss.test:service=LocalTxCM,name=XmlDeployTestDS");
      //check they aren't there already
      //Well, lets try to kill it.
      if (getServer().isRegistered(testObjectName)) 
      {
         getServer().unregisterMBean(testObjectName);
      } // end of if ()
      
      assertTrue("test mbean already registered before deploy", !getServer().isRegistered(testObjectName));

      //deploy the test xml doc.
      deploy(testUrl);

      //check deployment registered expected mbeans
      assertTrue("test mbean not registered after deploy", checkState(testObjectName, RUNNING));

      //deploy the test xml doc again, should undeploy and redeploy.
      undeploy(testUrl);
      deploy(testUrl);

      //check deployment registered expected mbeans
      assertTrue("test mbean not registered after redeploy", checkState(testObjectName, RUNNING));

      //undeploy test xml doc.
      undeploy(testUrl);

      //check they aren't there any more
      assertTrue("test mbean still registered after undeploy", !getServer().isRegistered(testObjectName));

   }

   /** Test that a spaces in the classpath element archives attribute do not
    * affect the classpath seen. 
    * @throws Exception
    */ 
   public void testSpaceInClasspath() throws Exception
   {
      getLog().debug("+++ testSpaceInClasspath");
      //deploy the test xml doc.
      deploy("archivestest-service.xml");
      ObjectName testObjectName = new ObjectName("test:name=TestSpaceInClasspath");
      //check deployment registered expected mbeans
      assertTrue(testObjectName+" registered", checkState(testObjectName, RUNNING));      
      undeploy("archivestest-service.xml");
   }

   /**
    * Test deployment and undeployment of a service archive (sar). Test that
    * after undeployment classes from the sar are not available.
    *
    * @exception Exception  Description of Exception
    */
   public void testDeploySAR() throws Exception
   {
      //Find the testdeploy.sar file in lib directory with other jars.

      String testUrl = "testdeploy.sar";
      getLog().debug("testUrl is : " + testUrl);

      //the mbeans we are trying to deploy/undeploy
      ObjectName testObjectName = new ObjectName("test:name=TestDeployer");
      ObjectName testObjectName2 = new ObjectName("test:name=TestDeployer2");
      ObjectName testObjectName3 = new ObjectName("test:name=TestDeployer3");
      //check they aren't there already
      assertTrue("test mbean already registered before deploy", !getServer().isRegistered(testObjectName));

      //deploy the test xml doc.
      deploy(testUrl);

      //check deployment registered expected mbeans
      assertTrue("test mbean not registered after deploy", checkState(testObjectName, RUNNING));

      //make sure we can create an mbean based on the class we just deployed.
      try
      {
         getServer().createMBean("org.jboss.test.jmx.mbean.TestDeployer", testObjectName2);
      }
      catch (Exception e)
      {
         fail("could not create mbean after class loaded in jsr" + e);
      }
      //now remove it again
      try
      {
         getServer().unregisterMBean(testObjectName2);
      }
      catch (Exception e)
      {
         fail("could not remove mbean after class loaded in jsr" + e);
      }
      //deploy the test xml doc again, should undeploy and redeploy.
      undeploy(testUrl);
      deploy(testUrl);

      //check deployment registered expected mbeans
      assertTrue("test mbean not registered after redeploy", checkState(testObjectName, RUNNING));

      //undeploy test xml doc.
      undeploy(testUrl);

      //check they aren't there any more
      assertTrue("test mbean still registered after undeploy", !getServer().isRegistered(testObjectName));

      //check the class is not available
      try
      {
         ObjectInstance oe = getServer().createMBean("org.jboss.test.jmx.mbean.TestDeployer", testObjectName3);
         fail("created mbean when class should not be present: object instance: " + oe);
      }
      catch (ReflectionException re)
      {
         Exception e = re.getTargetException();
         if (!(e instanceof ClassNotFoundException))
         {
            fail("Wrong exception thrown when trying to create mbean" + e);
         }
      }

      //deploy the test xml doc a second time.
      deploy(testUrl);

      //check deployment registered expected mbeans
      assertTrue("test mbean not registered after deploy", checkState(testObjectName, RUNNING));

      //undeploy test xml doc.
      undeploy(testUrl);

      //check they aren't there any more
      assertTrue("test mbean still registered after undeploy", !getServer().isRegistered(testObjectName));

   }

   /**
    * Test deployment and undeployment of a service archive (sar). Test that
    * after undeployment classes from the sar are not available.
    *
    * @exception Exception  Description of Exception
    */
   public void testDeploySARWithJar() throws Exception
   {
      String testUrl = "testdeploye.sar";
      getLog().debug("testUrl is : " + testUrl);

      ObjectName testObjectName = new ObjectName("test:name=TestDeployerE");
      deploy(testUrl);

      // See that deployment registered expected mbeans
      assertTrue("test:name=TestDeployerE is registered after deploy",
         checkState(testObjectName, RUNNING));

      // Invoke the accessUtilClass operation
      Object[] args = {};
      String[] sig = {};
      getServer().invoke(testObjectName, "accessUtilClass", args, sig);

      //make sure we can create an mbean based on the class we just deployed.
      ObjectName testObjectName2 = new ObjectName("test:name=TestDeployerE#2");
      getServer().createMBean("org.jboss.test.jmx.mbeane.TestDeployerE", testObjectName2);

      // Remove the instance
      getServer().unregisterMBean(testObjectName2);

      undeploy(testUrl);
   }

   /**
    * Test some deployment/undeployment dependencies for sars. C depends on A
    * and B. Make sure undeploying A undeploys C, and redeploying A redeploys C.
    *
    * @exception Exception  Description of Exception
    */
   public void testDependsElement() throws Exception
   {
      //C depends on A and B
      //Find the testdeploy[ABC].sar files in lib directory with other jars.

      String testUrlA = "testdeploya.sar";
      String testUrlB = "testdeployb.sar";
      String testUrlC = "testdeployc.sar";
      getLog().debug("testUrlA is : " + testUrlA);

      //the mbeans we are trying to deploy/undeploy
      ObjectName testObjectNameA = new ObjectName("test:name=TestDeployerA");
      ObjectName testObjectNameB = new ObjectName("test:name=TestDeployerB");
      ObjectName testObjectNameC = new ObjectName("test:name=TestDeployerC");
      try
      {
         //check they aren't there already
         assertTrue("test mbean already registered before deploy", !getServer().isRegistered(testObjectNameA));
         assertTrue("test mbean already registered before deploy", !getServer().isRegistered(testObjectNameB));
         assertTrue("test mbean already registered before deploy", !getServer().isRegistered(testObjectNameC));

         //deploy the test jsrs.
         deploy(testUrlA);
         deploy(testUrlB);
         deploy(testUrlC);

         //check deployment registered expected mbeans
         assertTrue("test mbean A not running after deploy", checkState(testObjectNameA, RUNNING));
         assertTrue("test mbean B not running after deploy", checkState(testObjectNameB, RUNNING));
         assertTrue("test mbean C not running after deploy", checkState(testObjectNameC, RUNNING));

         //we'll believe from testDeployJSR that the classes are available.

         //undeploy test xml doc.
         undeploy(testUrlA);

         //check they aren't there any more or they have stopped.
         assertTrue("test mbean A still registered after undeploy of A", !getServer().isRegistered(testObjectNameA));
         assertTrue("test mbean C not stopped after undeploy of A", !checkState(testObjectNameC, RUNNING));
         assertTrue("test mbean B stopped after undeploy of A", checkState(testObjectNameB, RUNNING));

         //Now undeploy B, should prevent redeploy of C when A is redeployed.
         
         // not working, skip for now
         undeploy(testUrlB);
         assertTrue("test mbean B is registered after undeploy of B", !getServer().isRegistered(testObjectNameB));

         //deploy the test jsr A doc a second time.
         deploy(testUrlA);
         
         // check deployment registered expected mbeans
         assertTrue("test mbean A not registered after deploy of A", checkState(testObjectNameA, RUNNING));
         assertTrue("test mbean B is registered after deploy of A", !getServer().isRegistered(testObjectNameB));
         assertTrue("test mbean C started after deploy of A, with B unregistered", !checkState(testObjectNameC, RUNNING));

         //now redeploy B, should also redeploy C
         deploy(testUrlB);
         //check deployment registered expected mbeans- all three should be registered
         assertTrue("test mbean A not running after deploy of B", checkState(testObjectNameA, RUNNING));
         assertTrue("test mbean B not running after deploy of B", checkState(testObjectNameB, RUNNING));
         assertTrue("test mbean C not running after deploy of B", checkState(testObjectNameC, RUNNING));

         //undeploy test xml doc.
         undeploy(testUrlC);
         undeploy(testUrlA);
         undeploy(testUrlB);

         //check they aren't there any more
         assertTrue("test mbean still registered after undeploy", !getServer().isRegistered(testObjectNameA));
         assertTrue("test mbean still registered after undeploy", !getServer().isRegistered(testObjectNameB));
         assertTrue("test mbean still registered after undeploy", !getServer().isRegistered(testObjectNameC));
      }
      finally
      {
         undeploy(testUrlC);
         undeploy(testUrlA);
         undeploy(testUrlB);
      }

   }

   /**
    * Test depends tag D depends on mbeans in A and C.
    * Deploying D should wait for A and C; undeploying A and/or C should 
    * undeploy D's mbean.  Redeploying both should resuscitate D's mbean.
    *
    * @exception Exception  Description of Exception
    */
   public void testDependsListElement() throws Exception
   {
      //C depends on A and B via anonymous depends element.
      //D depends on  A and C via anonymous dependsList.
      //Find the testdeploy[ABC].sar files in lib directory with other jars.

      String testUrlA = "testdeploya.sar";
      String testUrlB = "testdeployb.sar";
      String testUrlC = "testdeployc.sar";
      String testUrlD = "testdeployd.sar";
      getLog().debug("testUrlA is : " + testUrlA);

      //the mbeans we are trying to deploy/undeploy
      ObjectName testObjectNameA = new ObjectName("test:name=TestDeployerA");
      ObjectName testObjectNameB = new ObjectName("test:name=TestDeployerB");
      ObjectName testObjectNameC = new ObjectName("test:name=TestDeployerC");
      ObjectName testObjectNameD = new ObjectName("test:name=TestDeployerD");
      try
      {
         //check they aren't there already
         assertTrue("test mbean a already registered before deploy", !getServer().isRegistered(testObjectNameA));
         assertTrue("test mbean b already registered before deploy", !getServer().isRegistered(testObjectNameB));
         assertTrue("test mbean c already registered before deploy", !getServer().isRegistered(testObjectNameC));
         assertTrue("test mbean d already registered before deploy", !getServer().isRegistered(testObjectNameD));

         //deploy the test jsrs.
         try 
         {
            deploy(testUrlD);
            fail("D deployed without dependencies");

         }
         catch (IncompleteDeploymentException e)
         {
            //expected
         } // end of try-catch
         

         assertTrue("test mbean D started with A, B, and C unregistered", !checkState(testObjectNameD, RUNNING));


         //deploy A.
         deploy(testUrlA);
         //A should be started, not anything else
         assertTrue("test mbean A not started", checkState(testObjectNameA, RUNNING));
         assertTrue("test mbean D started with A, B, and C unregistered", !checkState(testObjectNameD, RUNNING));

         //Deploy C, should not start since B is not deployed
         try 
         {
            deploy(testUrlC);
            fail("C deployed completely, D should still be waiting");
         }
         catch (IncompleteDeploymentException e)
         {
            //expected
         } // end of try-catch
         assertTrue("test mbean C started with B unregistered", !checkState(testObjectNameC, RUNNING));



         //deploy the test bean B.  C and D should start too.
         deploy(testUrlB);
         assertTrue("test mbean A not started", checkState(testObjectNameA, RUNNING));
         assertTrue("test mbean B not started", checkState(testObjectNameB, RUNNING));
         assertTrue("test mbean C not started", checkState(testObjectNameC, RUNNING));
         assertTrue("test mbean D not started", checkState(testObjectNameD, RUNNING));


         //undeploy test xml doc.
         undeploy(testUrlA);
         assertTrue("test mbean A present after undeploy", !getServer().isRegistered(testObjectNameA));
         assertTrue("test mbean B not started after undeploy of A", checkState(testObjectNameB, RUNNING));
         assertTrue("test mbean C started after undeploy of A", !checkState(testObjectNameC, RUNNING));
         assertTrue("test mbean D started after undeploy of A", !checkState(testObjectNameD, RUNNING));
         undeploy(testUrlC);
         undeploy(testUrlB);
         undeploy(testUrlD);

         //check they aren't there any more
         Collection ds = (Collection)getServer().invoke(serviceControllerName, 
                                                  "listDeployedNames",
                                                  new Object[] {},
                                                  new String[] {});
         assertTrue("test mbean A still registered after undeploy", !ds.contains(testObjectNameA));

         assertTrue("test mbean B still registered after undeploy", !ds.contains(testObjectNameB));

         assertTrue("test mbean C still registered after undeploy", !ds.contains(testObjectNameC));
         assertTrue("test mbean D still registered after undeploy", !ds.contains(testObjectNameD));
      }
      finally
      {
         try
         {
            undeploy(testUrlD);
         }
         catch (Exception e)
         {
         }
         try
         {
            undeploy(testUrlC);
         }
         catch (Exception e)
         {
         }
         try
         {
            undeploy(testUrlA);
         }
         catch (Exception e)
         {
         }
         try
         {
            undeploy(testUrlB);
         }
         catch (Exception e)
         {
         }

      }

   }

    /**
     * The <code>testCopyLocalDir</code> method tests the local-directory element
     * in jboss-system.xml.  
     *
     * @exception Exception if an error occurs
     */
   /*Needs a package not recognized as possibly deployable
   public void testCopyLocalDir() throws Exception
   {

      String testUrl = "testcopylocaldir.sar";
      getLog().debug("testUrl is : " + testUrl);

      //the mbeans we are trying to deploy/undeploy
      ObjectName testObjectName = new ObjectName("test:name=TestCopyLocalDir");
      try
      {
         //check they aren't there already
         assertTrue("test mbean already registered before deploy", !getServer().isRegistered(testObjectName));

         //deploy C
         deploy(testUrl);
         String base = (String)getServer().getAttribute(testObjectName, "BaseDir");
         File f = new File(base + File.separator + "db" + File.separator + "local-directory");
         //local dir should still be there after undeploy.
         undeploy(testUrl);  
         assertTrue("local-directory not found!", f.exists());
         File[] subs = f.listFiles();
         assertTrue("Subdir missing!", subs.length != 0);


         //delete it, redeploy, check again - it might have been there before we deployed.
         recursiveDelete(f);
         deploy(testUrl);
         assertTrue("local-directory not found!", f.exists());
         subs = f.listFiles();
         assertTrue("Subdir missing!", subs.length != 0);
         recursiveDelete(f);
      }
      finally 
      {
         undeploy(testUrl);  
      } // end of finally
   }
   */
   
   public void testConfigureError() throws Exception
   {

      String testUrl = "testdeploy.sar";
      getLog().debug("testUrl is : " + testUrl);

      //deploy sar
      deploy(testUrl);
      try 
      {
         //deploy service.xml with a nonexistent attribute to cause
         //a configuration error
         String errorUrl = "testConfigError-service.xml";
         String fixedUrl = "testConfigFixed-service.xml";
         ObjectName errorObjectName = new ObjectName("test:name=TestConfigError");
         try 
         {
            deploy(errorUrl);
         } catch (Exception e) 
         {
            //??
            getLog().info("deploying errorUrl gave exception: ", e);
         } // end of try-catch

         if (getServer().isRegistered(errorObjectName)) 
         {
            System.out.println("Erroneous mbean state: " + (Integer)getServer().getAttribute(errorObjectName, "State"));
            //assertTrue("Erroneous mbean is not failed!", checkState(errorObjectName, FAILED));
         } // end of if ()
         
         //the mbeans we are trying to deploy/undeploy
         try 
         {
            undeploy(errorUrl);
   

         } catch (Exception e) 
         {
   
         } // end of try-catch

         try 
         {
            deploy(fixedUrl);
            assertTrue("Corrected mbean is not registered!", checkState(errorObjectName, RUNNING));

         } finally
         {
            undeploy(fixedUrl);
         } // end of try-catch
      } finally 
      {
         undeploy(testUrl);
      } // end of try-catch
      
   }


   public void testCrashInStart() throws Exception
   //Thanks to David Budworth for this test.
   {
      String testUrl = "testcrashinstart.sar";
      getLog().debug("testUrl is : " + testUrl);
      ObjectName dontCrashName = new ObjectName("CrashTest:name=DontCrash");
      ObjectName crashName = new ObjectName("CrashTest:name=Crash");

      //deploy sar
      try 
      {
         deploy(testUrl);
         fail("expected IncompleteDeploymentException");
      }
      catch (IncompleteDeploymentException e)
      {
         //expected
      } // end of try-catch
      

      try 
      {
         assertTrue("dontcrash mbean is missing!", getServer().isRegistered(dontCrashName));
         assertTrue("crash mbean is missing!", getServer().isRegistered(crashName));

         assertTrue("dontcrash mbean is started!", !getServer().getAttribute(dontCrashName, "StateString").equals("Started"));
         assertTrue("crash mbean is started!", !getServer().getAttribute(crashName, "StateString").equals("Started"));

         undeploy(testUrl);
         assertTrue("dontcrash mbean is registered!", !getServer().isRegistered(dontCrashName));
         assertTrue("crash mbean is registered!", !getServer().isRegistered(crashName));

      } finally 
      {
         undeploy(testUrl);
      } // end of try-catch
      
   }


   public void testNullInfoInDynamicMBean() throws Exception
   //Thanks to David Budworth for this test.
   //Tries to deploy a DynamicMBean that returns null from getMBeanInfo.
   //The Sun RI jmx registers this invalid mbean!!
   {
      String testUrl = "testnullinfo.sar";
      getLog().debug("testUrl is : " + testUrl);
      ObjectName nullInfoName = new ObjectName("NullInfoTest:name=NullInfo");
      //deploy sar
      try 
      {
         deploy(testUrl);
         fail("Was able to deploy invalid sar");
      }
      catch(Exception e)
      {
         getLog().debug("Deployment failed as expected", e);
         boolean isRegistered = getServer().isRegistered(nullInfoName);
         assertTrue("NullInfoTest:name=NullInfo is NOT registered", !isRegistered);
      }
      finally
      {
         try
         {         
            undeploy(testUrl);
         }
         catch (Exception e) 
         {
         }

         try
         {         
            getServer().unregisterMBean(nullInfoName);
         }
         catch (Exception e) 
         {
         }
      } // end of try-catch

   }

   /** Test that a sar deployment descriptor that 
    */
   public void testDDEntityRefs() throws Exception
   {
      String testUrl = "entityref.sar";
      getLog().debug("testUrl is : " + testUrl);
      //deploy sar
      deploy(testUrl);
      //check deployment registered expected mbeans
      ObjectName name = new ObjectName("test:name=EntityRefTest");
      assertTrue("test:name=EntityRefTest is registered after deploy", checkState(name, RUNNING));
      undeploy(testUrl);
   }

   /** Test that a sar deployment that has its service class in the default
    * package
    */
   public void testDefaultPkgService() throws Exception
   {
      String testUrl = "defaultpkg.sar";
      getLog().debug("testUrl is : " + testUrl);
      //deploy sar
      deploy(testUrl);
      //check deployment registered expected mbeans
      ObjectName name = new ObjectName("test:name=DefaultPkgService");
      assertTrue("test:name=DefaultPkgService is registered after deploy", checkState(name, RUNNING));
      undeploy(testUrl);
   }

   protected boolean recursiveDelete(File f)
   {
      if (f.isDirectory())
      {
         File[] files = f.listFiles();
         for (int i = 0; i < files.length; ++i)
         {
            if (!recursiveDelete(files[i]))
            {
               return false;
            }
         }
      }
      return f.delete();
   }

   protected boolean checkState(ObjectName mbean, int state) throws Exception
   {
      Integer mbeanState = (Integer)getServer().getAttribute(mbean, "State");
      return state == mbeanState.intValue();
   }

}
