/*
 * JBoss, the OpenSource WebOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.web.tomcat.session;

import java.util.HashMap;
import java.util.Iterator;

import org.apache.catalina.Context;
import org.jboss.logging.Logger;

/**
 A snapshot manager that collects all modified sessions over a given
 period of time and distributes them en bloc.
 @author Thomas Peuss <jboss@peuss.de>
 @version $Revision: 1.1.1.1.2.2 $
 */
public class IntervalSnapshotManager extends SnapshotManager implements Runnable
{
   static Logger log = Logger.getLogger(IntervalSnapshotManager.class);

   // the interval in ms
   protected int interval = 1000;

   // the modified sessions
   protected HashMap sessions = new HashMap();

   // the distribute thread
   protected Thread thread = null;

   // has the thread finished?
   protected boolean threadDone = false;

   public IntervalSnapshotManager(ClusterManager manager, Context context)
   {
      super(manager, context);
   }

   public IntervalSnapshotManager(ClusterManager manager, Context context, int interval)
   {
      super(manager, context);
      this.interval = interval;
   }

   /**
    Store the modified session in a hashmap for the distributor thread
    */
   public void snapshot(String id)
   {
      try
      {
         ClusteredSession session = (ClusteredSession) manager.findSession(id);
         synchronized (sessions)
         {
            sessions.put(id, session);
         }
      }
      catch (Exception e)
      {
         log.warn("Failed to replicate sessionID:" + id, e);
      }
   }

   /**
    Distribute all modified sessions
    */
   protected void processSessions()
   {
      HashMap copy = new HashMap (sessions.size());

      synchronized (sessions)
      {
         copy.putAll(sessions);
         sessions.clear();
      }
      Iterator iter = copy.values().iterator();

      // distribute all modified sessions using default replication type
      while (iter.hasNext())
      {
         ClusteredSession session = (ClusteredSession) iter.next();
         session.setReplicationTypeForSession ( ((ClusterManager)manager).getReplicationType ());
         manager.storeSession(session);
      }
      copy.clear();
   }

   /**
    Start the snapshot manager
    */
   public void start()
   {
      startThread();
   }

   /**
    Stop the snapshot manager
    */
   public void stop()
   {
      stopThread();
      synchronized (sessions)
      {
         sessions.clear();
      }
   }

   /**
    Start the distributor thread
    */
   protected void startThread()
   {
      if (thread != null)
      {
         return;
      }

      thread = new Thread(this, "ClusteredSessionDistributor[" + contextPath + "]");
      thread.setDaemon(true);
      thread.setContextClassLoader(manager.getContainer().getLoader().getClassLoader());
      threadDone = false;
      thread.start();
   }

   /**
    Stop the distributor thread
    */
   protected void stopThread()
   {
      if (thread == null)
      {
         return;
      }
      threadDone = true;
      thread.interrupt();
      try
      {
         thread.join();
      }
      catch (InterruptedException e)
      {
      }
      thread = null;
   }

   /**
    Little Thread - sleep awhile...
    */
   protected void threadSleep()
   {
      try
      {
         Thread.sleep(interval);
      }
      catch (InterruptedException e)
      {
      }
   }

   /**
    Thread-loop
    */
   public void run()
   {
      while (!threadDone)
      {
         threadSleep();
         processSessions();
      }
   }
}
