/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.mx.util;

import java.util.Iterator;
import java.util.TreeSet;

/**
 * A runnable scheduler.<p>
 * 
 * The scheduler needs to be started to do real work. To add work to the
 * scheduler, create a SchedulableRunnable and set the scheduler. When
 * the next run has passed the work is performed.
 * 
 * @see SchedulableRunnable
 *
 * @author  <a href="mailto:Adrian.Brock@HappeningTimes.com">Adrian Brock</a>.
 * @version $Revision: 1.1 $
 */
public class RunnableScheduler
   implements Runnable
{
   // Attributes ----------------------------------------------------

   /**
    * The runnables to schedule
    */
   private TreeSet runnables = new TreeSet();

   /**
    * The thread pool used to process the runnables.
    */
   private ThreadPool threadPool;

   /**
    * The controller thread.
    */
   private Thread controller = null;

   // Static --------------------------------------------------------

   // Constructors --------------------------------------------------

   /**
    * Constructs a new runnable scheduler.
    */
   public RunnableScheduler()
   {
   }

   /**
    * Start the scheduler
    */
   public synchronized void start()
   {
      if (controller != null)
         return;
      controller = new Thread(this);
      controller.start();
   }

   /**
    * Stop the scheduler
    */
   public synchronized void stop()
   {
      if (controller == null)
         return;
      controller.interrupt();
      controller = null;
   }

   /**
    * Run the scheduler
    */
   public void run()
   {
      // Start the threadpool
      threadPool = new ThreadPool();
      threadPool.setActive(true);
      try
      {
         // Do outstanding work until stopped
         while (true)
         {
            try
            {
               runOutstanding();
               waitOutstanding();
            }
            catch (InterruptedException weAreDone)
            {
               break;
            }
         }
      }
      finally
      {
         // Stop the thread pool
         threadPool.setActive(false);
         threadPool = null;
      }
   }

   // Public --------------------------------------------------------

   // X Implementation ----------------------------------------------

   // Y Overrides ---------------------------------------------------

   // Protected -----------------------------------------------------

   // Package -------------------------------------------------------

   /**
    * Add a schedulable runnable
    *
    * @param runnable the runnable to add
    */
   synchronized void add(SchedulableRunnable runnable)
   {
      runnables.add(runnable);
      notifyAll();
   }

   /**
    * Remove a schedulable runnable
    *
    * @param runnable the runnable to add
    */
   synchronized void remove(SchedulableRunnable runnable)
   {
      runnables.remove(runnable);
   }

   /**
    * Check whether the scheduler contains a runnable
    *
    * @param runnable the runnable to check
    * @return true when the runnable is present, false otherwise
    */
   synchronized boolean contains(SchedulableRunnable runnable)
   {
      return runnables.contains(runnable);
   }

   // Private -------------------------------------------------------

   /**
    * Run all outstanding runnables, they are in date order
    */
   private synchronized void runOutstanding()
   {
      long current = System.currentTimeMillis();
      Iterator iterator = runnables.iterator();
      while (iterator.hasNext())
      {
         SchedulableRunnable next = (SchedulableRunnable) iterator.next();
         if (next.getNextRun() <= current)
         {
            iterator.remove();
            threadPool.run(next);
         }
         else
            break;
      }
   }

   /**
    * Wait for the next outstanding runnable
    */
   private synchronized void waitOutstanding()
      throws InterruptedException
   {
      // There is nothing to run
      if (runnables.size() == 0)
         wait();
      else
      {
         // Wait until the next runnable
         SchedulableRunnable next = (SchedulableRunnable) runnables.first();
         long wait = next.getNextRun() - System.currentTimeMillis();
         if (wait > 0)
            wait(wait);
      }
   }

   // Inner Classes -------------------------------------------------
}

