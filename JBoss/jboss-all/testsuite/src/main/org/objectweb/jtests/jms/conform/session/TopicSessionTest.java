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

package org.objectweb.jtests.jms.conform.session;

import org.objectweb.jtests.jms.framework.*;
import junit.framework.*;

import javax.jms.*;
import java.util.Hashtable;

/**
 * Test topic sessions
 * <br />
 * See JMS specifications, §4.4 Session
 * 
 * @author Jeff Mesnil (jmesnil@inrialpes.fr)
 * @version $Id: TopicSessionTest.java,v 1.1.4.1 2003/01/09 16:41:41 ejort Exp $
 */
public class TopicSessionTest extends PubSubTestCase {

    /**
     * Test that if we rollback a transaction which has consumed a message,
     * the message is effectively redelivered.
     */
    public void testRollbackReceivedMessage() {
	try {
	    publisherConnection.stop();
	    // publisherSession has been declared has non transacted
	    // we recreate it as a transacted session
	    publisherSession = publisherConnection.createTopicSession(true,0);
	    assertEquals(true, publisherSession.getTransacted());
	    // we also recreate the publisher
	    publisher = publisherSession.createPublisher(publisherTopic);
	    publisherConnection.start();
      
	    subscriberConnection.stop();
	    // subscriberSession has been declared has non transacted
	    // we recreate it as a transacted session
	    subscriberSession = subscriberConnection.createTopicSession(true,0);
	    assertEquals(true, subscriberSession.getTransacted());
	    // we also recreate the subscriber
	    subscriber = subscriberSession.createSubscriber(subscriberTopic);
	    subscriberConnection.start();
      
	    // we create a message...
	    TextMessage message = publisherSession.createTextMessage();
	    message.setText("testRollbackReceivedMessage");
	    // ... publish it ...
	    publisher.publish(message);
	    // ... and commit the transaction
	    publisherSession.commit();
      
	    // we receive it
	    Message msg1 = subscriber.receive(1000);
	    assertTrue("no message received", msg1 != null);
	    assertTrue(msg1 instanceof TextMessage);
	    assertEquals("testRollbackReceivedMessage", ((TextMessage)msg1).getText());
	
	    // we rollback the transaction of subscriberSession
	    subscriberSession.rollback();
      
	    // we expect to receive a second time the message
	    Message msg2 = subscriber.receive(1000);
	    assertTrue("no message received after rollbacking subscriber session.", msg2 != null);
	    assertTrue(msg2 instanceof TextMessage);
	    assertEquals("testRollbackReceivedMessage", ((TextMessage)msg2).getText());
      
      
	    // finally we commit the subscriberSession transaction
	    subscriberSession.commit();
	} catch (Exception e) {
	    fail(e);
	}
    }

    /**
     * Test that a durable subscriber effectively receives the messages sent to its
     * topic while it was inactive.
     */
    public void testDurableSubscriber() {
	try {
	    subscriber = subscriberSession.createDurableSubscriber(subscriberTopic,"testTopic");
	    subscriberConnection.close();
	    subscriberConnection = null;
      
	    TextMessage message = publisherSession.createTextMessage();
	    message.setText("test");
	    publisher.publish(message);

	    subscriberConnection = subscriberTCF.createTopicConnection();
	    if (subscriberConnection.getClientID() == null) {
		subscriberConnection.setClientID("subscriberConnection");
	    }	    
	    subscriberSession = subscriberConnection.createTopicSession(false, Session.AUTO_ACKNOWLEDGE);
	    subscriber = subscriberSession.createDurableSubscriber(subscriberTopic, "testTopic");
	    subscriberConnection.start();

	    TextMessage m = (TextMessage)subscriber.receiveNoWait();
	    assertTrue(m != null);
	    assertEquals("test", m.getText());
	} catch (JMSException e) {
	    fail(e);
	}
      finally
      {
         try
         {
            subscriberSession.unsubscribe("testTopic");
         }
         catch (JMSException e)
         {
            fail(e);
         }
      } 
    }

    /**
     * Test the unsubscription of a durable subscriber.
     */
    public void testUnsubscribe() {
	try {
	    subscriber = subscriberSession.createDurableSubscriber(subscriberTopic,"topic");
	    subscriber.close();
	    // nothing should happen when unsubscribing the durable subscriber
	    subscriberSession.unsubscribe("topic");
	} catch (JMSException e) {
	    fail(e);
	}
    }

    /**
     * Test that a call to the <code>createDurableSubscriber()</code> method with an invalid
     * message selector throws a <code>javax.jms.InvalidSelectorException</code>.
     */
    public void testCreateDurableSubscriber_2() {
	try {
	    subscriberSession.createDurableSubscriber(subscriberTopic, 
						      "topic",
						      "definitely not a message selector!",
						      true);
	    fail("Should throw a javax.jms.InvalidSelectorException.\n");
	} catch (InvalidSelectorException e) {
	} catch (JMSException e) {
	    fail("Should throw a javax.jms.InvalidSelectorException, not a "+ e);
	}
    }

    /**
     * Test that a call to the <code>createDurableSubscriber()</code> method with an invalid
     * <code>Topic</code> throws a <code>javax.jms.InvalidDestinationException</code>.
     */
    public void testCreateDurableSubscriber_1() {
	try {
	    subscriberSession.createDurableSubscriber((Topic)null, "topic");
	    fail("Should throw a javax.jms.InvalidDestinationException.\n");
	} catch (InvalidDestinationException e) {
	} catch (JMSException e) {
	    fail("Should throw a javax.jms.InvalidDestinationException, not a "+ e);
	}
    }

    /**
     * Test that a call to the <code>createSubscriber()</code> method with an invalid
     * message selector throws a <code>javax.jms.InvalidSelectorException</code>.
     */
    public void testCreateSubscriber_2() {
	try {
	    subscriberSession.createSubscriber(subscriberTopic, 
					       "definitely not a message selector!",
					       true);
	    fail("Should throw a javax.jms.InvalidSelectorException.\n");
	} catch (InvalidSelectorException e) {
	} catch (JMSException e) {
	    fail("Should throw a javax.jms.InvalidSelectorException, not a "+ e);
	}
    }

    /**
     * Test that a call to the <code>createSubscriber()</code> method with an invalid
     * <code>Topic</code> throws a <code>javax.jms.InvalidDestinationException</code>.
     */
    public void testCreateSubscriber_1() {
	try {
	    subscriberSession.createSubscriber((Topic)null);
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
	return new TestSuite(TopicSessionTest.class);
    }
  
    public TopicSessionTest(String name) {
	super(name);
    }
}
