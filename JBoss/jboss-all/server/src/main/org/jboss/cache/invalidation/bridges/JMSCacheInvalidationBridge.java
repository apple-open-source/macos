/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.cache.invalidation.bridges;

import java.io.Serializable;
import java.util.HashSet;
import java.util.HashMap;
import java.util.Iterator;

import javax.jms.MessageListener;
import javax.jms.TopicConnection;
import javax.jms.TopicSession;
import javax.jms.Topic;
import javax.jms.TopicSubscriber;
import javax.jms.TopicPublisher;
import javax.jms.Message;
import javax.jms.ObjectMessage;
import javax.jms.TopicConnectionFactory;
import javax.jms.JMSException;
import javax.naming.InitialContext;
import javax.naming.NamingException;
import javax.naming.Context;

import org.jboss.cache.invalidation.InvalidationManager;
import org.jboss.cache.invalidation.InvalidationBridgeListener;
import org.jboss.cache.invalidation.BatchInvalidation;
import org.jboss.system.ServiceMBeanSupport;

/**
 * JMS implementation of a cache invalidation bridge
 *
 * Based on previous code of Bill Burke based on interceptors
 *
 * @see org.jboss.cache.invalidation.InvalidationManagerMBean
 *
 * @author  <a href="mailto:sacha.labourey@cogito-info.ch">Sacha Labourey</a>.
 * @author  <a href="mailto:bill@jboss.org">Bill Burke</a>.
 * @version $Revision: 1.1.2.2 $
 *
 * <p><b>Revisions:</b>
 *
 * <p><b>28 septembre 2002 Sacha Labourey:</b>
 * <ul>
 * <li> First implementation </li>
 * </ul>
 */

