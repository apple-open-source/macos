/*
 * Licensed under the X license (see http://www.x.org/terms.htm)
 */
package org.jboss.pool;

import java.util.HashSet;
import java.util.Iterator;

import org.jboss.logging.Logger;

/**
 * Runs garbage collection on all available pools. Only one GC thread is
 * created, no matter how many pools there are - it just tries to calculate the
 * next time it should run based on the figues for all the pools. It will run
 * on any pools which are "pretty close" to their requested time.
 *
 * @author  Aaron Mulder (ammulder@alumni.princeton.edu)
 */
class PoolGCThread extends Thread
{
   /** Class logger. */
   private static Logger log = Logger.getLogger(PoolGCThread.class);

   private HashSet pools = new HashSet();

   PoolGCThread() {
      super( "Minerva ObjectPool GC Thread" );
      setDaemon( true );
   }

   public void run() {
      boolean debug = log.isDebugEnabled();
      if (debug)
         log.debug("Started gc thread");

      while ( true ) {
         // Don't do anything while there's nothing to do
         waitForPools();
         if (debug)
            log.debug("gc thread waited for pools");

         // Figure out how long to sleep
         long delay = getDelay();
         if (debug)
            log.debug("gc thread delay: " + delay);

         // Sleep
         if ( delay > 0l ) {
            try {
               sleep( delay );
            } catch ( InterruptedException ignore ) {}
         }

         // Run garbage collection on eligible pools
         runGC();
      }
   }

   synchronized void addPool( ObjectPool pool ) {
      if (log.isDebugEnabled()) {
         log.debug("Adding pool: " + pool.getName() +
                   ", Cleanup enabled: " + pool.isCleanupEnabled());
      }
      if ( pool.isCleanupEnabled() ) {
         pools.add( pool );
      }
      notify();
   }

   synchronized void removePool( ObjectPool pool ) {
      pools.remove( pool );
   }

   private synchronized long getDelay() {
      long next = Long.MAX_VALUE;
      long now = System.currentTimeMillis();
      long current;
      for ( Iterator it = pools.iterator(); it.hasNext();  ) {
         ObjectPool pool = ( ObjectPool )it.next();
         current = pool.getNextCleanupMillis( now );
         if ( current < next ) {
            next = current;
         }
      }
      return next >= 0l ? next : 0l;
   }

   private synchronized void waitForPools() {
      while ( pools.size() == 0 ) {
         try {
            wait();
         } catch ( InterruptedException ignore ) {
            log.debug("waitForPools interrupted");
         }
      }
   }

   private synchronized void runGC() {
      boolean debug = log.isDebugEnabled();
      if (debug) {
         log.debug("gc thread running gc");
      }

      for ( Iterator it = pools.iterator(); it.hasNext();  ) {
         ObjectPool pool = ( ObjectPool )it.next();

         if (debug) {
            log.debug("gc thread pool: " + pool.getName() +
                      " isTimeToCleanup()" + pool.isTimeToCleanup());
         }
         if ( pool.isTimeToCleanup() ) {
            pool.runCleanupandShrink();
         }
      }
   }
}
