/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.test.jbossmq.test;

import javax.jms.Message;
import javax.jms.Queue;
import javax.jms.QueueConnection;
import javax.jms.QueueConnectionFactory;
import javax.jms.QueueReceiver;
import javax.jms.QueueSender;
import javax.jms.QueueSession;
import javax.jms.Session;
import javax.management.Attribute;
import javax.management.ObjectName;
import javax.naming.Context;

import org.jboss.mq.DestinationFullException;
import org.jboss.mx.util.ObjectNameFactory;
import org.jboss.test.JBossTestCase;

/**
 * Destination Full tests
 *
 * @author <a href="mailto:adrian@jboss.org>Adrian Brock</a>
 * @version <tt>$Revision: 1.1.2.1 $</tt>
 */
public class DestinationFullUnitTestCase extends JBossTestCase
{
   static String QUEUE_FACTORY = "ConnectionFactory";
   static String TEST_QUEUE = "queue/testQueue";
   static ObjectName QUEUE_OBJECT_NAME = ObjectNameFactory.create("jboss.mq.destination:service=Queue,name=testQueue");

   QueueConnection queueConnection;
   Queue queue;

   public DestinationFullUnitTestCase(String name) throws Exception
   {
      super(name);
   }

   public void testQueueFull() throws Exception
   {
      connect();
      try
      {
         drainQueue();
         setMaxDepth(10);
         
         QueueSession session = queueConnection.createQueueSession(false, Session.AUTO_ACKNOWLEDGE);
         QueueSender sender = session.createSender(queue);
         Message message = session.createMessage();
         
         for (int i = 0; i < 10; ++i)
            sender.send(message);

         try
         {
            sender.send(message);
            fail("Expected a destination full exception.");           
         }
         catch (DestinationFullException expected)
         {
         }
         session.close();
         drainQueue();
      }
      finally
      {
         setMaxDepth(0);
         disconnect();
      }
   }

   protected void setMaxDepth(int depth)
      throws Exception
   {
      getServer().setAttribute(QUEUE_OBJECT_NAME, new Attribute("MaxDepth", new Integer(depth)));
   }

   protected void connect() throws Exception
   {
      Context context = getInitialContext();
      QueueConnectionFactory queueFactory = (QueueConnectionFactory) context.lookup(QUEUE_FACTORY);
      queue = (Queue) context.lookup(TEST_QUEUE);
      queueConnection = queueFactory.createQueueConnection();

      getLog().debug("Connection established.");
   }

   protected void disconnect()
   {
      try
      {
         if (queueConnection != null)
            queueConnection.close();
      }
      catch (Exception ignored)
      {
      }

      getLog().debug("Connection closed.");
   }

   // Emptys out all the messages in a queue
   protected void drainQueue() throws Exception
   {
      QueueSession session = queueConnection.createQueueSession(false, Session.AUTO_ACKNOWLEDGE);

      QueueReceiver receiver = session.createReceiver(queue);
      Message message = receiver.receive(50);
      int c = 0;
      while (message != null)
      {
         c++;
         message = receiver.receive(50);
      }

      if (c != 0)
         getLog().debug("Drained " + c + " messages from the queue");

      session.close();
   }
}
