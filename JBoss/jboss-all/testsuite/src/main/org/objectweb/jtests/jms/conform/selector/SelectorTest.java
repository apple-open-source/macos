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

package org.objectweb.jtests.jms.conform.selector;

import org.objectweb.jtests.jms.framework.*;
import junit.framework.*;

import javax.jms.*;

/**
 * Test the message selector features of JMS
 *
 * @author Jeff Mesnil (jmesnil@inrialpes.fr)
 * @version $Id: SelectorTest.java,v 1.1 2002/04/21 21:15:19 chirino Exp $
 */
public class SelectorTest extends PTPTestCase {

  /**
   * Test the message selector using the filter example provided by the JMS specifications
   * <br />
   * <ul>
   *  <li><code>"JMSType = 'car' AND color = 'blue' AND weight > 2500"</code></li>
   * </ul>
   */
  public void testSelectorExampleFromSpecs() {
    try {
      receiverConnection.stop();
      receiver  = receiverSession.createReceiver(receiverQueue, "JMSType = 'car' AND color = 'blue' AND weight > 2500");
      receiverConnection.start();

      TextMessage dummyMessage = senderSession.createTextMessage();
      dummyMessage.setJMSType("car");
      dummyMessage.setStringProperty("color", "red");
      dummyMessage.setLongProperty("weight", 3000);
      dummyMessage.setText("testSelectorExampleFromSpecs:1");
      sender.send(dummyMessage);
      
      TextMessage message = senderSession.createTextMessage();
      message.setJMSType("car");
      message.setStringProperty("color", "blue");
      message.setLongProperty("weight", 3000);
      message.setText("testSelectorExampleFromSpecs:2");
      sender.send(message);

      TextMessage msg = (TextMessage)receiver.receive();
      assertEquals("testSelectorExampleFromSpecs:2", msg.getText());
    } catch (JMSException e) {
       e.printStackTrace();
      fail(e);
    }
  }

  /**
   * Test the ">" condition in message selector
   * <br />
   * <ul>
   *  <li><code>"weight > 2500"</code></li>
   * </ul>
   */
  public void testGreaterThan() {
    try {
      receiverConnection.stop();
      receiver  = receiverSession.createReceiver(receiverQueue, "weight > 2500");
      receiverConnection.start();
      
      TextMessage dummyMessage = senderSession.createTextMessage();
      dummyMessage.setLongProperty("weight", 1000);
      dummyMessage.setText("testGreaterThan:1");
      sender.send(dummyMessage);
      
      TextMessage message = senderSession.createTextMessage();
      message.setLongProperty("weight", 3000);
      message.setText("testGreaterThan:2");
      sender.send(message);
      
      TextMessage msg = (TextMessage)receiver.receive();
      assertEquals("testGreaterThan:2", msg.getText());
    } catch (JMSException e) {
      fail(e);
    }
  }

  /**
   * Test the "=" condition in message selector
   * <br />
   * <ul>
   *  <li><code>"weight > 2500"</code></li>
   * </ul>
   */
   public void testEquals() {
     try {
       receiverConnection.stop();
       receiver  = receiverSession.createReceiver(receiverQueue, "weight = 2500");
       receiverConnection.start();
   
       TextMessage dummyMessage = senderSession.createTextMessage();
       dummyMessage.setLongProperty("weight", 1000);
       dummyMessage.setText("testEquals:1");
       sender.send(dummyMessage);
     
       TextMessage message = senderSession.createTextMessage();
       message.setLongProperty("weight", 2500);
       message.setText("testEquals:2");
       sender.send(message);
       
       TextMessage msg = (TextMessage)receiver.receive();
       assertEquals("testEquals:2", msg.getText());
     } catch (JMSException e) {
       fail(e);
     }
   }

  /**
   * Test the BETWEEN condition
   * <br />
   * "age BETWEEN 15 and 19" is <code>true</code> for 17 and <code>false</code> for 20
   */
  public void testBetween() {
    try {
      receiverConnection.stop();
      receiver  = receiverSession.createReceiver(receiverQueue, "age BETWEEN 15 and 19");
      receiverConnection.start();

      TextMessage dummyMessage = senderSession.createTextMessage();
      dummyMessage.setIntProperty("age", 20);
      dummyMessage.setText("testBetween:1");
      sender.send(dummyMessage);

      TextMessage message = senderSession.createTextMessage();
      message.setIntProperty("age", 17);
      message.setText("testBetween:2");
      sender.send(message);

      TextMessage msg = (TextMessage)receiver.receive();
      assertTrue("Message not received",
		 msg != null);
      assertTrue("Message of another test: "+ msg.getText(),
		 msg.getText().startsWith("testBetween"));
      assertEquals("testBetween:2", msg.getText());
      
    } catch (JMSException e) {
      fail(e);
    }
  }

