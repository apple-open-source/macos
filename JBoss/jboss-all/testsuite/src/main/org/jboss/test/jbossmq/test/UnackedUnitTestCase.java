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
public class UnackedUnitTestCase extends JBossTestCase
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
   public UnackedUnitTestCase(String name) throws Exception
   {
      super(name);
   }

   /**
    * #Description of the Method
    *
    * @param persistence    Description of Parameter
    * @exception Exception  Description of Exception
    */
   public void runUnackedQueue(final int persistence) throws Exception
   {
      drainQueue();

      final int iterationCount = getIterationCount();

      QueueSession session = queueConnection.createQueueSession(false, Session.AUTO_ACKNOWLEDGE);
      Queue queue = (Queue)context.lookup(TEST_QUEUE);

      QueueSender sender = session.createSender(queue);

      Message message = session.createBytesMessage();
      ((BytesMessage)message).writeBytes(PAYLOAD);

      for (int i = 0; i < iterationCount; i++)
         sender.send(message, persistence, 4, 0);

      session.close();

      session = queueConnection.createQueueSession(false, Session.CLIENT_ACKNOWLEDGE);
      queue = (Queue)context.lookup(TEST_QUEUE);
      QueueReceiver receiver = session.createReceiver(queue);
      queueConnection.start();
      message = receiver.receive(50);
      int c = 0;
      while (message != null)
      {
         message = receiver.receive(50);
         c++;
      }
      assertTrue("Should have received all data unacked", c == iterationCount);

      queueConnection.close();
      QueueConnectionFactory queueFactory = (QueueConnectionFactory)context.lookup(QUEUE_FACTORY);
      queueConnection = queueFactory.createQueueConnection();

      assertTrue("Queue should be full", drainQueue() == iterationCount);

   }

   /**
    * #Description of the Method
    *
    * @param persistence    Description of Parameter
    * @exception Exception  Description of Exception
    */
   public void runUnackedMultipleSession(final int persistence) throws Exception
   {
      drainQueue();

      final int iterationCount = getIterationCount();

      QueueSession session = queueConnection.createQueueSession(false, Session.AUTO_ACKNOWLEDGE);
      Queue queue = (Queue)context.lookup(TEST_QUEUE);

      QueueSender sender = session.createSender(queue);

      Message message = session.createBytesMessage();
      ((BytesMessage)message).writeBytes(PAYLOAD);

      for (int i = 0; i < iterationCount; i++)
         sender.send(message, persistence, 4, 0);

      session.close();

      QueueSession session1 = queueConnection.createQueueSession(false, Session.CLIENT_ACKNOWLEDGE);
      queue = (Queue)context.lookup(TEST_QUEUE);
      QueueReceiver receiver1 = session1.createReceiver(queue);
      QueueSession session2 = queueConnection.createQueueSession(false, Session.CLIENT_ACKNOWLEDGE);
      QueueReceiver receiver2 = session2.createReceiver(queue);
      queueConnection.start();

      // Read half from session1
      int c = 0;
      for (int l = 0; l < iterationCount/2; l++)
      {
         message = receiver1.receive(50);
         if (message != null)
            c++;
      }
      assertTrue("Should have received half data unacked", c == iterationCount/2);

      // Read the rest from session2
      c = 0;
      Message lastMessage = null;
      while (message != null)
      {
         message = receiver2.receive(50);
         if (message != null)
         {
            c++;
            lastMessage = message;
         }
      }
      assertTrue("Should have received all data unacked", c == iterationCount - iterationCount/2);

      // Close session1, the messages are unacked and should go back in the queue
      session1.close();

      // Acknowledge messages on session2 and close it
      lastMessage.acknowledge();
      session2.close();

      queueConnection.stop();

      assertTrue("Session1 messages should be available", drainQueue() == iterationCount/2);

   }

   /**
    * #Description of the Method
    *
    * @param persistence    Description of Parameter
    * @exception Exception  Description of Exception
    */
   public void runUnackedMultipleConnection(final int persistence) throws Exception
   {
      drainQueue();

      final int iterationCount = getIterationCount();

      QueueSession session = queueConnection.createQueueSession(false, Session.AUTO_ACKNOWLEDGE);
      Queue queue = (Queue)context.lookup(TEST_QUEUE);

      QueueSender sender = session.createSender(queue);

      Message message = session.createBytesMessage();
      ((BytesMessage)message).writeBytes(PAYLOAD);

      for (int i = 0; i < iterationCount; i++)
         sender.send(message, persistence, 4, 0);

      session.close();

      QueueConnectionFactory queueFactory = (QueueConnectionFactory)context.lookup(QUEUE_FACTORY);
      QueueConnection queueConnection1 = queueFactory.createQueueConnection();
      QueueSession session1 = queueConnection1.createQueueSession(false, Session.CLIENT_ACKNOWLEDGE);
      queue = (Queue)context.lookup(TEST_QUEUE);
      QueueReceiver receiver1 = session1.createReceiver(queue);

      QueueConnection queueConnection2 = queueFactory.createQueueConnection();
      QueueSession session2 = queueConnection2.createQueueSession(false, Session.CLIENT_ACKNOWLEDGE);
      QueueReceiver receiver2 = session2.createReceiver(queue);

      queueConnection1.start();
      queueConnection2.start();

      // Read half from session1
      int c = 0;
      for (int l = 0; l < iterationCount/2; l++)
      {
         message = receiver1.receive(50);
         if (message != null)
            c++;
      }
      assertTrue("Should have received half data unacked", c == iterationCount/2);

      // Read the rest from session2
      Message lastMessage = null;
      c = 0;
      while (message != null)
      {
         message = receiver2.receive(50);
         if (message != null)
         {
            c++;
            lastMessage = message;
         }
      }
      assertTrue("Should have received all data unacked", c == iterationCount - iterationCount/2);

      // Close session1, the messages are unacked and should go back in the queue
      queueConnection1.close();

      // Acknowledge messages for connection 2 and close it
      lastMessage.acknowledge();
      queueConnection2.close();

      assertTrue("Connection1 messages should be available", drainQueue() == iterationCount/2);

   }

   /**
    * #Description of the Method
    *
    * @param persistence    Description of Parameter
    * @exception Exception  Description of Exception
    */
   public void runUnackedTopic(final int persistence) throws Exception
   {
      drainQueue();
      drainTopic();

      final int iterationCount = getIterationCount();
      final Category log = getLog();

      Thread sendThread =
         new Thread()
         {
            public void run()
            {
               try
               {

                  TopicSession session = topicConnection.createTopicSession(false, Session.AUTO_ACKNOWLEDGE);
                  Topic topic = (Topic)context.lookup(TEST_TOPIC);

                  TopicPublisher publisher = session.createPublisher(topic);

                  waitForSynchMessage();

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

      TopicSession session = topicConnection.createTopicSession(false, Session.CLIENT_ACKNOWLEDGE);
      Topic topic = (Topic)context.lookup(TEST_TOPIC);
      TopicSubscriber subscriber = session.createSubscriber(topic);


      MyMessageListener listener = new MyMessageListener(iterationCount, log);

      queueConnection.start();
      sendThread.start();
      subscriber.setMessageListener(listener);
      topicConnection.start();
      sendSynchMessage();
      synchronized (listener)
      {
         if (listener.i < iterationCount)
            listener.wait();
      }
      sendThread.join();
      topicConnection.close();
      TopicConnectionFactory topicFactory = (TopicConnectionFactory)context.lookup(TOPIC_FACTORY);
      topicConnection = topicFactory.createTopicConnection();
      queueConnection.stop();
      assertTrue("Topic should be empty", drainTopic() == 0);
   }

   /**
    * #Description of the Method
    *
    * @param persistence    Description of Parameter
    * @exception Exception  Description of Exception
    */
   public void runUnackedDurableTopic(final int persistence) throws Exception
   {
      drainQueue();
      drainDurableTopic();

      final int iterationCount = getIterationCount();
      final Category log = getLog();

      Thread sendThread =
         new Thread()
         {
            public void run()
            {
               try
               {

                  TopicSession session = topicConnection.createTopicSession(false, Session.AUTO_ACKNOWLEDGE);
                  Topic topic = (Topic)context.lookup(TEST_DURABLE_TOPIC);

                  TopicPublisher publisher = session.createPublisher(topic);

                  waitForSynchMessage();

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

      TopicSession session = topicDurableConnection.createTopicSession(false, Session.CLIENT_ACKNOWLEDGE);
      Topic topic = (Topic)context.lookup(TEST_DURABLE_TOPIC);
      TopicSubscriber subscriber = session.createDurableSubscriber(topic, "test");

      MyMessageListener listener = new MyMessageListener(iterationCount, log);

      queueConnection.start();
      sendThread.start();
      subscriber.setMessageListener(listener);
      topicDurableConnection.start();
      sendSynchMessage();
      synchronized (listener)
      {
         if (listener.i < iterationCount)
            listener.wait();
      }

      sendThread.join();
      topicDurableConnection.close();
      TopicConnectionFactory topicFactory = (TopicConnectionFactory)context.lookup(TOPIC_FACTORY);
      topicDurableConnection = topicFactory.createTopicConnection("john", "needle");
      queueConnection.stop();
      assertTrue("Topic should be full", drainDurableTopic() == iterationCount);
   }

   /**
    * A unit test for JUnit
    *
    * @exception Exception  Description of Exception
    */
   public void testUnackedQueue() throws Exception
   {

      getLog().debug("Starting UnackedQueue test");

      runUnackedQueue(DeliveryMode.NON_PERSISTENT);
      runUnackedQueue(DeliveryMode.PERSISTENT);

      getLog().debug("UnackedQueue passed");
   }

   /**
    * A unit test for JUnit
    *
    * @exception Exception  Description of Exception
    */
   public void testUnackedMultipleSession() throws Exception
   {

      getLog().debug("Starting UnackedMultipleSession test");

      runUnackedMultipleSession(DeliveryMode.NON_PERSISTENT);
      runUnackedMultipleSession(DeliveryMode.PERSISTENT);

      getLog().debug("UnackedMultipleSession passed");
   }

   /**
    * A unit test for JUnit
    *
    * @exception Exception  Description of Exception
    */
   public void testUnackedMultipleConnection() throws Exception
   {

      getLog().debug("Starting UnackedMultipleConnection test");

      runUnackedMultipleConnection(DeliveryMode.NON_PERSISTENT);
      runUnackedMultipleConnection(DeliveryMode.PERSISTENT);

      getLog().debug("UnackedMultipleConnection passed");
   }

   /**
    * A unit test for JUnit
    *
    * @exception Exception  Description of Exception
    */
   public void testUnackedTopic() throws Exception
   {

      getLog().debug("Starting UnackedTopic test");

      runUnackedTopic(DeliveryMode.NON_PERSISTENT);
      runUnackedTopic(DeliveryMode.PERSISTENT);

      getLog().debug("UnackedTopic passed");
   }

   /**
    * A unit test for JUnit
    *
    * @exception Exception  Description of Exception
    */
   public void testUnackedDurableTopic() throws Exception
   {

      getLog().debug("Starting UnackedDurableTopic test");

      runUnackedDurableTopic(DeliveryMode.NON_PERSISTENT);
      runUnackedDurableTopic(DeliveryMode.PERSISTENT);

      getLog().debug("UnackedDurableTopic passed");
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
      if (context == null)
      {
         context = getInitialContext();

         QueueConnectionFactory queueFactory = (QueueConnectionFactory)context.lookup(QUEUE_FACTORY);
         queueConnection = queueFactory.createQueueConnection();

         TopicConnectionFactory topicFactory = (TopicConnectionFactory)context.lookup(TOPIC_FACTORY);
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
      Queue queue = (Queue)context.lookup(TEST_QUEUE);

      QueueReceiver receiver = session.createReceiver(queue);
      Message message = receiver.receive(50);
      int c = 0;
      while (message != null)
      {
         message = receiver.receive(50);
         c++;
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
      Topic topic = (Topic)context.lookup(TEST_TOPIC);
      TopicSubscriber subscriber = session.createSubscriber(topic);

      Message message = subscriber.receive(50);
      int c = 0;
      while (message != null)
      {
         message = subscriber.receive(50);
         c++;
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
      Topic topic = (Topic)context.lookup(TEST_DURABLE_TOPIC);
      TopicSubscriber subscriber = session.createDurableSubscriber(topic, "test");

      Message message = subscriber.receive(50);
      int c = 0;
      while (message != null)
      {
         message = subscriber.receive(50);
         c++;
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
      Queue queue = (Queue)context.lookup(TEST_QUEUE);

      QueueReceiver receiver = session.createReceiver(queue);
      receiver.receive();
      session.close();
      getLog().debug("Got Synch Message");
   }

   private void sendSynchMessage() throws Exception
   {
      getLog().debug("Sending Synch Message");
      QueueSession session = queueConnection.createQueueSession(false, Session.AUTO_ACKNOWLEDGE);
      Queue queue = (Queue)context.lookup(TEST_QUEUE);

      QueueSender sender = session.createSender(queue);

      Message message = session.createMessage();
      sender.send(message);

      session.close();
      getLog().debug("Sent Synch Message");
   }

   public class MyMessageListener
      implements MessageListener
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
