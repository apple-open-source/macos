/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.cache.invalidation;

import java.io.Serializable;

/** 
 * An InvalidationGroup (IG) is the meeting point of all caches and invaliders
 * working for the same cached information.
 * For example, two entity beans based
 * on the same database information will share their cache invalidation so that
 * a modification performed on the bean of one of the deployed EJB will imply
 * a cache invalidation in the cache of the other EJB.
 * In this case, we say that both EJBs work in the same InvalidationGroup.
 * @see InvalidationManagerMBean
 * @author <a href="mailto:sacha.labourey@cogito-info.ch">Sacha Labourey</a>.
 * @version $Revision: 1.2.2.1 $
 *
 * <p><b>Revisions:</b>
 *
 * <p><b>21 septembre 2002 Sacha Labourey:</b>
 * <ul>
 * <li> First implementation </li>
 * </ul>
 */

public interface InvalidationGroup
{
   /** 
    * Return the name (i.e. identifier) of the current IG
    * @return Name of the current IG
    */   
   public String getGroupName ();
   
   /**  
    * Invalidate a single key in all caches associated with this IG
    * @param key The key to be invalidated
    */   
   public void invalidate (Serializable key);
   
   /**  
    * Invalidate a single key in all caches associated with this IG specifying a
    * prefered invalidation mode (synchronous vs. asynchronous)
    * @param key The key to be invalidated in the cache
    * @param asynchronous if true, the invalidation will be, if possible, performed asynchronously
    */   
   public void invalidate (Serializable key, boolean asynchronous);
   
   /**  
    * Invalidate a set of keys in all caches associated with this IG
    * @param keys Set of keys to be invalidated
    */   
   public void invalidate (Serializable[] keys);

   /**  
    * Invalidate a set of keys in all caches associated with this IG specifying a
    * prefered invalidation mode (synchronous vs. asynchronous)
    * @param keys keys to be invalidated
    * @param asynchronous if true, the invalidation will be, if possible, performed asynchronously
    */   
   public void invalidate (Serializable[] keys, boolean asynchronous);
   
   /**
    * Used to know when to definitively remove this IG. Every component using this IG
    * must increment the counter, otherwise, the group may be dropped while they are still
    * using it. Exceptions:
    * - When calling InvalidationManager.getInvalidationGroup, the counter is automatically
    *   called. Consequently, code accessing this IG through this mean must not call
    *   addReference. Nevertheless, they *must* still call removeReference
    *   appropriatly
    * - When calling InvalidationGroup.unregister, the counter is automatically
    *   decreased
    *
    * Consequenlty, users such as cache generally don't need to increment or decrement
    * the reference count because it is automatically done while getting access to the
    * InvalidationGroup and unsubscribing. Invaliders do need to correctly unsubscribe.
    *
    * When the reference count drops to 0, the group is removed from the Manager
    * Thus, any active user of this group should appropriatly increment the reference
    * count. Passive listener (JMS/JG bridge for example) should not subscribe.
    */   
   public void addReference ();
   
   /**  
    * Decrease the usage counter. When the counter drops to 0, the group is removed.
    */   
   public void removeReference ();
   
   /**  
    * Return the value of the usage counter for this IG.
    * @return Actual usage counter
    */   
   public int getReferenceCount ();

   /**  
    * Register an Invalidatable instance has a subscriber of this InvalidationGroup.
    * @param registered Subscribing instance
    */   
   public void register (Invalidatable registered);   
   
   /**  
    * Unregister an Invalidatable instance
    * @param registered Unsubscribing instance
    */   
   public void unregister (Invalidatable registered);
   
   /**  
    * Set the default mode for managing invalidations in this IG: synchronous or
    * asynchronous.
    * Note: this is a best effort setting.
    * @param async If true, asynchronous communication should be used if possible.
    */   
   public void setAsynchronousInvalidation (boolean async);
   
   /**  
    * Return the default (a-)synchronous communication scheme used
    */   
   public boolean getAsynchronousInvalidation ();

   /**  
    * Return the InvalidationManager (IM) to which is linked this group (IG)
    */   
   public InvalidationManagerMBean getInvalidationManager ();
   
}
