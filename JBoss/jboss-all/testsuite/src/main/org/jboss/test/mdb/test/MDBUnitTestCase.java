/*
 * Copyright (c) 2000 Peter Antman Tim <peter.antman@tim.se>
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
package org.jboss.test.mdb.test;

import javax.management.ObjectName;
import javax.jms.*;
import javax.naming.*;

import junit.framework.Test;
import junit.framework.TestCase;
import junit.framework.TestSuite;

import org.jboss.test.mdb.bean.CustomMessage;

import org.jboss.test.JBossTestCase;
import org.jboss.test.JBossTestSetup;

//import org.jboss.jms.jndi.*;

/**
 * Some simple tests of MDB. These could be much more elaborated.
 *
 * In the future at least the following tests should be done some how:
 * <ol>
 *   <li>Queue
 *   <li>Topic
 *   <li>Durable topic
 *   <li>Bean TX - with AUTO_ACK and DUPS_OK
 *   <li>CMT Required
 *   <li>CMT NotSupported
 *   <li>Selector
 *   <li>User and password login
 *   <li>Al the stuff with the context
 * </ol>
 *
 * <p>Created: Fri Dec 29 16:53:26 2000
 *
 * @author  <a href="mailto:peter.antman@tim.se">Peter Antman</a>
 * @author  <a href="mailto:jason@planet57.com">Jason Dillon</a>
 * @author  <a href="mailto:d_jencks@users.sourceforge.net">David Jencks</a>
 * @version <pre>$Revision: 1.7.4.2 $</pre>
 */
