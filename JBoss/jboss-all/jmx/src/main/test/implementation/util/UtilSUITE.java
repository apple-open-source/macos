/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package test.implementation.util;

import junit.framework.Test;
import junit.framework.TestSuite;

public class UtilSUITE extends TestSuite
{
   public static void main(String[] args)
   {
      junit.textui.TestRunner.run(suite());
   }

   public static Test suite()
   {
      TestSuite suite = new TestSuite("JBossMX Util Tests");

      suite.addTest(new TestSuite(MBeanProxyTEST.class));
      suite.addTest(new TestSuite(AgentIDTEST.class));
      
      return suite;
   }

}
