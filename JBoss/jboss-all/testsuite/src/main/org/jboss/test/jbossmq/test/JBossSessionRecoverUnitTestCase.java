/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.test.jbossmq.test;

import javax.jms.JMSException;
import javax.jms.Message;
import javax.jms.MessageListener;
import javax.jms.ObjectMessage;
import javax.jms.Queue;
import javax.jms.QueueConnection;
import javax.jms.QueueConnectionFactory;
import javax.jms.QueueReceiver;
import javax.jms.QueueSender;
import javax.jms.QueueSession;
import javax.jms.Session;
import javax.naming.Context;
import javax.naming.InitialContext;

import org.jboss.test.JBossTestCase;

/**
 * JBossSessionRecoverUnitTestCase.java
 *
 * a simple session.recover test of JBossMQ
 *
 * @author Seth Sites
 * @version $Revision: 1.2.4.2 $
 */
public class JBossSessionRecoverUnitTestCase extends JBossTestCase
{
   String QUEUE_FACTORY = "ConnectionFactory";
   String TEST_QUEUE = "queue/testQueue";

   Context context;
   QueueConnection queueConnection;
   QueueSession session;

   int counter = 0;
   Exception exception = null;

   public JBossSessionRecoverUnitTestCase(String name) throws Exception
   {
      super(name);
   }

   protected void setUp() throws Exception
   {
      this.getLog().debug("JBossSessionRecoverUnitTestCase, ConnectionFactory started");
   }

   protected void tearDown() throws Exception
   {
      this.getLog().debug("JBossSessionRecoverUnitTestCase, ConnectionFactory done");
   }

   // Emptys out all the messages in a queue
   private void drainQueue() throws Exception
   {
      QueueSession session = queueConnection.createQueueSession(false, Session.AUTO_ACKNOWLEDGE);
      Queue queue = (Queue) context.lookup(TEST_QUEUE);

      QueueReceiver receiver = session.createReceiver(queue);
      Message message = receiver.receive(1000);
      int c = 0;
      while (message != null)
      {
         message = receiver.receive(1000);
         c++;
      }

      if (c != 0)
         getLog().debug("  Drained " + c + " messages from the queue");

      session.close();
   }

   static public void main(String[] args)
   {
      String newArgs[] = { "org.jboss.test.jbossmq.test.JBossSessionRecoverUnitTestCase" };
      junit.swingui.TestRunner.main(newArgs);
   }

   protected void connect() throws Exception
   {
      if (context == null)
      {
         context = new InitialContext();
      }
      QueueConnectionFactory queueFactory = (QueueConnectionFactory) context.lookup(QUEUE_FACTORY);
      queueConnection = queueFactory.createQueueConnection();

      getLog().debug("Connection to JBossMQ established.");
   }

   /**
    * Test that session.recover works with a message listener
    */
   public void testQueueSessionRecovermessageListener() throws Exception
   {
      counter = 0;
      getLog().debug("Starting session.recover() Message Listener test");

      connect();

      queueConnection.start();

      drainQueue();

      session = queueConnection.createQueueSession(false, Session.AUTO_ACKNOWLEDGE);
      Queue queue = (Queue) context.lookup(TEST_QUEUE);
      QueueSender sender = session.createSender(queue);

      // send 20 messages to the queue
      for (int i = 0; i < 20; i++)
      {
         sender.send(session.createObjectMessage(new Integer(i)));
      }

      //close the session, so we can start one with CLIENT_ACKNOWLEDGE
      session.close();
      queueConnection.stop();
      session = queueConnection.createQueueSession(false, Session.CLIENT_ACKNOWLEDGE);

      //create our receiver
      QueueReceiver receiver = session.createReceiver(queue);
      MessageListener messagelistener = new MessageListener()
      {
         public void onMessage(Message message)
         {
            processMessage(message);
         }
      };
      receiver.setMessageListener(messagelistener);
      queueConnection.start();

      //since we put in 20 messages and recovered after receiving 20 we should receive those 20
      //back and get 40 total
      while (counter < 40 && exception == null)
      {
         try
         {
            Thread.sleep(500);
         }
         catch (InterruptedException ie)
         {
         }
      }

      if (exception != null)
      {
         queueConnection.close();
         throw exception;
      }

      queueConnection.close();
      getLog().debug("session.recover() Message Listener passed");
   }

