/*
 * Copyright (c) 2000 Hiram Chirino <Cojonudo14@hotmail.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
package org.jboss.test.jbossmq.test;

import java.util.Enumeration;

import javax.jms.DeliveryMode;
import javax.jms.InvalidDestinationException;
import javax.jms.JMSException;
import javax.jms.Message;
import javax.jms.MessageListener;
import javax.jms.Queue;
import javax.jms.QueueBrowser;
import javax.jms.QueueConnection;
import javax.jms.QueueConnectionFactory;
import javax.jms.QueueReceiver;
import javax.jms.QueueRequestor;
import javax.jms.QueueSender;
import javax.jms.QueueSession;
import javax.jms.ServerSession;
import javax.jms.ServerSessionPool;
import javax.jms.Session;
import javax.jms.TemporaryQueue;
import javax.jms.TemporaryTopic;
import javax.jms.TextMessage;
import javax.jms.Topic;
import javax.jms.TopicConnection;
import javax.jms.TopicConnectionFactory;
import javax.jms.TopicPublisher;
import javax.jms.TopicSession;
import javax.jms.TopicSubscriber;
import javax.naming.Context;
import javax.naming.InitialContext;

import org.apache.log4j.Category;
import org.jboss.test.JBossTestCase;

import EDU.oswego.cs.dl.util.concurrent.CountDown;

/**
 * JBossMQUnitTestCase.java
 *
 * Some simple tests of spyderMQ
 *
 * @author
 * @version
 */
public class JBossMQUnitTest extends JBossTestCase
{
   // Provider specific
   static String TOPIC_FACTORY; // = "ConnectionFactory";
   static String QUEUE_FACTORY; // = "ConnectionFactory";

   static String TEST_QUEUE = "queue/testQueue";
   static String TEST_TOPIC = "topic/testTopic";
   static String TEST_DURABLE_TOPIC = "topic/testDurableTopic";

   //JMSProviderAdapter providerAdapter;
   static Context context;
   static QueueConnection queueConnection;
   static TopicConnection topicConnection;

   public JBossMQUnitTest(String name) throws Exception
   {
      super(name);
   }

   // Emptys out all the messages in a queue
   protected void drainQueue() throws Exception
   {
      QueueSession session = queueConnection.createQueueSession(false, Session.AUTO_ACKNOWLEDGE);
      Queue queue = (Queue) context.lookup(TEST_QUEUE);

      QueueReceiver receiver = session.createReceiver(queue);
      Message message = receiver.receive(50);
      int c = 0;
      while (message != null)
      {
         message = receiver.receive(50);
         c++;
      }

      if (c != 0)
         getLog().debug("  Drained " + c + " messages from the queue");

      session.close();
   }

   protected void connect() throws Exception
   {

      if (context == null)
      {

         context = new InitialContext();

      }
      QueueConnectionFactory queueFactory = (QueueConnectionFactory) context.lookup(QUEUE_FACTORY);
      queueConnection = queueFactory.createQueueConnection();

      TopicConnectionFactory topicFactory = (TopicConnectionFactory) context.lookup(TOPIC_FACTORY);
      topicConnection = topicFactory.createTopicConnection();

      getLog().debug("Connection to spyderMQ established.");

   }

   protected void disconnect() throws Exception
   {
      queueConnection.close();
      topicConnection.close();
   }

   /**
    * Test that messages are ordered by message arrival and priority.
    * This also tests :
    * 		Using a non-transacted AUTO_ACKNOWLEDGE session
    *		Using a QueueReceiver
    *		Using a QueueSender
    *			Sending PERSITENT and NON_PERSISTENT text messages.
    *		Using a QueueBrowser
    */
   public void testQueueMessageOrder() throws Exception
   {

      getLog().debug("Starting QueueMessageOrder test");

      connect();

      queueConnection.start();

      drainQueue();

      QueueSession session = queueConnection.createQueueSession(false, Session.AUTO_ACKNOWLEDGE);
      Queue queue = (Queue) context.lookup(TEST_QUEUE);
      QueueSender sender = session.createSender(queue);

      TextMessage message = session.createTextMessage();
      message.setText("Normal message");
      sender.send(message, DeliveryMode.NON_PERSISTENT, 4, 0);
      //sender.send(queue, message, DeliveryMode.NON_PERSISTENT, 4, 0);
      message.setText("Persistent message");
      sender.send(message, DeliveryMode.PERSISTENT, 4, 0);
      //sender.send(queue, message, DeliveryMode.PERSISTENT, 4, 0);
      message.setText("High Priority Persistent message");
      sender.send(message, DeliveryMode.PERSISTENT, 10, 0);
      //sender.send(queue, message, DeliveryMode.PERSISTENT, 10, 0);

      //message.setText("Expiring Persistent message");
      //sender.send(queue, message, DeliveryMode.NON_PERSISTENT, 4, 1);

      QueueBrowser browser = session.createBrowser(queue);
      Enumeration enum = browser.getEnumeration();
      //message = (TextMessage)enum.nextElement();
      //if( !message.getText().equals("High Priority Persistent message") )
      //	throw new Exception("Queue is not prioritizing messages correctly. Unexpected Message:"+message);
      getLog().debug(message.getText());

      message = (TextMessage) enum.nextElement();
      //if( !message.getText().equals("Normal message") )
      //	throw new Exception("Queue is not ordering messages correctly. Unexpected Message:"+message);
      getLog().debug(message.getText());

      message = (TextMessage) enum.nextElement();
      //if( !message.getText().equals("Persistent message") )
      //	throw new Exception("Queue is not ordering messages correctly. Unexpected Message:"+message);
      getLog().debug(message.getText());

      // if( enum.hasMoreElements() )
      //	throw new Exception("Queue does not expire messages correctly. Unexpected Message:"+enum.nextElement());

      disconnect();
      getLog().debug("QueueMessageOrder passed");
   }

