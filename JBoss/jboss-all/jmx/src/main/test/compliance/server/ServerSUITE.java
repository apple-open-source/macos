/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package test.compliance.server;

import junit.framework.Test;
import junit.framework.TestSuite;

public class ServerSUITE extends TestSuite
{
   public static void main(String[] args)
   {
      junit.textui.TestRunner.run(suite());
   }

   public static Test suite()
   {
      TestSuite suite = new TestSuite("General MBeanServer Tests");

      suite.addTest(new TestSuite(MBeanServerFactoryTEST.class));
      suite.addTest(new TestSuite(MBeanServerTEST.class));
      suite.addTest(new TestSuite(MBeanDelegateTEST.class));
      suite.addTest(new TestSuite(DefaultDomainTestCase.class));
      
      return suite;
   }

}
