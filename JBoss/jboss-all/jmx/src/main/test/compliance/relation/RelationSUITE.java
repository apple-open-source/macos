/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package test.compliance.relation;

import junit.framework.Test;
import junit.framework.TestSuite;

/**
 * Tests for the relation service.
 *
 * @author <a href="mailto:AdrianBrock@HappeningTimes.com">Adrian Brock</a>.
 */
public class RelationSUITE
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
    TestSuite suite = new TestSuite("Relation Service Tests");

    suite.addTest(new TestSuite(MBeanServerNotificationFilterTestCase.class));
    suite.addTest(new TestSuite(RoleTestCase.class));
    suite.addTest(new TestSuite(RoleInfoTestCase.class));
    suite.addTest(new TestSuite(RoleListTestCase.class));
    suite.addTest(new TestSuite(RoleStatusTestCase.class));
    suite.addTest(new TestSuite(RoleUnresolvedTestCase.class));
    suite.addTest(new TestSuite(RoleUnresolvedListTestCase.class));
    suite.addTest(new TestSuite(RelationNotificationTestCase.class));
    suite.addTest(new TestSuite(RelationTypeSupportTestCase.class));
    suite.addTest(new TestSuite(RelationServiceTestCase.class));
    suite.addTest(new TestSuite(RelationSupportTestCase.class));

    return suite;
  }
}