   /**
    * Test that a using QueueRequestor works.
    * this also tests that :
    *		temporary queues work.
    */
   public void testRequestReplyQueue() throws Exception
   {

      getLog().debug("Starting RequestReplyQueue test");
      connect();

      {
         queueConnection.start();
         drainQueue();
      }

      Thread serverThread = new Thread()
      {
         public void run()
         {
            Category log = Category.getInstance(getClass().getName());
            try
            {
               log.debug("Server Thread Started");
               QueueSession session = queueConnection.createQueueSession(false, Session.AUTO_ACKNOWLEDGE);
               Queue queue = (Queue) context.lookup(TEST_QUEUE);

               QueueReceiver queueReceiver = session.createReceiver(queue);

               boolean done = false;
               while (!done)
               {
                  TextMessage message = (TextMessage) queueReceiver.receive();
                  Queue tempQueue = (Queue) message.getJMSReplyTo();

                  QueueSender replySender = session.createSender(tempQueue);
                  TextMessage reply = session.createTextMessage();
                  reply.setText("Request Processed");
                  reply.setJMSCorrelationID(message.getJMSMessageID());
                  replySender.send(reply);

                  if (message.getText().equals("Quit"))
                     done = true;
               }

               session.close();
               log.debug("Server Thread Finished");

            }
            catch (Exception e)
            {
               log.error("Error", e);
            }
         }
      };

      serverThread.start();

      QueueSession session = queueConnection.createQueueSession(false, Session.AUTO_ACKNOWLEDGE);
      Queue queue = (Queue) context.lookup(TEST_QUEUE);

      QueueRequestor queueRequestor = new QueueRequestor(session, queue);
      TextMessage message = session.createTextMessage();
      message.setText("Request Test");

      for (int i = 0; i < 5; i++)
      {

         getLog().debug("Making client request #" + i);
         TextMessage reply = (TextMessage) queueRequestor.request(message);
         String replyID = new String(reply.getJMSCorrelationID());
         if (!replyID.equals(message.getJMSMessageID()))
            throw new Exception("REQUEST: ERROR: Reply does not match sent message");

      }

      getLog().debug("Making client request to shut server down.");
      message.setText("Quit");
      queueRequestor.request(message);

      serverThread.join();
      disconnect();

      getLog().debug("RequestReplyQueue passed");
   }

   /**
    * Test that temporary queues can be deleted.
    */
   public void testTemporaryQueueDelete() throws Exception
   {

      getLog().debug("Starting TemporaryQueueDelete test");
      connect();

      QueueSession session = queueConnection.createQueueSession(false, Session.AUTO_ACKNOWLEDGE);
      TemporaryQueue queue = session.createTemporaryQueue();

      queue.delete();

      disconnect();

      getLog().debug("TemporaryQueueDelete passed");
   }

   /**
    * Test that temporary topics can be deleted.
    */
   public void testTemporaryTopicDelete() throws Exception
   {

      getLog().debug("Starting TemporaryTopicDelete test");
      connect();

      TopicSession session = topicConnection.createTopicSession(false, Session.AUTO_ACKNOWLEDGE);
      TemporaryTopic topic = session.createTemporaryTopic();

      topic.delete();

      disconnect();

      getLog().debug("TemporaryTopicDelete passed");
   }

   /**
    * Test invalid destination trying to send a message.
    */
   public void testInvalidDestinationQueueSend() throws Exception
   {

      getLog().debug("Starting InvaidDestinationQueueSend test");
      connect();

      QueueSession session = queueConnection.createQueueSession(false, Session.AUTO_ACKNOWLEDGE);
      TemporaryQueue queue = session.createTemporaryQueue();
      QueueSender sender = session.createSender(queue);
      queue.delete();

      TextMessage message = session.createTextMessage("hello");
      boolean caught = false;
      try
      {
         sender.send(message);
      }
      catch (InvalidDestinationException expected)
      {
         caught = true;
      }

      disconnect();

      assertTrue("Expected an InvalidDestinationException", caught);

      getLog().debug("InvaldDestinationQueueSend passed");
   }

