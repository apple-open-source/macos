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

/**
 * Test the conversion of primitive types for the <code>javax.jms.Message</code> properties.
 * <br />
 * See JMS Specification, §3.5.4 Property Value Conversion and the corresponding table (p.33-34).
 * <br />
 * The method name <code>testXXX2YYY</code> means that we test if a property
 * which has been set as a <code>XXX</code> type can be read as a <code>YYY</code> type, 
 * where <code>XXX</code> and <code>YYY</code> can be <code>boolean, byte, short, long, float
 * double</code> or <code>String</code>.
 *
 * <pre>
 *          ---------------------------------------------------------------|
 *          | boolean | byte | short | int | long | float | double | String| 
 * |-----------------------------------------------------------------------|
 * |boolean |    X                                                     X   |
 * |byte    |            X       X      X     X                        X   |
 * |short   |                    X      X     X                        X   |
 * |int     |                           X     X                        X   |
 * |long    |                                 X                        X   |
 * |float   |                                         X       X        X   |
 * |double  |                                                 X        X   |
 * |String  |    Y       Y       Y      Y     Y       Y       Y        X   |
 * |-----------------------------------------------------------------------|
 * </pre>
 * A value set as the row type can be read as the column type.
 * <br />
 * The unmarked cases must throw a <code>javax.jms.MessageFormatException</code>
 * <br />
 * The cases marked with a Y should throw a <code>java.lang.MessageFormatException</code> <strong>if</strong> the
 * String is not a correct representation of the column type (otherwise, it returns the property). 
 *
 * @author Jeff Mesnil (jmesnil@inrialpes.fr)
 * @version $Id: MessagePropertyConversionTest.java,v 1.1.4.1 2003/05/31 00:35:11 ejort Exp $
 */
public class MessagePropertyConversionTest extends PTPTestCase {
  
  /**
   * if a property is set as a <code>java.lang.String</code>, 
   * it can also be read as a <code>java.lang.String</code>.
   */
  public void testString2String() {
    try {
      Message message = senderSession.createMessage();
      message.setStringProperty("pi", "3.14159");
      assertEquals("3.14159", message.getStringProperty("pi"));
    } catch (JMSException e) {
      fail(e);
    }
  }

  /**
   * if a property is set as a <code>java.lang.String</code>, 
   * to get it as a <code>double</code> throws a <code>java.lang.NuberFormatException</code>
   * if the <code>String</code> is not a correct representation for a <code>double</code> 
   * (e.g. <code>"not a number"</code>).
   */
  public void testString2Double_2() {
    try {
      Message message = senderSession.createMessage();
      message.setStringProperty("pi", "not_a_number");
      message.getDoubleProperty("pi");
      fail("§3.5.4 The String to numeric conversions must throw the java.lang.NumberFormatException " +
	   " if the numeric's valueOf() method does not accept the String value as a valid representation.\n");
    } catch (java.lang.NumberFormatException e) {
    } catch (JMSException e) {
      fail(e);
    }
  }
  
  /**
   * if a property is set as a <code>java.lang.String</code>, 
   * it can also be read as a <code>double</code> as long as the <code>String</code>
   * is a correct representation of a <code>double</code> (e.g. <code>"3.14159"</code>).
   */
  public void testString2Double_1() {
    try {
      Message message = senderSession.createMessage();
      message.setStringProperty("pi", "3.14159");
      assertEquals(3.14159, message.getDoubleProperty("pi"),0);
    } catch (JMSException e) {
      fail(e);
    }
  }

  /**
   * if a property is set as a <code>java.lang.String</code>, 
   * to get it as a <code>float</code> throws a <code>java.lang.NuberFormatException</code>
   * if the <code>String</code> is not a correct representation for a <code>float</code> 
   * (e.g. <code>"not_a_number"</code>).
   */
  public void testString2Float_2() {
    try {
      Message message = senderSession.createMessage();
      message.setStringProperty("pi", "not_a_number");
      message.getFloatProperty("pi");
      fail("§3.5.4 The String to numeric conversions must throw the java.lang.NumberFormatException " +
	   " if the numeric's valueOf() method does not accept the String value as a valid representation.\n");
    } catch (java.lang.NumberFormatException e) {
    } catch (JMSException e) {
      fail(e);
    }
  }
  
  /**
   * if a property is set as a <code>java.lang.String</code>, 
   * it can also be read as a <code>float</code> as long as the <code>String</code>
   * is a correct representation of a <code>float</code> (e.g. <code>"3.14159"</code>).
   */
  public void testString2Float_1() {
    try {
      Message message = senderSession.createMessage();
      message.setStringProperty("pi", "3.14159");
      assertEquals(3.14159F,message.getFloatProperty("pi"), 0);
    } catch (JMSException e) {
      fail(e);
    }
  }

