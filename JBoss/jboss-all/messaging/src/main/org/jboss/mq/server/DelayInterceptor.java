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
 * A pass through Interceptor, which delays all calls.
 * Used to simulate a busy server which cannot response to all 
 * calls imeadiately.
 * 
 * @author    <a href="mailto:hiram.chirino@jboss.org">Hiram Chirino</a>
 * @version $Revision: 1.3 $
 */

public class DelayInterceptor extends JMSServerInterceptorSupport
{

   public boolean delayEnabled = false;
   public long delayTime = 50;

   /**
    * Get the thread group of the server.
    */
   public ThreadGroup getThreadGroup()
   {
      if (delayEnabled)
      {
         try
         {
            Thread.sleep(delayTime);
         }
         catch (InterruptedException e)
         {
            Thread.currentThread().interrupt();
         }
      }
      return getNext().getThreadGroup();
   }

   /**
    * Gets a clientID from server.
    *
    * @return               The ID value
    * @exception JMSException  Description of Exception
    */
   public String getID() throws JMSException
   {
      if (delayEnabled)
      {
         try
         {
            Thread.sleep(delayTime);
         }
         catch (InterruptedException e)
         {
            Thread.currentThread().interrupt();
         }
      }
      return getNext().getID();
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
      if (delayEnabled)
      {
         try
         {
            Thread.sleep(delayTime);
         }
         catch (InterruptedException e)
         {
            Thread.currentThread().interrupt();
         }
      }
      return getNext().getTemporaryTopic(dc);
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
      if (delayEnabled)
      {
         try
         {
            Thread.sleep(delayTime);
         }
         catch (InterruptedException e)
         {
            Thread.currentThread().interrupt();
         }
      }

      return getNext().getTemporaryQueue(dc);
   }

   /**
    * #Description of the Method
    *
    * @param dc             Description of Parameter
    * @exception JMSException  Description of Exception
    */
   public void connectionClosing(ConnectionToken dc) throws JMSException
   {

      if (delayEnabled)
      {
         try
         {
            Thread.sleep(delayTime);
         }
         catch (InterruptedException e)
         {
            Thread.currentThread().interrupt();
         }
      }
      getNext().connectionClosing(dc);
   }

   /**
    * Check id, must not be taken.
    *
    * @param ID             a clientID
    * @exception JMSException if ID is already taken
    */
   public void checkID(String ID) throws JMSException
   {

      if (delayEnabled)
      {
         try
         {
            Thread.sleep(delayTime);
         }
         catch (InterruptedException e)
         {
            Thread.currentThread().interrupt();
         }
      }
      getNext().checkID(ID);
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

      if (delayEnabled)
      {
         try
         {
            Thread.sleep(delayTime);
         }
         catch (InterruptedException e)
         {
            Thread.currentThread().interrupt();
         }
      }
      getNext().addMessage(dc, message);
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

      if (delayEnabled)
      {
         try
         {
            Thread.sleep(delayTime);
         }
         catch (InterruptedException e)
         {
            Thread.currentThread().interrupt();
         }
      }
      return getNext().createQueue(dc, dest);
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

      if (delayEnabled)
      {
         try
         {
            Thread.sleep(delayTime);
         }
         catch (InterruptedException e)
         {
            Thread.currentThread().interrupt();
         }
      }
      return getNext().createTopic(dc, dest);
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

      if (delayEnabled)
      {
         try
         {
            Thread.sleep(delayTime);
         }
         catch (InterruptedException e)
         {
            Thread.currentThread().interrupt();
         }
      }
      getNext().deleteTemporaryDestination(dc, dest);
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

      if (delayEnabled)
      {
         try
         {
            Thread.sleep(delayTime);
         }
         catch (InterruptedException e)
         {
            Thread.currentThread().interrupt();
         }
      }
      getNext().transact(dc, t);
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

      if (delayEnabled)
      {
         try
         {
            Thread.sleep(delayTime);
         }
         catch (InterruptedException e)
         {
            Thread.currentThread().interrupt();
         }
      }
      getNext().acknowledge(dc, item);
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

      if (delayEnabled)
      {
         try
         {
            Thread.sleep(delayTime);
         }
         catch (InterruptedException e)
         {
            Thread.currentThread().interrupt();
         }
      }
      return getNext().browse(dc, dest, selector);
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

      if (delayEnabled)
      {
         try
         {
            Thread.sleep(delayTime);
         }
         catch (InterruptedException e)
         {
            Thread.currentThread().interrupt();
         }
      }
      return getNext().receive(dc, subscriberId, wait);
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

      if (delayEnabled)
      {
         try
         {
            Thread.sleep(delayTime);
         }
         catch (InterruptedException e)
         {
            Thread.currentThread().interrupt();
         }
      }
      getNext().setEnabled(dc, enabled);
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

      if (delayEnabled)
      {
         try
         {
            Thread.sleep(delayTime);
         }
         catch (InterruptedException e)
         {
            Thread.currentThread().interrupt();
         }
      }
      getNext().unsubscribe(dc, subscriptionId);
   }