   /**
    * Test invalid destination trying to receive a message.
    */
   public void testInvalidDestinationQueueReceive() throws Exception
   {

      getLog().debug("Starting InvalidDestinationQueueReceive test");
      connect();

      QueueSession session = queueConnection.createQueueSession(false, Session.AUTO_ACKNOWLEDGE);
      TemporaryQueue queue = session.createTemporaryQueue();
      QueueReceiver receiver = session.createReceiver(queue);
      queue.delete();

      boolean caught = false;
      try
      {
         receiver.receiveNoWait();
      }
      catch (InvalidDestinationException expected)
      {
         caught = true;
      }

      disconnect();

      assertTrue("Expected an InvalidDestinationException", caught);

      getLog().debug("InvalidDestinationQueueReceive passed");
   }

   /**
    * Test invalid destination trying to browse a message.
    */
   public void testInvalidDestinationQueueBrowse() throws Exception
   {

      getLog().debug("Starting InvalidDestinationQueueBrowse test");
      connect();

      QueueSession session = queueConnection.createQueueSession(false, Session.AUTO_ACKNOWLEDGE);
      TemporaryQueue queue = session.createTemporaryQueue();
      QueueBrowser browser = session.createBrowser(queue);
      queue.delete();

      boolean caught = false;
      try
      {
         browser.getEnumeration();
      }
      catch (InvalidDestinationException expected)
      {
         caught = true;
      }

      disconnect();

      assertTrue("Expected an InvalidDestinationException", caught);

      getLog().debug("InvalidDestinationQueueBrowse passed");
   }

   /**
    * Test invalid destination trying to send a message.
    */
   public void testInvalidDestinationTopicPublish() throws Exception
   {

      getLog().debug("Starting InvaidDestinationTopicPublish test");
      connect();

      TopicSession session = topicConnection.createTopicSession(false, Session.AUTO_ACKNOWLEDGE);
      TemporaryTopic topic = session.createTemporaryTopic();
      TopicPublisher publisher = session.createPublisher(topic);
      topic.delete();

      TextMessage message = session.createTextMessage("hello");
      boolean caught = false;
      try
      {
         publisher.publish(message);
      }
      catch (InvalidDestinationException expected)
      {
         caught = true;
      }

      disconnect();

      assertTrue("Expected an InvalidDestinationException", caught);

      getLog().debug("InvaldDestinationTopicPublish passed");
   }

   /**
    * Test errors trying on topic subscribe.
    */
   public void testErrorsTopicSubscribe() throws Exception
   {

      getLog().debug("Starting InvalidDestinationTopicSubscribe test");
      connect();

      try
      {
         TopicSession session = topicConnection.createTopicSession(false, Session.AUTO_ACKNOWLEDGE);
         Topic topic = (Topic) context.lookup(TEST_TOPIC);
         TemporaryTopic temp = session.createTemporaryTopic();

         boolean caught = false;
         try
         {
            session.createSubscriber(null);
         }
         catch (InvalidDestinationException expected)
         {
            caught = true;
         }
         assertTrue("Expected an InvalidDestinationException for a null topic", caught);

         caught = false;
         try
         {
            session.createSubscriber(null, null, true);
         }
         catch (InvalidDestinationException expected)
         {
            caught = true;
         }
         assertTrue("Expected an InvalidDestinationException for a null topic", caught);

         caught = false;
         try
         {
            session.createDurableSubscriber(null, "NotUsed");
         }
         catch (InvalidDestinationException expected)
         {
            caught = true;
         }
         assertTrue("Expected an InvalidDestinationException for a null topic", caught);

         caught = false;
         try
         {
            session.createDurableSubscriber(temp, "NotUsed");
         }
         catch (InvalidDestinationException expected)
         {
            caught = true;
         }
         assertTrue("Expected an InvalidDestinationException for a temporary topic", caught);

         caught = false;
         try
         {
            session.createDurableSubscriber(null, "NotUsed", null, true);
         }
         catch (InvalidDestinationException expected)
         {
            caught = true;
         }
         assertTrue("Expected an InvalidDestinationException for a null topic", caught);

         caught = false;
         try
         {
            session.createDurableSubscriber(temp, "NotUsed", null, true);
         }
         catch (InvalidDestinationException expected)
         {
            caught = true;
         }
         assertTrue("Expected an InvalidDestinationException for a temporary topic", caught);

         caught = false;
         try
         {
            session.createDurableSubscriber(topic, null);
         }
         catch (Exception expected)
         {
            caught = true;
         }
         assertTrue("Expected a Exception for a null subscription", caught);

         caught = false;
         try
         {
            session.createDurableSubscriber(topic, null, null, false);
         }
         catch (Exception expected)
         {
            caught = true;
         }
         assertTrue("Expected a Exception for a null subscription", caught);

         caught = false;
         try
         {
            session.createDurableSubscriber(topic, "  ");
         }
         catch (Exception expected)
         {
            caught = true;
         }
         assertTrue("Expected a Exception for an empty subscription", caught);

         caught = false;
         try
         {
            session.createDurableSubscriber(topic, "  ", null, false);
         }
         catch (Exception expected)
         {
            caught = true;
         }
         assertTrue("Expected a Exception for an empty subscription", caught);

         TopicSubscriber subscriber = session.createSubscriber(temp);
         temp.delete();

         caught = false;
         try
         {
            subscriber.receiveNoWait();
         }
         catch (InvalidDestinationException expected)
         {
            caught = true;
         }
         assertTrue("Expected an InvalidDestinationException", caught);
      }
      finally
      {
         disconnect();
      }

      getLog().debug("InvalidDestinationTopicSubscriber passed");
   }

