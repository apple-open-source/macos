/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.test.jbossmq.test;

import javax.jms.BytesMessage;
import javax.jms.DeliveryMode;
import javax.jms.Message;
import javax.jms.MessageListener;
import javax.jms.Queue;
import javax.jms.QueueConnection;
import javax.jms.QueueConnectionFactory;
import javax.jms.QueueReceiver;
import javax.jms.QueueSender;
import javax.jms.QueueSession;
import javax.jms.Session;
import javax.jms.Topic;
import javax.jms.TopicConnection;
import javax.jms.TopicConnectionFactory;
import javax.jms.TopicPublisher;
import javax.jms.TopicSession;
import javax.jms.TopicSubscriber;
import javax.naming.Context;

import org.apache.log4j.Category;
import org.jboss.test.JBossTestCase;

/**
 * Rollback tests
 *
 * @author
 * @version
 */
public class RollBackUnitTestCase extends JBossTestCase
{

   // Provider specific
   static String TOPIC_FACTORY = "ConnectionFactory";
   static String QUEUE_FACTORY = "ConnectionFactory";

   static String TEST_QUEUE = "queue/testQueue";
   static String TEST_TOPIC = "topic/testTopic";
   static String TEST_DURABLE_TOPIC = "topic/testDurableTopic";

   static byte[] PAYLOAD = new byte[10];

   static Context context;
   static QueueConnection queueConnection;
   static TopicConnection topicConnection;
   static TopicConnection topicDurableConnection;

   /**
    * Constructor the test
    *
    * @param name           Description of Parameter
    * @exception Exception  Description of Exception
    */
   public RollBackUnitTestCase(String name) throws Exception
   {
      super(name);
   }

   /**
    * #Description of the Method
    *
    * @param persistence    Description of Parameter
    * @exception Exception  Description of Exception
    */
   public void runQueueSendRollBack(final int persistence, final boolean explicit) throws Exception
   {
      drainQueue();
      final int iterationCount = getIterationCount();
      final Category log = getLog();

      Thread sendThread = new Thread()
      {
         public void run()
         {
            try
            {
               QueueSession session = queueConnection.createQueueSession(true, Session.AUTO_ACKNOWLEDGE);
               Queue queue = (Queue) context.lookup(TEST_QUEUE);

               QueueSender sender = session.createSender(queue);

               BytesMessage message = session.createBytesMessage();
               message.writeBytes(PAYLOAD);

               for (int i = 0; i < iterationCount; i++)
               {
                  sender.send(message, persistence, 4, 0);
               }

               if (explicit)
                  session.rollback();
               session.close();
            }
            catch (Exception e)
            {
               log.error("error", e);
            }
         }
      };

      sendThread.start();
      sendThread.join();
      assertTrue("Queue should be empty", drainQueue() == 0);
   }

   /**
    * #Description of the Method
    *
    * @param persistence    Description of Parameter
    * @exception Exception  Description of Exception
    */
   public void runTopicSendRollBack(final int persistence, final boolean explicit) throws Exception
   {
      drainQueue();
      drainTopic();

      final int iterationCount = getIterationCount();
      final Category log = getLog();

      Thread sendThread = new Thread()
      {
         public void run()
         {
            try
            {

               TopicSession session = topicConnection.createTopicSession(true, Session.AUTO_ACKNOWLEDGE);
               Topic topic = (Topic) context.lookup(TEST_TOPIC);

               TopicPublisher publisher = session.createPublisher(topic);

               BytesMessage message = session.createBytesMessage();
               message.writeBytes(PAYLOAD);

               for (int i = 0; i < iterationCount; i++)
               {
                  publisher.publish(message, persistence, 4, 0);
               }

               session.close();
            }
            catch (Exception e)
            {
               log.error("error", e);
            }
         }
      };

      sendThread.start();
      sendThread.join();
      assertTrue("Topic should be empty", drainTopic() == 0);
   }

