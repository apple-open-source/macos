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

import org.apache.log4j.NDC;
import org.jboss.logging.Logger;
import org.jboss.mq.AcknowledgementRequest;
import org.jboss.mq.ConnectionToken;
import org.jboss.mq.DurableSubscriptionID;
import org.jboss.mq.SpyDestination;
import org.jboss.mq.SpyMessage;
import org.jboss.mq.SpyTopic;
import org.jboss.mq.Subscription;
import org.jboss.mq.TransactionRequest;

/**
 * A pass through Interceptor, wich will trace all calls.
 *
 * @author <a href="mailto:pra@tim.se">Peter Antman</a>
 * @version $Revision: 1.3.2.4 $
 */

public class TracingInterceptor extends JMSServerInterceptorSupport
{

   static protected Logger log = Logger.getLogger(TracingInterceptor.class);

   /**
    * Get the thread group of the server.
    */
   public ThreadGroup getThreadGroup()
   {
      if (!log.isTraceEnabled())
      {
         return getNext().getThreadGroup();
      }

      try
      {
         log.trace("CALLED : getThreadGroup");
         return getNext().getThreadGroup();
      }
      finally
      {
         log.trace("RETURN : getThreadGroup");
      }
   }

   /**
    * Gets a clientID from server.
    *
    * @return               The ID value
    * @exception JMSException  Description of Exception
    */
   public String getID() throws JMSException
   {

      if (!log.isTraceEnabled())
      {
         return getNext().getID();
      }

      try
      {
         log.trace("CALLED : getID");
         return getNext().getID();
      }
      catch (JMSException e)
      {
         throw e;
      }
      catch (RuntimeException e)
      {
         log.trace("EXCEPTION : getID: ", e);
         throw e;
      }
      finally
      {
         log.trace("RETURN : getID");
      }
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

      if (!log.isTraceEnabled())
      {
         return getNext().getTemporaryTopic(dc);
      }

      try
      {
         NDC.push(dc.getClientID());
         log.trace("CALLED : getTemporaryTopic");
         return getNext().getTemporaryTopic(dc);
      }
      catch (JMSException e)
      {
         log.trace("EXCEPTION : getTemporaryTopic: ", e);
         throw e;
      }
      catch (RuntimeException e)
      {
         log.trace("EXCEPTION : getTemporaryTopic: ", e);
         throw e;
      }
      finally
      {
         log.trace("RETURN : getTemporaryTopic");
         NDC.pop();
         NDC.remove();
      }

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

      if (!log.isTraceEnabled())
      {
         return getNext().getTemporaryQueue(dc);
      }

      try
      {
         NDC.push(dc.getClientID());
         log.trace("CALLED : getTemporaryQueue");
         return getNext().getTemporaryQueue(dc);
      }
      catch (JMSException e)
      {
         log.trace("EXCEPTION : getTemporaryQueue: ", e);
         throw e;
      }
      catch (RuntimeException e)
      {
         log.trace("EXCEPTION : getTemporaryQueue: ", e);
         throw e;
      }
      finally
      {
         log.trace("RETURN : getTemporaryQueue");
         NDC.pop();
         NDC.remove();
      }

   }

   /**
    * #Description of the Method
    *
    * @param dc             Description of Parameter
    * @exception JMSException  Description of Exception
    */
   public void connectionClosing(ConnectionToken dc) throws JMSException
   {

      if (!log.isTraceEnabled())
      {
         getNext().connectionClosing(dc);
         return;
      }

      try
      {
         if (dc != null)
            NDC.push(dc.getClientID());
         log.trace("CALLED : connectionClosing");
         getNext().connectionClosing(dc);
         return;
      }
      catch (JMSException e)
      {
         log.trace("EXCEPTION : connectionClosing: ", e);
         throw e;
      }
      catch (RuntimeException e)
      {
         log.trace("EXCEPTION : receive: ", e);
         throw e;
      }
      finally
      {
         log.trace("RETURN : connectionClosing");
         if (dc != null)
         {
            NDC.pop();
            NDC.remove();
         }
      }

   }

