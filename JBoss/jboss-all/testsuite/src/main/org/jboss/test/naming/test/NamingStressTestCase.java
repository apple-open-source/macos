/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.test.naming.test;

import java.util.Properties;
import junit.framework.Test;

import org.jboss.test.JBossTestCase;
import org.jboss.test.JBossTestSuite;
import org.jboss.test.naming.ejb.NamingTests;

/** Stress tests for the JNDI naming layer
 *
 *  @author Scott.Stark@jboss.org
 *  @version $Revision: 1.1.2.1 $
 */
public class NamingStressTestCase extends JBossTestCase
{
   public NamingStressTestCase(String name)
   {
      super(name);
   }

	public static Test suite() throws Exception
   {
      Properties props = new Properties();
      props.setProperty("ejbRunnerJndiName", "EJBTestRunnerHome");
      props.setProperty("encBeanJndiName", "ENCBean");
      props.setProperty("encIterations", "1000");
		JBossTestSuite testSuite = new JBossTestSuite(props);
		testSuite.addTestSuite(NamingTests.class);
		return getDeploySetup(testSuite, "naming.jar");
   }

}
