/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.test.cts.test;

import junit.framework.Test;
import junit.framework.TestSuite;

import org.jboss.test.JBossTestCase;
import org.jboss.test.cts.ejb.LocalEjbTests;

/** Basic conformance tests for stateless sessions
 *
 *  @author Scott.Stark@jboss.org
 *  @version $Revision: 1.1.2.2 $
 */
public class LocalEjbTestCase extends JBossTestCase
{

   public LocalEjbTestCase(String name)
   {
      super(name);
   }

	public static Test suite() throws Exception
   {
		TestSuite testSuite = new TestSuite("LocalEjbTestCase");
		testSuite.addTestSuite(LocalEjbTests.class);
		return getDeploySetup(testSuite, "cts.jar");
   }

}
