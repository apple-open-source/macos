/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package test.compliance.openmbean;

import junit.framework.Test;
import junit.framework.TestSuite;

/**
 * Tests for the openmbeans.
 *
 * @author <a href="mailto:AdrianBrock@HappeningTimes.com">Adrian Brock</a>.
 */
public class OpenMBeanSUITE
  extends TestSuite
{
  /**
   * Run the tests
   * 
   * @param args the arguments for the test
   */
  public static void main(String[] args)
  {
    junit.textui.TestRunner.run(suite());
  }

  /**
   * Get a list of tests.
   *
   * @return the tests
   */
  public static Test suite()
  {
    TestSuite suite = new TestSuite("OpenMBean Tests");

    suite.addTest(new TestSuite(OpenTypeTestCase.class));
    suite.addTest(new TestSuite(SimpleTypeTestCase.class));
    suite.addTest(new TestSuite(CompositeTypeTestCase.class));
    suite.addTest(new TestSuite(TabularTypeTestCase.class));
    suite.addTest(new TestSuite(ArrayTypeTestCase.class));
    suite.addTest(new TestSuite(CompositeDataSupportTestCase.class));
    suite.addTest(new TestSuite(TabularDataSupportTestCase.class));
    suite.addTest(new TestSuite(OpenMBeanParameterInfoSupportTestCase.class));
    suite.addTest(new TestSuite(OpenMBeanAttributeInfoSupportTestCase.class));
    suite.addTest(new TestSuite(OpenMBeanConstructorInfoSupportTestCase.class));
    suite.addTest(new TestSuite(OpenMBeanOperationInfoSupportTestCase.class));
    suite.addTest(new TestSuite(OpenMBeanInfoSupportTestCase.class));

    return suite;
  }
}