   /**
    * #Description of the Method
    *
    * @param id             Description of Parameter
    * @exception JMSException  Description of Exception
    */
   public void destroySubscription(ConnectionToken dc, DurableSubscriptionID id) throws JMSException
   {

      if (delayEnabled)
      {
         try
         {
            Thread.sleep(delayTime);
         }
         catch (InterruptedException e)
         {
            Thread.currentThread().interrupt();
         }
      }
      getNext().destroySubscription(dc, id);
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
      if (delayEnabled)
      {
         try
         {
            Thread.sleep(delayTime);
         }
         catch (InterruptedException e)
         {
            Thread.currentThread().interrupt();
         }
      }
      return getNext().checkUser(userName, password);
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

      if (delayEnabled)
      {
         try
         {
            Thread.sleep(delayTime);
         }
         catch (InterruptedException e)
         {
            Thread.currentThread().interrupt();
         }
      }
      return getNext().authenticate(userName, password);
   }

   /**
    * @param dc                       org.jboss.mq.ConnectionToken
    * @param s                        org.jboss.mq.Subscription
    * @exception JMSException  The exception description.
    */
   public void subscribe(org.jboss.mq.ConnectionToken dc, org.jboss.mq.Subscription s) throws JMSException
   {

      if (delayEnabled)
      {
         try
         {
            Thread.sleep(delayTime);
         }
         catch (InterruptedException e)
         {
            Thread.currentThread().interrupt();
         }
      }
      getNext().subscribe(dc, s);
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

      if (delayEnabled)
      {
         try
         {
            Thread.sleep(delayTime);
         }
         catch (InterruptedException e)
         {
            Thread.currentThread().interrupt();
         }
      }
      getNext().ping(dc, clientTime);

   }

   public SpyTopic getDurableTopic(DurableSubscriptionID sub) throws JMSException
   {

      if (delayEnabled)
      {
         try
         {
            Thread.sleep(delayTime);
         }
         catch (InterruptedException e)
         {
            Thread.currentThread().interrupt();
         }
      }
      return getNext().getDurableTopic(sub);
   }

   public Subscription getSubscription(ConnectionToken dc, int subscriberId) throws JMSException
   {
      if (delayEnabled)
      {
         try
         {
            Thread.sleep(delayTime);
         }
         catch (InterruptedException e)
         {
            Thread.currentThread().interrupt();
         }
      }
      return getNext().getSubscription(dc, subscriberId);
   }

   public boolean isDelayEnabled()
   {
      return delayEnabled;
   }

   public long getDelayTime()
   {
      return delayTime;
   }

   public void setDelayEnabled(boolean delayEnabled)
   {
      this.delayEnabled = delayEnabled;
   }

   public void setDelayTime(long delayTime)
   {
      this.delayTime = delayTime;
   }

}