   /**
    * Check id, must not be taken.
    *
    * @param ID             a clientID
    * @exception JMSException if ID is already taken
    */
   public void checkID(String ID) throws JMSException
   {

      if (!log.isTraceEnabled())
      {
         getNext().checkID(ID);
         return;
      }

      try
      {
         log.trace("CALLED : checkID");
         log.trace("ARG    : " + ID);
         getNext().checkID(ID);
         return;
      }
      catch (JMSException e)
      {
         log.trace("EXCEPTION : checkID: ", e);
         throw e;
      }
      catch (RuntimeException e)
      {
         log.trace("EXCEPTION : checkID: ", e);
         throw e;
      }
      finally
      {
         log.trace("RETURN : checkID");
      }

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

      if (!log.isTraceEnabled())
      {
         getNext().addMessage(dc, message);
         return;
      }

      try
      {
         NDC.push(dc.getClientID());
         log.trace("CALLED : addMessage");
         log.trace("ARG    : " + message);
         getNext().addMessage(dc, message);
         return;
      }
      catch (JMSException e)
      {
         log.trace("EXCEPTION : addMessage: ", e);
         throw e;
      }
      catch (RuntimeException e)
      {
         log.trace("EXCEPTION : addMessage: ", e);
         throw e;
      }
      finally
      {
         log.trace("RETURN : addMessage");
         NDC.pop();
         NDC.remove();
      }

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

      if (!log.isTraceEnabled())
      {
         return getNext().createQueue(dc, dest);
      }

      try
      {
         NDC.push(dc.getClientID());
         log.trace("CALLED : createQueue");
         log.trace("ARG    : " + dest);
         return getNext().createQueue(dc, dest);
      }
      catch (JMSException e)
      {
         log.trace("EXCEPTION : createQueue: ", e);
         throw e;
      }
      catch (RuntimeException e)
      {
         log.trace("EXCEPTION : createQueue: ", e);
         throw e;
      }
      finally
      {
         log.trace("RETURN : createQueue");
         NDC.pop();
         NDC.remove();
      }

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

      if (!log.isTraceEnabled())
      {
         return getNext().createTopic(dc, dest);
      }

      try
      {
         NDC.push(dc.getClientID());
         log.trace("CALLED : createTopic");
         log.trace("ARG    : " + dest);
         return getNext().createTopic(dc, dest);
      }
      catch (JMSException e)
      {
         log.trace("EXCEPTION : createTopic: ", e);
         throw e;
      }
      catch (RuntimeException e)
      {
         log.trace("EXCEPTION : createTopic: ", e);
         throw e;
      }
      finally
      {
         log.trace("RETURN : createTopic");
         NDC.pop();
         NDC.remove();
      }
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

      if (!log.isTraceEnabled())
      {
         getNext().deleteTemporaryDestination(dc, dest);
         return;
      }

      try
      {
         NDC.push(dc.getClientID());
         log.trace("CALLED : deleteTemporaryDestination");
         log.trace("ARG    : " + dest);
         getNext().deleteTemporaryDestination(dc, dest);
         return;
      }
      catch (JMSException e)
      {
         log.trace("EXCEPTION : deleteTemporaryDestination: ", e);
         throw e;
      }
      catch (RuntimeException e)
      {
         log.trace("EXCEPTION : deleteTemporaryDestination: ", e);
         throw e;
      }
      finally
      {
         log.trace("RETURN : deleteTemporaryDestination");
         NDC.pop();
         NDC.remove();
      }
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

      if (!log.isTraceEnabled())
      {
         getNext().transact(dc, t);
         return;
      }

      try
      {
         NDC.push(dc.getClientID());
         log.trace("CALLED : transact");
         log.trace("ARG    : " + t);
         getNext().transact(dc, t);
         return;
      }
      catch (JMSException e)
      {
         log.trace("EXCEPTION : transact: ", e);
         throw e;
      }
      catch (RuntimeException e)
      {
         log.trace("EXCEPTION : transact: ", e);
         throw e;
      }
      finally
      {
         log.trace("RETURN : transact");
         NDC.pop();
         NDC.remove();
      }

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

      if (!log.isTraceEnabled())
      {
         getNext().acknowledge(dc, item);
         return;
      }

      try
      {
         NDC.push(dc.getClientID());
         log.trace("CALLED : acknowledge");
         log.trace("ARG    : " + item);
         getNext().acknowledge(dc, item);
         return;
      }
      catch (JMSException e)
      {
         log.trace("EXCEPTION : acknowledge: ", e);
         throw e;
      }
      catch (RuntimeException e)
      {
         log.trace("EXCEPTION : acknowledge: ", e);
         throw e;
      }
      finally
      {
         log.trace("RETURN : acknowledge");
         NDC.pop();
         NDC.remove();
      }

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

      if (!log.isTraceEnabled())
      {
         return getNext().browse(dc, dest, selector);
      }

      try
      {
         NDC.push(dc.getClientID());
         log.trace("CALLED : browse");
         log.trace("ARG    : " + dest);
         log.trace("ARG    : " + selector);
         return getNext().browse(dc, dest, selector);
      }
      catch (JMSException e)
      {
         log.trace("EXCEPTION : browse: ", e);
         throw e;
      }
      catch (RuntimeException e)
      {
         log.trace("EXCEPTION : browse: ", e);
         throw e;
      }
      finally
      {
         log.trace("RETURN : browse");
         NDC.pop();
         NDC.remove();
      }

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

      if (!log.isTraceEnabled())
      {
         return getNext().receive(dc, subscriberId, wait);
      }

      try
      {
         NDC.push(dc.getClientID());
         log.trace("CALLED : receive");
         log.trace("ARG    : " + subscriberId);
         log.trace("ARG    : " + wait);
         return getNext().receive(dc, subscriberId, wait);
      }
      catch (JMSException e)
      {
         log.trace("EXCEPTION : receive: ", e);
         throw e;
      }
      catch (RuntimeException e)
      {
         log.trace("EXCEPTION : receive: ", e);
         throw e;
      }
      finally
      {
         log.trace("RETURN : receive");
         NDC.pop();
         NDC.remove();
      }

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

      if (!log.isTraceEnabled())
      {
         getNext().setEnabled(dc, enabled);
         return;
      }

      try
      {
         NDC.push(dc.getClientID());
         log.trace("CALLED : setEnabled");
         log.trace("ARG    : " + enabled);
         getNext().setEnabled(dc, enabled);
         return;
      }
      catch (JMSException e)
      {
         log.trace("EXCEPTION : setEnabled: ", e);
         throw e;
      }
      catch (RuntimeException e)
      {
         log.trace("EXCEPTION : setEnabled: ", e);
         throw e;
      }
      finally
      {
         log.trace("RETURN : setEnabled");
         NDC.pop();
         NDC.remove();
      }

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

      if (!log.isTraceEnabled())
      {
         getNext().unsubscribe(dc, subscriptionId);
         return;
      }

      try
      {
         NDC.push(dc.getClientID());
         log.trace("CALLED : unsubscribe");
         log.trace("ARG    : " + subscriptionId);
         getNext().unsubscribe(dc, subscriptionId);
         return;
      }
      catch (JMSException e)
      {
         log.trace("EXCEPTION : unsubscribe: ", e);
         throw e;
      }
      catch (RuntimeException e)
      {
         log.trace("EXCEPTION : unsubscribe: ", e);
         throw e;
      }
      finally
      {
         log.trace("RETURN : unsubscribe");
         NDC.pop();
         NDC.remove();
      }

   }

