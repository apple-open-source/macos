/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.cache.invalidation;

import org.jboss.system.ServiceMBean;
import java.util.Collection;

/** Main service dealing with cache invalidation. While more than one instance may
 * be running at the same time, most of the time, only one will be used.
 * Each InvalidationManager (IM) gives access to a set of InvalidationGroup (IG).
 * Each IG concerns a particular cache and links subscribers that listen for cache
 * invalidations messages with cache invaliders that will create invalidation
 * messages.
 * Thus, to start, a given service will first ask for a specific IG to work with. This
 * is an in-VM operation: each cache and invalider works with a *locally* bound IM. If
 * you want to extend the in-VM mode of operation, you need to provide (possibly
 * dynamically), your IM-Bridge. A bridge forwards cache-invalidation messages on other
 * nodes. It may select which IG are bridged. More than one cache can be bound to a
 * given IM.
 *
 * As some applications needs to be able to send in batch invalidation messages that concern
 * more than one cache. To satisfy this need, a global batchInvalidate method is available
 * at the IM level.
 * @see org.jboss.cache.invalidation.InvalidationManager
 * @author <a href="mailto:sacha.labourey@cogito-info.ch">Sacha Labourey</a>.
 * @version $Revision: 1.1.2.1 $
 *
 * <p><b>Revisions:</b>
 *
 * <p><b>21 septembre 2002 Sacha Labourey:</b>
 * <ul>
 * <li> First implementation </li>
 * </ul>
 */

public interface InvalidationManagerMBean extends ServiceMBean
{
   
   /**  
    * Returns a given InvalidationGroup instance that is associated with the group name.
    * All caches that will share the same cache invalidation messages must share the
    * same group name => the group name (or the IG) represents the identifier of
    * a set of caches and invaliders.
    * NOTE: InvalidationGroup.addReference is automatically called when calling this method
    *       Thus, there is no need to call it again on the IG. Nevertheless,
    *       you are still responsible for calling removeReference to GC IG.
    * @param groupName Name of the group (of the cache for example).
    * @return The InvalidationGroup associated to the group name i.e. the identifier of the set
    */   
   public InvalidationGroup getInvalidationGroup (String groupName);   
   
   /**  
    * Return the set of all InvalidationGroup currently managed by this IM
    * @return A collection of InvalidationGroup instances
    */   
   public Collection getInvalidationGroups ();
   
   /**  
    * Allow the subscription of a given Bridge to this IM
    * @param listener The Bridge registring for invalidation messages
    * @return A BridgeInvalidationSubscription instance that can is used by the bridge
    * to communicate with the local IM.
    * @see BridgeInvalidationSubscription
    */   
   public BridgeInvalidationSubscription registerBridgeListener (InvalidationBridgeListener listener);
   
   /**  
    * Invalidate a set of IG managed by this IM. This can be used as an optimisation
    * if a bridge will forward requests accross a cluster. In this case, a single message
    * containing all invocations is send accross the wire (it only costs a single network
    * latency). The IM will manage the dispatching of the invalidation messages to the
    * Bridges and to the concerned InvalidationGroups.
    * @param invalidations A set of BatchcInvalidations. Each BatchInvalidation instance contains invalidations
    * for a given InvalidationGroup.
    */   
   
   public void batchInvalidate (BatchInvalidation[] invalidations); 
   
   /**  
    * Identical as previous method. In this case though, it is override the default
    * "asynchronous" tag of each InvalidationGroup and explicitly state if the invalidation
    * messages should be, if possible, be done asynchronously (if implemented by the
    * bridges for example).
    * @param invalidations Invalidation messages
    * @param asynchronous Indicates if the briges should try to do asynchronous invalidations (accross the
    * network for example) or if a synchronous behaviour is required.
    */   
   public void batchInvalidate (BatchInvalidation[] invalidations, boolean asynchronous);   
}