public class MDBUnitTestCase
   extends JBossTestCase
{
   // Static --------------------------------------------------------
    
   // Provider specific
   static String TOPIC_FACTORY = "ConnectionFactory";
   static String QUEUE_FACTORY = "ConnectionFactory";
    
   QueueConnection queueConnection;
   TopicConnection topicConnection;

   // JMSProviderAdapter providerAdapter;

   String dest;
    
   public MDBUnitTestCase(String name, String dest) {
      super(name);
      this.dest = dest;
      // Get JMS JNDI Adapter
      // Class cls = Class.forName(providerAdapterClass);
      // providerAdapter = (JMSProviderAdapter)cls.newInstance();
      // This is not completly clean since it still have to use
      // provider specific queue and topic names!!
   }
    
    
   protected void tearDown() throws Exception {
      if (topicConnection != null) {
         topicConnection.close();
      }
      if (queueConnection != null) {
         queueConnection.close();
      }
   }

   protected void printHeader() {
      getLog().info("\n---- Testing method " + getName() + 
                         " for destination " +dest);
   }

   private QueueSession getQueueSession() throws Exception {
      if (queueConnection == null) {
         QueueConnectionFactory queueFactory = 
            (QueueConnectionFactory)getInitialContext().lookup(QUEUE_FACTORY);
	    
         queueConnection = queueFactory.createQueueConnection();
      }
      return queueConnection.createQueueSession(false, 
                                                Session.AUTO_ACKNOWLEDGE);
   }
    
   private TopicSession getTopicSession() throws Exception {
      if (topicConnection == null) {
         TopicConnectionFactory topicFactory = 
            (TopicConnectionFactory)getInitialContext().lookup(TOPIC_FACTORY);
         topicConnection = topicFactory.createTopicConnection();
      }

      // No transaction & auto ack
      return topicConnection.createTopicSession(false,
                                                Session.AUTO_ACKNOWLEDGE);
   }

   /**
    * Test sending messages to Topic testTopic
    */
   public void testQueue() throws Exception {
      printHeader();
      QueueSession session = getQueueSession();
      Queue queue = (Queue)getInitialContext().lookup(dest);
      QueueSender sender = session.createSender(queue);

      getLog().debug("TestQueue: " + dest + " Sending 10 messages 1-10");
      for (int i = 1; i < 11; i++) {
         TextMessage message = session.createTextMessage();
         message.setText("Queue Message " + dest + " nr " + i);
         sender.send(queue, message);
      }

      sender.close();
   }

   /**
    * Test sending messages to Queue testQueue
    */
   public void testTopic() throws Exception {
      printHeader();
      TopicSession session = getTopicSession();
      Topic topic = (Topic)getInitialContext().lookup(dest);
      TopicPublisher pub = session.createPublisher(topic);

      getLog().debug("TestTopic: " + dest +
                         ": Sending 10st messages 1-10");
        
      for (int i = 1; i < 11; i++) {
         TextMessage message = session.createTextMessage();
         message.setText("Topic Message " + dest + " nr " + i);
         pub.publish(topic, message);
      }

      pub.close();
   }

   /**
    * Test sending messages to queue testObjectMessage
    */
   public void testObjectMessage() throws Exception {
      printHeader();
      QueueSession session = getQueueSession();
      // Non portable!!
      Queue queue = (Queue)getInitialContext().lookup("queue/testObjectMessage");
      QueueSender sender = session.createSender(queue);

      getLog().debug("TestQueue: Sending 10 messages 1-10");
      for (int i = 1; i < 11; i++) {
         ObjectMessage message = session.createObjectMessage();
         message.setObject(new CustomMessage(i));
         sender.send(queue, message);
      }

      sender.close();
      session.close();
   }


   public void testWaitForCompleation() throws Exception {
      try { Thread.currentThread().sleep(1000*20);
      } catch ( InterruptedException e ) {}
   }

   public void testNoQueueConstructionForAlreadyExists()
      throws Exception
   {
      try
      {
         getInitialContext().lookup("queue/QueueInADifferentContext");
      }
      catch (NamingException expected)
      {
         return;
      }
      fail("It should not create queue/QueueInADifferentContext");
   }

   public void testNoTopicConstructionForAlreadyExists()
      throws Exception
   {
      try
      {
         getInitialContext().lookup("topic/TopicInADifferentContext");
      }
      catch (NamingException expected)
      {
         return;
      }
      fail("It should not create topic/TopicInADifferentContext");
   }

   /**
    * Setup the test suite.
    */
   public static Test suite() throws Exception
   {
      TestSuite suite = new TestSuite();
      suite.addTest(new MDBUnitTestCase("testServerFound",""));
      suite.addTest(new MDBUnitTestCase("testNoQueueConstructionForAlreadyExists",""));
      suite.addTest(new MDBUnitTestCase("testNoTopicConstructionForAlreadyExists",""));
      suite.addTest(new MDBUnitTestCase("testObjectMessage",""));
      suite.addTest(new MDBUnitTestCase("testQueue","queue/testQueue"));
      suite.addTest(new MDBUnitTestCase("testTopic","topic/testTopic"));
      suite.addTest(new MDBUnitTestCase("testTopic","topic/testDurableTopic"));
      suite.addTest(new MDBUnitTestCase("testQueue","queue/ex"));
      suite.addTest(new MDBUnitTestCase("testQueue","queue/A"));
      suite.addTest(new MDBUnitTestCase("testWaitForCompleation",""));
      suite.addTest(new MDBUnitTestCase("testQueue","queue/B"));

      return new JBossTestSetup(getDeploySetup(suite, "mdb.jar"))
         {
             protected void tearDown() throws Exception
             {
                super.tearDown();

                // Remove the DLQ messages created by the TxTimeout test
                getServer().invoke
                (
                   new ObjectName("jboss.mq.destination:service=Queue,name=DLQ"),
                   "removeAllMessages",
                   new Object[0],
                   new String[0]
                );

                // Remove the durable subscription
                TopicConnectionFactory topicFactory = (TopicConnectionFactory) getInitialContext().lookup(TOPIC_FACTORY);
                TopicConnection topicConnection = topicFactory.createTopicConnection("john", "needle");
                TopicSession session = topicConnection.createTopicSession(false, Session.AUTO_ACKNOWLEDGE);
                session.unsubscribe("DurableSubscriberExample");
                topicConnection.close();
             }
          };
   }
}