  /**
   * if a property is set as a <code>java.lang.String</code>, 
   * to get it as a <code>long</code> throws a <code>java.lang.NuberFormatException</code>
   * if the <code>String</code> is not a correct representation for a <code>long</code> 
   * (e.g. <code>"3.14159"</code>).
   */
  public void testString2Long_2() {
    try {
      Message message = senderSession.createMessage();
      message.setStringProperty("pi", "3.14159");
      message.getLongProperty("pi");
      fail("§3.5.4 The String to numeric conversions must throw the java.lang.NumberFormatException " +
	   " if the numeric's valueOf() method does not accept the String value as a valid representation.\n");
    } catch (java.lang.NumberFormatException e) {
    } catch (JMSException e) {
      fail(e);
    }
  }

  /**
   * if a property is set as a <code>java.lang.String</code>, 
   * it can also be read as a <code>long</code> as long as the <code>String</code>
   * is a correct representation of a <code>long</code> (e.g. <code>"0"</code>).
   */
  public void testString2Long_1() {
    try {
      Message message = senderSession.createMessage();
      message.setStringProperty("prop", "0");
      assertEquals((long)0,message.getLongProperty("prop"));
    } catch (JMSException e) {
      fail(e);
    }
  }

  /**
   * if a property is set as a <code>java.lang.String</code>, 
   * to get it as a <code>int</code> throws a <code>java.lang.NuberFormatException</code>
   * if the <code>String</code> is not a correct representation for a <code>int</code> 
   * (e.g. <code>"3.14159"</code>).
   */
  public void testString2Int_2() {
    try {
      Message message = senderSession.createMessage();
      message.setStringProperty("pi", "3.14159");
      message.getIntProperty("pi");
      fail("§3.5.4 The String to numeric conversions must throw the java.lang.NumberFormatException " +
	   " if the numeric's valueOf() method does not accept the String value as a valid representation.\n");
    } catch (java.lang.NumberFormatException e) {
    } catch (JMSException e) {
      fail(e);
    }
  }

  /**
   * if a property is set as a <code>java.lang.String</code>, 
   * it can also be read as a <code>int</code> as long as the <code>String</code>
   * is a correct representation of a <code>int</code> (e.g. <code>"0"</code>).
   */
  public void testString2Int_1() {
    try {
      Message message = senderSession.createMessage();
      message.setStringProperty("prop", "0");
      assertEquals((int)0,message.getIntProperty("prop"));
    } catch (JMSException e) {
      fail(e);
    }
  }

  /**
   * if a property is set as a <code>java.lang.String</code>, 
   * to get it as a <code>short</code> throws a <code>java.lang.NuberFormatException</code>
   * if the <code>String</code> is not a correct representation for a <code>short</code> 
   * (e.g. <code>"3.14159"</code>).
   */
  public void testString2Short_2() {
    try {
      Message message = senderSession.createMessage();
      message.setStringProperty("pi", "3.14159");
      message.getShortProperty("pi");
      fail("§3.5.4 The String to numeric conversions must throw the java.lang.NumberFormatException " +
	   " if the numeric's valueOf() method does not accept the String value as a valid representation.\n");
    } catch (java.lang.NumberFormatException e) {
    } catch (JMSException e) {
      fail(e);
    }
  }

  /**
   * if a property is set as a <code>java.lang.String</code>, 
   * it can also be read as a <code>short</code> as long as the <code>String</code>
   * is a correct representation of a <code>short</code> (e.g. <code>"0"</code>).
   */
  public void testString2Short_1() {
    try {
      Message message = senderSession.createMessage();
      message.setStringProperty("prop", "0");
      assertEquals((short)0,message.getShortProperty("prop"));
    } catch (JMSException e) {
      fail(e);
    }
  }

  /**
   * if a property is set as a <code>java.lang.String</code>, 
   * to get it as a <code>byte</code> throws a <code>java.lang.NuberFormatException</code>
   * if the <code>String</code> is not a correct representation for a <code>byte</code> 
   * (e.g. <code>"3.14159"</code>).
   */
  public void testString2Byte_2() {
    try {
      Message message = senderSession.createMessage();
      message.setStringProperty("pi", "3.14159");
      message.getByteProperty("pi");
      fail("§3.5.4 The String to numeric conversions must throw the java.lang.NumberFormatException " +
	   " if the numeric's valueOf() method does not accept the String value as a valid representation.\n");
    } catch (java.lang.NumberFormatException e) {
    } catch (JMSException e) {
      fail(e);
    }
  }

  /**
   * if a property is set as a <code>java.lang.String</code>, 
   * it can also be read as a <code>byte</code> if the <code>String</code>
   * is a correct representation of a <code>byte</code> (e.g. <code>"0"</code>).
   */
  public void testString2Byte_1() {
    try {
      Message message = senderSession.createMessage();
      message.setStringProperty("prop", "0");
      assertEquals((byte)0,message.getByteProperty("prop"));
    } catch (JMSException e) {
      fail(e);
    }
  }

