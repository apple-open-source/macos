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
 * Durable subscriber tests.
 *
 * @author     <a href="mailto:pra@tim.se">Peter Antman</a>
 * @version $Revision: 1.1 $
 */

public class DurableSubscriberTest extends MQBase {
   
   public DurableSubscriberTest(String name) {
      super(name);
   }

   /**
    * Test setting up a durable subscription. Disconnect after half
    * the messages have been sent. Connect later to see if they are still there.
    * This test is done it two parts to be able to take down the server in
    * between
    */
   public void runDurableSubscriberPartOne() throws Exception {
      try {
         // Clean testarea up
         drainTopic();

         int ic = getIterationCount();
      
         // Set up a durable subscriber
         IntRangeMessageFilter f1 = new IntRangeMessageFilter(javax.jms.Message.class,
                                                              "DURABLE_NR",
                                                              0,
                                                              ic/2);
                                               
         TopicWorker sub1 = new TopicWorker(SUBSCRIBER,
                                            TRANS_NONE,
                                            f1);
         sub1.setDurable("john", "needle", "sub2");
         Thread t1 = new Thread(sub1);
         t1.start();

         // Publish 
         IntRangeMessageCreator c1 = new IntRangeMessageCreator("DURABLE_NR",
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
      
         // let sub1 have some time to handle the messages.
         log.debug("Sleeping for " + ((ic*10)/60000) + " minutes");
         sleep(ic*10);

      
         Assert.assertEquals("Subscriber did not get correct number of messages "+sub1.getMessageHandled(),
                             ic/2,
                             sub1.getMessageHandled());
      
         // Take down first sub
         sub1.close();
         t1.stop();
      
         //Publish some more
         pub1.publish(ic/2);
         Assert.assertEquals("Publisher did not publish correct number of messages "+pub1.getMessageHandled(),ic,
                             pub1.getMessageHandled());
      
         pub1.close();
      }catch(Throwable t) {
         log.error("Error in test: " +t,t);
         throw new Exception(t.getMessage());
      }
   }

   /**
    * Part two of durable subscriber test, part one should be run before
    * this is run.
    */
   public void runDurableSubscriberPartTwo() throws Exception {
      try {
         int ic = getIterationCount();
         // Set up a durable subscriber
         IntRangeMessageFilter f1 = new IntRangeMessageFilter(javax.jms.Message.class,
                                                              "DURABLE_NR",
                                                              0,
                                                              ic/2);
                                               
         TopicWorker sub1 = new TopicWorker(SUBSCRIBER,
                                            TRANS_NONE,
                                            f1);
         sub1.setDurable("john", "needle", "sub2");

         // Start up subscription again
         Thread t2 = new Thread(sub1);
         t2.start();
      
         log.debug("Sleeping for " + ((ic*10)/60000) + " minutes");
         sleep(ic*10);
         Assert.assertEquals("Subscriber did not get correct number of messages "+sub1.getMessageHandled(), ic/2,
                             sub1.getMessageHandled());

         //OK, take everything down
         sub1.unsubscribe();
         sub1.close();
         t2.stop();
   
      }catch(Throwable t) {
         log.error("Error in test: " +t,t);
         throw new Exception(t.getMessage());
      }
   }

   public void testDurableSubscriber() throws Exception {
      runDurableSubscriberPartOne();
      runDurableSubscriberPartTwo();
   }

   public void runGoodClient() throws Exception {
      TopicWorker sub1 = new TopicWorker(CONNECTOR,
                                         TRANS_NONE,
                                         null);
      sub1.setDurable("john", "needle", "sub2");
      Thread t1 = new Thread(sub1);
      t1.start();
      try{ Thread.sleep(2000); }catch(InterruptedException e){}
      // Take it down abruptly
      t1.stop();
      sub1.close();
      Assert.assertNull("Error in connecting durable sub",sub1.getException());
      
   }
   /**
    * Test connecting as a durable subscriber and diconecction without taking
    * the connection down properly
    */
   public void runBadClient() throws Exception {
      TopicWorker sub1 = new TopicWorker(CONNECTOR,
                                         TRANS_NONE,
                                         null);
      sub1.setDurable("john", "needle", "sub2");
      Thread t1 = new Thread(sub1);
      t1.start();
      try{ Thread.sleep(2000); }catch(InterruptedException e){}
      // Take it down abruptly
      t1.stop();
      //sub1.close();
      Assert.assertNull("Error in connecting durable sub",sub1.getException());
   }

   public static junit.framework.Test suite() throws Exception{
      
      TestSuite suite= new TestSuite();
      suite.addTest(new DurableSubscriberTest("runGoodClient"));
      suite.addTest(new DurableSubscriberTest("testDurableSubscriber"));
      
      //suite.addTest(new DurableSubscriberTest("testBadClient"));
      return suite;
   }
   public static void main(String[] args) {
      
   }
   
} // DurableSubscriberTest