public class JMSCacheInvalidationBridge
   extends ServiceMBeanSupport
   implements JMSCacheInvalidationBridgeMBean, 
              InvalidationBridgeListener,
              MessageListener
{   
   // Constants -----------------------------------------------------
   
   public static final String JMS_CACHE_INVALIDATION_BRIDGE = "JMS_CACHE_INVALIDATION_BRIDGE";

   // Attributes ----------------------------------------------------

   // JMX Attributes
   //
   protected org.jboss.cache.invalidation.InvalidationManagerMBean invalMgr = null;
   protected org.jboss.cache.invalidation.BridgeInvalidationSubscription invalidationSubscription = null;
   protected String invalidationManagerName = InvalidationManager.DEFAULT_JMX_SERVICE_NAME;

   protected boolean publishingAuthorized = false;
   protected String connectionFactoryName = "java:/ConnectionFactory";
   protected String topicName = "topic/JMSCacheInvalidationBridge";
   protected boolean transacted = true;
   protected int acknowledgeMode = TopicSession.AUTO_ACKNOWLEDGE; // AUTO_ACK by default
   protected int propagationMode = JMSCacheInvalidationBridgeMBean.IN_OUT_BRIDGE_PROPAGATION; // IN_OUT by default
   
   protected  java.rmi.dgc.VMID serviceId = new java.rmi.dgc.VMID();
 
   protected TopicConnection  conn = null;
   protected TopicSession session = null;
   protected Topic topic = null;
   protected TopicSubscriber subscriber = null;
   protected TopicPublisher pub = null;

   protected String providerUrl = null;

   // Static --------------------------------------------------------
   
   // Constructors --------------------------------------------------
   
   public JMSCacheInvalidationBridge () { super (); }
   
   // Public --------------------------------------------------------
   
   // *MBean implementation ----------------------------------------------

   public String getInvalidationManager ()
   {
      return this.invalidationManagerName;
   }
   
   public void setInvalidationManager (String objectName)
   {
      this.invalidationManagerName = objectName;
   }
   
   public String getConnectionFactoryName ()
   {
      return this.connectionFactoryName;
   }   
   public void setConnectionFactoryName (String factoryName)
   {
      this.connectionFactoryName = factoryName;
   }
   
   public String getTopicName ()
   {
      return this.topicName;
   }
   public void setTopicName (String topicName)
   {
      this.topicName = topicName;
   }
   
   public String getProviderUrl ()
   {
      return providerUrl;
   }

   public void setProviderUrl (String providerUrl)
   {
      this.providerUrl = providerUrl;
   }

   public boolean isTransacted ()
   {
      return this.transacted;
   }
   public void setTransacted (boolean isTransacted)
   {
      this.transacted = isTransacted;
   }
   
   public int getAcknowledgeMode ()
   {
      return this.acknowledgeMode;
   }
   public void setAcknowledgeMode (int ackMode)
   {
      if (ackMode > 3 || ackMode < 1)
         throw new RuntimeException ("Value AcknowledgeMode must be between 1 and 3");
      
      switch (ackMode)
      {
         case 1: this.acknowledgeMode = TopicSession.AUTO_ACKNOWLEDGE; break;
         case 2: this.acknowledgeMode = TopicSession.CLIENT_ACKNOWLEDGE; break;
         case 3: this.acknowledgeMode = TopicSession.DUPS_OK_ACKNOWLEDGE; break;
      }
   }
   
   public int getPropagationMode ()
   {
      return this.propagationMode;
   }   
   public void setPropagationMode (int propMode)
   {
      if (propMode > 3 || propMode < 1)
         throw new RuntimeException ("Value PropagationMode must be between 1 and 3");
      
      this.propagationMode = propMode;
   }

   // MessageListener implementation ----------------------------------------------
   
   public void onMessage(Message msg)
   {
      // just to make sure we are in the good mode
      //
      if (this.propagationMode == JMSCacheInvalidationBridgeMBean.IN_OUT_BRIDGE_PROPAGATION ||
            this.propagationMode == JMSCacheInvalidationBridgeMBean.IN_ONLY_BRIDGE_PROPAGATION)
      {         
         try
         {
            ObjectMessage objmsg = (ObjectMessage)msg;
            if (!objmsg.getJMSType().equals(JMS_CACHE_INVALIDATION_BRIDGE)) return;          
            JMSCacheInvalidationMessage content = (JMSCacheInvalidationMessage)objmsg.getObject();
            
            // Not very efficient as the whole message must be unserialized just to check
            // if we were the emitter. Maybe wrapping this in a byte array would be more efficient
            //
            if (!content.emitter.equals (this.serviceId))
            {
               this.invalidationSubscription.batchInvalidate (content.getInvalidations ());
            }
         }
         catch (Exception ex)
         {
            log.warn(ex.getMessage());
         }
      }
   }

   // InvalidationBridgeListener implementation ----------------------------------------------
   
   public void batchInvalidate (BatchInvalidation[] invalidations, boolean asynchronous)
   {
      if ( (this.propagationMode == JMSCacheInvalidationBridgeMBean.IN_OUT_BRIDGE_PROPAGATION ||
            this.propagationMode == JMSCacheInvalidationBridgeMBean.OUT_ONLY_BRIDGE_PROPAGATION)
            && this.publishingAuthorized)
      {         
         JMSCacheInvalidationMessage msg = new JMSCacheInvalidationMessage (this.serviceId, invalidations);
         this.sendJMSInvalidationEvent (msg);
      }
   }

   public void invalidate (String invalidationGroupName, Serializable[] keys, boolean asynchronous)
   {
      if ( (this.propagationMode == JMSCacheInvalidationBridgeMBean.IN_OUT_BRIDGE_PROPAGATION ||
            this.propagationMode == JMSCacheInvalidationBridgeMBean.OUT_ONLY_BRIDGE_PROPAGATION)
            && this.publishingAuthorized)
      {         
         JMSCacheInvalidationMessage msg = new JMSCacheInvalidationMessage (
                  this.serviceId, 
                  invalidationGroupName, 
                  keys);
         this.sendJMSInvalidationEvent (msg);
      }
   }

   public void invalidate (String invalidationGroupName, Serializable key, boolean asynchronous)
   {
      if ( (this.propagationMode == JMSCacheInvalidationBridgeMBean.IN_OUT_BRIDGE_PROPAGATION ||
            this.propagationMode == JMSCacheInvalidationBridgeMBean.OUT_ONLY_BRIDGE_PROPAGATION)
            && this.publishingAuthorized)
      {         
         JMSCacheInvalidationMessage msg = new JMSCacheInvalidationMessage (
                  this.serviceId, 
                  invalidationGroupName, 
                  new Serializable[] {key} );
         this.sendJMSInvalidationEvent (msg);
      }
   }

   public void newGroupCreated (String groupInvalidationName)
   {
      // we don't manage groups dynamically, so we don't really care...
      //
   }   
   
   public void groupIsDropped (String groupInvalidationName)
   {
      // we don't manage groups dynamically, so we don't really care...
      //
   }

   // ServiceMBeanSupport overrides ---------------------------------------------------

   protected void startService () throws Exception
   {
      log.info("Starting JMS cache invalidation bridge");
            
      // Deal with the InvalidationManager first..
      //
      this.invalMgr = (org.jboss.cache.invalidation.InvalidationManagerMBean)
         org.jboss.system.Registry.lookup (this.invalidationManagerName);
      
      this.invalidationSubscription = invalMgr.registerBridgeListener (this);

      // deal with JMS next
      //
      InitialContext iniCtx = getInitialContext ();
      
      Object tmp = iniCtx.lookup(this.connectionFactoryName);
      TopicConnectionFactory tcf = (TopicConnectionFactory) tmp;
      conn = tcf.createTopicConnection();
      
      topic = (Topic) iniCtx.lookup(this.topicName);
      session = conn.createTopicSession(this.transacted,
                                        this.acknowledgeMode);
      
      conn.start();
      
      // Are we publisher, subscriber, or both?
      //
      if (this.propagationMode == JMSCacheInvalidationBridgeMBean.IN_OUT_BRIDGE_PROPAGATION ||
            this.propagationMode == JMSCacheInvalidationBridgeMBean.IN_ONLY_BRIDGE_PROPAGATION)
      {
         this.subscriber = session.createSubscriber(topic);     
         this.subscriber.setMessageListener(this);
      }
      
      if (this.propagationMode == JMSCacheInvalidationBridgeMBean.IN_OUT_BRIDGE_PROPAGATION ||
            this.propagationMode == JMSCacheInvalidationBridgeMBean.OUT_ONLY_BRIDGE_PROPAGATION)
      {
         this.pub = session.createPublisher(topic);
         this.publishingAuthorized = true;
      }
   }
   
   protected void stopService ()
   {
      log.info ("Stoping JMS cache invalidation bridge");
      try
      {
         if (this.propagationMode == JMSCacheInvalidationBridgeMBean.IN_OUT_BRIDGE_PROPAGATION ||
               this.propagationMode == JMSCacheInvalidationBridgeMBean.IN_ONLY_BRIDGE_PROPAGATION)
         {
            subscriber.close();
         }

         if (this.propagationMode == JMSCacheInvalidationBridgeMBean.IN_OUT_BRIDGE_PROPAGATION ||
               this.propagationMode == JMSCacheInvalidationBridgeMBean.OUT_ONLY_BRIDGE_PROPAGATION)
         {
            this.publishingAuthorized = false;
            pub.close();
         }

         conn.stop();
         session.close();
         conn.close();
         
      }
      catch (Exception ex)
      {
         log.warn("Failed to stop JMS resources associated with the JMS bridge: ", ex);
      }
   }

   // Package protected ---------------------------------------------
   
   // Protected -----------------------------------------------------
   
   protected synchronized TopicSession getSession()
   {
      return this.session;
   }

   protected synchronized TopicPublisher getPublisher()
   {
      return this.pub;
   }

   protected void sendJMSInvalidationEvent(JMSCacheInvalidationMessage invalidationMsg)
   {
      try
      {
         if (log.isTraceEnabled ())
            log.trace("sending JMS message for cache invalidation" + invalidationMsg);
         
         try
         {
            ObjectMessage msg = getSession().createObjectMessage();
            msg.setJMSType(JMS_CACHE_INVALIDATION_BRIDGE);
            msg.setObject(invalidationMsg);
            getPublisher().publish(msg);
         }
         catch (JMSException ex)
         {
            log.debug("failed to publish seppuku event: ", ex);
         }
      }
      catch (Exception ex)
      {
         log.warn("failed to do cluster seppuku event: " , ex);
      }
   }

   protected InitialContext getInitialContext() 
      throws NamingException
   {
      if (providerUrl == null) 
      {
         return new InitialContext();
      }
      else
      {
         if (log.isDebugEnabled()) 
            log.debug("Using Context.PROVIDER_URL: " + providerUrl);

         java.util.Properties props = new java.util.Properties(System.getProperties());
         props.put(Context.PROVIDER_URL, providerUrl);
         return new InitialContext(props);
      }
    }
   

   // Private -------------------------------------------------------
   
   // Inner classes -------------------------------------------------
   
}
