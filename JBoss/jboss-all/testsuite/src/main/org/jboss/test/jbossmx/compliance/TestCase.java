/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.test.jbossmx.compliance;

import org.jboss.test.JBossTestCase;

public class TestCase
   extends JBossTestCase
{

   /**
    * The period for a timer notification. This needs to be small so the tests
    * don't take too long.
    * The wait needs to be long enough to be sure the monitor has enough time
    * to send the notification and switch the context to the handler.
    */
   public static final long PERIOD = 100;
   public static final long WAIT = PERIOD * 2;

   /**
    * The number of repeats for occurances tests
    */
   public static final long REPEATS = 2;

   /**
    * The name of the MBeanServerDelegate from the spec
    */
   public static final String MBEAN_SERVER_DELEGATE = "JMImplementation:type=MBeanServerDelegate";

   public TestCase(String s)
   {
      super(s);
   }

   /**
    * We do not need the JBoss Server for compliance tests
    */
   public void testServerFound()
   {
   }
}
