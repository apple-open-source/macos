/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package test.performance.registration;

import junit.framework.Test;
import junit.framework.TestSuite;

import test.performance.registration.support.Standard;

/**
 * Registration performance tests.
 *
 * @author <a href="mailto:AdrianBrock@HappeningTimes.com">Adrian Brock</a>.
 */
public class RegistrationSUITE
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
    TestSuite suite = new TestSuite("Registation Performance tests");

    suite.addTest(new RegistrationTEST("testIt", new Standard(), "test:test=test", "Standard"));

    return suite;
  }
}
