/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.test.jbossmq.test;

import javax.naming.*;
import javax.jms.*;
import java.util.*;
import junit.framework.*;

import org.apache.log4j.Category;

import org.jboss.test.*;

/**
 * Tests message bodies.
 *
 * @author Loren Rosen (submitted patch)
 * @author <a href="mailto:jason@planet57.com">Jason Dillon</a>
 * @version $Revision: 1.2 $
 */
public class MessageBodyUnitTestCase
   extends JBossTestCase
{
   // Provider specific
   public static final String QUEUE_FACTORY = "ConnectionFactory";
   public static final String TEST_QUEUE = "queue/testQueue";

   Context context;
   QueueConnection queueConnection; 
   QueueSession session;
   Queue queue;

   QueueReceiver receiver;
   QueueSender sender;

   Category log;
   
   public MessageBodyUnitTestCase(String name) throws Exception {
      super(name);
      log = getLog();
   }

   protected void setUp() throws Exception  {
      connect();
   }
	
   protected void tearDown() throws Exception {
      disconnect();
   }

   protected void connect() throws Exception {
      log.debug("connecting");
      if (context == null) {
         context = getInitialContext();
      }

      QueueConnectionFactory queueFactory = (QueueConnectionFactory) context.lookup(QUEUE_FACTORY);
      queueConnection = queueFactory.createQueueConnection();
      log.debug("connected");

      queueConnection.start();
      session = queueConnection.createQueueSession(false, Session.AUTO_ACKNOWLEDGE);
      log.debug("session established");

      queue = (Queue)context.lookup(TEST_QUEUE);

      receiver = session.createReceiver(queue);	
      sender = session.createSender(queue);
      log.debug("sender established");

      drainQueue();
      log.debug("end of connect call");
   }

   protected void disconnect() throws Exception {
      queueConnection.close();
   }

   private void drainQueue() throws Exception {
      log.debug("draining queue");
		
      Message message = receiver.receive(2000);
      int c=0;
      while (message != null) {
         message = receiver.receive(2000);
         c++;
      }
		
      if (c!=0)
         log.debug("Drained "+c+" messages from the queue");
		
      log.debug("drained queue");
		
   }

   protected void validate(String payload) throws Exception{
      log.debug("validating text |" + payload + "|");

      TextMessage outMessage = session.createTextMessage();
      outMessage.setText (payload);
      log.debug("sending |" + payload + "|");
      sender.send (outMessage);

      log.debug("receiving |" + payload + "|");
      TextMessage inMessage = (TextMessage) receiver.receive();
      log.debug("received |" + payload + "|");
      String inPayload  = inMessage.getText();

      assertEquals ("Message body text test", payload, inPayload);
      log.debug("validated text " + payload);
   }

   public void testTextMessageBody() throws Exception {
      log.debug("testing text");

      validate ("ordinary text");
      validate (" ");
      validate ("");
      // TBD: very long strings, non-printable ASCII strings
      // TBD: Unicode non-ASCII strings
      log.debug("tested text");
   }


   protected void validate (java.io.Serializable payload) throws Exception {
      ObjectMessage outMessage = session.createObjectMessage();
      outMessage.setObject (payload);
      sender.send (outMessage);

      ObjectMessage inMessage = (ObjectMessage) receiver.receive();
      Object inPayload  = inMessage.getObject();

      assertEquals("Message body object test", payload, inPayload);
   }

   public void testObjectMessageBody() throws Exception {
      log.debug("testing object");
      validate(new Integer(0));
      validate(new Integer(1));
      validate(new Integer(-1));
      validate(new Integer(Integer.MAX_VALUE));
      validate(new Integer(Integer.MIN_VALUE));
      validate(new Integer(-1));
      validate(new Float(1.0));
      validate(new Float(0.0));
      validate(new Float(-1.0));
      validate(new Float(Float.MAX_VALUE));
      validate(new Float(Float.MIN_VALUE));
      validate(new Float(Float.NaN));
      validate(new Float(Float.POSITIVE_INFINITY));
      validate(new Float(Float.NEGATIVE_INFINITY));
      validate(new Float(1.0));

      // TBD: more complicated objects
   }
}
