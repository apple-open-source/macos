/*
 * JBoss, the OpenSource J2EE WebOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.mx.loading;

import java.util.Collections;
import java.util.HashMap;
import java.util.LinkedList;
import java.util.Map;
import java.util.WeakHashMap;
import java.util.List;

import org.jboss.logging.Logger;
import org.jboss.mx.loading.ClassLoadingTask.ThreadTask;


/** A utility class used by the UnifiedClassLoader3 to manage the thread based
 * class loading tasks.
 *
 * @author Scott.Stark@jboss.org
 * @version $Revision: 1.1.2.1 $
 */
public class LoadMgr4
{
   private static Logger log = Logger.getLogger(LoadMgr4.class);
   /** Used as a synchronization monitor during the setup/teardown of the
      thread owning a UCL.loadClass lock
    */
   private static Object registrationLock = new Object();

   /** A Map<UnifiedClassLoader4, Thread> of the active loadClass UCL4/threads.
    * This must be accessed under the registrationLock monitor.
    */
   private static HashMap loadClassThreads = new HashMap();
   /** A Map<Thread, LinkedList<ThreadTask> > of the class loading tasks
    * associated with a thread
    */
   private static Map loadTasksByThread = Collections.synchronizedMap(new WeakHashMap());

   /** A UCL and its relative ordering with respect to the class loading.
    * The UCL with the lowest order to load a class is the UCL that will
    * populate the repository cache and be assigned as the UCL.loadClass
    * return value.
    */
   public static class PkgClassLoader
   {
      public final UnifiedClassLoader4 ucl;
      public final int order;

      public PkgClassLoader(UnifiedClassLoader4 ucl)
      {
         this(ucl, Integer.MAX_VALUE);
      }
      public PkgClassLoader(UnifiedClassLoader4 ucl, int order)
      {
         this.ucl = ucl;
         this.order = order;
      }
   }

   /** Register that a thread owns the UCL4.loadClass monitor. This is called
    * from within UCL4.loadClass(String,boolean) and this method creates
    * entries in the loadClassThreads and loadTasksByThread maps.
    */
   public static void registerLoaderThread(UnifiedClassLoader4 ucl, Thread t)
   {
      synchronized( registrationLock )
      {
         Object prevThread = loadClassThreads.put(ucl, t);
         if( log.isTraceEnabled() )
            log.trace("registerLoaderThread, ucl="+ucl+", t="+t+", prevT="+prevThread);

         synchronized( loadTasksByThread )
         {
            List taskList = (List) loadTasksByThread.get(t);
            if( taskList == null )
            {
               taskList = Collections.synchronizedList(new LinkedList());
               loadTasksByThread.put(t, taskList);
               if( log.isTraceEnabled() )
                  log.trace("created new task list");
            }
         }
         registrationLock.notifyAll();
      }
   }

   /** Initiate the class loading task. This is called by UCL4.loadClass to
    * initiate the process of loading the requested class. This first attempts
    * to load the class from the repository cache, and then the class loaders
    * in the repsository. If the package of the class is found in the repository
    * then one or more ThreadTask are created to complete the ClassLoadingTask.
    * The ThreadTask are assigned to the threads that own the associated UCL4
    * monitor. If no class loader serves the class package, then the requesting
    * class loader is asked if it can load the class.
    *
    * @return true if the class could be loaded from the cache or requesting
    * UCL4, false to indicate the calling thread must process the
    * tasks assigned to it until the ClassLoadingTask state is FINISHED
    * @exception ClassNotFoundException if there is no chance the class can
    * be loaded from the current repository class loaders.
    */
   public static boolean beginLoadTask(ClassLoadingTask task,
      UnifiedLoaderRepository4 repository)
      throws ClassNotFoundException
   {
      boolean trace = log.isTraceEnabled();
      if( trace )
         log.trace("Begin beginLoadTask, task="+task);

      // Try the cache before anything else.
      Class cls = repository.loadClassFromCache(task.classname);
      if( cls != null )
      {
         task.loadedClass = cls;
         task.state = ClassLoadingTask.FINISHED;
         if( trace )
            log.trace("End beginLoadTask, loadClassFromCache");
         return true;
      }

      // Next get the class loaders for this class
      UnifiedClassLoader4 ucl = repository.getClassLoader(task.classname);
      if( ucl == null)
      {
         /* If there are no class loaders in the repository capable of handling
         the request ask the class loader itself in the event that its parent(s)
         can load the class.
         */
         try
         {
            cls = repository.loadClassFromClassLoader(task.classname, false,
               task.requestingClassLoader);
         }
         catch(LinkageError e)
         {
            if( trace )
               log.trace("End beginLoadTask, LinkageError for task: "+task, e);
            throw e;
         }
         if( cls != null )
         {
            task.loadedClass = cls;
            task.state = ClassLoadingTask.FINISHED;
            if( trace )
               log.trace("End beginLoadTask, loadClassFromClassLoader");
            return true;
         }

         // Else, fail the load
         if( trace )
            log.trace("End beginLoadTask, ClassNotFoundException");
         String msg = "No ClassLoaders found for: "+task.classname;
         throw new ClassNotFoundException(msg);
      }

      /* A class loading task for each ClassLoader is needed. There can be
         multiple class loaders for a pkg due to the pkg being spread out over
         multiple jars, or duplicate classes due to versioning/patches, or
         just bad packaging.

         In the case of a non-scoped deployment of multiple classes which
         will provide a PkgClassLoader to define the ordering, we simply
         choose an ordering based on the order the UCL4s were added to the
         repository. At most one of the candidate UCL4s will load the class
         in order to avoid ClassCastExceptions or LinkageErrors due to the
         strong Java type system/security model.

         TODO: A simple ordering mechanism exists, but this probably needs
         to be augmented.
       */
      scheduleTask(task, ucl, 0, false, trace);
      task.state = ClassLoadingTask.FOUND_CLASS_LOADER;
      if( trace )
         log.trace("End beginLoadTask, task="+task);

      return false;
   }

