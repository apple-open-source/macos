/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 *
 */
package org.jboss.test.cmp2.perf.test;

import javax.naming.InitialContext;

import junit.framework.Test;
import org.jboss.test.JBossTestCase;
import org.jboss.test.cmp2.perf.interfaces.CheckBookMgrHome;
import org.jboss.test.cmp2.perf.interfaces.CheckBookMgr;

/**
 *
 * @author Scott.Stark@jboss.org
 * @version 1.0
 */
public class PerfUnitTestCase
   extends JBossTestCase
{

   // Constructor -----------------------------------
   public PerfUnitTestCase(String name)
   {
      super(name);
   }

   // TestCase overrides ----------------------------
   public static Test suite() throws Exception
   {
      return getDeploySetup(PerfUnitTestCase.class, "cmp2-perf.jar");
   }

   // Tests -----------------------------------------
   public void testCheckBookBalance() throws Exception
   {
      InitialContext ctx = getInitialContext();
      CheckBookMgrHome home = (CheckBookMgrHome) ctx.lookup("cmp2/perf/CheckBookMgrHome");
      CheckBookMgr mgr = home.create("Acct123456789USD", 10000);
      long start = System.currentTimeMillis();
      int entryCount = mgr.getEntryCount();
      double balance = mgr.getBalance();
      long end = System.currentTimeMillis();
      double expectedBalance = 10000 - entryCount;
      assertTrue(expectedBalance+" == "+balance, balance == expectedBalance);
      mgr.remove();
      getLog().info("getBalance() time: "+(end - start));
   }

}
