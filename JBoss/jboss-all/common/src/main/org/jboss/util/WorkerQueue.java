/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.util;

/**
 * Class that queues {@link Executable} jobs that are executed sequentially 
 * by a single thread.
 *
 * @see Executable
 * 
 * @author <a href="mailto:simone.bordet@compaq.com">Simone Bordet</a>
 * @version $Revision: 1.1 $
 */
public class WorkerQueue
{
   /** The thread that runs the Executable jobs */
   protected Thread m_queueThread;
   
   /** The job that will be executed by the worker thread */
   private JobItem m_currentJob;

   /**
    * Creates a new worker queue with default thread name of "Worker Thread"
    */
   public WorkerQueue() 
   {
      this("Worker Thread");
   }

   /**
    * Creates a new worker queue with the specified thread name
    */
   public WorkerQueue(String threadName) 
   {
      m_queueThread = new Thread(createQueueLoop(), threadName);
   }
   
   /**
    * Creates a new worker queue with the specified thread name
    * and daemon mode flag
    */
   public WorkerQueue(String threadName, boolean isDaemon) 
   {
      m_queueThread = new Thread(createQueueLoop(), threadName);
      m_queueThread.setDaemon(isDaemon);
   }

   /**
    * Starts the worker queue.
    * @see #stop
    */
   public void start() 
   {
      if (m_queueThread != null) {m_queueThread.start();}
   }

   /**
    * Stops nicely the worker queue. <br> 
    * After this call trying to put a new job will result in a 
    * InterruptedException to be thrown. The jobs queued before and not 
    * yet processed are processed until the queue is empty, then this 
    * worker queue is cleared.
    * @see #clear
    * @see #start
    * @see #isInterrupted
    */
   public synchronized void stop() 
   {
      if (m_queueThread != null) {m_queueThread.interrupt();}
   }
   
   /**
    * Called by a thread that is not the WorkerQueue thread, this method 
    * queues the job and, if necessary, wakes up this worker queue that is 
    * waiting in {@link #getJob}.
    */
   public synchronized void putJob(Executable job)
   {
      // Preconditions
      if (m_queueThread == null || !m_queueThread.isAlive()) {
         throw new IllegalStateException("Can't put job, thread is not alive or not present");
      }
      
      if (isInterrupted()) {
         throw new IllegalStateException("Can't put job, thread was interrupted");
      }
        
      putJobImpl(job);
   }
   
   /**
    * Returns whether the worker thread has been interrupted. <br>
    * When this method returns true, it is not possible to put new jobs in the
    * queue and the already present jobs are executed and removed from the 
    * queue, then the thread exits.
    * 
    * @see #stop
    */
   protected boolean isInterrupted() 
   {
      return m_queueThread.isInterrupted();
   }

   /**
    * Called by this class, this method checks if the queue is empty; 
    * if it is, then waits, else returns the current job.
    * 
    * @see #putJob
    */
   protected synchronized Executable getJob() throws InterruptedException
   {
      // Preconditions
      if (m_queueThread == null || !m_queueThread.isAlive()) {
         throw new IllegalStateException();
      }
        
      return getJobImpl();
   }
   
   /**
    * Never call this method, only override in subclasses to perform
    * job getting in a specific way, normally tied to the data structure 
    * holding the jobs.
    */
   protected Executable getJobImpl() throws InterruptedException
   {
      // While the queue is empty, wait();
      // when notified take an event from the queue and return it.
      while (m_currentJob == null) {wait();}
      // This one is the job to return
      JobItem item = m_currentJob;
      // Go on to the next object for the next call. 
      m_currentJob = m_currentJob.m_next;
      return item.m_job;
   }
   
   /**
    * Never call this method, only override in subclasses to perform
    * job adding in a specific way, normally tied to the data structure
    * holding the jobs.
    */
   protected void putJobImpl(Executable job) 
   {
      JobItem posted = new JobItem(job);      
      if (m_currentJob == null) 
      {
         // The queue is empty, set the current job to process and
         // wake up the thread waiting in method getJob
         m_currentJob = posted;
         notifyAll();
      }
      else 
      {
         JobItem item = m_currentJob;
         // The queue is not empty, find the end of the queue ad add the
         // posted job at the end
         while (item.m_next != null) {item = item.m_next;}
         item.m_next = posted;           
      }
   }

   /**
    * Clears the running thread after the queue has been stopped. <br> 
    * After this call, this worker queue is unusable and can be garbaged.
    */
   protected void clear() 
   {
      m_queueThread = null;
      m_currentJob = null;
   }
   
   /**
    * Creates the loop that will get the next job and process it. <br>
    * Override in subclasses to create a custom loop.
    */
   protected Runnable createQueueLoop() {
      return new QueueLoop();
   }
   
   /**
    * Class that loops getting the next job to be executed and then 
    * executing it, in the worker thread.
    */
   protected class QueueLoop
      implements Runnable 
   {
      public void run() 
      {
         try
         {
            while (true) 
            {
               try 
               {
                  if (isInterrupted()) 
                  {
                     flush();
                     break;
                  }
                  else 
                  {
                     getJob().execute();
                  }
               }
               catch (InterruptedException e) 
               {
                  try {
                     flush();
                  }
                  catch (Exception ignored) {}
                  break;
               }
               catch (Exception e) {
                  ThrowableHandler.add(ThrowableHandler.Type.ERROR, e);
               }
            }
         }
         finally {
            clear();
         }
      }
      
      protected void flush() throws Exception
      {
         // Empty the queue of the posted jobs and exit
         while (m_currentJob != null) 
         {
            m_currentJob.m_job.execute();
            m_currentJob = m_currentJob.m_next;
         }
      }
   }
   
   /**
    * Simple linked cell, that has only a reference to the next job.
    */
   private class JobItem 
   {
      private Executable m_job;
      private JobItem m_next;
      private JobItem(Executable job) {m_job = job;}
   }
}
