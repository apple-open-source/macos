/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package test.implementation.persistence;

import junit.framework.Test;
import junit.framework.TestSuite;

public class PersistenceSUITE extends TestSuite
{
   public static void main(String[] args)
   {
      junit.textui.TestRunner.run(suite());
   }

   public static Test suite()
   {
      TestSuite suite = new TestSuite("JBossMX Persistence Interceptor and Persistence Manager Tests");

      suite.addTest(new TestSuite(OnTimerPersistenceTEST.class));
      
      return suite;
   }

}
