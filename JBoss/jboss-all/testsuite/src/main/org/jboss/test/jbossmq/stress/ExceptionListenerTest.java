/*
 * JBoss, the OpenSource EJB server
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.test.jbossmq.stress;

import junit.framework.TestSuite;
import junit.framework.Assert;

import org.jboss.test.jbossmq.MQBase;
/**
 * Exception listener tests-
 *
 *
 * @author     <a href="mailto:pra@tim.se">Peter Antman</a>
 * @version $Revision: 1.1 $
 */

public class ExceptionListenerTest  extends MQBase{
   
   public ExceptionListenerTest(String name) {
      super(name);
   }
   /*
     To catch the old typ of error, we need a consumer that has not consumed
     anything before the server goes down.

     To govern this we need a publisher method to be used in the tests.

     Big question is: when are we satisfyed with the listeners result???
    */


   public void runListener() throws Exception {
      // Clean testarea up
      drainTopic();

      int ic = getIterationCount();
      long sleep = getRunSleep();
      IntRangeMessageFilter f1 = new IntRangeMessageFilter(javax.jms.Message.class,
                                                           "FAILSAFE_NR",
                                                           0,
                                                           ic);
      
      TopicWorker sub1 = new TopicWorker(FAILSAFE_SUBSCRIBER,
                                         TRANS_NONE,
                                         f1);
      Thread t1 = new Thread(sub1);
      t1.start();
      
      // Now we must wait until JBoss has been restarted before we check
      // messages
      sleep(5*sleep);
      log.info("Awakened from sleep");
      
      Assert.assertEquals("Subscriber did not get correct number of messages "+sub1.getMessageHandled(), ic,
                          sub1.getMessageHandled());
      
      sub1.setStoped();
      t1.interrupt();
      t1.stop();
      sub1.close();

   }

   public void runPublish() throws Exception {
      int ic = getIterationCount();
      // This does NOT work perfect, since both sends will have base 0
      IntRangeMessageCreator c1 = new IntRangeMessageCreator("FAILSAFE_NR",
                                                             0);
      TopicWorker pub1 = new TopicWorker(PUBLISHER,
                                         TRANS_NONE,
                                         c1,
                                         ic/2);
      pub1.connect();
      pub1.publish();
      
      Assert.assertEquals("Publisher did not publish correct number of messages "+pub1.getMessageHandled(),
                          ic/2,
                          pub1.getMessageHandled());
      
      
      pub1.close();
   }
   public static junit.framework.Test suite() throws Exception{
      
      TestSuite suite= new TestSuite();
      suite.addTest(new  ExceptionListenerTest("runListener"));
      
      //suite.addTest(new DurableSubscriberTest("testBadClient"));
      return suite;
   }
   
   public static void main(String[] args) {
      
   }
   
} // ExceptionListenerTest
