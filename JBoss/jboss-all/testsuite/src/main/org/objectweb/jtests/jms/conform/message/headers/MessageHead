/*
 * JORAM: Java(TM) Open Reliable Asynchronous Messaging
 * Copyright (C) 2002 INRIA
 * Contact: joram-team@objectweb.org
 * 
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or any later version.
 * 
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307
 * USA
 * 
 * Initial developer(s): Jeff Mesnil (jmesnil@inrialpes.fr)
 * Contributor(s): Andreas Mueller <am@iit.de>.
 */

package org.objectweb.jtests.jms.conform.message.headers;

import org.objectweb.jtests.jms.framework.PTPTestCase;
import javax.jms.*;
import junit.framework.*;
import javax.naming.* ;

/**
 * Test the headers of a message
 *
 * @author Jeff Mesnil (jmesnil@inrialpes.fr)
 * @version $Id: MessageHeaderTest.java,v 1.1 2002/04/21 21:15:18 chirino Exp $
 */
public class MessageHeaderTest extends PTPTestCase {

    /**
     * Test that the <code>MessageProducer.setPriority()</code> changes effectively
     * priority of the message.
     */
    public void testJMSPriority_2() {
	try {
	    Message message= senderSession.createMessage();
	    sender.send(message);
	    sender.setPriority(9);
	    sender.send(message);
	    assertEquals("§3.4.9 After completion of the send it holds the value specified by the " +
			 "method sending the message.\n",
			 9, message.getJMSPriority());
      
	    Message msg = receiver.receive();
	}  catch (JMSException e) {
	    fail(e);
	} 
    } 

    /**
     * Test that the priority set by <code>Message.setJMSPriority()</code> is ignored when a 
     * message is sent and that it holds the value specified when sending the message (i.e. 
     * <code>Message.DEFAULT_PRIORITY</code> in this test).
     */
    public void testJMSPriority_1() {
	try {
	    Message message= senderSession.createMessage();
	    message.setJMSPriority(0);
	    sender.send(message);
	    assertTrue("§3.4.9 When a message is sent this value is ignored.\n",
		       message.getJMSPriority() != 0);
	    assertEquals("§3.4.9 After completion of the send it holds the value specified by the " +
			 "method sending the message.\n",
			 Message.DEFAULT_PRIORITY, message.getJMSPriority());
      
	    Message msg = receiver.receive();
	}  catch (JMSException e) {
	    fail(e);
	} 
    } 
    
    /**
     * Test that the value of the <code>JMSExpiration<code> header field is the same
     * for the sent message and the received one.
     */
    public void testJMSExpiration() {
	try {
	    Message message= senderSession.createMessage();
	    sender.send(message);

	    Message msg = receiver.receive();
	    assertEquals("§3.4.9 When a message is received its JMSExpiration header field contains this same " +
			 "value [i.e. set on return of the send method].\n",
			 message.getJMSExpiration(), msg.getJMSExpiration());
	}  catch (JMSException e) {
	    fail(e);
	} 
    } 

    /**
     * Test that the <code>JMSMessageID</code> is set by the provider when the <code>send</code> method returns 
     * and that it starts with <code>"ID:"</code>.
     */
    public void testJMSMessageID_2() {
	try {
	    Message message = senderSession.createMessage();
	    sender.send(message);
	    assertTrue("§3.4.3 When the send method returns it contains a provider-assigned value.\n",
		       message.getJMSMessageID() != null);      
	    assertTrue("§3.4.3 All JMSMessageID values must start with the prefix 'ID:'.\n",
		       message.getJMSMessageID().startsWith("ID:"));      
      
	    Message msg = receiver.receive();
	    assertTrue("§3.4.3 All JMSMessageID values must start with the prefix 'ID:'.\n",
		       msg.getJMSMessageID().startsWith("ID:"));      
	} catch (JMSException e) {
	    fail(e);
	} 
    } 

    /**
     * Test that the <code>JMSMessageID</code> header field value is 
     * ignored when the message is sent.
     */
    public void testJMSMessageID_1() {
	try {
	    Message message = senderSession.createMessage();
	    message.setJMSMessageID("ID:foo");
	    sender.send(message);
	    assertTrue("§3.4.3 When a message is sent this value is ignored.\n",
		       message.getJMSMessageID() != "ID:foo");
	    Message msg = receiver.receive();
	} catch (JMSException e) {
	    fail(e);
	} 
    }
  