  /**
   * if a property is set as a <code>java.lang.String</code>, 
   * to get it as a <code>boolean</code> returns <code>true</code> if the property is not 
   * null and is equal, ignoring case, to the string "true" (.eg. "True" is ok), else it
   * returns <code>false</code> (e.g. "test")
   */
  public void testString2Boolean_2() {
    try {
      Message message = senderSession.createMessage();
      message.setStringProperty("prop", "test");
      assertEquals(false, message.getBooleanProperty("prop"));
    } catch (MessageFormatException e) {
    } catch (JMSException e) {
      fail(e);
    }
  }

  /**
   * if a property is set as a <code>java.lang.String</code>, 
   * it can also be read as a <code>boolean</code> if the <code>String</code>
   * is a correct representation of a <code>boolean</code> (e.g. <code>"true"</code>).
   */
  public void testString2Boolean_1() {
    try {
      Message message = senderSession.createMessage();
      message.setStringProperty("prop", "true");
      assertEquals(true,message.getBooleanProperty("prop"));
    } catch (JMSException e) {
      fail(e);
    }
  }

  /**
   * if a property is set as a <code>double</code>, 
   * it can also be read as a <code>java.lang.String</code>.
   */
  public void testDouble2String() {
    try {
      Message message = senderSession.createMessage();
      message.setDoubleProperty("prop", 127.0);
      assertEquals("127.0", message.getStringProperty("prop"));
    } catch (JMSException e) {
      fail(e);
    }
  }
  
  /**
   * if a property is set as a <code>double</code>, 
   * it can also be read as a <code>double</code>.
   */
  public void testDouble2Double() {
    try {
      Message message = senderSession.createMessage();
      message.setDoubleProperty("prop", 127.0);
      assertEquals(127.0, message.getDoubleProperty("prop"), 0);
    } catch (JMSException e) {
      fail(e);
    }
  }
  
  /**
   * if a property is set as a <code>double</code>, 
   * to get is as a <code>float</code> throws a <code>javax.jms.MessageFormatException</code>.
   */
  public void testDouble2Float() {
    try {
      Message message = senderSession.createMessage();
      message.setDoubleProperty("prop", 127.0);
      message.getFloatProperty("prop");
      fail("§3.5.4 The unmarked cases [of Table 0-4] should raise a JMS MessageFormatException.\n");
    } catch (MessageFormatException e) {
    } catch (JMSException e) {
      fail(e);
    }
  }
  
  /**
   * if a property is set as a <code>double</code>, 
   * to get is as a <code>long</code> throws a <code>javax.jms.MessageFormatException</code>.
   */
  public void testDouble2Long() {
   try {
      Message message = senderSession.createMessage();
      message.setDoubleProperty("prop", 127.0);
      message.getLongProperty("prop");
      fail("§3.5.4 The unmarked cases [of Table 0-4] should raise a JMS MessageFormatException.\n");
    } catch (MessageFormatException e) {
    } catch (JMSException e) {
      fail(e);
    }
  }
  
  /**
   * if a property is set as a <code>double</code>, 
   * to get is as an <code>int</code> throws a <code>javax.jms.MessageFormatException</code>.
   */
  public void testDouble2Int() {
   try {
      Message message = senderSession.createMessage();
      message.setDoubleProperty("prop", 127.0);
      message.getIntProperty("prop");
      fail("§3.5.4 The unmarked cases [of Table 0-4] should raise a JMS MessageFormatException.\n");
    } catch (MessageFormatException e) {
    } catch (JMSException e) {
      fail(e);
    }
  }

  /**
   * if a property is set as a <code>double</code>, 
   * to get is as a <code>short</code> throws a <code>javax.jms.MessageFormatException</code>.
   */
  public void testDouble2Short() {
    try {
      Message message = senderSession.createMessage();
      // store a value that can't be converted to short
      message.setDoubleProperty("prop", 127.0);
      message.getShortProperty("prop");
      fail("§3.5.4 The unmarked cases [of Table 0-4] should raise a JMS MessageFormatException.\n");
    } catch (MessageFormatException e) {
    } catch (JMSException e) {
      fail(e);
    }
  }

  /**
   * if a property is set as a <code>double</code>, 
   * to get is as a <code>byte</code> throws a <code>javax.jms.MessageFormatException</code>.
   */
  public void testDouble2Byte() {
    try {
      Message message = senderSession.createMessage();
      // store a value that can't be converted to byte
      message.setDoubleProperty("prop", 127.0);
      message.getByteProperty("prop");
      fail("§3.5.4 The unmarked cases [of Table 0-4] should raise a JMS MessageFormatException.\n");
    } catch (MessageFormatException e) {
    } catch (JMSException e) {
      fail(e);
    }
  }

