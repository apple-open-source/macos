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

package org.objectweb.jtests.jms.conform.connection;

import org.objectweb.jtests.jms.framework.*;
import javax.jms.*;
import junit.framework.*;

/**
 * Test connections.
 * 
 * See JMS specifications, §4.3.5 Closing a Connection
 *
 * @author Jeff Mesnil (jmesnil@inrialpes.fr)
 * @version $Id: ConnectionTest.java,v 1.1 2002/04/21 21:15:18 chirino Exp $
 */
public class ConnectionTest extends PTPTestCase {
  
    /**
     * Test that invoking the <code>acknowledge()</code> method of a received message 
     * from a closed connection's session must throw an <code>IllegalStateException</code>.
     */
    public void testAcknowledge() {
	try {
	    receiverConnection.stop();
	    receiverSession = receiverConnection.createQueueSession(false, Session.CLIENT_ACKNOWLEDGE);
	    receiver = receiverSession.createReceiver(receiverQueue);
	    receiverConnection.start();

	    Message message = senderSession.createMessage();
	    sender.send(message);

	    Message m = receiver.receive();
	    receiverConnection.close();
	    m.acknowledge();
	    fail("§4.3.5 Invoking the acknowledge method of a received message from a closed "+
		 "connection's session must throw a [javax.jms.]IllegalStateException.\n");
	} catch (javax.jms.IllegalStateException e) {
	} catch (JMSException e) {
	    fail("§4.3.5 Invoking the acknowledge method of a received message from a closed "+
		 "connection's session must throw a [javax.jms.]IllegalStateException, not a "+ e);
	} catch (java.lang.IllegalStateException e) {
	    fail("§4.3.5 Invoking the acknowledge method of a received message from a closed "+
		 "connection's session must throw an [javax.jms.]IllegalStateException "+
		 "[not a java.lang.IllegalStateException]");
	}
    }
    
    /**
     * Test that an attempt to use a <code>Connection</code> which has been closed 
     * throws a <code>javax.jms.IllegalStateException</code>.
     */
    public void testUseClosedConnection() {
	try {
	    senderConnection.close();
	    senderConnection.createQueueSession(false, Session.AUTO_ACKNOWLEDGE);
	    fail("Should raise a javax.jms.IllegalStateException");
	} catch (javax.jms.IllegalStateException e) {
	} catch (JMSException e) {
	    fail("Should raise a javax.jms.IllegalStateException, not a "+ e);
	} catch (java.lang.IllegalStateException e) {
	    fail("Should raise a javax.jms.IllegalStateException, not a java.lang.IllegalStateException");
	}
    }

    /**
     * Test that a <code>MessageProducer</code> can send messages while a 
     * <code>Connection</code> is stopped.
     */
    public void testMessageSentWhenConnectionClosed() {
	try {
	    senderConnection.stop();
	    Message message = senderSession.createTextMessage();
	    sender.send(message);
      
	    Message m = receiver.receive();
	} catch (JMSException e) {
	    fail(e);
	}
    }
  

    /**
     * Test that closing a closed connectiondoes not throw an exception.
     */
    public void testCloseClosedConnection() {
	try {
	    // senderConnection is already started
	    // we close it once
	    senderConnection.close();
	    // we close it a second time
	    senderConnection.close();
	} catch (Exception e) {
	    fail("§4.3.5 Closing a closed connection must not throw an exception.\n");
	}
    }
  
    /**
     * Test that starting a started connection is ignored
     */
    public void testStartStartedConnection() {
	try {
	    // senderConnection is already started
	    // start it again should be ignored
	    senderConnection.start();
	} catch (JMSException e) {
	    fail(e);
	}
    } 

    /**
     * Test that stopping a stopped connection is ignored
     */
    public void testStopStoppedConnection() {
	try {
	    // senderConnection is started
	    // we stop it once
	    senderConnection.stop();
	    // stopping it a second time is ignored
	    senderConnection.stop();
	} catch (JMSException e) {
	    fail(e);
	}
    } 

    /**
     * Test that delivery of message is stopped if the message consumer connection is stopped
     */
    public void testStopConsumerConnection() {
	try {
	    receiverConnection.stop();
      
	    receiver.setMessageListener(new MessageListener() {
		    public void onMessage(Message m) {
			try {
			    fail("The message must not be received, the consumer connection is stopped");
			    assertEquals("test", ((TextMessage)m).getText());
			} catch (JMSException e) {
			    fail(e);
			} 
		    }
		});
      
	    TextMessage message = senderSession.createTextMessage();
	    message.setText("test");
	    sender.send(message);
	    synchronized(this) {
		try {
		    wait(1000);
		} catch (Exception e) {
		    fail (e);
		}
	    }
	} catch (JMSException e) {
	    fail(e);
	}
    } 
  
    /** 
     * Method to use this class in a Test suite
     */
    public static Test suite() {
	return new TestSuite(ConnectionTest.class);
    }
  
    public ConnectionTest(String name) {
	super(name);
    }
}