   /**
    * #Description of the Method
    *
    * @param id             Description of Parameter
    * @exception JMSException  Description of Exception
    */
   public void destroySubscription(ConnectionToken dc, DurableSubscriptionID id) throws JMSException
   {

      if (!log.isTraceEnabled())
      {
         getNext().destroySubscription(dc, id);
         return;
      }

      try
      {
         NDC.push(dc.getClientID());
         log.trace("CALLED : destroySubscription");
         log.trace("ARG    : " + id);
         getNext().destroySubscription(dc, id);
         return;
      }
      catch (JMSException e)
      {
         log.trace("EXCEPTION : destroySubscription: ", e);
         throw e;
      }
      catch (RuntimeException e)
      {
         log.trace("EXCEPTION : destroySubscription: ", e);
         throw e;
      }
      finally
      {
         log.trace("RETURN : destroySubscription");
         NDC.pop();
         NDC.remove();
      }

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

      if (!log.isTraceEnabled())
      {
         return getNext().checkUser(userName, password);
      }

      try
      {
         log.trace("CALLED : checkUser");
         log.trace("ARG    : " + userName);
         log.trace("ARG    : (password not shown)");
         return getNext().checkUser(userName, password);
      }
      catch (JMSException e)
      {
         log.trace("EXCEPTION : checkUser: ", e);
         throw e;
      }
      catch (RuntimeException e)
      {
         log.trace("EXCEPTION : checkUser: ", e);
         throw e;
      }
      finally
      {
         log.trace("RETURN : checkUser");
      }

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

      if (!log.isTraceEnabled())
      {
         return getNext().authenticate(userName, password);
      }

      try
      {
         log.trace("CALLED : authenticate");
         return getNext().authenticate(userName, password);
      }
      catch (JMSException e)
      {
         log.trace("EXCEPTION : authenticate: ", e);
         throw e;
      }
      catch (RuntimeException e)
      {
         log.trace("EXCEPTION : authenticate: ", e);
         throw e;
      }
      finally
      {
         log.trace("RETURN : authenticate");
      }

   }

