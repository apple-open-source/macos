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

package org.objectweb.jtests.jms.conform.session;

import org.objectweb.jtests.jms.framework.*;
import junit.framework.*;

import javax.jms.*;

/**
 * Test sessions
 * <br />
 * See JMS specifications, §4.4 Session
 * 
 * @author Jeff Mesnil (jmesnil@inrialpes.fr)
 * @version $Id: SessionTest.java,v 1.1 2002/04/21 21:15:19 chirino Exp $
 */
public class SessionTest extends PTPTestCase {
        
  /**
   * Test that an attempt to call the <code>recover()</code> method on a 
   * <strong>transacted </strong> <code>Session</code> throws a 
   * <code>javax.jms.IllegalStateException</code>.
   */  
  public void testRecoverTransactedSession() {
    try {
      // senderSession has been created as non transacted
      assertEquals(false, senderSession.getTransacted());
      // we create it again but as a transacted session
      senderSession = senderConnection.createQueueSession(true, 0);
      assertEquals(true, senderSession.getTransacted());
      senderSession.recover();
      fail("Should raise an IllegalStateException, the session is not transacted.\n");
    } catch (javax.jms.IllegalStateException e) {
    } catch (java.lang.IllegalStateException e) {
      fail ("Should raise a javax.jms.IllegalStateException, not a java.lang.IllegalStateException.\n");
    } catch (Exception e) {
      fail("Should raise a javax.jms.IllegalStateException, not a "+ e);
    }
  }

  /**
   * Test that a call to the <code>rollback()</code> method on a 
   * <strong>transacted</strong> <code>Session</code> rollbacks all
   * the messages sent in the transaction.
   */
 public void testRollbackTransactedSession() {
    try {
      // re-create senderSession as a transacted session
      senderSession = senderConnection.createQueueSession(true, 0);
      sender = senderSession.createSender(senderQueue);
      assertEquals(true, senderSession.getTransacted());
      
      TextMessage message = senderSession.createTextMessage();
      message.setText("testRollbackTransactedSession");
      // send a message within a transacted session
      sender.send (message);
      
      // rollback the transaction -> the sent message shouldn't be received
      senderSession.rollback();
      
      TextMessage m = (TextMessage) receiver.receiveNoWait();
      // test that no message has been received
      assertEquals(null, m);
    } catch (Exception e) {
      fail(e);
    }
  }

  /**
   * Test that a call to the <code>rollback()</code> method on a 
   * <strong>transacted</strong> <code>Session</code> rollbacks all
   * the messages sent in the transaction.
   */
  public void testCommitTransactedSession() {
    try {
      // re-create senderSession as a transacted session
      senderSession = senderConnection.createQueueSession(true, 0);
      sender = senderSession.createSender(senderQueue);
      assertEquals(true, senderSession.getTransacted());
      
      TextMessage message = senderSession.createTextMessage();
      message.setText("testCommitTransactedSession");
      // send a message within a transacted session
      sender.send (message);
      
      TextMessage m = (TextMessage) receiver.receiveNoWait();
      // test that no message has been received (the transaction has not been committed yet)
      assertEquals(null,m);
      
      // commit the transaction -> the sent message should be received
      senderSession.commit();
      
      m = (TextMessage) receiver.receiveNoWait();
      assertTrue(m != null);
      assertEquals("testCommitTransactedSession", m.getText());
    } catch (Exception e) {
      fail(e);
    }
  }

  /**
   * Test that an attempt to call the <code>roolback()</code> method on a 
   * <strong>non transacted</strong> <code>Session</code> throws a 
   * <code>javax.jms.IllegalStateException</code>.
   */
  public void testRollbackNonTransactedSession() {
    try {
      // senderSession has been created as non transacted in the setUp() method
      assertEquals(false, senderSession.getTransacted());
      senderSession.rollback();
      fail("Should raise an IllegalStateException, the session is not transacted.\n");
    } catch (javax.jms.IllegalStateException e) {
    } catch (java.lang.IllegalStateException e) {
      fail ("Should raise a javax.jms.IllegalStateException, not a java.lang.IllegalStateException.\n");
    } catch (Exception e) {
      fail("Should raise a javax.jms.IllegalStateException, not a "+ e);
    }
  }
  