   private void processMessage(Message message)
   {
      try
      {
         if (message instanceof ObjectMessage)
         {
            counter++;
            ObjectMessage objectmessage = (ObjectMessage) message;
            Integer integer = (Integer) objectmessage.getObject();
            int mynumber = integer.intValue();
            getLog().debug("message object " + integer + " counter=" + counter);
            if (mynumber == 19)
            {
               if (counter == 20)
               {
                  session.recover();
               }
               else
               {
                  message.acknowledge();
               }
            }
         }
      }
      catch (JMSException e)
      {
         exception = e;
      }
   }

   class Synch
   {
      boolean waiting = false;
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
   }

   /**
    * Test that session.recover delivers messages in the correct orer
    */
   public void testQueueSessionRecoverMessageListenerOrder() throws Exception
   {
      counter = 0;
      exception = null;
      getLog().debug("Starting session.recover() Message Listener Order test");

      connect();

      queueConnection.start();

      drainQueue();

      session = queueConnection.createQueueSession(false, Session.AUTO_ACKNOWLEDGE);
      Queue queue = (Queue) context.lookup(TEST_QUEUE);
      QueueSender sender = session.createSender(queue);

      // send 4 messages to the queue
      for (int i = 0; i < 4; ++i)
      {
         sender.send(session.createObjectMessage(new Integer(i)));
      }

      //create our receiver
      QueueReceiver receiver = session.createReceiver(queue);
      final Synch synch = new Synch();
      MessageListener messagelistener = new MessageListener()
      {
         public void onMessage(Message message)
         {
            checkMessagesInOrder(session, message, synch);
         }
      };
      receiver.setMessageListener(messagelistener);
      queueConnection.start();

      synch.doWait(10000);

      if (exception != null)
      {
         queueConnection.close();
         throw exception;
      }

      queueConnection.close();
      getLog().debug("session.recover() Message Listener Order passed");
   }

   private void checkMessagesInOrder(Session session, Message message, Synch synch)
   {
      try
      {
         ObjectMessage objectmessage = (ObjectMessage) message;
         Integer integer = (Integer) objectmessage.getObject();
         int mynumber = integer.intValue();

         if (message.getJMSRedelivered() == false)
         {
            log.debug("Recovering " + mynumber);
            session.recover();
            return;
         }

         log.debug("Checking " + mynumber);
         assertTrue("Expected messages in order", mynumber == counter);
         counter++;
         if (counter == 4)
            synch.doNotify();
      }
      catch (Exception e)
      {
         exception = e;
      }
   }

   /**
    * Test that session.recover works with receive
    */
   public void testQueueSessionRecoverReceive() throws Exception
   {
      counter = 0;
      getLog().debug("Starting session.recover() receive test");

      connect();

      queueConnection.start();

      drainQueue();

      session = queueConnection.createQueueSession(false, Session.AUTO_ACKNOWLEDGE);
      Queue queue = (Queue) context.lookup(TEST_QUEUE);
      QueueSender sender = session.createSender(queue);

      // send 20 messages to the queue
      for (int i = 0; i < 20; i++)
      {
         sender.send(session.createObjectMessage(new Integer(i)));
      }

      //close the session, so we can start one with CLIENT_ACKNOWLEDGE
      session.close();
      queueConnection.stop();
      session = queueConnection.createQueueSession(false, Session.CLIENT_ACKNOWLEDGE);

      //create our receiver
      QueueReceiver receiver = session.createReceiver(queue);
      queueConnection.start();

      Message message = receiver.receive(1000);
      int messagecounter = 0;
      while (message != null)
      {
         message = receiver.receive(1000);
         messagecounter++;
      }

      if (messagecounter != 20)
      {
         throw new Exception("Not all sent messages were delivered! messagecounter=" + messagecounter);
      }

      //we got all of our messages, let's recover
      session.recover();

      message = receiver.receive();
      messagecounter = 0;
      while (message != null)
      {
         if (!message.getJMSRedelivered())
         {
            throw new Exception("Message was not marked as redelivered! messagecounter=" + messagecounter);
         }
         message.acknowledge();

         messagecounter++;
         //workaround to keep from timing out since there are no more message on the server
         if (messagecounter < 15)
         {
            message = receiver.receive();
         }
         else
         {
            message = receiver.receive(1000);
         }
      }

      if (messagecounter != 20)
      {
         throw new Exception("Not all unacknowledged messages were redelivered! messagecounter=" + messagecounter);
      }

      queueConnection.close();
      getLog().debug("session.recover() receive passed");
   }

