/*
 * JBoss, the OpenSource WebOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.web.tomcat.tc5.session;

import org.apache.catalina.Context;

/**
 Abstract base class for a session snapshot manager.
 @author Thomas Peuss <jboss@peuss.de>
 @version $Revision: 1.1.2.1 $
 */
public abstract class SnapshotManager
{
   // The manager the snapshot manager should use
   protected JBossManager manager;

   // The context-path
   protected String contextPath;

   public SnapshotManager(JBossManager manager, String path)
   {
      this.manager = manager;
      contextPath = path;
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
