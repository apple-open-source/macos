
/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 *
 */

package org.jboss.resource.connectionmanager;

import java.util.ArrayList;


import java.util.Collection;
import java.util.Iterator;
import org.jboss.logging.Logger;

/**
 * IdleRemover.java
 *
 *
 * Created: Thu Jan  3 21:44:35 2002
 *
 * @author <a href="mailto:d_jencks@users.sourceforge.net">David Jencks</a>
 * @version
 */

public class IdleRemover 
{
   private final Logger log = Logger.getLogger(getClass());

   private final Collection pools = new ArrayList();

   private long interval = Long.MAX_VALUE;

   private long next = Long.MAX_VALUE;//important initialization!

   private static final IdleRemover remover = new IdleRemover();

   private final Thread removerThread;

   public static void  registerPool(InternalManagedConnectionPool mcp, long interval)
   {
      remover.internalRegisterPool(mcp, interval);
   }

   public static void unregisterPool(InternalManagedConnectionPool mcp)
   {
      remover.internalUnregisterPool(mcp);
   }

   private IdleRemover ()
   {
      removerThread = new Thread(
         new Runnable() {

            public void run()
            {
               synchronized (pools)
               {
                  while (true)
                  {
                     try 
                     {
                        pools.wait(interval);
                        log.debug("run: IdleRemover notifying pools, interval: " + interval);
                        for (Iterator i = pools.iterator(); i.hasNext(); ) 
                        {
                           ((InternalManagedConnectionPool)i.next()).removeTimedOut();
                        } // end of if ()
                        next = System.currentTimeMillis() + interval;
                        if (next < 0) 
                        {
                           next = Long.MAX_VALUE;      
                        } // end of if ()
                        
                     }
                     catch (InterruptedException ie)
                     {
                        log.info("run: IdleRemover has been interrupted, returning");
                        return;  
                     } // end of try-catch
                     catch (RuntimeException e)
                     {
                        log.warn("run: IdleRemover ignored unexpected runtime exception", e);
                     }
                     catch (Error e)
                     {
                        log.warn("run: IdleRemover ignored unexpected error", e);
                     }
                        
                  } // end of while ()
                  
               }
            }
         }, "IdleRemover");
      removerThread.start();
      
   }

   private void internalRegisterPool(InternalManagedConnectionPool mcp, long interval)
   {
      log.debug("internalRegisterPool: registering pool with interval " + interval + " old interval: " + this.interval);
      synchronized (pools)
      {
         pools.add(mcp);
         if (interval > 1 && interval/2 < this.interval) 
         {
            this.interval = interval/2;
            long maybeNext = System.currentTimeMillis() + this.interval;
            if (next > maybeNext && maybeNext > 0) 
            {
               next = maybeNext;
               log.debug("internalRegisterPool: about to notify thread: old next: " + next + ", new next: " + maybeNext);
               pools.notify();
               //removerThread.interrupt();
            } // end of if ()
            
         } // end of if ()
         
      }
   }

   private void internalUnregisterPool(InternalManagedConnectionPool mcp)
   {
      synchronized (pools)
      {
         pools.remove(mcp);
         if (pools.size() == 0) 
         {
            log.debug("internalUnregisterPool: setting interval to Long.MAX_VALUE");
            interval = Long.MAX_VALUE;
         } // end of if ()
         
      }
   }

   private void stop()
   {
      interval = -1;
      log.debug("stop: stopping IdleRemover");
      removerThread.interrupt();
   }
}// IdleRemover