   /**
    * Test that session.recover works with receive(timeout)
    */
   public void testQueueSessionRecoverReceiveTimeout() throws Exception
   {
      counter = 0;
      getLog().debug("Starting session.recover() receive(timeout) test");

      connect();

      queueConnection.start();

      drainQueue();

      session = queueConnection.createQueueSession(false, Session.AUTO_ACKNOWLEDGE);
      Queue queue = (Queue) context.lookup(TEST_QUEUE);
      QueueSender sender = session.createSender(queue);

      // send 20 messages to the queue
      for (int i = 0; i < 20; i++)
      {
         sender.send(session.createObjectMessage(new Integer(i)));
      }

      //close the session, so we can start one with CLIENT_ACKNOWLEDGE
      session.close();
      queueConnection.stop();
      session = queueConnection.createQueueSession(false, Session.CLIENT_ACKNOWLEDGE);

      //create our receiver
      QueueReceiver receiver = session.createReceiver(queue);
      queueConnection.start();

      Message message = receiver.receive(1000);
      int messagecounter = 0;
      while (message != null)
      {
         message = receiver.receive(1000);
         messagecounter++;
      }

      if (messagecounter != 20)
      {
         throw new Exception("Not all sent messages were delivered! messagecounter=" + messagecounter);
      }

      //we got all of our messages, let's recover
      session.recover();

      message = receiver.receive(1000);
      messagecounter = 0;
      while (message != null)
      {
         if (!message.getJMSRedelivered())
         {
            throw new Exception("Message was not marked as redelivered! messagecounter=" + messagecounter);
         }
         message.acknowledge();

         messagecounter++;
         message = receiver.receive(1000);
      }

      if (messagecounter != 20)
      {
         throw new Exception("Not all unacknowledged messages were redelivered! messagecounter=" + messagecounter);
      }

      queueConnection.close();
      getLog().debug("session.recover() receive(timeout) passed");
   }

   /**
   * Test that session.recover works with receiveNoWait
   */
   public void testQueueSessionRecoverReceiveNoWait() throws Exception
   {
      counter = 0;
      getLog().debug("Starting session.recover() receiveNoWait test");

      connect();

      queueConnection.start();

      drainQueue();

      session = queueConnection.createQueueSession(false, Session.AUTO_ACKNOWLEDGE);
      Queue queue = (Queue) context.lookup(TEST_QUEUE);
      QueueSender sender = session.createSender(queue);

      // send 20 messages to the queue
      for (int i = 0; i < 20; i++)
      {
         sender.send(session.createObjectMessage(new Integer(i)));
      }

      //close the session, so we can start one with CLIENT_ACKNOWLEDGE
      session.close();
      queueConnection.stop();
      session = queueConnection.createQueueSession(false, Session.CLIENT_ACKNOWLEDGE);

      //create our receiver
      QueueReceiver receiver = session.createReceiver(queue);
      queueConnection.start();

      Message message = receiver.receiveNoWait();
      int messagecounter = 0;
      while (message != null)
      {
         message = receiver.receiveNoWait();
         messagecounter++;
      }

      if (messagecounter != 20)
      {
         throw new Exception("Not all sent messages were delivered! messagecounter=" + messagecounter);
      }

      //we got all of our messages, let's recover
      session.recover();

      message = receiver.receiveNoWait();
      messagecounter = 0;
      while (message != null)
      {
         if (!message.getJMSRedelivered())
         {
            throw new Exception("Message was not marked as redelivered! messagecounter=" + messagecounter);
         }
         message.acknowledge();

         messagecounter++;
         message = receiver.receiveNoWait();
      }

      if (messagecounter != 20)
      {
         throw new Exception("Not all unacknowledged messages were redelivered! messagecounter=" + messagecounter);
      }

      queueConnection.close();
      getLog().debug("session.recover() receiveNoWait passed");
   }
}