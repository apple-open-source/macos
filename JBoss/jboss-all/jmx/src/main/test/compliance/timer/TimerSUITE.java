/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package test.compliance.timer;

import junit.framework.Test;
import junit.framework.TestSuite;

/**
 * Tests for the timer service.
 *
 * @author <a href="mailto:AdrianBrock@HappeningTimes.com">Adrian Brock</a>.
 */
public class TimerSUITE
  extends TestSuite
{
  /**
   * The maximum wait for a notification
   */
  public static final long MAX_WAIT = 10000;

  /**
   * The time for a repeated notification
   */
  public static final long REPEAT_TIME = 500;

  /**
   * The immediate registration time, allow for processing
   */
  public static final long ZERO_TIME = 100;

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
    TestSuite suite = new TestSuite("Timer Service Tests");

    suite.addTest(new TestSuite(TimerNotificationTestCase.class));
    suite.addTest(new TestSuite(TimerTest.class));

    return suite;
  }
}
