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

package org.objectweb.jtests.jms.conform.message.properties;

import org.objectweb.jtests.jms.framework.*;
import javax.jms.*;
import junit.framework.*;
import java.util.Vector;
import java.util.Enumeration;

/**
 * Test the <code>javax.jms.Message</code> properties.
 * <br />
 *  See JMS Specification, §3.5 Message Properties (p.32-37)
 *
 * @author Jeff Mesnil (jmesnil@inrialpes.fr)
 * @version $Id: MessagePropertyTest.java,v 1.1 2002/04/21 21:15:18 chirino Exp $
 */
public class MessagePropertyTest extends PTPTestCase {
  
  /**
   * Test that any other class than <code>Boolean, Byte, Short, Integer, Long,
   * Float, Double</code> and <code>String</code> used in the <code>Message.setObjectProperty()</code>
   * method throws a <code>javax.jms.MessageFormatException</code>.
   */
  public void testSetObjectProperty_2() {
    try {
      Message message = senderSession.createMessage();
      message.setObjectProperty("prop", new Vector());
      fail("§3.5.5 An attempt to use any other class [than Boolean, Byte,...,String] must throw " +
	   "a JMS MessageFormatException.\n");
    } catch (MessageFormatException e) {
    } catch (JMSException e) {
      fail("Should throw a javax.jms.MessageFormatException, not a "+ e);
    }
  }

  /**
   * if a property is set as a <code>Float</code> with the <code>Message.setObjectProperty()</code>
   * method, it can be retrieve directly as a <code>double</code> by <code>Message.getFloatProperty()</code>
   */
  public void testSetObjectProperty_1() {
    try {
      Message message = senderSession.createMessage();
      message.setObjectProperty("pi", new Float(3.14159f));
      assertEquals(3.14159f, message.getFloatProperty("pi"),0);
    } catch (JMSException e) {
      fail(e);
    }
  }

  /**
   * Test that a <code>null</code> value is returned by the <code>Message.getObjectProperty()</code> method
   * if a property by the specified name does not exits.
   */
  public void testGetObjectProperty() {
    try {
      Message message = senderSession.createMessage();
      assertEquals("§3.5.5 A null value is returned [by the getObjectProperty method] if a property by the specified " +
		   "name does not exits.\n",
		   null,message.getObjectProperty("prop"));
    } catch (JMSException e) {
      fail(e);
    }
  }

  /**
   * Test that a <code>null</code> value is returned by the <code>Message.getStringProperty()</code> method
   * if a property by the specified name does not exits.
   */  
  public void testGetStringProperty() {
   try {
     Message message = senderSession.createMessage();
     assertEquals("§3.5.5 A null value is returned [by the getStringProperty method] if a property by the specified " +
		  "name does not exits.\n",
		  null,message.getStringProperty("prop"));
   } catch (JMSException e) {
     fail(e);
   } 
  } 

  /** 
   * Test that an attempt to get a <code>double</code> property which does not exist throw
   * a <code>java.lang.NullPointerException</code>
   */
  public void testGetDoubleProperty() {
    try {
      Message message = senderSession.createMessage();
      message.getDoubleProperty("prop");
      fail("Should raise a NullPointerException.\n");
    } catch (NullPointerException e) {
    } catch (JMSException e) {
      fail(e);
    } 
  } 
  
  /** 
   * Test that an attempt to get a <code>float</code> property which does not exist throw
   * a <code>java.lang.NullPointerException</code>
   */
  public void testGetFloatProperty() {
   try {
     Message message = senderSession.createMessage();
     message.getFloatProperty("prop");
     fail("Should raise a NullPointerException.\n");
   } catch (NullPointerException e) {
   } catch (JMSException e) {
     fail(e);
   } 
  } 

  /** 
   * Test that an attempt to get a <code>long</code> property which does not exist throw
   * a <code>java.lang.NumberFormatException</code>
   */
  public void testGetLongProperty() {
   try {
     Message message = senderSession.createMessage();
     message.getLongProperty("prop");
     fail("Should raise a NumberFormatException.\n");
   } catch (NumberFormatException e) {
   } catch (JMSException e) {
     fail(e);
   } 
  } 

