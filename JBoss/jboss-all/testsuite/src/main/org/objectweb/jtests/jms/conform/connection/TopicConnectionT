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
 * Contributor(s): Andreas Mueller <am@iit.de>
 */

package org.objectweb.jtests.jms.conform.connection;

import org.objectweb.jtests.jms.framework.*;
import javax.jms.*;
import junit.framework.*;

/**
 * Test topic-specific connection features.
 *
 * Test setting of client ID which is relevant only for Durable Subscribtion
 */

public class TopicConnectionTest extends PubSubTestCase {
    
    /**
     * Test that a call to <code>setClientID</code> will throw an 
     * <code>IllegalStateException</code> if a client ID has already been set
     * see JMS javadoc 
     * http://java.sun.com/j2ee/sdk_1.3/techdocs/api/javax/jms/Connection.html#setClientID(java.lang.String)
     */
    public void testSetClientID_1() {
	try {
	    // we start from a clean state for the connection
	    subscriberConnection.close();
	    subscriberConnection = null;
	    
	    subscriberConnection = subscriberTCF.createTopicConnection();
	    // if the JMS provider does not set a client ID, we do.
	    if (subscriberConnection.getClientID() == null) {
		subscriberConnection.setClientID("testSetClientID_1");
		assertEquals("testSetClientID_1", subscriberConnection.getClientID());
	    }
	    // now the connection has a client ID (either "testSetClientID_1" or one set by the provider
	    assertTrue(subscriberConnection.getClientID() != null);
	    
	    // a attempt to set a client ID should now throw an IllegalStateException
	    subscriberConnection.setClientID("another client ID");
	    fail("Should raise a javax.jms.IllegalStateException");
	} catch (javax.jms.IllegalStateException e) {
	} catch (JMSException e) {
	    fail("Should raise a javax.jms.IllegalStateException, not a "+ e);
	} catch (java.lang.IllegalStateException e) {
	    fail("Should raise a javax.jms.IllegalStateException, not a java.lang.IllegalStateException");
	}
    }

    /** 
     * Test that a call to <code>setClientID</code> can occur only after connection creation 
     * and before any other action on the connection.
     * <em>This test is relevant only if the ID is set by the JMS client</em>
     * see JMS javadoc 
     * http://java.sun.com/j2ee/sdk_1.3/techdocs/api/javax/jms/Connection.html#setClientID(java.lang.String)
     */
    public void testSetClientID_2() {
	try {
	    // we start from a clean state for the first connection
	    subscriberConnection.close();
	    subscriberConnection = null;

	    subscriberConnection = subscriberTCF.createTopicConnection();
	    // if the JMS provider has set a client ID, this test is not relevant
	    if (subscriberConnection.getClientID() != null) {
		return;
	    }

	    //we start the connection 
	    subscriberConnection.start();
	    
	    // an attempt to set the client ID now should throw a IllegalStateException
	    subscriberConnection.setClientID("testSetClientID_2");
	    fail("Should throw a javax.jms.IllegalStateException");
	} catch (javax.jms.IllegalStateException e) {
	} catch (JMSException e) {
	    fail("Should raise a javax.jms.IllegalStateException, not a "+ e);
	} catch (java.lang.IllegalStateException e) {
	    fail("Should raise a javax.jms.IllegalStateException, not a java.lang.IllegalStateException");
	}
    }
	
    /**
     * Test that if another connection with the same clientID is already running when 
     * <code>setClientID</code> is called, the JMS provider should detect the duplicate
     * ID and throw an <code>InvalidClientIDException</code>
     * <em>This test is relevant only if the ID is set by the JMS client</em>
     * see JMS javadoc 
     * http://java.sun.com/j2ee/sdk_1.3/techdocs/api/javax/jms/Connection.html#setClientID(java.lang.String)
     */
    public void testSetClientID_3() {
	try {
	    // we start from a clean state for the first connection
	    subscriberConnection.close();
	    subscriberConnection = null;

	    subscriberConnection = subscriberTCF.createTopicConnection();
	    // if the JMS provider has set a client ID, this test is not relevant
	    if (subscriberConnection.getClientID() != null) {
		return;
	    }
	    // the JMS provider has not set a client ID, so we do
	    subscriberConnection.setClientID("testSetClientID_3");
	    assertEquals("testSetClientID_3", subscriberConnection.getClientID());

	    // we create a new connection and try to set the same ID than for subscriberConnection
	    TopicConnection connection_2 = subscriberTCF.createTopicConnection();
	    assertTrue(connection_2.getClientID() == null);
	    connection_2.setClientID("testSetClientID_3");
	    fail("Should throw a javax.jms.InvalidClientIDException");
	} catch (InvalidClientIDException e) {
	} catch (JMSException e) {
	    fail(e);
	}
    }

    /** 
     * Method to use this class in a Test suite
     */
    public static Test suite() {
	return new TestSuite(TopicConnectionTest.class);
    }
  
    public TopicConnectionTest(String name) {
	super(name);
    }
}



