/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package test.implementation.modelmbean;

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
      TestSuite suite = new TestSuite("JBossMX Model MBean Tests");

      suite.addTest(new TestSuite(XMBeanTEST.class));
      suite.addTest(new TestSuite(AttributeCacheTEST.class));
      
      return suite;
   }

}