   /**
    * Test create queue.
    */
   public void testCreateQueue() throws Exception
   {

      getLog().debug("Starting create queue test");
      connect();

      QueueSession session = queueConnection.createQueueSession(false, Session.AUTO_ACKNOWLEDGE);

      Queue jndiQueue = (Queue) getInitialContext().lookup("queue/testQueue");
      Queue createQueue = session.createQueue(jndiQueue.getQueueName());
      assertTrue("Failed for " + QUEUE_FACTORY, jndiQueue.equals(createQueue));

      getLog().debug("InvalidDestinationTopicSubscriber passed");
   }

   public void testMessageListener() throws Exception
   {
      getLog().debug("Starting create queue test");

      connect();
      queueConnection.start();
      drainQueue();
      final CountDown counter1 = new CountDown(3);

      QueueSession session = queueConnection.createQueueSession(false, Session.AUTO_ACKNOWLEDGE);
      Queue queue = (Queue) context.lookup(TEST_QUEUE);

      QueueReceiver receiver = session.createReceiver(queue);
      receiver.setMessageListener(new MessageListener()
      {
         public void onMessage(Message msg)
         {
            Category log = Category.getInstance(getClass().getName());
            log.debug("ML");
            try
            {
               if (msg instanceof TextMessage)
               {
                  log.debug(((TextMessage) msg).getText());
                  counter1.release();
               }
            }
            catch (Exception e)
            {
            }
         }
      });

      QueueSender sender = session.createSender(queue);

      TextMessage message = session.createTextMessage();
      message.setText("Normal message");
      sender.send(message, DeliveryMode.NON_PERSISTENT, 4, 0);
      //sender.send(queue, message, DeliveryMode.NON_PERSISTENT, 4, 0);
      message.setText("Persistent message");
      sender.send(message, DeliveryMode.PERSISTENT, 4, 0);
      //sender.send(queue, message, DeliveryMode.PERSISTENT, 4, 0);
      message.setText("High Priority Persistent message");
      sender.send(message, DeliveryMode.PERSISTENT, 10, 0);
      //sender.send(queue, message, DeliveryMode.PERSISTENT, 10, 0);

      // Wait for the msgs to be received
      counter1.acquire();
      log.debug("MessageListener1 received the TMs sent");

      final CountDown counter2 = new CountDown(2);
      receiver.setMessageListener(new MessageListener()
      {
         public void onMessage(Message msg)
         {
            Category log = Category.getInstance(getClass().getName());
            log.debug("ML 2");
            try
            {
               if (msg instanceof TextMessage)
               {
                  log.debug(((TextMessage) msg).getText());
                  counter2.release();
               }
            }
            catch (Exception e)
            {
            }
         }
      });

      message.setText("Persistent message");
      sender.send(message, DeliveryMode.PERSISTENT, 4, 0);
      //sender.send(queue, message, DeliveryMode.PERSISTENT, 4, 0);
      message.setText("High Priority Persistent message");
      sender.send(message, DeliveryMode.PERSISTENT, 10, 0);
      //sender.send(queue, message, DeliveryMode.PERSISTENT, 10, 0);

      // Wait for the msgs to be received
      counter2.acquire();
      log.debug("MessageListener2 received the TMs sent");

      receiver.setMessageListener(null);

      message.setText("Persistent message");
      sender.send(message, DeliveryMode.PERSISTENT, 4, 0);
      //sender.send(queue, message, DeliveryMode.PERSISTENT, 4, 0);
      message.setText("High Priority Persistent message");
      sender.send(message, DeliveryMode.PERSISTENT, 10, 0);
      //sender.send(queue, message, DeliveryMode.PERSISTENT, 10, 0);

      sender.close();
      drainQueue();
      disconnect();
      getLog().debug("MessageListener test passed");
   }

