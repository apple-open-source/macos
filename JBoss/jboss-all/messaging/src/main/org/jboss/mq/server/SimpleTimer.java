/*
 * JBossMQ, the OpenSource JMS implementation
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.mq.server;

import EDU.oswego.cs.dl.util.concurrent.Heap;

/**
 * Much like the <code>java.util.Timer</code> class, except a separate
 * thread is launched only when a new task appears.
 * Another useful feature is that all pending tasks may be canceled without
 * invalidating the whole timer.
 */
final class SimpleTimer
   extends Thread
{

   // Count of created instances, for setting default thread name
   private static int creationCount = 0;

   // Basic heap
   private Heap heap = new Heap(16);
   // Flag indicating timer is canceled
   private boolean canceled = false;

   /**
    * Constructs a new SimpleTimer with a thread default name and
    * by default as a daemon.
    */
   public SimpleTimer() {
      setName("SimpleTimer-" + creationCount++);
      setDaemon(true);
   }

   /**
    * Returns the number of tasks.
    */
   public int size()
   {
      return heap.size();
   }

   /**
    * Clears all currently scheduled tasks.
    */
   public void clear()
   {
      heap.clear();
   }

   /**
    * Terminates all timer tasks, ends this thread.
    */
   public synchronized void cancel()
   {
      clear();
      canceled = true;
      interrupt();
   }

   private SimpleTimerTask peekNextTask()
   {
      return (SimpleTimerTask)heap.peek();
   }

   /**
    * Schedules a task on the specified heap, at a specific time.
    * The heap must have been previously registered using the method {@link #add}.
    */
   public void schedule(SimpleTimerTask task, long at)
   {
      task.scheduled = at;
      synchronized (this)
      {
         if (!isAlive())
            start();
         heap.insert(task);
         if (task == peekNextTask())
            notify();
      }
   }

   public void run()
   {
      while (true)
      {
         try
         {
            SimpleTimerTask task;
            boolean execute = false;
            synchronized (this)
            {
               // No tasks, so wait for one...
               while ((task = peekNextTask()) == null && !canceled)
                  wait();
               if (canceled)
                  break;

               // Check if we're ready for this task
               long now = System.currentTimeMillis();
               long duration = task.scheduled - now;
               if (duration > 0)
               {
                  wait(duration);
               }
               else
               { 
                  execute = true;
                  heap.extract();
               }
            }
            // Do not lock when running
            if (execute)
               task.run();
         } 
         catch (InterruptedException e) {}
      }
   }

   /**
    * Calls <code>cancel</code> on this object.
    */
   protected void finalize() {
      cancel();
   }

}
