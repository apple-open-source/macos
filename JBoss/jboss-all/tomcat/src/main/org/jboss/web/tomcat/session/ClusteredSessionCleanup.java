/*
 * JBoss, the OpenSource WebOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.web.tomcat.session;

import org.apache.catalina.Session;
import org.jboss.logging.Logger;
import org.jboss.metadata.WebMetaData;

/** This class removes expired sessions from the clustered
 *  session manager

 @see org.jboss.ha.httpsession.server.ClusteredHTTPSessionService

 @author Thomas Peuss <jboss@peuss.de>
 @version $Revision: 1.1.1.1.2.1 $
 */
public class ClusteredSessionCleanup implements Runnable
{
   // The Manager for which sessions are checked
   protected ClusterManager manager;

   // A Log-object
   protected Logger log;

   // Interval of checks in seconds
   protected int checkInterval = 60;

   // Has the thread ended?
   protected boolean threadDone = false;

   // The cleaner-thread
   protected Thread thread = null;

   // The name for this thread
   protected String threadName = null;

   public ClusteredSessionCleanup(ClusterManager manager, Logger log)
   {
      this.manager = manager;
      this.log = log;
   }

   /**
    * Go through all sessions and look if they have expired
    */
   protected void processExpires()
   {
      // What's the time?
      long timeNow = System.currentTimeMillis();

      // Get all sessions
      Session sessions[] = manager.findSessions();

      log.debug("Looking for sessions that have expired");


      for (int i = 0; i < sessions.length; ++i)
      {
         ClusteredSession session = (ClusteredSession) sessions[i];

         // We only look at valid sessions
         if (!session.isValid())
         {
            continue;
         }

         // How long are they allowed to be idle?
         int maxInactiveInterval = session.getMaxInactiveInterval();

         // Negative values = never expire
         if (maxInactiveInterval < 0)
         {
            continue;
         }

         // How long has this session been idle?
         int timeIdle =
               (int) ((timeNow - session.getLastAccessedTime()) / 1000L);

         // Too long?
         if (timeIdle >= maxInactiveInterval)
         {
            try
            {
               log.debug("Session with id = " + session.getId() + " has expired on local node");
               // Did another node access this session?
               // Try to get the session from the clustered store
               ClusteredSession clusteredSession = manager.loadSession(session.getId());
               if (clusteredSession != null)
               {
                  int timeIdleCluster =
                        (int) ((timeNow - clusteredSession.getLastAccessedTime()) / 1000L);
                  if (timeIdleCluster < maxInactiveInterval)
                  {
                     log.debug("Session " + session.getId() + " has only expired on local node but is alive on another node - removing only from local store");
                     // Remove from local store, because the session is
                     // alive on another node
                     manager.removeLocal(session);
                     continue;
                  }

                  log.debug("Session " + session.getId() + " has also expired on all other nodes - removing globally");
               }


               // Kick this session
               try
               {
                  session.setReplicationTypeForSession ( WebMetaData.REPLICATION_TYPE_ASYNC );
                  session.expire();
               }
               finally
               {
                  session.setReplicationTypeForSession (manager.getReplicationType ());
               }

            }
            catch (Throwable t)
            {
               log.error("Problems while expiring session with id = " + session.getId(), t);
            }
         }
      }
   }

   /**
    * Sleep for the duration specified by the <code>checkInterval</code>
    * property.
    */
   protected void threadSleep()
   {

      try
      {
         Thread.sleep(checkInterval * 1000L);
      }
      catch (InterruptedException e)
      {
         ;
      }

   }

   /**
    * Start up the cleanup thread
    */
   protected void threadStart()
   {
      if (thread != null)
      {
         return;
      }

      threadDone = false;
      threadName = "ClusterManagerCleanupThread[" + manager.contextPath + "]";
      thread = new Thread(this, threadName);
      thread.setDaemon(true);
      thread.setContextClassLoader(manager.getContainer().getLoader().getClassLoader());
      thread.start();
   }

   /**
    * Stop the cleanup thread
    */
   protected void threadStop()
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
         // ignore
      }

      thread = null;
   }

   /**
    * Work-loop
    */
   public void run()
   {
      while (!threadDone)
      {
         threadSleep();
         processExpires();
      }
   }
}