   /**
    * #Description of the Method
    *
    * @param persistence    Description of Parameter
    * @exception Exception  Description of Exception
    */
   public void runAsynchQueueReceiveRollBack(final int persistence, final boolean explicit) throws Exception
   {
      drainQueue();

      final int iterationCount = getIterationCount();
      final Category log = getLog();

      Thread sendThread = new Thread()
      {
         public void run()
         {
            try
            {
               QueueSession session = queueConnection.createQueueSession(false, Session.AUTO_ACKNOWLEDGE);
               Queue queue = (Queue) context.lookup(TEST_QUEUE);

               QueueSender sender = session.createSender(queue);

               BytesMessage message = session.createBytesMessage();
               message.writeBytes(PAYLOAD);

               for (int i = 0; i < iterationCount; i++)
               {
                  sender.send(message, persistence, 4, 0);
               }

               session.close();
            }
            catch (Exception e)
            {
               log.error("error", e);
            }
         }
      };

      QueueSession session = queueConnection.createQueueSession(true, Session.AUTO_ACKNOWLEDGE);
      Queue queue = (Queue) context.lookup(TEST_QUEUE);
      QueueReceiver receiver = session.createReceiver(queue);

      MyMessageListener listener = new MyMessageListener(iterationCount, log);

      sendThread.start();
      receiver.setMessageListener(listener);
      queueConnection.start();
      synchronized (listener)
      {
         if (listener.i < iterationCount)
            listener.wait();
      }
      receiver.setMessageListener(null);

      if (explicit)
         session.rollback();
      session.close();

      queueConnection.stop();

      sendThread.join();

      assertTrue("Queue should be full", drainQueue() == iterationCount);

   }

   /**
    * #Description of the Method
    *
    * @param persistence    Description of Parameter
    * @exception Exception  Description of Exception
    */
   public void runAsynchTopicReceiveRollBack(final int persistence, final boolean explicit) throws Exception
   {
      drainQueue();
      drainTopic();

      final int iterationCount = getIterationCount();
      final Category log = getLog();

      Thread sendThread = new Thread()
      {
         public void run()
         {
            try
            {

               TopicSession session = topicConnection.createTopicSession(false, Session.AUTO_ACKNOWLEDGE);
               Topic topic = (Topic) context.lookup(TEST_TOPIC);

               TopicPublisher publisher = session.createPublisher(topic);

               waitForSynchMessage();

               BytesMessage message = session.createBytesMessage();
               message.writeBytes(PAYLOAD);

               for (int i = 0; i < iterationCount; i++)
               {
                  publisher.publish(message, persistence, 4, 0);
                  log.debug("Published message " + i);
               }

               session.close();
            }
            catch (Exception e)
            {
               log.error("error", e);
            }
         }
      };

      TopicSession session = topicConnection.createTopicSession(true, Session.AUTO_ACKNOWLEDGE);
      Topic topic = (Topic) context.lookup(TEST_TOPIC);
      TopicSubscriber subscriber = session.createSubscriber(topic);

      MyMessageListener listener = new MyMessageListener(iterationCount, log);

      queueConnection.start();
      sendThread.start();
      subscriber.setMessageListener(listener);
      topicConnection.start();
      sendSynchMessage();
      getLog().debug("Waiting for all messages");
      synchronized (listener)
      {
         if (listener.i < iterationCount)
            listener.wait();
      }
      getLog().debug("Got all messages");
      subscriber.setMessageListener(null);

      if (explicit)
         session.rollback();
      session.close();

      sendThread.join();
      topicConnection.stop();
      queueConnection.stop();
      assertTrue("Topic should be empty", drainTopic() == 0);
   }

   /**
    * #Description of the Method
    *
    * @param persistence    Description of Parameter
    * @exception Exception  Description of Exception
    */
   public void runAsynchDurableTopicReceiveRollBack(final int persistence, final boolean explicit) throws Exception
   {
      getLog().debug("====> runAsynchDurableTopicReceiveRollBack persistence=" + persistence + " explicit=" + explicit);
      drainQueue();
      drainDurableTopic();

      final int iterationCount = getIterationCount();
      final Category log = getLog();

      Thread sendThread = new Thread()
      {
         public void run()
         {
            try
            {

               TopicSession session = topicConnection.createTopicSession(false, Session.AUTO_ACKNOWLEDGE);
               Topic topic = (Topic) context.lookup(TEST_DURABLE_TOPIC);

               TopicPublisher publisher = session.createPublisher(topic);

               waitForSynchMessage();

               BytesMessage message = session.createBytesMessage();
               message.writeBytes(PAYLOAD);

               for (int i = 0; i < iterationCount; i++)
               {
                  publisher.publish(message, persistence, 4, 0);
                  log.debug("Published message " + i);
               }

               session.close();
            }
            catch (Exception e)
            {
               log.error("error", e);
            }
         }
      };

      TopicSession session = topicDurableConnection.createTopicSession(true, Session.AUTO_ACKNOWLEDGE);
      Topic topic = (Topic) context.lookup(TEST_DURABLE_TOPIC);
      TopicSubscriber subscriber = session.createDurableSubscriber(topic, "test");

      MyMessageListener listener = new MyMessageListener(iterationCount, log);

      queueConnection.start();
      sendThread.start();
      subscriber.setMessageListener(listener);
      topicDurableConnection.start();
      sendSynchMessage();
      getLog().debug("Waiting for all messages");
      synchronized (listener)
      {
         if (listener.i < iterationCount)
            listener.wait();
      }
      getLog().debug("Got all messages");
      subscriber.setMessageListener(null);
      subscriber.close();

      if (explicit)
         session.rollback();
      session.close();

      sendThread.join();
      topicDurableConnection.stop();
      queueConnection.stop();
      assertTrue("Topic should be full", drainDurableTopic() == iterationCount);
   }

