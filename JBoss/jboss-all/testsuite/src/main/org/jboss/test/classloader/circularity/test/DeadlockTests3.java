package org.jboss.test.classloader.circularity.test;

import java.net.URL;

import org.apache.log4j.Logger;

import org.jboss.mx.loading.UnifiedClassLoader;
import org.jboss.mx.loading.UnifiedClassLoader3;
import org.jboss.mx.loading.UnifiedLoaderRepository3;
import org.jboss.mx.loading.ClassLoadingTask;
import org.jboss.mx.loading.LoadMgr3;
import EDU.oswego.cs.dl.util.concurrent.CyclicBarrier;

/** Deadlock tests of the UnifiedClassLoader3
 * @author Scott.Stark@jboss.org
 * @version $Revision: 1.1.2.3 $
 */
public class DeadlockTests3
{
   private static Logger log = Logger.getLogger(DeadlockTests3.class);
   private CyclicBarrier t0Barrier = new CyclicBarrier(2);
   private CyclicBarrier t1Barrier = new CyclicBarrier(2);

   public DeadlockTests3()
   {
   }

   /** The scenario is:
    - Thread1 starts to load Base via UCL1 which has dl11.jar(Base) and waits at
    t0Barrier after it has registered as the thread owning UCL1.

    - Thread0 loads Dervied via UCL0 which has dl10.jar(pkg0.Derived). This
    will recursively call UCL0.loadClass to load Base from UCL1 with the
    LoadMgr.class monitor held. Thread1 will not be able to load Base because
    Thread0 goes to sleep waiting for a ClassLoadingTask.WAIT_ON_EVENT event
    that cannot be issued since Thread1 cannot enter LoadMgr.begingLoadTask
    or LoadMgr.nextTask.

    @throws Exception
    */
   public void testDeadlockCase1() throws Exception
   {
      log.info("Begin testDeadlockCase1");
      UnifiedLoaderRepository3 repository = new UnifiedLoaderRepository3();
      Class thisClass = getClass();
      UnifiedClassLoader thisUCL = (UnifiedClassLoader) thisClass.getClassLoader();
      URL origURL = thisUCL.getOrigURL();
      log.info("Service origURL="+origURL);
      URL j0 = new URL(origURL, "dl10.jar");
      log.info("j0 = "+j0);
      URL j1 = new URL(origURL, "dl11.jar");
      log.info("j1 = "+j1);

      UnifiedClassLoader3 ucl0 = new UnifiedClassLoader3(j0);
      repository.addClassLoader(ucl0);
      UCL1 ucl1 = new UCL1(j1, t0Barrier, t1Barrier);
      repository.addClassLoader(ucl1);

      T1 t1 = new T1(ucl1);
      t1.start();
      t0Barrier.barrier();
      log.info("Passed t0Barrier");

      T0 t0 = new T0(ucl0);
      t0.start();
      t1Barrier.barrier();
      log.info("Passed t1Barrier");

      t1.join(5000);
      if( t1.loadedClass == null )
         throw new Exception("Thread1 failed to load Base");
      t0.join(5000);
      if( t0.loadedClass == null )
         throw new Exception("Thread0 failed to load Derived");
      log.info("End testDeadlockCase1");
   }

   /** Load org.jboss.test.classloader.circularity.support.pkg0.Derived via
    * UCL0
    */
   static class T0 extends Thread
   {
      Class loadedClass;
      Throwable loadEx;
      UnifiedClassLoader3 ucl0;
      T0(UnifiedClassLoader3 ucl0)
      {
         super("Thread0:UCL0");
         this.ucl0 = ucl0;
      }
      public void run()
      {
         try
         {
            loadedClass = ucl0.loadClass("org.jboss.test.classloader.circularity.support.pkg0.Derived");
         }
         catch(Throwable t)
         {
            loadEx = t;
            log.error("T0 failed to load Derived", t);
         }
      }
   }
   /** Load org.jboss.test.classloader.circularity.support.Base via UCL1
    */
   static class T1 extends Thread
   {
      Class loadedClass;
      Throwable loadEx;
      UCL1 ucl1;

