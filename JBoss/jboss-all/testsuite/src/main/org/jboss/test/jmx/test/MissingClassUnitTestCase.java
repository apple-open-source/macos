
/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 *
 */

package org.jboss.test.jmx.test;


import javax.management.ObjectName;
import org.jboss.deployment.IncompleteDeploymentException;
import org.jboss.test.JBossTestCase;
import org.jboss.test.jmx.missingclass.MissingClassTestMBean;

/**
 * MissingClassUnitTestCase.java
 *
 *
 * Created: Fri Aug  9 14:20:49 2002
 *
 * @author <a href="mailto:d_jencks@users.sourceforge.net">David Jencks</a>
 * @version
 */

public class MissingClassUnitTestCase extends JBossTestCase {

   private static final String SERVICE = "missingclass-service.xml";
   private static final ObjectName SERVICE_NAME = MissingClassTestMBean.OBJECT_NAME;
   private static final String CLASS_PACKAGE = "missingclassmbean.jar";

   public MissingClassUnitTestCase (String name){
      super(name);
   }

   public void testDeployServiceWithoutClass() throws Exception
   {
      try
      {
	 try
	 {
	    assertTrue("Mbean is not registered before start of test", !getServer().isRegistered(SERVICE_NAME));
	    try
	    {
	       deploy(SERVICE);
	       fail("IncompleteDeploymentException expected");
	    }
	    catch (IncompleteDeploymentException ide)
	    {
	       //expected
	    }
	    assertTrue("Mbean is not registered after deployment since its class is missing", !getServer().isRegistered(SERVICE_NAME));

	    deploy(CLASS_PACKAGE);

	    log.info("Mbean is registered after deployment of its class" +  getServer().isRegistered(SERVICE_NAME));


	    assertTrue("Mbean is registered after deployment of its class", getServer().isRegistered(SERVICE_NAME));
	    undeploy(CLASS_PACKAGE);
	    assertTrue("Mbean is not registered after undeployment of its class", !getServer().isRegistered(SERVICE_NAME));
	    deploy(CLASS_PACKAGE);
	    assertTrue("Mbean is registered after redeployment of its class", getServer().isRegistered(SERVICE_NAME));
	    undeploy(CLASS_PACKAGE);
	    assertTrue("Mbean is not registered after undeployment of its class", !getServer().isRegistered(SERVICE_NAME));
	    undeploy(SERVICE);
	    assertTrue("Mbean is not registered after undeployment of its config", !getServer().isRegistered(SERVICE_NAME));
	 }
	 finally
	 {
	    try
	    {
	        undeploy(SERVICE);
	    }
	    catch (Throwable t) {}
	 }
      }
      finally
      {
	 try
	 {
	    undeploy (CLASS_PACKAGE);
	 }
	 catch (Throwable t) {}
      }
   }
   
}// MissingClassUnitTestCase
