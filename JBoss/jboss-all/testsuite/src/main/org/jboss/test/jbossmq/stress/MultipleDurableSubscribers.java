/*
 * JBoss, the OpenSource EJB server
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.test.jbossmq.stress;

import javax.jms.JMSException;

import junit.framework.Assert;

import org.jboss.test.jbossmq.MQBase;

/**
 * According to JMS spec, 6.11.1 to have several durable subscriptions under
 * one client id. We test for this here.
 *
 * @author     <a href="mailto:pra@tim.se">Peter Antman</a>
 * @version $Revision: 1.1.4.1 $
 */
public class MultipleDurableSubscribers extends MQBase
{
   class PiggyBackWorker extends TopicWorker
   {
      public PiggyBackWorker(TopicWorker worker)
      {
         super();
         connection = worker.connection;
         destination = worker.destination;
         session = worker.session;
      }
      public void connect()
      {
         log.debug("In null connect");
         // Does nothing
      }

      public void subscribe() throws JMSException
      {
         super.subscribe();
         log.debug("Message consumer set up " + consumer);
      }
   }
   public MultipleDurableSubscribers(String name)
   {
      super(name);
   }

   // This is build the same way as Durable test, to make it possible to take
   // server down in between.
   /**
    * Test setting up a durable subscription. Disconnect after half
    * the messages have been sent. Connect later to see if they are still there.
    * This test is done it two parts to be able to take down the server in
    * between
    */
   public void runDurableSubscriberPartOne() throws Exception
   {
      try
      {
         // Clean testarea up
         drainTopic();

         int ic = getIterationCount();

         // Set up a durable subscriber
         IntRangeMessageFilter f1 = new IntRangeMessageFilter(javax.jms.Message.class, "DURABLE_NR", 0, ic / 2);

         TopicWorker sub1 = new TopicWorker(SUBSCRIBER, TRANS_NONE, f1);
         sub1.setDurable("john", "needle", "sub1");
         Thread t1 = new Thread(sub1);
         t1.start();

         log.debug("Sub1 set up");
         sleep(5000L);
         TopicWorker sub2 = new PiggyBackWorker(sub1);
         sub2.setSubscriberAttrs(SUBSCRIBER, TRANS_NONE, f1);
         sub2.setDurable("john", "needle", "sub2");
         Thread t2 = new Thread(sub2);
         t2.start();
         log.debug("Sub2 setup");

         // Publish 
         IntRangeMessageCreator c1 = new IntRangeMessageCreator("DURABLE_NR", 0);
         TopicWorker pub1 = new TopicWorker(PUBLISHER, TRANS_NONE, c1, ic / 2);
         pub1.connect();
         pub1.publish();

         Assert.assertEquals(
            "Publisher did not publish correct number of messages " + pub1.getMessageHandled(),
            ic / 2,
            pub1.getMessageHandled());

         // let sub1 have some time to handle the messages.
         log.debug("Sleeping for " + ((ic * 10) / 60000) + " minutes");
         sleep(ic * 10);

         Assert.assertEquals(
            "Subscriber1 did not get correct number of messages " + sub1.getMessageHandled(),
            ic / 2,
            sub1.getMessageHandled());
         Assert.assertEquals(
            "Subscriber2 did not get correct number of messages " + sub1.getMessageHandled(),
            ic / 2,
            sub2.getMessageHandled());

         // Take down subs
         sub1.close();
         t1.stop();
         sub2.close();
         t2.stop();

         //Publish some more
         pub1.publish(ic / 2);
         Assert.assertEquals(
            "Publisher did not publish correct number of messages " + pub1.getMessageHandled(),
            ic,
            pub1.getMessageHandled());

         pub1.close();
      }
      catch (Throwable t)
      {
         log.error("Error in test: " + t, t);
         throw new Exception(t.getMessage());
      }
   }

   /**
    * Part two of durable subscriber test, part one should be run before
    * this is run.
    */
   public void runDurableSubscriberPartTwo() throws Exception
   {
      try
      {
         int ic = getIterationCount();
         // Set up a durable subscriber
         IntRangeMessageFilter f1 = new IntRangeMessageFilter(javax.jms.Message.class, "DURABLE_NR", 0, ic / 2);

         TopicWorker sub1 = new TopicWorker(SUBSCRIBER, TRANS_NONE, f1);
         sub1.setDurable("john", "needle", "sub1");

         // Start up subscription again
         Thread t1 = new Thread(sub1);
         t1.start();
         sleep(5000L);
         TopicWorker sub2 = new PiggyBackWorker(sub1);
         sub2.setSubscriberAttrs(SUBSCRIBER, TRANS_NONE, f1);
         sub2.setDurable("john", "needle", "sub2");
         Thread t2 = new Thread(sub2);
         t2.start();

         log.debug("Sleeping for " + ((ic * 10) / 60000) + " minutes");
         sleep(ic * 10);
         Assert.assertEquals(
            "Subscriber did not get correct number of messages " + sub1.getMessageHandled(),
            ic / 2,
            sub1.getMessageHandled());
         Assert.assertEquals(
            "Subscriber did not get correct number of messages " + sub1.getMessageHandled(),
            ic / 2,
            sub2.getMessageHandled());

         //OK, take everything down
         sub1.unsubscribe();
         sub2.unsubscribe();
         sub1.close();
         t1.stop();
         sub2.close();
         t2.stop();
      }
      catch (Throwable t)
      {
         log.error("Error in test: " + t, t);
         throw new Exception(t.getMessage());
      }
   }

   public void testDurableSubscriber() throws Exception
   {
      runDurableSubscriberPartOne();
      runDurableSubscriberPartTwo();
   }
   public static void main(String[] args)
   {

   }

} // MultipleDurableSubscribers
