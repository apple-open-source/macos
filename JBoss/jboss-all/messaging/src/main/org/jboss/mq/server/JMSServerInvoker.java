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

import org.jboss.logging.Logger;
import org.jboss.mq.AcknowledgementRequest;
import org.jboss.mq.ConnectionToken;
import org.jboss.mq.DurableSubscriptionID;
import org.jboss.mq.SpyDestination;
import org.jboss.mq.SpyMessage;
import org.jboss.mq.SpyTopic;
import org.jboss.mq.Subscription;
import org.jboss.mq.TransactionRequest;
import org.jboss.mq.il.Invoker;

/**
 * A pass through JMSServerInvoker.
 *
 * @author <a href="mailto:pra@tim.se">Peter Antman</a>
 * @version $Revision: 1.2.2.1 $
 */

public class JMSServerInvoker implements Invoker
{
   protected Logger log;
   /**
    * Next invoker in chain.
    */
   protected JMSServerInterceptor nextInterceptor = null;

   public JMSServerInvoker()
   {
      log = Logger.getLogger(this.getClass().getName());
   }

   /**
    * Set next invoker in chain to be called. Is mot often the real JMSServer
    */
   public void setNext(JMSServerInterceptor server)
   {
      this.nextInterceptor = server;
   }

   /**
    * @see JMSServerInterceptor#getNext()
    */
   public JMSServerInterceptor getNext()
   {
      return this.nextInterceptor;
   }

   /**
    * Get the thread group of the server.
    */
   public ThreadGroup getThreadGroup()
   {
      return nextInterceptor.getThreadGroup();
   }
   /**
    * Gets a clientID from server.
    *
    * @return               The ID value
    * @exception JMSException  Description of Exception
    */
   public String getID() throws JMSException
   {
      return nextInterceptor.getID();
   }

   /**
    * Gets the TemporaryTopic attribute of the ServerIL object
    *
    * @param dc             Description of Parameter
    * @return               The TemporaryTopic value
    * @exception JMSException  Description of Exception
    */
   public TemporaryTopic getTemporaryTopic(ConnectionToken dc) throws JMSException
   {
      return nextInterceptor.getTemporaryTopic(dc);
   }

   /**
    * Gets the TemporaryQueue attribute of the ServerIL object
    *
    * @param dc             Description of Parameter
    * @return               The TemporaryQueue value
    * @exception JMSException  Description of Exception
    */
   public TemporaryQueue getTemporaryQueue(ConnectionToken dc) throws JMSException
   {
      return nextInterceptor.getTemporaryQueue(dc);
   }

   /**
    * #Description of the Method
    *
    * @param dc             Description of Parameter
    * @exception JMSException  Description of Exception
    */
   public void connectionClosing(ConnectionToken dc) throws JMSException
   {
      nextInterceptor.connectionClosing(dc);
   }

   /**
    * Check id, must not be taken.
    *
    * @param ID             a clientID
    * @exception JMSException if ID is already taken
    */
   public void checkID(String ID) throws JMSException
   {
      nextInterceptor.checkID(ID);
   }

   /**
    * Add the message to the destination.
    *
    * @param dc             The feature to be added to the Message attribute
    * @param message        The feature to be added to the Message attribute
    * @exception JMSException  Description of Exception
    */
   public void addMessage(ConnectionToken dc, SpyMessage message) throws JMSException
   {
      nextInterceptor.addMessage(dc, message);
   }

   /**
    * #Description of the Method
    *
    * @param dc             Description of Parameter
    * @param dest           Description of Parameter
    * @return               Description of the Returned Value
    * @exception JMSException  Description of Exception
    */
   public Queue createQueue(ConnectionToken dc, String dest) throws JMSException
   {
      return nextInterceptor.createQueue(dc, dest);
   }

   /**
    * #Description of the Method
    *
    * @param dc             Description of Parameter
    * @param dest           Description of Parameter
    * @return               Description of the Returned Value
    * @exception JMSException  Description of Exception
    */
   public Topic createTopic(ConnectionToken dc, String dest) throws JMSException
   {
      return nextInterceptor.createTopic(dc, dest);
   }

   /**
    * #Description of the Method
    *
    * @param dc             Description of Parameter
    * @param dest           Description of Parameter
    * @exception JMSException  Description of Exception
    */
   public void deleteTemporaryDestination(ConnectionToken dc, SpyDestination dest) throws JMSException
   {
      nextInterceptor.deleteTemporaryDestination(dc, dest);
   }