   /**
    * A unit test for JUnit
    *
    * @exception Exception  Description of Exception
    */
   public void testQueueSendRollBack() throws Exception
   {

      getLog().debug("Starting AsynchQueueSendRollBack test");

      runQueueSendRollBack(DeliveryMode.NON_PERSISTENT, false);
      runQueueSendRollBack(DeliveryMode.PERSISTENT, false);
      runQueueSendRollBack(DeliveryMode.NON_PERSISTENT, true);
      runQueueSendRollBack(DeliveryMode.PERSISTENT, true);

      getLog().debug("AsynchQueueSendRollBack passed");
   }

   /**
    * A unit test for JUnit
    *
    * @exception Exception  Description of Exception
    */
   public void testAsynchQueueReceiveBack() throws Exception
   {

      getLog().debug("Starting AsynchQueueReceiveRollBack test");

      runAsynchQueueReceiveRollBack(DeliveryMode.NON_PERSISTENT, false);
      runAsynchQueueReceiveRollBack(DeliveryMode.PERSISTENT, false);
      runQueueSendRollBack(DeliveryMode.NON_PERSISTENT, true);
      runQueueSendRollBack(DeliveryMode.PERSISTENT, true);

      getLog().debug("AsynchQueueReceiveRollBack passed");
   }

   /**
    * A unit test for JUnit
    *
    * @exception Exception  Description of Exception
    */
   public void testTopicSendRollBack() throws Exception
   {

      getLog().debug("Starting AsynchTopicSendRollBack test");

      runTopicSendRollBack(DeliveryMode.NON_PERSISTENT, false);
      runTopicSendRollBack(DeliveryMode.PERSISTENT, false);
      runTopicSendRollBack(DeliveryMode.NON_PERSISTENT, true);
      runTopicSendRollBack(DeliveryMode.PERSISTENT, true);

      getLog().debug("AsynchTopicSendRollBack passed");
   }

   /**
    * A unit test for JUnit
    *
    * @exception Exception  Description of Exception
    */
   public void testAsynchTopicReceiveRollBack() throws Exception
   {

      getLog().debug("Starting AsynchTopicReceiveRollBack test");

      runAsynchTopicReceiveRollBack(DeliveryMode.NON_PERSISTENT, false);
      runAsynchTopicReceiveRollBack(DeliveryMode.PERSISTENT, false);
      runAsynchTopicReceiveRollBack(DeliveryMode.NON_PERSISTENT, true);
      runAsynchTopicReceiveRollBack(DeliveryMode.PERSISTENT, true);

      getLog().debug("AsynchTopicReceiveRollBack passed");
   }

   /**
    * A unit test for JUnit
    *
    * @exception Exception  Description of Exception
    */
   public void testAsynchDurableTopicReceiveRollBack() throws Exception
   {

      getLog().debug("Starting AsynchDurableTopicReceiveRollBack test");

      runAsynchDurableTopicReceiveRollBack(DeliveryMode.NON_PERSISTENT, false);
      runAsynchDurableTopicReceiveRollBack(DeliveryMode.PERSISTENT, false);
      runAsynchDurableTopicReceiveRollBack(DeliveryMode.NON_PERSISTENT, true);
      runAsynchDurableTopicReceiveRollBack(DeliveryMode.PERSISTENT, true);

      getLog().debug("AsynchDurableTopicReceiveRollBack passed");
   }

   /**
    * A unit test for JUnit
    *
    * @exception Exception  Description of Exception
    */
   public void testDummyLast() throws Exception
   {

      TopicSession session = topicDurableConnection.createTopicSession(false, Session.AUTO_ACKNOWLEDGE);
      session.unsubscribe("test");

      queueConnection.close();
      topicConnection.close();
      topicDurableConnection.close();
   }

