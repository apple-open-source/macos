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
import javax.jms.QueueSession;
import javax.jms.Queue;
import javax.jms.QueueSender;
import javax.jms.Session;
import javax.jms.TextMessage;
import javax.jms.QueueBrowser;

import org.jboss.test.JBossTestSetup;

import junit.framework.TestSuite;
import junit.framework.Test;


/**
 * JBossMQUnitTestCase.java
 *
 * Some simple tests of spyderMQ
 *
 * @author
 * @version $Revision: 1.1.2.3 $
 */
public class ScheduledDeliveryUnitTestCase
   extends JBossMQUnitTest
{
   
   public ScheduledDeliveryUnitTestCase(String name) throws Exception
   {
      super(name);
   }
   
   /**
    * Test that messages are ordered by scheduled date.
    * Tests vendor property.
    * <code>SpyMessage.PROPERTY_SCHEDULED_DELIVERY</code>
    */
   public void testScheduledDelivery() throws Exception
   {
      getLog().debug("Starting ScheduledDelivery test");
      
      connect();
      
      queueConnection.start();
      
      drainQueue();
      
      QueueSession session = queueConnection.createQueueSession(false, Session.AUTO_ACKNOWLEDGE);
      Queue queue = (Queue)context.lookup(TEST_QUEUE);
      QueueSender sender = session.createSender(queue);

      long now = System.currentTimeMillis();
      
      TextMessage message = session.createTextMessage();
      message.setText("normal");
      message.setLongProperty("JMS_JBOSS_SCHEDULED_DELIVERY", 0);
      sender.send(message);

      message.setText("late");
      message.setLongProperty("JMS_JBOSS_SCHEDULED_DELIVERY", now + 5000);
      sender.send(message, DeliveryMode.PERSISTENT, 10, 0);

      message.setText("early");
      message.setLongProperty("JMS_JBOSS_SCHEDULED_DELIVERY", now + 1000);
      sender.send(message, DeliveryMode.PERSISTENT, 7, 0);
      
      QueueBrowser browser = session.createBrowser( queue );
      Enumeration enum = browser.getEnumeration();
      enum.nextElement();
      if (enum.hasMoreElements())
        fail("Should only find two messages now");

      Thread.sleep(3000);

      enum = browser.getEnumeration();
      message = (TextMessage)enum.nextElement();
      if (!message.getText().equals("early"))
        throw new Exception("Queue is not scheduling messages correctly. Unexpected Message:"+message);
      enum.nextElement();
      if (enum.hasMoreElements())
        fail("Should only find three messages now");

      Thread.sleep(3000);
      
      enum = browser.getEnumeration();
      message = (TextMessage)enum.nextElement();
      if (!message.getText().equals("late"))
        throw new Exception("Queue is not scheduling messages correctly. Unexpected Message:"+message);
      enum.nextElement();
      enum.nextElement();

      disconnect();
      getLog().debug("ScheduledDelivery passed");
   }
   
   protected void setUp() throws Exception
   {
      ScheduledDeliveryUnitTestCase.TOPIC_FACTORY = "ConnectionFactory";
      ScheduledDeliveryUnitTestCase.QUEUE_FACTORY = "ConnectionFactory";
   }

   public static Test suite() throws Exception
   {
      TestSuite suite = new TestSuite();
      suite.addTest(new JBossTestSetup(new ScheduledDeliveryUnitTestCase("testScheduledDelivery")));
      return suite;
   }

   static public void main( String []args )
   {
      String newArgs[] = { "org.jboss.test.jbossmq.test.ScheduledDeliveryUnitTestCase" };
      junit.swingui.TestRunner.main(newArgs);
   }
}
