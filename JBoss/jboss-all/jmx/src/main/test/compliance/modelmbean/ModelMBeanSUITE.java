/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package test.compliance.modelmbean;

import junit.framework.Test;
import junit.framework.TestSuite;

public class ModelMBeanSUITE extends TestSuite
{
   public static void main(String[] args)
   {
      junit.textui.TestRunner.run(suite());
   }

   public static Test suite()
   {
      TestSuite suite = new TestSuite("All ModelMBean Related Tests");

      suite.addTest(new TestSuite(ModelMBeanTEST.class));
      suite.addTest(new TestSuite(DescriptorTEST.class));
      suite.addTest(new TestSuite(ModelMBeanInfoSupportTEST.class));
      
      return suite;
   }

}
