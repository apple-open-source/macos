/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
 
package test.stress;

import junit.framework.Test;
import junit.framework.TestSuite;

/**
 * Stress tests
 *
 * @author  <a href="mailto:Adrian.Brock@HappeningTimes.com">Adrian Brock</a>.
 */

public class StressSUITE extends TestSuite
{

   public static void main(String[] args)
   {
      junit.textui.TestRunner.run(suite());
   }

   public static Test suite()
   {
      TestSuite suite = new TestSuite("All Stress Tests");

      suite.addTest(test.stress.timer.TimerSUITE.suite());
      
      return suite;
   }
}
