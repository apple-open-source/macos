/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.util;

/**
 * This class runs in a single thread {@link TimerTask}s that have been 
 * scheduled. <p>
 * A similar class is present in java.util package of jdk version >= 1.3;
 * for compatibility with jdk 1.2 it is reimplemented here.
 *
 * @see TimerTask
 * @author <a href="mailto:simone.bordet@compaq.com">Simone Bordet</a>
 * @version $Revision: 1.2 $
 */
public class TimerQueue 
   extends WorkerQueue
{
   // Constants -----------------------------------------------------

   // Attributes ----------------------------------------------------
   /* This member is accessed only from putJob and getJob, that will 
      then guarantee necessary synchronization on it */
   private Heap m_heap;

   // Static --------------------------------------------------------

   // Constructors --------------------------------------------------
   /**
    * Creates a new timer queue with default thread name of "TimerTask Thread"
    */
   public TimerQueue() 
   {
      this("TimerTask Thread");
   }
   /**
    * Creates a new timer queue with the specified thread name
    */
   public TimerQueue(String threadName) 
   {
      super(threadName);
      m_heap = new Heap();
   }

   // Public --------------------------------------------------------
   /**
    * Schedules the given TimerTask for immediate execution.
    */
   public void schedule(TimerTask t) 
   {
      schedule(t, 0);
   }
   /**
    * Schedule the given TimerTask to be executed after <code>delay</code>
    * milliseconds.
    */
   public void schedule(TimerTask t, long delay) 
   {
      if (t == null) throw new IllegalArgumentException("Can't schedule a null TimerTask");
      if (delay < 0) delay = 0;
      t.setNextExecutionTime(System.currentTimeMillis() + delay);
      putJob(t);
   }

   // Z implementation ----------------------------------------------

   // WorkerQueue overrides ---------------------------------------------------
   protected void putJobImpl(Executable task) 
   {
      m_heap.insert(task);
      ((TimerTask)task).setState(TimerTask.SCHEDULED);
      notifyAll();
   }
   protected Executable getJobImpl() throws InterruptedException
   {
      while (m_heap.peek() == null) 
      {
         wait();
      }
      TimerTask task = (TimerTask)m_heap.extract();
      switch (task.getState())
      {
       case TimerTask.CANCELLED:
       case TimerTask.EXECUTED:
          //don't hold onto last dead task if we wait.
          task = null;
          return getJobImpl();
       case TimerTask.NEW:
       case TimerTask.SCHEDULED:
          return task;
       default:
          throw new IllegalStateException("TimerTask has an illegal state");
      }
   }
   protected Runnable createQueueLoop() 
   {
      return new TimerTaskLoop();
   }
   protected void clear() 
   {
      super.clear();
      synchronized (this) 
      {
         m_heap.clear();
      }
   }

   // Package protected ---------------------------------------------

   // Protected -----------------------------------------------------

   // Private -------------------------------------------------------

   // Inner classes -------------------------------------------------
   /**
    * Class that loops getting the next job to be executed and then 
    * executing it, in the timer task thread.
    */
   protected class TimerTaskLoop implements Runnable 
   {
      public void run() 
      {
         try
         {
            while (true) 
            {
               try 
               {
                  // Wait for the first job
                  TimerTask task = (TimerTask)getJob();
                  long now = System.currentTimeMillis();
                  long executionTime = task.getNextExecutionTime();
                  long timeToWait = executionTime - now;
                  boolean runTask = timeToWait <= 0;
                  if (!runTask)
                  {
                     // Entering here when a new job is scheduled but
                     // it's not yet time to run the first one;
                     // the job was extracted from the heap, reschedule
                     putJob(task);
                     Object mutex = TimerQueue.this;
                     synchronized (mutex)
                     {
                        // timeToWait is always strictly > 0, so I don't wait forever
                        mutex.wait(timeToWait);
                     }
                  }
                  else
                  {
                     if (task.isPeriodic())
                     {
                        // Reschedule with the new time
                        task.setNextExecutionTime(executionTime + task.getPeriod());
                        putJob(task);
                     }
                     else
                     {
                        // The one-shot task is already removed from 
                        // the heap, mark it as executed
                        task.setState(TimerTask.EXECUTED);
                     }
                     // Run it !
                     task.execute();
                  }
               }
               catch (InterruptedException x)
               {
                  // Exit the thread
                  break;
               }
               catch (Exception e)
               {
                  ThrowableHandler.add(ThrowableHandler.Type.ERROR, e);
               }
            }
         }
         finally	{clear();}
      }
   }
}