   public void testApplicationServerStuff() throws Exception
   {
      getLog().debug("Starting testing app server stuff");
      connect();

      Queue testQueue = (Queue) context.lookup(TEST_QUEUE);
      final QueueSession session = queueConnection.createQueueSession(false, Session.AUTO_ACKNOWLEDGE);

      session.setMessageListener(new MessageListener()
      {
         public void onMessage(Message mess)
         {
            Category log = Category.getInstance(getClass().getName());
            log.debug("Processing message");
            try
            {
               if (mess instanceof TextMessage)
                  log.debug(((TextMessage) mess).getText());
            }
            catch (Exception e)
            {
               log.error("Error", e);
            }
         }
      });

      QueueSender sender = session.createSender(testQueue);
      sender.send(session.createTextMessage("Hi"));
      sender.send(session.createTextMessage("There"));
      sender.send(session.createTextMessage("Guys"));
      queueConnection.createConnectionConsumer(testQueue, null, new ServerSessionPool()
      {
         public ServerSession getServerSession()
         {
            Category.getInstance(getClass().getName()).debug("Getting server session.");
            return new ServerSession()
            {
               public Session getSession()
               {
                  return session;
               }
               public void start()
               {
                  Category.getInstance(getClass().getName()).debug("Starting server session.");
                  session.run();
               }
            };
         }
      }, 10);

      queueConnection.start();

      try
      {
         Thread.sleep(5 * 1000);
      }
      catch (Exception e)
      {
      }

      disconnect();
      getLog().debug("Testing app server stuff passed");
   }

   //simply put a few messages on the test queue for next time.
   /*   public void testPM() throws Exception
      {
         getLog().debug("Starting testing pm");
         connect();
         
         Queue testQueue = (Queue)context.lookup(TEST_QUEUE);
         QueueSession session = queueConnection.createQueueSession(false,Session.AUTO_ACKNOWLEDGE);
         QueueSender sender = session.createSender(testQueue);
         sender.send(session.createTextMessage("From last time"));
         sender.send(session.createTextMessage("From last time"));
         sender.send(session.createTextMessage("From last time"));
         sender.close();
         session.close();
         disconnect();
         getLog().debug("Testing pm stuff passed");
      }
   */
   private void drainMessagesForTopic(TopicSubscriber sub) throws JMSException
   {
      Message msg = sub.receive(50);
      int c = 0;
      while (msg != null)
      {
         c++;
         if (msg instanceof TextMessage)
            getLog().debug(((TextMessage) msg).getText());
         msg = sub.receive(50);
      }
      getLog().debug("Received " + c + " messages from topic.");
   }

   public void testTopics() throws Exception
   {
      getLog().debug("Starting Topic test");
      connect();

      TopicConnectionFactory topicFactory = (TopicConnectionFactory) context.lookup(TOPIC_FACTORY);
      topicConnection = topicFactory.createTopicConnection("john", "needle");

      topicConnection.start();

      //set up some subscribers to the topic
      TopicSession session = topicConnection.createTopicSession(false, Session.AUTO_ACKNOWLEDGE);
      Topic topic = (Topic) context.lookup(TEST_TOPIC);

      TopicSubscriber sub1 = session.createDurableSubscriber(topic, "sub1");
      TopicSubscriber sub2 = session.createSubscriber(topic);
      TopicSubscriber sub3 = session.createSubscriber(topic);

      //Now a sender
      TopicPublisher sender = session.createPublisher(topic);

      //send some messages
      sender.publish(session.createTextMessage("Message 1"));
      sender.publish(session.createTextMessage("Message 2"));
      sender.publish(session.createTextMessage("Message 3"));
      drainMessagesForTopic(sub1);
      drainMessagesForTopic(sub2);
      drainMessagesForTopic(sub3);

      //close some subscribers
      sub1.close();
      sub2.close();

      //send some more messages
      sender.publish(session.createTextMessage("Message 4"));
      sender.publish(session.createTextMessage("Message 5"));
      sender.publish(session.createTextMessage("Message 6"));

      //give time for message 4 to be negatively acked (as it will be cause last receive timed out)
      try
      {
         Thread.sleep(5 * 1000);
      }
      catch (InterruptedException e)
      {
      }

      drainMessagesForTopic(sub3);

      //open subscribers again.
      sub1 = session.createDurableSubscriber(topic, "sub1");
      sub2 = session.createSubscriber(topic);

      //Send a final message
      sender.publish(session.createTextMessage("Final message"));
      sender.close();

      drainMessagesForTopic(sub1);
      drainMessagesForTopic(sub2);
      drainMessagesForTopic(sub3);

      sub1.close();
      sub2.close();
      sub3.close();

      session.unsubscribe("sub1");

      topicConnection.stop();
      topicConnection.close();

      disconnect();
      getLog().debug("Topic test passed");
   }

