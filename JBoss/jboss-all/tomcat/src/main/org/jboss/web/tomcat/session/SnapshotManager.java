/*
 * JBoss, the OpenSource WebOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.web.tomcat.session;

import org.apache.catalina.Context;

/**
 Abstract base class for a session snapshot manager.
 @author Thomas Peuss <jboss@peuss.de>
 @version $Revision: 1.1.1.1 $
 */
public abstract class SnapshotManager
{
   // The manager the snapshot manager should use
   protected ClusterManager manager;

   // The context-path
   protected String contextPath;

   public SnapshotManager(ClusterManager manager, Context context)
   {
      this.manager = manager;
      contextPath = context.getPath();
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
