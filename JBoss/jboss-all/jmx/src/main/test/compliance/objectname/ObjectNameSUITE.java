/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package test.compliance.objectname;

import junit.framework.Test;
import junit.framework.TestSuite;

public class ObjectNameSUITE extends TestSuite
{
   public static void main(String[] args)
   {
      junit.textui.TestRunner.run(suite());
   }

   public static Test suite()
   {
      TestSuite suite = new TestSuite("All ObjectName Tests");

      suite.addTest(new TestSuite(BasicTEST.class));
      suite.addTest(MalformedSUITE.suite());
      suite.addTest(new TestSuite(PatternTEST.class));
      suite.addTest(new TestSuite(CanonicalTEST.class));

      return suite;
   }
}
