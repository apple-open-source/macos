/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.test.jbossmq.test;

import java.util.Properties;

import javax.jms.ExceptionListener;
import javax.jms.JMSException;
import javax.jms.QueueConnection;
import javax.jms.QueueConnectionFactory;
import javax.jms.QueueReceiver;
import javax.jms.QueueSender;
import javax.jms.QueueSession;
import javax.jms.Session;
import javax.jms.TemporaryQueue;
import javax.jms.TextMessage;

import org.jboss.mq.SpyConnectionFactory;
import org.jboss.mq.il.oil.OILServerILFactory;
import org.jboss.test.JBossTestCase;

/** 
 * A that a large message doesn't get in the way of ping/pong
 *
 * @author Adrian@jboss.org
 * @version $Revision: 1.1.4.5 $
 */
public class LargeMessageUnitTestCase extends JBossTestCase implements ExceptionListener
{

   private Exception failed = null;

   public LargeMessageUnitTestCase(String name)
   {
      super(name);
   }

   protected void setUp() throws Exception
   {
   }

   public void testOILLargeMessage() throws Exception
   {
      Properties props = new Properties();
      props.setProperty(OILServerILFactory.SERVER_IL_FACTORY_KEY, OILServerILFactory.SERVER_IL_FACTORY);
      props.setProperty(OILServerILFactory.CLIENT_IL_SERVICE_KEY, OILServerILFactory.CLIENT_IL_SERVICE);
      props.setProperty(OILServerILFactory.PING_PERIOD_KEY, "60000");
      props.setProperty(OILServerILFactory.OIL_ADDRESS_KEY, "localhost");
      props.setProperty(OILServerILFactory.OIL_PORT_KEY, "8090");

      runTest(props);
   }
   /*
      public void testUILLargeMessage() throws Exception
      {
         Properties props = new Properties();
         props.setProperty(UILServerILFactory.SERVER_IL_FACTORY_KEY, UILServerILFactory.SERVER_IL_FACTORY);
         props.setProperty(UILServerILFactory.CLIENT_IL_SERVICE_KEY, UILServerILFactory.CLIENT_IL_SERVICE);
         props.setProperty(UILServerILFactory.PING_PERIOD_KEY, "60000");
         props.setProperty(UILServerILFactory.UIL_ADDRESS_KEY, "localhost");
         props.setProperty(UILServerILFactory.UIL_PORT_KEY, "8091");
   
         runTest(props);
      }
   */
   public void testUIL2LargeMessage() throws Exception
   {
      Properties props = new Properties();
      props.setProperty(
         org.jboss.mq.il.uil2.UILServerILFactory.SERVER_IL_FACTORY_KEY,
         org.jboss.mq.il.uil2.UILServerILFactory.SERVER_IL_FACTORY);
      props.setProperty(
         org.jboss.mq.il.uil2.UILServerILFactory.CLIENT_IL_SERVICE_KEY,
         org.jboss.mq.il.uil2.UILServerILFactory.CLIENT_IL_SERVICE);
      props.setProperty(org.jboss.mq.il.uil2.UILServerILFactory.PING_PERIOD_KEY, "60000");
      props.setProperty(org.jboss.mq.il.uil2.UILServerILFactory.UIL_ADDRESS_KEY, "localhost");
      props.setProperty(org.jboss.mq.il.uil2.UILServerILFactory.UIL_PORT_KEY, "8093");
      props.setProperty(org.jboss.mq.il.uil2.UILServerILFactory.UIL_BUFFERSIZE_KEY, "1");
      props.setProperty(org.jboss.mq.il.uil2.UILServerILFactory.UIL_CHUNKSIZE_KEY, "10000");

      runTest(props);
   }

   public void runTest(Properties props) throws Exception
   {
      QueueConnectionFactory cf = new SpyConnectionFactory(props);
      QueueConnection c = cf.createQueueConnection();
      TemporaryQueue queue = null;
      try
      {
         failed = null;
         c.setExceptionListener(this);
         c.start();
         QueueSession session = c.createQueueSession(false, Session.AUTO_ACKNOWLEDGE);
         queue = session.createTemporaryQueue();
         QueueSender sender = session.createSender(queue);
         char[] chars = new char[1000000];
         for (int i = 0; i < chars.length - 1; ++i)
            chars[i] = 'a';
         chars[chars.length - 1] = 0;
         TextMessage message = session.createTextMessage(new String(chars));
         sender.send(message);

         QueueReceiver receiver = session.createReceiver(queue);
         assertTrue("No message?", receiver.receiveNoWait() != null);

         assertTrue("We should not get a ping exception because it should pong every chunk: " + failed, failed == null);
      }
      finally
      {
         c.close();
      }
   }

   public void onException(JMSException e)
   {
      failed = e;
   }

   public static void main(java.lang.String[] args)
   {
      junit.textui.TestRunner.run(LargeMessageUnitTestCase.class);
   }
}
