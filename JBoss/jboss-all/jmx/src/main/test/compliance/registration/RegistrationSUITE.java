/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package test.compliance.registration;

import junit.framework.Test;
import junit.framework.TestSuite;

public class RegistrationSUITE extends TestSuite
{
   public static void main(String[] args)
   {
      junit.textui.TestRunner.run(suite());
   }

   public static Test suite()
   {
      TestSuite suite = new TestSuite("All MBeanRegistration Related Tests");

      suite.addTest(new TestSuite(RegistrationTEST.class));

      return suite;
   }

}
