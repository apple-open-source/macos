/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
 
package test.performance;

import junit.framework.Test;
import junit.framework.TestSuite;

/**
 *
 * @author  <a href="mailto:juha@jboss.org">Juha Lindfors</a>.
 */

public class PerformanceSUITE extends TestSuite
{
   
   public final static int SECOND          = 1000;
   public final static int THROUGHPUT_TIME = 3 * SECOND;
   
   public final static int ITERATION_COUNT = 100000;
   public final static int REGISTRATION_ITERATION_COUNT = 1000;
   public final static int REPEAT_COUNT = 10;
   public final static int SERIALIZE_ITERATION_COUNT = 1000;
   public final static int TIMER_ITERATION_COUNT = 2000;

   public static void main(String[] args)
   {
      junit.textui.TestRunner.run(suite());
   }

   public static Test suite()
   {
      TestSuite suite = new TestSuite("All Performance Tests");

      suite.addTest(test.performance.dynamic.DynamicSUITE.suite());
      suite.addTest(test.performance.standard.StandardSUITE.suite());
      suite.addTest(test.performance.serialize.SerializeSUITE.suite());
      suite.addTest(test.performance.registration.RegistrationSUITE.suite());
      suite.addTest(test.performance.timer.TimerSUITE.suite());
      
      return suite;
   }
}