   /** Called by threads owning a UCL4.loadLock from within UCL4.loadClass to
    * process ThreadTasks assigned to them. This is the mechanism by which we
    * avoid deadlock due to a given loadClass request requiring multiple UCLs
    * to be involved. Any thread active in loadClass with the monitor held
    * processes class loading tasks that must be handled by its UCL4. The
    * active set of threads loading classes form a pool of cooperating threads.
    */
   public static void nextTask(Thread t, ClassLoadingTask task,
      UnifiedLoaderRepository4 repository)
      throws InterruptedException
   {
      boolean trace = log.isTraceEnabled();
      List taskList = (List) loadTasksByThread.get(t);
      synchronized( task )
      {
         // There may not be any ThreadTasks
         while( taskList.size() == 0 && task.threadTaskCount != 0 )
         {
            /* There are no more tasks for the calling thread to execute, so the
            calling thread must wait until the task.threadTaskCount reaches 0
             */
            if( trace )
               log.trace("Begin nextTask(WAIT_ON_EVENT), task="+task);
            try
            {
               task.state = ClassLoadingTask.WAIT_ON_EVENT;
               /* TODO: We are waiting with the UCL monitor held here. Is there
               a deadlock scenario that can occur due to the class we are
               waiting on needing a class from the UCL?
               */
               task.wait();
            }
            catch(InterruptedException e)
            {
               if( trace )
                  log.trace("nextTask(WAIT_ON_EVENT), interrupted, task="+task, e);
               // Abort this task attempt
               throw e;
            }
         }

         if( trace )
            log.trace("Continue nextTask("+taskList.size()+"), task="+task);

         // See if the task is complete
         if( task.threadTaskCount == 0 )
         {
            task.state = ClassLoadingTask.FINISHED;
            log.trace("End nextTask(FINISHED), task="+task);
            return;
         }
      }

      ThreadTask threadTask = (ThreadTask) taskList.remove(0);
      ClassLoadingTask loadTask = threadTask.getLoadTask();
      if( trace )
         log.trace("Begin nextTask("+taskList.size()+"), loadTask="+loadTask);

      UnifiedClassLoader4 ucl4 = (UnifiedClassLoader4) threadTask.ucl;
      try
      {
         if( threadTask.t == null )
         {
            /* This is a task that has been reassigned back to the original
            requesting thread ClassLoadingTask, so a new ThreadTask must
            be scheduled.
            */
            if( trace )
               log.trace("Rescheduling threadTask="+threadTask);
            scheduleTask(loadTask, ucl4, threadTask.order, true, trace);
         }
         else
         {
            if( trace )
               log.trace("Running threadTask="+threadTask);
            // Load the class using this thread
            threadTask.run();
         }
      }
      catch(Throwable e)
      {
         loadTask.loadException = e;
         if( trace )
            log.trace("Run failed with exception", e);
      }
      finally
      {
         // We must release the loadLock acquired in beginLoadTask
         if( threadTask.releaseInNextTask == true )
         {
            if( trace )
               log.trace("Releasing loadLock and ownership of UCL: "+threadTask.ucl);
            synchronized( registrationLock )
            {
               loadClassThreads.remove(threadTask.ucl);
            }
            synchronized( threadTask.ucl )
            {
               ucl4.release();
               ucl4.notifyAll();
            }
         }
      }

      // If the ThreadTasks are complete mark the ClassLoadingTask finished
      if( loadTask.threadTaskCount == 0 )
      {
         Class loadedClass = threadTask.getLoadedClass();
         if( loadedClass != null )
         {
            ClassLoader loader = loadedClass.getClassLoader();
            // Place the loaded class into the repositry cache
            repository.cacheLoadedClass(threadTask.getClassname(),
               loadedClass, loader);
         }
         synchronized( loadTask )
         {
            loadTask.state = ClassLoadingTask.FINISHED;
            loadTask.notify();
         }
      }
      if( trace )
         log.trace("End nextTask("+taskList.size()+"), loadTask="+loadTask);
   }

