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
 * Contributor(s): ______________________________________.
 */

package org.objectweb.jtests.jms.conform.session;

import org.objectweb.jtests.jms.framework.*;
import junit.framework.*;

import javax.jms.*;

/**
 * Test queue sessions
 * <br />
 * See JMS specifications, §4.4 Session
 * 
 * @author Jeff Mesnil (jmesnil@inrialpes.fr)
 * @version $Id: QueueSessionTest.java,v 1.1 2002/04/21 21:15:19 chirino Exp $
 */
public class QueueSessionTest extends PTPTestCase {

    /**
     * Test that if we rollback a transaction which has consumed a message,
     * the message is effectively redelivered.
     */
    public void testRollbackRececeivedMessage() {
	try {
	    senderConnection.stop();
	    // senderSession has been created as non transacted
	    // we create it again but as a transacted session
	    senderSession = senderConnection.createQueueSession(true, 0);
	    assertEquals(true, senderSession.getTransacted());
	    // we create again the sender
	    sender = senderSession.createSender(senderQueue);
	    senderConnection.start();

	    receiverConnection.stop();
	    // receiverSession has been created as non transacted
	    // we create it again but as a transacted session
	    receiverSession = receiverConnection.createQueueSession(true,0);
	    assertEquals(true, receiverSession.getTransacted());
	    // we create again the receiver
	    receiver = receiverSession.createReceiver(receiverQueue);
	    receiverConnection.start();

	    // we send a message...
	    TextMessage message = senderSession.createTextMessage();
	    message.setText("testRollbackRececeivedMessage");
	    sender.send(message);
	    // ... and commit the *producer* transaction
	    senderSession.commit();

	    // we receive a message...
	    Message m = receiver.receiveNoWait();
	    assertTrue(m != null);
	    assertTrue(m instanceof TextMessage);
	    TextMessage msg = (TextMessage)m;
	    // ... which is the one which was sent...
	    assertEquals("testRollbackRececeivedMessage", msg.getText());
	    // ...and has not been redelivered
	    assertEquals(false, msg.getJMSRedelivered());

	    // we rollback the *consumer* transaction
	    receiverSession.rollback();

	    // we receive again a message
	    m = receiver.receiveNoWait();
	    assertTrue(m != null);
	    assertTrue(m instanceof TextMessage);
	    msg = (TextMessage)m;
	    // ... which is still the one which was sent...
	    assertEquals("testRollbackRececeivedMessage", msg.getText());
	    // .. but this time, it has been redelivered
	    assertEquals(true, msg.getJMSRedelivered());
      
	} catch (Exception e) {
	    fail(e);
	}
    }    

    /**
     * Test that a call to the <code>createBrowser()</code> method with an invalid
     * messaeg session throws a <code>javax.jms.InvalidSelectorException</code>.
     */
    public void testCreateBrowser_2() {
	try {
	    QueueBrowser browser = senderSession.createBrowser(senderQueue,
							       "definitely not a message selector!");
	    fail("Should throw a javax.jms.InvalidSelectorException.\n");
	} catch (InvalidSelectorException e) {
	} catch (JMSException e) {
	    fail("Should throw a javax.jms.InvalidSelectorException, not a "+ e);
	}
    }

    /**
     * Test that a call to the <code>createBrowser()</code> method with an invalid
     * <code>Queue</code> throws a <code>javax.jms.InvalidDestinationException</code>.
     */
    public void testCreateBrowser_1() {
	try {
	    QueueBrowser browser = senderSession.createBrowser((Queue)null);
	    fail("Should throw a javax.jms.InvalidDestinationException.\n");
	} catch (InvalidDestinationException e) {
	} catch (JMSException e) {
	    fail("Should throw a javax.jms.InvalidDestinationException, not a "+ e);
	}
    }

    /**
     * Test that a call to the <code>createReceiver()</code> method with an invalid
     * message selector throws a <code>javax.jms.InvalidSelectorException</code>.
     */
    public void testCreateReceiver_2() {
	try {
	    receiver = senderSession.createReceiver(senderQueue,
						    "definitely not a message selector!");
	    fail("Should throw a javax.jms.InvalidSelectorException.\n");
	} catch (InvalidSelectorException e) {
	} catch (JMSException e) {
	    fail("Should throw a javax.jms.InvalidSelectorException, not a "+ e);
	}
    }

    /**
     * Test that a call to the <code>createReceiver()</code> method with an invalid
     * <code>Queue</code> throws a <code>javax.jms.InvalidDestinationException</code>>
     */
    public void testCreateReceiver_1() {
	try {
	    receiver = senderSession.createReceiver((Queue)null);
	    fail("Should throw a javax.jms.InvalidDestinationException.\n");
	} catch (InvalidDestinationException e) {
	} catch (JMSException e) {
	    fail("Should throw a javax.jms.InvalidDestinationException, not a "+ e);
	}
    }

    /** 
     * Method to use this class in a Test suite
     */
    public static Test suite() {
	return new TestSuite(QueueSessionTest.class);
    }
  
    public QueueSessionTest(String name) {
	super(name);
    }
}
