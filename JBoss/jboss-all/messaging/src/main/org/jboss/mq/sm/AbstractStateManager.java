/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.mq.sm;

import java.util.Collection;
import java.util.Set;
import java.util.HashSet;
import javax.jms.JMSException;
import javax.jms.JMSSecurityException;
import javax.jms.InvalidClientIDException;
import org.jboss.mq.SpyTopic;
import org.jboss.mq.DurableSubscriptionID;
import org.jboss.mq.server.JMSDestinationManager;
import org.jboss.mq.server.JMSTopic;

import org.jboss.system.ServiceMBeanSupport;

/**
 * An abstract baseclass to make it a little bit easier to implement
 * new StateManagers.
 *
 * <p>
 * Apart from one methods in StateManager subclasses implement
 * the protected abstract callback methods to do its work.
 *
 * @author     <a href="pra@tim.se">Peter Antman</a>
 * @author     <a href="Norbert.Lataille@m4x.org">Norbert Lataille</a>
 * @author     <a href="hiram.chirino@jboss.org">Hiram Chirino</a>
 * @version $Revision: 1.4.2.1 $
 */

public abstract class AbstractStateManager 
   extends ServiceMBeanSupport
   implements StateManager, AbstractStateManagerMBean 
{
   /**
    * Abstracts the data between a subclass and this class.
    * 
    * A sublcass can extends this class to ad custom behaviour.
    */
   protected class DurableSubscription {
      String clientID;
      String name;
      String topic;
      String selector;
      
      public DurableSubscription () {}
      public DurableSubscription(String clientID, String name, String topic, String selector) {
         this.clientID = clientID;
         this.name = name;
         this.topic = topic;
         this.selector = selector;
      }
      public String getClientID() {return clientID;}
      public String getName() {return name;}
      public String getTopic() {return topic;}
      public void setTopic(String topic) { this.topic = topic;}
      
      /**
       * Gets the selector.
       * @return Returns a String
       */
      public String getSelector()
      {
         return selector;
      }

      /**
       * Sets the selector.
       * @param selector The selector to set
       */
      public void setSelector(String selector)
      {
         this.selector = selector;
      }

   }
   /**
    * The currently loggen in clientID's
    */
   private final Set loggedOnClientIds = new HashSet();
   
   public AbstractStateManager() {
      
   }

   /**
    * Set the durable subscriber.
    *
    * getDurableSubscription() will be called to get the durable sub.
    *
    * removeDurableSubscription() will be called to remove a dur sub.
    *
    * saveDurableSubscription() will be called to save a new dur sub or
    * to save a changed one (same clientID and name, different topic).
    */
   public void setDurableSubscription(JMSDestinationManager server, DurableSubscriptionID sub, SpyTopic topic) throws JMSException {
      boolean debug = log.isDebugEnabled();
      if (debug)
         log.debug("Checking durable subscription: " + sub + ", on topic: " + topic);
      
      DurableSubscription subscription = getDurableSubscription(sub);
      
     // A new subscription
      if ( subscription== null) {
         if (debug)
            log.debug("The subscription was not previously registered.");
         // Either this was a remove attemt, not to successfull,
         // or it was a bogus request (should then an exception be raised?9
         if (topic == null)
         {
            return;
         }
         // Create the real durable subscription
         JMSTopic dest = (JMSTopic)server.getJMSDestination(topic);
         dest.createDurableSubscription(sub);
         
         // Save it
         subscription  = new DurableSubscription(sub.getClientID(),
                                      sub.getSubscriptionName(),
                                      topic.getName(), sub.getSelector());
         // Call subclass
         saveDurableSubscription(subscription);
      }  
      // An existing subscription...it was previously registered...
      // Check if it is an unsubscribe, or a change of subscription.
      else
      {
         if (debug)
            log.debug("The subscription was previously registered.");

         String newSelector = sub.getSelector();
         String oldSelector = subscription.getSelector();
         boolean selectorChanged = false;
         if ((newSelector == null && oldSelector != null) ||
            (newSelector != null && newSelector.equals(oldSelector) == false))
            selectorChanged = true;

         // The client wants an unsubscribe 
         // TODO: we are not spec compliant since we neither check if
         // the topic has an active subscriber or if there are messages
         // destined for the client not yet acked by the session!!!
         if (topic == null)
         {
            if (debug)
               log.debug("Removing subscription.");
            // we have to change the subscription...do physical work
            SpyTopic prevTopic = new SpyTopic(subscription.getTopic());
            JMSTopic dest = (JMSTopic)server.getJMSDestination(prevTopic);
            // TODO here we should check if  the client still has 
            // an active consumer
            
            dest.destroyDurableSubscription(sub);
            
            //straight deletion, remove subscription - call subclass
            removeDurableSubscription(subscription);
         }
         // The topic previously subscribed to is not the same as the
         // one in the subscription request.
         else if (!subscription.getTopic().equals(topic.getName())
            || selectorChanged)
         {
            //new topic so we have to change it
            if (debug)
               log.debug("But the topic or selector was different, changing the subscription.");
            // remove the old sub
            SpyTopic prevTopic = new SpyTopic(subscription.getTopic());
            JMSTopic dest = (JMSTopic)server.getJMSDestination(prevTopic);
            dest.destroyDurableSubscription(sub);
            
            // Create the new
            dest = (JMSTopic)server.getJMSDestination(topic);
            dest.createDurableSubscription(sub);
            
            //durable subscription has new topic, save.
            subscription.setTopic(topic.getName());
            subscription.setSelector(sub.getSelector());
            saveDurableSubscription(subscription);
         }
      }
   }
   /**
    * Get SpyTopic for the give DurableSubscriptionID. Will call
    * getDurableSubscription() to get the subscription.
    */
   public SpyTopic getDurableTopic(DurableSubscriptionID sub) throws JMSException {
      DurableSubscription subscription = getDurableSubscription(sub);
      if (subscription == null)
         throw new JMSException("No durable subscription found for subscription: " + sub.getSubscriptionName());
      
      return new SpyTopic(subscription.getTopic());
   }
       
   /**
    * Get a preconfiged clientID and possible do authentication.
    *
    * Subclass will be called by getPreconfClientId(), and may if it wants
    * to do authentication.
    */
   public String checkUser(String login, String passwd) throws JMSException {
      String clientId = getPreconfClientId(login,passwd);
      
      if (clientId != null)
      {
         synchronized (loggedOnClientIds)
         {
            if (loggedOnClientIds.contains(clientId))
            {
               throw new JMSSecurityException
                  ("The login id has an assigned client id. " +
                   "That client id is already connected to the server!");
            }
            loggedOnClientIds.add(clientId);
         }
      }
      return clientId;
      
   }

   /**
    * This manager keeps the state of the  logged in clientID.
    *
    * Before a clientID is added, the callback checkLoggedOnClientId() is
    * called. A subclass can do its specific check there and throw
    * an exception if it does not allow the clientID to loggin.
    */
   public void addLoggedOnClientId(String ID) throws JMSException {
      //Check : this ID must not be registered
      synchronized (loggedOnClientIds)
      {
         if (loggedOnClientIds.contains(ID))
         {
            throw new InvalidClientIDException("This loggedOnClientIds is already registered !");
         }
      }
      
      checkLoggedOnClientId(ID);

      synchronized (loggedOnClientIds)
      {
         loggedOnClientIds.add(ID);
      }
   }

   /**
    * Remove the clienID from the logged in ones.
    *
    * No callback is done. If sublaclass needs to do anything specific it must
    * override this method.
    */
   public void removeLoggedOnClientId(String ID) { 
      synchronized (loggedOnClientIds)
      {
         loggedOnClientIds.remove(ID);
      }
   }
   //
   // Abstract method sublclasses need to implement.
   //
 
   abstract public Collection getDurableSubscriptionIdsForTopic(SpyTopic topic)
      throws JMSException;

   /**
    * Get preconfigured clientID for login/user, and if state manager wants
    * do authentication. This is NOT recomended when using a SecurityManager.
    */
   abstract protected String getPreconfClientId(String login,String passwd) throws JMSException;
   
   /**
    * Check if the clientID is allowed to logg in from the particular state
    * managers perspective.
    */
   abstract protected void checkLoggedOnClientId(String clientID) throws JMSException;

   /**
    * Get a DurableSubscription describer.
    *
    * If subscription is not found, return null. The get
    *
    */
   abstract protected DurableSubscription getDurableSubscription(DurableSubscriptionID sub) throws JMSException;

   /**
    * Add to durable subs and save the subsrcription to persistent storage.
    *
    * Called by this class so the sublclass can save. This may be both
    * a new subscription or a changed one. It is up to the sublcass
    * to know how to find a changed on. (Only the topic will have changed,
    * and it is the same DurableSubscription that is saved again that this
    * class got through getDurableSubscription.
    *
    *
    */
   abstract protected void saveDurableSubscription(DurableSubscription ds) throws JMSException;

   /**
    * Remove the subscription and save  to persistent storage.
    *
    * Called by this class so the sublclass can remove.
    */
   abstract protected void removeDurableSubscription(DurableSubscription ds) throws JMSException;
   
} // AbstractStateManager