   /** Complete a ClassLoadingTask. This is called by UCL3.loadClass to indicate
    * that the thread is existing the loadClass method.
    */
   public static void endLoadTask(ClassLoadingTask task)
   {
      boolean trace = log.isTraceEnabled();
      if( trace )
         log.trace("Begin endLoadTask, task="+task);

      // Unregister as the owning thread and notify any waiting threads
      synchronized( registrationLock )
      {
         loadClassThreads.remove(task.requestingClassLoader);
         registrationLock.notifyAll();
      }

      // Any ThreadTasks associated with this thread must be reassigned
      List taskList = (List) loadTasksByThread.get(task.requestingThread);
      int size = taskList != null ? taskList.size() : 0;
      synchronized( taskList )
      {
         for(int i = 0; i < size; i ++)
         {
            ThreadTask threadTask = (ThreadTask) taskList.remove(0);
            ClassLoadingTask loadTask = threadTask.getLoadTask();
            /* Synchronize on loadTask and reassign the thread task back to the
            requesting thread of loadTask. We need to synchronize on loadTask
            to ensure that the transfer of this task back to loadTask.requestingThread
            is atomic wrt loadTask.requestingThread checking its task list.
            */
            synchronized( loadTask )
            {
               if( trace )
                  log.trace("Reassigning task: "+threadTask+", to: "+loadTask.requestingThread);
               threadTask.t = null;
               // Insert the task into the front of requestingThread task list
               List toTaskList = (List) loadTasksByThread.get(loadTask.requestingThread);
               toTaskList.add(0, threadTask);
               loadTask.state = ClassLoadingTask.NEXT_EVENT;
               loadTask.notify();
            }
         }
      }
   }

   /** Invoked to create a ThreadTask to assign a thread to the task of
    * loading the class of ClassLoadingTask.
    *
    * @param task the orginating UCL3.loadClass task for which the thread
    * @param ucl the UCL3 the ThreadTask will call loadClassLocally on
    * @param order the heirachical ordering of the task
    * @param reschedule a boolean indicating if this task is being rescheduled
    *    with another UCL3
    * @param trace the Logger trace level flag
    * @throws ClassNotFoundException
    */
   static private void scheduleTask(ClassLoadingTask task, UnifiedClassLoader4 ucl,
      int order, boolean reschedule, boolean trace) throws ClassNotFoundException
   {
      Thread t = null;
      boolean releaseInNextTask = false;
      ThreadTask subtask = null;
      List taskList = null;
      synchronized( registrationLock )
      {
         // Find the thread that owns the ucl
         t = (Thread) loadClassThreads.get(ucl);
         if( t == null )
         {
            /* There is no thread in the UCL.loadClass yet that has registered
               as the owning thread. We must attempt to acquire the loadLock
               and if we cannot, wait until the thread entering UCL.loadClass
               gets to the registerLoaderThread call. By the time we are
               notified, the thread coule in fact have exited loadClass, so
               we either assign the task to the thread, or take ownership of
               the UCL.
             */
            while( t == null && ucl.attempt(1) == false )
            {
               if( trace )
                  log.trace("Waiting for owner of UCL: "+ucl);
               try
               {
                  registrationLock.wait();
               }
               catch(InterruptedException e)
               {
                  String msg = "Interrupted waiting for registration notify,"
                     + " classame: "+task.classname;
                  throw new ClassNotFoundException(msg);
               }

               t = (Thread) loadClassThreads.get(ucl);
               if( trace )
                  log.trace("Notified that UCL owner is: "+t);
            }

            // Get the thread registered as owning the UCL.loadClass lock
            t = (Thread) loadClassThreads.get(ucl);
            if( t == null )
            {
               // There is no such thread, register as the owner
               releaseInNextTask = true;
               t = task.requestingThread;
               Object prevThread = loadClassThreads.put(ucl, t);
               if( trace )
               {
                  log.trace("scheduleTask, taking ownership of ucl="+ucl
                     +", t="+t+", prevT="+prevThread);
               }
            }
         }

         // Now that we have the UCL owner thread, create and assign the task
         subtask = task.newThreadTask(ucl, t, order, reschedule,
            releaseInNextTask);
         // Add the task to the owning thread
         taskList = (List) loadTasksByThread.get(t);
         synchronized( taskList )
         {
            taskList.add(subtask);
         }
      }

      if( trace )
         log.trace("scheduleTask("+taskList.size()+"), created subtask: "+subtask);
   }
}