    /**
     * Test that the <code>JMSDeliveryMode</code> header field value is ignored
     * when the message is sent and that it holds the value specified by the sending 
     * method (i.e. <code>Message.DEFAULT_DELIVERY_MODE</code> in this test when the message is received.
     */
    public void testJMSDeliveryMode() {
	try {
	    // sender has been created with the DEFAULT_DELIVERY_MODE which is PERSISTENT
	    assertEquals(DeliveryMode.PERSISTENT, sender.getDeliveryMode());
	    Message message = senderSession.createMessage();
	    // send a message specfiying NON_PERSISTENT for the JMSDeliveryMode header field
	    message.setJMSDeliveryMode(DeliveryMode.NON_PERSISTENT);
	    sender.send(message);
	    assertTrue("§3.4.2 When a message is sent this value is ignored",
		       message.getJMSDeliveryMode() != DeliveryMode.NON_PERSISTENT);
	    assertEquals("§3.4.2 After completion of the send it holds the delivery mode specified " +
			 "by the sending method (persistent by default).\n",
			 Message.DEFAULT_DELIVERY_MODE, message.getJMSDeliveryMode());

	    Message msg = receiver.receive();
	} catch (JMSException e) {
	    fail(e);
	} 
    }

    /**
     * Test that the <code>JMSDestination</code> header field value is ignored when the message
     * is sent and that after completion of the sending method, it holds the <code>Destination</code>
     * specified by the sending method.
     * Also test that the value of the header on the received message is the same that on the sent message.
     */
    public void testJMSDestination() {
	try {
	    admin.createQueue("anotherQueue");
	    Context ctx = admin.createInitialContext();
	    Queue anotherQueue = (Queue)ctx.lookup("anotherQueue");
	    assertTrue(anotherQueue != senderQueue);

	    // set the JMSDestination header field to the anotherQueue Destination
	    Message message = senderSession.createMessage();
	    message.setJMSDestination(anotherQueue);
	    sender.send(message);
	    assertTrue("§3.4.1 When a message is sent this value is ignored.\n",
		       message.getJMSDestination() != anotherQueue);
	    assertEquals("§3.4.1 After completion of the send it holds the destination object specified " +
			 "by the sending method.\n",
			 senderQueue, message.getJMSDestination());

	    Message msg = receiver.receive();
	    assertEquals("§3.4.1 When a message is received, its destination value must be equivalent  " +
			 " to the value assigned when it was sent.\n",
			 ((Queue)message.getJMSDestination()).getQueueName(), 
			 ((Queue)msg.getJMSDestination()).getQueueName());

	    admin.deleteQueue("anotherQueue");
	} catch (JMSException e) {
	    fail(e);
	} catch (NamingException e) {
	    fail(e.getMessage());
	}
    }

    /**
     * Test that a <code>Destination</code> set by the <code>setJMSReplyTo()</code>
     * method on a sended message corresponds to the <code>Destination</code> get by 
     * the </code>getJMSReplyTo()</code> method.
     */
    public void testJMSReplyTo_1() {
	try {
	    Message message = senderSession.createMessage();
	    message.setJMSReplyTo(senderQueue);
	    sender.send(message);

	    Message msg = receiver.receive();
	    Destination dest = msg.getJMSReplyTo();
	    assertTrue("JMS ReplyTo header field should be a Queue", 
		       dest instanceof Queue);
	    Queue replyTo = (Queue) dest;
	    assertEquals("JMS ReplyTo header field should be equals to the sender queue",
			 ((Queue)replyTo).getQueueName(), ((Queue)senderQueue).getQueueName());
	} catch (JMSException e) {
	    fail(e);
	}
    }

    /** 
     * Test that if the JMS ReplyTo header field has been set as a <code>TemporaryQueue</code>,
     * it will be rightly get also as a <code>TemporaryQueue</code> 
     * (and not only as a <code>Queue</code>).
     */
    public void testJMSReplyTo_2() {
	try {
	    TemporaryQueue tempQueue = senderSession.createTemporaryQueue();
	    Message message = senderSession.createMessage();
	    message.setJMSReplyTo(tempQueue);
	    sender.send(message);
	    
	    Message msg = receiver.receive();
	    Destination dest = msg.getJMSReplyTo();
	    assertTrue("JMS ReplyTo header field should be a TemporaryQueue", 
		       dest instanceof TemporaryQueue);
	    Queue replyTo = (Queue) dest;
	    assertEquals("JMS ReplyTo header field should be equals to the temporary queue",
			 ((Queue)replyTo).getQueueName(), ((Queue)tempQueue).getQueueName());
	} catch (JMSException e) {
	    fail(e);
	}
    }    
    
    /** 
     * Method to use this class in a Test suite
     */
    public static Test suite() {
	return new TestSuite(MessageHeaderTest.class);
    }
  
    public MessageHeaderTest(String name) {
	super(name);
    }
}

