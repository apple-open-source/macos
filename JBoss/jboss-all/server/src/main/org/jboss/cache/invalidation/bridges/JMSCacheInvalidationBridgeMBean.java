/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.cache.invalidation.bridges;

/**
 * Cache invalidation bridge based on JMS.
 * The list of InvalidationGroup to be bridged is *not* automatically
 * discovered, thus, all invalidation messages that are locally generated
 * are forwarded over JMS.
 * In the future, it should be possible, through a JMX attribute, to list
 * the InvalidationGroup that should be included/excluded
 *
 * @see org.jboss.cache.invalidation.InvalidationManagerMBean
 *
 * @author  <a href="mailto:sacha.labourey@cogito-info.ch">Sacha Labourey</a>.
 * @version $Revision: 1.1.2.3 $
 *
 * <p><b>Revisions:</b>
 *
 * <p><b>28 septembre 2002 Sacha Labourey:</b>
 * <ul>
 * <li> First implementation </li>
 * </ul>
 */

public interface JMSCacheInvalidationBridgeMBean 
   extends org.jboss.system.ServiceMBean
{
   public static final int AUTO_ACKNOWLEDGE_MODE = 1;
   public static final int CLIENT_ACKNOWLEDGE_MODE = 2;
   public static final int DUPS_OK_ACKNOWLEDGE_MODE = 3;
   
   public static final int IN_OUT_BRIDGE_PROPAGATION = 1;
   public static final int IN_ONLY_BRIDGE_PROPAGATION = 2;
   public static final int OUT_ONLY_BRIDGE_PROPAGATION = 3;
   
   /**
    * ObjectName of the InvalidationManager to be used. Optional: in this
    * case, the default InvalidationManager is used.
    */
   public String getInvalidationManager ();
   public void setInvalidationManager (String objectName);

   /**
    * JNDI name of the JMS connection factory to use for cache invalidations
    */   
   public String getConnectionFactoryName ();
   public void setConnectionFactoryName (String factoryName);
   
   /**
    * JNDI name of the Topic to use to send/receive cache invalidations.
    * Defaults to "topic/JMSCacheInvalidationBridge"
    */   
   public String getTopicName ();
   public void setTopicName (String topicName);

   /**
   * Provider URL to use for JMS access. If null, use the default settings
   */
   public String getProviderUrl();
   public void setProviderUrl(String url);
   
   /**
    * Status of the JMS topic wrt transactions
    */   
   public boolean isTransacted ();
   public void setTransacted (boolean isTransacted);
   
   /**
    * Status of the JMS topic wrt messages acknowledgement
    */   
   public int getAcknowledgeMode ();
   public void setAcknowledgeMode (int ackMode);
   
   /**
    * Indicates if this bridge should:
    * 1 - Post local invalidations to the topic and invalidate local caches with invalidations received on the topic
    * 2 - Only invalidate local caches with invalidations received on the topic but not post anything on the topic
    * 3 - Only post local invalidations to the topic and not listen to the Topic for invalidation messages
    */   
   public int getPropagationMode ();
   public void setPropagationMode (int propagationMode);
        
}