  /**
   * Test that an attempt to call the <code>commit()</code> method on a 
   * <strong>non transacted</strong> <code>Session</code> throws a 
   * <code>javax.jms.IllegalStateException</code>.
   */
  public void testCommitNonTransactedSession() {
    try {
      // senderSession has been created as non transacted in the setUp() method
      assertEquals(false, senderSession.getTransacted());
      senderSession.commit();
      fail("Should raise an IllegalStateException, the session is not transacted.\n");
    } catch (javax.jms.IllegalStateException e) {
    } catch (java.lang.IllegalStateException e) {
      fail ("Should raise a javax.jms.IllegalStateException, not a java.lang.IllegalStateException.\n");
    } catch (Exception e) {
      fail("Should raise a javax.jms.IllegalStateException, not a "+ e);
    }
  }

  /**
   * Test that the <code>getTransacted()</code> method of a <code>Session</code> returns <code>true</code>
   * if the session is transacted, <code>false</code> else.
   */
  public void testGetTransacted() {
    try {
      // senderSession has been created as non transacted
      assertEquals(false, senderSession.getTransacted());
      // we re-create senderSession as a transacted session
      senderSession = senderConnection.createQueueSession(true, Session.AUTO_ACKNOWLEDGE);
      assertEquals(true, senderSession.getTransacted());
    } catch (Exception e) {
      fail(e);
    }
  }


  /**
   * Test that invoking the <code>acknowledge()</code> method of a received message 
   * from a closed session must throw an <code>IllegalStateException</code>.
   */
  public void testAcknowledge() {
    try {
      receiverSession = receiverConnection.createQueueSession(false, Session.CLIENT_ACKNOWLEDGE);
      receiver = receiverSession.createReceiver(receiverQueue);
      
      Message message = senderSession.createMessage();
      sender.send(message);

      Message m = receiver.receive();
      receiverSession.close();
      m.acknowledge();
      fail("§4.4.1 Invoking the acknowledge method of a received message from a closed "+
	   " session must throw an [javax.jms.]IllegalStateException.\n");
    } catch (javax.jms.IllegalStateException e) {
    } catch (JMSException e) {
      fail("Should raise a javax.jms.IllegalStateException, not a "+ e);
    } catch (java.lang.IllegalStateException e) {
      fail("§4.4.1 Invoking the acknowledge method of a received message from a closed "+
	   "session must throw an [javax.jms.]IllegalStateException, "+
	   "[not a java.lang.IllegalStateException]");
    }
  }

  /** 
   * Test that it is valid to use message objects created or received via the [closed] session with the
   * exception of a received message <code>acknowledge()</code> method.
   */
  public void testUseMessage() {
    try {
      TextMessage message = senderSession.createTextMessage();
      message.setText("testUseMessage");
      sender.send(message);
      
      TextMessage m = (TextMessage)receiver.receive();
      receiverSession.close();
      assertEquals("testUseMessage", m.getText());
    } catch (Exception e) {
      fail("§4.4.1 It is valid to continue to use message objects created or received via "+
	   "the [closed] session.\n");
    }
  }

  /**
   * Test that an attempt to use a <code>Session</code> which has been closed
   * throws a <code>javax.jms.IllegalStateException</code>.
   */
  public void testUsedClosedSession() {
    try {
      senderSession.close();
      senderSession.createMessage();
      fail ("§4.4.1 An attempt to use [a closed session] must throw a [javax.jms.]IllegalStateException.\n");
    } catch (javax.jms.IllegalStateException e) {
    } catch (JMSException e) {
      fail("Should raise a javax.jms.IllegalStateException, not a "+ e);
    } catch (java.lang.IllegalStateException e) {
      fail("Should raise a javax.jms.IllegalStateException, not a java.lang.IllegalStateException");
    }
  }

  /**
   * Test that closing a closed session does <strong>not</strong> throw
   * an exception.
   */
  public void testCloseClosedSession() {
    try {
      // senderSession is already started
      // we close it once
      senderSession.close();
      // we close it a second time
      senderSession.close();
    } catch (Exception e) {
      fail("§4.4.1 Closing a closed session must NOT throw an exception.\n");
    }
  } 

  /** 
   * Method to use this class in a Test suite
   */
  public static Test suite() {
     return new TestSuite(SessionTest.class);
   }
  
  public SessionTest(String name) {
    super(name);
  }
}