   /**
    * The JUnit setup method
    *
    * @exception Exception  Description of Exception
    */
   protected void setUp() throws Exception
   {
      getLog().debug("START TEST " + getName());
      if (context == null)
      {
         context = getInitialContext();

         QueueConnectionFactory queueFactory = (QueueConnectionFactory) context.lookup(QUEUE_FACTORY);
         queueConnection = queueFactory.createQueueConnection();

         TopicConnectionFactory topicFactory = (TopicConnectionFactory) context.lookup(TOPIC_FACTORY);
         topicConnection = topicFactory.createTopicConnection();
         topicDurableConnection = topicFactory.createTopicConnection("john", "needle");

         getLog().debug("Connection to JBossMQ established.");
      }
   }

   // Emptys out all the messages in a queue
   private int drainQueue() throws Exception
   {
      getLog().debug("Draining Queue");
      queueConnection.start();

      QueueSession session = queueConnection.createQueueSession(false, Session.AUTO_ACKNOWLEDGE);
      Queue queue = (Queue) context.lookup(TEST_QUEUE);

      QueueReceiver receiver = session.createReceiver(queue);
      Message message = receiver.receive(50);
      int c = 0;
      while (message != null)
      {
         c++;
         message = receiver.receive(50);
      }

      getLog().debug("  Drained " + c + " messages from the queue");

      session.close();

      queueConnection.stop();

      return c;
   }

   // Emptys out all the messages in a topic
   private int drainTopic() throws Exception
   {
      getLog().debug("Draining Topic");
      topicConnection.start();

      final TopicSession session = topicConnection.createTopicSession(false, Session.AUTO_ACKNOWLEDGE);
      Topic topic = (Topic) context.lookup(TEST_TOPIC);
      TopicSubscriber subscriber = session.createSubscriber(topic);

      Message message = subscriber.receive(50);
      int c = 0;
      while (message != null)
      {
         c++;
         message = subscriber.receive(50);
      }

      getLog().debug("  Drained " + c + " messages from the topic");

      session.close();

      topicConnection.stop();

      return c;
   }

   // Emptys out all the messages in a durable topic
   private int drainDurableTopic() throws Exception
   {
      getLog().debug("Draining Durable Topic");
      topicDurableConnection.start();

      final TopicSession session = topicDurableConnection.createTopicSession(false, Session.AUTO_ACKNOWLEDGE);
      Topic topic = (Topic) context.lookup(TEST_DURABLE_TOPIC);
      TopicSubscriber subscriber = session.createDurableSubscriber(topic, "test");

      Message message = subscriber.receive(50);
      int c = 0;
      while (message != null)
      {
         c++;
         message = subscriber.receive(50);
      }

      getLog().debug("  Drained " + c + " messages from the durable topic");

      session.close();

      topicDurableConnection.stop();

      return c;
   }

   private void waitForSynchMessage() throws Exception
   {
      getLog().debug("Waiting for Synch Message");
      QueueSession session = queueConnection.createQueueSession(false, Session.AUTO_ACKNOWLEDGE);
      Queue queue = (Queue) context.lookup(TEST_QUEUE);

      QueueReceiver receiver = session.createReceiver(queue);
      receiver.receive();
      session.close();
      getLog().debug("Got Synch Message");
   }

   private void sendSynchMessage() throws Exception
   {
      getLog().debug("Sending Synch Message");
      QueueSession session = queueConnection.createQueueSession(false, Session.AUTO_ACKNOWLEDGE);
      Queue queue = (Queue) context.lookup(TEST_QUEUE);

      QueueSender sender = session.createSender(queue);

      Message message = session.createMessage();
      sender.send(message);

      session.close();
      getLog().debug("Sent Synch Message");
   }

   public class MyMessageListener implements MessageListener
   {
      public int i = 0;

      public int iterationCount;

      public Category log;

      public MyMessageListener(int iterationCount, Category log)
      {
         this.iterationCount = iterationCount;
         this.log = log;
      }

      public void onMessage(Message message)
      {
         synchronized (this)
         {
            i++;
            log.debug("Got message " + i);
            if (i >= iterationCount)
               this.notify();
         }
      }
   }

   // Workarounds for java 1.3 bugs
   public Category getLog()
   {
      return super.getLog();
   }
   public int getIterationCount()
   {
      return 5;
   }
}
