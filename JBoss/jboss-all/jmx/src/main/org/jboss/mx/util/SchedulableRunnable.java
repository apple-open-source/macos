/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.mx.util;

/**
 * A schedulable runnable.<p>
 *
 * Subclasses should implement doRun() to do some real work.<p>
 *
 * setScheduler(RunnableScheduler) has to be invoked with a RunnableScheduler
 * that has been started for the work to be performed. If the doRun() does
 * not invoke setNextRun(), the link to the scheduler is removed.
 *
 * @see RunnableScheduler
 *
 * @author  <a href="mailto:Adrian.Brock@HappeningTimes.com">Adrian Brock</a>.
 * @version $Revision: 1.2 $
 */
public abstract class SchedulableRunnable
   implements Comparable, Runnable
{
   // Attributes ----------------------------------------------------

   /**
    * A unique identifier
    */
   private long id;

   /**
    * The next run timestamp
    */
   private long nextRun;

   /**
    * The current scheduler
    */
   private RunnableScheduler scheduler;

   /**
    * Whether we are running
    */
   private boolean running;

   /**
    * Whether we should reschedule after the run
    */
   private boolean reschedule;

   // Static --------------------------------------------------------

   /**
    * The next unique identifier
    */
   private static long nextId = 0;

   // Constructors --------------------------------------------------

   /**
    * Constructs a new schedulable runnable.
    */
   public SchedulableRunnable()
   {
      this.id = getNextId();
   }

   // Public --------------------------------------------------------

   /**
    * Gets the next run timestamp
    *
    * @return the next run
    */
   public long getNextRun()
   {
      return nextRun;
   }

   /**
    * Sets the next run date<p>
    *
    * If it is linked to a scheduler, it is temporarily removed while the date
    * is modified.
    *
    * @param nextRun the next run date
    */
   public synchronized void setNextRun(long nextRun)
   {
      // Remove from scheduler
      if (scheduler != null)
         scheduler.remove(this);

      // Set the new run time
      this.nextRun = nextRun;

      // If we are not running, add it to the scheduler otherwise
      // remember we want adding back
      if (running == false && scheduler != null)
         scheduler.add(this);
      else
         reschedule = true;
   }

   /**
    * Set the scheduler for this runnable
    *
    * @param the scheduler pass null to remove the runnable from
    *        any scheduler
    * @return the previous scheduler or null if there was no previous
    *         scheduler
    */
   public synchronized RunnableScheduler setScheduler(RunnableScheduler scheduler)
   {
      // Null operation
      if (this.scheduler == scheduler)
         return this.scheduler;

      // Remember the result
      RunnableScheduler result = this.scheduler;

      // Remove from previous scheduler
      if (this.scheduler != null)
         this.scheduler.remove(this);

      // Set the new state
      this.scheduler = scheduler;

      // This is a remove operation
      if (scheduler == null)
         reschedule = false;

      // If we are not running, add it to the scheduler otherwise
      // remember we want adding
      else if (running == false)
         scheduler.add(this);
      else
         reschedule = true;

      // We are done
      return result;
   }

   /**
    * Do the work, the scheduled runnable should do its work here
    */
   public abstract void doRun();

   // Runnable Implementation ---------------------------------------

   /**
    * Runs doRun()<p>
    *
    * If setNextRun() is not invoked during the doRun(), the link to the 
    * scheduler is removed
    */
   public final void run()
   {
      startRun();
      try
      {
         doRun();
      }
      finally
      {
         endRun();
      }
   }

   // Runnable Implementation ---------------------------------------

   public int compareTo(Object o)
   {
       SchedulableRunnable other = (SchedulableRunnable) o;
       long temp = this.nextRun - other.nextRun;
       if (temp < 0)
          return -1;
       if (temp > 0)
          return +1;
       temp = this.id - other.id;
       if (temp < 0)
          return -1;
       if (temp > 0)
          return +1;
       return 0;
   }

   // Object Overrides ----------------------------------------------

   public boolean equals(Object obj)
   {
      return (compareTo(obj) == 0);
   }

   // Protected -----------------------------------------------------

   // Package -------------------------------------------------------

   // Private -------------------------------------------------------

   /**
    * Start the run
    */
   private synchronized void startRun()
   {
      running = true;
   }

   /**
    * Check whether the work got rescheduled
    */
   private synchronized void endRun()
   {
      running = false;
      if (reschedule == true)
         scheduler.add(this);
      reschedule = false;
   }

   /**
    * Get the next identifier
    */
   private static synchronized long getNextId()
   {
      return nextId++;
   }

   // Inner Classes -------------------------------------------------
}

