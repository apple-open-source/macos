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

import org.objectweb.jtests.jms.framework.JMSTestCase;
import javax.jms.*;
import junit.framework.*;

/**
 * Test the default constants of the <code>javax.jms.Message</code> interface.
 *
 * @author Jeff Mesnil (jmesnil@inrialpes.fr)
 * @version $Id: MessageDefaultTest.java,v 1.1 2002/04/21 21:15:18 chirino Exp $
 */
public class MessageDefaultTest extends JMSTestCase {

  /**
   * test that the <code>DEFAULT_DELIVERY_MODE</code> of <code>javax.jms.Message</code>
   * corresponds to <code>javax.jms.Delivery.PERSISTENT</code>.
   */
  public void testDEFAULT_DELIVERY_MODE() {
    assertEquals("The delivery mode is persistent by default.\n",
		 DeliveryMode.PERSISTENT, Message.DEFAULT_DELIVERY_MODE);
  }
  
  /**
   * test that the <code>DEFAULT_PRIORITY</code> of <code>javax.jms.Message</code>
   * corresponds to 4.
   */
  public void testDEFAULT_PRIORITY() {
    assertEquals("The default priority is 4.\n",
		 4, Message.DEFAULT_PRIORITY);
  }
  
  /** 
   * Method to use this class in a Test suite
   */
  public static Test suite() {
    return new TestSuite(MessageDefaultTest.class);
  }
  
  public MessageDefaultTest(String name) {
    super(name);
  }
}