   /**
    * Test to seeif the NoLocal feature of topics works.
    * Messages published from the same connection should not
    * be received by Subscribers on the same connection.
    */
   public void testTopicNoLocal() throws Exception
   {
      getLog().debug("Starting TopicNoLocal test");
      connect();

      TopicConnectionFactory topicFactory = (TopicConnectionFactory) context.lookup(TOPIC_FACTORY);
      TopicConnection topicConnection1 = topicFactory.createTopicConnection();
      topicConnection1.start();
      TopicConnection topicConnection2 = topicFactory.createTopicConnection();
      topicConnection2.start();

      // We don't want local messages on this topic.
      TopicSession session1 = topicConnection1.createTopicSession(false, Session.AUTO_ACKNOWLEDGE);
      Topic topic = (Topic) context.lookup(TEST_TOPIC);
      TopicSubscriber subscriber1 = session1.createSubscriber(topic, null, true);
      TopicPublisher sender1 = session1.createPublisher(topic);

      //Now a sender
      TopicSession session2 = topicConnection2.createTopicSession(false, Session.AUTO_ACKNOWLEDGE);
      TopicPublisher sender2 = session2.createPublisher(topic);

      drainMessagesForTopic(subscriber1);

      //send some messages
      sender1.publish(session1.createTextMessage("Local Message"));
      sender2.publish(session2.createTextMessage("Remote Message"));

      // Get the messages, we should get the remote message
      // but not the local message
      TextMessage msg1 = (TextMessage) subscriber1.receive(2000);
      if (msg1 == null)
      {
         fail("Did not get any messages");
      }
      else
      {
         getLog().debug("Got message: " + msg1);
         if (msg1.getText().equals("Local Message"))
         {
            fail("Got a local message");
         }
         TextMessage msg2 = (TextMessage) subscriber1.receive(2000);
         if (msg2 != null)
         {
            getLog().debug("Got message: " + msg2);
            fail("Got an extra message.  msg1:" + msg1 + ", msg2:" + msg2);
         }
      }

      topicConnection1.stop();
      topicConnection1.close();
      topicConnection2.stop();
      topicConnection2.close();

      disconnect();
      getLog().debug("TopicNoLocal test passed");
   }

   /**
    * Test to see whether no local works if a message
    * was created somewhere else.
    */
   public void testTopicNoLocalBounce() throws Exception
   {
      getLog().debug("Starting TopicNoLocalBounce test");
      connect();

      TopicConnectionFactory topicFactory = (TopicConnectionFactory) context.lookup(TOPIC_FACTORY);
      TopicConnection topicConnection1 = topicFactory.createTopicConnection();
      topicConnection1.start();
      TopicConnection topicConnection2 = topicFactory.createTopicConnection();
      topicConnection2.start();

      // Session 1
      TopicSession session1 = topicConnection1.createTopicSession(false, Session.AUTO_ACKNOWLEDGE);
      Topic topic = (Topic) context.lookup(TEST_TOPIC);
      TopicSubscriber subscriber1 = session1.createSubscriber(topic, null, true);
      TopicPublisher sender1 = session1.createPublisher(topic);

      // Session 2
      TopicSession session2 = topicConnection2.createTopicSession(false, Session.AUTO_ACKNOWLEDGE);
      TopicSubscriber subscriber2 = session2.createSubscriber(topic, null, true);
      TopicPublisher sender2 = session2.createPublisher(topic);

      drainMessagesForTopic(subscriber1);
      drainMessagesForTopic(subscriber2);

      //send the message
      sender1.publish(session1.createTextMessage("Message"));

      assertTrue("Subscriber1 should not get a message", subscriber1.receiveNoWait() == null);
      TextMessage msg = (TextMessage) subscriber2.receive(2000);
      assertTrue("Subscriber2 should get a message, got " + msg, msg != null && msg.getText().equals("Message"));

      //send it back
      sender2.publish(msg);

      msg = (TextMessage) subscriber1.receive(2000);
      assertTrue("Subscriber1 should get a message, got " + msg, msg != null && msg.getText().equals("Message"));
      assertTrue("Subscriber2 should not get a message", subscriber2.receiveNoWait() == null);

      topicConnection1.stop();
      topicConnection1.close();
      topicConnection2.stop();
      topicConnection2.close();

      disconnect();
      getLog().debug("TopicNoLocalBounce test passed");
   }

