/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.cache.invalidation;

import java.io.Serializable;

/**
 * InvalidationManager (IM) represents locally managed caches and invaliders. To be able
 * to do distributed invalidations, it is necessary to bridge these IM to forward
 * cache invalidation messages.
 * The InvalidationBridgeListener provides the way for any transport mechanism to
 * be used to forward cache invalidation messages accross a network/cluster.
 * @see InvalidationManagerMBean
 * @author <a href="mailto:sacha.labourey@cogito-info.ch">Sacha Labourey</a>.
 * @version $Revision: 1.1.2.1 $
 *
 * <p><b>Revisions:</b>
 *
 * <p><b>24 septembre 2002 Sacha Labourey:</b>
 * <ul>
 * <li> First implementation </li>
 * </ul>
 */

public interface InvalidationBridgeListener
{
   /**
    * Called when a set of invalidations, concerning more than one IG, should be forwarded
    * accross the bridge.
    * It is the bridge responsability to determine:
    * - which IG must be bridged (some IG may not exist on other nodes, in this case
    *  the bridge may decide to drop these invalidations messages to reduce the
    *  serialization cost and network usage)
    * - to which other nodes the invalidations must be communicated. This can be done
    *  by any mean (automatic discovery, configuration file, etc.)
    * @param invalidations BatchInvalidation messages containing invalidations
    * @param asynchronous Determine the best-effort indication to be used to communicate invalidations
    */   
   public void batchInvalidate (BatchInvalidation[] invalidations, boolean asynchronous);
   
   /**
    * Called when a single invalidation, concerning a single IG, should be forwarded
    * accross the bridge.
    * It is the bridge responsability to determine:
    * - which IG must be bridged (some IG may not exist on other nodes, in this case
    *  the bridge may decide to drop these invalidations messages to reduce the
    *  serialization cost and network usage)
    * - to which other nodes the invalidations must be communicated. This can be done
    *  by any mean (automatic discovery, configuration file, etc.)
    * @param invalidationGroupName InvalidationGroup name
    * @param key Key to be invalidated
    * @param asynchronous Best effort communication setting
    */   
   public void invalidate (String invalidationGroupName, Serializable key, boolean asynchronous);
   
   /** Called when a set of invalidations, concerning a single IG, should be forwarded
    * accross the bridge.
    * It is the bridge responsability to determine:
    * - which IG must be bridged (some IG may not exist on other nodes, in this case
    *  the bridge may decide to drop these invalidations messages to reduce the
    *  serialization cost and network usage)
    * - to which other nodes the invalidations must be communicated. This can be done
    *  by any mean (automatic discovery, configuration file, etc.)
    * @param invalidationGroupName Name of the InvalidationGroup to which is linked the invalidation message
    * @param keys Keys to be invalidated
    * @param asynchronous Best effort communication setting
    */   
   public void invalidate (String invalidationGroupName, Serializable[] keys, boolean asynchronous);
   
   /**
    * Called when an InvocationGroup is dropped (because no cache and invalider are
    * using it anymore).
    * For bridge implementations that automatically discover which IG should be
    * bridged, this callback can be used to communicate to the other nodes that this
    * node is no more interested in invalidation for this group.
    * @param groupInvalidationName Name of the InvalidationGroup being dropped
    */   
   public void groupIsDropped (String groupInvalidationName);
   
   /**
    * Called when an InvocationGroup is created.
    * For bridge implementations that automatically discover which IG should be
    * bridged, this callback can be used to communicate to the other nodes that this
    * node is now interested in invalidation for this group.
    * @param groupInvalidationName Name of the InvalidationGroup just being created
    */   
   public void newGroupCreated (String groupInvalidationName);
   
}
