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

package org.objectweb.jtests.jms.framework;

import org.objectweb.jtests.jms.admin.*;

import junit.framework.*;
import javax.naming.*;
import javax.jms.*;

/**
 * Creates convenient Point to Point JMS objects which can be needed for tests.
 * <br />
 * This class defines the setUp and tearDown methods so
 * that JMS administrated objects and  other "ready to use" PTP objects (that is to say queues,
 * sessions, senders and receviers) are available conveniently for the test cases.
 * <br />
 * Classes which want that convenience should extend <code>PTPTestCase</code> instead of 
 * <code>JMSTestCase</code>.
 *
 * @author Jeff Mesnil (jmesnil@inrialpes.fr)
 * @version $Id: PTPTestCase.java,v 1.1 2002/04/21 21:15:19 chirino Exp $
 */
public class PTPTestCase extends JMSTestCase {

    protected Admin admin;
    protected InitialContext ctx;
    private static final String QCF_NAME = "testQCF";
    private static final String QUEUE_NAME = "testQueue";

    /**
     * Queue used by a sender
     */
    protected Queue senderQueue;
    
    /**
     * Sender on queue
     */
    protected QueueSender sender;
    
    /**
     * QueueConnectionFactory of the sender
     */
    protected QueueConnectionFactory senderQCF;
    
    /**
     * QueueConnection of the sender
     */
    protected QueueConnection senderConnection;
    
    /**
     * QueueSession of the sender (non transacted, AUTO_ACKNOWLEDGE)
     */
    protected QueueSession senderSession;
    
    /**
     * Queue used by a receiver
     */
    protected Queue receiverQueue;
    
    /**
     * Receiver on queue
     */
    protected QueueReceiver receiver;
    
    /**
     * QueueConnectionFactory of the receiver
     */
    protected QueueConnectionFactory receiverQCF;
    
    /**
     * QueueConnection of the receiver
     */
    protected QueueConnection receiverConnection;
    
    /**
     * QueueSession of the receiver (non transacted, AUTO_ACKNOWLEDGE)
     */
    protected QueueSession receiverSession; 
    
    /**
     * Create all administrated objects connections and sessions ready to use for tests.
     * <br />
     * Start connections.
     */
    protected void setUp() {
	try {
	    // Admin step
	    // gets the provider administration wrapper...
	    admin = AdminFactory.getAdmin();	    
	    // ...and creates administrated objects and binds them
	    admin.createQueueConnectionFactory(QCF_NAME);
	    admin.createQueue(QUEUE_NAME);

	    // end of admin step, start of JMS client step
	    ctx = admin.createInitialContext();

	    senderQCF = (QueueConnectionFactory)ctx.lookup(QCF_NAME);
	    senderQueue = (Queue)ctx.lookup(QUEUE_NAME);
	    senderConnection = senderQCF.createQueueConnection();
	    senderSession = senderConnection.createQueueSession(false, Session.AUTO_ACKNOWLEDGE);
	    sender = senderSession.createSender(senderQueue);

	    receiverQCF = (QueueConnectionFactory)ctx.lookup(QCF_NAME);
	    receiverQueue = (Queue)ctx.lookup(QUEUE_NAME);
	    receiverConnection = receiverQCF.createQueueConnection();
	    receiverSession = receiverConnection.createQueueSession(false, Session.AUTO_ACKNOWLEDGE);
	    receiver = receiverSession.createReceiver(receiverQueue);

	    senderConnection.start();
	    receiverConnection.start();
	    //end of client step
	} catch (Exception e) { 
	    //XXX
	    e.printStackTrace();
	}
    }
    
    /**
     *  Close connections and delete administrated objects
     */
    protected void tearDown() {
	try {
	    senderConnection.close();
	    receiverConnection.close();

	    admin.deleteQueueConnectionFactory(QCF_NAME);
	    admin.deleteQueue(QUEUE_NAME);

	    ctx.close();
	} catch (Exception e) {
	    //XXX
	    e.printStackTrace();
	} finally {
	    senderQueue = null;
	    sender = null;
	    senderQCF = null;
	    senderSession = null;
	    senderConnection = null;

	    receiverQueue = null;
	    receiver = null;
	    receiverQCF = null;
	    receiverSession = null;
	    receiverConnection = null;
	}
    }
    
    public PTPTestCase(String name) {
	super(name);
    }
}
