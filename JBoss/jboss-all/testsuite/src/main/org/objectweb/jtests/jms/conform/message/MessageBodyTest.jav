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

package org.objectweb.jtests.jms.conform.message;

import org.objectweb.jtests.jms.framework.PTPTestCase;
import javax.jms.*;
import junit.framework.*;

/**
 * Tests on message body.
 *
 * @author Jeff Mesnil (jmesnil@inrialpes.fr)
 * @version $Id: MessageBodyTest.java,v 1.1 2002/04/21 21:15:18 chirino Exp $
 */
public class MessageBodyTest extends PTPTestCase {
  
  /**
   * Test that the <code>TextMessage.clearBody()</code> method does nto clear the 
   * message properties.
   */
  public void testClearBody_2() {
    try {
      TextMessage message = senderSession.createTextMessage();
      message.setStringProperty("prop", "foo");
      message.clearBody();
      assertEquals("§3.11.1 Clearing a message's body does not clear its property entries.\n",
		   "foo", message.getStringProperty("prop"));
    } catch (JMSException e) {
      fail(e);
    }
  }

  /**
   * Test that the <code>TextMessage.clearBody()</code> effectively clear the body of the message
   */
  public void testClearBody_1() {
    try {
      TextMessage message = senderSession.createTextMessage();
      message.setText("bar");
      message.clearBody();
      assertEquals("§3 .11.1 the clearBody method of Message resets the value of the message body " +
		   "to the 'empty' initial message value as set by the message type's create " +
		   "method provided by Session.\n",
		   null, message.getText());
    } catch (JMSException e) {
      fail(e);
    }
  }

  /**
   * Test that a call to the <code>TextMessage.setText()</code> method on a 
   * received message raises a <code>javax.jms.MessageNotWriteableException</code>.
   */
  public void testWriteOnReceivedBody() {
    try {
      TextMessage message = senderSession.createTextMessage();
      message.setText("foo");
      sender.send(message);
      
      Message m = receiver.receive();
      assertTrue("The message should be an instance of TextMessage.\n",
		 m instanceof TextMessage);
      TextMessage msg = (TextMessage)m;
      msg.setText("bar");
      fail("should raise a MessageNotWriteableException (§3.11.2)");
    } catch (MessageNotWriteableException e) {
    } catch (JMSException e) {
      fail(e);
    }
  }
    
  /** 
   * Method to use this class in a Test suite
   */
  public static Test suite() {
     return new TestSuite(MessageBodyTest.class);
   }
  
  public MessageBodyTest(String name) {
    super(name);
  }
}
