/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package test.stress.timer;

import junit.framework.Test;
import junit.framework.TestSuite;

/**
 * Timer stress tests.
 *
 * @author <a href="mailto:AdrianBrock@HappeningTimes.com">Adrian Brock</a>.
 */
public class TimerSUITE
  extends TestSuite
{
  /**
   * The number of timers to test
   */
  public static int TIMERS = 200;

  /**
   * The start offset
   */
  public static int OFFSET = 100;

  /**
   * The period
   */
  public static int PERIOD = 500;

  /**
   * The number of notifications
   */
  public static int NOTIFICATIONS = 100;

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
    TestSuite suite = new TestSuite("Timer Stress tests");

    suite.addTest(new TestSuite(TimerTestCase.class));

    return suite;
  }
}
