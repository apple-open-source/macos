/*
* JBoss, the OpenSource EJB server
*
* Distributable under LGPL license.
* See terms of license at gnu.org.
*/
package org.jboss.mq.server;

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
import org.jboss.mq.SpyTopic;
import org.jboss.mq.Subscription;
import org.jboss.mq.TransactionRequest;

/**
 * Interceptor interface for clients IL accessing the JMSServer.
 *
 * Using an iterface for this layer makes it possible to put in logic
 * without having to modify the server objet. And also makes this
 * pluggable.
 *
 *
 * @author <a href="mailto:pra@tim.se">Peter Antman</a>
 * @version $Revision: 1.1.4.1 $
 */

public interface JMSServerInterceptor
{

   /**
    * Set next Interceptor in chain to be called. Is mot often the real JMSServer
    */
   public void setNext(JMSServerInterceptor server);

   /**
    * Get next invoker in chain to be called. Is mot often the real JMSServer
    */
   public JMSServerInterceptor getNext();

   /**
    * Get the thread group of the server.
    */
   public ThreadGroup getThreadGroup();
   /**
    * Gets a clientID from server.
    *
    * @return               The ID value
    * @exception JMSException  Description of Exception
    */
   public String getID() throws JMSException;

   /**
    * Get a temporary topic.
    *
    * @param dc             Description of Parameter
    * @return               The TemporaryTopic value
    * @exception JMSException  Description of Exception
    */
   public TemporaryTopic getTemporaryTopic(ConnectionToken dc) throws JMSException;

   /**
    * Get a temporary queue
    *
    * @param dc             Description of Parameter
    * @return               The TemporaryQueue value
    * @exception JMSException  Description of Exception
    */
   public TemporaryQueue getTemporaryQueue(ConnectionToken dc) throws JMSException;

   /**
    * Close connection.    
    *
    * @param dc             Description of Parameter
    * @exception JMSException  Description of Exception
    */
   public void connectionClosing(ConnectionToken dc) throws JMSException;

   /**
    * Check id, must not be taken.
    *
    * @param ID             Description of Parameter
    * @exception JMSException  Description of Exception
    */
   public void checkID(String ID) throws JMSException;

   /**
    * Add the message to the destination.
    *
    * @param dc             The feature to be added to the Message attribute
    * @param message        The feature to be added to the Message attribute
    * @exception JMSException  Description of Exception
    */
   public void addMessage(ConnectionToken dc, SpyMessage message) throws JMSException;

   /**
    * Create a queue. 
    *
    * The destination name must be the name of an already existing
    * destination. This method should only be used to skip looking
    * up a destination through JNDI, not to actually create a new destination.
    *
    * @param dc             Description of Parameter
    * @param dest           Description of Parameter
    * @return               Description of the Returned Value
    * @exception JMSException  Description of Exception
    */
   public Queue createQueue(ConnectionToken dc, String dest) throws JMSException;

   /**
    * Create a topic. 
    *
    * The destination name must be the name of an already existing
    * destination. This method should only be used to skip looking
    * up a destination through JNDI, not to actually create a new destination.
    *
    * @param dc             Description of Parameter
    * @param dest           Description of Parameter
    * @return               Description of the Returned Value
    * @exception JMSException  Description of Exception
    */
   public Topic createTopic(ConnectionToken dc, String dest) throws JMSException;

   /**
    * #Description of the Method
    *
    * @param dc             Description of Parameter
    * @param dest           Description of Parameter
    * @exception JMSException  Description of Exception
    */
   public void deleteTemporaryDestination(ConnectionToken dc, SpyDestination dest) throws JMSException;

   /**
    * #Description of the Method
    *
    * @param dc             Description of Parameter
    * @param t              Description of Parameter
    * @exception JMSException  Description of Exception
    */
   public void transact(ConnectionToken dc, TransactionRequest t) throws JMSException;

   /**
    * #Description of the Method
    *
    * @param dc             Description of Parameter
    * @param item           Description of Parameter
    * @exception JMSException  Description of Exception
    */
   public void acknowledge(ConnectionToken dc, AcknowledgementRequest item) throws JMSException;

   /**
    * #Description of the Method
    *
    * @param dc             Description of Parameter
    * @param dest           Description of Parameter
    * @param selector       Description of Parameter
    * @return               Description of the Returned Value
    * @exception JMSException  Description of Exception
    */
   public SpyMessage[] browse(ConnectionToken dc, Destination dest, String selector) throws JMSException;

   /**
    * #Description of the Method
    *
    * @param dc             Description of Parameter
    * @param subscriberId   Description of Parameter
    * @param wait           Description of Parameter
    * @return               Description of the Returned Value
    * @exception JMSException  Description of Exception
    */
   public SpyMessage receive(ConnectionToken dc, int subscriberId, long wait) throws JMSException;

   /**
    * Sets the Enabled attribute of the ServerIL object
    *
    * @param dc             The new Enabled value
    * @param enabled        The new Enabled value
    * @exception JMSException  Description of Exception
    */
   public void setEnabled(ConnectionToken dc, boolean enabled) throws JMSException;

   /**
    * Close the server side message consumer. Client is no longer
    * available to receive messages.
    *
    * @param dc              Description of Parameter
    * @param subscriptionId  Description of Parameter
    * @exception JMSException   Description of Exception
    */
   public void unsubscribe(ConnectionToken dc, int subscriptionId) throws JMSException;

   /**
    * Unsubscribe from the durable subscription.
    *
    * @param id             Description of Parameter
    * @exception JMSException  Description of Exception
    */
   public void destroySubscription(ConnectionToken dc, DurableSubscriptionID id) throws JMSException;

   /**
    * Check user for autentication.
    *
    * @param userName       Description of Parameter
    * @param password       Description of Parameter
    * @return               a preconfigured clientId.
    * @exception JMSException  if user was not allowed to login
    */
   public String checkUser(String userName, String password) throws JMSException;

   /**
    * Check user for autentication.
    *
    * @param userName       Description of Parameter
    * @param password       Description of Parameter
    * @return               a sessionId.
    * @exception JMSException  if user was not allowed to login
    */
   public String authenticate(String userName, String password) throws JMSException;

   /**
    * @param dc                       org.jboss.mq.ConnectionToken
    * @param s                        org.jboss.mq.Subscription
    * @exception JMSException  The exception description.
    */
   void subscribe(org.jboss.mq.ConnectionToken dc, org.jboss.mq.Subscription s) throws JMSException;

   /**
    * Ping the server.
    *
    * @param dc             Description of Parameter
    * @param clientTime     Description of Parameter
    * @exception JMSException  Description of Exception
    */
   public void ping(ConnectionToken dc, long clientTime) throws JMSException;

   /**
    * Get the topic the durable subscription is on.
    * Primary for internal use in the server, and not for the IL's.
    */
   public SpyTopic getDurableTopic(DurableSubscriptionID sub) throws JMSException;

   /**
    * Get the subscription that match the id.
    *
    * @exception JMSException if it can not find the subscription.
    */
   public Subscription getSubscription(ConnectionToken dc, int subscriberId) throws JMSException;
}
