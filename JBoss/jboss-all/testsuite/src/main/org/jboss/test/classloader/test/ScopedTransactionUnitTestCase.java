/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.test.classloader.test;

import org.jboss.test.JBossTestCase;
import org.jboss.test.classloader.scoping.transaction.interfaces.TestSession;
import org.jboss.test.classloader.scoping.transaction.interfaces.TestSessionHome;

import junit.framework.Test;
import junit.framework.TestSuite;

/**
 * Unit tests for class and transaction passing between scopes
 *
 * @author adrian@jboss.org
 * @version $Revision: 1.1.2.1 $
 */
public class ScopedTransactionUnitTestCase extends JBossTestCase
{
   public ScopedTransactionUnitTestCase(String name)
   {
      super(name);
   }

   public void testScopedTransaction() throws Exception
   {
      getLog().debug("+++ testScopedTransaction start");

      TestSessionHome home = (TestSessionHome) getInitialContext().lookup("ScopedTxTestSession");
      TestSession session = home.create();
      session.runTest();

      getLog().debug("+++ testScopedTransaction end");
   }

   public static Test suite() throws Exception
   {
      return getDeploySetup(ScopedTransactionUnitTestCase.class, "scopedtx.jar");
   }
}