   /**
    * @param dc                       org.jboss.mq.ConnectionToken
    * @param s                        org.jboss.mq.Subscription
    * @exception JMSException  The exception description.
    */
   public void subscribe(org.jboss.mq.ConnectionToken dc, org.jboss.mq.Subscription s) throws JMSException
   {

      if (!log.isTraceEnabled())
      {
         getNext().subscribe(dc, s);
         return;
      }

      try
      {
         NDC.push(dc.getClientID());
         log.trace("CALLED : subscribe");
         log.trace("ARG    : " + s);
         getNext().subscribe(dc, s);
         return;
      }
      catch (JMSException e)
      {
         log.trace("EXCEPTION : subscribe: ", e);
         throw e;
      }
      catch (RuntimeException e)
      {
         log.trace("EXCEPTION : subscribe: ", e);
         throw e;
      }
      finally
      {
         log.trace("RETURN : subscribe");
         NDC.pop();
         NDC.remove();
      }

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

      if (!log.isTraceEnabled())
      {
         getNext().ping(dc, clientTime);
         return;
      }

      try
      {
         NDC.push(dc.getClientID());
         log.trace("CALLED : ping");
         log.trace("ARG    : " + clientTime);
         getNext().ping(dc, clientTime);
         return;
      }
      catch (JMSException e)
      {
         log.trace("EXCEPTION : ping: ", e);
         throw e;
      }
      catch (RuntimeException e)
      {
         log.trace("EXCEPTION : ping: ", e);
         throw e;
      }
      finally
      {
         log.trace("RETURN : ping");
         NDC.pop();
         NDC.remove();
      }

   }

   public SpyTopic getDurableTopic(DurableSubscriptionID sub) throws JMSException
   {

      if (!log.isTraceEnabled())
      {
         return getNext().getDurableTopic(sub);
      }

      try
      {
         log.trace("CALLED : getDurableTopic");
         log.trace("ARG    : " + sub);
         return getNext().getDurableTopic(sub);
      }
      catch (JMSException e)
      {
         log.trace("EXCEPTION : getDurableTopic: ", e);
         throw e;
      }
      catch (RuntimeException e)
      {
         log.trace("EXCEPTION : getDurableTopic: ", e);
         throw e;
      }
      finally
      {
         log.trace("RETURN : getDurableTopic");
      }

   }

   public Subscription getSubscription(ConnectionToken dc, int subscriberId) throws JMSException
   {

      if (!log.isTraceEnabled())
      {
         return getNext().getSubscription(dc, subscriberId);
      }

      try
      {
         NDC.push(dc.getClientID());
         log.trace("CALLED : getSubscription");
         log.trace("ARG    : " + subscriberId);
         return getNext().getSubscription(dc, subscriberId);
      }
      catch (JMSException e)
      {
         log.trace("EXCEPTION : getSubscription: ", e);
         throw e;
      }
      catch (RuntimeException e)
      {
         log.trace("EXCEPTION : getSubscription: ", e);
         throw e;
      }
      finally
      {
         log.trace("RETURN : getSubscription");
         NDC.pop();
         NDC.remove();
      }

   }
}
