/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package test.compliance.notcompliant;

import junit.framework.Test;
import junit.framework.TestSuite;

public class NCMBeanSUITE extends TestSuite
{
   public static void main(String[] args)
   {
      junit.textui.TestRunner.run(suite());
   }

   public static Test suite()
   {
      TestSuite suite = new TestSuite("All Not Compliant MBean Tests");

      suite.addTest(new TestSuite(NCMBeanTEST.class));

      return suite;
   }
}
