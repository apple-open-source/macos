/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.mx.util;

import java.util.Stack;

/**
 * A simple thread pool. Idle threads are cached for future use.
 * The cache grows until it reaches a maximum size (default 10).
 * When there is nothing in the cache, a new thread is created.
 * By default the threads are daemon threads.
 *
 * <a href="mailto:rickard.oberg@telkel.com">Rickard \u00d6berg</a>
 * <a href="mailto:adrian.brock@happeningtimes.com">Adrian Brock</a>
 * @version $Revision: 1.1 $
 */
public class ThreadPool
{
  // Constants -----------------------------------------------------

  // Attributes ----------------------------------------------------

  /**
   * Stack of idle threads cached for future use.
   */
  private Stack pool = new Stack();

  /**
   * Maximum number of idle threads cached in this pool.
   */
  private int maxSize = 10;

  /**
   * Is the thread pool active
   */
  private boolean active = false;

  /**
   * Whether the threads are daemon threads.
   */
  private boolean daemon = true;

  // Static --------------------------------------------------------

  // Constructors --------------------------------------------------

  /**
   * Create a new pool.
   */
  public ThreadPool()
  {
  }

  // Public --------------------------------------------------------

  /**
   * Set the maximum number of idle threads cached in this pool.
   *
   * @param size the new maximum size.
   */
  public void setMaximumSize(int size)
  {
    maxSize = size;
  }

  /**
   * Get the maximum number of idle threads cached in this pool.
   *
   * @returns the maximum size
   */
  public int getMaximumSize()
  {
    return maxSize;
  }

  /**
   * Set the activity status of the pool. Setting the pool to
   * inactive, clears the pool.
   *
   * @param status pass true for active, false otherwise.
   */
  public void setActive(boolean status)
  {
    active = status;
    if (active == false)
      while (pool.size() > 0)
        ((Worker)pool.pop()).die();
  }

  /**
   * Get the activity status of the pool.
   *
   * @returns true for an active pool, false otherwise.
   */
  public boolean isActive()
  {
    return active;
  }

  /**
   * Set whether new threads are daemon threads.
   *
   * @param value pass true for daemon threads, false otherwise.
   */
  public void setDaemonThreads(boolean value)
  {
    daemon = value;
  }

  /**
   * Get whether new threads are daemon threads.
   *
   * @returns true for daemon threads, false otherwise.
   */
  public boolean getDaemonThreads()
  {
    return daemon;
  }

  /**
   * Do some work.
   * This will either create a new thread to do the work, or
   * use an existing idle cached thread.
   *
   * @param work the work to perform.
   */
   public synchronized void run(Runnable work)
   {
     if (pool.size() == 0)
       new Worker(work);
     else 
     {
       Worker worker = (Worker) pool.pop();
       worker.run(work);
     }
   }

   // Private -------------------------------------------------------

   /**
    * Put an idle worker thread back in the pool of cached idle threads.
    * This is called from the worker thread itself. When the cache is
    * full, the thread is discarded.
    *
    * @param worker the worker to return.
    */
   private synchronized void returnWorker(Worker worker)
   {
     if (pool.size() < maxSize)
       pool.push(worker);
     else
       worker.die();   
   }

   // Inner classes -------------------------------------------------

  /**
   * A worker thread runs a worker.
   */
  class Worker extends Thread
  {
    /**
     * Flags that this worker may continue to work.
     */
    boolean running = true;

    /**
     * Work to do, of <code>null</code> if no work to do.
     */
    Runnable work;

    /**
    * Create a new Worker to do some work.
    *
    * @param work the work to perform
    */
    Worker(Runnable work)
    {
      this.work = work;
      setDaemon(daemon);
      start();
    }

    /**
     * Tell this worker to die.
     */
    public synchronized void die()
    {
      running = false;
      this.notify();
    }

    /**
     * Give this Worker some work to do.
     *
     * @param the work to perform.
     * @throws IllegalStateException If this worker already
     *         has work to do.
     */
    public synchronized void run(Runnable work)
    {
      if (this.work != null)
        throw new IllegalStateException("Worker already has work to do.");
      this.work = work;
      this.notify();
    }

    /**
     * The worker loop.
     */
    public void run()
    {
      while (active && running)
      {
        // If work is available then execute it
        if (work != null)
        {
          try
          {
            work.run();
          }
          catch (Exception ignored) {}

          // Clear work
          work = null;
        }
                
        // Return to pool of cached idle threads
        returnWorker(this);

        // Wait for more work to become available
        synchronized (this)
        {
          while (running && work == null)
          {
            try
            {
              this.wait();
            }
            catch (InterruptedException ignored) {}
          }
        }
      }
    }
  }
}
