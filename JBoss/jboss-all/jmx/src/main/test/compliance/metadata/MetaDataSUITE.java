/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package test.compliance.metadata;

import junit.framework.Test;
import junit.framework.TestSuite;

/**
 * Test cases for JMX metadata classes.
 *
 * @author  <a href="mailto:juha@jboss.org">Juha Lindfors</a>.
 * @version $Revision: 1.3 $ 
 */
public class MetaDataSUITE
   extends TestSuite
{
   
  public static void main(String[] args)
   {
      junit.textui.TestRunner.run(suite());
   }

   public static Test suite()
   {
      TestSuite suite = new TestSuite("JBossMX Meta Data Tests");

      suite.addTest(new TestSuite(MBeanFeatureInfoTEST.class));
      suite.addTest(new TestSuite(MBeanOperationInfoTEST.class));
      suite.addTest(new TestSuite(MBeanAttributeInfoTEST.class));
      
      return suite;
   }
   
}
      