   /**
    * Test subscribing to a topic with one selector, then changing to another
    */
   public void testTopicSelectorChange() throws Exception
   {
      getLog().debug("Starting TopicSelectorChange test");

      getLog().debug("Create topic connection");
      TopicConnectionFactory topicFactory = (TopicConnectionFactory) context.lookup(TOPIC_FACTORY);
      topicConnection = topicFactory.createTopicConnection("john", "needle");
      topicConnection.start();

      try
      {
         getLog().debug("Retrieving Topic");
         Topic topic = (Topic) context.lookup(TEST_DURABLE_TOPIC);

         getLog().debug("Creating a send session");
         TopicSession sendSession = topicConnection.createTopicSession(false, Session.AUTO_ACKNOWLEDGE);
         TopicPublisher sender = sendSession.createPublisher(topic);

         getLog().debug("Clearing the topic");
         TopicSession subSession = topicConnection.createTopicSession(false, Session.AUTO_ACKNOWLEDGE);
         TopicSubscriber subscriber = subSession.createDurableSubscriber(topic, "test");
         Message message = subscriber.receive(50);
         while (message != null)
            message = subscriber.receive(50);
         subSession.close();

         getLog().debug("Subscribing to topic, looking for Value = 'A'");
         subSession = topicConnection.createTopicSession(false, Session.AUTO_ACKNOWLEDGE);
         subscriber = subSession.createDurableSubscriber(topic, "test", "Value = 'A'", false);

         getLog().debug("Send some messages");
         message = sendSession.createTextMessage("Message1");
         message.setStringProperty("Value", "A");
         sender.publish(message);
         message = sendSession.createTextMessage("Message2");
         message.setStringProperty("Value", "A");
         sender.publish(message);
         message = sendSession.createTextMessage("Message3");
         message.setStringProperty("Value", "B");
         sender.publish(message);

         getLog().debug("Retrieving the A messages");
         message = subscriber.receive(2000);
         assertTrue("Expected message 1", message != null);
         assertTrue("Should get an A", message.getStringProperty("Value").equals("A"));
         message = subscriber.receive(2000);
         assertTrue("Expected message 2", message != null);
         assertTrue("Should get a second A", message.getStringProperty("Value").equals("A"));
         assertTrue("That should be it for A", subscriber.receive(2000) == null);

         getLog().debug("Closing the subscriber without acknowledgement");
         subSession.close();

         getLog().debug("Subscribing to topic, looking for Value = 'B'");
         subSession = topicConnection.createTopicSession(false, Session.AUTO_ACKNOWLEDGE);
         subscriber = subSession.createDurableSubscriber(topic, "test", "Value = 'B'", false);

         getLog().debug("Retrieving the non-existent B messages");
         assertTrue("B should not be there", subscriber.receive(2000) == null);

         getLog().debug("Closing the subscriber.");
         subSession.close();

         getLog().debug("Subscribing to topic, looking for those Value = 'A'");
         subSession = topicConnection.createTopicSession(false, Session.AUTO_ACKNOWLEDGE);
         subscriber = subSession.createDurableSubscriber(topic, "test", "Value = 'A'", false);
         assertTrue("Should not be any A the subscription was changed", subscriber.receive(2000) == null);
         subSession.close();

         getLog().debug("Subscribing to topic, looking for everything");
         subSession = topicConnection.createTopicSession(false, Session.CLIENT_ACKNOWLEDGE);
         subscriber = subSession.createDurableSubscriber(topic, "test", null, false);

         message = sendSession.createTextMessage("Message4");
         message.setStringProperty("Value", "A");
         sender.publish(message);

         message = subscriber.receive(2000);
         assertTrue("Expected message 4", message != null);
         assertTrue("Should be an A which we don't acknowledge", message.getStringProperty("Value").equals("A"));
         subSession.close();

         getLog().debug("Subscribing to topic, looking for the Value = 'A'");
         subSession = topicConnection.createTopicSession(false, Session.AUTO_ACKNOWLEDGE);
         subscriber = subSession.createDurableSubscriber(topic, "test", "Value = 'A'", false);
         assertTrue(
            "Should not be any A, the subscription was changed. Even though the old and new selectors match the message",
            subscriber.receive(2000) == null);
         subSession.close();

         getLog().debug("Closing the send session");
         sendSession.close();

         getLog().debug("Removing the subscription");
         subSession = topicConnection.createTopicSession(false, Session.AUTO_ACKNOWLEDGE);
         subSession.unsubscribe("test");

      }
      finally
      {
         getLog().debug("Closing the connection");
         topicConnection.close();
      }

      getLog().debug("TopicSelectorChange test passed");
   }

