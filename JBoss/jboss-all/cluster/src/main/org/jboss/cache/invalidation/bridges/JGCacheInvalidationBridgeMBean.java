/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.cache.invalidation.bridges;

import org.jboss.system.ServiceMBean;

/**
 * Cache Invalidation bridge working over JavaGroup.
 * The partition to be used and the invalidation manager can be defined as part
 * of the MBean interface.
 * The bridge automatically discovers which are the InvalidationGroup that are
 * managed by other node of the cluster and only send invalidation information
 * for these groups over the network. This makes this bridge very easy to setup
 * while still being efficient with network resource and CPU serialization cost.
 *
 * @see JGCacheInvalidationBridge
 * @see org.jboss.cache.invalidation.InvalidationManager
 *
 * @author  <a href="mailto:sacha.labourey@cogito-info.ch">Sacha Labourey</a>.
 * @version $Revision: 1.1.2.2 $
 *
 * <p><b>Revisions:</b>
 *
 * <p><b>24 septembre 2002 Sacha Labourey:</b>
 * <ul>
 * <li> First implementation </li>
 * </ul>
 */

public interface JGCacheInvalidationBridgeMBean extends ServiceMBean
{
   /**
    * Name of the Clustering partition to be used to exchange
    * invalidation messages and discover which caches (i.e. InvalidationGroup)
    * are available
    */
   public String getPartitionName();
   public void setPartitionName (String partitionName);
   
   /**
    * ObjectName of the InvalidationManager to be used. Optional: in this
    * case, the default InvalidationManager is used.
    */
   public String getInvalidationManager ();
   public void setInvalidationManager (String objectName);
   
   public String getBridgeName ();
   public void setBridgeName (String name);
   
}
