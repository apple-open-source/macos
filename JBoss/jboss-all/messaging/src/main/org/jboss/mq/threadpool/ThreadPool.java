/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.mq.threadpool;

import java.lang.Thread;
import java.lang.ThreadGroup;

import java.util.ArrayList;
import java.util.LinkedList;


/**
 *  This is an implementation of a simple thread pool with
 *  an embedded work queue.
 *
 * @author    Ole Husgaard (osh@sparre.dk)
 * @version   $Revision: 1.1 $
 */
public class ThreadPool
{
   /**
    *  The name of this thread pool
    */
   private String name;

   /**
    *  Flags that worker threads should be created as daemon threads.
    */
   private boolean daemon;

   /**
    *  The ThreadGroup of threads in this pool.
    */
   private ThreadGroup threadGroup;

   /**
    *  The worker threads.
    */
   private ArrayList workers;

   /**
    *  Maximum number of worker threads.
    */
   private int maxWorkers;

   /**
    *  Count of idle worker threads.
    *  Synchronized on the [@link #workers} field.
    */
   private int idleWorkers;

   /**
    *  Flags that we are shutting down the pool.
    *  Synchronized on the [@link #workers} field.
    */
   private volatile boolean stopping;

   /**
    *  The work queue.
    */
   private LinkedList queue;


   /**
    *  Create a new thread pool instance.
    *
    *  @param Name The name of this thread pool. This is used for naming its
    *         worker threads.
    *  @param threadGroup The <code>ThreadGroup</code> that worker threads
    *         in this pool should belong to.
    *  @param maxWorkers The maximum number of worker threads in this pool.
    *  @param daemon If <code>true</code>, worker threads will be created as
    *         daemon threads.
    */
   public ThreadPool(String name, ThreadGroup threadGroup, int maxWorkers,
                     boolean daemon)
   {
      if (name == null || threadGroup == null || maxWorkers <= 0)
         throw new IllegalArgumentException();

      this.name = name;
      this.daemon = daemon;
      this.threadGroup = threadGroup;
      workers = new ArrayList();
      this.maxWorkers = maxWorkers;
      idleWorkers = 0;
      stopping = false;
      queue = new LinkedList();
   }

   /**
    *  Shutdown this thread pool.
    *  This will not return until all enqueued work has been cancelled,
    *  and all worker threads have done any work they started and have
    *  died.
    */
    public void shutdown()
    {
       stopping = true;

       synchronized (queue) {
          // Remove all queued work
          queue.clear();
          // Notify all waiting threads
          queue.notifyAll();
       }

       // wait for all worker threads to die.
       synchronized (workers) {
          while (workers.size() > 0) {
             try {
                // wait for some worker threads to die.
                workers.wait();
             } catch (InterruptedException ex) {
                // ignore
             }
          }
       }
    }


   /**
    *  Enqueue a piece of work for this thread to handle.
    *  As soon as a thread becomes available, it will call
    *  {@link Work#doWork} of the argument.
    *  If the pool is shutting down, this method will not enqueue the
    *  work, but instead simply return.
    *
    *  @param work The piece of work to be enqueued.
    */
   public void enqueueWork(Work work)
   {
//System.err.println("ThreadPool("+name+"): enqueueWork() entered.");
      // We may want to start a worker thread
      synchronized (workers) {
//System.err.println("ThreadPool("+name+"): enqueueWork(): idleWorkers="+idleWorkers+" stopping="+stopping+".");
//System.err.println("ThreadPool("+name+"): enqueueWork(): workers.size()="+workers.size()+" maxWorkers="+maxWorkers+".");
         if (idleWorkers == 0 && !stopping && workers.size() < maxWorkers)
{
            new WorkerThread(name + "-" + (workers.size() + 1)).start();
//System.err.println("ThreadPool("+name+"): started new WorkerThread.");
}
      }

      synchronized (queue) {
         if (stopping)
            return; // we are shutting down, cannot take new work.

         queue.addLast(work);
//System.err.println("ThreadPool("+name+"): enqueueWork(): enqueued work..");
         queue.notify();
      }
   }

   /**
    *  Cancel a piece of enqueued work.
    *
    *  @param work The piece of work to be cancel.
    */
   public void cancelWork(Work work)
   {
      synchronized (queue) {
         // It may be enqueued several times.
         while (queue.remove(work))
            ;
      }
   }


   /**
    *  The threads that do the actual work.
    */
   private class WorkerThread
      extends Thread
   {
      /**
       *  Create a new WorkerThread.
       *  This must be called when holding the workers monitor.
       */
      WorkerThread(String name)
      {
         super(threadGroup, name);
         setDaemon(daemon);
         workers.add(this);
//System.err.println("ThreadPool("+name+"): " + getName() + " created.");
      }

      /**
       *  Wait for work do to.
       *  This must be called when holding the queue monitor.
       *  This will temporarily increment the count of idle workers
       *  while waiting.
       */
      private void idle()
      {
         try {
            synchronized (workers) {
              ++idleWorkers;
            }
            //System.err.println("ThreadPool("+name+"): " + getName() + " starting to wait.");
            queue.wait();
         } catch (InterruptedException ex) {
            // ignore
         } finally {
            //System.err.println("ThreadPool("+name+"): " + getName() + " done waiting.");
            synchronized (workers) {
              --idleWorkers;
            }
         }
      }

      public void run()
      {
//System.err.println("ThreadPool("+name+"): " + getName() + " started to run.");
         while (!stopping) {
            Work work = null;

            synchronized (queue) {
               if (queue.size() == 0)
                  idle();
               if (!stopping && queue.size() > 0)
                  work = (Work)queue.removeFirst();
            }

            if (work != null)
               work.doWork();
         }
         synchronized (workers) {
            workers.remove(this);
            // Notify the shutdown thread.
            workers.notify();
         }
      }
   }
}