   /**
    * Test subscribing to a topic with a null and empty selector
    */
   public void testTopicSelectorNullOrEmpty() throws Exception
   {
      getLog().debug("Starting TopicSelectorNullOrEmpty test");

      getLog().debug("Create topic connection");
      TopicConnectionFactory topicFactory = (TopicConnectionFactory) context.lookup(TOPIC_FACTORY);
      topicConnection = topicFactory.createTopicConnection("john", "needle");
      topicConnection.start();

      try
      {
         getLog().debug("Retrieving Topic");
         Topic topic = (Topic) context.lookup(TEST_DURABLE_TOPIC);

         getLog().debug("Creating a send session");
         TopicSession sendSession = topicConnection.createTopicSession(false, Session.AUTO_ACKNOWLEDGE);
         TopicPublisher sender = sendSession.createPublisher(topic);

         getLog().debug("Clearing the topic");
         TopicSession subSession = topicConnection.createTopicSession(false, Session.AUTO_ACKNOWLEDGE);
         TopicSubscriber subscriber = subSession.createDurableSubscriber(topic, "test");
         TextMessage message = (TextMessage) subscriber.receive(50);
         while (message != null)
            message = (TextMessage) subscriber.receive(50);
         subSession.close();

         getLog().debug("Subscribing to topic, with null selector");
         subSession = topicConnection.createTopicSession(false, Session.AUTO_ACKNOWLEDGE);
         subscriber = subSession.createDurableSubscriber(topic, "test", null, false);

         getLog().debug("Send a message");
         message = sendSession.createTextMessage("Message1");
         sender.publish(message);

         getLog().debug("Retrieving the message");
         message = (TextMessage) subscriber.receive(2000);
         assertTrue("Expected message 1", message != null);
         assertTrue("Should get Message1", message.getText().equals("Message1"));
         getLog().debug("Closing the subscriber");
         subSession.close();

         getLog().debug("Subscribing to topic, with an empty selector");
         subSession = topicConnection.createTopicSession(false, Session.AUTO_ACKNOWLEDGE);
         subscriber = subSession.createDurableSubscriber(topic, "test", "   ", false);

         getLog().debug("Send a message");
         message = sendSession.createTextMessage("Message2");
         sender.publish(message);

         getLog().debug("Retrieving the message");
         message = (TextMessage) subscriber.receive(2000);
         assertTrue("Expected message 2", message != null);
         assertTrue("Should get Message2", message.getText().equals("Message2"));
         getLog().debug("Closing the subscriber");

         getLog().debug("Removing the subscription");
         subSession = topicConnection.createTopicSession(false, Session.AUTO_ACKNOWLEDGE);
         subSession.unsubscribe("test");
         subSession.close();

      }
      finally
      {
         getLog().debug("Closing the connection");
         topicConnection.close();
      }

      getLog().debug("TopicSelectorNullOrEmpty test passed");
   }

   /**
    * Test sending/receiving an outdated message
    */
   public void testSendReceiveOutdated() throws Exception
   {
      getLog().debug("Starting SendReceiveOutdated test");

      connect();
      try
      {
         queueConnection.start();
         drainQueue();
         queueConnection.stop();

         QueueSession session = queueConnection.createQueueSession(false, Session.AUTO_ACKNOWLEDGE);
         Queue queue = (Queue) context.lookup(TEST_QUEUE);
         QueueSender sender = session.createSender(queue);
         QueueReceiver receiver = session.createReceiver(queue);

         // Send a message that has expired
         TextMessage message = session.createTextMessage("Outdated");
         sender.send(message, DeliveryMode.PERSISTENT, 4, 1);
         Thread.sleep(100);

         // Send a message that has not expired
         message = session.createTextMessage("OK");
         sender.send(message);

         // Try to receive the message the not expired message
         queueConnection.start();
         message = (TextMessage) receiver.receiveNoWait();
         assertEquals("OK", message.getText());

         // Should be no more
         assertTrue("Didn't expect anymore messages", receiver.receiveNoWait() == null);
      }
      finally
      {
         disconnect();
      }

      getLog().debug("SendReceiveOutdated test passed");
   }

   class Synch
   {
      boolean waiting = false;
      String text;
      public synchronized void doWait(long timeout) throws InterruptedException
      {
         waiting = true;
         this.wait(timeout);
      }
      public synchronized void doNotify() throws InterruptedException
      {
         while (waiting == false)
            wait(100);
         this.notifyAll();
      }
      public String getText()
      {
         return text;
      }
      public void setText(String text)
      {
         this.text = text;
      }
   }

   /**
    * Test sending/listening an outdated message
    */
   public void testSendListenOutdated() throws Exception
   {
      getLog().debug("Starting SendListenOutdated test");

      connect();
      try
      {
         queueConnection.start();
         drainQueue();
         queueConnection.stop();

         QueueSession session = queueConnection.createQueueSession(false, Session.AUTO_ACKNOWLEDGE);
         Queue queue = (Queue) context.lookup(TEST_QUEUE);
         QueueSender sender = session.createSender(queue);
         QueueReceiver receiver = session.createReceiver(queue);

         // Send a message that has expired
         TextMessage message = session.createTextMessage("Outdated");
         sender.send(message, DeliveryMode.PERSISTENT, 4, 1);
         Thread.sleep(100);

         // Send a message that has not expired
         message = session.createTextMessage("OK");
         sender.send(message);

         // Try to receive the message the not expired message
         final Synch synch = new Synch();
         MessageListener messagelistener = new MessageListener()
         {
            public void onMessage(Message message)
            {
               listenOutdated(message, synch);
            }
         };
         receiver.setMessageListener(messagelistener);
         queueConnection.start();

         synch.doWait(10000);
         assertEquals("OK", synch.getText());
      }
      finally
      {
         disconnect();
      }

      getLog().debug("SendListenOutdated test passed");
   }

   private void listenOutdated(Message message, Synch synch)
   {
      try
      {
         synch.setText(((TextMessage) message).getText());
      }
      catch (Throwable t)
      {
         log.error("Error:", t);
      }
      finally
      {
         try
         {
            synch.doNotify();
         }
         catch (Throwable t)
         {
            log.error("Error:", t);
         }
      }
   }
}