  /**
   * if a property is set as a <code>double</code>, 
   * to get is as a <code>boolean</code> throws a <code>javax.jms.MessageFormatException</code>.
   */
  public void testDouble2Boolean() {
    try {
      Message message = senderSession.createMessage();
      // store a value that can be converted to boolean
      message.setDoubleProperty("prop", 127.0);
      message.getBooleanProperty("prop");
      fail("§3.5.4 The unmarked cases [of Table 0-4] should raise a JMS MessageFormatException.\n");
    } catch (MessageFormatException e) {
    } catch (JMSException e) {
      fail(e);
    }
  }

  /**
   * if a property is set as a <code>float</code>, 
   * it can also be read as a <code>String</code>.
   */
  public void testFloat2String() {
    try {
      Message message = senderSession.createMessage();
      message.setFloatProperty("prop", 127.0F);
      assertEquals("127.0", message.getStringProperty("prop"));
    } catch (JMSException e) {
      fail(e);
    }
  }
  
  /**
   * if a property is set as a <code>float</code>, 
   * it can also be read as a <code>double</code>.
   */
  public void testFloat2Double() {
    try {
      Message message = senderSession.createMessage();
      message.setFloatProperty("prop", 127.0F);
      assertEquals(127.0, message.getDoubleProperty("prop"), 0);
    } catch (JMSException e) {
      fail(e);
    }
  }
  
  /**
   * if a property is set as a <code>float</code>, 
   * it can also be read as a <code>float</code>.
   */
  public void testFloat2Float() {
    try {
      Message message = senderSession.createMessage();
      message.setFloatProperty("prop", 127.0F);
      assertEquals(127.0F, message.getFloatProperty("prop"), 0);
    } catch (JMSException e) {
      fail(e);
    }
  }
  
  /**
   * if a property is set as a <code>float</code>, 
   * to get is as a <code>long</code> throws a <code>javax.jms.MessageFormatException</code>.
   */
  public void testFloat2Long() {
   try {
      Message message = senderSession.createMessage();
      message.setFloatProperty("prop", 127.0F);
      message.getLongProperty("prop");
      fail("§3.5.4 The unmarked cases [of Table 0-4] should raise a JMS MessageFormatException.\n");
    } catch (MessageFormatException e) {
    } catch (JMSException e) {
      fail(e);
    }
  }
  
  /**
   * if a property is set as a <code>float</code>, 
   * to get is as a <code>int</code> throws a <code>javax.jms.MessageFormatException</code>.
   */
  public void testFloat2Int() {
   try {
      Message message = senderSession.createMessage();
      message.setFloatProperty("prop", 127.0F);
      message.getIntProperty("prop");
      fail("§3.5.4 The unmarked cases [of Table 0-4] should raise a JMS MessageFormatException.\n");
    } catch (MessageFormatException e) {
    } catch (JMSException e) {
      fail(e);
    }
  }

  /**
   * if a property is set as a <code>float</code>, 
   * to get is as a <code>short</code> throws a <code>javax.jms.MessageFormatException</code>.
   */
  public void testFloat2Short() {
    try {
      Message message = senderSession.createMessage();
      // store a value that can't be converted to short
      message.setFloatProperty("prop", 127.0F);
      message.getShortProperty("prop");
      fail("§3.5.4 The unmarked cases [of Table 0-4] should raise a JMS MessageFormatException.\n");
    } catch (MessageFormatException e) {
    } catch (JMSException e) {
      fail(e);
    }
  }

  /**
   * if a property is set as a <code>float</code>, 
   * to get is as a <code>byte</code> throws a <code>javax.jms.MessageFormatException</code>.
   */
  public void testFloat2Byte() {
    try {
      Message message = senderSession.createMessage();
      // store a value that can't be converted to byte
      message.setFloatProperty("prop", 127.0F);
      message.getByteProperty("prop");
      fail("§3.5.4 The unmarked cases [of Table 0-4] should raise a JMS MessageFormatException.\n");
    } catch (MessageFormatException e) {
    } catch (JMSException e) {
      fail(e);
    }
  }

  /**
   * if a property is set as a <code>float</code>, 
   * to get is as a <code>boolean</code> throws a <code>javax.jms.MessageFormatException</code>.
   */
  public void testFloat2Boolean() {
    try {
      Message message = senderSession.createMessage();
      // store a value that can be converted to boolean
      message.setFloatProperty("prop", 127.0F);
      message.getBooleanProperty("prop");
      fail("§3.5.4 The unmarked cases [of Table 0-4] should raise a JMS MessageFormatException.\n");
    } catch (MessageFormatException e) {
    } catch (JMSException e) {
      fail(e);
    }
  }

  /**
   * if a property is set as a <code>long</code>, 
   * it can also be read as a <code>String</code>.
   */
  public void testLong2String() {
    try {
      Message message = senderSession.createMessage();
      message.setLongProperty("prop", 127L);
      assertEquals("127", message.getStringProperty("prop"));
    } catch (JMSException e) {
      fail(e);
    }
  }
  
