/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 *
 */
package org.jboss.resource.connectionmanager;

import java.util.Collections;
import java.util.HashSet;
import java.util.Iterator;

import javax.resource.ResourceException;
import javax.resource.spi.ConnectionRequestInfo;
import javax.resource.spi.ManagedConnection;
import javax.resource.spi.ManagedConnectionFactory;
import javax.security.auth.Subject;

import org.jboss.logging.Logger;
import org.jboss.resource.JBossResourceException;
import org.jboss.util.ProfilerPoint;

import EDU.oswego.cs.dl.util.concurrent.FIFOSemaphore;

/**
 * The internal pool implementation
 *
 * @author <a href="mailto:d_jencks@users.sourceforge.net">David Jencks</a>
 * @author <a href="mailto:adrian@jboss.org">Adrian Brock</a>
 * @version $Revision: 1.5.2.22 $
 */

public class InternalManagedConnectionPool
{
   /** The managed connection factory */
   private final ManagedConnectionFactory mcf;

   /** The connection listener factory */
   private final ConnectionListenerFactory clf;

   /** The default subject */
   private final Subject defaultSubject;

   /** The default connection request information */
   private final ConnectionRequestInfo defaultCri;

   /** The pooling parameters */
   private final PoolParams poolParams;

   /** Copy of the maximum size from the pooling parameters.
    *  Dynamic changes to this value are not compatible with
    *  the semaphore which cannot be dynamically changed.
    */
   private int maxSize;
        

   /** The available connection event listeners */
   private ConnectionListener[] cls;
   private int currentPoolIndex = -1;

   /** The permits used to control who can checkout a connection */
   private final FIFOSemaphore permits;

   /** The log */
   private final Logger log;

   /** Whether trace is enabled */
   private final boolean traceEnabled;

   /** Stats */
   private final Counter connectionCounter = new Counter();

   /** The checked out connections */
   private final HashSet checkedOut = new HashSet();

   /** Whether the pool has been started */
   private boolean started = false;

   /** Whether the pool has been shutdown */
   private boolean shutdown = false;

   /** the max connections ever checked out **/
   private volatile int maxUsedConnections = 0;

   /**
    * Create a new internal pool
    *
    * @param mcf the managed connection factory
    * @param subject the subject
    * @param cri the connection request information
    * @param poolParams the pooling parameters
    * @param log the log
    */
   public InternalManagedConnectionPool
   (
      ManagedConnectionFactory mcf,
      ConnectionListenerFactory clf,
      Subject subject,
      ConnectionRequestInfo cri,
      PoolParams poolParams,
      Logger log
   )
   {
      this.mcf = mcf;
      this.clf = clf;
      defaultSubject = subject;
      defaultCri = cri;
      this.poolParams = poolParams;
      this.maxSize = poolParams.maxSize;
      this.log = log;
      this.traceEnabled = log.isTraceEnabled();
      cls = new ConnectionListener[this.maxSize];
      permits = new FIFOSemaphore(this.maxSize);
      if (poolParams.idleTimeout != 0)
         IdleRemover.registerPool(this, poolParams.idleTimeout);
   }

   public long getAvailableConnections()
   {
      return permits.permits();
   }

   public int getMaxConnectionsInUseCount()
   {
      return maxUsedConnections;
   }

   public int getConnectionInUseCount ()
   {
      return checkedOut.size();
   }

