/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package test.compliance.standard;

import junit.framework.Test;
import junit.framework.TestSuite;

/**
 * @author  <a href="mailto:trevor@protocool.com">Trevor Squires</a>.
 */

public class StandardSUITE extends TestSuite
{
   public static void main(String[] args)
   {
      junit.textui.TestRunner.run(suite());
   }

   public static Test suite()
   {
      TestSuite suite = new TestSuite("StandardMBean Tests");

      suite.addTest(new TestSuite(TrivialTEST.class));
      suite.addTest(InfoTortureSUITE.suite());
      suite.addTest(InheritanceSUITE.suite());

      return suite;
   }
}