  /**
   * if a property is set as a <code>long</code>, 
   * to get is as a <code>double</code> throws a <code>javax.jms.MessageFormatException</code>.
   */
  public void testLong2Double() {
    try {
      Message message = senderSession.createMessage();
      message.setLongProperty("prop", 127L);
      message.getDoubleProperty("prop");
      fail("§3.5.4 The unmarked cases [of Table 0-4] should raise a JMS MessageFormatException.\n");
    } catch (MessageFormatException e) {
    } catch (JMSException e) {
      fail(e);
    }
  }
  
  /**
   * if a property is set as a <code>long</code>, 
   * to get is as a <code>float</code> throws a <code>javax.jms.MessageFormatException</code>.
   */
  public void testLong2Float() {
    try {
      Message message = senderSession.createMessage();
      message.setLongProperty("prop", 127L);
      message.getFloatProperty("prop");
      fail("§3.5.4 The unmarked cases [of Table 0-4] should raise a JMS MessageFormatException.\n");
    } catch (MessageFormatException e) {
    } catch (JMSException e) {
      fail(e);
    }
  }
  
  /**
   * if a property is set as a <code>long</code>, 
   * it can also be read as a <code>long</code>.
   */
  public void testLong2Long() {
   try {
      Message message = senderSession.createMessage();
      message.setLongProperty("prop", 127L);
      assertEquals(127L, message.getLongProperty("prop"));
    } catch (JMSException e) {
      fail(e);
    }
  }
  
  /**
   * if a property is set as a <code>long</code>, 
   * to get is as an <code>int</code> throws a <code>javax.jms.MessageFormatException</code>.
   */
  public void testLong2Int() {
   try {
      Message message = senderSession.createMessage();
      message.setLongProperty("prop", 127L);
      message.getIntProperty("prop");
      fail("§3.5.4 The unmarked cases [of Table 0-4] should raise a JMS MessageFormatException.\n");
    } catch (MessageFormatException e) {
    } catch (JMSException e) {
      fail(e);
    }
  }

  /**
   * if a property is set as a <code>long</code>, 
   * to get is as a <code>short</code> throws a <code>javax.jms.MessageFormatException</code>.
   */
  public void testLong2Short() {
    try {
      Message message = senderSession.createMessage();
      // store a value that can't be converted to short
      message.setLongProperty("prop", 127L);
      message.getShortProperty("prop");
      fail("§3.5.4 The unmarked cases [of Table 0-4] should raise a JMS MessageFormatException.\n");
    } catch (MessageFormatException e) {
    } catch (JMSException e) {
      fail(e);
    }
  }

  /**
   * if a property is set as a <code>long</code>, 
   * to get is as a <code>byte</code> throws a <code>javax.jms.MessageFormatException</code>.
   */
  public void testLong2Byte() {
    try {
      Message message = senderSession.createMessage();
      // store a value that can't be converted to byte
      message.setLongProperty("prop", 127L);
      message.getByteProperty("prop");
      fail("§3.5.4 The unmarked cases [of Table 0-4] should raise a JMS MessageFormatException.\n");
    } catch (MessageFormatException e) {
    } catch (JMSException e) {
      fail(e);
    }
  }

  /**
   * if a property is set as a <code>long</code>, 
   * to get is as a <code>boolean</code> throws a <code>javax.jms.MessageFormatException</code>.
   */
  public void testLong2Boolean() {
    try {
      Message message = senderSession.createMessage();
      // store a value that can be converted to boolean
      message.setLongProperty("prop", 127L);
      message.getBooleanProperty("prop");
      fail("§3.5.4 The unmarked cases [of Table 0-4] should raise a JMS MessageFormatException.\n");
    } catch (MessageFormatException e) {
    } catch (JMSException e) {
      fail(e);
    }
  }

  /**
   * if a property is set as an <code>int</code>, 
   * it can also be read as a <code>String</code>.
   */
  public void testInt2String() {
    try {
      Message message = senderSession.createMessage();
      message.setIntProperty("prop", (int)127);
      assertEquals("127", message.getStringProperty("prop"));
    } catch (JMSException e) {
      fail(e);
    }
  }
  
  /**
   * if a property is set as a <code>int</code>, 
   * to get is as a <code>double</code> throws a <code>javax.jms.MessageFormatException</code>.
   */
  public void testInt2Double() {
    try {
      Message message = senderSession.createMessage();
      message.setIntProperty("prop", (int)127);
      message.getDoubleProperty("prop");
      fail("§3.5.4 The unmarked cases [of Table 0-4] should raise a JMS MessageFormatException.\n");
    } catch (MessageFormatException e) {
    } catch (JMSException e) {
      fail(e);
    }
  }
  
  /**
   * if a property is set as a <code>int</code>, 
   * to get is as a <code>float</code> throws a <code>javax.jms.MessageFormatException</code>.
   */
  public void testInt2Float() {
    try {
      Message message = senderSession.createMessage();
      message.setIntProperty("prop", (int)127);
      message.getFloatProperty("prop");
      fail("§3.5.4 The unmarked cases [of Table 0-4] should raise a JMS MessageFormatException.\n");
    } catch (MessageFormatException e) {
    } catch (JMSException e) {
      fail(e);
    }
  }
  
