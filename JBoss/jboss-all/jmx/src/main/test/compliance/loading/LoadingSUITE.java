/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package test.compliance.loading;

import junit.framework.Test;
import junit.framework.TestSuite;

public class LoadingSUITE extends TestSuite
{
   public static void main(String[] args)
   {
      junit.textui.TestRunner.run(suite());
   }

   public static Test suite()
   {
      TestSuite suite = new TestSuite("MLet Tests");

      suite.addTest(new TestSuite(MLetTEST.class));
      
      return suite;
   }

}
