/*
 * JBoss, the OpenSource WebOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.web.catalina.session;

import org.apache.catalina.Context;

/**
  A concrete implementation of the snapshot manager interface
  that does instant replication of a modified session
  @author Thomas Peuss <jboss@peuss.de>
  @version $Revision: 1.3.2.3 $
  */
public class InstantSnapshotManager extends SnapshotManager
{
   public InstantSnapshotManager(ClusterManager manager, Context context, boolean economicSnapshotting)
   {
      super(manager, context, economicSnapshotting);
   }

   /**
     Instant replication of the modified session
     */
   public void snapshot(String id)
   {
      try {
	 // find the session that has been modified
         ClusteredSession session=(ClusteredSession)manager.findSession(id);

	 if (economicSnapshotting) {
	    // First look at the session - is it modified?
	    if (session.isModified()) {
	       manager.storeSession(session);
	    }
	 } else {
	    manager.storeSession(session);
	 }
	 session.setModified(false);
      } catch (Exception e) {
	 log.error("Could not distribute session",e);
      }
   }

   public void start()
   {
   }

   public void stop()
   {
   }
}