  /**
   * Test the IN condition
   * <br />
   * "Country IN ('UK', 'US', 'France')" is <code>true</code> for 'UK' and <code>false</code> for 'Peru'
   */
  public void testIn() {
    try {
      receiverConnection.stop();
      receiver  = receiverSession.createReceiver(receiverQueue, "Country IN ('UK', 'US', 'France')");
      receiverConnection.start();

      TextMessage dummyMessage = senderSession.createTextMessage();
      dummyMessage.setStringProperty("Country", "Peru");
      dummyMessage.setText("testIn:1");
      sender.send(dummyMessage);
  
      TextMessage message = senderSession.createTextMessage();
      message.setStringProperty("Country", "UK");
      message.setText("testIn:2");
      sender.send(message);
  
      TextMessage msg = (TextMessage)receiver.receive();
      assertTrue("Message not received",
		 msg != null);
      assertTrue("Message of another test: "+ msg.getText(),
		 msg.getText().startsWith("testIn"));
      assertEquals("testIn:2", msg.getText());
      
    } catch (JMSException e) {
      fail(e);
    }
  }
  
  /**
   * Test the LIKE ... ESCAPE condition
   * <br />
   * "underscored LIKE '\_%' ESCAPE '\'" is <code>true</code> for '_foo' and <code>false</code> for 'bar'
   */
  public void testLikeEscape() {
    try {
      receiverConnection.stop();
      receiver  = receiverSession.createReceiver(receiverQueue, "underscored LIKE '\\_%' ESCAPE '\\'");
      receiverConnection.start();

      TextMessage dummyMessage = senderSession.createTextMessage();
      dummyMessage.setStringProperty("underscored", "bar");
      dummyMessage.setText("testLikeEscape:1");
      sender.send(dummyMessage);

      TextMessage message = senderSession.createTextMessage();
      message.setStringProperty("underscored", "_foo");
      message.setText("testLikeEscape:2");
      sender.send(message);

      TextMessage msg = (TextMessage)receiver.receive();
      assertTrue("Message not received",
		 msg != null);
      assertTrue("Message of another test: "+ msg.getText(),
		 msg.getText().startsWith("testLikeEscape"));
      assertEquals("testLikeEscape:2", msg.getText());
      
    } catch (JMSException e) {
      fail(e);
    }
   }

  /**
   * Test the LIKE condition with '_' in the pattern
   * <br />
   * "word LIKE 'l_se'" is <code>true</code> for 'lose' and <code>false</code> for 'loose'
   */
  public void testLike_2() {
    try {
      receiverConnection.stop();
      receiver  = receiverSession.createReceiver(receiverQueue, "word LIKE 'l_se'");
      receiverConnection.start();

      TextMessage dummyMessage = senderSession.createTextMessage();
      dummyMessage.setStringProperty("word", "loose");
      dummyMessage.setText("testLike_2:1");
      sender.send(dummyMessage);

      TextMessage message = senderSession.createTextMessage();
      message.setStringProperty("word", "lose");
      message.setText("testLike_2:2");
      sender.send(message);

      TextMessage msg = (TextMessage)receiver.receive();
      assertTrue("Message not received",
		 msg != null);
      assertTrue("Message of another test: "+ msg.getText(),
		 msg.getText().startsWith("testLike_2"));
      assertEquals("testLike_2:2", msg.getText());
      
    } catch (JMSException e) {
      fail(e);
    }
   }

  /**
   * Test the LIKE condition with '%' in the pattern
   * <br />
   * "phone LIKE '12%3'" is <code>true</code> for '12993' and <code>false</code> for '1234'
   */
  public void testLike_1() {
    try {
      receiverConnection.stop();
      receiver  = receiverSession.createReceiver(receiverQueue, "phone LIKE '12%3'");
      receiverConnection.start();

      TextMessage dummyMessage = senderSession.createTextMessage();
      dummyMessage.setStringProperty("phone", "1234");
      dummyMessage.setText("testLike_1:1");
      sender.send(dummyMessage);

      TextMessage message = senderSession.createTextMessage();
      message.setStringProperty("phone", "12993");
      message.setText("testLike_1:2");
      sender.send(message);

      TextMessage msg = (TextMessage)receiver.receive();
      assertTrue("Message not received", msg != null);
      assertTrue("Message of another test: "+ msg.getText(),
		 msg.getText().startsWith("testLike_1"));
      assertEquals("testLike_1:2", msg.getText());
      
    } catch (JMSException e) {
      fail(e);
    }
  }
    
  /**
   * Test the <code>NULL</code> value in message selector
   * <br />
   * <ul>
   *  <li><code>"prop IS NULL"</code></li>
   * </ul>
   */
  public void testNull() {
     try {
       receiverConnection.stop();
       receiver  = receiverSession.createReceiver(receiverQueue, "prop_name IS NULL");
       receiverConnection.start();
   
       TextMessage dummyMessage = senderSession.createTextMessage();
       dummyMessage.setStringProperty("prop_name", "not null");
       dummyMessage.setText("testNull:1");
       sender.send(dummyMessage);
     
       TextMessage message = senderSession.createTextMessage();
       message.setText("testNull:2");
       sender.send(message);
       
       TextMessage msg = (TextMessage)receiver.receive();
       assertTrue(msg != null);
       assertEquals("testNull:2", msg.getText());
     } catch (JMSException e) {
       fail(e);
       }
   }
  
  /** 
   * Method to use this class in a Test suite
   */
  public static Test suite() {
     return new TestSuite(SelectorTest.class);
   }
  
  public SelectorTest(String name) {
    super(name);
  }
}

