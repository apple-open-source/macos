/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.test;

import org.jboss.jmx.adaptor.rmi.RMIAdaptor;

import junit.framework.Test;
import junit.framework.TestSuite;

/**
 * Derived implementation of JBossTestCase for cluster testing.
 *
 * @see org.jboss.test.JBossTestCase
 *
 * @author  <a href="mailto:sacha.labourey@cogito-info.ch">Sacha Labourey</a>.
 * @version $Revision: 1.1.4.1 $
 *
 * <p><b>Revisions:</b>
 *
 * <p><b>12 avril 2002 Sacha Labourey:</b>
 * <ul>
 * <li> First implementation </li>
 * </ul>
 */

public class JBossClusteredTestCase extends JBossTestCase
{
   
   // Constants -----------------------------------------------------
   
   // Attributes ----------------------------------------------------
   
   // Static --------------------------------------------------------
   
   // Constructors --------------------------------------------------
   
   public JBossClusteredTestCase(String name)
   {
      super (name);
   }
   
   public void initDelegate ()
   {
      delegate = new JBossTestClusteredServices(getClass().getName());
   }
   
   // Public --------------------------------------------------------
   
   public void testServerFound() throws Exception
   {
      if (deploymentException != null)
         throw deploymentException;
      assertTrue("Server was not found", getServers() != null);
   }

   // Z implementation ----------------------------------------------
   
   // Y overrides ---------------------------------------------------
   
   // Package protected ---------------------------------------------
   
   // Protected -----------------------------------------------------
   
   protected RMIAdaptor[] getServers() throws Exception
   {
      return ((JBossTestClusteredServices)delegate).getServers();
   }

   public static Test getDeploySetup(final Test test, final String jarName) throws Exception
   {
      JBossTestSetup wrapper = new JBossTestClusteredSetup(test)
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
                
                // wait a few seconds so that the cluster stabilize
                //
                synchronized (this)
                {
                   wait (2000);
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

   // Private -------------------------------------------------------
   
   // Inner classes -------------------------------------------------
   
}