      T1(UCL1 ucl1)
      {
         super("Thread1:UCL1");
         this.ucl1 = ucl1;
      }
      public void run()
      {
         try
         {
            loadedClass = ucl1.loadClass("org.jboss.test.classloader.circularity.support.Base");
         }
         catch(Throwable t)
         {
            loadEx = t;
            log.error("T1 failed to load Base", t);
         }
      }
   }

   static class MyClassLoadingTask extends ClassLoadingTask
   {
      MyClassLoadingTask(String classname, UnifiedClassLoader3 requestingClassLoader,
         Thread requestingThread)
      {
         super(classname, requestingClassLoader, requestingThread);
      }
      int threadTaskCount()
      {
         return threadTaskCount;
      }
      int state()
      {
         return state;
      }
      Class loadedClass()
      {
         return loadedClass;
      }
      Throwable loadException()
      {
         return loadException;
      }
   }

   public static class UCL1 extends UnifiedClassLoader3
   {
      private static final Logger log = Logger.getLogger(UCL1.class);
      CyclicBarrier t0Barrier;
      CyclicBarrier t1Barrier;
      boolean passedBarriers;

      public UCL1(URL url, CyclicBarrier t0Barrier, CyclicBarrier t1Barrier)
      {
         super(url);
         this.t0Barrier = t0Barrier;
         this.t1Barrier = t1Barrier;
      }

      /** Override to
       */
      public synchronized Class loadClass(String name, boolean resolve)
         throws ClassNotFoundException
      {
         log.info("loadClass, name="+name);
         boolean acquired = attempt(1);
         if( acquired == false )
            throw new IllegalStateException("Failed to acquire loadClass lock");
         log.info("Acquired loadClass lock");

         MyClassLoadingTask task = null;
         try
         {
            Thread t = Thread.currentThread();
            // Register this thread as owning this UCL
            if( loadLock.holds() == 1 )
               LoadMgr3.registerLoaderThread(this, t);

            // Wait with the loadClass lock held
            try
            {
               if( passedBarriers == false )
                  t0Barrier.barrier();
            }
            catch(InterruptedException e)
            {
               throw new IllegalStateException("UCL1 failed to enter t0Barrier");
            }
            log.info("Passed t0Barrier");
            try
            {
               if( passedBarriers == false )
                  t1Barrier.barrier();
            }
            catch(InterruptedException e)
            {
               throw new IllegalStateException("UCL1 failed to enter t0Barrier");
            }
            log.info("Passed t1Barrier");
            passedBarriers = true;

            // Create a class loading task and submit it to the repository
            task = new MyClassLoadingTask(name, this, t);
            /* Process class loading tasks needing this UCL until our task has
               been completed by the thread owning the required UCL(s).
             */
            UnifiedLoaderRepository3 ulr3 = (UnifiedLoaderRepository3) repository;
            if( LoadMgr3.beginLoadTask(task, ulr3) == false )
            {
               while( task.threadTaskCount() != 0 )
               {
                  try
                  {
                     LoadMgr3.nextTask(t, task, ulr3);
                  }
                  catch(InterruptedException e)
                  {
                     // Abort the load or retry?
                     break;
                  }
               }
            }
         }
         finally
         {
            // Unregister as the UCL owner to reschedule any remaining load tasks
            if( loadLock.holds() == 1 )
               LoadMgr3.endLoadTask(task);
            // Notify any threads waiting to use this UCL
            this.release();
            this.notifyAll();
         }

         if( task.loadedClass() == null )
         {
            if( task.loadException() instanceof ClassNotFoundException )
               throw (ClassNotFoundException) task.loadException();
            else if( task.loadException() != null )
            {
               log.info("Unexpected error during load of:"+name, task.loadException());
               String msg = "Unexpected error during load of: "+name
                  + ", msg="+task.loadException().getMessage();
               throw new ClassNotFoundException(msg);
            }
            // Assert that loadedClass is not null
            else
               throw new IllegalStateException("ClassLoadingTask.loadedTask is null, name: "+name);
         }

         return task.loadedClass();
      }
   }
}
