/*
 * JBoss, the OpenSource WebOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.web.tomcat.session;

import org.apache.catalina.Context;
import org.jboss.logging.Logger;

/**
 A concrete implementation of the snapshot manager interface
 that does instant replication of a modified session
 @author Thomas Peuss <jboss@peuss.de>
 @version $Revision: 1.1.1.1.2.1 $
 */
public class InstantSnapshotManager extends SnapshotManager
{
   static Logger log = Logger.getLogger(InstantSnapshotManager.class);

   public InstantSnapshotManager(ClusterManager manager, Context context)
   {
      super(manager, context);
   }

   /**
    Instant replication of the modified session
    */
   public void snapshot(String id)
   {
      try
      {
         // find the session that has been modified
         ClusteredSession session = (ClusteredSession) manager.findSession(id);

         // replicate it using default replication type
         session.setReplicationTypeForSession ( ((ClusterManager)manager).getReplicationType ());
         manager.storeSession(session);
      }
      catch (Exception e)
      {
         log.warn("Failed to replicate sessionID:" + id, e);
      }
   }

   public void start()
   {
   }

   public void stop()
   {
   }
}
