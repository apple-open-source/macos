/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.web;

import java.util.Stack;

/**
 *  A simple thread pool.
 *
 *  <a href="mailto:rickard.oberg@telkel.com">Rickard Öberg</a>
 *  @version $Revision: 1.8.4.1 $
 */
public class ThreadPool
{
   // Constants -----------------------------------------------------

   // Attributes ----------------------------------------------------

   /**
    *  Stack of idle threads cached for future use.
    */
   private final Stack pool = new Stack();

   /**
    *  Maximum number of idle threads cached in this pool.
    */
   private int maxSize = 10;


   private boolean enabled = false;

   // Static --------------------------------------------------------

   // Constructors --------------------------------------------------

   /**
    *  Create a new pool.
    */
   public ThreadPool()
   {
   }

   // Public --------------------------------------------------------
   public synchronized void enable()
   {
      enabled = true;
   }

   public synchronized void disable()
   {
      enabled = false;
      while (!pool.isEmpty())
      {
         Worker w = (Worker)pool.pop();
         w.die();
      } // end of while ()
   }



   /**
    *  Set the maximum number of idle threads cached in this pool.
    */
   public void setMaximumSize(int size)
   {
      maxSize = size;
   }

   /**
    *  Do some work.
    *  This will either create a new thread to do the work, or
    *  use an existing idle cached thread.
    */
   public synchronized void run(Runnable work)
   {
      if (pool.size() == 0) {
         new Worker(work);
      } else {
         Worker w = (Worker)pool.pop();
         w.run(work);
      }
   }

   // Private -------------------------------------------------------

   /**
    *  Return an idle worker thread to the pool of cached idle threads.
    *  This is called from the worker thread itself.
    */
   private synchronized void returnWorker(Worker w)
   {
      if (enabled && pool.size() < maxSize)
      {
         pool.push(w);
      }
      else
      {
         w.die();
      } // end of else
   }

   // Inner classes -------------------------------------------------

   class Worker extends Thread
   {
      /**
       *  Flags that this worker may continue to work.
       */
      boolean running = true;

      /**
       *  Work to do, of <code>null</code> if no work to do.
       */
      Runnable work;

      /**
       *  Create a new Worker to do some work.
       */
      Worker(Runnable work)
      {
         this.work = work;
         setDaemon(true);
         start();
      }

      /**
       *  Tell this worker to die.
       */
      public synchronized void die()
      {
         running = false;
         this.notify();
      }

      /**
       *  Give this Worker some work to do.
       *
       *  @throws IllegalStateException If this worker already
       *          has work to do.
       */
      public synchronized void run(Runnable work)
      {
         if (this.work != null)
            throw new IllegalStateException("Worker already has work to do.");
         this.work = work;
         this.notify();
      }

      /**
       *  The worker loop.
       */
      public void run()
      {
         while (running) {
            // If work is available then execute it
            if (work != null) {
               try {
                  work.run();
               } catch (Exception e) {
                  //DEBUG Logger.exception(e);
               }
               // Clear work
               work = null;
            }

            // Return to pool of cached idle threads
            returnWorker(this);

            // Wait for more work to become available
            synchronized (this) {
               while (running && work == null) {
                  try {
                     this.wait();
                  } catch (InterruptedException e) {
                     // Ignore
                  }
               }
            }
         }
      }

   }
}

