/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.test.jbossmx.performance;

import org.jboss.test.JBossTestCase;

public class TestCase
   extends JBossTestCase
{
   public final static int ITERATION_COUNT = 100000;
   public final static int REPEAT_COUNT = 10;

   public TestCase(String s)
   {
      super(s);
   }

   /**
    * We do not need the JBoss Server for performance tests
    */
   public void testServerFound()
   {
   }
}
