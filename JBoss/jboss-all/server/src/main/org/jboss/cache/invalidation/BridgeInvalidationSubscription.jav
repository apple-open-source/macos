/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.cache.invalidation;

import java.io.Serializable;

/** 
 * Every bridge subscribing to a InvalidationManager has access to this interface that
 * it can used to invalidate messages on the local IM.
 * @see InvalidationManagerMBean
 * @see InvalidationBridgeListener
 * @version $Revision: 1.1.2.1 $
 *
 * <p><b>Revisions:</b>
 *
 * <p><b>21 septembre 2002 Sacha Labourey:</b>
 * <ul>
 * <li> First implementation </li>
 * </ul>
 */

public interface BridgeInvalidationSubscription
{
   /** 
    * Used to invalidate a single key in a given InvalidationGroup
    * @param invalidationGroupName Name of the InvalidationGroup for which this invalidation is targeted
    * @param key Key to be invalidated
    */   
   public void invalidate (String invalidationGroupName, Serializable key);
   
   /**
    * Invalidate a set of keys in a give InvalidationGRoup
    * @param invalidationGroupName Name of the InvalidationGroup to which is targeted this invalidation
    * @param keys Keys to be invalidated
    */   
   public void invalidate (String invalidationGroupName, Serializable[] keys);
   
   /**
    * Invalidates a set of keys in a set of InvalidationGroup. It is the responsability
    * of the InvalidationManager to determine which IG are actually present i.e. the
    * bridge may transmit BatchInvalidation for IG that are not present locally. The
    * IM will simply ignore them.
    * @param invalidations Invalidations to be performed on the local IM instance
    */   
   public void batchInvalidate (BatchInvalidation[] invalidations);
   
   /**
    * Unregister the current bridge form the InvalidationManager
    */   
   public void unregister ();   
   
}