   /**
    * todo distinguish between connection dying while match called
    * and bad match strategy.  In latter case we should put it back in
    * the pool.
    */
   public ConnectionListener getConnection(Subject subject, ConnectionRequestInfo cri)
      throws ResourceException
   {
      subject = (subject == null) ? defaultSubject: subject;
      cri = (cri == null) ? defaultCri: cri;
      long startWait = System.currentTimeMillis();
      try
      {
         if (permits.attempt(poolParams.blockingTimeout))
         {
            //We have a permit to get a connection. Is there one in the pool already?
            ConnectionListener cl = null;
            do
            {
               synchronized (cls)
               {
                  if (shutdown)
                  {
                     permits.release();
                     throw new ResourceException("The pool has been shutdown");
                  }

                  if (currentPoolIndex >= 0)
                  {
                     cl = cls[currentPoolIndex];
                     cls[currentPoolIndex--] = null;
                     checkedOut.add(cl);
                     int size =(int)(maxSize - permits.permits());
                     if (size > maxUsedConnections) maxUsedConnections = size;
                  }
               }
               if (cl != null)
               {
                  //Yes, we retrieved a ManagedConnection from the pool. Does it match?
                  try
                  {
                     Object matchedMC = mcf.matchManagedConnections(Collections.singleton(cl.getManagedConnection()), subject, cri);
                     if (matchedMC != null)
                     {
                        if (traceEnabled)
                           log.trace("supplying ManagedConnection from pool: " + matchedMC);
                        return cl;
                     }

                     //Match did not succeed but no exception was thrown.
                     //Either we have the matching strategy wrong or the
                     //connection died while being checked.  We need to
                     //distinguish these cases, but for now we always
                     //destroy the connection.
                     log.warn("Destroying connection that could not be successfully matched: " + cl.getManagedConnection());
                     synchronized(cls)
                     {
                        checkedOut.remove(cl);
                     }
                     doDestroy(cl);
                     cl = null;
                  }
                  catch (Throwable t)
                  {
                     log.warn("Throwable while trying to match ManagedConnection, destroying connection:", t);
                     synchronized(cls)
                     {
                        checkedOut.remove(cl);
                     }
                     doDestroy(cl);
                     cl = null;
                  }
               } // end of if ()
            }
            while (currentPoolIndex >=0);//end of do loop

            //OK, we couldnt find a working connection from the pool.  Make a new one.
            try
            {
               cl = createConnectionEventListener(subject, cri);
               //No, the pool was empty, so we have to make a new one.
               synchronized(cls)
               {
                  checkedOut.add(cl);
                  int size =(int)(maxSize - permits.permits());
                  if (size > maxUsedConnections) maxUsedConnections = size;
               }

               //lack of synch on "started" probably ok, if 2 reads occur we will just
               //run fillPool twice, no harm done.
               if (started == false)
               {
                  started = true;
                  PoolFiller.fillPool(this);
               }
               if (traceEnabled)
                  log.trace("supplying new ManagedConnection: " + cl.getManagedConnection());
               return cl;
            }
            catch (Throwable t)
            {
               log.warn("Throwable while attempting to get a new connection: ", t);
               //return permit and rethrow
               synchronized(cls)
               {
                  checkedOut.remove(cl);
               }
               permits.release();
               if (t instanceof ResourceException)
                  throw (ResourceException)t;
               throw new JBossResourceException("Unexpected throwable while trying to create a connection: ", t);
            }
         }
         else
         {
            // we timed out
            throw new ResourceException("No ManagedConnections available within configured blocking timeout ( " +
			                             poolParams.blockingTimeout + " [ms] )");
         }

      }
      catch (InterruptedException ie)
      {
         long end = System.currentTimeMillis() - startWait;
         long invocationTime = System.currentTimeMillis() - ProfilerPoint.getStartTime();
         throw new ResourceException("Interrupted while requesting permit!  Waited " + end + " ms, invocation time: " + invocationTime);
      }
   }

   public void returnConnection(ConnectionListener cl, boolean kill)
   {
      if (cl.getState() == ConnectionListener.DESTROYED)
      {
         log.trace("ManagedConnection is being returned after it was destroyed" + cl.getManagedConnection());
         return;
      }

      log.trace("putting ManagedConnection back into pool");
      boolean wasInPool = false;
      try
      {
         try
         {
            cl.getManagedConnection().cleanup();
         }
         catch (ResourceException re)
         {
            log.warn("ResourceException cleaning up ManagedConnection:" + re);
            kill = true;
         }

         // We need to destroy this one
         if (cl.getState() == ConnectionListener.DESTROY)
            kill = true;
         synchronized(cls)
         {
            checkedOut.remove(cl);
            if (kill)
            {
               for (int i = 0; i <= currentPoolIndex; ++i)
               {
                  if (cl.equals(cls[i]))
                  {
                     wasInPool = true;
                     break;
                  }
               }
            }
            else
            {
               cl.used();
               if (currentPoolIndex + 1 > cls.length)
               {
                  kill = true;
                  log.warn("Releasing a connection because pool count is maxed");
                  for (int i = 0; i <= currentPoolIndex; ++i)
                  {
                     if (cl.equals(cls[i]))
                     {
                        wasInPool = true;
                        break;
                     }
                  }
               }
               else
               {
                  cls[++currentPoolIndex] = cl;
               }
            }
         }

         if (kill)
            doDestroy(cl);
      }
      finally
      {
         if (wasInPool == false)
            permits.release();
      }
   }

   public void flush()
   {
      synchronized (cls)
      {
         // Mark checked out connections as requiring destruction
         for (Iterator i = checkedOut.iterator(); i.hasNext();)
         {
            ConnectionListener cl = (ConnectionListener) i.next();
            cl.setState(ConnectionListener.DESTROY);
         }
         // Destroy connections in the pool
         while (currentPoolIndex >=0)
         {
            ConnectionListener cl = cls[currentPoolIndex];
            cls[currentPoolIndex--] = null;
            doDestroy(cl);
         }
      }
   }