  /**
   * if a property is set as an <code>int</code>, 
   * it can also be read as a <code>long</code>.
   */
  public void testInt2Long() {
   try {
      Message message = senderSession.createMessage();
      message.setIntProperty("prop", (int)127);
      assertEquals(127L, message.getLongProperty("prop"));
    } catch (JMSException e) {
      fail(e);
    }
  }
  
  /**
   * if a property is set as an <code>int</code>, 
   * it can also be read as an <code>int</code>.
   */
  public void testInt2Int() {
   try {
      Message message = senderSession.createMessage();
      message.setIntProperty("prop", (int)127);
      assertEquals((int)127, message.getIntProperty("prop"));
    } catch (JMSException e) {
      fail(e);
    }
  }

  /**
   * if a property is set as a <code>int</code>, 
   * to get is as a <code>short</code> throws a <code>javax.jms.MessageFormatException</code>.
   */
  public void testInt2Short() {
    try {
      Message message = senderSession.createMessage();
      // store a value that can't be converted to short
      message.setIntProperty("prop", Integer.MAX_VALUE);
      message.getShortProperty("prop");
      fail("§3.5.4 The unmarked cases [of Table 0-4] should raise a JMS MessageFormatException.\n");
    } catch (MessageFormatException e) {
    } catch (JMSException e) {
      fail(e);
    }
  }

  /**
   * if a property is set as a <code>int</code>, 
   * to get is as a <code>byte</code> throws a <code>javax.jms.MessageFormatException</code>.
   */
  public void testInt2Byte() {
    try {
      Message message = senderSession.createMessage();
      // store a value that can't be converted to byte
      message.setIntProperty("prop", Integer.MAX_VALUE);
      message.getByteProperty("prop");
      fail("§3.5.4 The unmarked cases [of Table 0-4] should raise a JMS MessageFormatException.\n");
    } catch (MessageFormatException e) {
    } catch (JMSException e) {
      fail(e);
    }
  }

  /**
   * if a property is set as a <code>int</code>, 
   * to get is as a <code>boolean</code> throws a <code>javax.jms.MessageFormatException</code>.
   */
  public void testInt2Boolean() {
    try {
      Message message = senderSession.createMessage();
      // store a value that can be converted to boolean
      message.setIntProperty("prop", Integer.MAX_VALUE);
      message.getBooleanProperty("prop");
      fail("§3.5.4 The unmarked cases [of Table 0-4] should raise a JMS MessageFormatException.\n");
    } catch (MessageFormatException e) {
    } catch (JMSException e) {
      fail(e);
    }
  }

  /**
   * if a property is set as a <code>short</code>, 
   * it can also be read as a <code>String</code>.
   */
  public void testShort2String() {
    try {
      Message message = senderSession.createMessage();
      message.setShortProperty("prop", (short)127);
      assertEquals("127", message.getStringProperty("prop"));
    } catch (JMSException e) {
      fail(e);
    }
  }
  
  /**
   * if a property is set as a <code>short</code>, 
   * to get is as a <code>double</code> throws a <code>javax.jms.MessageFormatException</code>.
   */
  public void testShort2Double() {
    try {
      Message message = senderSession.createMessage();
      message.setShortProperty("prop", (short)127);
      message.getDoubleProperty("prop");
      fail("§3.5.4 The unmarked cases [of Table 0-4] should raise a JMS MessageFormatException.\n");
    } catch (MessageFormatException e) {
    } catch (JMSException e) {
      fail(e);
    }
  }
  
  /**
   * if a property is set as a <code>short</code>, 
   * to get is as a <code>float</code> throws a <code>javax.jms.MessageFormatException</code>.
   */
  public void testShort2Float() {
    try {
      Message message = senderSession.createMessage();
      message.setShortProperty("prop", (short)127);
      message.getFloatProperty("prop");
      fail("§3.5.4 The unmarked cases [of Table 0-4] should raise a JMS MessageFormatException.\n");
    } catch (MessageFormatException e) {
    } catch (JMSException e) {
      fail(e);
    }
  }

  /**
   * if a property is set as a <code>short</code>, 
   * it can also be read as a <code>long</code>.
   */
  public void testShort2Long() {
   try {
      Message message = senderSession.createMessage();
      message.setShortProperty("prop", (short)127);
      assertEquals(127L, message.getLongProperty("prop"));
    } catch (JMSException e) {
      fail(e);
    }
  }
  
  /**
   * if a property is set as a <code>short</code>, 
   * it can also be read as an <code>int</code>.
   */
  public void testShort2Int() {
   try {
      Message message = senderSession.createMessage();
      message.setShortProperty("prop", (short)127);
      assertEquals((int)127, message.getIntProperty("prop"));
    } catch (JMSException e) {
      fail(e);
    }
  }
  
