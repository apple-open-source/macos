/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package test.compliance;

import junit.framework.Test;
import junit.framework.TestSuite;

/**
 * This suite includes the timer and monitoring tests that take
 * a while to run.
 *
 * @author  <a href="mailto:Adrian.Brock@HappeningTimes.com">Adrian Brock</a>.
 */

public class FullComplianceSUITE extends TestSuite
{
   public static void main(String[] args)
   {
      junit.textui.TestRunner.run(suite());
   }

   public static Test suite()
   {
      TestSuite suite = new TestSuite("All Compliance Tests");

      suite.addTest(test.compliance.ComplianceSUITE.suite());
      suite.addTest(test.compliance.monitor.MonitorSUITE.suite());
      suite.addTest(test.compliance.timer.TimerSUITE.suite());
      
      return suite;
   }
}
