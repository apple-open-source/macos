/*
 * JBoss, the OpenSource WebOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.web.catalina.session;

import org.apache.catalina.Context;
import org.jboss.logging.Logger;

/**
  Abstract base class for a session snapshot manager.
  @author Thomas Peuss <jboss@peuss.de>
  @version $Revision: 1.3.2.4 $
 */
public abstract class SnapshotManager
{
   // The manager the snapshot manager should use
   protected ClusterManager manager;

   // The context-path
   protected String contextPath;

   // Logfile
   protected Logger log;

   protected boolean economicSnapshotting;

   public SnapshotManager(ClusterManager manager, Context context, boolean economicSnapshotting)
   {
      this.manager=manager;
      contextPath=context.getPath();
      if(contextPath.equals("")) {
	 contextPath="/";
      }
      log=Logger.getLogger(this.getClass().getName());
      this.economicSnapshotting=economicSnapshotting;
   }
   
   /** Tell the snapshot manager which session was modified and
       must be replicated */
   public abstract void snapshot(String id);

   /**
     Start the snapshot manager
     */
   public abstract void start();

   /**
     Stop the snapshot manager
     */
   public abstract void stop();
}
