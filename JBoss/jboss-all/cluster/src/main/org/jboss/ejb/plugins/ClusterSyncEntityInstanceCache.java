/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.ejb.plugins;

import org.jboss.deployment.DeploymentException;
import org.jboss.ha.framework.interfaces.DistributedState;
import org.jboss.metadata.ClusterConfigMetaData;
import org.jboss.system.Registry;

/**
 * Cache subclass for entity beans shared accross a cluster with
 * distributed cache corruption mechanism.
 *
 * @author <a href="mailto:sacha.labourey@cogito-info.ch">Sacha Labourey</a>
 * @version $Revision: 1.3.2.5 $
 */
public class ClusterSyncEntityInstanceCache
   extends EntityInstanceCache
   implements org.jboss.ha.framework.interfaces.DistributedState.DSListenerEx
{
   protected DistributedState ds = null;
   protected String DS_CATEGORY = null;

   public void create() throws Exception
   {
      super.create ();

      // Get a reference to the DS service
      ClusterConfigMetaData config = getContainer().getBeanMetaData().getClusterConfigMetaData();
      String partitionName = config.getPartitionName();
      String name = "jboss:service=DistributedState,partitionName=" + partitionName;
      ds = (DistributedState) Registry.lookup (name);
      if( ds == null )
         throw new DeploymentException("Failed to find DistributedState service: "+name);
   }

   public void start() throws Exception
   {
      super.start ();

      String ejbName = this.getContainer ().getBeanMetaData ().getEjbName ();
      this.DS_CATEGORY = "CMPClusteredInMemoryPersistenceManager-" + ejbName;

      this.ds.registerDSListenerEx (this.DS_CATEGORY, this);
   }

   /* From Service interface*/
   public void stop()
   {
      super.stop ();
      this.ds.unregisterDSListenerEx (this.DS_CATEGORY, this);
   }

   // DSListener implementation -------------------------------------

   /**
    * Called whenever a key has been removed from a category the called object had
    * subscribed in.
    * @param category The category under which a key has been removed
    * @param key The key that has been removed
    * @param previousContent The previous content of the key that has been removed
    */
   public void keyHasBeenRemoved (String category, java.io.Serializable key, java.io.Serializable previousContent, boolean locallyModified)
   {
      if (!locallyModified)
         this.cacheMiss ((String)key);
   }

   /**
    * Called whenever a key has been added or modified in the category the called object
    * has subscribed in.
    * @param category The category of the modified/added entry
    * @param key The key that has been added or its value modified
    * @param value The new value of the key
    */
   public void valueHasChanged (String category, java.io.Serializable key, java.io.Serializable value, boolean locallyModified)
   {
      if (!locallyModified)
         this.cacheMiss ((String)key);
   }

   public void cacheMiss(String key)
   {
      // a modification has occured on another node, we clean the cache!

      try
      {
         this.remove(key);
      }
      catch (Exception e)
      {
         log.warn("failed to remove key" ,e);
      }
   }

}

