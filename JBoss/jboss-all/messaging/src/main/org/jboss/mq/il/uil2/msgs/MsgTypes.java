/*
 * JBoss, the OpenSource J2EE WebOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.mq.il.uil2.msgs;

/**
 * @author Scott.Stark@jboss.org
 * @version $Revision: 1.1.4.1 $
 */
public interface MsgTypes
{
   final static int m_acknowledge = 1;
   final static int m_addMessage = 2;
   final static int m_browse = 3;
   final static int m_checkID = 4;
   final static int m_connectionClosing = 5;
   final static int m_createQueue = 6;
   final static int m_createTopic = 7;
   final static int m_deleteTemporaryDestination = 8;
   final static int m_getID = 9;
   final static int m_getTemporaryQueue = 10;
   final static int m_getTemporaryTopic = 11;
   final static int m_receive = 13;
   final static int m_setEnabled = 14;
   final static int m_setSpyDistributedConnection = 15;
   final static int m_subscribe = 16;
   final static int m_transact = 17;
   final static int m_unsubscribe = 18;
   final static int m_destroySubscription = 19;
   final static int m_checkUser = 20;
   final static int m_ping = 21;
   final static int m_authenticate = 22;
   final static int m_close = 23;
   final static int m_pong = 24;
   final static int m_receiveRequest = 25;

}
