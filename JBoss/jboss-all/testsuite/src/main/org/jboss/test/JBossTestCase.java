/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.test;

import java.io.File;
import java.net.InetAddress;
import java.net.MalformedURLException;
import java.net.URL;
import java.util.HashSet;
import java.util.Set;
import javax.management.MBeanException;
import javax.management.MBeanRegistrationException;
import javax.management.MalformedObjectNameException;
import javax.management.ObjectInstance;
import javax.management.ObjectName;
import javax.management.ReflectionException;
import javax.management.RuntimeErrorException;
import javax.management.RuntimeMBeanException;
import javax.management.RuntimeOperationsException;
import javax.naming.Context;
import javax.naming.InitialContext;
import javax.naming.NamingEnumeration;
import javax.naming.NamingException;

import junit.framework.*;

import org.apache.log4j.Category;

import org.jboss.jmx.adaptor.rmi.RMIAdaptor;

/**
 * This is the standard base test case for jboss junit test cases. It supplies
 * access to log4j logging, the jboss jmx server, jndi, and a method for
 * deploying ejb packages. You may supply the name of the machine the jboss
 * server is on with the system property jbosstest.server.name (default
 * getInetAddress().getLocalHost().getHostName()) and the directory for
 * deployable packages with the system property jbosstest.deploy.dir (default
 * ../lib).
 *
 * @author    <a href="mailto:d_jencks@users.sourceforge.net">David Jencks</a>
 * @version   $Revision: 1.12.4.1 $
 */
public class JBossTestCase
       extends TestCase
{
    protected Category log;
   
   /**
    *  Saved exception from deployment.
    *  Will be thrown from {@link #testServerFound}, if not <code>null</code>.
    */
   protected static Exception deploymentException = null;

   protected JBossTestServices delegate;


   // Static --------------------------------------------------------
   // Constructors --------------------------------------------------
   /**
    * Constructor for the JBossTestCase object
    *
    * @param name  Test case name
    */
   public JBossTestCase(String name)
   {
      super(name);
      initDelegate();
      log = getLog();
   }
   
   public void initDelegate ()
   {
      delegate = new JBossTestServices(getClass().getName());
   }

   // Public --------------------------------------------------------


   /**
    * This just checks the server is there... so you should get at least one
    * success!
    * Also checks if an exception occurred during deployment, and throws
    * any such exception from here.
    *
    * @exception Exception  Description of Exception
    */
   public void testServerFound() throws Exception
   {
      if (deploymentException != null)
         throw deploymentException;
      assertTrue("Server was not found", getServer() != null);
   }

   /**
    * The JUnit setup method
    *
    * @exception Exception  Description of Exception
    * /
   protected void setUp() throws Exception
   {
      delegate.setUp();
      }*/

   /**
    * The teardown method for JUnit
    *
    * @exception Exception  Description of Exception
    * /
   protected void tearDown() throws Exception
   {
      delegate.tearDown();
   }*/



   //protected---------

   /**
    * Gets the InitialContext attribute of the JBossTestCase object
    *
    * @return   The InitialContext value
    */
   protected InitialContext getInitialContext() throws Exception
   {
      return delegate.getInitialContext();
   }

   /**
    * Gets the Server attribute of the JBossTestCase object
    *
    * @return   The Server value
    */
   protected RMIAdaptor getServer() throws Exception
   {
      return delegate.getServer();
   }

   /**
    * Gets the Log attribute of the JBossTestCase object
    *
    * @return   The Log value
    */
   protected Category getLog()
   {
      return delegate.getLog();
   }

   /**
    * Gets the DeployerName attribute of the JBossTestCase object
    *
    * @return                                  The DeployerName value
    * @exception MalformedObjectNameException  Description of Exception
    */
   protected ObjectName getDeployerName() throws MalformedObjectNameException
   {
      return delegate.getDeployerName();
   }


   /**
    * Returns the deployment directory to use. This does it's best to figure out
    * where you are looking. If you supply a complete url, it returns it.
    * Otherwise, it looks for jbosstest.deploy.dir or if missing ../lib. Then it
    * tries to construct a file url or a url.
    *
    * @param filename                   name of the file/url you want
    * @return                           A more or less canonical string for the
    *      url.
    * @exception MalformedURLException  Description of Exception
    */
   protected String getDeployURL(final String filename) throws MalformedURLException
   {
       return delegate.getDeployURL(filename); 
   }

   /** Get a URL string to a resource in the testsuite/output/resources dir.
    * This relies on the output/resources directory being in the
    * testcase classpath.
    *
    */
   protected String getResourceURL(final String resource) throws MalformedURLException
   {
      ClassLoader loader = Thread.currentThread().getContextClassLoader();
      URL resURL = loader.getResource(resource);
      return resURL != null ? resURL.toString() : null;
   }


   //is this good for something??????
   /**
    * Gets the Deployed attribute of the JBossTestCase object
    *
    * @param name  Description of Parameter
    * @return      The Deployed value
    */
   protected boolean isDeployed(String name)
   {
      return delegate.isDeployed(name);
   }

   /**
    * invoke wraps an invoke call to the mbean server in a lot of exception
    * unwrapping.
    *
    * @param name           ObjectName of the mbean to be called
    * @param method         mbean method to be called
    * @param args           Object[] of arguments for the mbean method.
    * @param sig            String[] of types for the mbean methods parameters.
    * @return               Object returned by mbean method invocation.
    * @exception Exception  Description of Exception
    */
   protected Object invoke(ObjectName name, String method, Object[] args, String[] sig) throws Exception
   {
      return delegate.invoke(name, method, args, sig);
   }

   /**
    * Deploy a package with the main deployer. The supplied name is
    * interpreted as a url, or as a filename in jbosstest.deploy.lib or ../lib.
    *
    * @param name           filename/url of package to deploy.
    * @exception Exception  Description of Exception
    */
   protected void deploy(String name) throws Exception
   {
      delegate.deploy(name);
   }

   /**
    * Undeploy a package with the main deployer. The supplied name is
    * interpreted as a url, or as a filename in jbosstest.deploy.lib or ../lib.
    *
    * @param name           filename/url of package to undeploy.
    * @exception Exception  Description of Exception
    */
   protected void undeploy(String name) throws Exception
   {
      delegate.undeploy(name);
   }

   public static Test getDeploySetup(final Test test, final String jarName) throws Exception
   {
      JBossTestSetup wrapper = new JBossTestSetup(test)
         {

             protected void setUp() throws Exception
             {
                deploymentException = null;
                try {
                   this.deploy(jarName);
                   this.getLog().debug("deployed package: " + jarName);
                } catch (Exception ex) {
                   // Throw this in testServerFound() instead.
                   deploymentException = ex;
                }
             }

             protected void tearDown() throws Exception
             {
                this.undeploy(jarName);
                this.getLog().debug("undeployed package: " + jarName);
             }
          };
      return wrapper;
   }

   public static Test getDeploySetup(final Class clazz, final String jarName) throws Exception
   {
      TestSuite suite = new TestSuite();  
      suite.addTest(new TestSuite(clazz));
      return getDeploySetup(suite, jarName);
   }

   protected int getThreadCount()
   {
      return delegate.getThreadCount();
   }

   protected int getIterationCount()
   {
      return delegate.getIterationCount();
   }

   protected int getBeanCount()
   {
      return delegate.getBeanCount();
   }



   //private methods--------------

}
