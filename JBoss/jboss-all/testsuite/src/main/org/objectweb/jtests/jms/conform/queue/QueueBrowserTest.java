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
import java.util.Enumeration;

/**
 * Test the <code>javax.jms.QueueBrowser</code> features.
 *
 * @author Jeff Mesnil (jmesnil@inrialpes.fr)
 * @version $Id: QueueBrowserTest.java,v 1.1 2002/04/21 21:15:19 chirino Exp $
 */
public class QueueBrowserTest extends PTPTestCase {

  /**
   * The <code>QueueBrowser</code> of the receiver's session
   */
  protected QueueBrowser receiverBrowser;

  /**
   * The <code>QueueBrowser</code> of the sender's session
   */
  protected QueueBrowser senderBrowser ;

  /**
   * Test the <code>QueueBrowser</code> of the sender.
   */
  public void testSenderBrowser() {
    try {
      TextMessage message_1 = senderSession.createTextMessage();
      message_1.setText("testBrowser:message_1");
      TextMessage message_2 = senderSession.createTextMessage();
      message_2.setText("testBrowser:message_2");

      // send two messages...
      sender.send(message_1);
      sender.send(message_2);
      
      // ask the browser to browse the sender's session
      Enumeration enumeration = senderBrowser.getEnumeration();
      int count = 0;
      while (enumeration.hasMoreElements()) {
	// one more message in the queue
	count ++;
	// check that the message in the queue is one of the two which where sent
	Object obj = enumeration.nextElement();
	assertTrue(obj instanceof TextMessage);
	TextMessage msg = (TextMessage)obj;
	assertTrue(msg.getText().startsWith("testBrowser:message_"));
      }
      // check that there is effectively 2 messages in the queue
      assertEquals(2, count);

      // receive the first message...
      Message m = receiver.receive();
      // ... and check it is the first which was sent.
      assertTrue (m instanceof TextMessage);
      TextMessage msg = (TextMessage)m;
      assertEquals("testBrowser:message_1", msg.getText());

      // receive the second message...
      m = receiver.receive();
      // ... and check it is the second which was sent.
      assertTrue (m instanceof TextMessage);
      msg = (TextMessage)m;
      assertEquals("testBrowser:message_2", msg.getText());

      // ask the browser to browse the sender's session
      enumeration = receiverBrowser.getEnumeration();
      // check that there is no messages in the queue
      // (the two messages have been acknowledged and so removed
      // from the queue)
      assertTrue(!enumeration.hasMoreElements());
    } catch (JMSException e) {
      fail(e);
    }
  }

    /**
   * Test that a <code>QueueBrowser</cdeo> created with a message selector
   * browses only the messages matching this selector.
   */
  public void testBrowserWithMessageSelector() {
    try {
      senderBrowser = senderSession.createBrowser(senderQueue, "pi = 3.14159");

      TextMessage message_1 = senderSession.createTextMessage();
      message_1.setText("testBrowserWithMessageSelector:message_1");
      TextMessage message_2 = senderSession.createTextMessage();
      message_2.setDoubleProperty("pi", 3.14159);
      message_2.setText("testBrowserWithMessageSelector:message_2");

      sender.send(message_1);
      sender.send(message_2);

      Enumeration enumeration = senderBrowser.getEnumeration();
      int count = 0;
      while (enumeration.hasMoreElements()) {
	count ++;
	Object obj = enumeration.nextElement();
	assertTrue(obj instanceof TextMessage);
	TextMessage msg = (TextMessage)obj;
	assertEquals("testBrowserWithMessageSelector:message_2", msg.getText());
      }
      assertEquals(1, count);
    } catch (JMSException e) {
      fail(e);
    }
  }

  public void setUp() {
    try {
      super.setUp();
      receiverBrowser = receiverSession.createBrowser(receiverQueue);
      senderBrowser = senderSession.createBrowser(senderQueue);
    } catch (JMSException e) {
      e.printStackTrace();
    }
  }

  public void tearDown() {
    try {
      receiverBrowser.close();
      senderBrowser.close();
      super.tearDown();
    } catch (JMSException e) {
      e.printStackTrace();
    } finally {
      receiverBrowser = null;
      senderBrowser = null;
    }
  }  

  /** 
   * Method to use this class in a Test suite
   */
  public static Test suite() {
    return new TestSuite(QueueBrowserTest.class);
  }
  
  public QueueBrowserTest(String name) {
    super(name);
  }
}