   public void removeTimedOut()
   {
      synchronized (cls)
      {
         long timeout = System.currentTimeMillis() - poolParams.idleTimeout;
         int kept = currentPoolIndex + 1;
         for (int i = 0; i <= currentPoolIndex; ++i)
         {
            if (cls[i].isTimedOut(timeout))
               doDestroy(cls[i]);
            else
            {
               //They were inserted in chronologically, so if this one isn't timed out, following ones won't be either.
               kept = i;
               break;
            }
         }
         // Shuffle the array if we destroyed something
         if (kept > 0)
         {
            for (int i = 0; i <= currentPoolIndex; ++i)
            {
               int other = i + kept;
               if (other > currentPoolIndex)
                  cls[i] = null; // GC
                else
                  cls[i] = cls[other];
             }
             currentPoolIndex -= kept;
          }
      }
      //refill if necessary, asynchronously.
      PoolFiller.fillPool(this);
   }

   public void shutdown()
   {
      synchronized (cls)
      {
         shutdown = true;
         flush();
      }
      IdleRemover.unregisterPool(this);
   }

   public void fillToMin()
   {
      boolean gotPermit = false;
      try
      {
         if (permits.attempt(poolParams.blockingTimeout))
         {
            gotPermit = true;
            while (true)
            {
               synchronized (cls)
               {
                  if (shutdown)
                     return;

                  // We have enough connections now
                  if (getMinSize() - connectionCounter.getGuaranteedCount() <= 0)
                     return;

                  try
                  {
                     ConnectionListener cl = createConnectionEventListener(defaultSubject, defaultCri);
                     cls[++currentPoolIndex] = cl;
                  }
                  catch (ResourceException re)
                  {
                     log.warn("Unable to fill pool ", re);
                     return;
                  }
               }
            }
         }
         else
         {
            // we timed out
            log.warn("Unable to fill the pool due to timeout ( " +
                                      poolParams.blockingTimeout + " [ms] )");
         }
      }
      catch (InterruptedException ie)
      {
         log.warn("Interrupted while requesting permit!");
      }
      finally
      {
         if (gotPermit)
            permits.release();
      }
   }

   public int getConnectionCount()
   {
      return connectionCounter.getCount();
   }

   public int getConnectionCreatedCount()
   {
      return connectionCounter.getCreatedCount();
   }

   public int getConnectionDestroyedCount()
   {
      return connectionCounter.getDestroyedCount();
   }

   /**
    * Create a connection event listener
    *
    * @param subject the subject
    * @param cri the connection request information
    * @return the new listener
    * @throws ResourceException for any error
    */
   private ConnectionListener createConnectionEventListener(Subject subject, ConnectionRequestInfo cri)
      throws ResourceException
   {
      ManagedConnection mc = mcf.createManagedConnection(subject, cri);
      connectionCounter.inc();
      try
      {
         return clf.createConnectionListener(mc, this);
      }
      catch (ResourceException re)
      {
         connectionCounter.dec();
         mc.destroy();
         throw re;
      }
   }

   /**
    * Destroy a connection
    *
    * @param cl the connection to destroy
    */
   private void doDestroy(ConnectionListener cl)
   {
      connectionCounter.dec();
      cl.setState(ConnectionListener.DESTROYED);
      try
      {
         cl.getManagedConnection().destroy();
      }
      catch (Throwable t)
      {
         log.warn("Exception destroying ManagedConnection", t);
      }
   }

   /**
    * Guard against configurations or 
    * dynamic changes that may increase the minimum
    * beyond the maximum
    */
   private int getMinSize()
   {
      if (poolParams.minSize > maxSize)
         return maxSize;
      return poolParams.minSize;
   }

   public static class PoolParams
   {
      public int minSize = 0;
      public int maxSize = 10;
      public int blockingTimeout = 5000; //milliseconds
      public long idleTimeout = 1000*60*30; //milliseconds, 30 minutes.
   }

   /**
    * Stats
    */
   private static class Counter
   {
      private int created = 0;
      private int destroyed = 0;

      synchronized int getGuaranteedCount()
      {
         return created - destroyed;
      }

      int getCount()
      {
         return created - destroyed;
      }

      int getCreatedCount()
      {
         return created;
      }

      int getDestroyedCount()
      {
         return destroyed;
      }

      synchronized void inc()
      {
         ++created;
      }

      synchronized void dec()
      {
         ++destroyed;
      }
   }
}