  /** 
   * Test that an attempt to get a <code>int</code> property which does not exist throw
   * a <code>java.lang.NumberFormatException</code>
   */
  public void testGetIntProperty() {
   try {
     Message message = senderSession.createMessage();
     message.getIntProperty("prop");
     fail("Should raise a NumberFormatException.\n");
   } catch (NumberFormatException e) {
   } catch (JMSException e) {
     fail(e);
   } 
  } 

  /** 
   * Test that an attempt to get a <code>short</code> property which does not exist throw
   * a <code>java.lang.NumberFormatException</code>
   */
  public void testGetShortProperty() {
   try {
     Message message = senderSession.createMessage();
     message.getShortProperty("prop");
     fail("Should raise a NumberFormatException.\n");
   } catch (NumberFormatException e) {
   } catch (JMSException e) {
     fail(e);
   } 
  } 

  /** 
   * Test that an attempt to get a <code>byte</code> property which does not exist throw
   * a <code>java.lang.NumberFormatException</code>
   */
  public void testGetByteProperty() {
   try {
     Message message = senderSession.createMessage();
     message.getByteProperty("prop");
     fail("Should raise a NumberFormatException.\n");
   } catch (NumberFormatException e) {
   } catch (JMSException e) {
     fail(e);
   } 
  } 
  
  /** 
   * Test that an attempt to get a <code>boolean</code> property which does not exist 
   * returns <code>false</code>
   */
  public void testGetBooleanProperty() {
   try {
     Message message = senderSession.createMessage();
     assertEquals(false, message.getBooleanProperty("prop"));
   } catch (JMSException e) {
     fail(e);
   } 
  } 

  /**
   * Test that the <code>Message.getPropertyNames()</code> method does not return
   * the name of the JMS standard header fields (e.g. <code>JMSCorrelationID</code>.
   */
  public void testGetPropertyNames() {
    try {
      Message message = senderSession.createMessage();
      message.setJMSCorrelationID("foo");
      Enumeration enum = message.getPropertyNames();
      assertTrue("§3.5.6 The getPropertyNames method does not return the names of " +
		 "the JMS standard header field [e.g. JMSCorrelationID].\n",
		 !enum.hasMoreElements()); 
    } catch (JMSException e) {
      fail(e);
    }
  }

  /**
   * Test that the <code>Message.getPropertyNames()</code> method returns an empty
   * <code>java.util.Enumeration</code> if there is no properties.
   * <br />
   * If there are some, test that it properly return their names.
   */
  public void testPropertyIteration() {
    try {
      Message message = senderSession.createMessage();
      Enumeration enum = message.getPropertyNames();
      assertTrue("No property yet defined.\n",
		 !enum.hasMoreElements());
      message.setDoubleProperty("pi", 3.14159);
      enum = message.getPropertyNames();
      assertEquals("One property defined of name 'pi'.\n",
		   "pi", (String)enum.nextElement());
    } catch (JMSException e) {
      fail(e);
    }
  }

  /**
   * Test that the <code>Message.clearProperties()</code> method does not clear the
   * value of the Message's body.
   */
  public void testClearProperties_2() {
    try {
      TextMessage message = senderSession.createTextMessage();
      message.setText("foo");
      message.clearProperties();
      assertEquals("§3.5.7 Clearing a message's  property entries does not clear the value of its body.\n",	       
		   "foo", message.getText());
    } catch (JMSException e) {
      fail(e);
    }
  }

  /**
   * Test that the <code>Message.clearProperties()</code> method deletes all the 
   * properties of the Message.
   */
  public void testClearProperties_1() {
    try {
      TextMessage message = senderSession.createTextMessage();
      message.setStringProperty("prop", "foo");
      message.clearProperties();
      assertEquals("§3.5.7 A message's properties are deleted by the clearProperties method.\n",
		   null, message.getStringProperty("prop"));
    } catch (JMSException e) {
      fail(e);
    }
  }
    
  /** 
   * Method to use this class in a Test suite
   */
  public static Test suite() {
    return new TestSuite(MessagePropertyTest.class);
  }
  
  public MessagePropertyTest(String name) {
    super(name);
  }
}

