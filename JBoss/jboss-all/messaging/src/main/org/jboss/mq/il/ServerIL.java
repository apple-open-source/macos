/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.mq.il;
import javax.jms.Destination;

import javax.jms.JMSException;
import javax.jms.Queue;
import javax.jms.TemporaryQueue;
import javax.jms.TemporaryTopic;
import javax.jms.Topic;
import org.jboss.mq.AcknowledgementRequest;
import org.jboss.mq.ConnectionToken;
import org.jboss.mq.DurableSubscriptionID;

import org.jboss.mq.SpyDestination;
import org.jboss.mq.SpyMessage;
import org.jboss.mq.TransactionRequest;

/**
 * Defines the methods that can be called by a client on the server.
 *
 * @author    <a href="Cojonudo14@hotmail.com">Hiram Chirino</a>
 * @author    <a href="Norbert.Lataille@m4x.org">Norbert Lataille</a>
 * @author     <a href="pra@tim.se">Peter Antman</a>
 * @version   $Revision: 1.6 $
 * @created   August 16, 2001
 */
public interface ServerIL
{

   /**
    * #Description of the Method
    *
    * @return               Description of the Returned Value
    * @exception Exception  Description of Exception
    */
   public ServerIL cloneServerIL() throws Exception;

   /**
    * @param newConnectionToken       org.jboss.mq.ConnectionToken
    * @exception java.lang.Exception  The exception description.
    */
   void setConnectionToken(org.jboss.mq.ConnectionToken newConnectionToken) throws java.lang.Exception;
   /**
    * Get a clientID from the server. It is up to the server side components
    * to update the ConectionToken if this method returns normaly.
    *
    * @return               an internally generated clientID.
    * @exception Exception  Description of Exception
    */
   public String getID()
          throws Exception;

   /**
    * Gets the TemporaryTopic attribute of the ServerIL object
    *
    * @param dc             Description of Parameter
    * @return               The TemporaryTopic value
    * @exception Exception  Description of Exception
    */
   public TemporaryTopic getTemporaryTopic(ConnectionToken dc)
          throws Exception;

   /**
    * Gets the TemporaryQueue attribute of the ServerIL object
    *
    * @param dc             Description of Parameter
    * @return               The TemporaryQueue value
    * @exception Exception  Description of Exception
    */
   public TemporaryQueue getTemporaryQueue(ConnectionToken dc)
          throws Exception;

   /**
    * The client is closing the connection.
    *
    * @param dc             Description of Parameter
    * @exception Exception  Description of Exception
    */
   public void connectionClosing(ConnectionToken dc)
          throws Exception;

   /**
    * Check if clientID is a valid ID. 
    *
    * This method should be called when the client tries to set its own
    * clientID. It is up to the server side components
    * to update the ConectionToken if this method returns normaly.
    *
    * @param ID             a clientID set by the client.
    * @exception Exception  if the clientID was not vallid.
    */
   public void checkID(String ID)
          throws Exception;

   /**
    * Adds a message to the destination specifyed in the message.
    *
    * @param dc             The feature to be added to the Message attribute
    * @param message        The feature to be added to the Message attribute
    * @exception Exception  Description of Exception
    */
   public void addMessage(ConnectionToken dc, SpyMessage message)
          throws Exception;

   /**
    * #Description of the Method
    *
    * @param dc             Description of Parameter
    * @param dest           Description of Parameter
    * @return               Description of the Returned Value
    * @exception Exception  Description of Exception
    */
   public Queue createQueue(ConnectionToken dc, String dest)
          throws Exception;

   /**
    * #Description of the Method
    *
    * @param dc             Description of Parameter
    * @param dest           Description of Parameter
    * @return               Description of the Returned Value
    * @exception Exception  Description of Exception
    */
   public Topic createTopic(ConnectionToken dc, String dest)
          throws Exception;

   /**
    * #Description of the Method
    *
    * @param dc             Description of Parameter
    * @param dest           Description of Parameter
    * @exception Exception  Description of Exception
    */
   public void deleteTemporaryDestination(ConnectionToken dc, SpyDestination dest)
          throws Exception;

   /**
    * #Description of the Method
    *
    * @param dc             Description of Parameter
    * @param t              Description of Parameter
    * @exception Exception  Description of Exception
    */
   public void transact(ConnectionToken dc, TransactionRequest t)
          throws Exception;

   /**
    * #Description of the Method
    *
    * @param dc             Description of Parameter
    * @param item           Description of Parameter
    * @exception Exception  Description of Exception
    */
   public void acknowledge(ConnectionToken dc, AcknowledgementRequest item)
          throws Exception;

   /**
    * #Description of the Method
    *
    * @param dc             Description of Parameter
    * @param dest           Description of Parameter
    * @param selector       Description of Parameter
    * @return               Description of the Returned Value
    * @exception Exception  Description of Exception
    */
   public SpyMessage[] browse(ConnectionToken dc, Destination dest, String selector)
          throws Exception;

   /**
    * Get a message synchronously.
    *
    * @param dc             Description of Parameter
    * @param subscriberId   Description of Parameter
    * @param wait           Description of Parameter
    * @return               Description of the Returned Value
    * @exception Exception  Description of Exception
    */
   public SpyMessage receive(ConnectionToken dc, int subscriberId, long wait)
          throws Exception;

   /**
    * Sets the Enabled attribute of the ServerIL object
    *
    * @param dc             The new Enabled value
    * @param enabled        The new Enabled value
    * @exception Exception  Description of Exception
    */
   public void setEnabled(ConnectionToken dc, boolean enabled)
          throws Exception;

   /**
    * Remove a consumer. Is NOT the same as the topic session unsubscribe.
    *
    * @param dc              Description of Parameter
    * @param subscriptionId  Description of Parameter
    * @exception Exception   Description of Exception
    */
   public void unsubscribe(ConnectionToken dc, int subscriptionId)
          throws Exception;

   /**
    * Unsubscribe from a durable subscription.
    *
    * @param id             Description of Parameter
    * @exception Exception  Description of Exception
    */
   public void destroySubscription(ConnectionToken dc,DurableSubscriptionID id)
          throws Exception;

   /**
    * Get a clientID for a given user/password. 
    *
    * May also be used for autentication if StateManager is used as
    * authenticator.
    *
    * @param userName        a valid user name that the StateManager knows about.
    * @param password        a password
    * @return               a preconfigured clientID or null.
    * @exception Exception  Description of Exception
    */
   public String checkUser(String userName, String password)
          throws Exception;

   /**
    * Authenticate the user.
    *
    * If using a securityManager the user will be autenticated by that.
    *
    * @param userName       a username.
    * @param password       a password.
    * @return               a sessionid, valid only for the life of this connection.
    * @exception Exception  Description of Exception
    */
   public String authenticate(String userName, String password)
          throws Exception;
   
   
   
   /**
    * @param dc                       org.jboss.mq.ConnectionToken
    * @param s                        org.jboss.mq.Subscription
    * @exception java.lang.Exception  The exception description.
    */
   void subscribe(org.jboss.mq.ConnectionToken dc, org.jboss.mq.Subscription s)
          throws java.lang.Exception;

   /**
    * Ping the server.
    *
    * @param dc             Description of Parameter
    * @param clientTime     Description of Parameter
    * @exception Exception  Description of Exception
    */
   public void ping(ConnectionToken dc, long clientTime)
          throws Exception;

}