  /**
   * if a property is set as a <code>short</code>, 
   * it can also be read as a <code>short</code>.
   */
  public void testShort2Short() {
   try {
      Message message = senderSession.createMessage();
      message.setShortProperty("prop", (short)127);
      assertEquals((short)127, message.getShortProperty("prop"));
    } catch (JMSException e) {
      fail(e);
    }
  }
  
  /**
   * if a property is set as a <code>short</code>, 
   * to get is as a <code>byte</code> throws a <code>javax.jms.MessageFormatException</code>.
   */
  public void testShort2Byte() {
    try {
      Message message = senderSession.createMessage();
      message.setShortProperty("prop", (short)127);
      message.getByteProperty("prop");
      fail("§3.5.4 The unmarked cases [of Table 0-4] should raise a JMS MessageFormatException.\n");
    } catch (MessageFormatException e) {
    } catch (JMSException e) {
      fail(e);
    }
  }
  
  /**
   * if a property is set as a <code>short</code>, 
   * to get is as a <code>boolean</code> throws a <code>javax.jms.MessageFormatException</code>.
   */
  public void testShort2Boolean() {
    try {
      Message message = senderSession.createMessage();
      // store a value that can't be converted to boolean
      message.setShortProperty("prop", (short)127);
      message.getBooleanProperty("prop");
      fail("§3.5.4 The unmarked cases [of Table 0-4] should raise a JMS MessageFormatException.\n");
    } catch (MessageFormatException e) {
    } catch (JMSException e) {
      fail(e);
    }
  }

  /**
   * if a property is set as a <code>byte</code>, 
   * it can also be read as a <code>String</code>.
   */
  public void testByte2String() {
    try {
      Message message = senderSession.createMessage();
      message.setByteProperty("prop", (byte)127);
      assertEquals("127", message.getStringProperty("prop"));
    } catch (JMSException e) {
      fail(e);
    }
  }
  
  /**
   * if a property is set as a <code>byte</code>, 
   * to get is as a <code>double</code> throws a <code>javax.jms.MessageFormatException</code>.
   */
  public void testByte2Double() {
    try {
      Message message = senderSession.createMessage();
      message.setByteProperty("prop", (byte)127);
      message.getDoubleProperty("prop");
      fail("§3.5.4 The unmarked cases [of Table 0-4] should raise a JMS MessageFormatException.\n");
    } catch (MessageFormatException e) {
    } catch (JMSException e) {
      fail(e);
    }
  }
  
  /**
   * if a property is set as a <code>byte</code>, 
   * to get is as a <code>float</code> throws a <code>javax.jms.MessageFormatException</code>.
   */
  public void testByte2Float() {
    try {
      Message message = senderSession.createMessage();
      message.setByteProperty("prop", (byte)127);
      message.getFloatProperty("prop");
      fail("§3.5.4 The unmarked cases [of Table 0-4] should raise a JMS MessageFormatException.\n");
    } catch (MessageFormatException e) {
    } catch (JMSException e) {
      fail(e);
    }
  }
  
  /**
   * if a property is set as a <code>byte</code>, 
   * it can also be read as a <code>long</code>.
   */
  public void testByte2Long() {
   try {
      Message message = senderSession.createMessage();
      message.setByteProperty("prop", (byte)127);
      assertEquals(127L, message.getLongProperty("prop"));
    } catch (JMSException e) {
      fail(e);
    }
  }
  
  /**
   * if a property is set as a <code>byte</code>, 
   * it can also be read as an <code>int</code>.
   */
  public void testByte2Int() {
   try {
      Message message = senderSession.createMessage();
      message.setByteProperty("prop", (byte)127);
      assertEquals((int)127, message.getIntProperty("prop"));
    } catch (JMSException e) {
      fail(e);
    }
  }
  
  /**
   * if a property is set as a <code>byte</code>, 
   * it can also be read as a <code>short</code>.
   */
  public void testByte2Short() {
   try {
      Message message = senderSession.createMessage();
      message.setByteProperty("prop", (byte)127);
      assertEquals((short)127, message.getShortProperty("prop"));
    } catch (JMSException e) {
      fail(e);
    }
  }
  
  /**
   * if a property is set as a <code>byte</code>, 
   * it can also be read as a <code>byte</code>.
   */
  public void testByte2Byte() {
    try {
      Message message = senderSession.createMessage();
      message.setByteProperty("prop", (byte)127);
      assertEquals((byte)127, message.getByteProperty("prop"));
    } catch (JMSException e) {
      fail(e);
    }
  }
  
