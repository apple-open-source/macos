/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package test.compliance.monitor;

import junit.framework.Test;
import junit.framework.TestSuite;

/**
 * Tests for the monitor service.
 *
 * @author <a href="mailto:AdrianBrock@HappeningTimes.com">Adrian Brock</a>.
 */
public class MonitorSUITE
  extends TestSuite
{
  /**
   * The maximum wait for a notification
   */
  public static final long MAX_WAIT = 1000;

  /**
   * The time between notifications
   */

  public static final long GRANULARITY_TIME = 1;

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
    TestSuite suite = new TestSuite("Monitor Service Tests");

    suite.addTest(new TestSuite(MonitorTestCase.class));

    return suite;
  }
}
