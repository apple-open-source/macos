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

package org.objectweb.jtests.jms.conform.queue;

import org.objectweb.jtests.jms.framework.*;
import junit.framework.*;

import javax.jms.*;


/**
 * Test the <code>javax.jms.TemporaryQueue</code> features.
 *
 * @author Jeff Mesnil (jmesnil@inrialpes.fr)
 * @version $Id: TemporaryQueueTest.java,v 1.1 2002/04/21 21:15:19 chirino Exp $
 */
public class TemporaryQueueTest extends PTPTestCase {

  private TemporaryQueue tempQueue;
  private QueueReceiver tempReceiver;

  /**
   * Test a TemporaryQueue
   */
  public void testTemporaryQueue() {
    try {
      // we stop both sender and receiver connections
      senderConnection.stop();
      receiverConnection.stop();
      // we create a temporary queue to receive messages
      tempQueue = receiverSession.createTemporaryQueue();
      // we recreate the sender because it has been 
      // already created with a Destination as parameter
      sender = senderSession.createSender(null);
      // we create a receiver on the temporary queue
      tempReceiver = receiverSession.createReceiver(tempQueue);
      receiverConnection.start();
      senderConnection.start();

      TextMessage message = senderSession.createTextMessage();
      message.setText("testTemporaryQueue");
      sender.send(tempQueue, message);

      Message m = tempReceiver.receive();
      assertTrue(m instanceof TextMessage);
      TextMessage msg = (TextMessage)m;
      assertEquals("testTemporaryQueue", msg.getText());
    } catch (JMSException e) {
      fail(e);
    }
  }

  /** 
   * Method to use this class in a Test suite
   */
  public static Test suite() {
    return new TestSuite(TemporaryQueueTest.class);
  }
  
  public TemporaryQueueTest(String name) {
    super(name);
  }
}