  /**
   * if a property is set as a <code>byte</code>, 
   * to get is as a <code>boolean</code> throws a <code>javax.jms.MessageFormatException</code>.
   */
  public void testByte2Boolean() {
    try {
      Message message = senderSession.createMessage();
      // store a value that can't be converted to boolean
      message.setByteProperty("prop", (byte)127);
      message.getBooleanProperty("prop");
      fail("§3.5.4 The unmarked cases [of Table 0-4] should raise a JMS MessageFormatException.\n");
    } catch (MessageFormatException e) {
    } catch (JMSException e) {
      fail(e);
    }
  }

  /**
   * if a property is set as a <code>boolean</code>, 
   * it can also be read as a <code>String</code>.
   */
  public void testBoolean2String() {
    try {
      Message message = senderSession.createMessage();
      message.setBooleanProperty("prop", true);
      assertEquals("true",message.getStringProperty("prop"));
    } catch (JMSException e) {
      fail(e);
    }
  }
  
  /**
   * if a property is set as a <code>boolean</code>, 
   * to get is as a <code>double</code> throws a <code>javax.jms.MessageFormatException</code>.
   */
  public void testBoolean2Double() {
    try {
      Message message = senderSession.createMessage();
      // store a value that can't be converted to double
      message.setBooleanProperty("prop", true);
      message.getDoubleProperty("prop");
      fail("§3.5.4 The unmarked cases [of Table 0-4] should raise a JMS MessageFormatException.\n");
    } catch (MessageFormatException e) {
    } catch (JMSException e) {
      fail(e);
    }
  }
  
  /**
   * if a property is set as a <code>boolean</code>, 
   * to get is as a <code>float</code> throws a <code>javax.jms.MessageFormatException</code>.
   */
  public void testBoolean2Float() {
    try {
      Message message = senderSession.createMessage();
      // store a value that can't be converted to float
      message.setBooleanProperty("prop", true);
      message.getFloatProperty("prop");
      fail("§3.5.4 The unmarked cases [of Table 0-4] should raise a JMS MessageFormatException.\n");
    } catch (MessageFormatException e) {
    } catch (JMSException e) {
      fail(e);
    }
  }
  
  /**
   * if a property is set as a <code>boolean</code>, 
   * to get is as a <code>long</code> throws a <code>javax.jms.MessageFormatException</code>.
   */
  public void testBoolean2Long() {
   try {
      Message message = senderSession.createMessage();
      // store a value that can't be converted to long
      message.setBooleanProperty("true", true);
      message.getLongProperty("true");
      fail("§3.5.4 The unmarked cases [of Table 0-4] should raise a JMS MessageFormatException.\n");
    } catch (MessageFormatException e) {
    } catch (JMSException e) {
      fail(e);
    }
  }
  
  /**
   * if a property is set as a <code>boolean</code>, 
   * to get is as a <code>int</code> throws a <code>javax.jms.MessageFormatException</code>.
   */
  public void testBoolean2Int() {
   try {
      Message message = senderSession.createMessage();
      // store a value that can't be converted to int
      message.setBooleanProperty("prop", true);
      message.getIntProperty("prop");
      fail("§3.5.4 The unmarked cases [of Table 0-4] should raise a JMS MessageFormatException.\n");
    } catch (MessageFormatException e) {
    } catch (JMSException e) {
      fail(e);
    }
  }
  
  /**
   * if a property is set as a <code>boolean</code>, 
   * to get is as a <code>short</code> throws a <code>javax.jms.MessageFormatException</code>.
   */
  public void testBoolean2Short() {
   try {
      Message message = senderSession.createMessage();
      // store a value that can't be converted to short
      message.setBooleanProperty("prop", true);
      message.getShortProperty("prop");
      fail("§3.5.4 The unmarked cases [of Table 0-4] should raise a JMS MessageFormatException.\n");
    } catch (MessageFormatException e) {
    } catch (JMSException e) {
      fail(e);
    }
  }
  
  /**
   * if a property is set as a <code>boolean</code>, 
   * to get is as a <code>byte</code> throws a <code>javax.jms.MessageFormatException</code>.
   */
  public void testBoolean2Byte() {
    try {
      Message message = senderSession.createMessage();
      // store a value that can't be converted to byte
      message.setBooleanProperty("prop", true);
      message.getByteProperty("prop");
      fail("§3.5.4 The unmarked cases [of Table 0-4] should raise a JMS MessageFormatException.\n");
    } catch (MessageFormatException e) {
    } catch (JMSException e) {
      fail(e);
    }
  }
  
  /**
   * if a property is set as a <code>boolean</code>, 
   * it can also be read as a <code>boolean</code>.
   */
  public void testBoolean2Boolean() {
    try {
      Message message = senderSession.createMessage();
      message.setBooleanProperty("prop", true);
      assertEquals(true,message.getBooleanProperty("prop"));
    } catch (JMSException e) {
      fail(e);
    }
  }
  
  /** 
   * Method to use this class in a Test suite
   */
  public static Test suite() {
     return new TestSuite(MessagePropertyConversionTest.class);
   }
  
  public MessagePropertyConversionTest(String name) {
    super(name);
  }
}
