/*
 * JBoss, the OpenSource WebOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.web.catalina.session;

import org.apache.catalina.Context;
import java.util.HashMap;
import java.util.Iterator;

/**
  A snapshot manager that collects all modified sessions over a given
  period of time and distributes them en bloc.
  @author Thomas Peuss <jboss@peuss.de>
  @version $Revision: 1.3.2.4 $
 */
public class IntervalSnapshotManager extends SnapshotManager implements Runnable
{
   // the interval in ms
   protected int interval=1000;

   // the modified sessions
   protected HashMap sessions=new HashMap();

   // the distribute thread
   protected Thread thread=null;

   // has the thread finished?
   protected boolean threadDone=false;
    
   public IntervalSnapshotManager(ClusterManager manager, Context context, boolean economicSnapshotting)
   {
      super(manager, context, economicSnapshotting);
   }

   public IntervalSnapshotManager(ClusterManager manager, Context context, int interval, boolean economicSnapshotting)
   {
      super(manager, context, economicSnapshotting);
      this.interval=interval;
   }
   
   /**
     Store the modified session in a hashmap for the distributor thread
     */
   public void snapshot(String id)
   {
      try {
         ClusteredSession session=(ClusteredSession)manager.findSession(id);
	 synchronized(sessions) {
	    sessions.put(id,session);
	 }
      } catch (Exception e) {
	 log.error("Could not distribute session",e);
      }
   }

   /**
     Distribute all modified sessions
     */
   protected void processSessions()
   {
      HashMap work;
      
      log.debug("Processing modified sessions");
      
      synchronized(sessions) {
	 // Make a copy of the HashMap to release the lock quickly
	 work=(HashMap)sessions.clone();
	 sessions.clear();
      }

      Iterator iter=work.values().iterator();

      // distribute all modified sessions
      while(iter.hasNext()) {
	 ClusteredSession session=(ClusteredSession)iter.next();
	 if (economicSnapshotting) {
	    if(session.isModified()) {
	       manager.storeSession(session);
	    }
	 } else {
	    manager.storeSession(session);
	 }
	 session.setModified(false);
	 log.trace("Session with id="+session.getId()+" processed");
      }
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
      synchronized(sessions) {
         sessions.clear();
      }
   }
   
   /**
     Start the distributor thread
     */
   protected void startThread()
   {
     if(thread!=null) {
	return;
     }
      
     thread=new Thread(this,"ClusteredSessionDistributor["+contextPath+"]");
     thread.setDaemon(true);
     thread.setContextClassLoader(manager.getContainer().getLoader().getClassLoader());
     threadDone=false;
     thread.start();
   }

   /**
     Stop the distributor thread
     */
   protected void stopThread()
   {
      if(thread==null) {
	 return;
      }
      threadDone=true;
      thread.interrupt();
      try {
	 thread.join();
      } catch (InterruptedException e) {
      }
      thread=null;
   }
   
   /**
     Little Thread - sleep awhile...
     */
   protected void threadSleep()
   {
      try {
	 Thread.sleep(interval);
      } catch (InterruptedException e) {
      }
   }
   
   /**
     Thread-loop
     */
   public void run()
   {
      while(!threadDone) {
	 threadSleep();
	 processSessions();
      }
   }
}
