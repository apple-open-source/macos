/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.mq.il.rmi;

import org.jboss.mq.il.ServerIL;

/**
 * The JVM implementation of the ServerIL object
 *
 * @author    Hiram Chirino (Cojonudo14@hotmail.com)
 * @author    Norbert Lataille (Norbert.Lataille@m4x.org)
 * @version   $Revision: 1.4 $
 * @created   August 16, 2001
 */
public interface RMIServerILRemote extends ServerIL, java.rmi.Remote
{

//	public ServerIL cloneServerIL()  throws Exception;
//	public void setConnectionToken(ConnectionToken newConnectionToken) throws Exception;
//
//	public String getID()  throws Exception;
//	public void addMessage(ConnectionToken dc, SpyMessage val)  throws Exception;
//	public Topic createTopic(ConnectionToken dc, String dest)  throws Exception;
//	public Queue createQueue(ConnectionToken dc, String dest)  throws Exception;
//	public void deleteTemporaryDestination(ConnectionToken dc, SpyDestination dest)  throws Exception;
//	public void checkID(String ID)  throws Exception;
//	public void connectionClosing(ConnectionToken dc)  throws Exception;
//	public TemporaryQueue getTemporaryQueue(ConnectionToken dc) throws Exception;
//	public TemporaryTopic getTemporaryTopic(ConnectionToken dc) throws Exception;
//	public void acknowledge(ConnectionToken dc, AcknowledgementRequest item) throws Exception;
//	public SpyMessage[] browse(ConnectionToken dc, Destination dest, String selector) throws Exception;
//	public void listenerChange(ConnectionToken dc, int subscriberId, boolean state) throws Exception;
//	public SpyMessage receive(ConnectionToken dc, int subscriberId, long wait) throws Exception;
//	public void setEnabled(ConnectionToken dc, boolean enabled) throws Exception;
//	public void unsubscribe(ConnectionToken dc, int subscriptionId) throws Exception;
//	public String checkUser(String userName, String password)  throws Exception;
//	public void subscribe(ConnectionToken dc, org.jboss.mq.Subscription s)  throws Exception;
//	public void transact(org.jboss.mq.ConnectionToken dc, TransactionRequest t)  throws Exception;
//  public void ping(org.jboss.mq.ConnectionToken dc, long clientTime) throws Exception;
}
