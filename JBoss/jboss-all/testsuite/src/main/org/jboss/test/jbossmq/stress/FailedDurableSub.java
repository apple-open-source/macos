/*
 * JBoss, the OpenSource EJB server
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.test.jbossmq.stress;

import junit.framework.TestSuite;
/**
 *
 *
 * @author     <a href="mailto:pra@tim.se">Peter Antman</a>
 * @version $Revision: 1.1.4.1 $
 */

public class FailedDurableSub extends DurableSubscriberTest
{

   public FailedDurableSub(String name)
   {
      super(name);
   }
   /**
    * Do a durable subscription and fail to disconnect. Should be run
    * in a separate VM from test, testing if a new connection is possible!
    *
    * This did not work before in JBossMQ, but seems to work fine now.
    */
   public static junit.framework.Test suite() throws Exception
   {

      TestSuite suite = new TestSuite();
      suite.addTest(new FailedDurableSub("runBadClient"));

      //suite.addTest(new DurableSubscriberTest("testBadClient"));
      return suite;
   }
   public static void main(String[] args)
   {

   }

} // FailedDurableSub
