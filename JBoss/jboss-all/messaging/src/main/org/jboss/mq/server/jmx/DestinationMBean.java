/*
* JBoss, the OpenSource EJB server
*
* Distributable under LGPL license.
* See terms of license at gnu.org.
*/
package org.jboss.mq.server.jmx;

import javax.management.ObjectName;
import org.jboss.system.ServiceMBean;
import org.jboss.mq.server.MessageCounter;

/**
 * MBean interface for destination managers.
 *
 *
 * @author  <a href="pra@tim.se">Peter Antman</a>
 * @version $Revision: 1.2.2.3 $
 */
public interface DestinationMBean extends ServiceMBean  
{
   /**
    * Get the value of JBossMQService.
    * @return value of JBossMQService.
    */
   void removeAllMessages() throws Exception; 
   
   /**
    * Get the value of JBossMQService.
    * @return value of JBossMQService.
    */
   ObjectName getDestinationManager(); 
   
   /**
    * Set the value of JBossMQService.
    * @param v  Value to assign to JBossMQService.
    */
   void setDestinationManager(ObjectName  jbossMQService); 

    /**
    * Sets the JNDI name for this destination
    * @param name Name to bind this topic to in the JNDI tree
    */
   void setJNDIName(String name) throws Exception;

   /**
    * Gets the JNDI name use by this destination.
    * @return  The JNDI name currently in use
    */
   String getJNDIName();
   
   /**
    * Sets the security xml config
    */
   //void setSecurityConf(String securityConf) throws Exception;
   void setSecurityConf(org.w3c.dom.Element securityConf) throws Exception;
   
   /**
    * Set the object name of the security manager.
    */
   public void setSecurityManager(ObjectName securityManager);
   
   /**
	* get message counter of all internal queues
	*/
   public MessageCounter[] getMessageCounter();

   /**
    * List destination message counter
    * @return String
    */
   public String listMessageCounter();
   
   /**
    * Reset destination message counter
    */
   public void resetMessageCounter();
   
   /**
    * List destination message counter history
    * @return String
    */
   public String listMessageCounterHistory();
   
   /**
    * Reset destination message counter history
    */
   public void resetMessageCounterHistory();

   /**
    * Sets the destination message counter history day limit
    * <0: unlimited, =0: disabled, > 0 maximum day count
    * 
    * @param days  maximum day count 
    */
   public void setMessageCounterHistoryDayLimit( int days );

   /**
    * Gets the destination message counter history day limit
    * @return  Maximum day count 
    */
   public int getMessageCounterHistoryDayLimit();

   /**
    * Retrieve the maximum depth of the queue or individual
    * subscriptions
    * @return the maximum depth
    */
   public int getMaxDepth();
   
   /**
    * Set the maximum depth of the queue or individual subscriptions
    * @param depth the maximum depth, zero means unlimited
    */
   public void setMaxDepth(int depth);
   
} // DestinationManagerMBean