   /**
    * #Description of the Method
    *
    * @param dc             Description of Parameter
    * @param t              Description of Parameter
    * @exception JMSException  Description of Exception
    */
   public void transact(ConnectionToken dc, TransactionRequest t) throws JMSException
   {
      nextInterceptor.transact(dc, t);
   }

   /**
    * #Description of the Method
    *
    * @param dc             Description of Parameter
    * @param item           Description of Parameter
    * @exception JMSException  Description of Exception
    */
   public void acknowledge(ConnectionToken dc, AcknowledgementRequest item) throws JMSException
   {
      nextInterceptor.acknowledge(dc, item);
   }

   /**
    * #Description of the Method
    *
    * @param dc             Description of Parameter
    * @param dest           Description of Parameter
    * @param selector       Description of Parameter
    * @return               Description of the Returned Value
    * @exception JMSException  Description of Exception
    */
   public SpyMessage[] browse(ConnectionToken dc, Destination dest, String selector) throws JMSException
   {
      return nextInterceptor.browse(dc, dest, selector);
   }

   /**
    * #Description of the Method
    *
    * @param dc             Description of Parameter
    * @param subscriberId   Description of Parameter
    * @param wait           Description of Parameter
    * @return               Description of the Returned Value
    * @exception JMSException  Description of Exception
    */
   public SpyMessage receive(ConnectionToken dc, int subscriberId, long wait) throws JMSException
   {
      return nextInterceptor.receive(dc, subscriberId, wait);
   }

   /**
    * Sets the Enabled attribute of the ServerIL object
    *
    * @param dc             The new Enabled value
    * @param enabled        The new Enabled value
    * @exception JMSException  Description of Exception
    */
   public void setEnabled(ConnectionToken dc, boolean enabled) throws JMSException
   {
      nextInterceptor.setEnabled(dc, enabled);
   }

   /**
    * #Description of the Method
    *
    * @param dc              Description of Parameter
    * @param subscriptionId  Description of Parameter
    * @exception JMSException   Description of Exception
    */
   public void unsubscribe(ConnectionToken dc, int subscriptionId) throws JMSException
   {
      nextInterceptor.unsubscribe(dc, subscriptionId);
   }

   /**
    * #Description of the Method
    *
    * @param id             Description of Parameter
    * @exception JMSException  Description of Exception
    */
   public void destroySubscription(ConnectionToken dc, DurableSubscriptionID id) throws JMSException
   {
      nextInterceptor.destroySubscription(dc, id);
   }

   /**
    * Check user for autentication.
    *
    * @param userName       Description of Parameter
    * @param password       Description of Parameter
    * @return               a clientId.
    * @exception JMSException  if user was not allowed to login
    */
   public String checkUser(String userName, String password) throws JMSException
   {
      return nextInterceptor.checkUser(userName, password);
   }

   /**
    * Check user for autentication.
    *
    * @param userName       Description of Parameter
    * @param password       Description of Parameter
    * @return               a sessionId
    * @exception JMSException  if user was not allowed to login
    */
   public String authenticate(String userName, String password) throws JMSException
   {
      return nextInterceptor.authenticate(userName, password);
   }

   /**
    * @param dc                       org.jboss.mq.ConnectionToken
    * @param s                        org.jboss.mq.Subscription
    * @exception JMSException  The exception description.
    */
   public void subscribe(org.jboss.mq.ConnectionToken dc, org.jboss.mq.Subscription s) throws JMSException
   {
      nextInterceptor.subscribe(dc, s);
   }

   /**
    * #Description of the Method
    *
    * @param dc             Description of Parameter
    * @param clientTime     Description of Parameter
    * @exception JMSException  Description of Exception
    */
   public void ping(ConnectionToken dc, long clientTime) throws JMSException
   {
      nextInterceptor.ping(dc, clientTime);
   }

   public SpyTopic getDurableTopic(DurableSubscriptionID sub) throws JMSException
   {
      return nextInterceptor.getDurableTopic(sub);
   }

   public Subscription getSubscription(ConnectionToken dc, int subscriberId) throws JMSException
   {
      return nextInterceptor.getSubscription(dc, subscriberId);
   }

} // JMSServerInvokerSupport
